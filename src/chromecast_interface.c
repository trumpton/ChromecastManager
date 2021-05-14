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
#include <ctype.h>

#include "libtools/net.h"
#include "libtools/log.h"
#include "libtools/mem.h"
#include "libtools/str.h"
#include "libtools/httpd.h"

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

  cch->vars = donew() ;

  // Add Default Watches

  ccaddwatch( cch, "sessionId",
              "urn:x-cast:com.google.cast.receiver", 
              "RECEIVER_STATUS",
              "/message/status/applications/0/sessionId") ;

/*
  ccaddwatch( cch, "appId",
              "urn:x-cast:com.google.cast.receiver", 
              "RECEIVER_STATUS",
              "/message/status/applications/0/appId") ;

  ccaddwatch( cch, "statusText",
              "urn:x-cast:com.google.cast.receiver", 
              "RECEIVER_STATUS",
              "/message/status/applications/0/statusText") ;

  ccaddwatch( cch, "playerState",
              "urn:x-cast:com.google.cast.media", 
              "MEDIA_STATUS",
              "/message/status/0/playerState") ;

  ccaddwatch( cch, "mediaSessionId",
              "urn:x-cast:com.google.cast.media", 
              "MEDIA_STATUS",
              "/message/status/0/mediaSessionId") ;
*/

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
  if (cch->vars) dodelete(cch->vars) ;
  if (cch->recvobject) dodelete(cch->recvobject) ;
  if (cch->sendobject) dodelete(cch->sendobject) ;
  if (cch->recvbuf) free(cch->recvbuf) ;
  if (cch->macro) free(cch->macro) ;
  if (cch->httpsessionvars) free(cch->httpsessionvars) ;

  cch->ssl=NULL ;
  cch->vars=NULL ;
  cch->recvobject=NULL ;
  cch->sendobject=NULL ;
  cch->recvbuf=NULL ;
  cch->macro=NULL ;
  cch->httpsessionvars=NULL ;

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
// @brief Search for variable, and return data object

DATAOBJECT *_cc_find_var(DATAOBJECT *root, char *variable)
{
  if (!root || !variable) return NULL ;
  return dofindnode(root, "/%s", variable) ;
}


//////////////////////////////////////////////////////////////////////////
//
// @brief Add variable to watch list
// @param(in) cch Handle of chromecast device
// @param(in) variable Name of variable
// @param(in) namespace Namespace to use in search
// @param(in) type Message type ("*" for any)
// @param(in) path Path to variable in namespace to add to watch
// @return True if watch could be added
//

int ccaddwatch(CHROMECAST *cch, char *variable, char *namespace, char *type, char *path)
{
  if (!cch || !cch->vars) return 0 ;

  dosetdata(cch->vars, do_string, variable, strlen(variable), "/%s/variable", variable) ;
  dosetdata(cch->vars, do_string, path, strlen(path), "/%s/path", variable) ;
  dosetdata(cch->vars, do_string, namespace, strlen(namespace), "/%s/namespace", variable) ;
  dosetdata(cch->vars, do_string, type, strlen(type), "/%s/type", variable) ;
  dosetdata(cch->vars, do_string, "", 0, "/%s/value", variable) ;
}


//////////////////////////////////////////////////////////////////////////
//
// @brief Set variable value (string, int or float)
// @param(in) cch Handle of chromecast device
// @param(in) variable Name of variable
// @param(in) value Value to be stored
// @return True if value could be added

int ccsetwatch(CHROMECAST *cch, char *variable, char *value)
{
  if (!cch || !variable || !value) return 0 ;
  if (!dogetdata(cch->vars, do_string, NULL, "/%s/value"), variable) return 0 ;
  dosetdata(cch->vars, do_string, value, strlen(value), "/%s/value", variable) ;
  return 1 ;
}


//////////////////////////////////////////////////////////////////////////
//
// @brief Return the value for the named variable (string, int or float)
// @param(in) cch Handle of chromecast device
// @param(int) variable Variable name
// @return Pointer to string, or ""
//

char *ccgetwatch(CHROMECAST *cch, char *variable)
{
  DATAOBJECT *dh = _cc_find_var(cch->vars, variable) ;
  if (!dh) return "" ;
  else return dogetdata(dh, do_string, NULL, "/value") ;
}



