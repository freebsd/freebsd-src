/*
    Alias_db.c encapsulates all data structures used for storing
    packet aliasing data.  Other parts of the aliasing software
    access data through functions provided in this file.

    Data storage is based on the notion of a "link", which is
    established for ICMP echo/reply packets, UDP datagrams and
    TCP stream connections.  A link stores the original source
    and destination addresses.  For UDP and TCP, it also stores
    source and destination port numbers, as well as an alias
    port number.

    There is a facility for sweeping through and deleting old
    links as new packets are sent through.  A simple timeout is
    used for ICMP and UDP links.  TCP links are left alone unless
    there is an incomplete connection, in which case the link
    can be deleted after a certain amount of time.  TCP links which
    properly close are deleted by a function call to DeleteLink()
    from alias.c.


    This software is placed into the public domain with no restrictions
    on its distribution.

    Initial version: August, 1996  (cjm)

    Version 1.4: September 16, 1996 (cjm)
        Facility for handling incoming links added.

    Version 1.6: September 18, 1996 (cjm)
        ICMP data handling simplified.
*/


/* Include files */

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "defs.h"


/* Constants */

#define FTP_CONTROL_PORT_NUMBER 21
#define LINK_TABLE_SIZE 101
#define LINK_ICMP 1
#define LINK_UDP  2
#define LINK_TCP  3
#define LINK_FRAGMENT_ID 4
#define N_LINK_ICMP_DATA 20
#define N_LINK_ID_DATA 20
#define N_LINK_TCP_DATA 20

#define ICMP_EXPIRE_TIME 60
#define UDP_EXPIRE_TIME 60
#define TCP_EXPIRE_TIME 60
#define FRAGMENT_EXPIRE_TIME 60

#define ALIAS_PORT_BASE 30000
#define ALIAS_PORT_MASK 0x7fff
#define ALIAS_PORT_TABLE_SIZE 0x8000


/* If MONITOR_ALIAS is defined, a message will be printed to
/var/log/alias.log every time a link is created or deleted.  This
is useful for debugging
*/

/* #define MONITOR_ALIAS */


/* If ALLOW_INCOMING is defined, then incoming connections (e.g.
to ftp, telnet or web servers will be handled.  Otherwise, these
types of connections will be prevented by the aliasing mechanism.
*/

#define ALLOW_INCOMING



/* Data Structures 

    The fundamental data structure used in this program is
    "struct link_record".  Whenever a TCP connection is made,
    a UDP datagram is sent out, or an ICMP echo request is made,
    a link record is made (if it has not already been created).

    The link record is identified by the source address/port
    and the destination address/port. In the case of an ICMP
    echo request, which has no port numbers, these fields are
    set to zero.

    The link record also can store some auxiliary data.  For ICMP
    echo requests, there is space allocated to save id and sequence
    numbers.  For tcp connections that have had sequence and
    acknowledgment modifications, data space is available to
    track these changes.  A state field is used to keep track in
    changes to the tcp connection state.  Id numbers of fragments
    can also be stored in the auxiliary space.

    The link records are chained together and initially referenced
    from a table in order to shorten lookup time.
*/

struct id_data_record     /* used to save data about ip fragments */
{
    u_short id;
    u_char protocol;
    struct in_addr src_addr;
    int active;
};

struct id_dat
{
    int index;
    struct id_data_record data[N_LINK_ID_DATA];
};

struct ack_data_record     /* used to save changes to ack/seq numbers */
{
    u_long ack_old;
    u_long ack_new;
    int delta;
    int active;
};

struct tcp_state           /* Information about tcp connection */
{
    int in;                /* State for outside -> inside  */
    int out;               /* State for inside  -> outside */
    int index;             /* Index to ack data array */
    int ack_modified;      /* Indicates whether ack and seq numbers have
                              been modified */
};

struct tcp_dat
{
    struct tcp_state state;
    struct ack_data_record ack[N_LINK_TCP_DATA];
};

