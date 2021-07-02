/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
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

#ifndef _ALIAS_DB_H_
#define _ALIAS_DB_H_


/*
   Constants (note: constants are also defined
	      near relevant functions or structs)
*/

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
#define TCP_EXPIRE_DEAD           10
#endif

/* When the link has been used and closed on one side - the other side
   is allowed to still send data */
#ifndef TCP_EXPIRE_SINGLEDEAD
#define TCP_EXPIRE_SINGLEDEAD     90
#endif

/* When the link isn't yet up */
#ifndef TCP_EXPIRE_INITIAL
#define TCP_EXPIRE_INITIAL       300
#endif

/* When the link is up */
#ifndef TCP_EXPIRE_CONNECTED
#define TCP_EXPIRE_CONNECTED   86400
#endif

/* Dummy port number codes used for FindLinkIn/Out() and AddLink().
   These constants can be anything except zero, which indicates an
   unknown port number. */

#define NO_DEST_PORT     1
#define NO_SRC_PORT      1

/* Matches any/unknown address in FindLinkIn/Out() and AddLink(). */
static struct in_addr const ANY_ADDR = { INADDR_ANY };

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

/* used to save changes to ACK/sequence numbers */
struct ack_data_record {
	u_long		ack_old;
	u_long		ack_new;
	int		delta;
	int		active;
};

/* Information about TCP connection */
struct tcp_state {
	int		in;	/* State for outside -> inside */
	int		out;	/* State for inside  -> outside */
	int		index;	/* Index to ACK data array */
	/* Indicates whether ACK and sequence numbers been modified */
	int		ack_modified;
};

/* Number of distinct ACK number changes
 * saved for a modified TCP stream */
#define N_LINK_TCP_DATA   3
struct tcp_dat {
	struct tcp_state state;
	struct ack_data_record ack[N_LINK_TCP_DATA];
	/* Which firewall record is used for this hole? */
	int		fwhole;
};

/* LSNAT server pool (circular list) */
struct server {
	struct in_addr	addr;
	u_short		port;
	struct server  *next;
};

/* Main data structure */
struct alias_link {
	struct libalias *la;
	/* Address and port information */
	struct in_addr	src_addr;
	struct in_addr	dst_addr;
	struct in_addr	alias_addr;
	struct in_addr	proxy_addr;
	u_short		src_port;
	u_short		dst_port;
	u_short		alias_port;
	u_short		proxy_port;
	struct server  *server;
	/* Type of link: TCP, UDP, ICMP, proto, frag */
	int		link_type;
/* values for link_type */
#define LINK_ICMP                     IPPROTO_ICMP
#define LINK_UDP                      IPPROTO_UDP
#define LINK_TCP                      IPPROTO_TCP
#define LINK_FRAGMENT_ID              (IPPROTO_MAX + 1)
#define LINK_FRAGMENT_PTR             (IPPROTO_MAX + 2)
#define LINK_ADDR                     (IPPROTO_MAX + 3)
#define LINK_PPTP                     (IPPROTO_MAX + 4)

	int		flags;	/* indicates special characteristics */
	int		pflags;	/* protocol-specific flags */
/* flag bits */
#define LINK_UNKNOWN_DEST_PORT     0x01
#define LINK_UNKNOWN_DEST_ADDR     0x02
#define LINK_PERMANENT             0x04
#define LINK_PARTIALLY_SPECIFIED   0x03	/* logical-or of first two bits */
#define LINK_UNFIREWALLED          0x08

	int		timestamp;	/* Time link was last accessed */
#ifndef NO_USE_SOCKETS
	int		sockfd;		/* socket descriptor */
#endif
	/* Linked list of pointers for input and output lookup tables  */
	union {
		struct {
			SPLAY_ENTRY(alias_link) out;
			LIST_ENTRY (alias_link) in;
		} all;
		struct {
			LIST_ENTRY (alias_link) list;
		} pptp;
	};
	struct {
		TAILQ_ENTRY(alias_link) list;
		int	time;	/* Expire time for link */
	} expire;
	/* Auxiliary data */
	union {
		char           *frag_ptr;
		struct in_addr	frag_addr;
		struct tcp_dat *tcp;
	} data;
};

