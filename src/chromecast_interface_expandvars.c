//
//
//
//

#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include "libnet/httpc.h"
#include "libtools/mem.h"
#include "libtools/str.h"
#include "libtools/httpd.h"
#include "libdataobject/dataobject.h"
#include "chromecast_interface.h"

// getscriptfolder from chromecastmanager.c
char *getscriptfolder() ;

// Functions to extract data from a file
int _ccexpandvars_updatecache(DATAOBJECT *var, int isfile, int suffixline, int suffixcolumn) ;

int _ccexpandvars_loaddata(DATAOBJECT *root, int isfile) ;
int _ccexpandvars_readfromfile(char *filename, char **buf, int *buflen) ;
int _ccexpandvars_readfromhttp(char *filename, char **buf, int *buflen) ;



//////////////////////////////////////////////////////////////////////////
//
// @brief Expand variables in format #(t:file:l:c) @(t:http:l:c) $(t:var)
// @param(in) dh Pointer to data object which contains variables to expand
// @param(in) vars Pointer to a variables structure
// @param(in) leaveifempty If true, empty / invalid variables are not expanded
// @param(in) loadfilehttp If true, @() and #() will be expanded
// @return True on success
//

int ccexpandvariables(DATAOBJECT *dh, DATAOBJECT *vars, int leaveifempty, int loadfilehttp)
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

      if (ccexpandstrvariables(buf, vars, leaveifempty, loadfilehttp)) {
        dosetdata(thisdh, type, buf, strlen(buf), "/") ;
      }
      mem_free(buf) ;

    }

    // Expand variables in label

    char *label = donodelabel(thisdh) ;
    if (strstr(label,"(")) {
      mem *buf = mem_malloc(strlen(label) + 1024) ;
      if (ccexpandstrvariables(buf, vars, leaveifempty, loadfilehttp)) {
        dorenamenode(thisdh, "/", buf) ;
      }
      mem_free(buf) ;
    }

    // Recurse if possible

    if (dochild(thisdh)) ccexpandvariables(dochild(thisdh), vars, leaveifempty, loadfilehttp) ;

    // Move to the next object

    thisdh = donext(thisdh) ;
  }

  return 1 ;

fail:

  return 0 ;
}