struct link_record               /* Main data structure */
{
    struct in_addr src_addr;
    struct in_addr dst_addr;
    u_short src_port;
    u_short dst_port;
    u_short alias_port;
    long timestamp;
    int passthrough;             /* set to 1 if non-aliasing link */
    int link_type;               /* Type of link: tcp, udp, icmp, frag */
    int table_index;             /* Index number in lookup table */
    int socket;                  /* Socket used to bind alias port */
    struct link_record *next;    /* Pointer to next data structure */
    struct link_record *last;    /* Pointer to previous data structure */
    union                        /* Auxiliary data */
    {
        struct id_dat   *id;
        struct tcp_dat  *tcp;
    } data;
};



/* Function Prototypes */
#include "alias.p"



/* Global Variables 

    The global variables listed here are only accessed from
    within alias_db.c.  
*/

struct in_addr aliasAddress;      /* Address written onto source */
                                  /*    field of IP packet.      */
struct link_record 
    *linkTable[LINK_TABLE_SIZE];  /* Lookup table of pointers to */
                                  /*   chains of link records.   */


char portTable[ALIAS_PORT_TABLE_SIZE];  /* Table showing which ports   */
                                        /* are in use (no distinction  */
                                        /* is made between TCP and UDP */
u_short alias_sequence;   /* used for aliasing sequence numbers of */
                          /*   ICMP packets                        */

int icmpLinkCount;        /* Storage for link statistics */
int udpLinkCount;
int tcpLinkCount;
int fragmentLinkCount;

int cleanupIndex;         /* Index to chain of link table being */
                          /*   inspected for old links          */

int monitorAlias;         /* Debug variable (set to 1 when active) */
FILE *monitorFile;



/* Internal utility routines (used only in alias_db.c)

    StartPoint     -- find lookup table start point
    GetNewPort     -- find and reserve new alias port number
    SetTimestamp   -- save last access time
    SeqDiff        -- difference between two TCP sequences
*/

int
StartPoint(dst_addr, dst_port, link_type)
struct in_addr dst_addr;
u_short dst_port;
int link_type;
{
    u_long n;

    n =  dst_addr.s_addr;
    n += dst_port;
    n += link_type;
    return(n % LINK_TABLE_SIZE);
}

u_short
GetNewPort()
{
    int i;

    for (i=0; i<20; i++)
    {
        int port_offset;

        port_offset = (int) (random() & ALIAS_PORT_MASK);
        if (portTable[port_offset] == 0)
        {
            portTable[port_offset] = 1;
            return( htons(ALIAS_PORT_BASE + port_offset) );
        }
    }
            
    fprintf(stderr,
      "PacketAlias/GetNewPort: Can't allocate port number.\n");
    return(0);
}

void
SetTimestamp(clink)
char *clink;
{
    struct link_record *link;
    struct timezone tz;
    struct timeval tv;

    if (clink != NULL_PTR)
    {
        link = (struct link_record *) clink;
        gettimeofday(&tv, &tz);
        link->timestamp = tv.tv_sec;
    }
}

int
SeqDiff(x, y)
u_long x;
u_long y;
{
/* Return the difference between two TCP sequence numbers */

/*
    This function is encapsulated in case there are any unusual
    arithmetic conditions that need to be considered.
*/

    return (ntohl(y) - ntohl(x));
}

void ShowAliasStats()
{
/* Used for debugging */

    fprintf(monitorFile,
        "ShowAliasStats: icmp=%d, udp=%d, tcp=%d, frag=%d  /  tot=%d\n",
        icmpLinkCount, udpLinkCount, tcpLinkCount, fragmentLinkCount,
        icmpLinkCount+udpLinkCount+tcpLinkCount+fragmentLinkCount);
    fflush(monitorFile);
}




/* Internal routines for finding, deleting and adding links

    CleanupAliasData()     - remove all link chains from lookup table
    IncrementalCleanup()   - look for stale links in a single chain
    FindLink1()            - find link based on original ports
    FindLink2()            - find link based on alias port
    DeleteLink()           - remove link
    AddLink()              - add link 
*/

void CleanupAliasData()
{
    struct link_record *link;
    int i, icount;

    icount = 0;
    for (i=0; i<LINK_TABLE_SIZE; i++)
    {
        link = linkTable[i];
        linkTable[i] = NULL_PTR;
        while (link != NULL_PTR)
        {
            struct link_record *link_next;
            link_next = link->next; icount++;
            DeleteLink((char *)link);
            link = link_next;
        }
    }

    icmpLinkCount = 0;
    udpLinkCount = 0;
    tcpLinkCount = 0;
    fragmentLinkCount = 0;

    cleanupIndex =0;
}

