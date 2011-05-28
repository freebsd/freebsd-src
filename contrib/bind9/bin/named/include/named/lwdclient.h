/*
 * Copyright (C) 2004, 2005, 2009  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: lwdclient.h,v 1.14.18.4 2009-01-19 23:46:14 tbox Exp $ */

#ifndef NAMED_LWDCLIENT_H
#define NAMED_LWDCLIENT_H 1

/*! \file */

#include <isc/event.h>
#include <isc/eventclass.h>
#include <isc/netaddr.h>
#include <isc/sockaddr.h>
#include <isc/types.h>

#include <dns/fixedname.h>
#include <dns/types.h>

#include <lwres/lwres.h>

#include <named/lwsearch.h>

#define LWRD_EVENTCLASS		ISC_EVENTCLASS(4242)

#define LWRD_SHUTDOWN		(LWRD_EVENTCLASS + 0x0001)

/*% Lightweight Resolver Daemon Client */
struct ns_lwdclient {
	isc_sockaddr_t		address;	/*%< where to reply */
	struct in6_pktinfo	pktinfo;
	isc_boolean_t		pktinfo_valid;
	ns_lwdclientmgr_t	*clientmgr;	/*%< our parent */
	ISC_LINK(ns_lwdclient_t) link;
	unsigned int		state;
	void			*arg;		/*%< packet processing state */

	/*
	 * Received data info.
	 */
	unsigned char		buffer[LWRES_RECVLENGTH]; /*%< receive buffer */
	isc_uint32_t		recvlength;	/*%< length recv'd */
	lwres_lwpacket_t	pkt;

	/*%
	 * Send data state.  If sendbuf != buffer (that is, the send buffer
	 * isn't our receive buffer) it will be freed to the lwres_context_t.
	 */
	unsigned char	       *sendbuf;
	isc_uint32_t		sendlength;
	isc_buffer_t		recv_buffer;

	/*%
	 * gabn (get address by name) state info.
	 */
	dns_adbfind_t		*find;
	dns_adbfind_t		*v4find;
	dns_adbfind_t		*v6find;
	unsigned int		find_wanted;	/*%< Addresses we want */
	dns_fixedname_t		query_name;
	dns_fixedname_t		target_name;
	ns_lwsearchctx_t	searchctx;
	lwres_gabnresponse_t	gabn;

	/*%
	 * gnba (get name by address) state info.
	 */
	lwres_gnbaresponse_t	gnba;
	dns_byaddr_t	       *byaddr;
	unsigned int		options;
	isc_netaddr_t		na;

	/*%
	 * grbn (get rrset by name) state info.
	 *
	 * Note: this also uses target_name and searchctx.
	 */
	lwres_grbnresponse_t	grbn;
	dns_lookup_t	       *lookup;
	dns_rdatatype_t		rdtype;

	/*%
	 * Alias and address info.  This is copied up to the gabn/gnba
	 * structures eventually.
	 *
	 * XXXMLG We can keep all of this in a client since we only service
	 * three packet types right now.  If we started handling more,
	 * we'd need to use "arg" above and allocate/destroy things.
	 */
	char		       *aliases[LWRES_MAX_ALIASES];
	isc_uint16_t		aliaslen[LWRES_MAX_ALIASES];
	lwres_addr_t		addrs[LWRES_MAX_ADDRS];
};

/*%
 * Client states.
 *
 * _IDLE	The client is not doing anything at all.
 *
 * _RECV	The client is waiting for data after issuing a socket recv().
 *
 * _RECVDONE	Data has been received, and is being processed.
 *
 * _FINDWAIT	An adb (or other) request was made that cannot be satisfied
 *		immediately.  An event will wake the client up.
 *
 * _SEND	All data for a response has completed, and a reply was
 *		sent via a socket send() call.
 *
 * Badly formatted state table:
 *
 *	IDLE -> RECV when client has a recv() queued.
 *
 *	RECV -> RECVDONE when recvdone event received.
 *
 *	RECVDONE -> SEND if the data for a reply is at hand.
 *	RECVDONE -> FINDWAIT if more searching is needed, and events will
 *		eventually wake us up again.
 *
 *	FINDWAIT -> SEND when enough data was received to reply.
 *
 *	SEND -> IDLE when a senddone event was received.
 *
 *	At any time -> IDLE on error.  Sometimes this will be -> SEND
 *	instead, if enough data is on hand to reply with a meaningful
 *	error.
 *
 *	Packets which are badly formatted may or may not get error returns.
 */