int ccexpandstrvariables(mem *buf, DATAOBJECT *vars, int leaveifempty, int loadfilehttp) 
{

  if (!vars || !buf) return 0 ;

  int updated=0 ;
  DATAOBJECT *vh ;
  int p=0 ;
  int inquote=0 ;

  // quick check / return
  if (!strstr(buf, "$(") && !strstr(buf, "#(") && !strstr(buf, "@(")) return 0 ;

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
    int suffixline = 0 ;
    int INCREMENT = -1 ; // suffixline constant
    int CURRENT = 0 ;    // suffixline constant
    int suffixcolumn = 0 ;
    int isfile = 0 ;
    int ishttp = 0 ;

    if (buf[p]=='\\') {
      p++ ;
    } else if (buf[p]=='\"') {
      inquote = !inquote ;
    }

    if ( ( (buf[p]=='$') && buf[p+1]=='(' )  ||
         ( (buf[p]=='@' || buf[p]=='#') && buf[p+1]=='(' && loadfilehttp ) ) {

      char *tstvariable = &buf[p] ;

      // Capture type

      isfile = (buf[p]=='#') ;
      ishttp = (buf[p]=='@') ;

      type = 's' ;
      if (buf[p+2]!='\0' && buf[p+3]==':') {
        type=buf[p+2] ;
        varlen=4 ;
      } else {
        varlen=2 ;
      }      

      //////////////////////////////
      // Copy variable

      int s=0 ;
      int freecopy=0 ;

      // http / https contain colons which must be copied
      // as part of the variable, and not used as terminators

      if (ishttp && strncmp(&buf[p+varlen],"http://",7)==0) {
        freecopy=7 ;
      } else if (ishttp && strncmp(&buf[p+varlen],"https://",8)==0) {
        freecopy=8 ;
      }

      // Copy variable

      while ( ( s < freecopy ) || 
              ( buf[p+varlen]!='\n' && buf[p+varlen] != '\0' && 
                buf[p+varlen] != ')' && buf[p+varlen] != ':' &&
                buf[p+varlen] != '$' && buf[p+varlen] !='@' && 
                buf[p+varlen] != '(' && s < MAXVARLABELLEN-1 ) ) {
        variable[s] = buf[p+varlen] ;
        varlen++ ;
        s++ ;
      }

      if (buf[p+varlen]==')') {

        // Terminate variable

        variable[s]='\0' ;
        varlen++ ;

      } else if (buf[p+varlen]==':') {

        // Store suffix values
        // suffix is in the form :<linenumber>:<columnnumber>
        // Linenumber = 0 or * -> current line is used
        // Linenumber = + -> current line is used, but incrmented afterwards
        // Columnnumber = missing -> First column is used
        // Columnnumber = 0 or * -> whole line is used
        // Linenumber or Columnnumber is digit -> appropriate line and column is used

        // Terminate variable

        variable[s]='\0' ;
        varlen++ ;

        if (buf[p+varlen]=='+') {
          suffixline = INCREMENT ;
        } else if (buf[p+varlen]=='*') {
          suffixline = CURRENT ;
        } else if (buf[p+varlen]>='0' && buf[p+varlen]<='9') {
          suffixline = atoi(&buf[p+varlen]) ;
        }

        // Check / skip suffix

        while (buf[p+varlen]=='+' || buf[p+varlen]=='*' || (buf[p+varlen]>='0' && buf[p+varlen]<='9') ) varlen++ ;

        // Extract column value

        if (buf[p+varlen]==':') {
          varlen++ ;
          if (buf[p+varlen]=='*' || (buf[p+varlen]>='0' && buf[p+varlen]<='9')) {
            suffixcolumn = atoi(&buf[p+varlen]) ;
          } else {
            suffixcolumn = 1 ;
          }
        }

        // Skip to end of suffix

        while (buf[p+varlen]=='+' || buf[p+varlen]=='*' || (buf[p+varlen]>='0' && buf[p+varlen]<='9') ) varlen++ ;

        // Check for correct termination

        if (buf[p+varlen]==')') {

          varlen++ ;

        } else {

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

      //////////////////////////////
      // Get variable value

      if (isfile || ishttp) {

        // Create / find @variable or #variable
        vh = ccsetvariable(vars, variable, "") ;
        _ccexpandvars_updatecache(vh, isfile, suffixline, suffixcolumn) ;

      } else {

        // find $variable
        vh = ccfindvariable(vars, variable) ;

      }

      // Get the value
      if (vh) {
        vhvalue = dogetdata(vh, do_string, NULL, "/value") ;
      }
    
      //////////////////////////////
      // Do substitution

      if (!vhvalue && !leaveifempty) vhvalue="" ;

      if (vhvalue) {

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
// @brief Load value from a file and store in var
//

int _ccexpandvars_updatecache(DATAOBJECT *var, int isfile, int suffixline, int suffixcolumn)
{
  long int line = 1 ;

  ////////////////////////////////////////////
  // Get current line and cached data value

  if (suffixline>0) {

    // Line number requested (0=current)

    line = suffixline ;

  } else {

    // No line number requested, so fetch
    // default = 1

    dogetsint(var, do_sint32, &line, "/line") ;

  }

  if (line<0) return 0 ;

  ////////////////////////////////////////////
  // Get cached value and update as needed

  if (_ccexpandvars_loaddata(var, isfile)) {

    char *value = dogetdata(var, do_string, NULL, "/filesource/%d/%d", line, suffixcolumn) ;
    if (!value) { value="" ; }
    dosetdata(var, do_string, value, strlen(value), "/value") ;
    
  } else {

   // Extraction failed
   line=-1 ;

  }    

  ////////////////////////////////////////////
  // Update saved line number

  if (line>=0 && suffixline<0 ) {

    // Increment line number ready for next read 

    line++ ;

  }
 
  if (line<0 || line!=suffixline) {

    dosetsint(var, do_sint32, line, "/line") ;

  }

  return (line>=0) ;

}



//////////////////////////////////////////////////////////////////////////
//
// @brief Load data from file or http into root variable object
// @param(in) root Data object in which to store the information
// @return true on success
//

int _ccexpandvars_loaddata(DATAOBJECT *root, int isfile)
{
  FILE *fp ;
  int linenum=1 ;
  char *filename=NULL ;
  char *buf ;
  int buflen ;
  char separator=',' ;

  if (!root) return 0 ;

  // Check if loaded / load attempted

  long int loaded=0 ;
  dogetuint(root, do_uint32, &loaded, "/loaded") ;
  if (loaded==1) return 1 ;
  else if (loaded==2) return 0 ;

  // Get filename

  filename = dogetdata(root, do_string, NULL, "/variable") ;
  if (!filename || *filename=='\0') goto loadfail;

  buf=NULL ;
  buflen=0 ;

  if (isfile) {

    _ccexpandvars_readfromfile(filename, &buf, &buflen) ;

  } else {

    _ccexpandvars_readfromhttp(filename, &buf, &buflen) ;

  }

  // Try to detect separator (non-space/newline character < ch32)

  for (int x=0; buf[x]!='\0' && separator==','; x++) {
    if (buf[x]!='\n' && buf[x]!='\r' && buf[x]!='\t' && buf[x]<32) {
      separator = buf[x] ;
    }
  }

  if (!buf) goto loadfail ;

  int s=0, e, nexts ;

  while (buf[s]!='\0') {

    // Skip leading spaces

    while (isspace(buf[s])) s++ ;

    // Locate end of line

    e=s ;
    while (buf[e]!='\0' && buf[e]!='\n' && buf[e]!='\r') e++ ;
    if (buf[e]=='\0') nexts=e ;
    else nexts=e+1 ;

    // Unskip trailing spaces

    while (isspace(buf[e])) e-- ;
    buf[e+1]='\0' ;

    if (buf[s]!='#' && buf[s]!='\0') {

      // entry 0 is the whole line

      dosetdata(root, do_string, &buf[s], strlen(&buf[s]), "/filesource/%d/0", linenum) ;

      int isend=0 ;
      int columnnum=1 ;

      // entry 1 - n is the appropriate,subsection,of,the,line

      do {

        for (e=s; buf[e]!=separator && buf[e]!='\0'; e++) ;
        if (buf[e]=='\0') isend=1 ;

        buf[e]='\0' ;
        dosetdata(root, do_string, &buf[s], strlen(&buf[s]), "/filesource/%d/%d", linenum, columnnum) ;

        if (!isend) s = e+1 ;

        columnnum++ ;

      } while (!isend) ;

      linenum++ ;

    }

    s = nexts ;

  }

  if (buf) free(buf) ;
  dosetuint(root, do_uint32, 1, "/loaded") ;
  return 1 ;

loadfail:

  if (buf) free(buf) ;
  dosetuint(root, do_uint32, 2, "/loaded") ;
  return 0 ;

}


//////////////////////////////////////////////////////////////////////////
//
// @brief Read contents of file into buffer
// @param(in) filename Pointer to filename in input script
// @param(out) buf Pointer to output storage buffer
// @param(in) maxlen maximum length of buffe
// @return true on success
//

int _ccexpandvars_readfromfile(char *filename, char **buf, int *buflen)
{

  if (!filename || !buf || !buflen) goto loadfail;

  if (*buf) free(*buf) ;
  (*buf)=NULL ;
  (*buflen)=0 ;

  char filepath[1024] ;
  int fd=-1 ;
  int r=0 ;

  // Files with paths which include  .. or are referenced from / are verboten

  if (strstr(filename, "..") || *filename=='/') goto loadfail ;

  // Open file

  snprintf(filepath, sizeof(filepath)-1, "%s/%s", getscriptfolder(), filename) ;
  fd=open(filepath, O_RDONLY);
  if (fd<0) goto loadfail ;

  // Load file contents into buf

  do {

    // Always allocate an extra byte to enable \0 termination

    (*buf) = realloc( (*buf), (*buflen)+256 ) ;
    if (!(*buf)) goto loadfail ;

    // Read and append to buffer

    r = read(fd, &(*buf)[(*buflen)], 255) ;

    // Increment buffer length

    if (r>0) {
      (*buflen)+=r ;
    }

  } while (r>0) ;

  // Terminate and close

  (*buf)[(*buflen)]='\0' ;

  close(fd) ;

  return 1 ;

loadfail:

  if (fd>=0) close(fd) ;

  return 0 ;

}


//////////////////////////////////////////////////////////////////////////
//
// @brief Read contents of http session into buffer
// @param(in) filename Pointer to filename in input script
// @param(out) buf Pointer to output storage buffer
// @param(in) maxlen maximum length of buffe
// @return true on success
//

int _ccexpandvars_readfromhttp(char *filename, char **buf, int *buflen)
{

  if (!filename || !buf || !buflen) goto loadfail;

  if (*buf) free(*buf) ;
  (*buf)=NULL ;
  (*buflen)=0 ;

  int r = httpsend( GET, filename, NULL, NULL, NULL, buf, buflen) ;

  return (r==200) ;

loadfail:

  return 0 ;

}




