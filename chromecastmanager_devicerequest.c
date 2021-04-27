//
// chromecastmanager_devicerequest
//
// Handler function for chromecast devicerequest
//
//

#include <string.h>
#include <stdio.h>
//#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
//#include <time.h>


#include "chromecastmanager.h"

// MDNS data management
#include "chromecast_mdns.h"

#include "libdataobject/dataobject.h"

#include "libtools/log.h"

// Help text for httpd server /help query
#include "html/html_filesystem.h"


/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// @brief Process http request
// Returns index of device processed or -1
//

int chromecast_device_request_process(HTTPD *httpsh, CHROMECAST **cclist, int maxcc) 
{
  if (!httpsh) return -1 ;

  // Get uri and query parameters

  char *device = hgeturiparamstr(httpsh, "device") ;
  char *uri = hgeturi(httpsh) ;

  // Search for device

  char *file, *mediatype ;
  int len ;
  int index=-1 ;

  if (device) index = chromecast_finddevice(device, maxcc) ;

  // Search for file in filesystem

  int filefound = html_filesystem(uri, &file, &mediatype, &len) ;

  // Dispatch to appropriate query handler

  if (filefound) {

    /////////////////////////////////////
    // URI: filesystem file


    logmsg(LOG_INFO, "Received request: %s from %s:%d - returning file", 
                     uri, 
                     hpeeripaddress(httpsh), hpeerport(httpsh)) ;

    hsendb(httpsh, 200, mediatype, file, len) ;
    index=-1 ;

  } else if (strcasecmp(uri, "/devicelist")==0) {

    /////////////////////////////////////
    // URI: /devicelist

    chromecast_device_request_process_devicelist(httpsh, cclist, maxcc) ;
    index=-1 ;

    logmsg(LOG_INFO, "Received request: %s from %s:%d - returning file", 
                     uri, 
                     hpeeripaddress(httpsh), hpeerport(httpsh)) ;


  } else if (index<0 || !cclist[index]) {

    /////////////////////////////////////
    // 503 - Device not found

    hsend( httpsh, 503, "application/json", 
             "{\"status\":\"NODEVICE\","
             "\"info\":\"chromecast device not found\"}" ) ;
    index=-1 ;

    logmsg(LOG_INFO, "Received request: %s from %s:%d - device '%s' not found", 
                     uri, 
                     hpeeripaddress(httpsh), hpeerport(httpsh),
                     device?device:"undefined") ;

  } else if (strcmp(uri, "/jsonquery")==0) {

    /////////////////////////////////////
    // URI: /jsonquery?device=ip&port=p


    if (!chromecast_device_request_process_jsonquery(httpsh, cclist[index])) {
      index=-1 ;
    }

    logmsg(LOG_INFO, "Received request: %s from %s:%d - json query %s", 
                     uri, 
                     hpeeripaddress(httpsh), hpeerport(httpsh),
                     index>=0?"OK":"failed") ;


  } else if (strcasecmp(uri, "/command")==0) {

    /////////////////////////////////////
    // URI: /command (first pass)

    if (!chromecast_device_request_process_command(httpsh, cclist[index])) {
      index=-1 ;
    }

    logmsg(LOG_INFO, "Received request: %s from %s:%d - command %s", 
                     uri, 
                     hpeeripaddress(httpsh), hpeerport(httpsh),
                     index>=0?"OK":"failed") ;

  } else {
 
    // html_filesystem originally returned file not found
    // and updated mediatype and file to point to 404.html

    logmsg(LOG_INFO, "Received request: %s from %s:%d - returning 404, file not found", 
                     uri, 
                     hpeeripaddress(httpsh), hpeerport(httpsh)) ;

    hsend(httpsh, 404, mediatype, file) ;
    index=-1 ;

  }


  return index ;

}


/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// @brief Process Device List
// Returns true

