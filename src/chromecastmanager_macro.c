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


//
// @brief Load Macro
// @param(in) httpsh Handle of current http session
// @param(in) cch Chromecast device handle
// @return True if further processing required
//

int chromecast_macro_load(HTTPD *httpsh, CHROMECAST *cch, char *macro)
{

  if (!cch || !macro) return 0 ;

  if (cch->macro) free(cch->macro) ;
  cch->macro=donew() ;

  /////////////////////////////////////////////////////
  // Parse macro (without parsing unquoted as they may contain variables)

  if (!dofromjsonu(cch->macro, macro) ) {

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

  } else if (!dofindnode(cch->macro, "/steps")) {

    // Script not found

    if (httpsh) {

      int responseCode=200 ;
      char *status = "FAILED" ;
      char *info = "Missing 'steps': [] from script" ;

      // Send response

      hsend(httpsh, responseCode, "application/json",
            "{\n  \"status\":\"%s\",\n  \"info\":\"%s\"\n}",
            status, info) ;

    }

    goto fail ;

  }

  /////////////////////////////////////////////////////
  // First macro step is 1 (index=0) - start processing

  cch->macroindex=1 ;
  cch->macrotimer = (time_t)0 ;
  cch->macroforce = 0 ;

  return chromecast_macro_process(httpsh, cch) ;

  /////////////////////////////////////////////////////
  // Failure - tidy up and return

fail:
  cch->macroindex=0 ;
  if (cch->macro) dodelete(cch->macro) ;
  cch->macro = NULL ;
  if (cch->httpsessionvars) dodelete(cch->httpsessionvars) ;

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
 
  int loopcount=512 ; // Detection of infinate loops

  if (!cch) return 0 ;
  if (!cch->macro) return 1 ;

  DATAOBJECT *thisstep ;
  int num = cch->macroindex-1 ;
  int last = -1 ;
  int responsemessagesent=0 ;

  // Cancel any force (it can be reasserted if required) 
  cch->macroforce=0 ;

  while ( (thisstep=dochild(dofindnode(cch->macro, "/steps/%d", num))) && 
          num>=0 && num!=last && (loopcount--)>0) {

    DATAOBJECT *step = donewfrom(thisstep) ;

    last=num ;

    // Expand entry variables (2 passes to allow for nested variables)

    ccexpandvariables(step, cch->vars, 1, 0) ;
    ccexpandvariables(step, cch->httpsessionvars, 1, 1) ;
    ccexpandvariables(step, cch->vars, 1, 0) ;
    ccexpandvariables(step, cch->httpsessionvars, 0, 1) ;

    // Process Entry

    char *op=dogetdata(step, do_string, NULL, "/op") ;

    if (op && strcmp(op, "addwatch")==0) {

      /////////////////////////////////////////////////////////
      /////////////////////////////////////////////////////////
      //
      // Add / update a watch
      //

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
 
      /////////////////////////////////////////////////////////
      /////////////////////////////////////////////////////////
      //
      // Set a watch variable to a specific value
      //

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



    } else if (op && strcmp(op, "send")==0) {
 
      /////////////////////////////////////////////////////////
      /////////////////////////////////////////////////////////
      //
      // Send message to the chromecast device
      //

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

      /////////////////////////////////////////////////////////
      /////////////////////////////////////////////////////////
      //
      // Check variables, and change state if indicated
      //

      int found=0 ;
      char *gotolabel=NULL ;
      char *type=NULL ;
      int condindex=0 ;

      DATAOBJECT *conditions=dochild(dofindnode(step, "/conditions")) ;
      DATAOBJECT *condition;

      while (!type && (condition=dochild(donoden(conditions,condindex))) ) {

        long int tih = -1, tim = -1, tis = -1 ;
        
        dogetuint(condition, do_uint32, &tih, "/hours") ;             // Hours to sleep      
        dogetuint(condition, do_uint32, &tih, "/h") ;

        dogetuint(condition, do_uint32, &tim, "/minutes") ;           // Minutes to sleep      
        dogetuint(condition, do_uint32, &tim, "/m") ;

        dogetuint(condition, do_uint32, &tis, "/seconds") ;           // Seconds to sleep      
        dogetuint(condition, do_uint32, &tis, "/s") ;

        char *a = dogetdata(condition, do_string, NULL, "/a") ;       // String1
        int ae = dogetnode(condition, "/a") != NULL ;

        char *b = dogetdata(condition, do_string, NULL, "/b") ;       // String2
        int be = dogetnode(condition, "/b") != NULL ;

        char *v = dogetdata(condition, do_string, NULL, "/valid") ;   // Valid and contains data   
        if (!v) v = dogetdata(condition, do_string, NULL, "/v") ; 
        int ve = dogetnode(condition, "/v") != NULL || dogetnode(condition, "/valid") != NULL ;
   
        char *g = dogetdata(condition, do_string, NULL, "/goto") ;
        char *e = dogetdata(condition, do_string, NULL, "/else") ;

        int match=0 ;

        if ( tih>=0 || tim>=0 || tis>=0 ) {

          /////////////////////////////////
          // Pause for specified interval

          if (tih<0) tih=0 ;
          if (tim<0) tim=0 ;
          if (tis<0) tis=0 ;

          tis = tis + tim*60 + tih*3600 ;

          if (tis>0 && cch->macrotimer == (time_t)0 ) {

            // First call, so report start and save current time in macrotimer

            char *comment=dogetdata(step, do_string, NULL, "/comment") ;
            logmsg( LOG_INFO, "Macro @%d%s%s%s starting pause for %ld seconds", 
                    last+1, comment?" (":"", comment?comment:"", comment?")":"", tis) ;
            cch->macrotimer = time(NULL) ;

          } else if ( tis==0 || time(NULL) >= (cch->macrotimer + tis) ) {

            // Timeout finished or invalid (zero) value, so report and mark complete

            char *comment=dogetdata(step, do_string, NULL, "/comment") ;
            logmsg( LOG_INFO, "Macro @%d%s%s%s pause complete", 
                    last+1, comment?" (":"", comment?comment:"", comment?")":"") ;

            match=1 ;
            cch->macrotimer=(time_t)0 ;

          }

          // Force script to re-trigger immediately even
          // if no data traffic event arrives.
          cch->macroforce=1 ;
        
        } else if (ae && be && a && b && strcmp(a,b)==0) {

          /////////////////////////////////
          // A and B Strings match

          match=1 ;

        } else if (ve && v && *v!='\0') {

          /////////////////////////////////
          // V contains valid data

          match=1 ;

        } else if (!ae && !be && !ve && tis<0 && e) {

          /////////////////////////////////
          // Just an else, so do it

          match=0 ;

        } else if (!ae && !be && !ve && tis<0 && g) {

          /////////////////////////////////
          // Just a goto, so do it

          match=1 ;

        } else if (!g && !e) {


          char *comment=dogetdata(step, do_string, NULL, "/comment") ;
          logmsg( LOG_INFO, "Macro @%d%s%s%s test condition %d invalid - goto next", 
                  num+1, comment?" (":"", comment?comment:"", comment?")":"", condindex+1) ;

          gotolabel="next" ;
          type="goto" ;

        }
     
        /////////////////////////////////
        // check test results and work
        // out how to dispatch / goto

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

        char *comment=dogetdata(step, do_string, NULL, "/comment") ;
        logmsg( LOG_INT, "Macro Test: index=%d, a=%s, b=%s, v=%s, e=%s, g=%s, action=%s%s%s",
                condindex+1, a?a:"", b?b:"", v?v:"", e?e:"", g?g:"", type?type:"",
                type?" ":"", gotolabel?gotolabel:"") ;


        // Move to next condition if no match found
        if (!type) condindex++ ;

    }

    ////////////////////////////////////////////////
    // Search of all conditions complete - dispatch

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
          DATAOBJECT *scriptlist = dochild(dofindnode(cch->macro, "/steps")) ;
          while (!found && (search=dochild(donoden(scriptlist, j))) ) {
            char *label = dogetdata(search, do_string, NULL, "/label") ;
            if (label && strcmp(label, gotolabel)==0) {
              found=1 ;
              num=j ;
            }
            j++ ;
          }

          if (!found) {
            char *comment=dogetdata(step, do_string, NULL, "/comment") ;
            logmsg( LOG_ERR, "Macro @%d - test condition %d - invalid GOTO label, aborting", 
                    last+1, condindex+1) ;
            num=-1 ;
          }

        }

      }



    } else if (op && strcmp(op, "respond")==0) {

      /////////////////////////////////////////////////////////
      /////////////////////////////////////////////////////////
      //
      // Sequence "respond" Identified, send response
      //

      DATAOBJECT *respond = dochild(dofindnode(step, "/response"));

      if (respond) {

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

        char *comment=dogetdata(step, do_string, NULL, "/comment") ;
        logmsg( LOG_ERR, "Macro @%d - missing response, aborting", last+1) ;
        num=-1 ;

     }



    } else if (!op) {

      /////////////////////////////////////////////////////////
      /////////////////////////////////////////////////////////
      //
      // No Opcode
      //

      char *comment=dogetdata(step, do_string, NULL, "/comment") ;
      logmsg( LOG_INFO, "Macro @%d%s%s%s no opcode - skipping to next", 
              last+1, comment?" (":"", comment?comment:"", comment?")":"") ;
       num++ ;



    } else {

      /////////////////////////////////////////////////////////
      /////////////////////////////////////////////////////////
      //
      // Invalid op 
      //

      char *comment=dogetdata(step, do_string, NULL, "/comment") ;
      logmsg( LOG_ERR, "Macro @%d%s%s%s - bad or missing 'op', aborting", 
              last+1, comment?" (":"", comment?comment:"", comment?")":"") ;
      num=-1 ;
      op=NULL ;

    }


    if (loopcount==1) {

      ////////////////////////////////////////////////
      // Probably stuck in a loop, so bail  
 
      char *comment=dogetdata(step, do_string, NULL, "/comment") ;
      logmsg( LOG_ERR, "Macro @%d - probable infinate loop, aborting", last+1) ;
      num=-1 ;

    }

    if (!op || strcmp(op, "test")!=0 ) {


      ////////////////////////////////////////////////
      // Check for "end": true

      unsigned long int endflag=0 ;
      dogetuint(step, do_bool, &endflag, "/end") ;
      if (endflag) {
        num=-1 ;
      }

    }

    dodelete(step) ;
   
  }


  ////////////////////////////////////////////////
  ////////////////////////////////////////////////
  //
  // Update macro index tidy up and return
  //

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

