/*  -*- mode: c; tab-width: 8; c-basic-indent: 4; -*- */

/*-
 * Copyright (c) 2001 Charles Mott <cm@linktel.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
    Alias_db.c encapsulates all data structures used for storing
    packet aliasing data.  Other parts of the aliasing software
    access data through functions provided in this file.

    Data storage is based on the notion of a "link", which is
    established for ICMP echo/reply packets, UDP datagrams and
    TCP stream connections.  A link stores the original source
    and destination addresses.  For UDP and TCP, it also stores
    source and destination port numbers, as well as an alias
    port number.  Links are also used to store information about
    fragments.

    There is a facility for sweeping through and deleting old
    links as new packets are sent through.  A simple timeout is
    used for ICMP and UDP links.  TCP links are left alone unless
    there is an incomplete connection, in which case the link
    can be deleted after a certain amount of time.


    Initial version: August, 1996  (cjm)

    Version 1.4: September 16, 1996 (cjm)
        Facility for handling incoming links added.

    Version 1.6: September 18, 1996 (cjm)
        ICMP data handling simplified.

    Version 1.7: January 9, 1997 (cjm)
        Fragment handling simplified.
        Saves pointers for unresolved fragments.
        Permits links for unspecified remote ports
          or unspecified remote addresses.
        Fixed bug which did not properly zero port
          table entries after a link was deleted.
        Cleaned up some obsolete comments.

    Version 1.8: January 14, 1997 (cjm)
        Fixed data type error in StartPoint().
        (This error did not exist prior to v1.7
        and was discovered and fixed by Ari Suutari)

    Version 1.9: February 1, 1997
        Optionally, connections initiated from packet aliasing host
        machine will will not have their port number aliased unless it
        conflicts with an aliasing port already being used. (cjm)

        All options earlier being #ifdef'ed are now available through
        a new interface, SetPacketAliasMode().  This allows run time
        control (which is now available in PPP+pktAlias through the
        'alias' keyword). (ee)

        Added ability to create an alias port without
        either destination address or port specified.
        port type = ALIAS_PORT_UNKNOWN_DEST_ALL (ee)

        Removed K&R style function headers
        and general cleanup. (ee)

        Added packetAliasMode to replace compiler #defines's (ee)

        Allocates sockets for partially specified
        ports if ALIAS_USE_SOCKETS defined. (cjm)

    Version 2.0: March, 1997
        SetAliasAddress() will now clean up alias links
        if the aliasing address is changed. (cjm)

        PacketAliasPermanentLink() function added to support permanent
        links.  (J. Fortes suggested the need for this.)
        Examples:

        (192.168.0.1, port 23)  <-> alias port 6002, unknown dest addr/port

        (192.168.0.2, port 21)  <-> alias port 3604, known dest addr
                                                     unknown dest port

        These permanent links allow for incoming connections to
        machines on the local network.  They can be given with a
        user-chosen amount of specificity, with increasing specificity
        meaning more security. (cjm)

        Quite a bit of rework to the basic engine.  The portTable[]
        array, which kept track of which ports were in use was replaced
        by a table/linked list structure. (cjm)

        SetExpire() function added. (cjm)

        DeleteLink() no longer frees memory association with a pointer
        to a fragment (this bug was first recognized by E. Eklund in
        v1.9).

    Version 2.1: May, 1997 (cjm)
        Packet aliasing engine reworked so that it can handle
        multiple external addresses rather than just a single
        host address.

        PacketAliasRedirectPort() and PacketAliasRedirectAddr()
        added to the API.  The first function is a more generalized
        version of PacketAliasPermanentLink().  The second function
        implements static network address translation.

    Version 3.2: July, 2000 (salander and satoh)
        Added FindNewPortGroup to get contiguous range of port values.

        Added QueryUdpTcpIn and QueryUdpTcpOut to look for an aliasing
	link but not actually add one.

        Added FindRtspOut, which is closely derived from FindUdpTcpOut,
	except that the alias port (from FindNewPortGroup) is provided
	as input.

    See HISTORY file for additional revisions.
*/


/* System include files */
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

/* BSD network include files */
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "alias.h"
#include "alias_local.h"



/*
   Constants (note: constants are also defined
              near relevant functions or structs)
*/

/* Sizes of input and output link tables */
#define LINK_TABLE_OUT_SIZE         101
#define LINK_TABLE_IN_SIZE         4001

/* Parameters used for cleanup of expired links */
#define ALIAS_CLEANUP_INTERVAL_SECS  60
#define ALIAS_CLEANUP_MAX_SPOKES     30

/* Timeouts (in seconds) for different link types */
#define ICMP_EXPIRE_TIME             60
#define UDP_EXPIRE_TIME              60
#define PROTO_EXPIRE_TIME            60
#define FRAGMENT_ID_EXPIRE_TIME      10
#define FRAGMENT_PTR_EXPIRE_TIME     30

/* TCP link expire time for different cases */
/* When the link has been used and closed - minimal grace time to
   allow ACKs and potential re-connect in FTP (XXX - is this allowed?)  */
#ifndef TCP_EXPIRE_DEAD
#   define TCP_EXPIRE_DEAD           10
#endif

/* When the link has been used and closed on one side - the other side
   is allowed to still send data */
#ifndef TCP_EXPIRE_SINGLEDEAD
#   define TCP_EXPIRE_SINGLEDEAD     90
#endif

/* When the link isn't yet up */
#ifndef TCP_EXPIRE_INITIAL
#   define TCP_EXPIRE_INITIAL       300
#endif

/* When the link is up */
#ifndef TCP_EXPIRE_CONNECTED
#   define TCP_EXPIRE_CONNECTED   86400
#endif


/* Dummy port number codes used for FindLinkIn/Out() and AddLink().
   These constants can be anything except zero, which indicates an
   unknown port number. */

#define NO_DEST_PORT     1
#define NO_SRC_PORT      1



/* Data Structures

    The fundamental data structure used in this program is
    "struct alias_link".  Whenever a TCP connection is made,
    a UDP datagram is sent out, or an ICMP echo request is made,
    a link record is made (if it has not already been created).
    The link record is identified by the source address/port
    and the destination address/port. In the case of an ICMP
    echo request, the source port is treated as being equivalent
    with the 16-bit ID number of the ICMP packet.

    The link record also can store some auxiliary data.  For
    TCP connections that have had sequence and acknowledgment
    modifications, data space is available to track these changes.
    A state field is used to keep track in changes to the TCP
    connection state.  ID numbers of fragments can also be
    stored in the auxiliary space.  Pointers to unresolved
    fragments can also be stored.

    The link records support two independent chainings.  Lookup
    tables for input and out tables hold the initial pointers
    the link chains.  On input, the lookup table indexes on alias
    port and link type.  On output, the lookup table indexes on
    source address, destination address, source port, destination
    port and link type.
*/

struct ack_data_record     /* used to save changes to ACK/sequence numbers */
{
    u_long ack_old;
    u_long ack_new;
    int delta;
    int active;
};

struct tcp_state           /* Information about TCP connection        */
{
    int in;                /* State for outside -> inside             */
    int out;               /* State for inside  -> outside            */
    int index;             /* Index to ACK data array                 */
    int ack_modified;      /* Indicates whether ACK and sequence numbers */
                           /* been modified                           */
};

#define N_LINK_TCP_DATA   3 /* Number of distinct ACK number changes
                               saved for a modified TCP stream */
struct tcp_dat
{
    struct tcp_state state;
    struct ack_data_record ack[N_LINK_TCP_DATA];
    int fwhole;             /* Which firewall record is used for this hole? */
};

struct server              /* LSNAT server pool (circular list) */
{
    struct in_addr addr;
    u_short port;
    struct server *next;
};

struct alias_link                /* Main data structure */
{
    struct in_addr src_addr;     /* Address and port information        */
    struct in_addr dst_addr;
    struct in_addr alias_addr;
    struct in_addr proxy_addr;
    u_short src_port;
    u_short dst_port;
    u_short alias_port;
    u_short proxy_port;
    struct server *server;

    int link_type;               /* Type of link: TCP, UDP, ICMP, proto, frag */

/* values for link_type */
#define LINK_ICMP                     IPPROTO_ICMP
#define LINK_UDP                      IPPROTO_UDP
#define LINK_TCP                      IPPROTO_TCP
#define LINK_FRAGMENT_ID              (IPPROTO_MAX + 1)
#define LINK_FRAGMENT_PTR             (IPPROTO_MAX + 2)
#define LINK_ADDR                     (IPPROTO_MAX + 3)
#define LINK_PPTP                     (IPPROTO_MAX + 4)

    int flags;                   /* indicates special characteristics   */
    int pflags;                  /* protocol-specific flags */

/* flag bits */
#define LINK_UNKNOWN_DEST_PORT     0x01
#define LINK_UNKNOWN_DEST_ADDR     0x02
#define LINK_PERMANENT             0x04
#define LINK_PARTIALLY_SPECIFIED   0x03 /* logical-or of first two bits */
#define LINK_UNFIREWALLED          0x08

    int timestamp;               /* Time link was last accessed         */
    int expire_time;             /* Expire time for link                */

    int sockfd;                  /* socket descriptor                   */

    LIST_ENTRY(alias_link) list_out; /* Linked list of pointers for     */
    LIST_ENTRY(alias_link) list_in;  /* input and output lookup tables  */

    union                        /* Auxiliary data                      */
    {
        char *frag_ptr;
        struct in_addr frag_addr;
        struct tcp_dat *tcp;
    } data;
};





/* Global Variables

    The global variables listed here are only accessed from
    within alias_db.c and so are prefixed with the static
    designation.
*/

int packetAliasMode;                 /* Mode flags                      */
                                     /*        - documented in alias.h  */

static struct in_addr aliasAddress;  /* Address written onto source     */
                                     /*   field of IP packet.           */

static struct in_addr targetAddress; /* IP address incoming packets     */
                                     /*   are sent to if no aliasing    */
                                     /*   link already exists           */

