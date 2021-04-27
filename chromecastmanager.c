//
// chromecastmanager.c
//
// This program monitors the network and maintains a list of
// chromecast devices using MDNS (chromecast_mdns.h)
//
// For each device, it opens and maintains a command / status
// connection using TLS and protobuf comms (chromecast_interface.h)
//
// It also provides a http server to allow chromecast devices
// to be controlled and queried. (libtools/httpd.h)
//
// 1. Get list of devices on network
//
//        type: GET
//        uri: /devicelist
//
//
// 2. Access test page
//
//        type: GET
//        uri: /test
//
//
// 3. Perform 'macro' command
//
//        type: GET
//        uri: /command
//        device=device_name|address
//        port=port_number (required if device is an IP address)
//        url=url_for_play
//        sleep=sleep_duration_for_play (in seconds)
//        volume=volume_level (0.0 - 1.0)
//
// 4. Low level json access
//
//        type: POST
//        uri: /jsonquery
//
//        used to pass json data to a specific namespace
//        and capture the response.  Messages are extracted
//        from the JSON.
//
//        The request and responses are formatted as follows:
//
//        { "namespace": "namespace", "sender": "sender-0", "receiver: "receiver-0", 
//          "status" : "response-status", "message" : { <json request/response> } }
//
//        Responses include the status as follows:
//
//        OK      - Message processed - message contains response
//        TIMEOUT - Message sent to device, but no response received
//
// Diagnostics:
//
//  Set SSLKEYLOGFILE environment variable before launching, and SSL keys required
//  for wireshark debugging is populated.
//
//  Note that this technique also allows the messages sent from the CACTool
//  (https://casttool.appspot.com/cactool/) to be intercepted - i.e. set variable
//  before launching the chrome web browser.
//


#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/select.h>
#include <time.h>
#include <ctype.h>

// Scanning for chromecast devices
#include "chromecast_mdns.h"

// Protobuf over TLS connection to chromecast devices
#include "chromecast_interface.h"

// Chromecastmanager httpd server
#include "libtools/httpd.h"

// Logging
#include "libtools/log.h"

#include "chromecastmanager.h"

// Check rate for timeouts
#define TICKRATE 2

//
// Signal Handler for SIGINT and SIGHUP
//

int shutdownpipefd[2] ;
int exit_requested=0 ;

void sigintHandler(int sig_num) {
  if (shutdownpipefd[0]>=0) write(shutdownpipefd[0],"EXIT",4) ;
  signal(SIGINT, sigintHandler);
  logmsg(LOG_NOTICE, "shutdown requested (sigint)") ;
  exit_requested=1 ;
}

void sighupHandler(int sig_num) {
  if (shutdownpipefd[0]>=0) write(shutdownpipefd[0],"EXIT",4) ;
  signal(SIGHUP, sighupHandler);
  logmsg(LOG_NOTICE, "shutdown requested (sighup)") ;
  exit_requested=1 ;
}

void sigpipeHandler(int sig_num) {
  signal(SIGPIPE, sighupHandler);
}

void setfdsetdbgstr(char *str, int reset, char *template, ...) ;

//
// Main function
//

