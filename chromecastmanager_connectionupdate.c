//
// chromecastmanager_connectionupdate.c
//
// Handler function to manage chromecast connections
//
//

// Protobuf over TLS connection to chromecast devices
#include "chromecast_interface.h"

// Logging
#include "libtools/log.h"

// MDNS data management
#include "chromecast_mdns.h"

#include "chromecastmanager.h"


/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// @brief Maintain chromecast device list
// Returns true

int chromecast_device_connection_update(CHROMECAST **cclist, int maxcc) 
{
  for (int i=0; i<maxcc; i++) {

    char *ipaddress = chromecast_mdns_at(i)->ipaddress ;
    int port = chromecast_mdns_at(i)->port ;
    char *friendlyname = chromecast_mdns_at(i)->friendlyname ;

    if (cclist[i] && !ipaddress) {

      // MDNS reports connection no longer present, so force closure

      logmsg( LOG_NOTICE, "Forcing disconnection of %s chromecast at %s:%d",
              friendlyname?friendlyname:"unknown", ipaddress, port) ;

      ccdisconnect(cclist[i]) ;
      ccdelete(cclist[i]) ;
      cclist[i]=NULL ;

    } else if (ipaddress && port>0 && !cclist[i]) {

      // MDNS report device now present, so connect to it

      cclist[i] = ccnew() ;
      if (cclist[i]) {
        if (!ccconnect(cclist[i], ipaddress, port)) {
          ccdelete(cclist[i]) ;
          cclist[i]=NULL ;
        } else {
          logmsg( LOG_NOTICE, "Establishing connection to %s chromecast at %s:%d", 
                  friendlyname?friendlyname:"unknown", ipaddress, port ) ;

          ccsendconnectionmessage(cclist[i], "CONNECT") ;
          ccsendreceivermessage(cclist[i],"GET_STATUS") ;
        }

      }

    }

  }

  return 1 ;

}



