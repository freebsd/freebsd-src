/*
 * Copyright (C) 2004, 2007, 2008  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1998-2003  Internet Software Consortium.
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

/* $Id: socket.h,v 1.54.12.16 2008/09/11 06:14:46 each Exp $ */

#ifndef ISC_SOCKET_H
#define ISC_SOCKET_H 1

/*****
 ***** Module Info
 *****/

/*
 * Sockets
 *
 * Provides TCP and UDP sockets for network I/O.  The sockets are event
 * sources in the task system.
 *
 * When I/O completes, a completion event for the socket is posted to the
 * event queue of the task which requested the I/O.
 *
 * MP:
 *	The module ensures appropriate synchronization of data structures it
 *	creates and manipulates.
 *
 *	Clients of this module must not be holding a socket's task's lock when
 *	making a call that affects that socket.  Failure to follow this rule
 *	can result in deadlock.
 *
 *	The caller must ensure that isc_socketmgr_destroy() is called only
 *	once for a given manager.
 *
 * Reliability:
 *	No anticipated impact.
 *
 * Resources:
 *	<TBS>
 *
 * Security:
 *	No anticipated impact.
 *
 * Standards:
 *	None.
 */

/***
 *** Imports
 ***/

#include <isc/lang.h>
#include <isc/types.h>
#include <isc/event.h>
#include <isc/eventclass.h>
#include <isc/time.h>
#include <isc/region.h>
#include <isc/sockaddr.h>

ISC_LANG_BEGINDECLS

/***
 *** Constants
 ***/

/*
 * Maximum number of buffers in a scatter/gather read/write.  The operating
 * system in use must support at least this number (plus one on some.)
 */
#define ISC_SOCKET_MAXSCATTERGATHER	8

/*%
 * In isc_socket_bind() set socket option SO_REUSEADDR prior to calling
 * bind() if a non zero port is specified (AF_INET and AF_INET6).
 */
#define ISC_SOCKET_REUSEADDRESS		0x01U

/***
 *** Types
 ***/

struct isc_socketevent {
	ISC_EVENT_COMMON(isc_socketevent_t);
	isc_result_t		result;		/* OK, EOF, whatever else */
	unsigned int		minimum;	/* minimum i/o for event */
	unsigned int		n;		/* bytes read or written */
	unsigned int		offset;		/* offset into buffer list */
	isc_region_t		region;		/* for single-buffer i/o */
	isc_bufferlist_t	bufferlist;	/* list of buffers */
	isc_sockaddr_t		address;	/* source address */
	isc_time_t		timestamp;	/* timestamp of packet recv */
	struct in6_pktinfo	pktinfo;	/* ipv6 pktinfo */
	isc_uint32_t		attributes;	/* see below */
	isc_eventdestructor_t	destroy;	/* original destructor */
};

typedef struct isc_socket_newconnev isc_socket_newconnev_t;
struct isc_socket_newconnev {
	ISC_EVENT_COMMON(isc_socket_newconnev_t);
	isc_socket_t *		newsocket;
	isc_result_t		result;		/* OK, EOF, whatever else */
	isc_sockaddr_t		address;	/* source address */
};

typedef struct isc_socket_connev isc_socket_connev_t;
struct isc_socket_connev {
	ISC_EVENT_COMMON(isc_socket_connev_t);
	isc_result_t		result;		/* OK, EOF, whatever else */
};

/*
 * _ATTACHED:	Internal use only.
 * _TRUNC:	Packet was truncated on receive.
 * _CTRUNC:	Packet control information was truncated.  This can
 *		indicate that the packet is not complete, even though
 *		all the data is valid.
 * _TIMESTAMP:	The timestamp member is valid.
 * _PKTINFO:	The pktinfo member is valid.
 * _MULTICAST:	The UDP packet was received via a multicast transmission.
 */
#define ISC_SOCKEVENTATTR_ATTACHED		0x80000000U /* internal */
#define ISC_SOCKEVENTATTR_TRUNC			0x00800000U /* public */
#define ISC_SOCKEVENTATTR_CTRUNC		0x00400000U /* public */
#define ISC_SOCKEVENTATTR_TIMESTAMP		0x00200000U /* public */
#define ISC_SOCKEVENTATTR_PKTINFO		0x00100000U /* public */
#define ISC_SOCKEVENTATTR_MULTICAST		0x00080000U /* public */

