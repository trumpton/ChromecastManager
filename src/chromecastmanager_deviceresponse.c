//
// chromecastmanager_deviceresponse
//
// Handler function for chromecast device response
//
//
// Sequence
//
// > CONNECT sender-0 to receiver-0
// >1 SET_VOLUME
// >2 LAUNCH application
// <  DEVICE_UPDATED (multi-zone)
// <1 RECEIVER_STATUS (reports volume set)
// <2 RECEIVER_STATUS (reports transportId)
// > CONNECT sender-1 to transportId
// >3 LOAD media / auto-play
// <3 LOAD_FAILED with reason or ....
// <3 MEDIA_STATUS (reports BUFFERING)
// <3 MEDIA_STATUS (reports PLAYING)
//

#include <string.h>
#include <stdio.h>
//#include <unistd.h>
//#include <stdlib.h>
//#include <time.h>

// Protobuf over TLS connection to chromecast devices
#include "chromecast_interface.h"

// Chromecastmanager httpd server
#include "libtools/httpd.h"

// Logging
#include "libtools/log.h"

#include "chromecastmanager.h"

#include "libdataobject/dataobject.h"


/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// @brief Handle chromecast device response, passing data back to the http session if open
// Returns true if queried packet processed
//

int chromecast_device_response_process(CHROMECAST *cch, HTTPD *httpsh) 
{

  DATAOBJECT *doh = ccgetmessage(cch) ;

  char *namespace = dogetdata(doh, do_string, NULL, "/namespace") ;
  char *type = dogetdata(doh, do_string, NULL, "/message/type") ;

  // Received message parameters

  char *sender = dogetdata(doh, do_string, NULL, "/sender") ;
  char *receiver = dogetdata(doh, do_string, NULL, "/receiver") ;
  unsigned long int requestid = 0 ;
  dogetuint(doh, do_int64, &requestid, "/message/requestId") ;

  // Sent message parameters

  DATAOBJECT *dosh = ccgetsentmessage(cch) ;

  char *sentnamespace = dogetdata(dosh, do_string, NULL, "/namespace") ;
  char *sentsender = dogetdata(dosh, do_string, NULL, "/sender") ;
  char *sentreceiver = dogetdata(dosh, do_string, NULL, "/receiver") ;
  unsigned long int sentrequestid = 0 ;
  dogetuint(dosh, do_int64, &sentrequestid, "/message/requestId") ;


  ///////////////////////////////////
  // Respond to PING

  if ( namespace && strcmp(namespace, "urn:x-cast:com.google.cast.tp.heartbeat")==0 && 
       type && strcasecmp(type, "PING")==0 ) {

    ccsendheartbeatmessage((cch), "PONG") ;
    return 0 ;

  }

  ///////////////////////////////////
  // Ignore PONG responses

  else if ( namespace && strcmp(namespace, "urn:x-cast:com.google.cast.tp.heartbeat")==0 && 
       type && strcasecmp(type, "PONG")==0 ) {

    // Response to our ping

    logmsg( LOG_DEBUG, "Received Pong from %s:%d",
            ccipaddress(cch), ccpeerport(cch) ) ;
    return 0 ;

  }

  ///////////////////////////////////
  // Process Macro 'POST' responses

  else if ( cch->macro ) {

   logmsg( LOG_DEBUG, "Received %s:%s - processing macro step %d ...", 
           namespace, type, cch->macroindex) ;

    return chromecast_macro_process(httpsh, cch) ;

  }

  ///////////////////////////////////
  // Provide response to query

  else if ( ccgetflag(cch)&CC_FLAG_JSONQUERY &&

            sender && sentreceiver && strcmp(sender, sentreceiver)==0 &&

            sentsender && receiver && ( strcmp(receiver, sentsender)==0 || strcmp(receiver, "*")==0) &&
                ( 
                    strcmp(namespace, CC_NAMESPACE_CONNECTION)==0  ||
                   ( 
                       namespace && sentnamespace && 
                       strcmp(namespace, sentnamespace)==0 &&
                      (requestid == sentrequestid || requestid == 0 )
                   )
               ) 
           ) {

    // Response to our request 

    if (httpsh) {

     logmsg( LOG_DEBUG, "Sending %s:%s response back to http client", 
             namespace, type) ;

      dosetdata(doh, do_string, "OK", 2, "/status") ;
      char *json = doasjson(doh, NULL) ;
      hsend(httpsh, 200, "application/json", json) ;
    }

    ccclearflag(cch, CC_FLAG_JSONQUERY) ;

    return 1 ;

  }

  ///////////////////////////////////
  // No handler, just log

  logmsg( LOG_DEBUG, "Ignoring %s:%d:%s from %s:%d", 
          namespace, requestid, type,
          ccipaddress(cch), ccpeerport(cch) ) ;

  return 0 ;

}



