/*  -*- mode: c; tab-width: 8; c-basic-indent: 4; -*-
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


    This software is placed into the public domain with no restrictions
    on its distribution.

    Initial version: August, 1996  (cjm)

    Version 1.4: September 16, 1996 (cjm)
        Facility for handling incoming links added.

    Version 1.6: September 18, 1996 (cjm)
        ICMP data handling simplified.

    Version 1.7: January 9, 1997 (cjm)
        Fragment handling simplified.
        Saves pointers for unresolved fragments.
        Permits links for unspecied remote ports
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

        All options earlier being #ifdef'ed now are available through
        a new interface, SetPacketAliasMode().  This allow run time
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

        These permament links allow for incoming connections to
        machines on the local network.  They can be given with a
        user-chosen amount of specificity, with increasing specificity
        meaning more security. (cjm)

        Quite a bit of rework to the basic engine.  The portTable[]
        array, which kept track of which ports were in use was replaced
        by a table/linked list structure. (cjm)

        SetExpire() function added. (cjm)

        DeleteLink() no longer frees memory association with a pointer
        to a fragment (this bug was first recognized by E. Eiklund in
        v1.9).

    Version 2.1: May, 1997 (cjm)
        Packet aliasing engine reworked so that it can handle
        multiple external addresses rather than just a single
        host address.

        PacketAliasRedirectPort() and PacketAliasRedirectAddr()
        added to the API.  The first function is a more generalized
        version of PacketAliasPermanentLink().  The second function
        implements static network address translation.
*/


/* System include files */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
 
#include <sys/errno.h>
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

/* Timouts (in seconds) for different link types) */
#define ICMP_EXPIRE_TIME             60
#define UDP_EXPIRE_TIME              60
#define TCP_EXPIRE_TIME              90
#define FRAGMENT_ID_EXPIRE_TIME      10
#define FRAGMENT_PTR_EXPIRE_TIME     30

/* Dummy port number codes used for FindLinkIn/Out() and AddLink().
   These constants can be anything except zero, which indicates an
   unknown port numbea. */

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
    with the 16-bit id number of the ICMP packet.

    The link record also can store some auxiliary data.  For
    TCP connections that have had sequence and acknowledgment
    modifications, data space is available to track these changes.
    A state field is used to keep track in changes to the tcp
    connection state.  Id numbers of fragments can also be
    stored in the auxiliary space.  Pointers to unresolved
    framgents can also be stored.

    The link records support two independent chainings.  Lookup
    tables for input and out tables hold the initial pointers
    the link chains.  On input, the lookup table indexes on alias
    port and link type.  On output, the lookup table indexes on
    source addreess, destination address, source port, destination
    port and link type.
*/

struct ack_data_record     /* used to save changes to ack/seq numbers */
{
    u_long ack_old;
    u_long ack_new;
    int delta;
    int active;
};

struct tcp_state           /* Information about tcp connection        */
{
    int in;                /* State for outside -> inside             */
    int out;               /* State for inside  -> outside            */
    int index;             /* Index to ack data array                 */
    int ack_modified;      /* Indicates whether ack and seq numbers   */
                           /* been modified                           */
};

#define N_LINK_TCP_DATA   3 /* Number of distinct ack number changes
                               saved for a modified TCP stream */
struct tcp_dat
{
    struct tcp_state state;
    struct ack_data_record ack[N_LINK_TCP_DATA];
};

struct alias_link                /* Main data structure */
{
    struct in_addr src_addr;     /* Address and port information        */
    struct in_addr dst_addr;     /*  .                                  */
    struct in_addr alias_addr;   /*  .                                  */
    u_short src_port;            /*  .                                  */
    u_short dst_port;            /*  .                                  */
    u_short alias_port;          /*  .                                  */

    int link_type;               /* Type of link: tcp, udp, icmp, frag  */

/* values for link_type */
#define LINK_ICMP                     1
#define LINK_UDP                      2
#define LINK_TCP                      3
#define LINK_FRAGMENT_ID              4
#define LINK_FRAGMENT_PTR             5
#define LINK_ADDR                     6