#define ISC_SOCKEVENT_ANYEVENT  (0)
#define ISC_SOCKEVENT_RECVDONE	(ISC_EVENTCLASS_SOCKET + 1)
#define ISC_SOCKEVENT_SENDDONE	(ISC_EVENTCLASS_SOCKET + 2)
#define ISC_SOCKEVENT_NEWCONN	(ISC_EVENTCLASS_SOCKET + 3)
#define ISC_SOCKEVENT_CONNECT	(ISC_EVENTCLASS_SOCKET + 4)

/*
 * Internal events.
 */
#define ISC_SOCKEVENT_INTR	(ISC_EVENTCLASS_SOCKET + 256)
#define ISC_SOCKEVENT_INTW	(ISC_EVENTCLASS_SOCKET + 257)

typedef enum {
	isc_sockettype_udp = 1,
	isc_sockettype_tcp = 2
} isc_sockettype_t;

/*
 * How a socket should be shutdown in isc_socket_shutdown() calls.
 */
#define ISC_SOCKSHUT_RECV	0x00000001	/* close read side */
#define ISC_SOCKSHUT_SEND	0x00000002	/* close write side */
#define ISC_SOCKSHUT_ALL	0x00000003	/* close them all */

/*
 * What I/O events to cancel in isc_socket_cancel() calls.
 */
#define ISC_SOCKCANCEL_RECV	0x00000001	/* cancel recv */
#define ISC_SOCKCANCEL_SEND	0x00000002	/* cancel send */
#define ISC_SOCKCANCEL_ACCEPT	0x00000004	/* cancel accept */
#define ISC_SOCKCANCEL_CONNECT	0x00000008	/* cancel connect */
#define ISC_SOCKCANCEL_ALL	0x0000000f	/* cancel everything */

/*
 * Flags for isc_socket_send() and isc_socket_recv() calls.
 */
#define ISC_SOCKFLAG_IMMEDIATE	0x00000001	/* send event only if needed */
#define ISC_SOCKFLAG_NORETRY	0x00000002	/* drop failed UDP sends */

/***
 *** Socket and Socket Manager Functions
 ***
 *** Note: all Ensures conditions apply only if the result is success for
 *** those functions which return an isc_result.
 ***/

isc_result_t
isc_socket_create(isc_socketmgr_t *manager,
		  int pf,
		  isc_sockettype_t type,
		  isc_socket_t **socketp);
/*
 * Create a new 'type' socket managed by 'manager'.
 *
 * Note:
 *
 *	'pf' is the desired protocol family, e.g. PF_INET or PF_INET6.
 *
 * Requires:
 *
 *	'manager' is a valid manager
 *
 *	'socketp' is a valid pointer, and *socketp == NULL
 *
 * Ensures:
 *
 *	'*socketp' is attached to the newly created socket
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *	ISC_R_NOMEMORY
 *	ISC_R_NORESOURCES
 *	ISC_R_UNEXPECTED
 */

void
isc_socket_cancel(isc_socket_t *sock, isc_task_t *task,
		  unsigned int how);
/*
 * Cancel pending I/O of the type specified by "how".
 *
 * Note: if "task" is NULL, then the cancel applies to all tasks using the
 * socket.
 *
 * Requires:
 *
 *	"socket" is a valid socket
 *
 *	"task" is NULL or a valid task
 *
 * "how" is a bitmask describing the type of cancelation to perform.
 * The type ISC_SOCKCANCEL_ALL will cancel all pending I/O on this
 * socket.
 *
 * ISC_SOCKCANCEL_RECV:
 *	Cancel pending isc_socket_recv() calls.
 *
 * ISC_SOCKCANCEL_SEND:
 *	Cancel pending isc_socket_send() and isc_socket_sendto() calls.
 *
 * ISC_SOCKCANCEL_ACCEPT:
 *	Cancel pending isc_socket_accept() calls.
 *
 * ISC_SOCKCANCEL_CONNECT:
 *	Cancel pending isc_socket_connect() call.
 */

