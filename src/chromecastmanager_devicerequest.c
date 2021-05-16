//
// chromecastmanager_devicerequest
//
// Handler function for chromecast devicerequest
//
//

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <dirent.h>

#include "chromecastmanager.h"

// MDNS data management
#include "chromecast_mdns.h"

#include "libdataobject/dataobject.h"

#include "libtools/log.h"

// Help text for httpd server /help query
#include "html/html_filesystem.h"


char *scriptfilename(char *uri) ;
char *loadscriptfromfile(char *uri) ;

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

  if (device && *device!='\0') {

    // Device was passed as a uri parameter

    index = chromecast_finddevice(device, maxcc) ;

  }

  if (index<0) {

    // Check JSON body for device name

    DATAOBJECT *body = donew() ;
    if (!body) return -1 ;
    char *bodytxt = hgetbody(httpsh) ;

    if (bodytxt) {
      dofromjson(body, bodytxt) ;
      char *dodevice = dogetdata(body, do_string, NULL, "/device") ;
      if (dodevice) {
        index = chromecast_finddevice(dodevice, maxcc) ;
      }
    }
    dodelete(body) ;
 
  }

  // Log friendlyname string

  char *friendlyname = (index<0) ? NULL : chromecast_mdns_at(index)->friendlyname ;
  logmsg(LOG_DEBUG, "Friendly name for request: %s\n", friendlyname ? friendlyname : "unknown device") ;

  // Search for file in filesystem

  int filefound = html_filesystem(uri, &file, &mediatype, &len) ;

  // Dispatch to appropriate query handler

  if (filefound) {

    /////////////////////////////////////
    // URI: filesystem file

    hsendb(httpsh, 200, mediatype, file, len) ;
    index=-1 ;

    logmsg(LOG_INFO, "Received request: %s from %s:%d - returning script %s.json", 
                     uri, 
                     hpeeripaddress(httpsh), hpeerport(httpsh),
                     uri) ;

  } else if (strcasecmp(uri, "/serverinfo")==0) {

    /////////////////////////////////////
    // URI: /serverinfo

    chromecast_device_request_process_serverinfo(httpsh) ;
    index=-1 ;

    logmsg(LOG_INFO, "Received request: %s from %s:%d - returning media list", 
                     uri, 
                     hpeeripaddress(httpsh), hpeerport(httpsh)) ;

  } else if (strcasecmp(uri, "/devicelist")==0) {

    /////////////////////////////////////
    // URI: /devicelist

    chromecast_device_request_process_devicelist(httpsh, cclist, maxcc) ;
    index=-1 ;

    logmsg(LOG_INFO, "Received request: %s from %s:%d - returning device list", 
                     uri, 
                     hpeeripaddress(httpsh), hpeerport(httpsh)) ;

  } else if (strcmp(uri, "/jsonquery")!=0 && strcmp(uri, "/jsonscript")!=0 && !scriptfilename(uri)) {
 
    /////////////////////////////////////
    // 404 - File not found
    //
    // All the remaining uris require that
    // the device to be specified in the uri
    // So if the request is not for one of 
    // the json queries / scripts by the time 
    // the process flow has reached here, 
    // bail now 

    hsend(httpsh, 404, "text/html", "/404.html") ;
    index=-1 ;

    logmsg(LOG_INFO, "Received request: %s from %s:%d - returning 404, file not found", 
                     uri, 
                     hpeeripaddress(httpsh), hpeerport(httpsh)) ;

  } else if (index<0 || !cclist[index]) {

    /////////////////////////////////////
    // 503 - Device not found

    hsend( httpsh, 503, "application/json", 
             "{\"status\":\"NODEVICE\","
             "\"info\":\"chromecast device not found\"}" ) ;
    index=-1 ;

    logmsg(LOG_NOTICE, "Received request: %s from %s:%d - device '%s' not found", 
                       uri, hpeeripaddress(httpsh), hpeerport(httpsh),
                       (device && *device!='\0')?device:"undefined") ;

  } else if (strcmp(uri, "/jsonquery")==0) {

    /////////////////////////////////////
    // URI: /jsonquery?device=ip

    if (!chromecast_device_request_process_jsonquery(httpsh, cclist[index])) {
      index=-1 ;
    }

    logmsg(LOG_INFO, "Received request: %s from %s:%d - json query", 
                       uri, hpeeripaddress(httpsh), hpeerport(httpsh)) ;


  } else if (strcmp(uri, "/jsonscript")==0) {

    /////////////////////////////////////
    // URI: /jsonscript?device=devicename

    char *json = hgetbody(httpsh) ;

    if (!json) {

      logmsg(LOG_ERR, "Processing request: %s from %s:%d - error in request body - FAILED", 
                       uri, hpeeripaddress(httpsh), hpeerport(httpsh)) ;
      index=-1 ;

    } else if (chromecast_macro_load(httpsh, cclist[index], json)) {

      logmsg(LOG_INFO, "Received request: %s from %s:%d - json script", 
                       uri, hpeeripaddress(httpsh), hpeerport(httpsh)) ;

    } else {

      logmsg(LOG_ERR, "Processing request: %s from %s:%d - json script loading error - FAILED", 
                       uri, hpeeripaddress(httpsh), hpeerport(httpsh)) ;
      index=-1 ;

    }


  } else if (scriptfilename(uri)) {

    /////////////////////////////////////
    // URI: /scriptname?device=devicename

    char *json = loadscriptfromfile(uri) ;

    if (!json) {

      logmsg(LOG_ERR, "Processing request: %s from %s:%d - script not found - FAILED", 
                       uri, 
                       hpeeripaddress(httpsh), hpeerport(httpsh)) ;
      index=-1 ;

    } else if (chromecast_macro_load(httpsh, cclist[index], json)) {

      logmsg(LOG_INFO, "Received request: %s from %s:%d - json script", 
                       uri, 
                       hpeeripaddress(httpsh), hpeerport(httpsh)) ;

    } else {

      logmsg(LOG_ERR, "Processing request: %s from %s:%d - json script loading error - FAILED", 
                       uri, 
                       hpeeripaddress(httpsh), hpeerport(httpsh)) ;
      index=-1 ;

    }

    if (json) free(json) ;

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

      sprintf(line, "    {\n"
                    "      \"friendlyName\":\"%s\",\n"
                    "      \"ipAddress\":\"%s\",\n"
                    "      \"port\":%d,\n"
                    "      \"id\":\"%s\",\n"
                    "      \"networkName\":\"%s\",\n"
                    "      \"deviceName\":\"%s\",\n"
                    "      \"netClass\":\"%s\",\n"
                    "      \"connection\":\"%s\",\n"
                    "      \"idle\":%d,\n"
                    "      \"vars\":{\n",
              (e->friendlyname)?(char *)(e->friendlyname):"",
              (e->ipaddress)?(char *)(e->ipaddress):"",
              (e->port),
              (e->id)?(char *)(e->id):"",
              (e->networkname)?(char *)(e->networkname):"",
              (e->devicename)?(char *)(e->devicename):"",
              (e->netclass)?(char *)(e->netclass):"",
              (cclist[i])?"connected":"disconnected",
              ccidletime(cclist[i])) ;

      if (strlen(buf)+strlen(line)<(sizeof(buf)-32)) {
        strcat(buf,line) ;
      }

      int numvariables=0 ;
      char *variable ;

      while ( variable = ccgetwatchvarnameat(cclist[i], numvariables) ) {

        sprintf( line, "        \"%s\":\"%s\",\n", 
                 variable, ccgetwatchat(cclist[i], numvariables)) ;

        if (strlen(buf)+strlen(line)<(sizeof(buf)-32)) {
          strcat(buf,line) ;
        }

        numvariables++ ;
  
      }

      snprintf(line, sizeof(line), "        \"count\":%d\n      }\n    }", numvariables) ;
      if (strlen(buf)+strlen(line)<(sizeof(buf)-32)) {
        strcat(buf, line) ;
      }

    }

  }

  snprintf(line, sizeof(line)-1, "\n  ],\n  \"deviceCount\":%d,\n  \"status\":\"OK\"\n}\n", numdevices) ;

  if (strlen(buf)+strlen(line)<(sizeof(buf)-32)) {
    strcat(buf, line) ;
  }

  hsend(httpsh, 200, "application/json", buf) ;
  return 1 ;
}



