/*
 * Copyright (C) 2011  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: clientinfo.h,v 1.3 2011/10/11 23:46:45 tbox Exp $ */

#ifndef DNS_CLIENTINFO_H
#define DNS_CLIENTINFO_H 1

/*****
 ***** Module Info
 *****/

/*! \file dns/clientinfo.h
 * \brief
 * The DNS clientinfo interface allows libdns to retrieve information
 * about the client from the caller.
 *
 * The clientinfo interface is used by the DNS DB and DLZ interfaces;
 * it allows databases to modify their answers on the basis of information
 * about the client, such as source IP address.
 *
 * dns_clientinfo_t contains a pointer to an opaque structure containing
 * client information in some form.  dns_clientinfomethods_t contains a
 * list of methods which operate on that opaque structure to return
 * potentially useful data.  Both structures also contain versioning
 * information.
 */

/*****
 ***** Imports
 *****/

#include <isc/sockaddr.h>
#include <isc/types.h>

ISC_LANG_BEGINDECLS

/*****
 ***** Types
 *****/

#define DNS_CLIENTINFO_VERSION 1
typedef struct dns_clientinfo {
	isc_uint16_t version;
	void *data;
} dns_clientinfo_t;

typedef isc_result_t (*dns_clientinfo_sourceip_t)(dns_clientinfo_t *client,
						  isc_sockaddr_t **addrp);

#define DNS_CLIENTINFOMETHODS_VERSION 1
#define DNS_CLIENTINFOMETHODS_AGE 0

typedef struct dns_clientinfomethods {
	isc_uint16_t version;
	isc_uint16_t age;
	dns_clientinfo_sourceip_t sourceip;
} dns_clientinfomethods_t;

/*****
 ***** Methods
 *****/
void
dns_clientinfomethods_init(dns_clientinfomethods_t *methods,
			   dns_clientinfo_sourceip_t sourceip);

void
dns_clientinfo_init(dns_clientinfo_t *ci, void *data);

ISC_LANG_ENDDECLS

#endif /* DNS_CLIENTINFO_H */
