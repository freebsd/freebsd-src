/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: interfaceiter.h,v 1.10 2001/01/09 21:57:01 bwelling Exp $ */

#ifndef ISC_INTERFACEITER_H
#define ISC_INTERFACEITER_H 1

/*****
 ***** Module Info
 *****/

/*
 * Interface iterator
 *
 * Iterate over the list of network interfaces.
 *
 * Interfaces whose address family is not supported are ignored and never
 * returned by the iterator.  Interfaces whose netmask, interface flags,
 * or similar cannot be obtained are also ignored, and the failure is logged.
 *
 * Standards:
 *	The API for scanning varies greatly among operating systems.
 *	This module attempts to hide the differences.
 */

/***
 *** Imports
 ***/

#include <isc/lang.h>
#include <isc/netaddr.h>
#include <isc/types.h>

/*
 * Public structure describing a network interface.
 */

struct isc_interface {
	char name[32];			/* Interface name, null-terminated. */
	unsigned int af;		/* Address family. */
	isc_netaddr_t address;		/* Local address. */
	isc_netaddr_t netmask;		/* Network mask. */
	isc_netaddr_t broadcast;	/* Broadcast address. */
	isc_netaddr_t dstaddress; 	/* Destination address
					   (point-to-point only). */
	isc_uint32_t flags;		/* Flags; see below. */
	unsigned int ifindex;		/* Interface Index */
	unsigned int scopeid;		/* Scope id for Multicasting */
};

/* Interface flags. */

#define INTERFACE_F_UP			0x00000001U /* Interface is up */
#define INTERFACE_F_POINTTOPOINT	0x00000002U /*this is point-to-point interface*/
#define INTERFACE_F_LOOPBACK		0x00000004U /* this is loopback interface */
#define INTERFACE_F_BROADCAST		0x00000008U /* Broadcast is  supported */
#define INTERFACE_F_MULTICAST		0x00000010U /* multicast is supported */

/***
 *** Functions
 ***/

ISC_LANG_BEGINDECLS

isc_result_t
isc_interfaceiter_create(isc_mem_t *mctx, isc_interfaceiter_t **iterp);
/*
 * Create an iterator for traversing the operating system's list
 * of network interfaces.
 *
 * Returns:
 *	ISC_R_SUCCESS
 * 	ISC_R_NOMEMORY
 *	Various network-related errors
 */

isc_result_t
isc_interfaceiter_first(isc_interfaceiter_t *iter);
/*
 * Position the iterator on the first interface.
 *
 * Returns:
 *	ISC_R_SUCCESS		Success.
 *	ISC_R_NOMORE		There are no interfaces.
 */

isc_result_t
isc_interfaceiter_current(isc_interfaceiter_t *iter,
			  isc_interface_t *ifdata);
/*
 * Get information about the interface the iterator is currently
 * positioned at and store it at *ifdata.
 *
 * Requires:
 * 	The iterator has been successfully positioned using
 * 	isc_interface_iter_first() / isc_interface_iter_next().
 *
 * Returns:
 *	ISC_R_SUCCESS		Success.
 */

isc_result_t
isc_interfaceiter_next(isc_interfaceiter_t *iter);
/*
 * Position the iterator on the next interface.
 *
 * Requires:
 * 	The iterator has been successfully positioned using
 * 	isc_interface_iter_first() / isc_interface_iter_next().
 *
 * Returns:
 *	ISC_R_SUCCESS		Success.
 *	ISC_R_NOMORE		There are no more interfaces.
 */

void
isc_interfaceiter_destroy(isc_interfaceiter_t **iterp);
/*
 * Destroy the iterator.
 */

ISC_LANG_ENDDECLS

#endif /* ISC_INTERFACEITER_H */