void
isc_socket_shutdown(isc_socket_t *sock, unsigned int how);
/*
 * Shutdown 'socket' according to 'how'.
 *
 * Requires:
 *
 *	'socket' is a valid socket.
 *
 *	'task' is NULL or is a valid task.
 *
 *	If 'how' is 'ISC_SOCKSHUT_RECV' or 'ISC_SOCKSHUT_ALL' then
 *
 *		The read queue must be empty.
 *
 *		No further read requests may be made.
 *
 *	If 'how' is 'ISC_SOCKSHUT_SEND' or 'ISC_SOCKSHUT_ALL' then
 *
 *		The write queue must be empty.
 *
 *		No further write requests may be made.
 */

void
isc_socket_attach(isc_socket_t *sock, isc_socket_t **socketp);
/*
 * Attach *socketp to socket.
 *
 * Requires:
 *
 *	'socket' is a valid socket.
 *
 *	'socketp' points to a NULL socket.
 *
 * Ensures:
 *
 *	*socketp is attached to socket.
 */

void
isc_socket_detach(isc_socket_t **socketp);
/*
 * Detach *socketp from its socket.
 *
 * Requires:
 *
 *	'socketp' points to a valid socket.
 *
 *	If '*socketp' is the last reference to the socket,
 *	then:
 *
 *		There must be no pending I/O requests.
 *
 * Ensures:
 *
 *	*socketp is NULL.
 *
 *	If '*socketp' is the last reference to the socket,
 *	then:
 *
 *		The socket will be shutdown (both reading and writing)
 *		for all tasks.
 *
 *		All resources used by the socket have been freed
 */

isc_result_t
isc_socket_open(isc_socket_t *sock);
/*
 * Open a new socket file descriptor of the given socket structure.  It simply
 * opens a new descriptor; all of the other parameters including the socket
 * type are inherited from the existing socket.  This function is provided to
 * avoid overhead of destroying and creating sockets when many short-lived
 * sockets are frequently opened and closed.  When the efficiency is not an
 * issue, it should be safer to detach the unused socket and re-create a new
 * one.  This optimization may not be available for some systems, in which
 * case this function will return ISC_R_NOTIMPLEMENTED and must not be used.
 *
 * Requires:
 *
 * \li	there must be no other reference to this socket.
 *
 * \li	'socket' is a valid and previously closed by isc_socket_close()
 *
 * Returns:
 *	Same as isc_socket_create().
 * \li	ISC_R_NOTIMPLEMENTED
 */

isc_result_t
isc_socket_close(isc_socket_t *sock);
/*
 * Close a socket file descriptor of the given socket structure.  This function
 * is provided as an alternative to destroying an unused socket when overhead
 * destroying/re-creating sockets can be significant, and is expected to be
 * used with isc_socket_open().  This optimization may not be available for some
 * systems, in which case this function will return ISC_R_NOTIMPLEMENTED and
 * must not be used.
 *
 * Requires:
 *
 * \li	The socket must have a valid descriptor.
 *
 * \li	There must be no other reference to this socket.
 *
 * \li	There must be no pending I/O requests.
 *
 * Returns:
 * \li	#ISC_R_NOTIMPLEMENTED
 */

isc_result_t
isc_socket_bind(isc_socket_t *sock, isc_sockaddr_t *addressp,
		unsigned int options);
/*
 * Bind 'socket' to '*addressp'.
 *
 * Requires:
 *
 *	'socket' is a valid socket
 *
 *	'addressp' points to a valid isc_sockaddr.
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *	ISC_R_NOPERM
 *	ISC_R_ADDRNOTAVAIL
 *	ISC_R_ADDRINUSE
 *	ISC_R_BOUND
 *	ISC_R_UNEXPECTED
 */

isc_result_t
isc_socket_filter(isc_socket_t *sock, const char *filter);
/*
 * Inform the kernel that it should perform accept filtering.
 * If filter is NULL the current filter will be removed.:w
 */

