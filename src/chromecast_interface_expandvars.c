//
//
//
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "libtools/mem.h"
#include "libtools/str.h"
#include "libtools/httpd.h"
#include "libdataobject/dataobject.h"
#include "chromecast_interface.h"

// getscriptfolder from chromecastmanager.c
char *getscriptfolder() ;

// Functions to extract data from a file
int _ccexpandvars_updatecache(DATAOBJECT *var, char *suffix) ;
int _ccexpandvars_readfile(DATAOBJECT *var, char *path, char *filename, int line) ;


//////////////////////////////////////////////////////////////////////////
//
// @brief Expand variables in format @(file) @(file:n) @(file:+) $(var) $(s:var) $(i:var) $(f:var) $(b:var)
// @param(in) dh Pointer to data object which contains variables to expand
// @param(in) vars Pointer to a variables structure
// @param(in) leaveifempty If true, empty / invalid variables are not expanded
// @return True on success
//

int ccexpandvariables(DATAOBJECT *dh, DATAOBJECT *vars, int leaveifempty)
{
  if (!dh || !vars) return 0 ;

  DATAOBJECT *thisdh = dh ;

  while (thisdh) {

    // Expand variables in data

    enum dataobject_type type = dogettype(thisdh,"/") ;

    if (type == do_unquoted || type == do_string || type == do_data) {

      char *dat = dogetdata(thisdh, do_string, NULL, "/") ;
      if (!dat) goto fail ;

      mem *buf = mem_malloc(strlen(dat) + 1024) ;
      if (!buf) goto fail ;

      strcpy(buf, dat) ;

      if (ccexpandstrvariables(buf, vars, leaveifempty)) {
        dosetdata(thisdh, type, buf, strlen(buf), "/") ;
      }
      mem_free(buf) ;

    }

    // Expand variables in label

    char *label = donodelabel(thisdh) ;
    if (strstr(label,"(")) {
      mem *buf = mem_malloc(strlen(label) + 1024) ;
      if (ccexpandstrvariables(buf, vars, leaveifempty)) {
        dorenamenode(thisdh, "/", buf) ;
      }
      mem_free(buf) ;
    }

    // Recurse if possible

    if (dochild(thisdh)) ccexpandvariables(dochild(thisdh), vars, leaveifempty) ;

    // Move to the next object

    thisdh = donext(thisdh) ;
  }

  return 1 ;

fail:

  return 0 ;
}


int ccexpandstrvariables(mem *buf, DATAOBJECT *vars, int leaveifempty) 
{

  if (!vars || !buf) return 0 ;

  int updated=0 ;
  DATAOBJECT *vh ;
  int p=0 ;
  int inquote=0 ;

  // quick check / return
  if (!strstr(buf, "$(") && !strstr(buf, "@(")) return 0 ;

  //////////////////////////////
  // Replace IP Address and Port

  char intbuf[64] ;
  snprintf(intbuf, sizeof(intbuf)-1, "%d", httpd_port()) ;

  str_replaceall(buf, "$(serverIpAddress)", httpd_ipaddress()) ;
  str_replaceall(buf, "$(serverPort)", intbuf) ;

  //////////////////////////////
  // Work through the string

  while (buf[p]!='\0') {

    int varlen=0 ;
    char type='s' ;
    #define MAXVARLABELLEN 128
    char vdata[MAXVARLABELLEN] ;
    char *variable = vdata ;
    char *suffix=NULL ;

    if (buf[p]=='\\') {
      p++ ;
    } else if (buf[p]=='\"') {
      inquote = !inquote ;
    }

    if ( (buf[p]=='$' || buf[p]=='@') && ( buf[p+1]=='(' ) ) {

      char *tstvariable = &buf[p] ;

      // Capture type

      if (buf[p]=='$' && buf[p+1]=='(') {
        type = 's' ;
      }

      if (buf[p]=='@' && buf[p+1]=='(') {
        type = '@' ;
      }

      if (buf[p+2]!='\0' && buf[p+3]==':') {
        type=buf[p+2] ;
        varlen=4 ;
      } else {
        varlen=2 ;
      }      

      //////////////////////////////
      // Copy variable

      int s=0 ;

      while (buf[p+varlen] != '\0' && buf[p+varlen] != ')' && buf[p+varlen] != ':' && s < MAXVARLABELLEN-1) {
        variable[s] = buf[p+varlen] ;
        varlen++ ;
        s++ ;
      }

      if (buf[p+varlen]==')') {

        // Terminate variable

        variable[s]='\0' ;
        varlen++ ;

      } else if (buf[p+varlen]==':') {

        // Store suffix

        variable[s]='\0' ;
        varlen++ ;
        suffix = &buf[p+varlen] ;

        // Check / skip suffix

        while (buf[p+varlen]=='+' || (buf[p+varlen]>='0' && buf[p+varlen]<='9') ) varlen++ ;

        // Check for correct termination

        if (buf[p+varlen]!=')') {
          variable[0]='\0' ;
          varlen=0 ;
        }

      } else {

        // Invalid termination

        variable[0]='\0' ;
        varlen=0 ;

      }

    }

    //////////////////////////////
    // Search for Variable

    if (varlen>0) {

      char *vhvalue=NULL ;
      DATAOBJECT *vh ;

      int i=0 ;
      while (!vhvalue && (vh=dochild(donoden(vars,i))) ) {

        // Get variable data

        char *vhvariable = dogetdata(vh, do_string, NULL, "/variable") ;

        if (vhvariable && strcmp(vhvariable, variable)==0) {

            if (type=='@') {

                _ccexpandvars_updatecache(vh, suffix) ;
                type='s' ;

            }

            vhvalue = dogetdata(vh, do_string, NULL, "/value") ;

        }   

        i++ ;   

      }

      //////////////////////////////
      // Add new @ type if not found

      if (type=='@') {

        dosetdata(vars, do_string, variable, strlen(variable), "/%s/variable", variable) ;
        vh = dofindnode(vars, "/%s", variable) ;
        _ccexpandvars_updatecache(vh, suffix) ;
        vhvalue = dogetdata(vh, do_string, NULL, "/valuecache") ;

      }

      //////////////////////////////
      // Do substitution

      if (!vhvalue && !leaveifempty) vhvalue="" ;

      if (vhvalue && *vhvalue!='\0') {

        int i=0 ;
        float f=0.0 ;

        mem *rvalue = mem_malloc(strlen(vhvalue)+1024) ;
        if (!rvalue) return 0 ;

        switch (type) {

          case 'i':

            sscanf(vhvalue, "%i", &i) ;
            sprintf(rvalue, "%i", i) ;
            break ;

          case 'f':

            sscanf(vhvalue, "%f", &f) ;
            sprintf(rvalue, "%.2f", f) ;
            break ;

          case 'b':

            if (vhvalue[0]=='f' || vhvalue[0]=='0' || vhvalue[0]=='F' || vhvalue[0]=='\0') {
              strcpy(rvalue, "false") ;
            } else {
              strcpy(rvalue, "true") ;
            }
            break ;

          default:
          case 's':
          case '@':
            strcpy(rvalue, vhvalue) ;
            str_replaceall(rvalue, "\"", "\\\"") ;
            break ;

        }

        str_insert(buf, p, varlen, rvalue) ;
        mem_free(rvalue) ;
        updated=1 ;

      } 

    }

    p++ ;

  }

  return updated ;

}


