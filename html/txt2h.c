
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{

  if (argc!=4) {
    printf("txt2h variablename source.txt destination.h\n") ;
    return 1 ;
  }

  char *mediatype ;
  char *ext =strstr(argv[2],".") ;  
  if (ext==NULL) {
    fprintf(stderr, "txt2h: missing extension\n") ;
    return 1 ;
  } else if (strcmp(ext, ".html")==0) {
    mediatype="text/html" ;
  } else if (strcmp(ext, ".css")==0) {
    mediatype="text/css" ;
  } else if (strcmp(ext, ".js")==0) {
    mediatype="application/javascript" ;
  } else if (strcmp(ext, ".json")==0) {
    mediatype="application/json" ;
  } else {
    fprintf(stderr, "txt2h: unrecognised extension\n") ;
    return 1 ;
  }
    
  FILE *fi, *fo ;

  if (strcmp(argv[2], "-")==0) {
    fi=stdin ;
  } else {
    fi = fopen(argv[2], "r") ;
  }

  if (strcmp(argv[3], "-")==0) {
    fo=stdout ;
  } else {
    fo = fopen(argv[3], "w") ;
  }

  if (!fi || !fo) {
    fprintf(stderr, "html2h: error, unable to access source / destination\n") ;
    return 1 ;
  }

  fprintf(fo, "//\n// %s\n//\n// Created from %s with txt2h\n//\n\n", argv[3], argv[2]) ;

  fprintf(fo, "#ifndef %s_DATA\n\n", argv[1]) ;

  // Output mediatype

  fprintf(fo, "#define %s_MEDIATYPE \"%s\"\n\n", argv[1], mediatype) ;

  // Output body

  fprintf(fo, "#define %s_DATA \\\n", argv[1]) ;

  int ch ;
  int endofline=0 ;
  int startofline=1 ;
  int len=0 ;

  while ( (ch = fgetc(fi) ) > 0 ) {

    len++ ;

    if (endofline) { fprintf(fo, "\\n\" \\\n") ; }
    if (startofline) { fprintf(fo, "  \"") ; }

    if (ch=='\n') { 

      endofline=1 ;
      startofline=1 ;

    } else if (ch=='\\') {

      fprintf(fo, "\\\\") ;

    } else if (ch=='\"') {

      fprintf(fo, "\\\"") ;

    } else {

      fprintf(fo, "%c", ch) ;
      startofline=0 ;
      endofline=0 ;

    }

  }

  fprintf(fo, "\\n\"\n\n") ;
 
  // Output Length

  fprintf(fo, "#define %s_LEN %d\n\n", argv[1], len) ;

  // Done

  fprintf(fo, "#endif\n") ;

  fclose(fi) ;
  fclose(fo) ;

  return 0 ;

}