/* Clean up procedure. */
static void finishoff(void);

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
static struct group_in *
StartPointIn(struct libalias *, struct in_addr, u_short, int, int);
static int	SeqDiff(u_long, u_long);

#ifndef NO_FW_PUNCH
/* Firewall control */
static void	InitPunchFW(struct libalias *);
static void	UninitPunchFW(struct libalias *);
static void	ClearFWHole(struct alias_link *);

#endif

/* Log file control */
static void	ShowAliasStats(struct libalias *);
static int	InitPacketAliasLog(struct libalias *);
static void	UninitPacketAliasLog(struct libalias *);

void		SctpShowAliasStats(struct libalias *la);


/* Splay handling */
static inline int
cmp_out(struct alias_link *a, struct alias_link *b) {
	int i = a->src_port - b->src_port;
	if (i != 0) return (i);
	if (a->src_addr.s_addr > b->src_addr.s_addr) return (1);
	if (a->src_addr.s_addr < b->src_addr.s_addr) return (-1);
	if (a->dst_addr.s_addr > b->dst_addr.s_addr) return (1);
	if (a->dst_addr.s_addr < b->dst_addr.s_addr) return (-1);
	i = a->dst_port - b->dst_port;
	if (i != 0) return (i);
	i = a->link_type - b->link_type;
	return (i);
}
SPLAY_PROTOTYPE(splay_out, alias_link, all.out, cmp_out);

static inline int
cmp_in(struct group_in *a, struct group_in *b) {
	int i = a->alias_port - b->alias_port;
	if (i != 0) return (i);
	i = a->link_type - b->link_type;
	if (i != 0) return (i);
	if (a->alias_addr.s_addr > b->alias_addr.s_addr) return (1);
	if (a->alias_addr.s_addr < b->alias_addr.s_addr) return (-1);
	return (0);
}
SPLAY_PROTOTYPE(splay_in, group_in, in, cmp_in);

/* Internal routines for finding, deleting and adding links

Port Allocation:
    GetNewPort()             -- find and reserve new alias port number
    GetSocket()              -- try to allocate a socket for a given port

Link creation and deletion:
    CleanupAliasData()      - remove all link chains from lookup table
    CleanupLink()           - look for a stale link
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
static int	GetNewPort(struct libalias *, struct alias_link *, int);
#ifndef NO_USE_SOCKETS
static u_short	GetSocket(struct libalias *, u_short, int *, int);
#endif
static void	CleanupAliasData(struct libalias *, int);
static void	CleanupLink(struct libalias *, struct alias_link **, int);
static void	DeleteLink(struct alias_link **, int);
static struct alias_link *
UseLink(struct libalias *, struct alias_link *);

static struct alias_link *
ReLink(struct alias_link *,
    struct in_addr, struct in_addr, struct in_addr,
    u_short, u_short, int, int, int);

static struct alias_link *
FindLinkOut(struct libalias *, struct in_addr, struct in_addr, u_short, u_short, int, int);

static struct alias_link *
FindLinkIn(struct libalias *, struct in_addr, struct in_addr, u_short, u_short, int, int);

static u_short _RandomPort(struct libalias *la);

#define GET_NEW_PORT_MAX_ATTEMPTS       20


#ifndef NO_FW_PUNCH

static void ClearAllFWHoles(struct libalias *la);

#define fw_setfield(la, field, num)			\
do {						\
    (field)[(num) - la->fireWallBaseNum] = 1;		\
} /*lint -save -e717 */ while(0)/* lint -restore */

#define fw_clrfield(la, field, num)			\
do {							\
    (field)[(num) - la->fireWallBaseNum] = 0;		\
} /*lint -save -e717 */ while(0)/* lint -restore */

#define fw_tstfield(la, field, num) ((field)[(num) - la->fireWallBaseNum])

#endif /* !NO_FW_PUNCH */

#endif /* _ALIAS_DB_H_ */
