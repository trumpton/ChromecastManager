//
//
// chromecast_mdns.c
//

#include <string.h>
#include <malloc.h>

//#include <netdb.h>
//#include <sys/stat.h>
#include <time.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#include "chromecast_mdns.h"
#include "libtools/mem.h"
#include "libtools/mdns.h"
#include "libtools/log.h"

// Local/Private constants

#define MAXCC 128
#define MAXBUF 9000
#define MAXSTRING 1024

// Local/Private variables

chromecast_mdns_record _chromecast_mdns_entry[MAXCC] ;
int _chromecast_mdns_fd ;

// Local/Private functions

int _chromecast_mdns_clrfield(mem **f) ;
int _chromecast_mdns_setfield(mem **f, mem *s) ;
int _chromecast_mdns_closerecvfd() ;
int _chromecast_mdns_openrecvfd() ;
int _chromecast_mdns_sendquery(mem *buf, int buflen) ;
char *_chromecast_mdns_localipaddress() ;
int _chromecast_mdns_setentry( mem *netclass, unsigned int ttl, unsigned int warnttl, mem *id,
                               mem *devicename, mem *friendlyname, mem *networkname, mem *ipaddress, int port) ;
int _chromecast_mdns_clrentry(chromecast_mdns_record *rec) ;
char *_chromecast_mdns_currenttime() ;

///////////////////////////////////////////////////////////
//
// Initialise the MDNS server
//
void chromecast_mdns_init()
{
  _chromecast_mdns_fd = -1 ;
  for (int i=0; i<MAXCC; i++) {
    explicit_bzero(&_chromecast_mdns_entry[i], sizeof(chromecast_mdns_record)) ;
  }
  _chromecast_mdns_openrecvfd() ;
}

///////////////////////////////////////////////////////////
//
// Close down the MDNS server
//
int chromecast_mdns_close()
{
  int success=1 ;
  for (int i=0; i<MAXCC; i++) {
    success &= _chromecast_mdns_clrentry(&(_chromecast_mdns_entry[i])) ;
   }
  _chromecast_mdns_closerecvfd() ;
  return success ;
}

///////////////////////////////////////////////////////////
//
// Report the MDNS file discriptor
// which can be used in the rdfs in
// a select() function
//
int chromecast_mdns_fd()
{
  if (_chromecast_mdns_fd<0) { _chromecast_mdns_openrecvfd() ; }
  return _chromecast_mdns_fd ;
}

///////////////////////////////////////////////////////////
//
// MDNS update, called from a
// response to a select function
//
int chromecast_mdns_update()
{
  if (chromecast_mdns_fd()<0) return 0 ;

  mem *buf = mem_malloc(MAXBUF) ;

  int udplen=recv(_chromecast_mdns_fd, (void *)buf, MAXBUF, 0) ;

  if (udplen<0) { 

    _chromecast_mdns_closerecvfd() ; 

 } else {

    int id, flags, qdcount, ancount, nscount, arcount;
    mem *name = mem_malloc(MAXBUF) ;

    int p = mdns_get_packethead(buf, &flags, &qdcount, &ancount, &nscount, &arcount) ;

    if (p>0 && qdcount==0 && (ancount+nscount+arcount)<10 ) {

      char *id = (char *)mem_malloc(MAXSTRING+1) ;
      char *devicename = (char *)mem_malloc(MAXSTRING+1) ;
      char *networkname = (char *)mem_malloc(MAXSTRING+1) ;
      char *ipaddress = (char *)mem_malloc(MAXSTRING+1) ;
      char *friendlyname = (char *)mem_malloc(MAXSTRING+1) ;
      char *netclass = (char *)mem_malloc(MAXSTRING+1) ;
      short port=0 ;
      unsigned int rhttl=0, ttl=0 ;

      for (int i=(qdcount+ancount+nscount+arcount); i>0 && p<udplen; i--) {

        unsigned short type, flags, len ;

        int q=mdns_get_recordhead(buf, p, name, &type, &flags, &rhttl, &len) ;

        if (q<0) { goto error ; }

        p+=q ;

        switch (type) {

        case 0x0C: // Check Record Type and Get Device Name
          if (strcmp(name, "_googlecast._tcp.local")!=0) { goto abort ; }
          if (ttl==0 || rhttl<ttl) ttl=rhttl ;
          strncpy(netclass, name, MAXSTRING) ;
          mdns_get_rdatanameptr(devicename, buf, p, len) ;
          break ;

        case 0x10: // Get ID, Friendly Name and TTL
          if (ttl==0 || rhttl<ttl) ttl=rhttl ;
          mdns_get_rdatafield(id, buf, p, "id=", len) ;
          mdns_get_rdatafield(friendlyname, buf, p, "fn=", len) ;
          break ;

        case 0x21: // Get Network Name and Port
          if (ttl==0 || rhttl<ttl) ttl=rhttl ;
          mdns_get_service(networkname, &port, buf, p, len) ;
          break ;

        case 0x01: // Get IP Address
          if (ttl==0 || rhttl<ttl) ttl=rhttl ;
          mdns_get_ipaddress(ipaddress, buf, p, len) ;
          break ;

        default:   // Skip to next record
          break ;

        }

        p+=len ;

      }

      // Store the record only if all data provided

      if ( netclass[0]!='\0' && friendlyname[0]!='\0' && 
           networkname[0]!='\0' && ipaddress[0]!='\0' && ttl>0 && port>0 ) {

        _chromecast_mdns_setentry( netclass, ttl+300, (ttl*9)/10, id,
                                   devicename, friendlyname, networkname, ipaddress, port) ;

      }

abort:
error:
      mem_free(netclass) ;
      mem_free(friendlyname) ;
      mem_free(ipaddress) ;
      mem_free(networkname) ;
      mem_free(devicename) ;
      mem_free(id) ;
    }

    mem_free(name) ;
  }

  mem_free(buf) ;
  return (_chromecast_mdns_fd>=0) ;
}


