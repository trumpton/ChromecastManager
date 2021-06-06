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
#include "libtools/execute.h"

// Help text for httpd server /help query
#include "html/html_filesystem.h"


char *scriptfilename(char *uri) ;
mem *loadscriptfromfile(char *uri) ;
mem *chromecast_device_request_malloc_convert_body(char *json, char *preprocess) ;
int chromecast_device_expand_json(DATAOBJECT *body, char *json, DATAOBJECT *sessionvars) ;
int chromecast_device_update_sessionvars(DATAOBJECT *sessionvars, char *json) ;


/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// @brief Process http request
// Returns index of device processed or -1
//

int chromecast_device_request_process(HTTPD *httpsh, CHROMECAST **cclist, int maxcc) 
{
  int index = -1 ;

  if (!httpsh || !cclist || !cclist[0]) return -1 ;


  ///////////////////////////////////////////////////
  // Capture uri

  char *uri = hgeturi(httpsh) ;


  ///////////////////////////////////////////////////
  // Dispatch to appropriate query handler

  char *file, *mediatype ;
  int len ;

  if (html_filesystem(uri, &file, &mediatype, &len)) {

    /////////////////////////////////////
    // URI: filesystem file

    hsendb(httpsh, 200, mediatype, file, len) ;

    logmsg(LOG_INFO, "Received request: %s from %s:%d - returning script %s.json", 
                     uri, 
                     hpeeripaddress(httpsh), hpeerport(httpsh),
                     uri) ;


  } else if (strcasecmp(uri, "/serverinfo")==0) {

    /////////////////////////////////////
    // URI: /serverinfo

    chromecast_device_request_process_serverinfo(httpsh) ;

    logmsg(LOG_INFO, "Received request: %s from %s:%d - returning media list", 
                     uri, 
                     hpeeripaddress(httpsh), hpeerport(httpsh)) ;



  } else if (strcasecmp(uri, "/devicelist")==0) {

    /////////////////////////////////////
    // URI: /devicelist

    chromecast_device_request_process_devicelist(httpsh, cclist, maxcc) ;

    logmsg(LOG_INFO, "Received request: %s from %s:%d - returning device list", 
                     uri, 
                     hpeeripaddress(httpsh), hpeerport(httpsh)) ;



  } else if (strcmp(uri, "/jsonquery")==0) {

    /////////////////////////////////////
    // URI: /jsonquery?device=devicename

    DATAOBJECT *sessionvars = NULL ;
    DATAOBJECT *body = donew() ;
    if (!body) goto jsonqueryfail ;

    // Transfer all uri query parameters into sessionvars

    sessionvars = donew() ;
    if (!sessionvars) goto jsonqueryfail ;

    char *variable ;
    int svi=1 ;
    while (variable=hgeturiparamname(httpsh, svi++)) {
      dosetdata(sessionvars, do_string, variable, strlen(variable), "/+/variable") ; 
      char *value = hgeturiparamstr(httpsh, variable) ;
      if (value) dosetdata(sessionvars, do_string, value, strlen(value), "/*/value") ; 
    }

    // parse json body into dataobject and extract device name if present

    if (!chromecast_device_expand_json(body, hgetbody(httpsh), sessionvars )) goto jsonqueryfail ;

    char *device=ccgetvariable(sessionvars, "device") ;

    char *friendlyname=NULL ;
    if (device) { 
      index = chromecast_finddevice(device, maxcc) ; 
      friendlyname = chromecast_mdns_at(index)->friendlyname ;
    }
    friendlyname = friendlyname ? friendlyname : "unknown" ;

    if (!device || index<0 || !cclist[index]) { 

      hsend( httpsh, 503, "application/json", 
               "{\"status\":\"NODEVICE\","
               "\"info\":\"chromecast device not found\"}" ) ;

      logmsg(LOG_NOTICE, "Received request: %s from %s:%d - device '%s' not found", 
                         uri, hpeeripaddress(httpsh), hpeerport(httpsh), friendlyname) ;
      index = -1 ;

    } else {

      logmsg(LOG_INFO, "Received request: %s for %s from %s:%d - json query", 
                         uri, friendlyname, hpeeripaddress(httpsh), hpeerport(httpsh)) ;

      if (!chromecast_device_request_process_jsonquery(httpsh, cclist[index])) {

        index = -1 ;

      }

    }

jsonqueryfail:
    if (sessionvars) dodelete(sessionvars) ;
    if (body) dodelete(body) ;




  } else if (scriptfilename(uri)) {

    /////////////////////////////////////
    // URI: /scriptname?device=devicename

    // Locate the script and get preprocess

    mem *jsonscript = NULL ;
    DATAOBJECT *script = NULL ;
    DATAOBJECT *sessionvars = NULL ;

    script = donew() ;
    if (!script) goto scriptfail ;

    char *jsonbody = hgetbody(httpsh) ;


    // Transfer all uri query parameters into sessionvars

    sessionvars = donew() ;
    if (!sessionvars) goto jsonqueryfail ;

    char *variable ;
    int svi=1 ;
    while (variable=hgeturiparamname(httpsh, svi++)) {
      char *value = hgeturiparamstr(httpsh, variable) ;
      ccsetvariable(sessionvars, variable, value) ;
    }


    jsonscript = loadscriptfromfile(uri) ;

    if (!jsonscript) {

      int responseCode=200 ;
      char *status = "FAILEDPREPROCESSING" ;
      char *info = "Script not found" ;

      // Send response

      hsend(httpsh, responseCode, "application/json",
            "{\n  \"status\":\"%s\",\n  \"info\":\"%s\"\n}",
            status, info) ;

      logmsg( LOG_ERR, "Processing request: %s from %s:%d - script not found - FAILED", 
                        uri, hpeeripaddress(httpsh), hpeerport(httpsh)) ;

      goto scriptfail ;

    }

    // Parse the jsonscript into the script object

    if (!dofromjsonu(script, jsonscript)) {

        // Errors expanding variables, so report

        int responseCode=200 ;
        char *status = "FAILEDPREPROCESSING" ;
        char *info = dojsonparsestrerror(script) ;

        // Convert double quotes to single in info so that the json response does not break

        for (int i=0; i<strlen(info); i++) { if (info[i]=='\"') info[i]='\'' ; }

        // Send response

        hsend(httpsh, responseCode, "application/json",
              "{\n  \"status\":\"%s\",\n  \"info\":\"%s\"\n}",
              status, info) ;

        logmsg( LOG_ERR, "Processing request: %s from %s:%d - script parse failed - %s", 
                          uri, hpeeripaddress(httpsh), hpeerport(httpsh), info) ;

        goto scriptfail ;

    }

    
    if (jsonbody) {

      // Preprocess body if requested

      char *preprocess = dogetdata(script, do_string, NULL, "/preprocess") ;
      mem *processedbody = chromecast_device_request_malloc_convert_body(jsonbody, preprocess) ;

      if (!processedbody) goto scriptfail ;

      // Parse the body, and update sessionvars

      if (!chromecast_device_update_sessionvars(sessionvars, processedbody)) goto scriptfail ;

      // Re-load the script using the processed body output

      mem_free(jsonscript) ;
      jsonscript = loadscriptfromfile(uri) ;

      mem_free(processedbody) ;

    }

    // Find the device

    char *device=ccgetvariable(sessionvars, "device") ;
    char *friendlyname=NULL ;

    if (device) { 
      index = chromecast_finddevice(device, maxcc) ; 
      friendlyname = chromecast_mdns_at(index)->friendlyname ;
    }
    friendlyname = friendlyname ? friendlyname : "unknown" ;

    if (!device || index<0 || !cclist[index]) { 

      hsend( httpsh, 503, "application/json", 
               "{\"status\":\"NODEVICE\","
               "\"info\":\"chromecast device not found\"}" ) ;

      logmsg(LOG_NOTICE, "Received request: %s from %s:%d - device '%s' not found", 
                         uri, hpeeripaddress(httpsh), hpeerport(httpsh), friendlyname) ;

      index = -1 ;

    } else {

      //  move sessionvars into httpsh

      if (cclist[index]->httpsessionvars) dodelete(cclist[index]->httpsessionvars) ;
      cclist[index]->httpsessionvars = sessionvars ;
      sessionvars = NULL ;

      // Log session vars

      char *svlog = doasjson(cclist[index]->httpsessionvars, NULL) ;
      logmsg(LOG_DEBUG, "Session vars pre macro load: %s", svlog) ;

      // Load the macro

      if (chromecast_macro_load(httpsh, cclist[index], jsonscript)) {

        logmsg(LOG_INFO, "Received request: %s for %s from %s:%d - json script", 
                         uri, friendlyname, hpeeripaddress(httpsh), hpeerport(httpsh)) ;

      } else {

        logmsg(LOG_ERR, "Processing request: %s for %s from %s:%d - json script loading error - FAILED", 
                         uri, friendlyname, hpeeripaddress(httpsh), hpeerport(httpsh)) ;
        index = -1 ;

      }

    }

scriptfail:

    if (sessionvars) dodelete(sessionvars) ;
    if (script) dodelete(script) ;
    if (jsonscript) mem_free(jsonscript) ;


  } else {
 
    /////////////////////////////////////
    // 404 - File not found

    if (html_filesystem("/404.html", &file, &mediatype, &len)) {
      hsendb(httpsh, 404, mediatype, file, len) ;
    } else {
      hsend(httpsh, 404, "text/plain", "404 - File not found") ;
    }
      
    logmsg(LOG_INFO, "Received request: %s from %s:%d - returning 404, file not found", 
                     uri, 
                     hpeeripaddress(httpsh), hpeerport(httpsh)) ;
    index = -1 ;

  }


  return index ;

}