static struct in_addr nullAddress;   /* Used as a dummy parameter for   */
                                     /*   some function calls           */
static LIST_HEAD(, alias_link)
linkTableOut[LINK_TABLE_OUT_SIZE];   /* Lookup table of pointers to     */
                                     /*   chains of link records. Each  */
static LIST_HEAD(, alias_link)       /*   link record is doubly indexed */
linkTableIn[LINK_TABLE_IN_SIZE];     /*   into input and output lookup  */
                                     /*   tables.                       */

static int icmpLinkCount;            /* Link statistics                 */
static int udpLinkCount;
static int tcpLinkCount;
static int pptpLinkCount;
static int protoLinkCount;
static int fragmentIdLinkCount;
static int fragmentPtrLinkCount;
static int sockCount;

static int cleanupIndex;             /* Index to chain of link table    */
                                     /* being inspected for old links   */

static int timeStamp;                /* System time in seconds for      */
                                     /* current packet                  */

static int lastCleanupTime;          /* Last time IncrementalCleanup()  */
                                     /* was called                      */

static int houseKeepingResidual;     /* used by HouseKeeping()          */

static int deleteAllLinks;           /* If equal to zero, DeleteLink()  */
                                     /* will not remove permanent links */

static FILE *monitorFile;            /* File descriptor for link        */
                                     /* statistics monitoring file      */

static int newDefaultLink;           /* Indicates if a new aliasing     */
                                     /* link has been created after a   */
                                     /* call to PacketAliasIn/Out().    */

#ifndef NO_FW_PUNCH
static int fireWallFD = -1;          /* File descriptor to be able to   */
                                     /* control firewall.  Opened by    */
                                     /* PacketAliasSetMode on first     */
                                     /* setting the PKT_ALIAS_PUNCH_FW  */
                                     /* flag.                           */
#endif







/* Internal utility routines (used only in alias_db.c)

Lookup table starting points:
    StartPointIn()           -- link table initial search point for
                                incoming packets
    StartPointOut()          -- link table initial search point for
                                outgoing packets

Miscellaneous:
    SeqDiff()                -- difference between two TCP sequences
    ShowAliasStats()         -- send alias statistics to a monitor file
*/


/* Local prototypes */
static u_int StartPointIn(struct in_addr, u_short, int);

static u_int StartPointOut(struct in_addr, struct in_addr,
                           u_short, u_short, int);

static int SeqDiff(u_long, u_long);

static void ShowAliasStats(void);

#ifndef NO_FW_PUNCH
/* Firewall control */
static void InitPunchFW(void);
static void UninitPunchFW(void);
static void ClearFWHole(struct alias_link *link);
#endif

/* Log file control */
static void InitPacketAliasLog(void);
static void UninitPacketAliasLog(void);

static u_int
StartPointIn(struct in_addr alias_addr,
             u_short alias_port,
             int link_type)
{
    u_int n;

    n  = alias_addr.s_addr;
    if (link_type != LINK_PPTP)
	n += alias_port;
    n += link_type;
    return(n % LINK_TABLE_IN_SIZE);
}


static u_int
StartPointOut(struct in_addr src_addr, struct in_addr dst_addr,
              u_short src_port, u_short dst_port, int link_type)
{
    u_int n;

    n  = src_addr.s_addr;
    n += dst_addr.s_addr;
    if (link_type != LINK_PPTP) {
	n += src_port;
	n += dst_port;
    }
    n += link_type;

    return(n % LINK_TABLE_OUT_SIZE);
}


static int
SeqDiff(u_long x, u_long y)
{
/* Return the difference between two TCP sequence numbers */

/*
    This function is encapsulated in case there are any unusual
    arithmetic conditions that need to be considered.
*/

    return (ntohl(y) - ntohl(x));
}


static void
ShowAliasStats(void)
{
/* Used for debugging */

   if (monitorFile)
   {
      fprintf(monitorFile, "icmp=%d, udp=%d, tcp=%d, pptp=%d, proto=%d, frag_id=%d frag_ptr=%d",
              icmpLinkCount,
              udpLinkCount,
              tcpLinkCount,
              pptpLinkCount,
              protoLinkCount,
              fragmentIdLinkCount,
              fragmentPtrLinkCount);

      fprintf(monitorFile, " / tot=%d  (sock=%d)\n",
              icmpLinkCount + udpLinkCount
                            + tcpLinkCount
                            + pptpLinkCount
                            + protoLinkCount
                            + fragmentIdLinkCount
                            + fragmentPtrLinkCount,
              sockCount);

      fflush(monitorFile);
   }
}





/* Internal routines for finding, deleting and adding links

Port Allocation:
    GetNewPort()             -- find and reserve new alias port number
    GetSocket()              -- try to allocate a socket for a given port

Link creation and deletion:
    CleanupAliasData()      - remove all link chains from lookup table
    IncrementalCleanup()    - look for stale links in a single chain
    DeleteLink()            - remove link
    AddLink()               - add link
    ReLink()                - change link

Link search:
    FindLinkOut()           - find link for outgoing packets
    FindLinkIn()            - find link for incoming packets

Port search:
    FindNewPortGroup()      - find an available group of ports
*/

/* Local prototypes */
static int GetNewPort(struct alias_link *, int);

static u_short GetSocket(u_short, int *, int);

static void CleanupAliasData(void);

static void IncrementalCleanup(void);

static void DeleteLink(struct alias_link *);

static struct alias_link *
AddLink(struct in_addr, struct in_addr, struct in_addr,
        u_short, u_short, int, int);

static struct alias_link *
ReLink(struct alias_link *,
       struct in_addr, struct in_addr, struct in_addr,
        u_short, u_short, int, int);

static struct alias_link *
FindLinkOut(struct in_addr, struct in_addr, u_short, u_short, int, int);

static struct alias_link *
FindLinkIn(struct in_addr, struct in_addr, u_short, u_short, int, int);


#define ALIAS_PORT_BASE            0x08000
#define ALIAS_PORT_MASK            0x07fff
#define ALIAS_PORT_MASK_EVEN       0x07ffe
#define GET_NEW_PORT_MAX_ATTEMPTS       20

#define GET_ALIAS_PORT                  -1
#define GET_ALIAS_ID        GET_ALIAS_PORT

#define FIND_EVEN_ALIAS_BASE             1

/* GetNewPort() allocates port numbers.  Note that if a port number
   is already in use, that does not mean that it cannot be used by
   another link concurrently.  This is because GetNewPort() looks for
   unused triplets: (dest addr, dest port, alias port). */

static int
GetNewPort(struct alias_link *link, int alias_port_param)
{
    int i;
    int max_trials;
    u_short port_sys;
    u_short port_net;

/*
   Description of alias_port_param for GetNewPort().  When
   this parameter is zero or positive, it precisely specifies
   the port number.  GetNewPort() will return this number
   without check that it is in use.

   When this parameter is GET_ALIAS_PORT, it indicates to get a randomly
   selected port number.
*/

    if (alias_port_param == GET_ALIAS_PORT)
    {
        /*
         * The aliasing port is automatically selected
         * by one of two methods below:
         */
        max_trials = GET_NEW_PORT_MAX_ATTEMPTS;

        if (packetAliasMode & PKT_ALIAS_SAME_PORTS)
        {
            /*
             * When the PKT_ALIAS_SAME_PORTS option is
             * chosen, the first try will be the
             * actual source port. If this is already
             * in use, the remainder of the trials
             * will be random.
             */
            port_net = link->src_port;
            port_sys = ntohs(port_net);
        }
        else
        {
            /* First trial and all subsequent are random. */
            port_sys = random() & ALIAS_PORT_MASK;
            port_sys += ALIAS_PORT_BASE;
            port_net = htons(port_sys);
        }
    }
    else if (alias_port_param >= 0 && alias_port_param < 0x10000)
    {
        link->alias_port = (u_short) alias_port_param;
        return(0);
    }
    else
    {
#ifdef DEBUG
        fprintf(stderr, "PacketAlias/GetNewPort(): ");
        fprintf(stderr, "input parameter error\n");
#endif
        return(-1);
    }


/* Port number search */
    for (i=0; i<max_trials; i++)
    {
        int go_ahead;
        struct alias_link *search_result;

        search_result = FindLinkIn(link->dst_addr, link->alias_addr,
                                   link->dst_port, port_net,
                                   link->link_type, 0);

        if (search_result == NULL)
            go_ahead = 1;
        else if (!(link->flags          & LINK_PARTIALLY_SPECIFIED)
               && (search_result->flags & LINK_PARTIALLY_SPECIFIED))
            go_ahead = 1;
        else
            go_ahead = 0;

        if (go_ahead)
        {
            if ((packetAliasMode & PKT_ALIAS_USE_SOCKETS)
             && (link->flags & LINK_PARTIALLY_SPECIFIED)
	     && ((link->link_type == LINK_TCP) ||
		 (link->link_type == LINK_UDP)))
            {
                if (GetSocket(port_net, &link->sockfd, link->link_type))
                {
                    link->alias_port = port_net;
                    return(0);
                }
            }
            else
            {
                link->alias_port = port_net;
                return(0);
            }
        }

        port_sys = random() & ALIAS_PORT_MASK;
        port_sys += ALIAS_PORT_BASE;
        port_net = htons(port_sys);
    }

#ifdef DEBUG
    fprintf(stderr, "PacketAlias/GetnewPort(): ");
    fprintf(stderr, "could not find free port\n");
#endif

    return(-1);
}


static u_short
GetSocket(u_short port_net, int *sockfd, int link_type)
{
    int err;
    int sock;
    struct sockaddr_in sock_addr;

    if (link_type == LINK_TCP)
        sock = socket(AF_INET, SOCK_STREAM, 0);
    else if (link_type == LINK_UDP)
        sock = socket(AF_INET, SOCK_DGRAM, 0);
    else
    {
#ifdef DEBUG
        fprintf(stderr, "PacketAlias/GetSocket(): ");
        fprintf(stderr, "incorrect link type\n");
#endif
        return(0);
    }

    if (sock < 0)
    {
#ifdef DEBUG
        fprintf(stderr, "PacketAlias/GetSocket(): ");
        fprintf(stderr, "socket() error %d\n", *sockfd);
#endif
        return(0);
    }

    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    sock_addr.sin_port = port_net;

    err = bind(sock,
               (struct sockaddr *) &sock_addr,
               sizeof(sock_addr));
    if (err == 0)
    {
        sockCount++;
        *sockfd = sock;
        return(1);
    }
    else
    {
        close(sock);
        return(0);
    }
}