//////////////////////////////////////////////////////////////////////////
//
// @brief Increments the value of a variable which is stored as a string
// @param(in) vars Pointer to a variables structure
// @param(in) path Identifies the variable within vars
// @returns true on success

int ccincrementvar(DATAOBJECT *vars, char *path)
{

  int value=0 ;

  char *values = dogetdata(vars, do_string, NULL, path) ;

  if (values) {

    char newvalue[32] ;
    value=atoi(values) ;
    sprintf(newvalue, "%d", value+1) ;
    dosetdata(vars, do_string, newvalue, strlen(newvalue), path) ;
    return 1 ;

  } else {

    return 0 ;

  }

}

//////////////////////////////////////////////////////////////////////////
//
// @brief Load value from a file and store in var
//

int _ccexpandvars_updatecache(DATAOBJECT *var, char *suffix)
{
  long int line = 1 ;

  ////////////////////////////////////////////
  // Get current line and cached data value

  if (suffix && *suffix>='0' && *suffix<='9') {

    line = atoi(suffix) ;

  } else {

    dogetsint(var, do_sint32, &line, "/line") ;

  }

  if (line<0) return 0 ;

  ////////////////////////////////////////////
  // Get cached value and update as needed

  if (! _ccexpandvars_readfile( var, "/value",
                                dogetdata(var, do_string, NULL, "/variable"),
                                (int)line) ) {
     // Extraction failed

     line=-1 ;

  }    

  ////////////////////////////////////////////
  // Update saved line number

  if (line>=0 && (suffix && *suffix=='+') ) { 
    line++ ;
  }
 
  if (line<0 || (suffix && *suffix!='\0') ) {
    dosetsint(var, do_sint32, line, "/line") ;
  }

  return (line>=0) ;

}


//////////////////////////////////////////////////////////////////////////
//
// @brief Search file for line (skipping blank ones), and store value in path
// @return true if a line was read

int _ccexpandvars_readfile(DATAOBJECT *var, char *path, char *filename, int line)
{
  if (!var || !path || !filename || line<0) return 0 ;

  // Files with paths which include  .. or are referenced from / are verboten
  if (strstr(filename, "..") || *filename=='/') return 0 ;

  FILE *fp ;
  char buf[1024] ;
  int linenum=1 ;
  int lastwasnewline=0 ;
  int ch=0 ;
  int op=0 ;

  snprintf(buf, sizeof(buf)-1, "%s/%s", getscriptfolder(), filename) ;
  fp=fopen(buf, "r") ;
  if (!fp) return 0 ;

  // Skip to the requested line

  if (linenum<line) do {
    ch = fgetc(fp) ;
    switch (ch) {
    case EOF:
      linenum=-1 ;
      break ;
    case '\r':
      break ;
    case '\n':
      if (!lastwasnewline) linenum++ ;
      lastwasnewline=1 ;
      break ;
    default:
      lastwasnewline=0 ;
    }
  } while (linenum>=0 && linenum!=line) ;

  // Copy the data across to buf

  op=0 ;

  if (linenum>=0) {

    while ( (ch=fgetc(fp)) != EOF && ch!='\n' ) {
      if (ch!='\r' && op<sizeof(buf)-1) buf[op++]=ch ;
    }
  }

  buf[op]='\0' ;

  // Save buf in variable's data

  dosetdata(var, do_string, buf, strlen(buf), path) ;

  fclose(fp) ;

  return (linenum>=0) ;
}