    int flags;                   /* indicates special characteristics   */

/* flag bits */
#define LINK_UNKNOWN_DEST_PORT     0x01
#define LINK_UNKNOWN_DEST_ADDR     0x02
#define LINK_PERMANENT             0x04
#define LINK_PARTIALLY_SPECIFIED   0x03 /* logical-or of first two bits */

    int timestamp;               /* Time link was last accessed         */
    int expire_time;             /* Expire time for link                */

    int sockfd;                  /* socket descriptor                   */

    u_int start_point_out;       /* Index number in output lookup table */
    u_int start_point_in;
    struct alias_link *next_out; /* Linked list pointers for input and  */
    struct alias_link *last_out; /* output tables                       */
    struct alias_link *next_in;  /*  .                                  */
    struct alias_link *last_in;  /*  .                                  */

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
static struct alias_link *
linkTableOut[LINK_TABLE_OUT_SIZE];   /* Lookup table of pointers to     */
                                     /*   chains of link records. Each  */
static struct alias_link *           /*   link record is doubly indexed */
linkTableIn[LINK_TABLE_IN_SIZE];     /*   into input and output lookup  */
                                     /*   tables.                       */

static int icmpLinkCount;            /* Link statistics                 */
static int udpLinkCount;
static int tcpLinkCount;
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

static int firstCall = 1;            /* Needed by InitAlias()           */

static int newDefaultLink;           /* Indicates if a new aliasing     */
                                     /* link has been created after a   */
                                     /* call to PacketAliasIn/Out().    */
             





/* Internal utility routines (used only in alias_db.c)

Lookup table starting points:
    StartPointIn()           -- link table initial search point for
                                outgoing packets
    StartPointOut()          -- port table initial search point for
                                incoming packets
    
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


static u_int
StartPointIn(struct in_addr alias_addr,
             u_short alias_port,
             int link_type)
{
    u_int n;

    n  = alias_addr.s_addr;
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
    n += src_port; 
    n += dst_port;
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

   if (packetAliasMode & PKT_ALIAS_LOG)
   {
      fprintf(monitorFile, "icmp=%d, udp=%d, tcp=%d, frag_id=%d frag_ptr=%d",
              icmpLinkCount,
              udpLinkCount,
              tcpLinkCount,
              fragmentIdLinkCount,
              fragmentPtrLinkCount);

      fprintf(monitorFile, " / tot=%d  (sock=%d)\n",
              icmpLinkCount + udpLinkCount
                            + tcpLinkCount
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

Link search:
    FindLinkOut()           - find link for outgoing packets
    FindLinkIn()            - find link for incoming packets
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
FindLinkOut(struct in_addr, struct in_addr, u_short, u_short, int);

static struct alias_link *
FindLinkIn(struct in_addr, struct in_addr, u_short, u_short, int, int);


#define ALIAS_PORT_BASE            0x08000
#define ALIAS_PORT_MASK            0x07fff
#define GET_NEW_PORT_MAX_ATTEMPTS       20

#define GET_ALIAS_PORT                  -1
#define GET_ALIAS_ID        GET_ALIAS_PORT

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

   Whis this parameter is -1, it indicates to get a randomly
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
             * When the ALIAS_SAME_PORTS option is
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
        fprintf(stderr, "PacketAlias/GetNewPort(): ");
        fprintf(stderr, "input parameter error\n");
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
            if ((packetAliasMode && PKT_ALIAS_USE_SOCKETS)
             && (link->flags & LINK_PARTIALLY_SPECIFIED))
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

    fprintf(stderr, "PacketAlias/GetnewPort(): ");
    fprintf(stderr, "could not find free port\n");

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
        fprintf(stderr, "PacketAlias/GetSocket(): ");
        fprintf(stderr, "incorrect link type\n");
        return(0);
    }

    if (sock < 0)
    {
        fprintf(stderr, "PacketAlias/GetSocket(): ");
        fprintf(stderr, "socket() error %d\n", *sockfd);
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
        return(0);
    }
}


static void
CleanupAliasData(void)
{
    struct alias_link *link;
    int i, icount;

    icount = 0;
    for (i=0; i<LINK_TABLE_OUT_SIZE; i++)
    {
        link = linkTableOut[i];
        linkTableOut[i] = NULL;
        while (link != NULL)
        {
            struct alias_link *link_next;
            link_next = link->next_out;
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
    link = linkTableOut[cleanupIndex++];
    while (link != NULL)
    {
        int idelta;
        struct alias_link *link_next;

        link_next = link->next_out; 
        idelta = timeStamp - link->timestamp;
        switch (link->link_type)
        {
            case LINK_ICMP:
            case LINK_UDP:
            case LINK_FRAGMENT_ID:
            case LINK_FRAGMENT_PTR:
                if (idelta > link->expire_time)
                {
                    DeleteLink(link);
                    icount++;
                }
                break;
            case LINK_TCP:
                if (idelta > link->expire_time)
                {
                    struct tcp_dat *tcp_aux;

                    tcp_aux = link->data.tcp; 
                    if (tcp_aux->state.in  != 1
                     || tcp_aux->state.out != 1)
                    {
                        DeleteLink(link);
                        icount++;
                    }
                }
                break;
        }
        link = link_next;
    }

    if (cleanupIndex == LINK_TABLE_OUT_SIZE)
        cleanupIndex = 0;
}

void
DeleteLink(struct alias_link *link)
{
    struct alias_link *link_last;
    struct alias_link *link_next;

/* Don't do anything if the link is marked permanent */
    if (deleteAllLinks == 0 && link->flags & LINK_PERMANENT)
        return;

/* Adjust output table pointers */
    link_last = link->last_out;
    link_next = link->next_out;

