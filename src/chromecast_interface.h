//
// chromecast_interface.h
//
//

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
////
////

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
////
//// Variable Management
////
//// Each Chromecast device has its own vars dataobject, which is used
//// to store variables along with their associated namespace and path
////
//// The recv operation scans messages received, and updates variables
//// when encountered. e.g. the following receipt:
////
//// {
////     namespace: "urn:x-cast:com.google.cast.receiver",
////     message: {"type":"MEDIA_STATUS",
////               "status":{
////                   "applications":[
////                      {"appId":"CC1AD845" .....
////
//// Can be watched with: 
////
////    ccaddwatch( cch, "myAppId", 
////                "urn:x-cast:com.google.cast.receiver", 
////                "MEDIA_STATUS",
////                "/message/status/applications/0/appId") ;
////
//// And accessed with:
////
////   ccgetwatch(cch, "myAppId") ;
////
////
//// Any send operations will then substitute the $myAppId variable with
//// the actual value. Note that all watch values are stored as strings.
////



#ifndef _CHROMECASTINTERFACE_DEFINED_
#define _CHROMECASTINTERFACE_DEFINED_

#include <sys/select.h>
#include <time.h>
#include "libdataobject/dataobject.h"
#include "libtools/net.h"
#include "libtools/mem.h"


typedef struct {

  NET *ssl ;

  enum { REC_START=0, REC_SIZE, REC_BODY, REC_DONE, REC_PARSED } recvstate ;

  int recvsize ;
  int recvlen ;
  char *recvbuf ;

  DATAOBJECT *vars ;
  DATAOBJECT *httpsessionvars ;
  DATAOBJECT *sysvarsptr ;

  DATAOBJECT *recvobject ;
  DATAOBJECT *sendobject ;

  DATAOBJECT *macro ;  // Macro data
  int macroindex ;     // Current step number
  time_t macrotimer ;  // Time value that pause started
  int macroforce ;     // Force service even if no message received

  time_t lastreceipt ; // Time last message was received
  int pingssent ;      // Number of pings sent

  // flags field for user to set/get as sees fit
  int flags ; 

} CHROMECAST ;

//////////////////////////////////////////////////////////////////////////
//


//////////////////////////////////////////////////////////////////////////
//
// https://github.com/thibauts/node-castv2-client/tree/master/lib/controllers
//


#define CC_APPID_DEFAULT_MEDIA_RECEIVER "CC1AD845"

//////////////////////////////////////////////////////////////////////////
//
// @brief Create a new chromecast connection structure
// @return handle to connection
//

CHROMECAST *ccnew(DATAOBJECT *sysvars) ;


//////////////////////////////////////////////////////////////////////////
//
// @brief Establish connection to chromecast client
// @param(in) cch Handle of chromecast device
// @param(in) ipaddress Address of device
// @param(in) port Port of device
// @return True on success
//

int ccconnect(CHROMECAST *cch, char *ipaddress, int port) ;


//////////////////////////////////////////////////////////////////////////
//
// @brief Disconnect connection to chromecast client
// @param(in) cch Handle of chromecast device
// @return True on success
//

int ccdisconnect(CHROMECAST *cch) ;


//////////////////////////////////////////////////////////////////////////
//
// @brief Delete / release Chromecast structure
// @param(in) cch Handle of chromecast device
// @return True on success
//

int ccdelete(CHROMECAST *cch) ;



//////////////////////////////////////////////////////////////////////////
//
// @brief Add connection to fd_set if active
// @param(in) sh Handle of network connection
// @param(in) fds FD Set for select()
// @param(inout) l pointer to largest fd found
// @return number of connections added
//

int ccrdfdset(CHROMECAST *cch, fd_set *rfds, fd_set *wfds, int *l) ;


//////////////////////////////////////////////////////////////////////////
//
// @brief Test to see if data is available
// @param(in) sh Handle of network connection
// @param(in) fds FD Set for select()
// @return Return true if information available
//

int ccrdfdisset(CHROMECAST *cch, fd_set *rfds, fd_set *wfds) ;



//////////////////////////////////////////////////////////////////////////
// 

char *ccfdsetinfo(CHROMECAST *cch, fd_set *rfds, fd_set *wfds) ;


//////////////////////////////////////////////////////////////////////////
// 
// @brief Get network connection state
// @param(in) Handle of open connection
// @return Returns true if network connection is connected
//

int ccisconnected(CHROMECAST *cch) ;


//////////////////////////////////////////////////////////////////////////
// 
// @brief Get time since last message receipt
// @param(in) Handle of open connection
// @return Returns idle time in seconds
//

int ccidletime(CHROMECAST *cch) ;


//////////////////////////////////////////////////////////////////////////
// 
// @brief Get number of pings sent since last receipt
// @param(in) Handle of open connection
// @return Returns number of pings
//

int ccpingssent(CHROMECAST *cch) ;


//////////////////////////////////////////////////////////////////////////
//
// @brief Set and get anciliary flag data
// @return flags
//
int ccsetflag(CHROMECAST *cch, int flags) ;
int ccclearflag(CHROMECAST *cch, int flags) ;
int ccgetflag(CHROMECAST *cch) ;

#define CC_FLAG_PROCESSING 1
#define CC_FLAG_LOADSENT 2
#define CC_FLAG_JSONQUERY 4
#define CC_FLAG_ALL 7


//////////////////////////////////////////////////////////////////////////
//
// @brief Return IP address / peer port of current connection

char *ccipaddress(CHROMECAST *cch) ;
int ccpeerport(CHROMECAST *cch) ;