void IncrementalCleanup()
{
    int icount;
    long secs;
    struct timeval tv;
    struct timezone tz;
    struct link_record *link;

    gettimeofday(&tv, &tz);
    secs = tv.tv_sec;

    icount = 0;
    link = linkTable[cleanupIndex++];
    while (link != NULL_PTR)
    {
        long idelta;
        struct link_record *link_next;

        link_next = link->next; 
        idelta = secs - link->timestamp;
        switch (link->link_type)
        {
            case LINK_ICMP:
                if (idelta > ICMP_EXPIRE_TIME)
                {
                    DeleteLink((char *) link);
                    icount++;
                }
                break;
            case LINK_UDP:
                if (idelta > UDP_EXPIRE_TIME)
                {
                    DeleteLink((char *) link);
                    icount++;
                }
                break;
            case LINK_TCP:
                if (idelta > TCP_EXPIRE_TIME)
                {
                    struct tcp_dat *tcp_aux;

                    tcp_aux = link->data.tcp; 
                    if (tcp_aux->state.in  != 1
                     || tcp_aux->state.out != 1)
                    {
                        DeleteLink((char *) link);
                        icount++;
                    }
                }
                break;
            case LINK_FRAGMENT_ID:
                if (idelta > FRAGMENT_EXPIRE_TIME)
                {
                    DeleteLink((char *) link);
                    icount++;
                }
                break;
        }
        link = link_next;
    }

    if (cleanupIndex == LINK_TABLE_SIZE)
        cleanupIndex = 0;
}

char *
FindLink1(src_addr, dst_addr, src_port, dst_port, link_type)
struct in_addr src_addr, dst_addr;
u_short src_port, dst_port;
int link_type;
{
    int i;
    struct link_record *link;

    i = StartPoint(dst_addr, dst_port, link_type);
    link = linkTable[i];
    while (link != NULL_PTR)
    {
        if (link->src_addr.s_addr == src_addr.s_addr
         && link->dst_addr.s_addr == dst_addr.s_addr
         && link->dst_port        == dst_port
         && link->src_port        == src_port
         && link->link_type       == link_type)
        {
            break;
        }
        link = link->next;
    }
    SetTimestamp((char *) link);
    IncrementalCleanup();
    return((char *) link);
}

char *
FindLink2(dst_addr, dst_port, alias_port, link_type)
struct in_addr dst_addr;
u_short dst_port, alias_port;
int link_type;
{
    int i;
    struct link_record *link;

    i = StartPoint(dst_addr, dst_port, link_type);
    link = linkTable[i];
    while (link != NULL_PTR)
    {
        if (link->dst_addr.s_addr == dst_addr.s_addr
         && link->dst_port        == dst_port
         && link->alias_port      == alias_port
         && link->link_type       == link_type)
        {
            break;
        }
        link = link->next;
    }
    SetTimestamp((char *) link);
    IncrementalCleanup();
    return((char *) link);
}

void
DeleteLink(clink)
char *clink;
{
    struct link_record *link;
    struct link_record *link_last;
    struct link_record *link_next;

    link = (struct link_record *) clink;

    link_last = link->last;
    link_next = link->next;

    if (link_last != NULL_PTR)
        link_last->next = link_next;
    else
        linkTable[link->table_index] = link->next;

    if (link_next != NULL_PTR)
        link_next->last = link_last;

    switch(link->link_type)
    {
        int port;

        case LINK_ICMP:
            icmpLinkCount--;
            break;
        case LINK_UDP:
            udpLinkCount--;
            port = ntohs(link->alias_port);
            if (port != 0 && link->passthrough == 0)
                portTable[port - ALIAS_PORT_BASE] = 0;
            break;
        case LINK_TCP:
            tcpLinkCount--;
            port = ntohs(link->alias_port);
            if (port != 0 && link->passthrough == 0)
                portTable[port - ALIAS_PORT_BASE] = 0;
            free(link->data.tcp);
            break;
        case LINK_FRAGMENT_ID:
            fragmentLinkCount--;
            free(link->data.id);
            break;
    }

    free(link);

    if (monitorAlias == 1)
    {
        ShowAliasStats();
    }
}