isc_result_t
isc_socket_listen(isc_socket_t *sock, unsigned int backlog);
/*
 * Set listen mode on the socket.  After this call, the only function that
 * can be used (other than attach and detach) is isc_socket_accept().
 *
 * Notes:
 *
 *	'backlog' is as in the UNIX system call listen() and may be
 *	ignored by non-UNIX implementations.
 *
 *	If 'backlog' is zero, a reasonable system default is used, usually
 *	SOMAXCONN.
 *
 * Requires:
 *
 *	'socket' is a valid, bound TCP socket.
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *	ISC_R_UNEXPECTED
 */

isc_result_t
isc_socket_accept(isc_socket_t *sock,
		  isc_task_t *task, isc_taskaction_t action, const void *arg);
/*
 * Queue accept event.  When a new connection is received, the task will
 * get an ISC_SOCKEVENT_NEWCONN event with the sender set to the listen
 * socket.  The new socket structure is sent inside the isc_socket_newconnev_t
 * event type, and is attached to the task 'task'.
 *
 * REQUIRES:
 *	'socket' is a valid TCP socket that isc_socket_listen() was called
 *	on.
 *
 *	'task' is a valid task
 *
 *	'action' is a valid action
 *
 * RETURNS:
 *	ISC_R_SUCCESS
 *	ISC_R_NOMEMORY
 *	ISC_R_UNEXPECTED
 */

isc_result_t
isc_socket_connect(isc_socket_t *sock, isc_sockaddr_t *addressp,
		   isc_task_t *task, isc_taskaction_t action,
		   const void *arg);
/*
 * Connect 'socket' to peer with address *saddr.  When the connection
 * succeeds, or when an error occurs, a CONNECT event with action 'action'
 * and arg 'arg' will be posted to the event queue for 'task'.
 *
 * Requires:
 *
 *	'socket' is a valid TCP socket
 *
 *	'addressp' points to a valid isc_sockaddr
 *
 *	'task' is a valid task
 *
 *	'action' is a valid action
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *	ISC_R_NOMEMORY
 *	ISC_R_UNEXPECTED
 *
 * Posted event's result code:
 *
 *	ISC_R_SUCCESS
 *	ISC_R_TIMEDOUT
 *	ISC_R_CONNREFUSED
 *	ISC_R_NETUNREACH
 *	ISC_R_UNEXPECTED
 */

isc_result_t
isc_socket_getpeername(isc_socket_t *sock, isc_sockaddr_t *addressp);
/*
 * Get the name of the peer connected to 'socket'.
 *
 * Requires:
 *
 *	'socket' is a valid TCP socket.
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *	ISC_R_TOOSMALL
 *	ISC_R_UNEXPECTED
 */

isc_result_t
isc_socket_getsockname(isc_socket_t *sock, isc_sockaddr_t *addressp);
/*
 * Get the name of 'socket'.
 *
 * Requires:
 *
 *	'socket' is a valid socket.
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *	ISC_R_TOOSMALL
 *	ISC_R_UNEXPECTED
 */

isc_result_t
isc_socket_recv(isc_socket_t *sock, isc_region_t *region,
		unsigned int minimum,
		isc_task_t *task, isc_taskaction_t action, const void *arg);
isc_result_t
isc_socket_recvv(isc_socket_t *sock, isc_bufferlist_t *buflist,
		 unsigned int minimum,
		 isc_task_t *task, isc_taskaction_t action, const void *arg);

isc_result_t
isc_socket_recv2(isc_socket_t *sock, isc_region_t *region,
		 unsigned int minimum, isc_task_t *task,
		 isc_socketevent_t *event, unsigned int flags);