/* FindNewPortGroup() returns a base port number for an available
   range of contiguous port numbers. Note that if a port number
   is already in use, that does not mean that it cannot be used by
   another link concurrently.  This is because FindNewPortGroup()
   looks for unused triplets: (dest addr, dest port, alias port). */

int
FindNewPortGroup(struct in_addr  dst_addr,
                 struct in_addr  alias_addr,
                 u_short         src_port,
                 u_short         dst_port,
                 u_short         port_count,
		 u_char          proto,
		 u_char          align)
{
    int     i, j;
    int     max_trials;
    u_short port_sys;
    int     link_type;

    /*
     * Get link_type from protocol
     */

    switch (proto)
    {
    case IPPROTO_UDP:
        link_type = LINK_UDP;
        break;
    case IPPROTO_TCP:
        link_type = LINK_TCP;
        break;
    default:
        return (0);
        break;
    }

    /*
     * The aliasing port is automatically selected
     * by one of two methods below:
     */
    max_trials = GET_NEW_PORT_MAX_ATTEMPTS;

    if (packetAliasMode & PKT_ALIAS_SAME_PORTS) {
      /*
       * When the ALIAS_SAME_PORTS option is
       * chosen, the first try will be the
       * actual source port. If this is already
       * in use, the remainder of the trials
       * will be random.
       */
      port_sys = ntohs(src_port);

    } else {

      /* First trial and all subsequent are random. */
      if (align == FIND_EVEN_ALIAS_BASE)
        port_sys = random() & ALIAS_PORT_MASK_EVEN;
      else
        port_sys = random() & ALIAS_PORT_MASK;

      port_sys += ALIAS_PORT_BASE;
    }

/* Port number search */
    for (i = 0; i < max_trials; i++) {

      struct alias_link *search_result;

      for (j = 0; j < port_count; j++)
        if (0 != (search_result = FindLinkIn(dst_addr, alias_addr,
                                        dst_port, htons(port_sys + j),
                                        link_type, 0)))
	  break;

      /* Found a good range, return base */
      if (j == port_count)
	return (htons(port_sys));

      /* Find a new base to try */
      if (align == FIND_EVEN_ALIAS_BASE)
        port_sys = random() & ALIAS_PORT_MASK_EVEN;
      else
        port_sys = random() & ALIAS_PORT_MASK;

      port_sys += ALIAS_PORT_BASE;
    }

#ifdef DEBUG
    fprintf(stderr, "PacketAlias/FindNewPortGroup(): ");
    fprintf(stderr, "could not find free port(s)\n");
#endif

    return(0);
}

static void
CleanupAliasData(void)
{
    struct alias_link *link;
    int i, icount;

    icount = 0;
    for (i=0; i<LINK_TABLE_OUT_SIZE; i++)
    {
        link = LIST_FIRST(&linkTableOut[i]);
        while (link != NULL)
        {
            struct alias_link *link_next;
            link_next = LIST_NEXT(link, list_out);
            icount++;
            DeleteLink(link);
            link = link_next;
        }
    }

    cleanupIndex =0;
}


static void
IncrementalCleanup(void)
{
    int icount;
    struct alias_link *link;

    icount = 0;
    link = LIST_FIRST(&linkTableOut[cleanupIndex++]);
    while (link != NULL)
    {
        int idelta;
        struct alias_link *link_next;

        link_next = LIST_NEXT(link, list_out);
        idelta = timeStamp - link->timestamp;
        switch (link->link_type)
        {
            case LINK_TCP:
                if (idelta > link->expire_time)
                {
                    struct tcp_dat *tcp_aux;

                    tcp_aux = link->data.tcp;
                    if (tcp_aux->state.in  != ALIAS_TCP_STATE_CONNECTED
                     || tcp_aux->state.out != ALIAS_TCP_STATE_CONNECTED)
                    {
                        DeleteLink(link);
                        icount++;
                    }
                }
                break;
            default:
                if (idelta > link->expire_time)
                {
                    DeleteLink(link);
                    icount++;
                }
                break;
        }
        link = link_next;
    }

    if (cleanupIndex == LINK_TABLE_OUT_SIZE)
        cleanupIndex = 0;
}

static void
DeleteLink(struct alias_link *link)
{

/* Don't do anything if the link is marked permanent */
    if (deleteAllLinks == 0 && link->flags & LINK_PERMANENT)
        return;

#ifndef NO_FW_PUNCH
/* Delete associated firewall hole, if any */
    ClearFWHole(link);
#endif

/* Free memory allocated for LSNAT server pool */
    if (link->server != NULL) {
	struct server *head, *curr, *next;

	head = curr = link->server;
	do {
	    next = curr->next;
	    free(curr);
	} while ((curr = next) != head);
    }

/* Adjust output table pointers */
    LIST_REMOVE(link, list_out);

/* Adjust input table pointers */
    LIST_REMOVE(link, list_in);

/* Close socket, if one has been allocated */
    if (link->sockfd != -1)
    {
        sockCount--;
        close(link->sockfd);
    }

/* Link-type dependent cleanup */
    switch(link->link_type)
    {
        case LINK_ICMP:
            icmpLinkCount--;
            break;
        case LINK_UDP:
            udpLinkCount--;
            break;
        case LINK_TCP:
            tcpLinkCount--;
            free(link->data.tcp);
            break;
        case LINK_PPTP:
            pptpLinkCount--;
            break;
        case LINK_FRAGMENT_ID:
            fragmentIdLinkCount--;
            break;
        case LINK_FRAGMENT_PTR:
            fragmentPtrLinkCount--;
            if (link->data.frag_ptr != NULL)
                free(link->data.frag_ptr);
            break;
	case LINK_ADDR:
	    break;
        default:
            protoLinkCount--;
            break;
    }

/* Free memory */
    free(link);

/* Write statistics, if logging enabled */
    if (packetAliasMode & PKT_ALIAS_LOG)
    {
        ShowAliasStats();
    }
}


static struct alias_link *
AddLink(struct in_addr  src_addr,
        struct in_addr  dst_addr,
        struct in_addr  alias_addr,
        u_short         src_port,
        u_short         dst_port,
        int             alias_port_param,  /* if less than zero, alias   */
        int             link_type)         /* port will be automatically */
{                                          /* chosen. If greater than    */
    u_int start_point;                     /* zero, equal to alias port  */
    struct alias_link *link;

    link = malloc(sizeof(struct alias_link));
    if (link != NULL)
    {
    /* Basic initialization */
        link->src_addr          = src_addr;
        link->dst_addr          = dst_addr;
        link->alias_addr        = alias_addr;
        link->proxy_addr.s_addr = INADDR_ANY;
        link->src_port          = src_port;
        link->dst_port          = dst_port;
        link->proxy_port        = 0;
        link->server            = NULL;
        link->link_type         = link_type;
        link->sockfd            = -1;
        link->flags             = 0;
        link->pflags            = 0;
        link->timestamp         = timeStamp;

    /* Expiration time */
        switch (link_type)
        {
        case LINK_ICMP:
            link->expire_time = ICMP_EXPIRE_TIME;
            break;
        case LINK_UDP:
            link->expire_time = UDP_EXPIRE_TIME;
            break;
        case LINK_TCP:
            link->expire_time = TCP_EXPIRE_INITIAL;
            break;
        case LINK_PPTP:
            link->flags |= LINK_PERMANENT;	/* no timeout. */
            break;
        case LINK_FRAGMENT_ID:
            link->expire_time = FRAGMENT_ID_EXPIRE_TIME;
            break;
        case LINK_FRAGMENT_PTR:
            link->expire_time = FRAGMENT_PTR_EXPIRE_TIME;
            break;
	case LINK_ADDR:
	    break;
        default:
            link->expire_time = PROTO_EXPIRE_TIME;
            break;
        }

    /* Determine alias flags */
        if (dst_addr.s_addr == INADDR_ANY)
            link->flags |= LINK_UNKNOWN_DEST_ADDR;
        if (dst_port == 0)
            link->flags |= LINK_UNKNOWN_DEST_PORT;

    /* Determine alias port */
        if (GetNewPort(link, alias_port_param) != 0)
        {
            free(link);
            return(NULL);
        }

    /* Link-type dependent initialization */
        switch(link_type)
        {
            struct tcp_dat  *aux_tcp;

            case LINK_ICMP:
                icmpLinkCount++;
                break;
            case LINK_UDP:
                udpLinkCount++;
                break;
            case LINK_TCP:
                aux_tcp = malloc(sizeof(struct tcp_dat));
                if (aux_tcp != NULL)
                {
                    int i;

                    tcpLinkCount++;
                    aux_tcp->state.in = ALIAS_TCP_STATE_NOT_CONNECTED;
                    aux_tcp->state.out = ALIAS_TCP_STATE_NOT_CONNECTED;
                    aux_tcp->state.index = 0;
                    aux_tcp->state.ack_modified = 0;
                    for (i=0; i<N_LINK_TCP_DATA; i++)
                        aux_tcp->ack[i].active = 0;
                    aux_tcp->fwhole = -1;
                    link->data.tcp = aux_tcp;
                }
                else
                {
#ifdef DEBUG
                    fprintf(stderr, "PacketAlias/AddLink: ");
                    fprintf(stderr, " cannot allocate auxiliary TCP data\n");
#endif
		    free(link);
		    return (NULL);
                }
                break;
            case LINK_PPTP:
                pptpLinkCount++;
                break;
            case LINK_FRAGMENT_ID:
                fragmentIdLinkCount++;
                break;
            case LINK_FRAGMENT_PTR:
                fragmentPtrLinkCount++;
                break;
	    case LINK_ADDR:
		break;
            default:
                protoLinkCount++;
                break;
        }

    /* Set up pointers for output lookup table */
        start_point = StartPointOut(src_addr, dst_addr,
                                    src_port, dst_port, link_type);
        LIST_INSERT_HEAD(&linkTableOut[start_point], link, list_out);

    /* Set up pointers for input lookup table */
        start_point = StartPointIn(alias_addr, link->alias_port, link_type);
        LIST_INSERT_HEAD(&linkTableIn[start_point], link, list_in);
    }
    else
    {
#ifdef DEBUG
        fprintf(stderr, "PacketAlias/AddLink(): ");
        fprintf(stderr, "malloc() call failed.\n");
#endif
    }

    if (packetAliasMode & PKT_ALIAS_LOG)
    {
        ShowAliasStats();
    }

    return(link);
}