char *
AddLink(src_addr, dst_addr, src_port, dst_port, alias_port, link_type)
struct in_addr src_addr, dst_addr;
u_short src_port, dst_port, alias_port;
int link_type;
{
    int i;
    struct link_record *link, *last_link;

    i = StartPoint(dst_addr, dst_port, link_type);
    last_link = NULL_PTR;
    link = linkTable[i];
    while (link != NULL_PTR)
    {
        last_link = link;
        link = link->next;
    }

    link = malloc( sizeof(struct link_record) );
    if (link != NULL_PTR)
    {
        if (last_link != NULL_PTR)
            last_link->next = link;
        else
            linkTable[i] = link;
        link->next = NULL_PTR;
        link->table_index = i;
        link->last = last_link;
        link->src_addr = src_addr;
        link->dst_addr = dst_addr;
        link->src_port = src_port;
        link->dst_port = dst_port;
        link->alias_port = 0;
        link->link_type = link_type;
        link->passthrough = 0;

        switch(link_type)
        {
            struct id_dat   *aux_id;
            struct tcp_dat  *aux_tcp;

            case LINK_ICMP:
                icmpLinkCount++;
                link->alias_port = alias_port;
                break;
            case LINK_UDP:
                udpLinkCount++;
                link->alias_port = alias_port;
                break;
            case LINK_TCP:
                link->alias_port = alias_port;
                aux_tcp = malloc( sizeof(struct tcp_dat) );
                link->data.tcp = aux_tcp;
                if (aux_tcp != NULL_PTR)
                {
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
                    fprintf(stderr,
                    "PacketAlias/AddLink:Cannot allocate auxiliary TCP data"
                    );
                }
                break;
            case LINK_FRAGMENT_ID:
                aux_id = malloc( sizeof(struct id_dat) );
                link->data.id = aux_id;
                if (aux_id != NULL_PTR)
                {
                    fragmentLinkCount++;
                    aux_id->index = 0;
                    for (i=0; i<N_LINK_ID_DATA; i++)
                        aux_id->data[i].active = 0;
                }
                else
                {
                    fprintf(stderr,
                    "PacketAlias/AddLink:Cannot allocate auxiliary id data"
                    );
                }
                break;
        }
    }
    else
    {
        fprintf(stderr,
        "PacketAlias/AddLink: Cannot dynamically allocate memory.\n");
    }

    SetTimestamp((char *) link);
    IncrementalCleanup();

    if (monitorAlias == 1)
    {
        ShowAliasStats();
    }

    return((char *) link);
}
        


/* External routines for finding/adding links

    FindIcmpIn(), FindIcmpOut()
    FindFragmentIn1(), FindFragmentIn2()
    FindUdpIn(), FindUdpOut()
    FindTcpIn(), FindTcpOut()
*/

char *
FindIcmpIn(dst_addr, id, seq_alias)
struct in_addr dst_addr;
u_short id, seq_alias;
{
    return(FindLink2(dst_addr, id, seq_alias, LINK_ICMP));
}

char *
FindIcmpOut(src_addr, dst_addr, id, seq)
struct in_addr src_addr, dst_addr;
u_short id, seq;
{
    char *link;

    link = FindLink1(src_addr, dst_addr, seq, id, LINK_ICMP);
    if (link == NULL_PTR)
        link = AddLink(src_addr, dst_addr, seq, id,
                        alias_sequence++, LINK_ICMP);
    return((char *) link);
}

char *
FindFragmentIn1(dst_addr)
struct in_addr dst_addr;
{
    char *link;

    link = FindLink2(dst_addr, 0, 0, LINK_FRAGMENT_ID);
    if (link == NULL_PTR)
        link = AddLink(aliasAddress, dst_addr, 0, 0, 0, LINK_FRAGMENT_ID);
    return(link);
}

char *
FindFragmentIn2(dst_addr)
struct in_addr dst_addr;
{
    return(FindLink2(dst_addr, 0, 0, LINK_FRAGMENT_ID));
}