/*
 * Receive from 'socket', storing the results in region.
 *
 * Notes:
 *
 *	Let 'length' refer to the length of 'region' or to the sum of all
 *	available regions in the list of buffers '*buflist'.
 *
 *	If 'minimum' is non-zero and at least that many bytes are read,
 *	the completion event will be posted to the task 'task.'  If minimum
 *	is zero, the exact number of bytes requested in the region must
 * 	be read for an event to be posted.  This only makes sense for TCP
 *	connections, and is always set to 1 byte for UDP.
 *
 *	The read will complete when the desired number of bytes have been
 *	read, if end-of-input occurs, or if an error occurs.  A read done
 *	event with the given 'action' and 'arg' will be posted to the
 *	event queue of 'task'.
 *
 *	The caller may not modify 'region', the buffers which are passed
 *	into this function, or any data they refer to until the completion
 *	event is received.
 *
 *	For isc_socket_recvv():
 *	On successful completion, '*buflist' will be empty, and the list of
 *	all buffers will be returned in the done event's 'bufferlist'
 *	member.  On error return, '*buflist' will be unchanged.
 *
 *	For isc_socket_recv2():
 *	'event' is not NULL, and the non-socket specific fields are
 *	expected to be initialized.
 *
 *	For isc_socket_recv2():
 *	The only defined value for 'flags' is ISC_SOCKFLAG_IMMEDIATE.  If
 *	set and the operation completes, the return value will be
 *	ISC_R_SUCCESS and the event will be filled in and not sent.  If the
 *	operation does not complete, the return value will be
 *	ISC_R_INPROGRESS and the event will be sent when the operation
 *	completes.
 *
 * Requires:
 *
 *	'socket' is a valid, bound socket.
 *
 *	For isc_socket_recv():
 *	'region' is a valid region
 *
 *	For isc_socket_recvv():
 *	'buflist' is non-NULL, and '*buflist' contain at least one buffer.
 *
 *	'task' is a valid task
 *
 *	For isc_socket_recv() and isc_socket_recvv():
 *	action != NULL and is a valid action
 *
 *	For isc_socket_recv2():
 *	event != NULL
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *	ISC_R_INPROGRESS
 *	ISC_R_NOMEMORY
 *	ISC_R_UNEXPECTED
 *
 * Event results:
 *
 *	ISC_R_SUCCESS
 *	ISC_R_UNEXPECTED
 *	XXX needs other net-type errors
 */

isc_result_t
isc_socket_send(isc_socket_t *sock, isc_region_t *region,
		isc_task_t *task, isc_taskaction_t action, const void *arg);
isc_result_t
isc_socket_sendto(isc_socket_t *sock, isc_region_t *region,
		  isc_task_t *task, isc_taskaction_t action, const void *arg,
		  isc_sockaddr_t *address, struct in6_pktinfo *pktinfo);
isc_result_t
isc_socket_sendv(isc_socket_t *sock, isc_bufferlist_t *buflist,
		 isc_task_t *task, isc_taskaction_t action, const void *arg);
isc_result_t
isc_socket_sendtov(isc_socket_t *sock, isc_bufferlist_t *buflist,
		   isc_task_t *task, isc_taskaction_t action, const void *arg,
		   isc_sockaddr_t *address, struct in6_pktinfo *pktinfo);
isc_result_t
isc_socket_sendto2(isc_socket_t *sock, isc_region_t *region,
		   isc_task_t *task,
		   isc_sockaddr_t *address, struct in6_pktinfo *pktinfo,
		   isc_socketevent_t *event, unsigned int flags);

/*
 * Send the contents of 'region' to the socket's peer.
 *
 * Notes:
 *
 *	Shutting down the requestor's task *may* result in any
 *	still pending writes being dropped or completed, depending on the
 *	underlying OS implementation.
 *
 *	If 'action' is NULL, then no completion event will be posted.
 *
 *	The caller may not modify 'region', the buffers which are passed
 *	into this function, or any data they refer to until the completion
 *	event is received.
 *
 *	For isc_socket_sendv() and isc_socket_sendtov():
 *	On successful completion, '*buflist' will be empty, and the list of
 *	all buffers will be returned in the done event's 'bufferlist'
 *	member.  On error return, '*buflist' will be unchanged.
 *
 *	For isc_socket_sendto2():
 *	'event' is not NULL, and the non-socket specific fields are
 *	expected to be initialized.
 *
 *	For isc_socket_sendto2():
 *	The only defined values for 'flags' are ISC_SOCKFLAG_IMMEDIATE
 *	and ISC_SOCKFLAG_NORETRY.
 *
 *	If ISC_SOCKFLAG_IMMEDIATE is set and the operation completes, the
 *	return value will be ISC_R_SUCCESS and the event will be filled
 *	in and not sent.  If the operation does not complete, the return
 *	value will be ISC_R_INPROGRESS and the event will be sent when
 *	the operation completes.
 *
 *	ISC_SOCKFLAG_NORETRY can only be set for UDP sockets.  If set
 *	and the send operation fails due to a transient error, the send
 *	will not be retried and the error will be indicated in the event.
 *	Using this option along with ISC_SOCKFLAG_IMMEDIATE allows the caller
 *	to specify a region that is allocated on the stack.
 *
 * Requires:
 *
 *	'socket' is a valid, bound socket.
 *
 *	For isc_socket_send():
 *	'region' is a valid region
 *
 *	For isc_socket_sendv() and isc_socket_sendtov():
 *	'buflist' is non-NULL, and '*buflist' contain at least one buffer.
 *
 *	'task' is a valid task
 *
 *	For isc_socket_sendv(), isc_socket_sendtov(), isc_socket_send(), and
 *	isc_socket_sendto():
 *	action == NULL or is a valid action
 *
 *	For isc_socket_sendto2():
 *	event != NULL
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *	ISC_R_INPROGRESS
 *	ISC_R_NOMEMORY
 *	ISC_R_UNEXPECTED
 *	ISC_R_NOTIMPLEMENTED
 *
 * Event results:
 *
 *	ISC_R_SUCCESS
 *	ISC_R_UNEXPECTED
 *	XXX needs other net-type errors
 */

