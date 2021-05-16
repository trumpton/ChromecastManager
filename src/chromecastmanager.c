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
// Diagnostics:
//
//  Set SSLKEYLOGFILE environment variable before launching, and SSL keys required
//  for wireshark debugging is populated.
//
//  Note that this technique also allows the messages sent from the CACTool
//  (https://casttool.appspot.com/cactool/) to be intercepted - i.e. set variable
//  before launching the chrome web browser.
//
//  Set NETDUMPENABLE to enable the raw (protobuf) network communications to/from the 
//  chromecast devices to be sent to stderr.  This is the same information that can
//  be captured with wireshark.
//
//  Set PINGLOGENABLE to enable the logging of PING and PONG messages.  Normally these
//  messages are suppressed from the logs as there tend to be an awful lot of them.
//

// Print out select status
//#define SELECTDEBUG

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

// SIGPIPE handler needed for valgrind
void sigpipeHandler(int sig_num) {
  signal(SIGPIPE, sigpipeHandler);
  logmsg(LOG_NOTICE, "sigpipe received - ignoring") ;
}

void setfdsetdbgstr(char *str, int reset, char *template, ...) ;

//
// Main function
//

char *_scriptfolder ;
char *getscriptfolder() { return _scriptfolder ; }

#define MAXHT 2