static struct alias_link *
ReLink(struct alias_link *old_link,
       struct in_addr  src_addr,
       struct in_addr  dst_addr,
       struct in_addr  alias_addr,
       u_short         src_port,
       u_short         dst_port,
       int             alias_port_param,   /* if less than zero, alias   */
       int             link_type)          /* port will be automatically */
{                                          /* chosen. If greater than    */
    struct alias_link *new_link;           /* zero, equal to alias port  */

    new_link = AddLink(src_addr, dst_addr, alias_addr,
                       src_port, dst_port, alias_port_param,
                       link_type);
#ifndef NO_FW_PUNCH
    if (new_link != NULL &&
        old_link->link_type == LINK_TCP &&
        old_link->data.tcp->fwhole > 0) {
      PunchFWHole(new_link);
    }
#endif
    DeleteLink(old_link);
    return new_link;
}

static struct alias_link *
_FindLinkOut(struct in_addr src_addr,
            struct in_addr dst_addr,
            u_short src_port,
            u_short dst_port,
            int link_type,
            int replace_partial_links)
{
    u_int i;
    struct alias_link *link;

    i = StartPointOut(src_addr, dst_addr, src_port, dst_port, link_type);
    LIST_FOREACH(link, &linkTableOut[i], list_out)
    {
        if (link->src_addr.s_addr == src_addr.s_addr
         && link->server          == NULL
         && link->dst_addr.s_addr == dst_addr.s_addr
         && link->dst_port        == dst_port
         && link->src_port        == src_port
         && link->link_type       == link_type)
        {
            link->timestamp = timeStamp;
            break;
        }
    }

/* Search for partially specified links. */
    if (link == NULL && replace_partial_links)
    {
        if (dst_port != 0 && dst_addr.s_addr != INADDR_ANY)
        {
            link = _FindLinkOut(src_addr, dst_addr, src_port, 0,
                                link_type, 0);
            if (link == NULL)
                link = _FindLinkOut(src_addr, nullAddress, src_port,
                                    dst_port, link_type, 0);
        }
        if (link == NULL &&
           (dst_port != 0 || dst_addr.s_addr != INADDR_ANY))
        {
            link = _FindLinkOut(src_addr, nullAddress, src_port, 0,
                                link_type, 0);
        }
        if (link != NULL)
        {
            link = ReLink(link,
                          src_addr, dst_addr, link->alias_addr,
                          src_port, dst_port, link->alias_port,
                          link_type);
        }
    }

    return(link);
}

static struct alias_link *
FindLinkOut(struct in_addr src_addr,
            struct in_addr dst_addr,
            u_short src_port,
            u_short dst_port,
            int link_type,
            int replace_partial_links)
{
    struct alias_link *link;

    link = _FindLinkOut(src_addr, dst_addr, src_port, dst_port,
                        link_type, replace_partial_links);

    if (link == NULL)
    {
    /* The following allows permanent links to be
       specified as using the default source address
       (i.e. device interface address) without knowing
       in advance what that address is. */
        if (aliasAddress.s_addr != 0 &&
            src_addr.s_addr == aliasAddress.s_addr)
        {
            link = _FindLinkOut(nullAddress, dst_addr, src_port, dst_port,
                               link_type, replace_partial_links);
        }
    }

    return(link);
}


static struct alias_link *
_FindLinkIn(struct in_addr dst_addr,
           struct in_addr  alias_addr,
           u_short         dst_port,
           u_short         alias_port,
           int             link_type,
           int             replace_partial_links)
{
    int flags_in;
    u_int start_point;
    struct alias_link *link;
    struct alias_link *link_fully_specified;
    struct alias_link *link_unknown_all;
    struct alias_link *link_unknown_dst_addr;
    struct alias_link *link_unknown_dst_port;

/* Initialize pointers */
    link_fully_specified  = NULL;
    link_unknown_all      = NULL;
    link_unknown_dst_addr = NULL;
    link_unknown_dst_port = NULL;

/* If either the dest addr or port is unknown, the search
   loop will have to know about this. */

    flags_in = 0;
    if (dst_addr.s_addr == INADDR_ANY)
        flags_in |= LINK_UNKNOWN_DEST_ADDR;
    if (dst_port == 0)
        flags_in |= LINK_UNKNOWN_DEST_PORT;

/* Search loop */
    start_point = StartPointIn(alias_addr, alias_port, link_type);
    LIST_FOREACH(link, &linkTableIn[start_point], list_in)
    {
        int flags;

        flags = flags_in | link->flags;
        if (!(flags & LINK_PARTIALLY_SPECIFIED))
        {
            if (link->alias_addr.s_addr == alias_addr.s_addr
             && link->alias_port        == alias_port
             && link->dst_addr.s_addr   == dst_addr.s_addr
             && link->dst_port          == dst_port
             && link->link_type         == link_type)
            {
                link_fully_specified = link;
                break;
            }
        }
        else if ((flags & LINK_UNKNOWN_DEST_ADDR)
              && (flags & LINK_UNKNOWN_DEST_PORT))
        {
            if (link->alias_addr.s_addr == alias_addr.s_addr
             && link->alias_port        == alias_port
             && link->link_type         == link_type)
            {
                if (link_unknown_all == NULL)
                    link_unknown_all = link;
            }
        }
        else if (flags & LINK_UNKNOWN_DEST_ADDR)
        {
            if (link->alias_addr.s_addr == alias_addr.s_addr
             && link->alias_port        == alias_port
             && link->link_type         == link_type
             && link->dst_port          == dst_port)
            {
                if (link_unknown_dst_addr == NULL)
                    link_unknown_dst_addr = link;
            }
        }
        else if (flags & LINK_UNKNOWN_DEST_PORT)
        {
            if (link->alias_addr.s_addr == alias_addr.s_addr
             && link->alias_port        == alias_port
             && link->link_type         == link_type
             && link->dst_addr.s_addr   == dst_addr.s_addr)
            {
                if (link_unknown_dst_port == NULL)
                    link_unknown_dst_port = link;
            }
        }
    }



    if (link_fully_specified != NULL)
    {
        link_fully_specified->timestamp = timeStamp;
        link = link_fully_specified;
    }
    else if (link_unknown_dst_port != NULL)
	link = link_unknown_dst_port;
    else if (link_unknown_dst_addr != NULL)
	link = link_unknown_dst_addr;
    else if (link_unknown_all != NULL)
	link = link_unknown_all;
    else
        return (NULL);

    if (replace_partial_links &&
	(link->flags & LINK_PARTIALLY_SPECIFIED || link->server != NULL))
    {
	struct in_addr src_addr;
	u_short src_port;

	if (link->server != NULL) {		/* LSNAT link */
	    src_addr = link->server->addr;
	    src_port = link->server->port;
	    link->server = link->server->next;
	} else {
	    src_addr = link->src_addr;
	    src_port = link->src_port;
	}

	link = ReLink(link,
		      src_addr, dst_addr, alias_addr,
		      src_port, dst_port, alias_port,
		      link_type);
    }

    return (link);
}

static struct alias_link *
FindLinkIn(struct in_addr dst_addr,
           struct in_addr alias_addr,
           u_short dst_port,
           u_short alias_port,
           int link_type,
           int replace_partial_links)
{
    struct alias_link *link;

    link = _FindLinkIn(dst_addr, alias_addr, dst_port, alias_port,
                       link_type, replace_partial_links);

    if (link == NULL)
    {
    /* The following allows permanent links to be
       specified as using the default aliasing address
       (i.e. device interface address) without knowing
       in advance what that address is. */
        if (aliasAddress.s_addr != 0 &&
            alias_addr.s_addr == aliasAddress.s_addr)
        {
            link = _FindLinkIn(dst_addr, nullAddress, dst_port, alias_port,
                               link_type, replace_partial_links);
        }
    }

    return(link);
}




/* External routines for finding/adding links

-- "external" means outside alias_db.c, but within alias*.c --

    FindIcmpIn(), FindIcmpOut()
    FindFragmentIn1(), FindFragmentIn2()
    AddFragmentPtrLink(), FindFragmentPtr()
    FindProtoIn(), FindProtoOut()
    FindUdpTcpIn(), FindUdpTcpOut()
    AddPptp(), FindPptpOutByCallId(), FindPptpInByCallId(),
    FindPptpOutByPeerCallId(), FindPptpInByPeerCallId()
    FindOriginalAddress(), FindAliasAddress()

(prototypes in alias_local.h)
*/


struct alias_link *
FindIcmpIn(struct in_addr dst_addr,
           struct in_addr alias_addr,
           u_short id_alias,
           int create)
{
    struct alias_link *link;

    link = FindLinkIn(dst_addr, alias_addr,
                      NO_DEST_PORT, id_alias,
                      LINK_ICMP, 0);
    if (link == NULL && create && !(packetAliasMode & PKT_ALIAS_DENY_INCOMING))
    {
        struct in_addr target_addr;

        target_addr = FindOriginalAddress(alias_addr);
        link = AddLink(target_addr, dst_addr, alias_addr,
                       id_alias, NO_DEST_PORT, id_alias,
                       LINK_ICMP);
    }

    return (link);
}