/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// @brief Allocate output, and fill with preprocess

mem *chromecast_device_request_malloc_convert_body(char *json, char *preprocess) 
{
  if (!json) return NULL ;

  if (!preprocess) {

   // No preprocessing, return copy of input
 
   char *jsonout = mem_malloc(strlen(json)+1) ;
   if (jsonout) strcpy(jsonout, json) ;
   return jsonout ;

  } else {

   // Preprocessing, execute script and return output

   char *jsonout = mem_malloc(8192) ; *jsonout='\0' ;
   char jsonerr[8192] ; *jsonerr='\0' ;

   // TODO: Update execute to malloc as it goes, and pass pointers to jsonout/err
   
   if (execute(preprocess, json, jsonout, 8191, jsonerr, sizeof(jsonerr)-1)==0) {

     logmsg(LOG_DEBUG, "parsed json -> %s", jsonout) ;

   } else {

     logmsg(LOG_ERR, "Error preprocessing json (%s)", json?json:"null") ;
     mem_free(jsonout) ;
     jsonout=NULL ;

   }

   if (jsonerr[0]!='\0') {
     logmsg(LOG_NOTICE, "%s", jsonerr) ;
   }

   return jsonout ;

  }


}



/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// @brief Extract session vars from body
//

int chromecast_device_update_sessionvars(DATAOBJECT *sessionvars, char *json)
{
  // Check Parameters

  if (!json || !sessionvars) return 0 ;

  // Expand JSON

  DATAOBJECT *body = donew() ;
  if (!body) goto fail ;

  if (!dofromjson(body, json)) {

    // Report error and return 0

    logmsg( LOG_INFO, "Request body sessionvars json parse error - %s", 
            dojsonparsestrerror(body)) ;

    goto fail ;

  }

  // Walk through body and store all entries as session vars

  int i=0 ;
  DATAOBJECT *p ;

  while (p=donoden(body, i++)) {

    // Note: This only works with strings!

    char *variable = donodelabel(p) ;
    char *value = donodedata(p, NULL) ;

    if (variable && value) {
      ccsetvariable(sessionvars, variable, value) ;
    }

  }


  dodelete(body) ;
  return 1 ;

fail:
  if (body) dodelete(body);
  return 0 ;
}