int chromecast_device_request_process_serverinfo(HTTPD *httpsh) 
{
  if (!httpsh) return 0 ;

  char buf[32768] ;
  char line[1024] ;

  char *ip = httpd_ipaddress() ;
  int port = httpd_port() ;

  snprintf(buf, sizeof(buf)-1,
               "{\n"
               "  \"vars\": {\n"
               "    \"serverIpAddress\": \"%s\",\n"
               "    \"serverPort\": %d\n"
               "  },\n"
               "  \"media\": {\n"
               "      \"%s\": \"http://%s:%d/%s\",\n"
               "      \"%s\": \"http://%s:%d/%s\",\n"
               "      \"%s\": \"http://%s:%d/%s\",\n"
               "      \"%s\": \"http://%s:%d/%s\",\n"
               "      \"%s\": \"http://%s:%d/%s\",\n"
               "      \"%s\": \"http://%s:%d/%s\",\n"
               "      \"%s\": \"http://%s:%d/%s\"\n"
               "  },\n"
               "  \"scripts\": [",
               ip, port,
               "logo",   ip, port, "logo.png", 
               "pp",     ip, port, "pp.png", 
               "img",     ip, port, "testimg1.jpg", 
               "test1", ip, port, "test1.ogg", 
               "test2", ip, port, "test2.ogg", 
               "alert1", ip, port, "alert1.ogg", 
               "alert2", ip, port, "alert2.ogg") ;
 
  DIR *dir;
  struct dirent *ent;
  int index=0 ;
  if ( dir = opendir(getscriptfolder()) ) {

    while ( ent = readdir(dir) ) {

      char *fnm = ent->d_name ;
      char *ext = strchr(fnm, '.') ;

      if (ext && strcmp(ext, ".json")==0) {
        *ext='\0' ;
        snprintf(line, sizeof(line)-1, "%s\n      \"%s\"", (index++)!=0?",":"", fnm);
        if (strlen(line)+strlen(buf)<sizeof(buf)-1) strcat(buf,line) ;
      }
    }
  }
  closedir (dir);

  snprintf(line, sizeof(line)-1, "\n  ],\n  \"status\":\"OK\"\n}") ;
  if (strlen(line)+strlen(buf)<sizeof(buf)-1) strcat(buf,line) ;

  hsend( httpsh, 200, "application/json", buf) ;

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

    // Clear Flags
    ccclearflag(cch, CC_FLAG_ALL) ;
    ccsetflag(cch, CC_FLAG_JSONQUERY) ;

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



//
// @brief Return filename of script (if it exists)
//

char *scriptfilename(char *uri) 
{
  static char path[1024] ;

  snprintf(path, sizeof(path)-32, "%s/", getscriptfolder()) ;
  int p=strlen(path) ;
  int u=0 ;
  while (uri[u++]!='\0' && p<sizeof(path)-32) {
    if (uri[u]!='/') path[p++]=uri[u] ;
  }
  if (p==strlen(path)) return NULL ;

  path[p]='\0' ;
  strcat(path,".json") ;

  // test to see that file is accessible

  FILE *fp ;
  fp=fopen(path, "r") ;
  if (!fp) return NULL ;

  fclose(fp) ;
  return path ;

}


//
// @brief Read script from file and return pointer to data
//

char *loadscriptfromfile(char *uri) 
{
  if (!uri) return NULL ;

  // Find and open file

  char *path = scriptfilename(uri) ;
  FILE *fp ;
  fp=fopen(path, "r") ;
  if (!fp) return NULL ;

  // Load data from file

  int ch ;
  int i=0;
  char *buf=malloc(32) ;
  int buflen=sizeof(buf) ;

  while ( (ch=fgetc(fp)) > 0 ) {
    if (i>=(buflen-1)) {
      buflen+=32 ;
      buf=realloc(buf, buflen) ;
      if (!buf) goto fail ;
    }
    buf[i++]=ch ;
  }

  buf[i]='\0' ;

fail:
  fclose(fp) ;
  return buf ;
}


