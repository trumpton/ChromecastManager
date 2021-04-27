//
// Execute command
//


#include "libtools/mem.h"
#include "libtools/str.h"

///////////////////////////////////////////////////////////
// Execute the command and return the response

int chromecast_execute_command(mem *head, mem *command, mem *response)
{
  mem *buf = mem_malloc(8192) ;
  if (!buf) return 0 ;

  *response='\0' ;

  char *method=NULL, *uri=NULL, *version=NULL, *friendlyname ;
  method=head ;
  for (int i=0; head[i]!='\0'; i++) {
    if (head[i]==' ' && !uri) { head[i]='\0' ; uri=&head[i+1] ; }
    if (head[i]==' ' && uri && !version) { head[i]='\0' ; version=&head[i+1] ; }
    if (uri) str_decode(uri) ;
  }

  if (strcmpi(method, "GET")==0 && strcmpi(uri, "/devices")==0) {

    mem_strcpy(response, "HTTP/1.1 200 OK") ;
    mem_strcat(response, "\r\nContent-Type: text/json") ;
    mem_strcpy(buf, "{ \"devices\": \"list\" }\r\n") ;

  } else if (strcmpi(method, "POST")==0 && strncmpi(uri,"/device/", 8)==0) {

    mem_strcpy(response, "HTTP/1.1 200 OK") ;
    friendlyname=&uri[i] ;
    
  } else {

    mem_strcpy(response "HTTP/1.1 404 Not Found") ;

  }

  // Complete header

  if (strlen(buf)>0) {
    mem_strcat(response, "\r\nContent-Length: ") ;
    mem_intcat(response, strlen(buf)) ;
  }
  mem_strcat(response, "\r\n\r\n") ; 

  // Attach body and return

  mem_strcat(response, buf) ;
  mem_free(buf) ;
  return 1 ;
}



