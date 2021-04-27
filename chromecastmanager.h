//
// chromecastmanager.h
//
//

#ifndef _CHROMECAST_MANAGER_DEFINED_
#define _CHROMECAST_MANAGER_DEFINED_

#include "libtools/httpd.h"
#include "chromecast_interface.h"




////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// chromecastmanager_device_connection.c
//
// This function manages the MDNS device list and the chromecast
// connection device list and opens / closes connections as required
// so as to maintain a working active control / status connection to
// each chromecast device.
//

int chromecast_device_connection_update(CHROMECAST **cclist, int maxcc) ;





////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// chromecastmanager_device_response.c
//
// This function processes response messages from all / each 
// active chromecast device, and if pending, forwards the response
// to the current http connection.
// The function also handles background tasks such as responding to
// PING messages.
//

int chromecast_device_response_process(CHROMECAST *cch, HTTPD *httpsh) ;





////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// chromecastmanager_devicerequest.c
//
// These functions handle requests from the http server, and
// return the requested built-in file, or processes the request
// sending data to the required chromecast device and providing 
// a response.  Responses from the chromecast devices are handled
// in the chromecastmanager_deviceresponse function, above.
// The response handler function _may_ call the 
//   chromecast_device_request_process_command_completion function
// if further commading action is required to complete the 
// request / task.
//

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// @brief Process http request
// Returns index of device processed or -1
//

int chromecast_device_request_process(HTTPD *httpsh, CHROMECAST **cclist, int maxcc) ;


/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// @brief Process Device List
// Returns true

int chromecast_device_request_process_devicelist(HTTPD *httpsh, CHROMECAST **cclist, int maxcc) ;


/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// @brief Process Chromecast JSON Query
// Returns true

int chromecast_device_request_process_jsonquery(HTTPD *httpsh, CHROMECAST *cch) ;


/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// @brief Process Chromecast Command
// Returns true

int chromecast_device_request_process_command(HTTPD *httpsh, CHROMECAST *cch) ;


/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// @brief Completes Chromecast Command Processing
// Returns true on complete

int chromecast_device_request_process_command_sendload(HTTPD *httpsh, CHROMECAST *cch) ;
int chromecast_device_request_process_command_loadcomplete(HTTPD *httpsh, CHROMECAST *cch) ;
int chromecast_device_request_process_command_loadfailed(HTTPD *httpsh, CHROMECAST *cch) ;



////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// chromecastmanager.c
//

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// @brief Find device friendlyname in mdns list
// @return index of device or -1
//

int chromecast_finddevice(char *device, int maxcc) ;

#endif