int main(int argc, char *argv[])
{
  int n ;
  int queriedindex=-1 ;               // Index of last item queried
  HTTPD *httpsh=NULL ;                // HTTPD session
  CHROMECAST **cch ;                  // Array of chromecast devices to use
  int maxcc=chromecast_mdns_size() ;  // Max number of chromecast devices
  time_t servicetime=0, pingchecktime=0 ;
  int httpdport = 9000 ;
  char *loglevel=NULL ;

  if (argc==2) {

    if (isdigit(argv[1][0])) httpdport=atoi(argv[1]) ;
    else loglevel=argv[1] ;

  } else if (argc==3) {

    if (isdigit(argv[2][0])) httpdport=atoi(argv[2]) ;
    else loglevel=argv[2] ;

  } else if (argc>3) {

    fprintf(stderr, "chromecastmanager [serverport] [loglevel]\n") ;
    goto fail ;

  }


  logopen("chromecast") ;
  logsetlevel(loglevel?loglevel:"info") ;
  logmsg(LOG_NOTICE, "server started") ;

  if (getenv("SSLKEYLOGFILE")) logmsg(LOG_NOTICE, "Environment variable SSLKEYLOGFILE found - logging tls keys") ;

  /////////////////////////////////////////////////////
  /////////////////////////////////////////////////////
  // Set up abort / exit request handling

  if (pipe(shutdownpipefd)!=0) {
    shutdownpipefd[0]=-1 ;
    shutdownpipefd[1]=-1 ;
  }
  signal(SIGINT, sigintHandler);
  signal(SIGHUP, sighupHandler);
  signal(SIGPIPE, sigpipeHandler);

  /////////////////////////////////////////////////////
  /////////////////////////////////////////////////////
  // Set up httpd server

  httpd_init(httpdport) ;
  int httpdfd = httpd_listenfd() ;
  if (httpdfd < 0) {
    logmsg(LOG_CRIT, "Unable to setup httpd server on port %d", httpdport) ;
    goto fail ;
  }
  logmsg(LOG_NOTICE, "HTTP server listening on port %d", httpdport) ;


  /////////////////////////////////////////////////////
  /////////////////////////////////////////////////////
  // Set up chromecast device structure

  cch = malloc(maxcc * sizeof(CHROMECAST *)) ;
  if (!cch) {
    logmsg(LOG_CRIT, "error allocating chromecast devices list") ;
    goto fail ;
  }
  memset(cch, '\0', maxcc * sizeof(CHROMECAST *)) ;


  /////////////////////////////////////////////////////
  /////////////////////////////////////////////////////
  // Initialise and listen for chromecast mdns messages

  chromecast_mdns_init() ;
  chromecast_mdns_refresh(TICKRATE, 1, 1) ;


  /////////////////////////////////////////////////////
  /////////////////////////////////////////////////////
  // Main Processing Loop

  do {

    int lp = 0;
    fd_set rfds, wfds;
    struct timeval tv;
    int mcfd=chromecast_mdns_fd() ;

    /////////////////////////////////////////////////////
    // Build list of ports to listen on

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);

// TODO: may need to re-initialise
// check what happens if network goes down and then
// comes back up

    // Chromecast device activity

    for (int i=0; i<maxcc; i++) {
      if (cch[i]) ccrdfdset(cch[i], &rfds, &wfds, &lp) ;
    }

    // HTTPD server activity

    FD_SET(httpdfd, &rfds);
    if (httpdfd > lp) lp = httpdfd;

    // MDNS broadcast activity

    if (mcfd >= 0) {
      FD_SET(mcfd, &rfds);
      if (mcfd > lp) lp = mcfd;
    }

    // HTTPD session activity

    if (httpsh && hfd(httpsh)>=0) {
      FD_SET(hfd(httpsh), &rfds) ;
      if (hfd(httpsh) > lp) lp = hfd(httpsh);
    }

    // Shutdown request activity

    if (shutdownpipefd[1]>=0) {
      FD_SET(shutdownpipefd[1], & rfds);
      if (shutdownpipefd[1] > lp) lp = shutdownpipefd[1];
    }


    /////////////////////////////////////////////////////
    // Wait for activity

    if (httpsh && httpdfd>0) {

      // Faster timeout if HTTP request in progress
      tv.tv_sec = 0 ;
      tv.tv_usec = 200;

    } else {

      tv.tv_sec = TICKRATE ;
      tv.tv_usec = 0;

    }

#ifdef DEBUG
    char fdsetdbg[13] ;
    setfdsetdbgstr(fdsetdbg, 1, "........mhsc", 
      cch[0]&&ccrdfdisset(cch[0],&rfds,&wfds),
      cch[1]&&ccrdfdisset(cch[1],&rfds,&wfds),
      cch[2]&&ccrdfdisset(cch[2],&rfds,&wfds),
      cch[3]&&ccrdfdisset(cch[3],&rfds,&wfds),
      cch[4]&&ccrdfdisset(cch[4],&rfds,&wfds),
      cch[5]&&ccrdfdisset(cch[5],&rfds,&wfds),
      cch[6]&&ccrdfdisset(cch[6],&rfds,&wfds),
      cch[7]&&ccrdfdisset(cch[7],&rfds,&wfds),
      FD_ISSET(mcfd,&rfds),
      FD_ISSET(httpdfd,&rfds),
      FD_ISSET(shutdownpipefd[1],&rfds),
      FD_ISSET(hfd(httpsh),&rfds)) ;
#endif

    n = select(lp + 1, &rfds, &wfds, NULL, &tv);

#ifdef SELECTDEBUG
    setfdsetdbgstr(fdsetdbg, 0, "01234567MHSC", 
      cch[0]&&ccrdfdisset(cch[0],&rfds,&wfds),
      cch[1]&&ccrdfdisset(cch[1],&rfds,&wfds),
      cch[2]&&ccrdfdisset(cch[2],&rfds,&wfds),
      cch[3]&&ccrdfdisset(cch[3],&rfds,&wfds),
      cch[4]&&ccrdfdisset(cch[4],&rfds,&wfds),
      cch[5]&&ccrdfdisset(cch[5],&rfds,&wfds),
      cch[6]&&ccrdfdisset(cch[6],&rfds,&wfds),
      cch[7]&&ccrdfdisset(cch[7],&rfds,&wfds),
      FD_ISSET(mcfd,&rfds),
      FD_ISSET(httpdfd,&rfds),
      FD_ISSET(shutdownpipefd[1],&rfds),
      FD_ISSET(hfd(httpsh),&rfds)) ;
    logmsg(LOG_INT, "FD_SET: %s", fdsetdbg) ;
#endif

    /////////////////////////////////////////////////////
    // Process new httpd server connection request

    // Timeout old connection
    if (httpsh && httpdfd>0 && hconnectiontime(httpsh)>5) {
      hclose(httpsh) ;
      httpsh=NULL ;
    }


    if (!exit_requested && !httpsh && httpdfd>0 && FD_ISSET(httpdfd, &rfds)) {

      if (!httpsh) {

        httpsh = haccept(httpdfd) ;
        logmsg( LOG_DEBUG, "Connection established from %s:%d",
                hpeeripaddress(httpsh), hpeerport(httpsh) ) ;

      }

    }


    /////////////////////////////////////////////////////
    // Process http session requests


    if (!exit_requested && httpsh && FD_ISSET(hfd(httpsh), &rfds)) {

      int r = hrecv(httpsh) ;

      switch ( r ) {

        case 0:

          // Wait for more data
          break ;

        case -1:

          // Connection closed

          logmsg(LOG_DEBUG, "Connection closed (%s:%d)",
              hpeeripaddress(httpsh), hpeerport(httpsh)) ;
          hclose(httpsh) ;
          httpsh=NULL ;
          break ;

        case 200:

          // Dispatch completed request

          queriedindex=chromecast_device_request_process(httpsh, cch, maxcc) ;
          logmsg(LOG_INFO, "Processing HTTP query from %s:%d",
              hpeeripaddress(httpsh), hpeerport(httpsh)) ;
          break ;

        default:

          // Send error response message

          hsend(httpsh, r, NULL, NULL) ;
          break ;

      } ;
    }

    /////////////////////////////////////////////////////
    // Process chromecast device transaction responses

    if (!exit_requested) for (int i=0; i<maxcc; i++) {

      if (ccrdfdisset(cch[i], &rfds, &wfds)) {

        switch (ccrecv(cch[i])) {

        case 0:

          // Continue collecting
          break ;

        case -1:

          // Connection closed / error
          logmsg(LOG_NOTICE, "Chromecast %s:%d connection closed", ccipaddress(cch[i]), ccpeerport(cch[i]) ) ;
          ccdelete(cch[i]) ;
          cch[i]=NULL ;
          break ;

        default:

          // Process request / response
          chromecast_device_response_process(cch[i], httpsh) ;

          break ;

        }

      }

    }

    /////////////////////////////////////////////////////
    // Timeout

    if (!exit_requested && time(NULL) > (servicetime+60) ) {

      servicetime = time(NULL) ;

      logmsg(LOG_INT, "Tick...") ;

      // Expire expired, and request refresh if needed
      
      chromecast_mdns_refresh(TICKRATE, 1, 0) ;
      chromecast_device_connection_update(cch, maxcc) ;

    }


    /////////////////////////////////////////////////////
    // Check and ping


    if (!exit_requested && time(NULL) > (pingchecktime+30) ) {

      pingchecktime = time(NULL) ;

      for (int i=0; i<maxcc; i++) {

        if ( cch[i] && ccpingssent(cch[i])==0 && ( ccidletime(cch[i]) > 300 ) ) {

          ccsendheartbeatmessage(cch[i], "PING") ;

        } else if ( cch[i] && ccpingssent(cch[i])==1 && ( ccidletime(cch[i]) > 420 ) ) {

          logmsg( LOG_DEBUG, "Sending final ping to %s:%d", ccipaddress(cch[i]), ccpeerport(cch[i])) ;
          ccsendheartbeatmessage(cch[i], "PING") ;

        } else if ( cch[i] && ( ccidletime(cch[i]) > 480 ) ) {

          logmsg( LOG_INFO, "Disconnecting non-responsive chromecast device at %s:%d",
                  ccipaddress(cch[i]), ccpeerport(cch[i])) ;
          ccdelete(cch[i]) ;
          cch[i]=NULL ;

        }

      }

    }

    /////////////////////////////////////////////////////
    // Update available broadcast information

    if (!exit_requested && mcfd>0 && FD_ISSET(mcfd, &rfds)) {

      chromecast_mdns_update() ;
      chromecast_device_connection_update(cch, maxcc) ;

    }

    /////////////////////////////////////////////////////
    // Handle shutdown request

    if (shutdownpipefd[1]>0 && FD_ISSET(shutdownpipefd[1], &rfds)) {
      exit_requested=1 ;
    }

    // Protection from runaway process
    usleep(100) ;

  } while (!exit_requested) ;


  /////////////////////////////////////////////////////
  /////////////////////////////////////////////////////
  // Shutdown