///////////////////////////////////////////////////////////
//
// MDNS refresh, sends out a
// request on the network, and
// expires old records
//
int chromecast_mdns_refresh(int tickrate, int sendenabled, int forcesend)
{
  int expiringentry=0 ;

  if (chromecast_mdns_fd()<0) return 0 ;

  for (int i=0; i<MAXCC; i++) {

    if ( _chromecast_mdns_entry[i].devicename ) {

      // Flag entry as demanding a refresh request

      if ( _chromecast_mdns_entry[i].warnttlcount!=0 &&
           _chromecast_mdns_entry[i].warnttlcount <= tickrate ) {
       
        expiringentry=1 ;
        _chromecast_mdns_entry[i].warnttlcount=0 ;

      } else if (_chromecast_mdns_entry[i].warnttlcount != 0) {

        _chromecast_mdns_entry[i].warnttlcount -= tickrate ;

      }

      // Countdown and expire

      if (_chromecast_mdns_entry[i].ttlcount <= tickrate) {

        logmsg(LOG_DEBUG, "mdns: removing device: %s @ %s:%d", 
                        _chromecast_mdns_entry[i].friendlyname, 
                        _chromecast_mdns_entry[i].ipaddress, 
                        _chromecast_mdns_entry[i].port) ;

        _chromecast_mdns_clrentry(&(_chromecast_mdns_entry[i])) ;

      } else {

        _chromecast_mdns_entry[i].ttlcount -= tickrate ;

      }
    }
  }      
   
  if ( sendenabled && (expiringentry || forcesend) ) {

    // Send UDP request for updates

    logmsg(LOG_DEBUG, "sending UDP request") ;

    char request[] = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0B, 0x5F, 0x67, 0x6F,
      0x6F, 0x67, 0x6C, 0x65, 0x63, 0x61, 0x73, 0x74, 0x04, 0x5F, 0x74, 0x63, 0x70, 0x05, 0x6C, 0x6F,
      0x63, 0x61, 0x6C, 0x00, 0x00, 0x0C, 0x00, 0x01 } ;
 
    if (!_chromecast_mdns_sendquery(request, sizeof(request))) {
      perror("chromecast_mdns_refresh: Unable to send UDP request") ;
    }

  }
  return 1 ;
}

///////////////////////////////////////////////////////////
//
// Report number of mdns entries
//
int chromecast_mdns_size()
{
  return MAXCC ;
}


///////////////////////////////////////////////////////////
//
// Get chromecast mdns entry
//
chromecast_mdns_record *chromecast_mdns_at(int index)
{
  if (index<0 || index>=MAXCC) index=0 ;
  return &(_chromecast_mdns_entry[index]) ;
}