//////////////////////////////////////////////////////////////////////////
//
// @brief Return the variable details at the given index
// @param(in) cch Handle of chromecast device
// @param(in) index Index of variable
// @return Variable details or NULL if not found

char * ccgetwatchvarnameat(CHROMECAST *cch, int index) 
{
  DATAOBJECT *node = dochild(donoden(cch->vars, index)) ;
  if (!node) return NULL ;
  return dogetdata(node, do_string, NULL, "/variable") ;
}

char * ccgetwatchat(CHROMECAST *cch, int index)
{
  DATAOBJECT *node = dochild(donoden(cch->vars, index)) ;
  if (!node) return NULL ;
  return dogetdata(node, do_string, NULL, "/value") ;
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

      char *sender = dogetdata(cch->recvobject, do_string, NULL, "/sender") ;
      if ( getenv("PINGLOGENABLE") || (sender && strcmp(sender, "Tr@n$p0rt-0")!=0) ) {

        logmsg( LOG_DEBUG, "RECV: %s from %s:%d",
                doasjson(cch->recvobject, NULL),
                ccipaddress(cch), ccpeerport(cch) ) ;
      }

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
  // Intercept and cache variables into the cch->vars object
  //
  //

  int varindex=0 ;
  DATAOBJECT *var ;

  char *msgnamespace = dogetdata(cch->recvobject, do_string, NULL, "/namespace") ;
  char *msgtype = dogetdata(cch->recvobject, do_string, NULL, "/message/type") ;

  if (msgnamespace) {
    dosetdata(cch->vars, do_string, msgnamespace, strlen(msgnamespace), "/lastMessageNamespace/value") ;
    dosetdata(cch->vars, do_string, "lastMessageNamespace", 20, "/lastMessageNamespace/variable") ;
  }

  if (msgtype) {
    dosetdata(cch->vars, do_string, msgtype, strlen(msgtype), "/lastMessageType/value") ;
    dosetdata(cch->vars, do_string, "lastMessageType", 15, "/lastMessageType/variable") ;
  }


/*
  // Clear relevant variables

  varindex=0 ;

  if (msgnamespace && msgtype) while ( var = dochild(donoden(cch->vars, varindex)) ) {

    // Get the variable details

    char *varnamespace = dogetdata(var, do_string, NULL, "/namespace") ;
    char *vartype = dogetdata(var, do_string, NULL, "/type") ;

    if (varnamespace && vartype && strcmp(msgnamespace,varnamespace)==0 && strcmp(msgtype, vartype)==0) {

      dosetdata(var, do_string, "", 0, "/value") ;

    }

    varindex++ ;

  }
*/


  // Walk through each variable node

  varindex=0 ;

  if (msgnamespace && msgtype) while ( var = dochild(donoden(cch->vars, varindex)) ) {

    // Get the variable details

    char *variable = dogetdata(var, do_string, NULL, "/variable") ;
    char *value = dogetdata(var, do_string, NULL, "/value") ;
    char *varnamespace = dogetdata(var, do_string, NULL, "/namespace") ;
    char *vartype = dogetdata(var, do_string, NULL, "/type") ;
    char *varpath = dogetdata(var, do_string, NULL, "/path") ;

    if (varnamespace && vartype && varpath) {

      // If SessionId has just changed, establish a connection

      if ( strcmp(varnamespace, msgnamespace)==0 &&
           ( strcmp(vartype, msgtype)==0 || strcmp("*", msgtype)==0 ) ) {


        // Extract the data and convert to a string
   
        enum dataobject_type msgparamtype = dogettype(cch->recvobject, varpath) ;

        if (dofindnode(cch->recvobject, varpath)) {

          char *msgvalue ;
          char buf[32] ;
          long int i ;
          double d ;
          switch (msgparamtype) {
          case do_string:
          case do_data:
            msgvalue = dogetdata(cch->recvobject,do_string,NULL,varpath) ;
            break ;
          case do_double:
          case do_float:
            dogetreal(cch->recvobject,msgparamtype,&d,varpath) ;
            snprintf(buf,sizeof(buf),"%f",d) ;
            msgvalue=buf ;
            break ;
          default:
            dogetsint(cch->recvobject,msgparamtype,&i,varpath) ;
            snprintf(buf,sizeof(buf),"%ld",i) ;
            msgvalue=buf ;
            break ;
          }


          // If the variable is the sessionId, then establish a session-0 connection

          if ( strcmp(variable, "sessionId")==0 && 
               (!value || strcmp(value, msgvalue)!=0) ) {

            if ( ccsendmessage( cch, "session-0", msgvalue, 
                 CC_NAMESPACE_CONNECTION, "{\"type\":\"CONNECT\"}") ) {

              ccsendmessage( cch, "session-0", msgvalue,
                             CC_NAMESPACE_MEDIA, 
                             "{\"type\":\"GET_STATUS\",\"requestId\":%d}", 
                             (++cch->requestid) ) ;

              logmsg( LOG_INFO, "Establishing session-0 / %s connection to chromecast at %s:%d",
                      msgvalue,
                      ccipaddress(cch), ccpeerport(cch) ) ;

            }

          }

          // Store the string as a variable

          dosetdata(var, do_string, msgvalue, strlen(msgvalue), "/value") ;

        }

      }

    }

    varindex++ ;

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

int ccsendmessage(CHROMECAST *cch, char *sender, char *receiver, char *namespace, const char *message, ...)
{
  if (!cch || !netisconnected(cch->ssl)) return 0 ;

  int len=-1 ;

  if (cch->sendobject) dodelete(cch->sendobject) ;

  cch->sendobject = donew() ;
  if (!cch->sendobject) return 0 ;

  // Expand varargs

  va_list args;

  mem *messagebuf=NULL ;

  va_start (args, message);
  size_t messagelen=vsnprintf(messagebuf, 0, message, args) ;

  // Extra 1024 to allow for string replacements later on
  messagebuf=mem_malloc(messagelen+1+1024) ;
  if (!messagebuf) return 0 ;

  va_start (args, message);
  vsnprintf(messagebuf, messagelen+1, message, args) ;

  // Substitute Variables

  ccexpandvariables(cch->httpsessionvars, messagebuf, 0, 1) ;
  ccexpandvariables(cch->vars, messagebuf, 0, 0) ;

  // Update sender, receiver and namespace as required

  char *senders = sender ;
  char *receivers = receiver ;
  char *namespaces = namespace ;

  DATAOBJECT *vh ;
  int i=0 ;
  char varname[128] ;
  while (vh=dochild(donoden(cch->vars,i))) {
     char *variable = dogetdata(vh, do_string, NULL, "/variable") ;
     char *value = dogetdata(vh, do_string, NULL, "/value") ;
     if (variable && value) {
       snprintf(varname, sizeof(varname)-1, "$%s", variable) ;
       if (strcmp(senders, varname)==0) senders=value ;
       if (strcmp(receivers, varname)==0) receivers=value ;
       if (strcmp(namespaces, varname)==0) namespaces=value ;
     }
     i++ ;
  }

  // Populate sendobject

  dosetuint(cch->sendobject, do_uint32, 0x00, "/f1") ;                            // Protocol Version

  dosetdata(cch->sendobject, do_string, senders, strlen(senders), "/f2") ;        // Source ID
  dosetdata(cch->sendobject, do_string, receivers, strlen(receivers), "/f3") ;    // Destination ID
  dosetdata(cch->sendobject, do_string, namespaces, strlen(namespaces), "/f4") ;  // Namespace
  dosetuint(cch->sendobject, do_uint32, 0, "/f5") ;                               // Payload Type (0 = string)
  dosetdata(cch->sendobject, do_string, messagebuf, strlen(messagebuf), "/f6") ;  // Payload Data (JSON string)

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
  dorenamenode(cch->sendobject, "/f6", "message") ;

  if ( getenv("PINGLOGENABLE") || (sender && strcmp(sender, "Tr@n$p0rt-0")!=0) ) {
    logmsg( LOG_DEBUG, "SEND%s: %s to %s:%d", (r1==4 && r2==len)?"":"FAILED", 
            doasjson(cch->sendobject, NULL), ccipaddress(cch), ccpeerport(cch)) ;
  }

  doexpandfromjson(cch->sendobject, "/message") ; 

  mem_free(messagebuf) ;

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