    if (link_last != NULL)
        link_last->next_out = link_next;
    else
        linkTableOut[link->start_point_out] = link_next;

    if (link_next != NULL)
        link_next->last_out = link_last;

/* Adjust input table pointers */
    link_last = link->last_in;
    link_next = link->next_in;

    if (link_last != NULL)
        link_last->next_in = link_next;
    else
        linkTableIn[link->start_point_in] = link_next;

    if (link_next != NULL)
        link_next->last_in = link_last;

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
            if (link->data.tcp != NULL)
                free(link->data.tcp);
            break;
        case LINK_FRAGMENT_ID:
            fragmentIdLinkCount--;
            break;
        case LINK_FRAGMENT_PTR:
            fragmentPtrLinkCount--;
            if (link->data.frag_ptr != NULL)
                free(link->data.frag_ptr);
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
    struct alias_link *first_link;

    link = malloc(sizeof(struct alias_link));
    if (link != NULL)
    {
    /* If either the aliasing address or source address are
       equal to the default device address (equal to the
       global variable aliasAddress), then set the alias
       address field of the link record to zero */

        if (src_addr.s_addr == aliasAddress.s_addr)
            src_addr.s_addr = 0;

        if (alias_addr.s_addr == aliasAddress.s_addr)
            alias_addr.s_addr = 0;

    /* Basic initialization */
        link->src_addr    = src_addr;
        link->dst_addr    = dst_addr;
        link->src_port    = src_port;
        link->alias_addr  = alias_addr;
        link->dst_port    = dst_port;
        link->link_type   = link_type;
        link->sockfd      = -1;
        link->flags       = 0;
        link->timestamp   = timeStamp;

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
            link->expire_time = TCP_EXPIRE_TIME;
            break;
        case LINK_FRAGMENT_ID:
            link->expire_time = FRAGMENT_ID_EXPIRE_TIME;
            break;
        case LINK_FRAGMENT_PTR:
            link->expire_time = FRAGMENT_PTR_EXPIRE_TIME;
            break;
        }

    /* Determine alias flags */
        if (dst_addr.s_addr == 0)
            link->flags |= LINK_UNKNOWN_DEST_ADDR;
        if (dst_port == 0)
            link->flags |= LINK_UNKNOWN_DEST_PORT;

    /* Determine alias port */
        if (GetNewPort(link, alias_port_param) != 0)
        {
            free(link);
            return(NULL);
        }