fail:

  httpd_shutdown() ;

  if (cch) {
    for (int i=0; i<maxcc; i++) {
      if (cch[i]) ccdelete(cch[i]) ;
    }
    free(cch) ;
  }

  chromecast_mdns_close() ;

  logmsg(LOG_NOTICE, "shutdown complete") ;
  logclose() ;

  return 0 ;

}


/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// @brief Find device friendlyname in mdns list
// @return index of device or -1
//

int chromecast_finddevice(char *device, int maxcc)
{
  int index=-1 ;
  if (device) for (int i=0; i<maxcc && index<0; i++) {
    chromecast_mdns_record *record = chromecast_mdns_at(i) ;
    if (record && record->friendlyname && strcasecmp(device, record->friendlyname)==0) index=i ;
  }
  return index ;
}


#ifdef DEBUG
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// @brief Build list of flags to report
//
void setfdsetdbgstr(char *str, int reset, char *template, ...) 
{
  int l=strlen(template) ;
  if (reset) {
    for (int i=0; i<l; i++) str[i]=' ' ;
    str[l]='\0' ;
  }
  va_list p ;
  va_start(p, template) ;
  for (int i=0; i<l; i++) {
    int e = va_arg(p, int) ;
    if (e) str[i]=template[i] ;
  }
}
#endif



