//
// html_filesystem.c
//
//

#include <string.h>
#include <stddef.h>

#include "html_filesystem.h"

#include "chromecast_HTMLCSS.cssh"
#include "chromecast_HTMLSCRIPT.jsh"
#include "chromecast_HTMLSAMPLEDEVICELIST.jsonh"
#include "chromecast_HTMLTEST.htmlh"
#include "chromecast_HTMLHELP.htmlh"
#include "chromecast_HTML404.htmlh"
#include "chromecast_BINICON.pngh"
#include "chromecast_BINALERT.mp3h"

int html_filesystem(char *path, char **data, char **mediatype, int *len)
{
  if (!path || !data || !mediatype || !len) return 0 ;

  if (strcasecmp(path, "/")==0) {

    *data = HTMLHELP_DATA ;
    *mediatype = HTMLHELP_MEDIATYPE ;
    *len = HTMLHELP_LEN ;
    return 1 ;

  } else if (strcasecmp(path, "/help")==0) {

    *data = HTMLHELP_DATA ;
    *mediatype = HTMLHELP_MEDIATYPE ;
    *len = HTMLHELP_LEN ;
    return 1 ;

 } else if (strcasecmp(path, "/sampledevicelist")==0) {

    *data = HTMLSAMPLEDEVICELIST_DATA ;
    *mediatype = HTMLSAMPLEDEVICELIST_MEDIATYPE ;
    *len = HTMLSAMPLEDEVICELIST_LEN ;
    return 1 ;

  } else if (strcasecmp(path, "/favicon.ico")==0) {

    *data = BINICON_DATA ;
    *mediatype = BINICON_MEDIATYPE ;
    *len = BINICON_LEN ;
    return 1 ;

  } else if (strcasecmp(path, "/alert.mp3")==0) {

    *data = BINALERT_DATA ;
    *mediatype = BINALERT_MEDIATYPE ;
    *len = BINALERT_LEN ;
    return 1 ;

  } else if (strcasecmp(path, "/site.css")==0) {

    *data = HTMLCSS_DATA ;
    *mediatype = HTMLCSS_MEDIATYPE ;
    *len = HTMLCSS_LEN ;
    return 1 ;

  } else if (strcasecmp(path, "/test.js")==0) {

    *data = HTMLSCRIPT_DATA ;
    *mediatype = HTMLSCRIPT_MEDIATYPE ;
    *len = HTMLSCRIPT_LEN ;
    return 1 ;

  } else if (strcasecmp(path, "/test")==0) {

    *data = HTMLTEST_DATA ;
    *mediatype = HTMLTEST_MEDIATYPE ;
    *len = HTMLTEST_LEN ;
    return 1 ;

  } else if (strcasecmp(path, "/404.html")==0) {

    *data = HTML404_DATA ;
    *mediatype = HTML404_MEDIATYPE ;
    *len = HTML404_LEN ;
    return 1 ;

  } else {

    *data = HTML404_DATA ;
    *mediatype = HTML404_MEDIATYPE ;
    *len = HTML404_LEN ;
    return 0 ;

  }
}
