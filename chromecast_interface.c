//
// chromecastmanager_ccinterface.c
//
// https://docs.rs/crate/gcast/0.1.5/source/PROTOCOL.md
//
//
// CHROMECAST *ccnew() ;
// int ccconnect(CHROMECAST *cch, char *ipaddress, int port) ;
// int ccfd(CHROMECAST *cch) ;
//

#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>

#include "libtools/net.h"
#include "libtools/log.h"
#include "libdataobject/dataobject.h"

#include "chromecast_interface.h"

#define MAXPROTOBUFSIZE 65536

unsigned char *_cc_uint32tobe(unsigned int n) ;
unsigned int _cc_betouint32(char *buf) ;


int ccnextrequestid(CHROMECAST *cch)
{
  if (!cch) return 0 ;
  else return (++cch->requestid) ;
}

int cclastrequestid(CHROMECAST *cch)
{
  if (!cch) return 0 ;
  else return (cch->requestid) ;
}


//////////////////////////////////////////////////////////////////////////
//
// @brief Return IP address / port of current connection

char *ccipaddress(CHROMECAST *cch)
{
  char *addr=NULL ;
  if (cch && cch->ssl) {
    addr = netpeerip(cch->ssl) ;
  }
  if (!addr) return "0.0.0.0" ;
  else return addr ;
}

int ccpeerport(CHROMECAST *cch) 
{
  if (cch && cch->ssl) 
    return netpeerport(cch->ssl) ;
  else
    return -1 ;
}


//////////////////////////////////////////////////////////////////////////
//
// @brief Create a new chromecast connection
// @return handle to connection
//

CHROMECAST *ccnew() 
{
  CHROMECAST *cch = malloc(sizeof(CHROMECAST)) ;
  if (!cch) return NULL ;
  memset(cch, '\0', sizeof(CHROMECAST)) ;
  return cch ;
}

//////////////////////////////////////////////////////////////////////////
//
// @brief Establish connection to chromecast client
// @param(in) cch Handle of chromecast device
// @param(in) ipaddress Address of device
// @param(in) port Port of device
// @return True on success
//

int ccconnect(CHROMECAST *cch, char *ipaddress, int port)
{
  if (!cch || !ipaddress) {
    perror("ccconnect called with NULL handle / ipaddress\n") ;
    return 0 ;
  }

  if (cch->ssl) {
    netclose(cch->ssl) ;
    cch->ssl = NULL ;
  }

  cch->ssl = netconnect(ipaddress, port, TLS|NONBLOCK|NOCERTCHAIN|DEBUGKEYDUMP|DEBUGDATADUMP) ;

  if (cch->ssl) {

    cch->recvstate = REC_START ;
    cch->lastreceipt = time(NULL) ;
    return 1 ;

  } else {

    return 0 ;

  }

}

//////////////////////////////////////////////////////////////////////////
//
// @brief Disconnect connection to chromecast client
// @param(in) cch Handle of chromecast device
// @return True on success
//

int ccdisconnect(CHROMECAST *cch)
{
  if (!cch) return 0 ;

  if (cch->ssl) netclose(cch->ssl) ;
  if (cch->recvobject) dodelete(cch->recvobject) ;
  if (cch->sendobject) dodelete(cch->sendobject) ;
  if (cch->recvbuf) free(cch->recvbuf) ;
  if (cch->receivertransportid) free(cch->receivertransportid) ;
  if (cch->receiversessionid) free(cch->receiversessionid) ;
  if (cch->receiverappid) free(cch->receiverappid) ;
  if (cch->receiverdisplayname) free(cch->receiverdisplayname) ;
  if (cch->receiverstatustext) free(cch->receiverstatustext) ;
  if (cch->playerstate) free(cch->playerstate) ;

  cch->ssl=NULL ;
  cch->recvobject=NULL ;
  cch->sendobject=NULL ;
  cch->recvbuf=NULL ;
  cch->receivertransportid=NULL ;
  cch->receiversessionid=NULL ;
  cch->receiverappid=NULL ;
  cch->receiverdisplayname=NULL ;
  cch->receiverstatustext=NULL ;
  cch->playerstate=NULL ;
  cch->recvstate=REC_START ;
  cch->recvsize=0 ;
  cch->recvlen=0 ;

  return 1 ;
}


//////////////////////////////////////////////////////////////////////////
//
// @brief Delete / release Chromecast structure
// @param(in) cch Handle of chromecast device
// @return True on success
//

int ccdelete(CHROMECAST *cch)
{
  if (!cch) return 0 ;
  ccdisconnect(cch) ;
  free(cch) ;
  return 1 ;
}