int main(int argc, char *argv[])
{
  int n ;

  HTTPD *httpsh[MAXHT];               // HTTPD session
  int hqi[MAXHT] ;                    // Associated chromecast device # with http session
  int queriedindex=-1 ;               // Queried Index of last item queried

  CHROMECAST **cch ;                  // Array of chromecast devices to use
  int maxcc=chromecast_mdns_size() ;  // Max number of chromecast devices

  time_t servicetime=0, pingchecktime=0 ;
  int httpdport = 9000 ;
  char *loglevel=NULL ;

  for (int i=0; i<MAXHT; i++) {
    httpsh[i]=NULL ;
    hqi[i]=-1;
  }

  if (argc>=2) {

    _scriptfolder = argv[1] ;

  }

  if (argc>=3) {

    if (isdigit(argv[2][0])) httpdport=atoi(argv[2]) ;
    else loglevel=argv[2] ;
  }

  if (argc==4) {

    if (isdigit(argv[3][0])) httpdport=atoi(argv[3]) ;
    else loglevel=argv[3] ;

  }

  if (argc>4 || argc<2) {

    fprintf(stderr, "chromecastmanager scriptfolder [serverport] [loglevel]\n") ;
    goto fail ;

  }

  logopen("chromecast") ;
  logsetlevel(loglevel?loglevel:"info") ;
  logmsg(LOG_NOTICE, "server started") ;

  if (getenv("SSLKEYLOGFILE")) 
    logmsg(LOG_NOTICE, "Environment variable SSLKEYLOGFILE found - logging tls keys") ;

  if (getenv("NETDUMPENABLE")) 
    logmsg(LOG_NOTICE, "Environment variable NETDUMPENABLE found - dumping raw protobuf data to stderr") ;

  if (getenv("PINGLOGENABLE")) 
    logmsg(LOG_NOTICE, "Environment variable PINGLOGENABLE found - logging pings") ;

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
    logmsg(LOG_CRIT, "Unable to setup HTTP server on port %d", httpdport) ;
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

    for (int i=0; i<MAXHT; i++) {
      if (httpsh[i] && hfd(httpsh[i])>=0) {
        FD_SET(hfd(httpsh[i]), &rfds) ;
        if (hfd(httpsh[i]) > lp) lp = hfd(httpsh[i]);
      }
    }

    // Shutdown request activity

    if (shutdownpipefd[1]>=0) {
      FD_SET(shutdownpipefd[1], & rfds);
      if (shutdownpipefd[1] > lp) lp = shutdownpipefd[1];
    }


    /////////////////////////////////////////////////////
    // Wait for activity

    if ( httpsh[0] || httpsh[1] ) {

      // Faster timeout if HTTP request in progress
      tv.tv_sec = 0 ;
      tv.tv_usec = 200;

    } else {

      tv.tv_sec = TICKRATE ;
      tv.tv_usec = 0;

    }

#ifdef DEBUG
    char fdsetdbg[13] ;
    setfdsetdbgstr(fdsetdbg, 1, "........mhscc", 
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
      FD_ISSET(hfd(httpsh[0]),&rfds),
      FD_ISSET(hfd(httpsh[1]),&rfds)) ;
#endif

    n = select(lp + 1, &rfds, &wfds, NULL, &tv);

#ifdef SELECTDEBUG
    setfdsetdbgstr(fdsetdbg, 0, "01234567MHSCC", 
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
      FD_ISSET(hfd(httpsh[0]),&rfds),
      FD_ISSET(hfd(httpsh[1]),&rfds)) ;
    logmsg(LOG_INT, "FD_SET: %s", fdsetdbg) ;
#endif

    /////////////////////////////////////////////////////
    // Process new httpd server connection request

    if (!exit_requested && httpdfd>0 && FD_ISSET(httpdfd, &rfds)) {

      // Find a free handle

      int thishc = -1 ;
      for (int hc=0; hc<MAXHT && thishc<0; hc++) {
        if (!httpsh[hc]) thishc=hc ;
      }

      if (thishc>=0) {
        httpsh[thishc] = haccept(httpdfd) ;
        logmsg( LOG_DEBUG, "HTTP session %d connection established from %s:%d",
              thishc+1, hpeeripaddress(httpsh[thishc]), hpeerport(httpsh[thishc]) ) ;
      }

    }

    /////////////////////////////////////////////////////
    // Process http session requests


    if (!exit_requested) {


      for (int hc=0; hc<MAXHT; hc++) {

        // Timeout connections

        int timeout = hqi[hc]>=0 ? 5 : 2 ;

        if (httpsh[hc] && hfd(httpsh[hc])>0 && hconnectiontime(httpsh[hc]) > timeout ) {
          logmsg( LOG_DEBUG, "HTTP session %d connection timed out (%s:%d )", 
                  hc+1, hpeeripaddress(httpsh[hc]), hpeerport(httpsh[hc])) ;
          hclose(httpsh[hc]) ;
          httpsh[hc]=NULL ;
          hqi[hc]=-1 ;
        }


        // Check for activity

        if (httpsh[hc] && FD_ISSET(hfd(httpsh[hc]), &rfds)) {

          int r = hrecv(httpsh[hc]) ;

          switch ( r ) {

            case 0:

              // Wait for more data
              break ;

            case -1:

              // Connection closed

              logmsg(LOG_DEBUG, "HTTP session %d connection closed (%s:%d)",
                  hc+1, hpeeripaddress(httpsh[hc]), hpeerport(httpsh[hc])) ;
              hclose(httpsh[hc]) ;
              httpsh[hc]=NULL ;
              hqi[hc]=-1 ;
              break ;

            case 200:

              // Dispatch completed request

              hqi[hc] = chromecast_device_request_process(httpsh[hc], cch, maxcc) ;
              break ;

            default:

              // Send error response message

              hsend(httpsh[hc], r, NULL, NULL) ;
              break ;

          } ;

        }

      }

    }

    /////////////////////////////////////////////////////
    // Process chromecast devices

    if (!exit_requested) for (int i=0; i<maxcc; i++) {

      if (ccrdfdisset(cch[i], &rfds, &wfds)) {

        // Responses from device

        switch (ccrecv(cch[i])) {

        case 0:

          // Continue collecting
          break ;

        case -1:

          // Connection closed
          logmsg( LOG_NOTICE, "Chromecast %d at %s:%d - connection closed by peer", 
                  i+1, ccipaddress(cch[i]), ccpeerport(cch[i]) ) ;
          ccdisconnect(cch[i]) ;
          ccdelete(cch[i]) ;
          cch[i]=NULL ;
          break ;

        case -2:

          // Connection error
          logmsg( LOG_NOTICE, "Chromecast %d at %s:%d - connection terminated", 
                  i+1, ccipaddress(cch[i]), ccpeerport(cch[i]) ) ;
          ccdisconnect(cch[i]) ;
          ccdelete(cch[i]) ;
          cch[i]=NULL ;
          break ;

        default:

          // Process request / response 

          queriedindex=-1 ;
          for (int hc=0; hc<MAXHT && queriedindex<0; hc++) {
            if (hqi[hc] == i) queriedindex=hc ;
          }

          chromecast_device_response_process(cch[i], queriedindex>=0 ? httpsh[queriedindex] : NULL) ;

          break ;

        }

      } else if (cch[i] && cch[i]->macro && cch[i]->macroforce ) {

         // Continue to process macro on a timer

         queriedindex=-1 ;
         for (int hc=0; hc<MAXHT && queriedindex<0; hc++) {
           if (hqi[hc] == i) queriedindex=hc ;
         }

         chromecast_macro_process(queriedindex>=0 ? httpsh[queriedindex] : NULL, cch[i]) ;

      }

    }

    /////////////////////////////////////////////////////
    // Timeout

    if (!exit_requested && time(NULL) > (servicetime+240) ) {

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

        if ( cch[i] && ( ccidletime(cch[i]) > 480 ) ) {

          logmsg( LOG_NOTICE, "Disconnecting non-responsive Chromecast %d at %s:%d",
                  i+1, ccipaddress(cch[i]), ccpeerport(cch[i])) ;
          ccdelete(cch[i]) ;
          cch[i]=NULL ;

        } else if ( cch[i] && ( ccidletime(cch[i]) > 60 ) ) {

          ccsendheartbeatmessage(cch[i], "PING") ;
          ccsendreceivermessage(cch[i], "GET_STATUS") ;

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

#ifdef DEBUGLOOPS
    static int lps=0 ;
    static time_t lasttime=0 ;
    time_t thistime=time(NULL) ;
    if (thistime!=lasttime) {
      printf("Loops/Second: %d\n", lps) ;
      lps=0 ;
      lasttime=thistime ;
    } else {
      lps++ ;
    }
#endif

  } while (!exit_requested) ;


  /////////////////////////////////////////////////////
  /////////////////////////////////////////////////////
  // Shutdown

fail:

  if (httpsh[0]) hclose(httpsh[0]) ;
  if (httpsh[1]) hclose(httpsh[1]) ;
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



