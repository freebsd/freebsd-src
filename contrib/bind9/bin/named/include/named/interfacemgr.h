/*
 * Copyright (C) 2004, 2005  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2002  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
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

/* $Id: interfacemgr.h,v 1.26.18.4 2005-04-27 05:00:35 sra Exp $ */

#ifndef NAMED_INTERFACEMGR_H
#define NAMED_INTERFACEMGR_H 1

/*****
 ***** Module Info
 *****/

/*! \file
 * \brief
 * The interface manager monitors the operating system's list
 * of network interfaces, creating and destroying listeners
 * as needed.
 *
 * Reliability:
 *\li	No impact expected.
 *
 * Resources:
 *
 * Security:
 * \li	The server will only be able to bind to the DNS port on
 *	newly discovered interfaces if it is running as root.
 *
 * Standards:
 *\li	The API for scanning varies greatly among operating systems.
 *	This module attempts to hide the differences.
 */

/***
 *** Imports
 ***/

#include <isc/magic.h>
#include <isc/mem.h>
#include <isc/socket.h>

#include <dns/result.h>

#include <named/listenlist.h>
#include <named/types.h>

/***
 *** Types
 ***/

#define IFACE_MAGIC		ISC_MAGIC('I',':','-',')')
#define NS_INTERFACE_VALID(t)	ISC_MAGIC_VALID(t, IFACE_MAGIC)

#define NS_INTERFACEFLAG_ANYADDR	0x01U	/*%< bound to "any" address */

/*% The nameserver interface structure */
struct ns_interface {
	unsigned int		magic;		/*%< Magic number. */
	ns_interfacemgr_t *	mgr;		/*%< Interface manager. */
	isc_mutex_t		lock;
	int			references;	/*%< Locked */
	unsigned int		generation;     /*%< Generation number. */
	isc_sockaddr_t		addr;           /*%< Address and port. */
	unsigned int		flags;		/*%< Interface characteristics */
	char 			name[32];	/*%< Null terminated. */
	dns_dispatch_t *	udpdispatch;	/*%< UDP dispatcher. */
	isc_socket_t *		tcpsocket;	/*%< TCP socket. */
	int			ntcptarget;	/*%< Desired number of concurrent
						     TCP accepts */
	int			ntcpcurrent;	/*%< Current ditto, locked */
	ns_clientmgr_t *	clientmgr;	/*%< Client manager. */
	ISC_LINK(ns_interface_t) link;
};

/***
 *** Functions
 ***/

isc_result_t
ns_interfacemgr_create(isc_mem_t *mctx, isc_taskmgr_t *taskmgr,
		       isc_socketmgr_t *socketmgr,
		       dns_dispatchmgr_t *dispatchmgr,
		       ns_interfacemgr_t **mgrp);
/*%
 * Create a new interface manager.
 *
 * Initially, the new manager will not listen on any interfaces.
 * Call ns_interfacemgr_setlistenon() and/or ns_interfacemgr_setlistenon6()
 * to set nonempty listen-on lists.
 */

void
ns_interfacemgr_attach(ns_interfacemgr_t *source, ns_interfacemgr_t **target);

void
ns_interfacemgr_detach(ns_interfacemgr_t **targetp);

void
ns_interfacemgr_shutdown(ns_interfacemgr_t *mgr);

void
ns_interfacemgr_scan(ns_interfacemgr_t *mgr, isc_boolean_t verbose);
/*%
 * Scan the operatings system's list of network interfaces
 * and create listeners when new interfaces are discovered.
 * Shut down the sockets for interfaces that go away.
 *
 * This should be called once on server startup and then
 * periodically according to the 'interface-interval' option
 * in named.conf.
 */

void
ns_interfacemgr_adjust(ns_interfacemgr_t *mgr, ns_listenlist_t *list,
		       isc_boolean_t verbose);
/*%
 * Similar to ns_interfacemgr_scan(), but this function also tries to see the
 * need for an explicit listen-on when a list element in 'list' is going to
 * override an already-listening a wildcard interface.
 *
 * This function does not update localhost and localnets ACLs.
 *
 * This should be called once on server startup, after configuring views and
 * zones.
 */

void
ns_interfacemgr_setlistenon4(ns_interfacemgr_t *mgr, ns_listenlist_t *value);
/*%
 * Set the IPv4 "listen-on" list of 'mgr' to 'value'.
 * The previous IPv4 listen-on list is freed.
 */

void
ns_interfacemgr_setlistenon6(ns_interfacemgr_t *mgr, ns_listenlist_t *value);
/*%
 * Set the IPv6 "listen-on" list of 'mgr' to 'value'.
 * The previous IPv6 listen-on list is freed.
 */

dns_aclenv_t *
ns_interfacemgr_getaclenv(ns_interfacemgr_t *mgr);

void
ns_interface_attach(ns_interface_t *source, ns_interface_t **target);

void
ns_interface_detach(ns_interface_t **targetp);

void
ns_interface_shutdown(ns_interface_t *ifp);
/*%
 * Stop listening for queries on interface 'ifp'.
 * May safely be called multiple times.
 */

void
ns_interfacemgr_dumprecursing(FILE *f, ns_interfacemgr_t *mgr);

isc_boolean_t
ns_interfacemgr_listeningon(ns_interfacemgr_t *mgr, isc_sockaddr_t *addr);

#endif /* NAMED_INTERFACEMGR_H */