isc_result_t
isc_socketmgr_create(isc_mem_t *mctx, isc_socketmgr_t **managerp);

isc_result_t
isc_socketmgr_create2(isc_mem_t *mctx, isc_socketmgr_t **managerp,
		      unsigned int maxsocks);
/*
 * Create a socket manager.  If "maxsocks" is non-zero, it specifies the
 * maximum number of sockets that the created manager should handle.
 * isc_socketmgr_create() is equivalent of isc_socketmgr_create2() with
 *
 * Notes:
 *
 *	All memory will be allocated in memory context 'mctx'.
 *
 * Requires:
 *
 *	'mctx' is a valid memory context.
 *
 *	'managerp' points to a NULL isc_socketmgr_t.
 *
 * Ensures:
 *
 *	'*managerp' is a valid isc_socketmgr_t.
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *	ISC_R_NOMEMORY
 *	ISC_R_UNEXPECTED
 */

isc_result_t
isc_socketmgr_getmaxsockets(isc_socketmgr_t *manager, unsigned int *nsockp);
/*%<
 * Returns in "*nsockp" the maximum number of sockets this manager may open.
 *
 * Requires:
 *
 *\li	'*manager' is a valid isc_socketmgr_t.
 *\li	'nsockp' is not NULL.
 *
 * Returns:
 *
 *\li	#ISC_R_SUCCESS
 *\li	#ISC_R_NOTIMPLEMENTED
 */

void
isc_socketmgr_destroy(isc_socketmgr_t **managerp);
/*
 * Destroy a socket manager.
 *
 * Notes:
 *
 *	This routine blocks until there are no sockets left in the manager,
 *	so if the caller holds any socket references using the manager, it
 *	must detach them before calling isc_socketmgr_destroy() or it will
 *	block forever.
 *
 * Requires:
 *
 *	'*managerp' is a valid isc_socketmgr_t.
 *
 *	All sockets managed by this manager are fully detached.
 *
 * Ensures:
 *
 *	*managerp == NULL
 *
 *	All resources used by the manager have been freed.
 */

isc_sockettype_t
isc_socket_gettype(isc_socket_t *sock);
/*
 * Returns the socket type for "sock."
 *
 * Requires:
 *
 *	"sock" is a valid socket.
 */

isc_boolean_t
isc_socket_isbound(isc_socket_t *sock);

void
isc_socket_ipv6only(isc_socket_t *sock, isc_boolean_t yes);
/*
 * If the socket is an IPv6 socket set/clear the IPV6_IPV6ONLY socket
 * option if the host OS supports this option.
 *
 * Requires:
 *	'sock' is a valid socket.
 */

void
isc__socketmgr_setreserved(isc_socketmgr_t *mgr, isc_uint32_t);
/*%<
 * Temporary.  For use by named only.
 */

ISC_LANG_ENDDECLS

#endif /* ISC_SOCKET_H */