//////////////////////////////////////////////////////////////////////////
//
// @brief Add connection to fd_set if active
// @param(in) cch Handle of network connection
// @param(in) fds FD Set for select()
// @param(inout) l pointer to largest fd found
// @return number of connections added
//

int ccrdfdset(CHROMECAST *cch, fd_set *rfds, fd_set *wfds, int *l) 
{
  if (!cch || !cch->ssl) return 0 ;
  return netrdfdset(cch->ssl, rfds, wfds, l) ;
}

//
// @brief Test to see if data is available
// @param(in) sh Handle of network connection
// @param(in) fds FD Set for select()
// @return Return true if information available
//

int ccrdfdisset(CHROMECAST *cch, fd_set *rfds, fd_set *wfds) 
{
  if (!cch || !cch->ssl) return 0 ;
  return netrdfdisset(cch->ssl, rfds, wfds) ;
}


//
//

char *ccfdsetinfo(CHROMECAST *cch, fd_set *rfds, fd_set *wfds) 
{
  return netfdsetinfo(cch->ssl, rfds,  wfds) ;
}


//////////////////////////////////////////////////////////////////////////
// 
// @brief Get network connection state
// @param(in) Handle of open connection
// @return Returns true if network connection is connected
//

int ccisconnected(CHROMECAST *cch) 
{
  if (!cch || !cch->ssl) return 0 ;
  return netisconnected(cch->ssl) ;
}


//////////////////////////////////////////////////////////////////////////
// 
// @brief Get time since last message receipt
// @param(in) Handle of open connection
// @return Returns idle time in seconds
//

int ccidletime(CHROMECAST *cch)
{
  if (!cch) return 99999 ;
  time_t now = time(NULL) ;
  return (int)(now - cch->lastreceipt) ;
}

//////////////////////////////////////////////////////////////////////////
// 
// @brief Get number of pings sent since last receipt
// @param(in) Handle of open connection
// @return Returns number of pings
//

int ccpingssent(CHROMECAST *cch)
{
  if (!cch) return 0 ;
  else return cch->pingssent ;
}


//////////////////////////////////////////////////////////////////////////
//
// @brief Set and get anciliary flag data
// @return handle to connection
//
int ccsetflag(CHROMECAST *cch, int flags)
{
  if (!cch) return 0 ;
  cch->flags |= flags ;
  return 1 ;
}

int ccclearflag(CHROMECAST *cch, int flags)
{
  if (!cch) return 0 ;
  cch->flags &= -flags ;
  return 1 ;
}

int ccgetflag(CHROMECAST *cch)
{
  if (!cch) return 0 ;
  return cch->flags ;
}


//////////////////////////////////////////////////////////////////////////
//
// @brief Process input
// @param(in) cch Handle of chromecast device
// @return 0 if processing does not require attention
// @return -1 if connection is closed
// @return >1 if processing complete and an input is ready (use ccgetcmd)
//

int ccrecv(CHROMECAST *cch)
{
  int l ;

  if ( !ccisconnected(cch) ) return -1 ;

  {

    if (cch->recvstate == REC_START || cch->recvstate == REC_DONE || cch->recvstate == REC_PARSED) {

      if (cch->recvbuf) {
        free(cch->recvbuf) ;
        cch->recvbuf=0 ; 
      }
      cch->recvlen=0 ;
      cch->recvsize=0 ;
      cch->recvstate = REC_SIZE ;
      cch->lastreceipt = time(NULL) ;
      cch->pingssent = 0 ;

    }

    switch (cch->recvstate) {

    case REC_SIZE:

      if (!cch->recvbuf) cch->recvbuf=malloc(4) ;
      if (!cch->recvbuf) goto fail ;
      l = netrecv(cch->ssl, &(cch->recvbuf[(cch->recvlen)]), 4 - cch->recvlen ) ;
      if (l<0) goto fail ;
      if (l==0) goto fail ;
      cch->recvlen+=l ;

      if (cch->recvlen>=4) {

        cch->recvlen=0 ;
        cch->recvsize = _cc_betouint32(cch->recvbuf) ;

        if (cch->recvsize==0 || cch->recvsize>MAXPROTOBUFSIZE) {
          errno=ENOMEM ;
          goto fail ;
        }
        free(cch->recvbuf) ;
        cch->recvbuf = malloc(cch->recvsize) ;
        if (!cch->recvbuf) {
          goto fail ;
        }
        cch->recvstate = REC_BODY ;
      }

      break ;     

    case REC_BODY:

      l = netrecv(cch->ssl, &(cch->recvbuf[cch->recvlen]), cch->recvsize - cch->recvlen ) ;
      if (l<0) goto fail ;
      if (l==0) goto fail ;
      cch->recvlen += l ;
      if (cch->recvlen == cch->recvsize) {
        cch->recvstate = REC_DONE ;
        return 1 ;
      }
      break ;

    case REC_DONE:
    case REC_PARSED:

      return 1 ;
      break ;

    }
  
  }

  return 0 ;

fail:
  ccdisconnect(cch) ;
  return -1 ;

}