char *
FindUdpIn(dst_addr, dst_port, alias_port)
struct in_addr dst_addr;
u_short dst_port, alias_port;
{
    char *link;

    link = FindLink2(dst_addr, dst_port, alias_port, LINK_UDP);

#ifdef ALLOW_INCOMING
    if (link == NULL_PTR) {
        link = AddLink(GetAliasAddress(), dst_addr,
                       alias_port, dst_port, alias_port,
                       LINK_UDP);
        if (link != NULL_PTR)
            ((struct link_record *) link)->passthrough = 1;
    }
#endif

   return(link);
}

char *
FindUdpOut(src_addr, dst_addr, src_port, dst_port)
struct in_addr src_addr, dst_addr;
u_short src_port, dst_port;
{
    char *link;

    link = FindLink1(src_addr, dst_addr, src_port, dst_port, LINK_UDP);
    if (link == NULL_PTR)
        link = AddLink(src_addr, dst_addr,
                       src_port, dst_port, GetNewPort(),
                       LINK_UDP);
    return(link);
}

char *
FindTcpIn(dst_addr, dst_port, alias_port)
struct in_addr dst_addr;
u_short dst_port, alias_port;
{
    char *link;

    link = FindLink2(dst_addr, dst_port, alias_port, LINK_TCP);

#ifdef ALLOW_INCOMING
    if (link == NULL_PTR)
        link = AddLink(GetAliasAddress(), dst_addr,
                       alias_port, dst_port, alias_port,
                       LINK_TCP);
        if (link != NULL_PTR)
            ((struct link_record *) link)->passthrough = 1;
#endif

    return(link);
}

char *
FindTcpOut(src_addr, dst_addr, src_port, dst_port)
struct in_addr src_addr, dst_addr;
u_short src_port, dst_port;
{
    char *link;

    link = FindLink1(src_addr, dst_addr, src_port, dst_port, LINK_TCP);
    if (link == NULL_PTR)
        link = AddLink(src_addr, dst_addr,
                       src_port, dst_port, GetNewPort(),
                       LINK_TCP);
    return(link);
}




/* External routines for getting or changing link data

    SetFragmentData(), GetFragmentData()
    SetStateIn(), SetStateOut(), GetStateIn(), GetStateOut()
    GetOriginalAddress(), GetDestAddress(), GetAliasAddress()
    GetOriginalPort(), GetDestPort(), GetAliasPort()
    SetAckModified(), GetAckModified()
    GetDeltaAckIn(), GetDeltaSeqOut(), AddSeq()
*/

void
SetFragmentData(clink, idnum, protocol, src_addr)
char *clink;
u_short idnum;
u_char protocol;
struct in_addr src_addr;
{
    struct link_record *link;
    struct id_dat *idx;
    int i;
    link = (struct link_record *) clink;

    idx = link->data.id;
    if (idx != NULL_PTR)
    {
        i = idx->index;
        idx->data[i].id       = idnum;
        idx->data[i].protocol = protocol;
        idx->data[i].src_addr = src_addr;
        idx->data[i].active   = 1;
        i++;
        if (i == N_LINK_ID_DATA)
            idx->index = 0;
        else
            idx->index = i;
    }
}

void
GetFragmentAddr(clink, idnum, protocol, src_addr)
char *clink;
u_short idnum;
u_char protocol;
struct in_addr *src_addr;
{
    struct link_record *link;
    int i;

    link = (struct link_record *) clink;

    for (i=0; i<N_LINK_ID_DATA; i++)
    {
        struct id_dat *idx;

        idx = link->data.id;

        if (idx->data[i].active == 1)
        {
            if (idx->data[i].id       == idnum
             && idx->data[i].protocol == protocol)
            {
                *src_addr = idx->data[i].src_addr;
                break;
            }
        }
    }
}

void
SetStateIn(clink, state)
char *clink;
int state;
{
    struct link_record *link;

/* TCP input state */
    link = (struct link_record *) clink;
    (link->data.tcp)->state.in = state;
}

void
SetStateOut(clink, state)
char *clink;
int state;
{
    struct link_record *link;

/* TCP output state */
    link = (struct link_record *) clink;
    (link->data.tcp)->state.out = state;
}

