//
//
//
//

#include <stdio.h>
#include <string.h>

#include "libtools/mem.h"
#include "libtools/str.h"
#include "libtools/httpd.h"
#include "libdataobject/dataobject.h"
#include "chromecast_interface.h"


//////////////////////////////////////////////////////////////////////////
//
// @brief Expand variables in format $(var) $(s:var) $(i:var) $(f:var) $(b:var)
// @param(in) vars Pointer to a variables structure
// @param(in) buf Pointer to string buffer
// @param(in) justunquoted If true, only expands unquoted
// @param(in) leaveifempty If true, empty / invalid variables are not expanded
// @return True on success
//

int ccexpandvariables(DATAOBJECT *vars, mem *buf, int justunquoted, int leaveifempty)
{
  if (!vars || !buf) return 0 ;

  DATAOBJECT *vh ;
  int p=0 ;
  int inquote=0 ;

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
    char variable[128] ;

    if (buf[p]=='\\') {
      p++ ;
    } else if (buf[p]=='\"') {
      inquote = !inquote ;
    }

    if (buf[p]=='$' && buf[p+1]=='(') {

      // Capture type

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

      while (buf[p+varlen] != '\0' && buf[p+varlen] != ')' && s < sizeof(variable)-1) {
        variable[s] = buf[p+varlen] ;
        varlen++ ;
        s++ ;
      }

      if (buf[p+varlen]==')') {
        variable[s]='\0' ;
        varlen++ ;
      } else {
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
          vhvalue = dogetdata(vh, do_string, NULL, "/value") ;
        }   

        i++ ;   

      }

      //////////////////////////////
      // Do substitution

      if (!vhvalue && !leaveifempty) vhvalue="" ;

      if ( (vhvalue && justunquoted && !inquote) ||
           (vhvalue && !justunquoted) ) {

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

      } 

    }

    p++ ;

  }

  return 1 ;

}