/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// @brief Expand json into body, and extract device
//

int chromecast_device_expand_json(DATAOBJECT *body, char *json, DATAOBJECT *sessionvars)
{
  // Check Parameters

  if (!body || !json || !sessionvars) return 0 ;

  // Expand JSON

  if (!dofromjson(body, json)) {

    // Report error and return 0

    logmsg( LOG_INFO, "Request body json parse error - %s", 
            dojsonparsestrerror(body)) ;

    return 0 ;

  }

  // Search for /device and store as a variable in sessionvars

  char *value = dogetdata(body, do_string, NULL, "/device") ;
  ccsetvariable(sessionvars, "device", value) ;

  return 1 ;
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

    if (e->friendlyname && cclist[i]) {

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
               "    \"serverPort\": %d,\n"
               "    \"logo\": \"http://%s:%d/logo.png\",\n" 
               "    \"pp\": \"http://%s:%d/pp.png\",\n"
               "    \"img1\": \"http://%s:%d/test1.jpg\",\n"
               "    \"img2\": \"http://%s:%d/test2.jpg\",\n"
               "    \"test1\": \"http://%s:%d/test1.ogg\",\n"
               "    \"test2\": \"http://%s:%d/test2.ogg\",\n"
               "    \"alert1\": \"http://%s:%d/alert1.ogg\",\n" 
               "    \"alert2\": \"http://%s:%d/alert2.ogg\",\n"
               "    \"ok1\": \"http://%s:%d/ok1.ogg\",\n"
               "    \"ok2\": \"http://%s:%d/ok2.ogg\",\n"
               "    \"no1\": \"http://%s:%d/no1.ogg\",\n"
               "    \"no2\": \"http://%s:%d/no2.ogg\",\n"
               "    \"start1\": \"http://%s:%d/start1.ogg\",\n"
               "    \"start2\": \"http://%s:%d/start2.ogg\",\n"
               "    \"end1\": \"http://%s:%d/end1.ogg\",\n"
               "    \"end2\": \"http://%s:%d/end2.ogg\"\n"
               "  },\n"
               "  \"scripts\": [\n",
               ip, port,
               ip, port,
               ip, port,
               ip, port,
               ip, port,
               ip, port,
               ip, port,
               ip, port,
               ip, port,
               ip, port,
               ip, port,
               ip, port,
               ip, port,
               ip, port,
               ip, port,
               ip, port,
               ip, port) ;

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

mem *loadscriptfromfile(char *uri) 
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
  char *buf=mem_malloc(32) ;
  int buflen=sizeof(buf) ;

  while ( (ch=fgetc(fp)) > 0 ) {
    if (i>=(buflen-1)) {
      buflen+=32 ;
      buf=mem_realloc(buf, buflen) ;
      if (!buf) goto fail ;
    }
    buf[i++]=ch ;
  }

  // Terminate

  buf[i]='\0' ;

  // Expand to allow space for variable expansion

  buf = mem_realloc(buf, buflen+1024) ;
  
fail:
  fclose(fp) ;
  return buf ;
}