int
GetStateIn(clink)
char *clink;
{
    struct link_record *link;

/* TCP input state */
    link = (struct link_record *) clink;
    return( (link->data.tcp)->state.in);
}

int
GetStateOut(clink)
char *clink;
{
    struct link_record *link;

/* TCP output state */
    link = (struct link_record *) clink;
    return( (link->data.tcp)->state.out);
}

struct in_addr
GetOriginalAddress(clink)
char *clink;
{
    struct link_record *link;

    link = (struct link_record *) clink;
    return(link->src_addr);
}

struct in_addr
GetDestAddress(clink)
char *clink;
{
    struct link_record *link;

    link = (struct link_record *) clink;
    return(link->dst_addr);
}

struct in_addr
GetAliasAddress()
{
    return(aliasAddress);
}

u_short
GetOriginalPort(clink)
char *clink;
{
    struct link_record *link;

    link = (struct link_record *) clink;
    return(link->src_port);
}

u_short
GetDestPort(clink)
char *clink;
{
    struct link_record *link;

    link = (struct link_record *) clink;
    return(link->dst_port);
}

u_short
GetAliasPort(clink)
char *clink;
{
    struct link_record *link;

    link = (struct link_record *) clink;
    return(link->alias_port);
}

void
SetAckModified(clink)
char *clink;
{
    struct link_record *link;

/* Indicate that ack numbers have been modified in a TCP connection */
    link = (struct link_record *) clink;
    (link->data.tcp)->state.ack_modified = 1;
}

int
GetAckModified(clink)
char *clink;
{
    struct link_record *link;

/* See if ack numbers have been modified */
    link = (struct link_record *) clink;
    return( (link->data.tcp)->state.ack_modified );
}

int
GetDeltaAckIn(pip, clink)
struct ip *pip;
char *clink;
{
/*
Find out how much the ack number has been altered for an incoming
TCP packet.  To do this, a circular list is ack numbers where the TCP
packet size was altered is searched. 
*/

    int i;
    struct link_record *link;
    struct tcphdr *tc;
    int delta, ack_diff_min;
    u_long ack;

    link = (struct link_record *) clink;
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
GetDeltaSeqOut(pip, clink)
struct ip *pip;
char *clink;
{
/*
Find out how much the seq number has been altered for an outgoing
TCP packet.  To do this, a circular list is ack numbers where the TCP
packet size was altered is searched. 
*/

    int i;
    struct link_record *link;
    struct tcphdr *tc;
    int delta, seq_diff_min;
    u_long seq;

    link = (struct link_record *) clink;
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
AddSeq(pip, clink, delta)
struct ip *pip;
char *clink;
int delta;
{
/*
When a TCP packet has been altered in length, save this
information in a circular list.  If enough packets have
been altered, then this list will begin to overwrite itself.
*/

    struct tcphdr *tc;
    struct link_record *link;
    struct ack_data_record x;
    int hlen, tlen, dlen;
    int i;

    link = (struct link_record *) clink;
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








/* Outside world interfaces

    SetAliasAddress()
    InitAliasLog()
    InitAlias()
*/

void
SetAliasAddress(addr)
struct in_addr addr;
{
    aliasAddress = addr;
    CleanupAliasData();
    if (monitorAlias == 1)
    {
        fprintf(monitorFile,
            "SetAliasAddress: %s\n", inet_ntoa(aliasAddress));
        fflush(monitorFile);
    }
}

void
InitAliasLog()
{
    monitorAlias = 1;
    monitorFile = fopen("/var/log/alias.log", "a");
    printf("Alias log file opened.\n");
}

void
InitAlias()
{
    int i;

    for (i=0; i<LINK_TABLE_SIZE; i++)
        linkTable[i] = NULL_PTR;

    for (i=0; i<ALIAS_PORT_TABLE_SIZE; i++)
        portTable[i] = 0;

    alias_sequence = 0;

    icmpLinkCount = 0;
    udpLinkCount = 0;
    tcpLinkCount = 0;
    fragmentLinkCount = 0;
    cleanupIndex =0;
    monitorAlias = 0;

    if (mode & MODE_ALIAS)
      printf("Packet aliasing enabled.\n");

#ifdef MONITOR_ALIAS
    InitAliasLog();
#endif
}