//////////////////////////////////////////////////////////////////////////
//
// @brief Process input
// @param(in) cch Handle of chromecast device
// @return True if message parsed and available
//

int _cc_processinputmessage(CHROMECAST *cch)
{

  if (!cch) return 0  ;
  if (cch->recvstate!=REC_DONE) return 0 ;
  if (cch->recvobject) dodelete(cch->recvobject) ;

  // Convert cch->recvbuf to dataobject
  //
  // /f2   ->   /sender
  // /f3   ->   /receiver
  // /f4   ->   /namespace
  // /f5   ->   /datatype
  // /f6/* ->   /message/*
  // /f7   ->   /bindata
  //

  cch->recvobject = donew() ;
  if (!cch->recvobject) goto fail ;

  //
  // Convert receive buffer to protobuffer
  //

  if (!dofromprotobuf(cch->recvobject, cch->recvbuf, cch->recvlen)) {
    perror("Unable to parse protobuf") ;
    goto fail ;
  }

  dosettype(cch->recvobject, do_string, "/f1") ; dorenamenode(cch->recvobject, "/f1", "version") ;
  dosettype(cch->recvobject, do_string, "/f2") ; dorenamenode(cch->recvobject, "/f2", "sender") ;
  dosettype(cch->recvobject, do_string, "/f3") ; dorenamenode(cch->recvobject, "/f3", "receiver") ;
  dosettype(cch->recvobject, do_string, "/f4") ; dorenamenode(cch->recvobject, "/f4", "namespace") ;
  dosettype(cch->recvobject, do_uint32, "/f5") ; dorenamenode(cch->recvobject, "/f5", "datatype") ;

  unsigned long int datatype=9999 ;
  dogetuint(cch->recvobject, do_uint32, &datatype, "/datatype") ;

  switch (datatype) {

    case 0:

      dorenamenode(cch->recvobject, "/f6", "message") ;

      logmsg( LOG_DEBUG, "RECV:%s %s from %s:%d",
              dogetdata(cch->recvobject, do_string, NULL, "/namespace"),
              dogetdata(cch->recvobject, do_string, NULL, "/message"),
              ccipaddress(cch), ccpeerport(cch) ) ;


      if (!doexpandfromjson(cch->recvobject, "/message")) {

        logmsg(LOG_NOTICE, "JSON parse error - %s (run with debug for context)", 
                dojsonparsestrerror(cch->recvobject) ) ;
      }

      break ;

    case 1:

      logmsg( LOG_DEBUG, "RECV:%s binary-data from %s:%d",
              dogetdata(cch->recvobject, do_string, NULL, "/namespace"),
              ccipaddress(cch), ccpeerport(cch) ) ;

      dorenamenode(cch->recvobject, "/f7", "bindata") ;
      break ;

    default:

      perror("Protobuf unrecognised type") ;
      goto fail ;
      break ;

  }


  //
  // Intercept and cache MEDIA_STATUS data
  //

  char *type = dogetdata(cch->recvobject, do_string, NULL, "/message/type") ;

  //
  // Intercept and cache MEDIA_STATUS data
  //

  if (type && strcmp(type,"MEDIA_STATUS")==0) {

    char *playerstate = dogetdata(cch->recvobject, do_string, NULL, "/message/status/0/playerState") ;

    // TODO: only returns status[0] - does not cater for more than 1 media channel
    // running, and the first one _not_ being a media player

    if (playerstate) {

      if (cch->playerstate) free(cch->playerstate) ;
      cch->playerstate=malloc(strlen(playerstate)+1) ;
      if (!cch->playerstate) goto fail ;
      strcpy(cch->playerstate, playerstate) ;
    }

  }


  //
  // Intercept and cache RECEIVER_STATUS data
  //

  if (type && strcmp(type,"RECEIVER_STATUS")==0) {

    if (cch->receivertransportid) free(cch->receivertransportid) ;
    if (cch->receiversessionid) free(cch->receiversessionid) ;
    if (cch->receiverappid) free(cch->receiverappid) ;
    if (cch->receiverdisplayname) free(cch->receiverdisplayname) ;
    if (cch->receiverstatustext) free(cch->receiverstatustext) ;

    cch->receivertransportid=NULL ;
    cch->receiversessionid=NULL ;
    cch->receiverappid=NULL ;
    cch->receiverdisplayname=NULL ;
    cch->receiverstatustext=NULL ;

    // TODO: only returns application[0] - does not cater for more than 1 application
    // running, and the first one _not_ being a media player

    char *appid = dogetdata(cch->recvobject, do_string, NULL, "/message/status/applications/0/appId") ;
    char *dname = dogetdata(cch->recvobject, do_string, NULL, "/message/status/applications/0/displayName") ;
    char *statt = dogetdata(cch->recvobject, do_string, NULL, "/message/status/applications/0/statusText") ;
    char *sesid = dogetdata(cch->recvobject, do_string, NULL, "/message/status/applications/0/sessionId") ;
    char *traid = dogetdata(cch->recvobject, do_string, NULL, "/message/status/applications/0/transportId") ;

    if (appid && sesid && traid) {

      cch->receivertransportid=malloc(strlen(traid)+1) ;
      cch->receiversessionid=malloc(strlen(sesid)+1) ;
      cch->receiverappid=malloc(strlen(appid)+1) ;
      if (!cch->receivertransportid || !cch->receiversessionid || !cch->receiverappid) goto fail ;
      strcpy(cch->receivertransportid, traid) ;
      strcpy(cch->receiversessionid, sesid) ;
      strcpy(cch->receiverappid, appid) ;

      if (dname) {
        cch->receiverdisplayname=malloc(strlen(dname)+1) ; 
        if (!cch->receiverdisplayname) goto fail ;
        strcpy(cch->receiverdisplayname, dname) ;
      }

      if (statt) {
        cch->receiverstatustext=malloc(strlen(statt)+1) ; 
        if (!cch->receiverstatustext) goto fail ;
        strcpy(cch->receiverstatustext, statt) ;
      }

    } else {

      if (cch->playerstate) free(cch->playerstate) ;
      cch->playerstate=NULL ;

    }
 
  }

  //
  // Receive complete, return true
  //

  return 1 ;

fail:
  fprintf(stderr, "ccgetmessage: Parsing of message failed\n") ;
  return 0 ;

}


