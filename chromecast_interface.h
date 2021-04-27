//
// chromecast_interface.c
//
//

#ifndef _CHROMECASTINTERFACE_DEFINED_
#define _CHROMECASTINTERFACE_DEFINED_

#include <sys/select.h>
#include "libdataobject/dataobject.h"
#include "libtools/net.h"

typedef struct {
  NET *ssl ;
  int requestid ;
  enum { REC_START=0, REC_SIZE, REC_BODY, REC_DONE, REC_PARSED } recvstate ;
  int recvsize ;
  int recvlen ;
  char *recvbuf ;
  DATAOBJECT *recvobject ;
// TODO: add unsolicitedrecvobject and function to obtain it
  DATAOBJECT *sendobject ;
  time_t lastreceipt ;
  int pingssent ;
  int flags ; // flags field for user to set/get as sees fit
  int state ; // received message state
  char *receiverappid ;
  char *receiverdisplayname ;
  char *receivertransportid ;
  char *receiversessionid ;
  char *receiverstatustext ;
  char *playerstate ;
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

CHROMECAST *ccnew() ;


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
// @return 0 if processing does not require attention
// @return -1 if connection is closed
// @return >1 if processing complete and an input is ready (use ccgetcmd)
//

int ccrecv(CHROMECAST *cch) ;


//////////////////////////////////////////////////////////////////////////
//
// @brief Process input
// @param(in) cch Handle of chromecast device
// @return Handle of message or NULL if no message ready
//

DATAOBJECT * ccgetmessage(CHROMECAST *cch) ;

//////////////////////////////////////////////////////////////////////////
//
// @brief Retrieve media connection details
// @param(in) cch Handle of chromecast device
// @param(in) type Record type CC_MCD_*
// @param(out) Returns pointer to data or "" if not found
//

char *ccgetmediaconnectiondetails(CHROMECAST *cch, int type) ;

#define CC_MCD_APPID 0
#define CC_MCD_DISPLAYNAME 1
#define CC_MCD_STATUSTEXT 2
#define CC_MCD_TRANSPORTID 3
#define CC_MCD_SESSIONID 4
#define CC_MCD_PLAYERSTATE 5

//////////////////////////////////////////////////////////////////////////
//
// @brief Retrieve last sent message as data object
// @param(in) cch Handle of chromecast device
// @return Handle of message or NULL if no message sent
//

DATAOBJECT *ccgetsentmessage(CHROMECAST *cch) ;


//////////////////////////////////////////////////////////////////////////
//

int ccnextrequestid(CHROMECAST *cch) ;
int cclastrequestid(CHROMECAST *cch) ;


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



#endif

