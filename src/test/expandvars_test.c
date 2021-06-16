

#include <string.h>
#include <stdio.h>

#include "../libtools/mem.h"
#include "../libtools/str.h"
#include "../libdataobject/dataobject.h"


int ccexpandvariables(DATAOBJECT *vars, mem *buf, int justunquoted, int leaveifempty) ;


int main()
{
  DATAOBJECT *v = donew() ;
  dosetdata(v, do_string, "V1", 2, "/0/variable") ;
  dosetdata(v, do_string, "value1", 6, "/0/value") ;
  dosetdata(v, do_string, "V2", 2, "/1/variable") ;
  dosetdata(v, do_string, "0.2", 3, "/1/value") ;
  dosetdata(v, do_string, "V3", 2, "/2/variable") ;
  dosetdata(v, do_string, "333", 3, "/2/value") ;
  dosetdata(v, do_string, "V4", 2, "/3/variable") ;
  dosetdata(v, do_string, "0", 1, "/3/value") ;
  dosetdata(v, do_string, "V5", 2, "/4/variable") ;
  dosetdata(v, do_string, "1", 1, "/4/value") ;

  mem *b = mem_malloc(512) ;

  strcpy(b, "Unexpanded=\"$(V3)\" 333=$(i:V3) true=$(b:V3) 333.00=$(f:V3) 333=$(s:V3)") ;
  ccexpandvariables(v, b, 1, 0) ;
  printf("Just Unquoted: '%s'\n", b) ;

  strcpy(b, "\"value1=\"=\"$(V1)\" 0=$(i:V1) true=$(b:V1) 0.00=$(f:V1) value1=$(s:V1)") ;
  ccexpandvariables(v, b, 0, 0) ;
  printf("All: '%s'\n", b) ;

  strcpy(b, "false=$(b:V4) true=$(b:V5)") ;
  ccexpandvariables(v, b, 0, 0) ;
  printf("Boolean: '%s'\n", b) ;

  strcpy(b, "false=$(b:V4) Unexpanded=$(b:V5") ;
  ccexpandvariables(v, b, 0, 0) ;
  printf("Unexpanded: '%s'\n", b) ;

  strcpy(b, "0.00=$(f:V9) 0=$(i:V9) Empty=$(s:V9) false=$(b:V9)") ;
  ccexpandvariables(v, b, 0, 0) ;
  printf("Invalid Var: '%s'\n", b) ;

  strcpy(b, "%c(f:V9)=$(f:V9) %c(i:V9)=$(i:V9) %c(s:V9)=$(s:V9) %c(b:V9)=$(b:V9)") ;
  ccexpandvariables(v, b, 0, 1) ;
  printf("Invalid Var: ") ;
  printf(b, '$', '$', '$', '$') ;
  printf("'\n") ;


  mem_free(b) ;
  dodelete(v) ;

  return 0 ;
}