int chromecast_device_request_process_devicelist(HTTPD *httpsh, CHROMECAST **cclist, int maxcc) 
{
  if (!httpsh || !cclist) return 0 ;

  int outputcount=0 ;
  char buf[32768] ;
  char line[1024] ;
  sprintf(buf, "{\n  \"devicelist\":[\n") ;

  int numdevices=0 ;

  for (int i=0; i<maxcc; i++) {

    chromecast_mdns_record *e = chromecast_mdns_at(i) ;

    if (e->friendlyname) {

      numdevices++ ;
      if ((outputcount++)>0) { strcat(buf, ",\n") ; }

      char *appid=(cclist[i])?ccgetmediaconnectiondetails(cclist[i],CC_MCD_APPID):"" ;
      char *playerstate=(cclist[i])?ccgetmediaconnectiondetails(cclist[i],CC_MCD_PLAYERSTATE):"" ;
      char *displayname=(cclist[i])?ccgetmediaconnectiondetails(cclist[i],CC_MCD_DISPLAYNAME):"" ;
      char *statustext=(cclist[i])?ccgetmediaconnectiondetails(cclist[i],CC_MCD_STATUSTEXT):"" ;
      char *transportid=(cclist[i])?ccgetmediaconnectiondetails(cclist[i],CC_MCD_TRANSPORTID):"" ;
      char *sessionid=(cclist[i])?ccgetmediaconnectiondetails(cclist[i],CC_MCD_SESSIONID):"" ;

      // notes:
      //  transportId is used as the receiver when connecting to the media application
      //  playerstate is unknown on startup, so it is 'guessed' based on statustext being available

      sprintf(line, "    {\n"
                    "      \"id\":\"%s\",\n"
                    "      \"networkName\":\"%s\",\n"
                    "      \"friendlyName\":\"%s\",\n"
                    "      \"deviceName\":\"%s\",\n"
                    "      \"ipAddress\":\"%s\",\n"
                    "      \"port\":%d,\n"
                    "      \"netClass\":\"%s\",\n"
                    "      \"connection\":\"%s\",\n"
                    "      \"idle\":%d,\n"
                    "      \"media\": {\n"
                    "        \"appId\": \"%s\",\n"
                    "        \"playerState\": \"%s\",\n"
                    "        \"displayName\": \"%s\",\n"
                    "        \"statusText\": \"%s\",\n"
                    "        \"transportId\": \"%s\",\n"
                    "        \"sessionId\": \"%s\"\n"
                    "      }\n"
                    "    }",
              (e->id)?(char *)(e->id):"",
              (e->networkname)?(char *)(e->networkname):"",
              (e->friendlyname)?(char *)(e->friendlyname):"",
              (e->devicename)?(char *)(e->devicename):"",
              (e->ipaddress)?(char *)(e->ipaddress):"",
              (e->port),
              (e->netclass)?(char *)(e->netclass):"",
              (cclist[i])?"connected":"disconnected",
              (cclist[i])?ccidletime(cclist[i]):0,
              appid,
              (*playerstate) ? playerstate : (*statustext) ? "PLAYING" : "",
              displayname,
              statustext,
              transportid,
              sessionid
      ) ;

      if (strlen(buf)+strlen(line)<(sizeof(buf)-32)) {
        strcat(buf,line) ;
      }

    }

  }

  snprintf(line, sizeof(line)-1, "\n  ],\n  \"deviceCount\":%d,\n  \"status\":\"OK\"\n}\n", numdevices) ;
  strcat(buf, line) ;

  hsend(httpsh, 200, "application/json", buf) ;
  return 1 ;
}


/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// @brief Process Chromecast JSON Query
// Returns true

int chromecast_device_request_process_jsonquery(HTTPD *httpsh, CHROMECAST *cch) 
{

  char *body = hgetbody(httpsh) ;

  if (!body) {

      hsend( httpsh, 400, "application/json", 
             "{\"status\":\"BADREQ\","
             "\"info\":\"jsonrequest has no body data\"}" ) ;

  } else {

    // Extract parameters from body

    DATAOBJECT *request = donew() ;
    dofromjson(request, body) ;

    char *sender = dogetdata(request, do_string, NULL, "/sender") ;
    char *receiver = dogetdata(request, do_string, NULL, "/receiver") ;
    char *namespace = dogetdata(request, do_string, NULL, "/namespace") ;
    DATAOBJECT *message = dochild(dofindnode(request, "/message")) ;
    unsigned long int requestid = 0 ;
    dogetuint(request, do_uint32, &requestid, "/message/requestId") ;

    if (!sender || !receiver || !namespace || index<0 || !message) {

        hsend( httpsh, 400, "application/json", 
               "{\"status\":\"BADREQ\","
               "\"info\":\"query missing device or json body missing sender, receiver or namespace\"}" ) ;
      return 0 ;

    } else {

      // Send request to device
      char *json = doasjson(message,NULL) ;

      logmsg(LOG_INT, "POSTING '%s' to CC Device (%s)", json, namespace) ;

      if (!ccsendmessage(cch, sender, receiver, namespace, json)) {

        hsend( httpsh, 501, "application/json", "{\"status\":\"NETERR\","
                                                "\"info\":\"unable to communicate with chromecast\","
                                                "\"errorcode\":1}" ) ;
        return 0 ;

      }


    }
      
    dodelete(request) ;

  }

  return 1 ;

}



/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// @brief Process Chromecast Command
// Returns true