//////////////////////////////////////////////////////////////////////////
//
// @brief Retrieve last received message as data object
// @param(in) cch Handle of chromecast device
// @return Handle of message or NULL if no message received
//

DATAOBJECT *ccgetmessage(CHROMECAST *cch)
{
  int status=1 ;
  if (!cch) return NULL ;

  switch (cch->recvstate) {

    case REC_DONE:

      status=_cc_processinputmessage(cch) ;
      cch->recvstate = REC_PARSED ;
      // Fall through
 
    case REC_PARSED:

      if (status) return cch->recvobject ;
      // Fall through

    default:

      return NULL ;
      break ;

  }

}

//////////////////////////////////////////////////////////////////////////
//
// @brief Retrieve last sent message as data object
// @param(in) cch Handle of chromecast device
// @return Handle of message or NULL if no message sent
//

DATAOBJECT *ccgetsentmessage(CHROMECAST *cch)
{
  if (!cch) return NULL ;
  else return cch->sendobject ;
}


//////////////////////////////////////////////////////////////////////////
//
// @brief Retrieve media connection details
// @param(in) cch Handle of chromecast device
// @param(in) type Record type (0-3)
// @param(out) Returns pointer to data or "" if not found
//

char *ccgetmediaconnectiondetails(CHROMECAST *cch, int type)
{
  char *reply=NULL ;
  if (cch) switch (type) {
  case CC_MCD_APPID: reply = cch->receiverappid ; break ;
  case CC_MCD_TRANSPORTID: reply = cch->receivertransportid ; break ;
  case CC_MCD_SESSIONID: reply = cch->receiversessionid ; break ;
  case CC_MCD_DISPLAYNAME: reply = cch->receiverdisplayname ; break ;
  case CC_MCD_STATUSTEXT: reply = cch->receiverstatustext ; break ;
  case CC_MCD_PLAYERSTATE: reply = cch->playerstate ; break ;
  }
  if (reply) return reply ;
  else return "" ;
}

//////////////////////////////////////////////////////////////////////////
//
// @brief Quick-send messages on default sender/receiver
//

int ccsendconnectionmessage(CHROMECAST *cch, char *type)
{
  if (ccsendmessage( cch, "sender-0", "receiver-0", 
              CC_NAMESPACE_CONNECTION, 
              "{\"type\":\"%s\"}", type)) {
    return 1 ;
  } else {
    return 0 ;
  }
}