struct alias_link *
FindIcmpOut(struct in_addr src_addr,
            struct in_addr dst_addr,
            u_short id,
            int create)
{
    struct alias_link * link;

    link = FindLinkOut(src_addr, dst_addr,
                       id, NO_DEST_PORT,
                       LINK_ICMP, 0);
    if (link == NULL && create)
    {
        struct in_addr alias_addr;

        alias_addr = FindAliasAddress(src_addr);
        link = AddLink(src_addr, dst_addr, alias_addr,
                       id, NO_DEST_PORT, GET_ALIAS_ID,
                       LINK_ICMP);
    }

    return(link);
}


struct alias_link *
FindFragmentIn1(struct in_addr dst_addr,
                struct in_addr alias_addr,
                u_short ip_id)
{
    struct alias_link *link;

    link = FindLinkIn(dst_addr, alias_addr,
                      NO_DEST_PORT, ip_id,
                      LINK_FRAGMENT_ID, 0);

    if (link == NULL)
    {
        link = AddLink(nullAddress, dst_addr, alias_addr,
                       NO_SRC_PORT, NO_DEST_PORT, ip_id,
                       LINK_FRAGMENT_ID);
    }

    return(link);
}


struct alias_link *
FindFragmentIn2(struct in_addr dst_addr,   /* Doesn't add a link if one */
                struct in_addr alias_addr, /*   is not found.           */
                u_short ip_id)
{
    return FindLinkIn(dst_addr, alias_addr,
                      NO_DEST_PORT, ip_id,
                      LINK_FRAGMENT_ID, 0);
}


struct alias_link *
AddFragmentPtrLink(struct in_addr dst_addr,
                   u_short ip_id)
{
    return AddLink(nullAddress, dst_addr, nullAddress,
                   NO_SRC_PORT, NO_DEST_PORT, ip_id,
                   LINK_FRAGMENT_PTR);
}


struct alias_link *
FindFragmentPtr(struct in_addr dst_addr,
                u_short ip_id)
{
    return FindLinkIn(dst_addr, nullAddress,
                      NO_DEST_PORT, ip_id,
                      LINK_FRAGMENT_PTR, 0);
}


struct alias_link *
FindProtoIn(struct in_addr dst_addr,
            struct in_addr alias_addr,
	    u_char proto)
{
    struct alias_link *link;

    link = FindLinkIn(dst_addr, alias_addr,
                      NO_DEST_PORT, 0,
                      proto, 1);

    if (link == NULL && !(packetAliasMode & PKT_ALIAS_DENY_INCOMING))
    {
        struct in_addr target_addr;

        target_addr = FindOriginalAddress(alias_addr);
        link = AddLink(target_addr, dst_addr, alias_addr,
                       NO_SRC_PORT, NO_DEST_PORT, 0,
                       proto);
    }

    return (link);
}


struct alias_link *
FindProtoOut(struct in_addr src_addr,
             struct in_addr dst_addr,
             u_char proto)
{
    struct alias_link *link;

    link = FindLinkOut(src_addr, dst_addr,
                       NO_SRC_PORT, NO_DEST_PORT,
                       proto, 1);

    if (link == NULL)
    {
        struct in_addr alias_addr;

        alias_addr = FindAliasAddress(src_addr);
        link = AddLink(src_addr, dst_addr, alias_addr,
                       NO_SRC_PORT, NO_DEST_PORT, 0,
                       proto);
    }

    return (link);
}


struct alias_link *
FindUdpTcpIn(struct in_addr dst_addr,
             struct in_addr alias_addr,
             u_short        dst_port,
             u_short        alias_port,
             u_char         proto,
             int            create)
{
    int link_type;
    struct alias_link *link;

    switch (proto)
    {
    case IPPROTO_UDP:
        link_type = LINK_UDP;
        break;
    case IPPROTO_TCP:
        link_type = LINK_TCP;
        break;
    default:
        return NULL;
        break;
    }

    link = FindLinkIn(dst_addr, alias_addr,
                      dst_port, alias_port,
                      link_type, create);

    if (link == NULL && create && !(packetAliasMode & PKT_ALIAS_DENY_INCOMING))
    {
        struct in_addr target_addr;

        target_addr = FindOriginalAddress(alias_addr);
        link = AddLink(target_addr, dst_addr, alias_addr,
                       alias_port, dst_port, alias_port,
                       link_type);
    }

    return(link);
}


struct alias_link *
FindUdpTcpOut(struct in_addr  src_addr,
              struct in_addr  dst_addr,
              u_short         src_port,
              u_short         dst_port,
              u_char          proto,
              int             create)
{
    int link_type;
    struct alias_link *link;

    switch (proto)
    {
    case IPPROTO_UDP:
        link_type = LINK_UDP;
        break;
    case IPPROTO_TCP:
        link_type = LINK_TCP;
        break;
    default:
        return NULL;
        break;
    }

    link = FindLinkOut(src_addr, dst_addr, src_port, dst_port, link_type, create);

    if (link == NULL && create)
    {
        struct in_addr alias_addr;

        alias_addr = FindAliasAddress(src_addr);
        link = AddLink(src_addr, dst_addr, alias_addr,
                       src_port, dst_port, GET_ALIAS_PORT,
                       link_type);
    }

    return(link);
}


struct alias_link *
AddPptp(struct in_addr  src_addr,
	struct in_addr  dst_addr,
	struct in_addr  alias_addr,
	u_int16_t       src_call_id)
{
    struct alias_link *link;

    link = AddLink(src_addr, dst_addr, alias_addr,
		   src_call_id, 0, GET_ALIAS_PORT,
		   LINK_PPTP);

    return (link);
}


struct alias_link *
FindPptpOutByCallId(struct in_addr src_addr,
		    struct in_addr dst_addr,
		    u_int16_t      src_call_id)
{
    u_int i;
    struct alias_link *link;

    i = StartPointOut(src_addr, dst_addr, 0, 0, LINK_PPTP);
    LIST_FOREACH(link, &linkTableOut[i], list_out)
	if (link->link_type == LINK_PPTP &&
	    link->src_addr.s_addr == src_addr.s_addr &&
	    link->dst_addr.s_addr == dst_addr.s_addr &&
	    link->src_port == src_call_id)
		break;

    return (link);
}


struct alias_link *
FindPptpOutByPeerCallId(struct in_addr src_addr,
			struct in_addr dst_addr,
			u_int16_t      dst_call_id)
{
    u_int i;
    struct alias_link *link;

    i = StartPointOut(src_addr, dst_addr, 0, 0, LINK_PPTP);
    LIST_FOREACH(link, &linkTableOut[i], list_out)
	if (link->link_type == LINK_PPTP &&
	    link->src_addr.s_addr == src_addr.s_addr &&
	    link->dst_addr.s_addr == dst_addr.s_addr &&
	    link->dst_port == dst_call_id)
		break;

    return (link);
}


struct alias_link *
FindPptpInByCallId(struct in_addr dst_addr,
		   struct in_addr alias_addr,
		   u_int16_t      dst_call_id)
{
    u_int i;
    struct alias_link *link;

    i = StartPointIn(alias_addr, 0, LINK_PPTP);
    LIST_FOREACH(link, &linkTableIn[i], list_in)
	if (link->link_type == LINK_PPTP &&
	    link->dst_addr.s_addr == dst_addr.s_addr &&
	    link->alias_addr.s_addr == alias_addr.s_addr &&
	    link->dst_port == dst_call_id)
		break;

    return (link);
}


struct alias_link *
FindPptpInByPeerCallId(struct in_addr dst_addr,
		       struct in_addr alias_addr,
		       u_int16_t      alias_call_id)
{
    struct alias_link *link;

    link = FindLinkIn(dst_addr, alias_addr,
		      0/* any */, alias_call_id,
		      LINK_PPTP, 0);


    return (link);
}


struct alias_link *
FindRtspOut(struct in_addr  src_addr,
            struct in_addr  dst_addr,
            u_short         src_port,
            u_short         alias_port,
            u_char          proto)
{
    int link_type;
    struct alias_link *link;

    switch (proto)
    {
    case IPPROTO_UDP:
        link_type = LINK_UDP;
        break;
    case IPPROTO_TCP:
        link_type = LINK_TCP;
        break;
    default:
        return NULL;
        break;
    }

    link = FindLinkOut(src_addr, dst_addr, src_port, 0, link_type, 1);

    if (link == NULL)
    {
        struct in_addr alias_addr;

        alias_addr = FindAliasAddress(src_addr);
        link = AddLink(src_addr, dst_addr, alias_addr,
                       src_port, 0, alias_port,
                       link_type);
    }

    return(link);
}


struct in_addr
FindOriginalAddress(struct in_addr alias_addr)
{
    struct alias_link *link;

    link = FindLinkIn(nullAddress, alias_addr,
                      0, 0, LINK_ADDR, 0);
    if (link == NULL)
    {
        newDefaultLink = 1;
        if (targetAddress.s_addr == INADDR_ANY)
            return alias_addr;
        else if (targetAddress.s_addr == INADDR_NONE)
            return aliasAddress;
        else
            return targetAddress;
    }
    else
    {
	if (link->server != NULL) {		/* LSNAT link */
	    struct in_addr src_addr;

	    src_addr = link->server->addr;
	    link->server = link->server->next;
	    return (src_addr);
        } else if (link->src_addr.s_addr == INADDR_ANY)
            return aliasAddress;
        else
            return link->src_addr;
    }
}


struct in_addr
FindAliasAddress(struct in_addr original_addr)
{
    struct alias_link *link;

    link = FindLinkOut(original_addr, nullAddress,
                       0, 0, LINK_ADDR, 0);
    if (link == NULL)
    {
        return aliasAddress;
    }
    else
    {
        if (link->alias_addr.s_addr == INADDR_ANY)
            return aliasAddress;
        else
            return link->alias_addr;
    }
}


