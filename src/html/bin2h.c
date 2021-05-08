
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{

  if (argc!=4) {
    printf("txt2h variablename source.bin destination.h\n") ;
    return 1 ;
  }

  char *mediatype ;
  char *ext =strstr(argv[2],".") ;  
  if (ext==NULL) {
    fprintf(stderr, "txt2h: missing extension\n") ;
    return 1 ;
  } else if (strcmp(ext, ".mp3")==0) {
    mediatype="audio/mpeg" ;
  } else if (strcmp(ext, ".ogg")==0) {
    mediatype="audio/ogg" ;
  } else if (strcmp(ext, ".jpg")==0) {
    mediatype="image/jpeg" ;
  } else if (strcmp(ext, ".png")==0) {
    mediatype="image/png" ;
  } else {
    fprintf(stderr, "bin2h: unrecognised extension (supported: mp3, ogg, jpg, png)\n") ;
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
    fprintf(stderr, "bin2h: error, unable to access source / destination\n") ;
    return 1 ;
  }

  fprintf(fo, "//\n// %s\n//\n// Created from %s with bin2h\n//\n\n", argv[3], argv[2]) ;

  fprintf(fo, "#ifndef %s_DATA\n\n", argv[1]) ;

  // Output mediatype

  fprintf(fo, "#define %s_MEDIATYPE \"%s\"\n\n", argv[1], mediatype) ;

  // Output body

  fprintf(fo, "#define %s_DATA \\\n", argv[1]) ;

  int ch ;
  int endofline=0 ;
  int startofline=1 ;
  int len=0 ;

  while ( (ch = fgetc(fi) ) >= 0 ) {

    len++ ;

    if (endofline) { fprintf(fo, "\"\\\n") ; }
    if (startofline) { fprintf(fo, "  \"") ; }
    endofline=0 ;
    startofline=0 ;

    fprintf(fo, "\\%03o", ch) ;

    if (len%16==0) {
      endofline=1 ;
      startofline=1 ;
    }
  }

  fprintf(fo, "\"\n\n") ;
 
  // Output Length

  fprintf(fo, "#define %s_LEN %d\n\n", argv[1], len) ;

  // Done

  fprintf(fo, "#endif\n") ;

  fclose(fi) ;
  fclose(fo) ;

  return 0 ;

}