#define NS_LWDCLIENT_STATEIDLE		1
#define NS_LWDCLIENT_STATERECV		2
#define NS_LWDCLIENT_STATERECVDONE	3
#define NS_LWDCLIENT_STATEFINDWAIT	4
#define NS_LWDCLIENT_STATESEND		5
#define NS_LWDCLIENT_STATESENDDONE	6

#define NS_LWDCLIENT_ISIDLE(c)		\
			((c)->state == NS_LWDCLIENT_STATEIDLE)
#define NS_LWDCLIENT_ISRECV(c)		\
			((c)->state == NS_LWDCLIENT_STATERECV)
#define NS_LWDCLIENT_ISRECVDONE(c)	\
			((c)->state == NS_LWDCLIENT_STATERECVDONE)
#define NS_LWDCLIENT_ISFINDWAIT(c)	\
			((c)->state == NS_LWDCLIENT_STATEFINDWAIT)
#define NS_LWDCLIENT_ISSEND(c)		\
			((c)->state == NS_LWDCLIENT_STATESEND)

/*%
 * Overall magic test that means we're not idle.
 */
#define NS_LWDCLIENT_ISRUNNING(c)	(!NS_LWDCLIENT_ISIDLE(c))

#define NS_LWDCLIENT_SETIDLE(c)		\
			((c)->state = NS_LWDCLIENT_STATEIDLE)
#define NS_LWDCLIENT_SETRECV(c)		\
			((c)->state = NS_LWDCLIENT_STATERECV)
#define NS_LWDCLIENT_SETRECVDONE(c)	\
			((c)->state = NS_LWDCLIENT_STATERECVDONE)
#define NS_LWDCLIENT_SETFINDWAIT(c)	\
			((c)->state = NS_LWDCLIENT_STATEFINDWAIT)
#define NS_LWDCLIENT_SETSEND(c)		\
			((c)->state = NS_LWDCLIENT_STATESEND)
#define NS_LWDCLIENT_SETSENDDONE(c)	\
			((c)->state = NS_LWDCLIENT_STATESENDDONE)

/*% lightweight daemon client manager */
struct ns_lwdclientmgr {
	ns_lwreslistener_t     *listener;
	isc_mem_t	       *mctx;
	isc_socket_t	       *sock;		/*%< socket to use */
	dns_view_t	       *view;
	lwres_context_t	       *lwctx;		/*%< lightweight proto context */
	isc_task_t	       *task;		/*%< owning task */
	unsigned int		flags;
	ISC_LINK(ns_lwdclientmgr_t)	link;
	ISC_LIST(ns_lwdclient_t)	idle;		/*%< idle client slots */
	ISC_LIST(ns_lwdclient_t)	running;	/*%< running clients */
};

#define NS_LWDCLIENTMGR_FLAGRECVPENDING		0x00000001
#define NS_LWDCLIENTMGR_FLAGSHUTTINGDOWN	0x00000002

isc_result_t
ns_lwdclientmgr_create(ns_lwreslistener_t *, unsigned int, isc_taskmgr_t *);

void
ns_lwdclient_initialize(ns_lwdclient_t *, ns_lwdclientmgr_t *);

isc_result_t
ns_lwdclient_startrecv(ns_lwdclientmgr_t *);

void
ns_lwdclient_stateidle(ns_lwdclient_t *);

void
ns_lwdclient_recv(isc_task_t *, isc_event_t *);

void
ns_lwdclient_shutdown(isc_task_t *, isc_event_t *);

void
ns_lwdclient_send(isc_task_t *, isc_event_t *);

isc_result_t
ns_lwdclient_sendreply(ns_lwdclient_t *client, isc_region_t *r);

/*
 * Processing functions of various types.
 */
void ns_lwdclient_processgabn(ns_lwdclient_t *, lwres_buffer_t *);
void ns_lwdclient_processgnba(ns_lwdclient_t *, lwres_buffer_t *);
void ns_lwdclient_processgrbn(ns_lwdclient_t *, lwres_buffer_t *);
void ns_lwdclient_processnoop(ns_lwdclient_t *, lwres_buffer_t *);

void ns_lwdclient_errorpktsend(ns_lwdclient_t *, isc_uint32_t);

void ns_lwdclient_log(int level, const char *format, ...)
     ISC_FORMAT_PRINTF(2, 3);

#endif /* NAMED_LWDCLIENT_H */