/* External routines for getting or changing link data
   (external to alias_db.c, but internal to alias*.c)

    SetFragmentData(), GetFragmentData()
    SetFragmentPtr(), GetFragmentPtr()
    SetStateIn(), SetStateOut(), GetStateIn(), GetStateOut()
    GetOriginalAddress(), GetDestAddress(), GetAliasAddress()
    GetOriginalPort(), GetAliasPort()
    SetAckModified(), GetAckModified()
    GetDeltaAckIn(), GetDeltaSeqOut(), AddSeq()
    SetProtocolFlags(), GetProtocolFlags()
    SetDestCallId()
*/


void
SetFragmentAddr(struct alias_link *link, struct in_addr src_addr)
{
    link->data.frag_addr = src_addr;
}


void
GetFragmentAddr(struct alias_link *link, struct in_addr *src_addr)
{
    *src_addr = link->data.frag_addr;
}


void
SetFragmentPtr(struct alias_link *link, char *fptr)
{
    link->data.frag_ptr = fptr;
}


void
GetFragmentPtr(struct alias_link *link, char **fptr)
{
   *fptr = link->data.frag_ptr;
}


void
SetStateIn(struct alias_link *link, int state)
{
    /* TCP input state */
    switch (state) {
    case ALIAS_TCP_STATE_DISCONNECTED:
        if (link->data.tcp->state.out != ALIAS_TCP_STATE_CONNECTED)
            link->expire_time = TCP_EXPIRE_DEAD;
        else
            link->expire_time = TCP_EXPIRE_SINGLEDEAD;
        break;
    case ALIAS_TCP_STATE_CONNECTED:
        if (link->data.tcp->state.out == ALIAS_TCP_STATE_CONNECTED)
            link->expire_time = TCP_EXPIRE_CONNECTED;
        break;
    default:
        abort();
    }
    link->data.tcp->state.in = state;
}


void
SetStateOut(struct alias_link *link, int state)
{
    /* TCP output state */
    switch (state) {
    case ALIAS_TCP_STATE_DISCONNECTED:
        if (link->data.tcp->state.in != ALIAS_TCP_STATE_CONNECTED)
            link->expire_time = TCP_EXPIRE_DEAD;
        else
            link->expire_time = TCP_EXPIRE_SINGLEDEAD;
        break;
    case ALIAS_TCP_STATE_CONNECTED:
        if (link->data.tcp->state.in == ALIAS_TCP_STATE_CONNECTED)
            link->expire_time = TCP_EXPIRE_CONNECTED;
        break;
    default:
        abort();
    }
    link->data.tcp->state.out = state;
}


int
GetStateIn(struct alias_link *link)
{
    /* TCP input state */
    return link->data.tcp->state.in;
}


int
GetStateOut(struct alias_link *link)
{
    /* TCP output state */
    return link->data.tcp->state.out;
}


struct in_addr
GetOriginalAddress(struct alias_link *link)
{
    if (link->src_addr.s_addr == INADDR_ANY)
        return aliasAddress;
    else
        return(link->src_addr);
}


struct in_addr
GetDestAddress(struct alias_link *link)
{
    return(link->dst_addr);
}


struct in_addr
GetAliasAddress(struct alias_link *link)
{
    if (link->alias_addr.s_addr == INADDR_ANY)
        return aliasAddress;
    else
        return link->alias_addr;
}


struct in_addr
GetDefaultAliasAddress()
{
    return aliasAddress;
}


void
SetDefaultAliasAddress(struct in_addr alias_addr)
{
    aliasAddress = alias_addr;
}


u_short
GetOriginalPort(struct alias_link *link)
{
    return(link->src_port);
}


u_short
GetAliasPort(struct alias_link *link)
{
    return(link->alias_port);
}

#ifndef NO_FW_PUNCH
static u_short
GetDestPort(struct alias_link *link)
{
    return(link->dst_port);
}
#endif

void
SetAckModified(struct alias_link *link)
{
/* Indicate that ACK numbers have been modified in a TCP connection */
    link->data.tcp->state.ack_modified = 1;
}


struct in_addr
GetProxyAddress(struct alias_link *link)
{
    return link->proxy_addr;
}


void
SetProxyAddress(struct alias_link *link, struct in_addr addr)
{
    link->proxy_addr = addr;
}


u_short
GetProxyPort(struct alias_link *link)
{
    return link->proxy_port;
}


void
SetProxyPort(struct alias_link *link, u_short port)
{
    link->proxy_port = port;
}


int
GetAckModified(struct alias_link *link)
{
/* See if ACK numbers have been modified */
    return link->data.tcp->state.ack_modified;
}


int
GetDeltaAckIn(struct ip *pip, struct alias_link *link)
{
/*
Find out how much the ACK number has been altered for an incoming
TCP packet.  To do this, a circular list of ACK numbers where the TCP
packet size was altered is searched.
*/

    int i;
    struct tcphdr *tc;
    int delta, ack_diff_min;
    u_long ack;

    tc = (struct tcphdr *) ((char *) pip + (pip->ip_hl << 2));
    ack      = tc->th_ack;

    delta = 0;
    ack_diff_min = -1;
    for (i=0; i<N_LINK_TCP_DATA; i++)
    {
        struct ack_data_record x;

        x = link->data.tcp->ack[i];
        if (x.active == 1)
        {
            int ack_diff;

            ack_diff = SeqDiff(x.ack_new, ack);
            if (ack_diff >= 0)
            {
                if (ack_diff_min >= 0)
                {
                    if (ack_diff < ack_diff_min)
                    {
                        delta = x.delta;
                        ack_diff_min = ack_diff;
                    }
                }
                else
                {
                    delta = x.delta;
                    ack_diff_min = ack_diff;
                }
            }
        }
    }
    return (delta);
}


int
GetDeltaSeqOut(struct ip *pip, struct alias_link *link)
{
/*
Find out how much the sequence number has been altered for an outgoing
TCP packet.  To do this, a circular list of ACK numbers where the TCP
packet size was altered is searched.
*/

    int i;
    struct tcphdr *tc;
    int delta, seq_diff_min;
    u_long seq;

    tc = (struct tcphdr *) ((char *) pip + (pip->ip_hl << 2));
    seq = tc->th_seq;

    delta = 0;
    seq_diff_min = -1;
    for (i=0; i<N_LINK_TCP_DATA; i++)
    {
        struct ack_data_record x;

        x = link->data.tcp->ack[i];
        if (x.active == 1)
        {
            int seq_diff;

            seq_diff = SeqDiff(x.ack_old, seq);
            if (seq_diff >= 0)
            {
                if (seq_diff_min >= 0)
                {
                    if (seq_diff < seq_diff_min)
                    {
                        delta = x.delta;
                        seq_diff_min = seq_diff;
                    }
                }
                else
                {
                    delta = x.delta;
                    seq_diff_min = seq_diff;
                }
            }
        }
    }
    return (delta);
}


void
AddSeq(struct ip *pip, struct alias_link *link, int delta)
{
/*
When a TCP packet has been altered in length, save this
information in a circular list.  If enough packets have
been altered, then this list will begin to overwrite itself.
*/

    struct tcphdr *tc;
    struct ack_data_record x;
    int hlen, tlen, dlen;
    int i;

    tc = (struct tcphdr *) ((char *) pip + (pip->ip_hl << 2));

    hlen = (pip->ip_hl + tc->th_off) << 2;
    tlen = ntohs(pip->ip_len);
    dlen = tlen - hlen;

    x.ack_old = htonl(ntohl(tc->th_seq) + dlen);
    x.ack_new = htonl(ntohl(tc->th_seq) + dlen + delta);
    x.delta = delta;
    x.active = 1;

    i = link->data.tcp->state.index;
    link->data.tcp->ack[i] = x;

    i++;
    if (i == N_LINK_TCP_DATA)
        link->data.tcp->state.index = 0;
    else
        link->data.tcp->state.index = i;
}

void
SetExpire(struct alias_link *link, int expire)
{
    if (expire == 0)
    {
        link->flags &= ~LINK_PERMANENT;
        DeleteLink(link);
    }
    else if (expire == -1)
    {
        link->flags |= LINK_PERMANENT;
    }
    else if (expire > 0)
    {
        link->expire_time = expire;
    }
    else
    {
#ifdef DEBUG
        fprintf(stderr, "PacketAlias/SetExpire(): ");
        fprintf(stderr, "error in expire parameter\n");
#endif
    }
}

void
ClearCheckNewLink(void)
{
    newDefaultLink = 0;
}

void
SetProtocolFlags(struct alias_link *link, int pflags)
{

    link->pflags = pflags;;
}

int
GetProtocolFlags(struct alias_link *link)
{

    return (link->pflags);
}

void
SetDestCallId(struct alias_link *link, u_int16_t cid)
{

    deleteAllLinks = 1;
    link = ReLink(link, link->src_addr, link->dst_addr, link->alias_addr,
		  link->src_port, cid, link->alias_port, link->link_type);
    deleteAllLinks = 0;
}


/* Miscellaneous Functions

    HouseKeeping()
    InitPacketAliasLog()
    UninitPacketAliasLog()
*/

/*
    Whenever an outgoing or incoming packet is handled, HouseKeeping()
    is called to find and remove timed-out aliasing links.  Logic exists
    to sweep through the entire table and linked list structure
    every 60 seconds.

    (prototype in alias_local.h)
*/