int chromecast_device_request_process_command(HTTPD *httpsh, CHROMECAST *cch) 
{

 float volume ;
 char *src = hgeturiparamstr(httpsh, "uri") ;
 char *appid = ccgetmediaconnectiondetails(cch,CC_MCD_APPID) ;

 int success=1 ;
 int completed=1 ;

// TODO: don't really need completed

 if (hgeturiparamfloat(httpsh, "volume", &volume)) {

   if (volume<0) { volume=0.0 ; }
   if (volume>1) { volume=1.0 ; }

   success &= ccsendmessage( cch, "sender-0", "receiver-0", 
                             CC_NAMESPACE_RECEIVER,
                             "{\"requestId\":%d,\"type\":\"SET_VOLUME\",\"volume\":{\"level\":%f}}",
                             ccnextrequestid(cch), volume+0.05) ;

  }

  if (src) {

    // LAUNCH the media receiver if a play request is pending
    // note that the media receiver _may_ already be running
    // but this function deliberaltely forces a new one to be
    // launched. Mainly for the 'beep' user experience on the 
    // speaker.
    //
    // The device_response_process will pick up the response
    // and callback the launchcomplete, loadcomplete, loadfailed
    // functions as required by the flags.
    //

    ccclearflag(cch, CC_FLAG_LOADSENT) ;
    ccsetflag(cch, CC_FLAG_PROCESSING) ;
 
    success &= ccsendmessage( cch, "sender-0", "receiver-0", 
                              CC_NAMESPACE_RECEIVER,
                              "{\"requestId\":%d,\"type\":\"LAUNCH\",\"appId\":\"%s\"}",
                               ccnextrequestid(cch), CC_APPID_DEFAULT_MEDIA_RECEIVER) ;

    completed=!success ;


  }

  if (!completed) {

    // Completion required later

    return success ;

  } else if (success) {

    hsend(httpsh, 200, "application/json", "{\"status\":\"OK\"}") ;

    ccclearflag(cch, CC_FLAG_PROCESSING) ;

    return 1;

  } else {

    hsend( httpsh, 200, "application/json", "{\"status\":\"NETERR\","
                                            "\"info\":\"unable to communicate with chromecast\","
                                            "\"errorcode\":2}" ) ;
    ccclearflag(cch, CC_FLAG_PROCESSING) ;

    return 0 ;

  }

}


/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// @brief Completes Chromecast Command Processing
// Returns true on complete

int chromecast_device_request_process_command_sendload(HTTPD *httpsh, CHROMECAST *cch) 
{
  int success=1 ;

  if (!httpsh || !cch) return 0 ;

  /////////////////////////////////////
  // URI: /command (second pass)

  char *src = hgeturiparamstr(httpsh, "uri") ;
  if (!src) {return 1 ; }

  char *transportid = ccgetmediaconnectiondetails(cch, CC_MCD_TRANSPORTID) ;
  char *appid = ccgetmediaconnectiondetails(cch,CC_MCD_APPID) ;

  if (transportid && strcmp(appid,CC_APPID_DEFAULT_MEDIA_RECEIVER)==0) {

    // Player has been launched and transport ID
    // has been collected, so configure and launch

    int sleep=0 ;
    hgeturiparamint(httpsh, "sleep", &sleep) ;

    char *title = hgeturiparamstr(httpsh, "title") ;
    char *mediatype = hgeturiparamstr(httpsh, "mediatype") ;

    success &= ccsendmessage( cch, "sender-3", transportid, 
                              CC_NAMESPACE_CONNECTION,
                              "{\"type\":\"CONNECT\"}") ;

    success &= ccsendmessage( cch, "sender-3", transportid, 
                              CC_NAMESPACE_MEDIA,
                              "{\"requestId\":%d,\"type\":\"LOAD\",\"autoplay\":true,"
                              "\"media\":{\"contentId\":\"%s\",\"streamType\":\"%s\","
                              "\"contentType\":\"%s\",\"metadata\":{\"metadataType\":0,"
                              "\"title\":\"%s\"}}}",
                              ccnextrequestid(cch), 
                              src, "LIVE", 
                              mediatype?mediatype:"audio/mpeg",
                              title?title:"Media" ) ;

    success &= ccsendmessage( cch, "sender-3", transportid, 
                              CC_NAMESPACE_MEDIA,
                              "{\"type\":\"GET_STATUS\",\"requestId\":%d}",
                              ccnextrequestid(cch)) ;
  
    if (success) ccsetflag(cch, CC_FLAG_LOADSENT) ;

  }


  if (!success) {

    hsend( httpsh, 501, "application/json", "{\"status\":\"NETERR\","
                                            "\"info\":\"unable to communicate with chromecast\","
                                            "\"errorcode\":3}" ) ;
    ccclearflag(cch, CC_FLAG_PROCESSING) ;

  }

fail:

  return success ;

}

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// @brief Completes Chromecast Command Processing
// Returns true on complete