////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
//
// PRIVATE FUNCTIONS
//
//


///////////////////////////////////////////////////////////
// Clear and set string

int _chromecast_mdns_clrfield(mem **f)
{
  if ((*f)!=NULL) mem_free(*f) ;
  (*f)=NULL ;
  return 1 ;
}

int _chromecast_mdns_setfield(mem **f, mem *s)
{
  _chromecast_mdns_clrfield(f) ;
  (*f) = mem_malloc(strlen(s)+1) ;
  if (*f) {
    strcpy((*f), s) ;
    return 1 ;
  } else {
    return 0 ;
  }
}

///////////////////////////////////////////////////////////
// Open the network connection for multicast transmit
// Return file handle on success, or -1 on failure
int _chromecast_mdns_sendquery(mem* msg, int msglen)
{
  int fd=-1 ;
  struct sockaddr_in srv;

  // Create socket

  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
    perror("_chromecast_mdns_sendquery: socket - unable to create socket");
    return 0 ;
  }
  int flag_on = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag_on, sizeof(flag_on));

  // Client (multicast output) settings

  struct sockaddr_in clnt;
  memset(&clnt, 0, sizeof (clnt));
  clnt.sin_family = AF_INET;
  clnt.sin_port = htons(MDNS_PORT);
  clnt.sin_addr.s_addr = inet_addr(MDNS_IPADDRESS);

  // Bind server to port

  if(bind(fd, (struct sockaddr*) &clnt, sizeof(clnt)) < 0) {
    perror("_chromecast_mdns_sendquery: bind - error binding to socket");
    return 0 ;
  }

  if (sendto(fd, msg, msglen, 0, (struct sockaddr *)&clnt, sizeof(clnt)) < 0 ) {
    perror("_chromecast_mdns_sendquery: sendto - error sending message");
    close(fd) ;
    return 0 ;
  }

  close(fd) ;
  return 1 ;
}


///////////////////////////////////////////////////////////
// Open the network connection for multicast receipt
// Return file handle on success, or -1 on failure

