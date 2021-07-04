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
#include "chromecast_BINALERT1.oggh"
#include "chromecast_BINALERT2.oggh"
#include "chromecast_BINOK1.oggh"
#include "chromecast_BINOK2.oggh"
#include "chromecast_BINNO1.oggh"
#include "chromecast_BINNO2.oggh"
#include "chromecast_BINSTART1.oggh"
#include "chromecast_BINSTART2.oggh"
#include "chromecast_BINEND1.oggh"
#include "chromecast_BINEND2.oggh"
#include "mixkit_TEST1.oggh"
#include "mixkit_TEST2.oggh"
#include "chromecast_TEST3.oggh"
#include "chromecast_TEST4.oggh"
#include "chromecast_BINPP.pngh"
#include "chromecast_BINIMG1.jpgh"
#include "chromecast_BINIMG2.jpgh"

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

 } else if (strcasecmp(path, "/pp.png")==0) {

    *data = BINPP_DATA ;
    *mediatype = BINPP_MEDIATYPE ;
    *len = BINPP_LEN ;
    return 1 ;

  } else if (strcasecmp(path, "/favicon.ico")==0 || strcasecmp(path, "/logo.png")==0) {

    *data = BINICON_DATA ;
    *mediatype = BINICON_MEDIATYPE ;
    *len = BINICON_LEN ;
    return 1 ;

 } else if (strcasecmp(path, "/test1.jpg")==0) {

    *data = BINIMG1_DATA ;
    *mediatype = BINIMG1_MEDIATYPE ;
    *len = BINIMG1_LEN ;
    return 1 ;

 } else if (strcasecmp(path, "/test2.jpg")==0) {

    *data = BINIMG2_DATA ;
    *mediatype = BINIMG2_MEDIATYPE ;
    *len = BINIMG2_LEN ;
    return 1 ;

  } else if (strcasecmp(path, "/alert1.ogg")==0) {

    *data = BINALERT1_DATA ;
    *mediatype = BINALERT1_MEDIATYPE ;
    *len = BINALERT1_LEN ;
    return 1 ;

  } else if (strcasecmp(path, "/alert2.ogg")==0) {

    *data = BINALERT2_DATA ;
    *mediatype = BINALERT2_MEDIATYPE ;
    *len = BINALERT2_LEN ;
    return 1 ;

  } else if (strcasecmp(path, "/ok1.ogg")==0) {

    *data = BINOK1_DATA ;
    *mediatype = BINOK1_MEDIATYPE ;
    *len = BINOK1_LEN ;
    return 1 ;

  } else if (strcasecmp(path, "/ok2.ogg")==0) {

    *data = BINOK2_DATA ;
    *mediatype = BINOK2_MEDIATYPE ;
    *len = BINOK2_LEN ;
    return 1 ;

  } else if (strcasecmp(path, "/no1.ogg")==0) {

    *data = BINNO1_DATA ;
    *mediatype = BINNO1_MEDIATYPE ;
    *len = BINNO1_LEN ;
    return 1 ;

  } else if (strcasecmp(path, "/no2.ogg")==0) {

    *data = BINNO2_DATA ;
    *mediatype = BINNO2_MEDIATYPE ;
    *len = BINNO2_LEN ;
    return 1 ;

  } else if (strcasecmp(path, "/start1.ogg")==0) {

    *data = BINSTART1_DATA ;
    *mediatype = BINSTART1_MEDIATYPE ;
    *len = BINSTART1_LEN ;
    return 1 ;

  } else if (strcasecmp(path, "/start2.ogg")==0) {

    *data = BINSTART2_DATA ;
    *mediatype = BINSTART2_MEDIATYPE ;
    *len = BINSTART2_LEN ;
    return 1 ;

  } else if (strcasecmp(path, "/end1.ogg")==0) {

    *data = BINEND1_DATA ;
    *mediatype = BINEND1_MEDIATYPE ;
    *len = BINEND1_LEN ;
    return 1 ;

  } else if (strcasecmp(path, "/end2.ogg")==0) {

    *data = BINEND2_DATA ;
    *mediatype = BINEND2_MEDIATYPE ;
    *len = BINEND2_LEN ;
    return 1 ;

  } else if (strcasecmp(path, "/test1.ogg")==0) {

    *data = TEST1_DATA ;
    *mediatype = TEST1_MEDIATYPE ;
    *len = TEST1_LEN ;
    return 1 ;

  } else if (strcasecmp(path, "/test2.ogg")==0) {

    *data = TEST2_DATA ;
    *mediatype = TEST2_MEDIATYPE ;
    *len = TEST2_LEN ;
    return 1 ;

  } else if (strcasecmp(path, "/test3.ogg")==0) {

    *data = TEST3_DATA ;
    *mediatype = TEST3_MEDIATYPE ;
    *len = TEST3_LEN ;
    return 1 ;

  } else if (strcasecmp(path, "/test4.ogg")==0) {
  
    *data = TEST4_DATA ;
    *mediatype = TEST4_MEDIATYPE ;
    *len = TEST4_LEN ;
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
