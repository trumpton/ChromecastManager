//
// chromecastmanager_macro.c
//
// Handler function for macro request
//
//

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include "chromecastmanager.h"

#include "libdataobject/dataobject.h"
#include "libtools/log.h"
#include "libtools/mem.h"
#include "libtools/str.h"

// Expand variables in entry structure with info from cch->vars and
// httpsh params
int _expand_entry_variables(DATAOBJECT *entry, CHROMECAST *cch) ;



//
// @brief Load Macro
// @param(in) httpsh Handle of current http session
// @param(in) cch Chromecast device handle
// @return True if further processing required
//

int chromecast_macro_load(HTTPD *httpsh, CHROMECAST *cch, char *macro)
{
  mem *evmacro=NULL ;

  if (!cch || !macro) return 0 ;

  if (cch->macro) free(cch->macro) ;
  cch->macro=donew() ;

  if (cch->httpsessionvars) free(cch->httpsessionvars) ;
  cch->httpsessionvars = donew() ;

  if (!cch->macro || !cch->httpsessionvars) goto fail ;

  if (!macro) {

    if (httpsh) {

      int responseCode=200 ;
      char *status = "FAILED" ;
      char *info = "json body missing in request" ;

      // Send response

      hsend(httpsh, responseCode, "application/json",
            "{\n  \"status\":\"%s\",\n  \"info\":\"%s\"\n}",
            status, info) ;

    }

    goto fail ;

  }


  // Copy macro and expand unquoted variables only

  evmacro = mem_malloc(strlen(macro)+1024) ;
  if (!evmacro) return 0 ;

  strcpy(evmacro, macro) ;

  ccexpandvariables(cch->vars, evmacro, 1, 1) ;
  ccexpandvariables(cch->httpsessionvars, evmacro, 1, 0) ;

  // Then parse macro

  if (!dofromjson(cch->macro, evmacro) ) {

    // Errors expanding macro, so report

    logmsg(LOG_ERR, "Macro parsing error: %s", dojsonparsestrerror(cch->macro)) ;

    if (httpsh) {

      int responseCode=200 ;
      char *status = "FAILED" ;
      char *info = dojsonparsestrerror(cch->macro) ;

      // Convert double quotes to single in info so that the json response does
      // not break

      for (int i=0; i<strlen(info); i++) { if (info[i]=='\"') info[i]='\'' ; }

      // Send response

      hsend(httpsh, responseCode, "application/json",
            "{\n  \"status\":\"%s\",\n  \"info\":\"%s\"\n}",
            status, info) ;

    }

    goto fail ;

  } else if (!dofindnode(cch->macro, "/script")) {

    // Script not found

    if (httpsh) {

      int responseCode=200 ;
      char *status = "FAILED" ;
      char *info = "Missing {'script': ... from script" ;

      // Send response

      hsend(httpsh, responseCode, "application/json",
            "{\n  \"status\":\"%s\",\n  \"info\":\"%s\"\n}",
            status, info) ;

    }

    goto fail ;

  }

  else {

    // Macro OK

    if (httpsh) {

      // Create httpsessionvars variables from http body

      char *body = hgetbody(httpsh) ;

      if (body) {

        DATAOBJECT *bh = donew() ;
        if (!bh) goto fail ;
        
        if (!dofromjson(bh, body)) {

          // Errors expanding variables, so report

          int responseCode=200 ;
          char *status = "FAILED" ;
          char *info = dojsonparsestrerror(cch->macro) ;

          // Convert double quotes to single in info so that the json response does
          // not break

          for (int i=0; i<strlen(info); i++) { if (info[i]=='\"') info[i]='\'' ; }

          // Send response

          hsend(httpsh, responseCode, "application/json",
                "{\n  \"status\":\"%s\",\n  \"info\":\"%s\"\n}",
                status, info) ;

          dodelete(bh) ;
          goto fail ;
 
        } else {

          DATAOBJECT *v ;
          char *variable ;
          char *value ;
          int i=0 ;

          while (v=donoden(bh, i++)) {

            variable=donodelabel(v) ;
            value=donodedata(v, NULL) ;

            if (variable && value && *variable!='\0') {
              dosetdata(cch->httpsessionvars, do_string, variable, strlen(variable), "/%s/variable", variable) ; 
              dosetdata(cch->httpsessionvars, do_string, value, strlen(value), "/%s/value", variable) ; 
            }

          }

          dodelete(bh) ;

        }


      }

      // Create httpsessionvars variables from http query parameters

      char *variable ;
      int index=1 ;
      while (variable=hgeturiparamname(httpsh, index++)) {

        dosetdata(cch->httpsessionvars, do_string, variable, strlen(variable), "/%s/variable", variable) ; 

        char *value = hgeturiparamstr(httpsh, variable) ;
        if (value) dosetdata(cch->httpsessionvars, do_string, value, strlen(value), "/%s/value", variable) ; 

      }

    }

    // First macro step is 1 (at index 0)
    cch->macroindex=1 ;
    cch->macrotimer = (time_t)0 ;
    cch->macroforce = 0 ;

    mem_free(evmacro) ;

    return chromecast_macro_process(httpsh, cch) ;

  }

fail:
  cch->macroindex=0 ;
  if (cch->macro) dodelete(cch->macro) ;
  cch->macro = NULL ;
  if (cch->httpsessionvars) dodelete(cch->httpsessionvars) ;
  if (evmacro) mem_free(evmacro) ;
  cch->httpsessionvars = NULL ;
  return 0 ;

}