int _chromecast_mdns_openrecvfd()
{
  struct sockaddr_in srv;

  _chromecast_mdns_closerecvfd() ;

  // Create socket

  if ((_chromecast_mdns_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
    perror("_chromecast_mdns_openrecvfd: error creating socket");
    _chromecast_mdns_fd=-1 ;
    return -1 ;
  }
  int flag_on = 1;
  setsockopt(_chromecast_mdns_fd, SOL_SOCKET, SO_REUSEADDR, &flag_on, sizeof(flag_on));

  // Server (input) settings

  memset(&srv, 0, sizeof(srv));
  srv.sin_family = AF_INET;
  srv.sin_port = htons(MDNS_PORT);
  srv.sin_addr.s_addr = htons(INADDR_ANY) ;

  // Bind server to port

  if( bind(_chromecast_mdns_fd, (struct sockaddr*) &srv, sizeof(srv)) < 0 ) {

    perror("_chromecast_mdns_openrecvfd:: error binding to socket");
    close(_chromecast_mdns_fd);
    _chromecast_mdns_fd=-1 ;
    return -1 ;

  }

  // Set non-blocking

  int flags = fcntl(_chromecast_mdns_fd,F_GETFL,0);
  assert(flags != -1);
  fcntl(_chromecast_mdns_fd, F_SETFL, flags | O_NONBLOCK);

  // Add to multicast listen group

  struct ip_mreq group;
  group.imr_multiaddr.s_addr = inet_addr("224.0.0.251");
  group.imr_interface.s_addr = htonl(INADDR_ANY);

  if ( setsockopt( _chromecast_mdns_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
                   (const void *)(&group), sizeof(group)) < 0 ) {

    perror("_chromecast_mdns_openrecvfd: error sockopt setting multicast interface") ;
    close(_chromecast_mdns_fd);
    _chromecast_mdns_fd=-1 ;
    return -1 ;

  }


  logmsg(LOG_NOTICE, "starting MDNS listener") ;

  // And return handle

  return _chromecast_mdns_fd ; 
}

///////////////////////////////////////////////////////////
// Close the network connection and return true
//

int _chromecast_mdns_closerecvfd()
{
  if (_chromecast_mdns_fd>=0) {
    logmsg(LOG_NOTICE, "stopping MDNS listener") ;
    close(_chromecast_mdns_fd) ;
  }
  _chromecast_mdns_fd=-1 ;
  return 1 ;
}

///////////////////////////////////////////////////////////
// Set entry in structure
//
int _chromecast_mdns_setentry( mem *netclass, unsigned int ttl, unsigned int warnttl, mem *id,
                               mem *devicename, mem *friendlyname, mem *networkname, mem *ipaddress, int port) 
{
  int firstempty=-1 ;
  int entry=-1 ;
  int numslots=0 ;

  // Search for slot

  for (int i=0; i<MAXCC; i++) {
    if (_chromecast_mdns_entry[i].devicename) {
      numslots++ ;
      if (entry<0 && strcmp(_chromecast_mdns_entry[i].friendlyname, friendlyname)==0) {
          entry=i ;
      }
    } else if (firstempty<0) {
       firstempty=i ;
    }
  }

  // no match found

  if (entry<0) {

    logmsg(LOG_DEBUG, "mdns: found device: %s @ %s:%d", friendlyname, ipaddress, port) ;
    entry=firstempty ;

  }

  // No slot left

  if (firstempty<0 && entry<0) return 0 ;

  // Store contents 

  int success = ( _chromecast_mdns_setfield( &(_chromecast_mdns_entry[entry].netclass), netclass) &&
           _chromecast_mdns_setfield( &(_chromecast_mdns_entry[entry].id), id) &&
           _chromecast_mdns_setfield( &(_chromecast_mdns_entry[entry].devicename), devicename) &&
           _chromecast_mdns_setfield( &(_chromecast_mdns_entry[entry].friendlyname), friendlyname) &&
           _chromecast_mdns_setfield( &(_chromecast_mdns_entry[entry].networkname), networkname) &&
           _chromecast_mdns_setfield( &(_chromecast_mdns_entry[entry].ipaddress), ipaddress) ) ;

  if (success) {
    _chromecast_mdns_entry[entry].ttl = ttl ;
    _chromecast_mdns_entry[entry].ttlcount = ttl ;
    _chromecast_mdns_entry[entry].warnttlcount = warnttl ;
    _chromecast_mdns_entry[entry].port = port ;
  }

  return success ;
}

///////////////////////////////////////////////////////////
// Clear structure entry
//
int _chromecast_mdns_clrentry(chromecast_mdns_record *rec) 
{
  if (!rec) return 0 ; 
  rec->ttl=0 ;  
  _chromecast_mdns_clrfield(&(rec->id)) ;
  _chromecast_mdns_clrfield(&(rec->netclass)) ;
  _chromecast_mdns_clrfield(&(rec->devicename)) ;
  _chromecast_mdns_clrfield(&(rec->networkname)) ;
  _chromecast_mdns_clrfield(&(rec->friendlyname)) ;
  _chromecast_mdns_clrfield(&(rec->ipaddress)) ;
  rec->ttl=0 ;
  rec->ttlcount=0 ;
  rec->warnttlcount=0 ;
  rec->port=0 ;
  return 1 ;
}


///////////////////////////////////////////////////////////
// Output current time as string (debug purposes)
//
char *_chromecast_mdns_currenttime()
{
  time_t rawtime;
  struct tm * timeinfo;
  time ( &rawtime );
  timeinfo = localtime ( &rawtime );
  char *t=asctime(timeinfo) ;
  t[strlen(t)-1]='\0' ;
  return t ;
}


///////////////////////////////////////////////////////////
// Get a non-local network interface IPV4 address
char *_chromecast_mdns_localipaddress()
{
  static char ipaddress[32] ;
  struct ifaddrs *idap, *p ;

  strcpy(ipaddress, "0.0.0.0") ;

  if (getifaddrs(&idap)!=0) {

    perror("Unable to get local interface IP address") ;

  } else {

    for (p=idap; p!=NULL; p=p->ifa_next) {

      if ( p->ifa_addr->sa_family == AF_INET && strcmp(p->ifa_name, "lo")!=0 ) {
        struct sockaddr_in *si = (struct sockaddr_in *)p->ifa_addr ;
        strcpy(ipaddress, inet_ntoa( si->sin_addr )) ;
      }

    }
    freeifaddrs(idap);

  }

  return ipaddress ;
}
