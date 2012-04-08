/*
 * Copyright (C) 2004, 2005, 2007, 2009  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2001  Internet Software Consortium.
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

/* $Id: lib.c,v 1.19 2009/09/03 00:12:23 each Exp $ */

/*! \file */

#include <config.h>

#include <stddef.h>

#include <isc/hash.h>
#include <isc/mem.h>
#include <isc/msgcat.h>
#include <isc/mutex.h>
#include <isc/once.h>
#include <isc/util.h>

#include <dns/db.h>
#include <dns/ecdb.h>
#include <dns/lib.h>
#include <dns/result.h>

#include <dst/dst.h>


/***
 *** Globals
 ***/

LIBDNS_EXTERNAL_DATA unsigned int			dns_pps = 0U;
LIBDNS_EXTERNAL_DATA isc_msgcat_t *			dns_msgcat = NULL;


/***
 *** Private
 ***/

static isc_once_t		msgcat_once = ISC_ONCE_INIT;


/***
 *** Functions
 ***/

static void
open_msgcat(void) {
	isc_msgcat_open("libdns.cat", &dns_msgcat);
}

void
dns_lib_initmsgcat(void) {

	/*
	 * Initialize the DNS library's message catalog, dns_msgcat, if it
	 * has not already been initialized.
	 */

	RUNTIME_CHECK(isc_once_do(&msgcat_once, open_msgcat) == ISC_R_SUCCESS);
}

static isc_once_t init_once = ISC_ONCE_INIT;
static isc_mem_t *dns_g_mctx = NULL;
#ifndef BIND9
static dns_dbimplementation_t *dbimp = NULL;
#endif
static isc_boolean_t initialize_done = ISC_FALSE;
static isc_mutex_t reflock;
static unsigned int references = 0;

static void
initialize(void) {
	isc_result_t result;

	REQUIRE(initialize_done == ISC_FALSE);

	result = isc_mem_create(0, 0, &dns_g_mctx);
	if (result != ISC_R_SUCCESS)
		return;
	dns_result_register();
#ifndef BIND9
	result = dns_ecdb_register(dns_g_mctx, &dbimp);
	if (result != ISC_R_SUCCESS)
		goto cleanup_mctx;
#endif
	result = isc_hash_create(dns_g_mctx, NULL, DNS_NAME_MAXWIRE);
	if (result != ISC_R_SUCCESS)
		goto cleanup_db;

	result = dst_lib_init(dns_g_mctx, NULL, 0);
	if (result != ISC_R_SUCCESS)
		goto cleanup_hash;

	result = isc_mutex_init(&reflock);
	if (result != ISC_R_SUCCESS)
		goto cleanup_dst;

	initialize_done = ISC_TRUE;
	return;

  cleanup_dst:
	dst_lib_destroy();
  cleanup_hash:
	isc_hash_destroy();
  cleanup_db:
#ifndef BIND9
	dns_ecdb_unregister(&dbimp);
  cleanup_mctx:
#endif
	isc_mem_detach(&dns_g_mctx);
}

isc_result_t
dns_lib_init(void) {
	isc_result_t result;

	/*
	 * Since this routine is expected to be used by a normal application,
	 * it should be better to return an error, instead of an emergency
	 * abort, on any failure.
	 */
	result = isc_once_do(&init_once, initialize);
	if (result != ISC_R_SUCCESS)
		return (result);

	if (!initialize_done)
		return (ISC_R_FAILURE);

	LOCK(&reflock);
	references++;
	UNLOCK(&reflock);

	return (ISC_R_SUCCESS);
}

void
dns_lib_shutdown(void) {
	isc_boolean_t cleanup_ok = ISC_FALSE;

	LOCK(&reflock);
	if (--references == 0)
		cleanup_ok = ISC_TRUE;
	UNLOCK(&reflock);

	if (!cleanup_ok)
		return;

	dst_lib_destroy();
	isc_hash_destroy();
#ifndef BIND9
	dns_ecdb_unregister(&dbimp);
#endif
	isc_mem_detach(&dns_g_mctx);
}