void
HouseKeeping(void)
{
    int i, n, n100;
    struct timeval tv;
    struct timezone tz;

    /*
     * Save system time (seconds) in global variable timeStamp for
     * use by other functions. This is done so as not to unnecessarily
     * waste timeline by making system calls.
     */
    gettimeofday(&tv, &tz);
    timeStamp = tv.tv_sec;

    /* Compute number of spokes (output table link chains) to cover */
    n100  = LINK_TABLE_OUT_SIZE * 100 + houseKeepingResidual;
    n100 *= timeStamp - lastCleanupTime;
    n100 /= ALIAS_CLEANUP_INTERVAL_SECS;

    n = n100/100;

    /* Handle different cases */
    if (n > ALIAS_CLEANUP_MAX_SPOKES)
    {
        n = ALIAS_CLEANUP_MAX_SPOKES;
        lastCleanupTime = timeStamp;
        houseKeepingResidual = 0;

        for (i=0; i<n; i++)
            IncrementalCleanup();
    }
    else if (n > 0)
    {
        lastCleanupTime = timeStamp;
        houseKeepingResidual = n100 - 100*n;

        for (i=0; i<n; i++)
            IncrementalCleanup();
    }
    else if (n < 0)
    {
#ifdef DEBUG
        fprintf(stderr, "PacketAlias/HouseKeeping(): ");
        fprintf(stderr, "something unexpected in time values\n");
#endif
        lastCleanupTime = timeStamp;
        houseKeepingResidual = 0;
    }
}


/* Init the log file and enable logging */
static void
InitPacketAliasLog(void)
{
   if ((~packetAliasMode & PKT_ALIAS_LOG)
    && (monitorFile = fopen("/var/log/alias.log", "w")))
   {
      packetAliasMode |= PKT_ALIAS_LOG;
      fprintf(monitorFile,
      "PacketAlias/InitPacketAliasLog: Packet alias logging enabled.\n");
   }
}


/* Close the log-file and disable logging. */
static void
UninitPacketAliasLog(void)
{
    if (monitorFile) {
        fclose(monitorFile);
        monitorFile = NULL;
    }
    packetAliasMode &= ~PKT_ALIAS_LOG;
}






/* Outside world interfaces

-- "outside world" means other than alias*.c routines --

    PacketAliasRedirectPort()
    PacketAliasAddServer()
    PacketAliasRedirectProto()
    PacketAliasRedirectAddr()
    PacketAliasRedirectDelete()
    PacketAliasSetAddress()
    PacketAliasInit()
    PacketAliasUninit()
    PacketAliasSetMode()

(prototypes in alias.h)
*/

/* Redirection from a specific public addr:port to a
   private addr:port */
struct alias_link *
PacketAliasRedirectPort(struct in_addr src_addr,   u_short src_port,
                        struct in_addr dst_addr,   u_short dst_port,
                        struct in_addr alias_addr, u_short alias_port,
                        u_char proto)
{
    int link_type;
    struct alias_link *link;

    switch(proto)
    {
    case IPPROTO_UDP:
        link_type = LINK_UDP;
        break;
    case IPPROTO_TCP:
        link_type = LINK_TCP;
        break;
    default:
#ifdef DEBUG
        fprintf(stderr, "PacketAliasRedirectPort(): ");
        fprintf(stderr, "only TCP and UDP protocols allowed\n");
#endif
        return NULL;
    }

    link = AddLink(src_addr, dst_addr, alias_addr,
                   src_port, dst_port, alias_port,
                   link_type);

    if (link != NULL)
    {
        link->flags |= LINK_PERMANENT;
    }
#ifdef DEBUG
    else
    {
        fprintf(stderr, "PacketAliasRedirectPort(): "
                        "call to AddLink() failed\n");
    }
#endif

    return link;
}

/* Add server to the pool of servers */
int
PacketAliasAddServer(struct alias_link *link, struct in_addr addr, u_short port)
{
    struct server *server;

    server = malloc(sizeof(struct server));

    if (server != NULL) {
	struct server *head;

	server->addr = addr;
	server->port = port;

	head = link->server;
	if (head == NULL)
	    server->next = server;
	else {
	    struct server *s;

	    for (s = head; s->next != head; s = s->next);
	    s->next = server;
	    server->next = head;
	}
	link->server = server;
	return (0);
    } else
	return (-1);
}

/* Redirect packets of a given IP protocol from a specific
   public address to a private address */
struct alias_link *
PacketAliasRedirectProto(struct in_addr src_addr,
                         struct in_addr dst_addr,
                         struct in_addr alias_addr,
                         u_char proto)
{
    struct alias_link *link;

    link = AddLink(src_addr, dst_addr, alias_addr,
                   NO_SRC_PORT, NO_DEST_PORT, 0,
                   proto);

    if (link != NULL)
    {
        link->flags |= LINK_PERMANENT;
    }
#ifdef DEBUG
    else
    {
        fprintf(stderr, "PacketAliasRedirectProto(): "
                        "call to AddLink() failed\n");
    }
#endif

    return link;
}

/* Static address translation */
struct alias_link *
PacketAliasRedirectAddr(struct in_addr src_addr,
                        struct in_addr alias_addr)
{
    struct alias_link *link;

    link = AddLink(src_addr, nullAddress, alias_addr,
                   0, 0, 0,
                   LINK_ADDR);

    if (link != NULL)
    {
        link->flags |= LINK_PERMANENT;
    }
#ifdef DEBUG
    else
    {
        fprintf(stderr, "PacketAliasRedirectAddr(): "
                        "call to AddLink() failed\n");
    }
#endif

    return link;
}


void
PacketAliasRedirectDelete(struct alias_link *link)
{
/* This is a dangerous function to put in the API,
   because an invalid pointer can crash the program. */

    deleteAllLinks = 1;
    DeleteLink(link);
    deleteAllLinks = 0;
}


void
PacketAliasSetAddress(struct in_addr addr)
{
    if (packetAliasMode & PKT_ALIAS_RESET_ON_ADDR_CHANGE
     && aliasAddress.s_addr != addr.s_addr)
        CleanupAliasData();

    aliasAddress = addr;
}


void
PacketAliasSetTarget(struct in_addr target_addr)
{
    targetAddress = target_addr;
}


void
PacketAliasInit(void)
{
    int i;
    struct timeval tv;
    struct timezone tz;
    static int firstCall = 1;

    if (firstCall == 1)
    {
        gettimeofday(&tv, &tz);
        timeStamp = tv.tv_sec;
        lastCleanupTime = tv.tv_sec;
        houseKeepingResidual = 0;

        for (i=0; i<LINK_TABLE_OUT_SIZE; i++)
            LIST_INIT(&linkTableOut[i]);
        for (i=0; i<LINK_TABLE_IN_SIZE; i++)
            LIST_INIT(&linkTableIn[i]);

        atexit(PacketAliasUninit);
        firstCall = 0;
    }
    else
    {
        deleteAllLinks = 1;
        CleanupAliasData();
        deleteAllLinks = 0;
    }

    aliasAddress.s_addr = INADDR_ANY;
    targetAddress.s_addr = INADDR_ANY;

    icmpLinkCount = 0;
    udpLinkCount = 0;
    tcpLinkCount = 0;
    pptpLinkCount = 0;
    protoLinkCount = 0;
    fragmentIdLinkCount = 0;
    fragmentPtrLinkCount = 0;
    sockCount = 0;

    cleanupIndex =0;

    packetAliasMode = PKT_ALIAS_SAME_PORTS
                    | PKT_ALIAS_USE_SOCKETS
                    | PKT_ALIAS_RESET_ON_ADDR_CHANGE;
}

void
PacketAliasUninit(void) {
    deleteAllLinks = 1;
    CleanupAliasData();
    deleteAllLinks = 0;
    UninitPacketAliasLog();
#ifndef NO_FW_PUNCH
    UninitPunchFW();
#endif
}


/* Change mode for some operations */
unsigned int
PacketAliasSetMode(
    unsigned int flags, /* Which state to bring flags to */
    unsigned int mask   /* Mask of which flags to affect (use 0 to do a
                           probe for flag values) */
)
{
/* Enable logging? */
    if (flags & mask & PKT_ALIAS_LOG)
    {
        InitPacketAliasLog();     /* Do the enable */
    } else
/* _Disable_ logging? */
    if (~flags & mask & PKT_ALIAS_LOG) {
        UninitPacketAliasLog();
    }

#ifndef NO_FW_PUNCH
/* Start punching holes in the firewall? */
    if (flags & mask & PKT_ALIAS_PUNCH_FW) {
        InitPunchFW();
    } else
/* Stop punching holes in the firewall? */
    if (~flags & mask & PKT_ALIAS_PUNCH_FW) {
        UninitPunchFW();
    }
#endif

/* Other flags can be set/cleared without special action */
    packetAliasMode = (flags & mask) | (packetAliasMode & ~mask);
    return packetAliasMode;
}


int
PacketAliasCheckNewLink(void)
{
    return newDefaultLink;
}


#ifndef NO_FW_PUNCH

/*****************
  Code to support firewall punching.  This shouldn't really be in this
  file, but making variables global is evil too.
  ****************/

#ifndef IPFW2
#define IPFW2	1	/* use new ipfw code */
#endif

/* Firewall include files */
#include <net/if.h>
#include <netinet/ip_fw.h>
#include <string.h>
#include <err.h>

#if IPFW2		/* support for new firewall code */
/*
 * helper function, updates the pointer to cmd with the length
 * of the current command, and also cleans up the first word of
 * the new command in case it has been clobbered before.
 */
static ipfw_insn *
next_cmd(ipfw_insn *cmd)
{
    cmd += F_LEN(cmd);
    bzero(cmd, sizeof(*cmd));
    return cmd;
}

/*
 * A function to fill simple commands of size 1.
 * Existing flags are preserved.
 */
static ipfw_insn *
fill_cmd(ipfw_insn *cmd, enum ipfw_opcodes opcode, int size,
	 int flags, u_int16_t arg)
{
    cmd->opcode = opcode;
    cmd->len =  ((cmd->len | flags) & (F_NOT | F_OR)) | (size & F_LEN_MASK);
    cmd->arg1 = arg;
    return next_cmd(cmd);
}

static ipfw_insn *
fill_ip(ipfw_insn *cmd1, enum ipfw_opcodes opcode, u_int32_t addr)
{
    ipfw_insn_ip *cmd = (ipfw_insn_ip *)cmd1;

    cmd->addr.s_addr = addr;
    return fill_cmd(cmd1, opcode, F_INSN_SIZE(ipfw_insn_u32), 0, 0);
}