int chromecast_device_request_process_command_loadcomplete(HTTPD *httpsh, CHROMECAST *cch) 
{

  /////////////////////////////////////
  // URI: /command (third pass)

  DATAOBJECT *doh = ccgetmessage(cch) ;

  if (!cch || !doh) return 0 ;

  char *state = dogetdata(doh, do_string, NULL, "/message/status/0/playerState") ;

  if ( state && 
       ( strcasecmp(state, "BUFFERING")==0 || strcasecmp(state, "PLAYING")==0 ) ) {

    int success=1 ;

    ccclearflag(cch, CC_FLAG_LOADSENT|CC_FLAG_PROCESSING) ;

    char *transportid = ccgetmediaconnectiondetails(cch, CC_MCD_TRANSPORTID) ;

    success &= ccsendmessage( cch, "sender-3", transportid, 
                              CC_NAMESPACE_MEDIA,
                              "{\"type\":\"VOLUME\",\"requestId\":%d,"
                              "\"volume\":{\"level\":1.0,\"muted\":false}}",
                              ccnextrequestid(cch)) ;


/*
    success &= ccsendmessage( cch, "sender-3", transportid, 
                              CC_NAMESPACE_CONNECTION,
                              "{\"type\":\"CLOSE\",\"requestId\":%d}",
                              ccnextrequestid(cch)) ;
*/


    if (httpsh) {
      if (success) {
        hsend(httpsh, 200, "application/json", "{\"status\":\"OK\"}") ;
      } else {
        hsend( httpsh, 501, "application/json", "{\"status\":\"NETERR\","
                                                "\"info\":\"unable to communicate with chromecast\","
                                                "\"errorcode\":4}" ) ;
      }
    }
    return success ;

  }

 // Stay in this state until a response is received

 return 1 ;

}


/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// @brief Completes Chromecast Command Processing
// Returns true on complete

int chromecast_device_request_process_command_loadfailed(HTTPD *httpsh, CHROMECAST *cch) 
{

  /////////////////////////////////////
  // URI: /command (third pass)

  if (!cch) return 0 ;

  DATAOBJECT *doh = ccgetmessage(cch) ;
  if (!doh) return 0 ;

  char *transportid = ccgetmediaconnectiondetails(cch, CC_MCD_TRANSPORTID) ;

  int success=1 ;

  success &= ccsendmessage( cch, "sender-3", transportid, 
                            CC_NAMESPACE_MEDIA,
                            "{\"type\":\"GET_STATUS\",\"requestId\":%d}",
                            ccnextrequestid(cch)) ;

  
  float volume=0.2 ;

  for (int j=0; j<3; j++) {

    success &= ccsendmessage( cch, "sender-0", "receiver-0", 
                            CC_NAMESPACE_RECEIVER,
                            "{\"requestId\":%d,\"type\":\"SET_VOLUME\",\"volume\":{\"level\":%f}}",
                            ccnextrequestid(cch), volume+0.05) ;

    success &= ccsendmessage( cch, "sender-0", "receiver-0", 
                            CC_NAMESPACE_RECEIVER,
                            "{\"requestId\":%d,\"type\":\"SET_VOLUME\",\"volume\":{\"level\":%f}}",
                            ccnextrequestid(cch), volume-0.05) ;
  }


// TODO: THIS MESSAGE IS BEING IGNORED
// THE GET_STATUS MESSAGES SEEM TO WORK
// THIS IS IN A SEQUENCE: LOAD / GET_STATUS / LOAD
// WHEN THE FIRST LOAD FAILS

  success &= ccsendmessage( cch, "sender-3", transportid, 
                      CC_NAMESPACE_MEDIA,
                      "{\"requestId\":%d,\"type\":\"LOAD\",\"autoplay\":true,"
                      "\"media\":{\"contentId\":\"http://%s:%d/alert.mp3\","
                      "\"streamType\":\"LIVE\","
                      "\"contentType\":\"audio/mpeg\","
                      "}}",
                      ccnextrequestid(cch), 
                      httpd_ipaddress(), httpd_port()) ;

  success &= ccsendmessage( cch, "sender-3", transportid, 
                            CC_NAMESPACE_MEDIA,
                            "{\"type\":\"GET_STATUS\",\"requestId\":%d}",
                            ccnextrequestid(cch)) ;
  
  if (httpsh) {

    unsigned long int errorcode = 0 ;
    char buf[512] ;

// TODO: Error Codes: https://developers.google.com/cast/docs/web_receiver/error_codes

    dogetuint(doh, do_int64, &errorcode, "/message/detailedErrorCode") ;
    snprintf(buf, sizeof(buf)-1, "{\"status\":\"LOADFAILED\","
                                 "\"errorcode\":%ld,"
                                 "\"info\":\"media stream loading failed (check uri)\"}", errorcode) ;
    hsend(httpsh, 501, "application/json", buf) ;

  }

  ccclearflag(cch, CC_FLAG_LOADSENT|CC_FLAG_PROCESSING) ;

  return 1 ;

}