    /* Set up pointers for output lookup table */
        start_point = StartPointOut(src_addr, dst_addr, 
                                    src_port, dst_port, link_type);
        first_link = linkTableOut[start_point];

        link->last_out        = NULL;
        link->next_out        = first_link;
        link->start_point_out = start_point;

        if (first_link != NULL)
            first_link->last_out = link;

        linkTableOut[start_point] = link;

    /* Set up pointers for input lookup table */
        start_point = StartPointIn(alias_addr, link->alias_port, link_type); 
        first_link = linkTableIn[start_point];

        link->last_in        = NULL;
        link->next_in        = first_link;
        link->start_point_in = start_point;

        if (first_link != NULL)
            first_link->last_in = link;

        linkTableIn[start_point] = link;

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
                link->data.tcp = aux_tcp;
                if (aux_tcp != NULL)
                {
                    int i;

                    tcpLinkCount++;
                    aux_tcp->state.in = 0;
                    aux_tcp->state.out = 0;
                    aux_tcp->state.index = 0;
                    aux_tcp->state.ack_modified = 0;
                    for (i=0; i<N_LINK_TCP_DATA; i++)
                        aux_tcp->ack[i].active = 0;
                }
                else
                {
                    fprintf(stderr, "PacketAlias/AddLink: ");
                    fprintf(stderr, " cannot allocate auxiliary TCP data\n");
                }
                break;
            case LINK_FRAGMENT_ID:
                fragmentIdLinkCount++;
                break;
            case LINK_FRAGMENT_PTR:
                fragmentPtrLinkCount++;
                break;
        }
    }
    else
    {
        fprintf(stderr, "PacketAlias/AddLink(): ");
        fprintf(stderr, "malloc() call failed.\n");
    }

    if (packetAliasMode & PKT_ALIAS_LOG)
    {
        ShowAliasStats();
    }

    return(link);
}


static struct alias_link *
FindLinkOut(struct in_addr src_addr,
            struct in_addr dst_addr,
            u_short src_port,
            u_short dst_port,
            int link_type)
{
    u_int i;
    struct alias_link *link;

    if (src_addr.s_addr == aliasAddress.s_addr)
        src_addr.s_addr = 0;

    i = StartPointOut(src_addr, dst_addr, src_port, dst_port, link_type);
    link = linkTableOut[i];
    while (link != NULL)
    {
        if (link->src_addr.s_addr == src_addr.s_addr
         && link->dst_addr.s_addr == dst_addr.s_addr
         && link->dst_port        == dst_port
         && link->src_port        == src_port
         && link->link_type       == link_type)
        {
            link->timestamp = timeStamp;
            break;
        }
        link = link->next_out;
    }

    return(link);
}


struct alias_link *
FindLinkIn(struct in_addr  dst_addr,
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
    if (dst_addr.s_addr == 0)
        flags_in |= LINK_UNKNOWN_DEST_ADDR;
    if (dst_port == 0)
        flags_in |= LINK_UNKNOWN_DEST_PORT;

/* The following allows permanent links to be
   be specified as using the default aliasing address
   (i.e. device interface address) without knowing
   in advance what that address is. */

    if (alias_addr.s_addr == aliasAddress.s_addr)
        alias_addr.s_addr = 0;

/* Search loop */
    start_point = StartPointIn(alias_addr, alias_port, link_type);
    link = linkTableIn[start_point];
    while (link != NULL)
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
        link = link->next_in;
    }



    if (link_fully_specified != NULL)
    {
        return (link_fully_specified);
    }
    else if (link_unknown_dst_port != NULL)
    {
        if (replace_partial_links)
        {
            link = AddLink(link_unknown_dst_port->src_addr, dst_addr,
                           alias_addr,
                           link_unknown_dst_port->src_port, dst_port,
                           alias_port, link_type);
            DeleteLink(link_unknown_dst_port);
            return(link);
        }
        else
        {
            return(link_unknown_dst_port);
        }
    }
    else if (link_unknown_dst_addr != NULL)
    {
        if (replace_partial_links)
        {
            link = AddLink(link_unknown_dst_addr->src_addr, dst_addr,
                           alias_addr,
                           link_unknown_dst_addr->src_port, dst_port,
                           alias_port, link_type);
            DeleteLink(link_unknown_dst_addr);
            return(link);
        }
        else
        {
            return(link_unknown_dst_addr);
        }
    }
    else if (link_unknown_all != NULL)
    {
        if (replace_partial_links)
        {
            link = AddLink(link_unknown_all->src_addr, dst_addr,
                           alias_addr,
                           link_unknown_all->src_port, dst_port,
                           alias_port, link_type);
            DeleteLink(link_unknown_all);
            return(link);
        }
        else
        {
            return(link_unknown_all);
        }
    }
    else
    {
        return(NULL);
    }
}