//////////////////////////////////////////////////////////////////////////
//
// @brief Process input
// @param(in) cch Handle of chromecast device
// @return 0 if processing does not require attention yet
// @return -1 if connection is closed by peer
// @return -2 if connection error occurred
// @return >1 if processing complete and an input is ready (use ccgetcmd)
//

int ccrecv(CHROMECAST *cch) ;



//////////////////////////////////////////////////////////////////////////
//
// @brief Add variable to watch list
// @param(in) cch Handle of chromecast device
// @param(in) variable Name of variable
// @param(in) namespace Namespace to use in search
// @param(in) type Message type ("*" for any)
// @param(in) path Path to variable in namespace to add to watch
// @return True if watch could be added
//

int ccaddwatch(CHROMECAST *cch, char *variable, char *namespace, char *type, char *path) ;


//////////////////////////////////////////////////////////////////////////
//
// @brief Set variable value (string, int or float)
// @param(in) cch Handle of chromecast device
// @param(in) variable Name of variable
// @param(in) value Value to be stored
// @return True if value could be added

int ccsetwatch(CHROMECAST *cch, char *variable, char *value) ;


//////////////////////////////////////////////////////////////////////////
//
// @brief Return the value for the named variable
// @param(in) cch Handle of chromecast device
// @param(int) variable Variable name
// @return Pointer to string, or NULL
//

char *ccgetwatch(CHROMECAST *cch, char *variable) ;


//////////////////////////////////////////////////////////////////////////
//
// @brief Return the variable details at the given index
// @param(in) cch Handle of chromecast device
// @param(in) index Index of variable
// @return Variable details or NULL if not found
//

char * ccgetwatchvarnameat(CHROMECAST *cch, int index) ;
char * ccgetwatchat(CHROMECAST *cch, int index) ;


//////////////////////////////////////////////////////////////////////////
//
// @brief Increments the seq (requestId) stored in vars
// @param(in) vars Pointer to a variables structure
// @returns true on success

int ccincrementrequestid(DATAOBJECT *vars) ;

//////////////////////////////////////////////////////////////////////////
//
// @brief Gets the requestId value stored in vars
// @param(in) vars Pointer to a variables structure
// @returns new value

int ccgetrequestid(DATAOBJECT *vars) ;


//////////////////////////////////////////////////////////////////////////
//
// @brief Expand variables in format #(t:file:l:c) @(t:http:l:c) $(t:var)
// @param(in) dh Pointer to data object which contains variables to expand
// @param(in) vars Pointer to a variables structure
// @param(in) leaveifempty If true, empty / invalid variables are not expanded
// @param(in) loadfilehttp If true, @() and #() will be expanded
// @return True on success
//

int ccexpandvariables(DATAOBJECT *dh, DATAOBJECT *vars, int leaveifempty, int loadfilehttp) ;
int ccexpandstrvariables(mem *buf, DATAOBJECT *vars, int leaveifempty, int loadfilehttp) ;



//////////////////////////////////////////////////////////////////////////
//
// @brief Process input
// @param(in) cch Handle of chromecast device
// @return Handle of message or NULL if no message ready
//

DATAOBJECT * ccgetmessage(CHROMECAST *cch) ;



//////////////////////////////////////////////////////////////////////////
//
// @brief Retrieve last sent message as data object
// @param(in) cch Handle of chromecast device
// @return Handle of message or NULL if no message sent
//

DATAOBJECT *ccgetsentmessage(CHROMECAST *cch) ;



//////////////////////////////////////////////////////////////////////////
//
// @brief Send Message
// @param(in) cch Handle of chromecast device
// @param(in) command Dataobject of command to send
// @return True on success
//

#define CC_NAMESPACE_DEVICEAUTH "urn:x-cast:com.google.cast.tp.deviceauth"
#define CC_NAMESPACE_HEARTBEAT "urn:x-cast:com.google.cast.tp.heartbeat"
#define CC_NAMESPACE_CONNECTION "urn:x-cast:com.google.cast.tp.connection"
#define CC_NAMESPACE_RECEIVER "urn:x-cast:com.google.cast.receiver"
#define CC_NAMESPACE_MEDIA "urn:x-cast:com.google.cast.media"

#define CC_DEFAULT_SENDER "sender-0"
#define CC_DEFAULT_RECEIVER "receiver-0"

int ccsendmessage(CHROMECAST *cch, char *sender, char *receiver, char *namespace, const char *message, ...) ;


//////////////////////////////////////////////////////////////////////////
//
// @brief Quick-send messages on default sender/receiver
//

int ccsendconnectionmessage(CHROMECAST *cch, char *type) ;
int ccsendheartbeatmessage(CHROMECAST *cch, char *type) ;
int ccsendreceivermessage(CHROMECAST *cch, char *type) ;


//////////////////////////////////////////////////////////////////////////
//
// @brief Search the vars list for the specified variable
// @param(in) vars List of variables
// @param(in) variable Name of variable to match
// @return pointer to variable data object or NULL
//

DATAOBJECT *ccfindvariable(DATAOBJECT *vars, char *variable) ;


//////////////////////////////////////////////////////////////////////////
//
// @brief Sets the value of the given variable
// @param(in) vars List of variables
// @param(in) variable Name of variable
// @param(in) value Value to set
// @return pointer to variable data object or NULL on error
//

DATAOBJECT *ccsetvariable(DATAOBJECT *vars, char *variable, char *value) ;

//////////////////////////////////////////////////////////////////////////
//
// @brief Gets the value of the given variable
// @param(in) vars List of variables
// @param(in) variable Name of variable
// @return pointer to variable string value or NULL on error
//

char *ccgetvariable(DATAOBJECT *vars, char *variable) ;


#endif