static ipfw_insn *
fill_one_port(ipfw_insn *cmd1, enum ipfw_opcodes opcode, u_int16_t port)
{
    ipfw_insn_u16 *cmd = (ipfw_insn_u16 *)cmd1;

    cmd->ports[0] = cmd->ports[1] = port;
    return fill_cmd(cmd1, opcode, F_INSN_SIZE(ipfw_insn_u16), 0, 0);
}

static int
fill_rule(void *buf, int bufsize, int rulenum,
	enum ipfw_opcodes action, int proto,
	struct in_addr sa, u_int16_t sp, struct in_addr da, u_int16_t dp)
{
    struct ip_fw *rule = (struct ip_fw *)buf;
    ipfw_insn *cmd = (ipfw_insn *)rule->cmd;

    bzero(buf, bufsize);
    rule->rulenum = rulenum;

    cmd = fill_cmd(cmd, O_PROTO, F_INSN_SIZE(ipfw_insn), 0, proto);
    cmd = fill_ip(cmd, O_IP_SRC, sa.s_addr);
    cmd = fill_one_port(cmd, O_IP_SRCPORT, sp);
    cmd = fill_ip(cmd, O_IP_DST, da.s_addr);
    cmd = fill_one_port(cmd, O_IP_DSTPORT, dp);

    rule->act_ofs = (u_int32_t *)cmd - (u_int32_t *)rule->cmd;
    cmd = fill_cmd(cmd, action, F_INSN_SIZE(ipfw_insn), 0, 0);

    rule->cmd_len = (u_int32_t *)cmd - (u_int32_t *)rule->cmd;

    return ((void *)cmd - buf);
}
#endif /* IPFW2 */

static void ClearAllFWHoles(void);

static int fireWallBaseNum;     /* The first firewall entry free for our use */
static int fireWallNumNums;     /* How many entries can we use? */
static int fireWallActiveNum;   /* Which entry did we last use? */
static char *fireWallField;     /* bool array for entries */

#define fw_setfield(field, num)                         \
do {                                                    \
    (field)[(num) - fireWallBaseNum] = 1;               \
} /*lint -save -e717 */ while(0) /*lint -restore */
#define fw_clrfield(field, num)                         \
do {                                                    \
    (field)[(num) - fireWallBaseNum] = 0;               \
} /*lint -save -e717 */ while(0) /*lint -restore */
#define fw_tstfield(field, num) ((field)[(num) - fireWallBaseNum])

static void
InitPunchFW(void) {
    fireWallField = malloc(fireWallNumNums);
    if (fireWallField) {
        memset(fireWallField, 0, fireWallNumNums);
        if (fireWallFD < 0) {
            fireWallFD = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
        }
        ClearAllFWHoles();
        fireWallActiveNum = fireWallBaseNum;
    }
}

static void
UninitPunchFW(void) {
    ClearAllFWHoles();
    if (fireWallFD >= 0)
        close(fireWallFD);
    fireWallFD = -1;
    if (fireWallField)
        free(fireWallField);
    fireWallField = NULL;
    packetAliasMode &= ~PKT_ALIAS_PUNCH_FW;
}

/* Make a certain link go through the firewall */
void
PunchFWHole(struct alias_link *link) {
    int r;                      /* Result code */
    struct ip_fw rule;          /* On-the-fly built rule */
    int fwhole;                 /* Where to punch hole */

/* Don't do anything unless we are asked to */
    if ( !(packetAliasMode & PKT_ALIAS_PUNCH_FW) ||
         fireWallFD < 0 ||
         link->link_type != LINK_TCP)
        return;

    memset(&rule, 0, sizeof rule);

/** Build rule **/

    /* Find empty slot */
    for (fwhole = fireWallActiveNum;
         fwhole < fireWallBaseNum + fireWallNumNums &&
             fw_tstfield(fireWallField, fwhole);
         fwhole++)
        ;
    if (fwhole == fireWallBaseNum + fireWallNumNums) {
        for (fwhole = fireWallBaseNum;
             fwhole < fireWallActiveNum &&
                 fw_tstfield(fireWallField, fwhole);
             fwhole++)
            ;
        if (fwhole == fireWallActiveNum) {
            /* No rule point empty - we can't punch more holes. */
            fireWallActiveNum = fireWallBaseNum;
#ifdef DEBUG
            fprintf(stderr, "libalias: Unable to create firewall hole!\n");
#endif
            return;
        }
    }
    /* Start next search at next position */
    fireWallActiveNum = fwhole+1;

    /*
     * generate two rules of the form
     *
     *	add fwhole accept tcp from OAddr OPort to DAddr DPort
     *	add fwhole accept tcp from DAddr DPort to OAddr OPort
     */
#if IPFW2
    if (GetOriginalPort(link) != 0 && GetDestPort(link) != 0) {
	u_int32_t rulebuf[255];
	int i;

	i = fill_rule(rulebuf, sizeof(rulebuf), fwhole,
		O_ACCEPT, IPPROTO_TCP,
		GetOriginalAddress(link), ntohs(GetOriginalPort(link)),
		GetDestAddress(link), ntohs(GetDestPort(link)) );
	r = setsockopt(fireWallFD, IPPROTO_IP, IP_FW_ADD, rulebuf, i);
	if (r)
		err(1, "alias punch inbound(1) setsockopt(IP_FW_ADD)");

	i = fill_rule(rulebuf, sizeof(rulebuf), fwhole,
		O_ACCEPT, IPPROTO_TCP,
		GetDestAddress(link), ntohs(GetDestPort(link)),
		GetOriginalAddress(link), ntohs(GetOriginalPort(link)) );
	r = setsockopt(fireWallFD, IPPROTO_IP, IP_FW_ADD, rulebuf, i);
	if (r)
		err(1, "alias punch inbound(2) setsockopt(IP_FW_ADD)");
    }
#else	/* !IPFW2, old code to generate ipfw rule */

    /* Build generic part of the two rules */
    rule.fw_number = fwhole;
    IP_FW_SETNSRCP(&rule, 1);	/* Number of source ports. */
    IP_FW_SETNDSTP(&rule, 1);	/* Number of destination ports. */
    rule.fw_flg = IP_FW_F_ACCEPT | IP_FW_F_IN | IP_FW_F_OUT;
    rule.fw_prot = IPPROTO_TCP;
    rule.fw_smsk.s_addr = INADDR_BROADCAST;
    rule.fw_dmsk.s_addr = INADDR_BROADCAST;

    /* Build and apply specific part of the rules */
    rule.fw_src = GetOriginalAddress(link);
    rule.fw_dst = GetDestAddress(link);
    rule.fw_uar.fw_pts[0] = ntohs(GetOriginalPort(link));
    rule.fw_uar.fw_pts[1] = ntohs(GetDestPort(link));

    /* Skip non-bound links - XXX should not be strictly necessary,
       but seems to leave hole if not done.  Leak of non-bound links?
       (Code should be left even if the problem is fixed - it is a
       clear optimization) */
    if (rule.fw_uar.fw_pts[0] != 0 && rule.fw_uar.fw_pts[1] != 0) {
        r = setsockopt(fireWallFD, IPPROTO_IP, IP_FW_ADD, &rule, sizeof rule);
#ifdef DEBUG
        if (r)
            err(1, "alias punch inbound(1) setsockopt(IP_FW_ADD)");
#endif
        rule.fw_src = GetDestAddress(link);
        rule.fw_dst = GetOriginalAddress(link);
        rule.fw_uar.fw_pts[0] = ntohs(GetDestPort(link));
        rule.fw_uar.fw_pts[1] = ntohs(GetOriginalPort(link));
        r = setsockopt(fireWallFD, IPPROTO_IP, IP_FW_ADD, &rule, sizeof rule);
#ifdef DEBUG
        if (r)
            err(1, "alias punch inbound(2) setsockopt(IP_FW_ADD)");
#endif
    }
#endif /* !IPFW2 */
/* Indicate hole applied */
    link->data.tcp->fwhole = fwhole;
    fw_setfield(fireWallField, fwhole);
}

/* Remove a hole in a firewall associated with a particular alias
   link.  Calling this too often is harmless. */
static void
ClearFWHole(struct alias_link *link) {
    if (link->link_type == LINK_TCP) {
        int fwhole =  link->data.tcp->fwhole; /* Where is the firewall hole? */
        struct ip_fw rule;

	if (fwhole < 0)
	    return;

        memset(&rule, 0, sizeof rule); /* useless for ipfw2 */
#if IPFW2
	while (!setsockopt(fireWallFD, IPPROTO_IP, IP_FW_DEL,
		    &fwhole, sizeof fwhole))
	    ;
#else /* !IPFW2 */
        rule.fw_number = fwhole;
        while (!setsockopt(fireWallFD, IPPROTO_IP, IP_FW_DEL,
		    &rule, sizeof rule))
            ;
#endif /* !IPFW2 */
        fw_clrfield(fireWallField, fwhole);
        link->data.tcp->fwhole = -1;
    }
}

/* Clear out the entire range dedicated to firewall holes. */
static void
ClearAllFWHoles(void) {
    struct ip_fw rule;          /* On-the-fly built rule */
    int i;

    if (fireWallFD < 0)
        return;

    memset(&rule, 0, sizeof rule);
    for (i = fireWallBaseNum; i < fireWallBaseNum + fireWallNumNums; i++) {
#if IPFW2
	int r = i;
	while (!setsockopt(fireWallFD, IPPROTO_IP, IP_FW_DEL, &r, sizeof r))
	    ;
#else /* !IPFW2 */
        rule.fw_number = i;
        while (!setsockopt(fireWallFD, IPPROTO_IP, IP_FW_DEL, &rule, sizeof rule))
            ;
#endif /* !IPFW2 */
    }
    memset(fireWallField, 0, fireWallNumNums);
}
#endif

void
PacketAliasSetFWBase(unsigned int base, unsigned int num) {
#ifndef NO_FW_PUNCH
    fireWallBaseNum = base;
    fireWallNumNums = num;
#endif
}