/* External routines for finding/adding links

-- "external" means outside alias_db.c, but within alias*.c --

    FindIcmpIn(), FindIcmpOut()
    FindFragmentIn1(), FindFragmentIn2()
    AddFragmentPtrLink(), FindFragmentPtr()
    FindUdpTcpIn(), FindUdpTcpOut()
    FindOriginalAddress(), FindAliasAddress()

(prototypes in alias_local.h)
*/


struct alias_link *
FindIcmpIn(struct in_addr dst_addr,
           struct in_addr alias_addr,
           u_short id_alias)
{
    return FindLinkIn(dst_addr, alias_addr,
                      NO_DEST_PORT, id_alias,
                      LINK_ICMP, 0);
}


struct alias_link *
FindIcmpOut(struct in_addr src_addr,
            struct in_addr dst_addr,
            u_short id)
{
    struct alias_link * link;

    link = FindLinkOut(src_addr, dst_addr,
                       id, NO_DEST_PORT,
                       LINK_ICMP);
    if (link == NULL)
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
FindUdpTcpIn(struct in_addr dst_addr,
             struct in_addr alias_addr,
             u_short        dst_port,
             u_short        alias_port,
             u_char         proto)
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
                      link_type, 1);

    if ( !(packetAliasMode & PKT_ALIAS_DENY_INCOMING) && link == NULL)
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

    link = FindLinkOut(src_addr, dst_addr, src_port, dst_port, link_type);

    if (link == NULL)
    {
        struct in_addr alias_addr;

        alias_addr = FindAliasAddress(src_addr);
        link = AddLink(src_addr, dst_addr, alias_addr,
                       src_port, dst_port, GET_ALIAS_PORT,
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
        if (targetAddress.s_addr != 0)
            return targetAddress;
        else
            return alias_addr;
    }
    else
    {
        if (link->src_addr.s_addr == 0)
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
                       0, 0, LINK_ADDR);
    if (link == NULL)
    {
        return aliasAddress;
    }
    else
    {
        if (link->alias_addr.s_addr == 0)
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
    (link->data.tcp)->state.in = state;
}


void
SetStateOut(struct alias_link *link, int state)
{
    /* TCP output state */
    (link->data.tcp)->state.out = state;
}


int
GetStateIn(struct alias_link *link)
{
    /* TCP input state */
    return( (link->data.tcp)->state.in);
}


int
GetStateOut(struct alias_link *link)
{
    /* TCP output state */
    return( (link->data.tcp)->state.out);
}


struct in_addr
GetOriginalAddress(struct alias_link *link)
{
    if (link->src_addr.s_addr == 0)
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
    if (link->alias_addr.s_addr == 0)
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


void
SetAckModified(struct alias_link *link)
{
/* Indicate that ack numbers have been modified in a TCP connection */
    (link->data.tcp)->state.ack_modified = 1;
}


int
GetAckModified(struct alias_link *link)
{
/* See if ack numbers have been modified */
    return( (link->data.tcp)->state.ack_modified );
}


int
GetDeltaAckIn(struct ip *pip, struct alias_link *link)
{
/*
Find out how much the ack number has been altered for an incoming
TCP packet.  To do this, a circular list is ack numbers where the TCP
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

        x = (link->data.tcp)->ack[i];
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
Find out how much the seq number has been altered for an outgoing
TCP packet.  To do this, a circular list is ack numbers where the TCP
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

        x = (link->data.tcp)->ack[i];
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

    i = (link->data.tcp)->state.index;
    (link->data.tcp)->ack[i] = x;

    i++;
    if (i == N_LINK_TCP_DATA)
        (link->data.tcp)->state.index = 0;
    else
        (link->data.tcp)->state.index = i;
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
        fprintf(stderr, "PacketAlias/SetExpire(): ");
        fprintf(stderr, "error in expire parameter\n");
    }
}

void
ClearCheckNewLink(void)
{
    newDefaultLink = 0;
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
        fprintf(stderr, "PacketAlias/HouseKeeping(): ");
        fprintf(stderr, "something unexpected in time values\n");
        lastCleanupTime = timeStamp;
        houseKeepingResidual = 0;
    }
}


/* Init the log file and enable logging */
void
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
void
UninitPacketAliasLog(void)
{
    if( monitorFile )
        fclose(monitorFile);
    packetAliasMode &= ~PKT_ALIAS_LOG;
}






/* Outside world interfaces

-- "outside world" means other than alias*.c routines --

    PacketAliasRedirectPort()
    PacketAliasRedirectAddr()
    PacketAliasRedirectDelete()
    PacketAliasSetAddress()
    PacketAliasInit()
    PacketAliasSetMode()

(prototypes in alias.h)
*/

/* Redirection from a specific public addr:port to a
   a private addr:port */
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
        fprintf(stderr, "PacketAliasRedirectPort(): ");
        fprintf(stderr, "only TCP and UDP protocols allowed\n");
        return NULL;
    }

    link = AddLink(src_addr, dst_addr, alias_addr,
                   src_port, dst_port, alias_port,
                   link_type);

    if (link != NULL)
    {
        link->flags |= LINK_PERMANENT;
    }
    else
    {
        fprintf(stderr, "PacketAliasRedirectPort(): " 
                        "call to AddLink() failed\n");
    }

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
    else
    {
        fprintf(stderr, "PacketAliasRedirectAddr(): " 
                        "call to AddLink() failed\n");
    }

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
    {
        CleanupAliasData();
        aliasAddress = addr;
    }
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

    if (firstCall == 1)
    {
        gettimeofday(&tv, &tz);
        timeStamp = tv.tv_sec;
        lastCleanupTime = tv.tv_sec;
        houseKeepingResidual = 0;

        for (i=0; i<LINK_TABLE_OUT_SIZE; i++)
            linkTableOut[i] = NULL;
        for (i=0; i<LINK_TABLE_IN_SIZE; i++)
            linkTableIn[i] = NULL;

        firstCall = 0;
    }
    else
    {
        deleteAllLinks = 1;
        CleanupAliasData();
        deleteAllLinks = 0;
    }

    aliasAddress.s_addr = 0;
    targetAddress.s_addr = 0;

    icmpLinkCount = 0;
    udpLinkCount = 0;
    tcpLinkCount = 0;
    fragmentIdLinkCount = 0;
    fragmentPtrLinkCount = 0;
    sockCount = 0;

    cleanupIndex =0;

    packetAliasMode = PKT_ALIAS_SAME_PORTS
                    | PKT_ALIAS_USE_SOCKETS
                    | PKT_ALIAS_RESET_ON_ADDR_CHANGE;
}


/* Change mode for some operations */
unsigned int
PacketAliasSetMode
(
    unsigned int flags, /* Which state to bring flags to */
    unsigned int mask   /* Mask of which flags to affect (use 0 to do a
                           probe for flag values) */
)
{
/* Enable logging? */
    if (flags & mask & PKT_ALIAS_LOG)
    {
        InitPacketAliasLog();     /* Do the enable */
    }
/* _Disable_ logging? */
    if (~flags & mask & PKT_ALIAS_LOG) {
        UninitPacketAliasLog();
    }

/* Other flags can be set/cleared without special action */
    packetAliasMode = (flags & mask) | (packetAliasMode & ~mask);
    return packetAliasMode;
}


int
PacketAliasCheckNewLink(void)
{
    return newDefaultLink;
}
