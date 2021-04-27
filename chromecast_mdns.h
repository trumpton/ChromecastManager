//
//
// chromecast_mdns.h
//
//
//

#include "libtools/mem.h"

//
// Multicast address and port
//

#define MDNS_IPADDRESS "224.0.0.251"
#define MDNS_PORT 5353


typedef struct chromecast_mdns_record {
  mem *netclass ;
  unsigned int ttl ;
  unsigned int warnttlcount ;
  unsigned int ttlcount ;
  mem *id;
  mem *devicename;
  mem *networkname;
  mem *ipaddress;
  mem *friendlyname;
  short port;
} chromecast_mdns_record ;

//
// Initialise the MDNS server
//
void chromecast_mdns_init() ;

//
// Report the MDNS file discriptor
// which can be used in the rdfs in
// a select() function
//
int chromecast_mdns_fd() ;

//
// Close down the MDNS server
//
int chromecast_mdns_close() ;

//
// MDNS update, called from a
// response to a select function
//
int chromecast_mdns_update() ;

//
// MDNS refresh, sends out a
// request on the network, and
// expires old records
//
int chromecast_mdns_refresh(int tickrate, int sendenabled, int forcesend) ;

//
// Report max number of mdns entries
//
int chromecast_mdns_size() ;

//
// Get chromecast mdns entry
//
chromecast_mdns_record *chromecast_mdns_at(int index) ;