int ccsendheartbeatmessage(CHROMECAST *cch, char *type)
{
  if (ccsendmessage( cch, "Tr@n$p0rt-0", "Tr@n$p0rt-0",
              CC_NAMESPACE_HEARTBEAT, 
              "{\"type\":\"%s\"}", type)) {
    (cch->pingssent)++ ;
    return 1 ;
  } else {
    return 0 ;
  }
}

int ccsendreceivermessage(CHROMECAST *cch, char *type)
{
  if (ccsendmessage( cch,  "sender-0", "receiver-0",
              CC_NAMESPACE_RECEIVER, 
              "{\"type\":\"%s\",\"requestId\":%d}", 
              type, (++cch->requestid))) {

    return cch->requestid ;
  } else {
    return 0 ;
  }
}


//////////////////////////////////////////////////////////////////////////
//
// @brief Send Message
// @param(in) cch Handle of chromecast device
// @param(in) command Dataobject of command to send
// @return True on success
//

// TODO: Change this to use varargs

int ccsendmessage(CHROMECAST *cch, char *sender, char *receiver, char *namespace, const char *message, ...)
{
  if (!cch || !netisconnected(cch->ssl)) return 0 ;


  int len=-1 ;

  if (cch->sendobject) dodelete(cch->sendobject) ;

  cch->sendobject = donew() ;
  if (!cch->sendobject) return 0 ;

  // Expand varargs

  va_list args;

  char *messagebuf=NULL ;

  va_start (args, message);
  size_t messagelen=vsnprintf(messagebuf, 0, message, args) ;

  messagebuf=malloc(messagelen+1) ;
  if (!messagebuf) return 0 ;

  va_start (args, message);
  vsnprintf(messagebuf, messagelen+1, message, args) ;

  // Populate sendobject

  dosetuint(cch->sendobject, do_uint32, 0x00, "/f1") ;                          // Protocol Version

  dosetdata(cch->sendobject, do_string, sender, strlen(sender), "/f2") ;        // Source ID
  dosetdata(cch->sendobject, do_string, receiver, strlen(receiver), "/f3") ;    // Destination ID
  dosetdata(cch->sendobject, do_string, namespace, strlen(namespace), "/f4") ;  // Namespace
  dosetuint(cch->sendobject, do_uint32, 0, "/f5") ;                             // Payload Type (0 = string)
  dosetdata(cch->sendobject, do_string, messagebuf, messagelen, "/f6") ;        // Payload Data (JSON string)

  // Convert to protobuf and send

  char *protobuf = doasprotobuf(cch->sendobject, &len) ;
  char *lengthst = _cc_uint32tobe(len) ;

  int r1 = netsend(cch->ssl, lengthst, 4) ;
  int r2 = netsend(cch->ssl, protobuf, len) ;

  // Rename sendobject fields to be more useful

  dosettype(cch->sendobject, do_string, "/f1") ; dorenamenode(cch->sendobject, "/f1", "version") ;
  dosettype(cch->sendobject, do_string, "/f2") ; dorenamenode(cch->sendobject, "/f2", "sender") ;
  dosettype(cch->sendobject, do_string, "/f3") ; dorenamenode(cch->sendobject, "/f3", "receiver") ;
  dosettype(cch->sendobject, do_string, "/f4") ; dorenamenode(cch->sendobject, "/f4", "namespace") ;
  dosettype(cch->sendobject, do_uint32, "/f5") ; dorenamenode(cch->sendobject, "/f5", "datatype") ;
  doexpandfromjson(cch->sendobject, "/f6") ; dorenamenode(cch->sendobject, "/f6", "message") ;

  logmsg( LOG_DEBUG, "SEND%s:%s %s to %s:%d", (r1==4 && r2==len)?"":"FAILED", 
          namespace, messagebuf, ccipaddress(cch), ccpeerport(cch)) ;

  free(messagebuf) ;

  return (r1==4 && r2==len) ;

}



//////////////////////////////////////////////////////////////////////////
//
// @brief Extract big endian integer from string
// @param(in) buf Pointer to string
// @return Value of data
//


unsigned int _cc_betouint32(char *buf)
{
  unsigned int n=0 ;
  assert(buf) ;

  int i=0 ;

  for (i=0; i<4; i++) {
    n = (n<<8) | (unsigned char)buf[i] ;
  }

  return n ;
}

unsigned char *_cc_uint32tobe(unsigned int n)
{
  static unsigned char r[4] ;
  for (int i=0; i<4; i++) {
    r[3-i] = n&0xFF ;
    n>>=8 ;
  }
  return r ;
}