/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// @brief Process Macro
// @param(in) httpsh handle of HTTP session
// @param(in) cch Handle of chromecast device
// @return true whilst processing and false on completion / error
//

int chromecast_macro_process(HTTPD *httpsh, CHROMECAST *cch)
{
 
  int loopcount=20 ; // Detection of infinate loops

  if (!cch) return 0 ;
  if (!cch->macro) return 1 ;

  DATAOBJECT *thisstep ;
  int num = cch->macroindex-1 ;
  int last = -1 ;
  int responsemessagesent=0 ;

  // Cancel any force (it can be reasserted if required) 
  cch->macroforce=0 ;

  while ( (thisstep=dochild(dofindnode(cch->macro, "/script/%d", num))) && 
          num>=0 && num!=last && (loopcount--)>0) {

    DATAOBJECT *step = donewfrom(thisstep) ;

    last=num ;

    // Process Entry

    _expand_entry_variables(step, cch) ;

    char *op=dogetdata(step, do_string, NULL, "/op") ;

    if (op && strcmp(op, "addwatch")==0) {

      // Add / update a watch

      char *variable = dogetdata(step, do_string, NULL, "/variable") ;
      char *namespace = dogetdata(step, do_string, NULL, "/namespace") ;
      char *type = dogetdata(step, do_string, NULL, "/type") ;
      char *path = dogetdata(step, do_string, NULL, "/path") ;
 
      if (variable && namespace && type && path) {
        dosetdata(cch->vars, do_string, variable, strlen(variable), "/%s/variable", variable) ;
        dosetdata(cch->vars, do_string, namespace, strlen(namespace), "/%s/namespace", variable) ;
        dosetdata(cch->vars, do_string, type, strlen(type), "/%s/type", variable) ;
        dosetdata(cch->vars, do_string, path, strlen(path), "/%s/path", variable) ;
        dosetdata(cch->vars, do_string, "", 0, "/%s/value", variable) ;
      }

      char *comment=dogetdata(step, do_string, NULL, "/comment") ;
      logmsg( LOG_INFO, "Macro @%d%s%s%s add '%s' watch", 
              num+1, comment?" (":"", comment?comment:"", comment?")":"",
              variable?variable:"(invalid)") ;
      num++ ;

    } else if (op && strcmp(op, "setwatch")==0) {
 
      // Set a watch variable to a specific value

      char *variable = dogetdata(step, do_string, NULL, "/variable") ;
      char *value = dogetdata(step, do_string, NULL, "/value") ;
 
      if (variable) {

        int len = value?strlen(value):0 ;
        dosetdata(cch->vars, do_string, value?value:"", len, "/%s/value", variable) ;

        char *comment=dogetdata(step, do_string, NULL, "/comment") ;
        logmsg( LOG_INFO, "Macro @%d%s%s%s set '%s' to '%s'", 
                num+1, comment?" (":"", comment?comment:"", comment?")":"",
                variable, value?value:"") ;

      } else {

        char *comment=dogetdata(step, do_string, NULL, "/comment") ;
        logmsg( LOG_INFO, "Macro @%d%s%s%s set '%s' invalid (not watched)", 
                num+1, comment?" (":"", comment?comment:"", comment?")":"",
                variable) ;
      }


      num++ ;

    } else if (op && strcmp(op, "pause")==0) {

      // Pause for specified interval

      unsigned long int tih = 0, tim = 0, tis = 0 ;
 
      char *tss = dogetdata(step, do_string, NULL, "/seconds") ;
      char *tsm = dogetdata(step, do_string, NULL, "/minutes") ;
      char *tsh = dogetdata(step, do_string, NULL, "/hours") ;
      char *comment=dogetdata(step, do_string, NULL, "/comment") ;

      if (tss) { tis = atoi(tss) ; } 
      else { dogetuint(step, do_int32, &tis, "/seconds") ; }

      if (tsm) { tim = atoi(tsm) ; } 
      else { dogetuint(step, do_int32, &tim, "/minutes") ; }

      if (tsh) { tih = atoi(tsh) ; } 
      else { dogetuint(step, do_int32, &tih, "/hours") ; }

      tis = tis + tim*60 + tih*3600 ;

      if (tis<=0) {

        logmsg( LOG_INFO, "Macro @%d%s%s%s skipping empty pause", 
                num+1, comment?" (":"", comment?comment:"", comment?")":"") ;
        num++ ;

      } else {

        if (cch->macrotimer == (time_t)0 ) {

          logmsg( LOG_INFO, "Macro @%d%s%s%s starting pause for %ld seconds", 
                  num+1, comment?" (":"", comment?comment:"", comment?")":"", tis) ;
          cch->macrotimer = time(NULL) ;

        } else if ( time(NULL) >= (cch->macrotimer + tis) ) {

          logmsg( LOG_INFO, "Macro @%d%s%s%s pause complete", 
                  num+1, comment?" (":"", comment?comment:"", comment?")":"") ;
          num++ ;
          cch->macrotimer=(time_t)0 ;

        }

      }

      // Force script to re-trigger even if no
      // data traffic event arrives.

      cch->macroforce=1 ;


    } else if (op && strcmp(op, "send")==0) {
 
      // Send message to the chromecast device

      char *namespace=dogetdata(step, do_string, NULL, "/namespace") ;
      char *sender=dogetdata(step, do_string, NULL, "/sender") ;
      char *receiver=dogetdata(step, do_string, NULL, "/receiver") ;
      char *message=doasjson(dochild(dofindnode(step,"/message")), NULL) ;
      char *comment=dogetdata(step, do_string, NULL, "/comment") ;

      logmsg( LOG_INFO, "Macro @%d%s%s%s send message", 
              num+1, comment?" (":"", comment?comment:"", comment?")":"") ;

      ccsendmessage(cch, sender, receiver, namespace, message) ;
      num++ ;

    } else if (op && strcmp(op, "test")==0) {

      // Check variables, and change state if indicated
      int found=0 ;
      char *gotolabel=NULL ;
      char *type=NULL ;
      int condindex=0 ;

      DATAOBJECT *conditions=dochild(dofindnode(step, "/conditions")) ;
      DATAOBJECT *condition;

      while (!type && (condition=dochild(donoden(conditions,condindex))) ) {

        char *a = dogetdata(condition, do_string, NULL, "/a") ; // String1
        char *b = dogetdata(condition, do_string, NULL, "/b") ; // String2
        char *v = dogetdata(condition, do_string, NULL, "/v") ; // Valid and contains data      

        char *g = dogetdata(condition, do_string, NULL, "/goto") ;
        char *e = dogetdata(condition, do_string, NULL, "/else") ;

        // If there is a match, or a does not contain an
        // unexpanded variable (i.e. it exists) then ...

        int match=0 ;

        if (a && b && strcmp(a,b)==0) {

          // A and B Strings match
          match=1 ;

        } else if (v && *v!='\0') {

          // V contains valid data
          match=1 ;

        }

        if (!a && !b && !v && e) {

          // Just an else, so do it
          match=0 ;

        }

        if (!a && !b && !v && g) {

          // Just a goto, so do it
          match=1 ;
        }
     
        if (!match && e) {          
          type="else" ;
          gotolabel=e ;
        }

        if (match && g) {
          type="goto" ;
          gotolabel=g ;
        }

        if (match && !g && !e) {
          type="goto" ;
          gotolabel="next" ;
        }

        logmsg( LOG_INT, "Macro Test: index=%d, a=%s, b=%s, v=%s, e=%s, g=%s, action=%s%s%s",
                condindex+1, a?a:"", b?b:"", v?v:"", e?e:"", g?g:"", type?type:"",
                type?" ":"", gotolabel?gotolabel:"") ;

        if (!type) condindex++ ;

      }

      if (type) {

        char *comment=dogetdata(step, do_string, NULL, "/comment") ;
        logmsg( LOG_INFO, "Macro @%d%s%s%s test condition %d triggered - %s %s", 
                num+1, comment?" (":"", comment?comment:"", comment?")":"",
                condindex+1, type, gotolabel?gotolabel:"next") ;

        if (strcmp(gotolabel, "next")==0) {

          // Go to next entry
          num++ ;
          found=1 ;

        } else {

          // Search for 'goto' entry

          int j=0 ;
          int found=0 ;
          DATAOBJECT *search ;
          DATAOBJECT *scriptlist = dochild(dofindnode(cch->macro, "/script")) ;
          while (!found && (search=dochild(donoden(scriptlist, j))) ) {
            char *label = dogetdata(search, do_string, NULL, "/label") ;
            if (label && strcmp(label, gotolabel)==0) {
              found=1 ;
              num=j ;
            }
            j++ ;
          }

          if (!found) {
            logmsg( LOG_ERR, "Macro @%d - test condition %d - invalid GOTO label, aborting", 
                    last+1, condindex+1) ;
            num=-1 ;
          }

        }

      }

    } else if (op && strcmp(op, "respond")==0) {

      DATAOBJECT *respond = dochild(dofindnode(step, "/response"));

      if (respond) {

        // Sequence "respond" Identified, send response

        if (httpsh && !responsemessagesent) {
  
          char *comment=dogetdata(step, do_string, NULL, "/comment") ;

          logmsg( LOG_INFO, "Macro @%d%s%s%s sending http response", 
                  last+1, comment?" (":"", comment?comment:"", comment?")":"") ;

          unsigned long int responseCode = 200 ;
          dogetuint(respond, do_uint32, &responseCode, "/responseCode") ;
          char *json = doasjson(respond, NULL) ;

          hsend( httpsh, responseCode, "application/json", "%s", json?json:"{\"status\":\"INTERR\"}" ) ;

          responsemessagesent=1 ;

       } else {

          char *comment=dogetdata(step, do_string, NULL, "/comment") ;

          logmsg( LOG_INFO, "Macro @%d%s%s%s not sending http response%s%s", 
                  last+1, comment?" (":"", comment?comment:"", comment?")":"",
                  httpsh?"":" - http session not active",
                  responsemessagesent?" - response already sent":"") ;

       }

       num++ ;

     } else {

        logmsg( LOG_ERR, "Macro @%d - missing response, aborting", last+1) ;
        num=-1 ;

     }

    } else if (!op) {

      // No Opcode

      char *comment=dogetdata(step, do_string, NULL, "/comment") ;

      logmsg( LOG_INFO, "Macro @%d%s%s%s no opcode - skipping to next", 
              last+1, comment?" (":"", comment?comment:"", comment?")":"") ;
       num++ ;

    } else {

      // Invalid op 

      char *comment=dogetdata(step, do_string, NULL, "/comment") ;

      logmsg( LOG_ERR, "Macro @%d%s%s%s - bad or missing 'op', aborting", 
              last+1, comment?" (":"", comment?comment:"", comment?")":"") ;
      num=-1 ;
      op=NULL ;

    }


    if (loopcount==1) {

      // Probably stuck in a loop, so bail  
 
      logmsg( LOG_ERR, "Macro @%d - probable infinate loop, aborting", last+1) ;
      num=-1 ;

    }

    unsigned long int endflag=0 ;
    dogetuint(step, do_bool, &endflag, "/end") ;
    if (endflag) {
      num=-1 ;
    }

    dodelete(step) ;
   
  }

  cch->macroindex=num+1 ;

  if (num<1 || !thisstep) {
    if (last>=0) logmsg( LOG_INFO, "Macro @%d end", last+1) ;
    if (cch->macro) dodelete(cch->macro) ;
    cch->macro = NULL ;
    cch->macroindex = -1 ;
    cch->macrotimer = (time_t)0 ;
    if (cch->httpsessionvars) dodelete(cch->httpsessionvars) ;
    cch->httpsessionvars = NULL ;
  }

  return (num>=0) ;

}


//////////////////////////////////////////////////////////////////////
//
// Expand variables in script entry structure with info from cch->vars 
// and httpsh params - clear any remaining $blah variables
//

int _expand_entry_variables(DATAOBJECT *entry, CHROMECAST *cch)
{

  int len ;
  mem *buf ;

  if (!entry || !cch) return 0 ;

  // Convert entry data to a string

  char *entryasjson = doasjson(entry, &len) ;
  buf=mem_malloc(len+1024) ;

  str_strcpy(buf, entryasjson) ;

  // Expand variables from watches and httpd queries

  ccexpandvariables(cch->httpsessionvars, buf, 0, 1) ;
  ccexpandvariables(cch->vars, buf, 0, 0) ;

  // Convert expanded string back to json data

  doclear(entry) ;
  dofromjson(entry, buf) ;
  mem_free(buf) ;

  return 1 ;

}


