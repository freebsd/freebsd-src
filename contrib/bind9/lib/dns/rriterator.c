/*
 * Copyright (C) 2009  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: rriterator.c,v 1.2 2009-06-30 02:52:32 each Exp $ */

/*! \file */

/***
 *** Imports
 ***/

#include <config.h>

#include <isc/string.h>
#include <isc/util.h>

#include <dns/db.h>
#include <dns/dbiterator.h>
#include <dns/rdata.h>
#include <dns/rdataset.h>
#include <dns/rdatasetiter.h>
#include <dns/result.h>
#include <dns/rriterator.h>

/***
 *** RRiterator methods
 ***/

isc_result_t
dns_rriterator_init(dns_rriterator_t *it, dns_db_t *db, dns_dbversion_t *ver,
		    isc_stdtime_t now)
{
	isc_result_t result;
	it->magic = RRITERATOR_MAGIC;
	it->db = db;
	it->dbit = NULL;
	it->ver = ver;
	it->now = now;
	it->node = NULL;
	result = dns_db_createiterator(it->db, 0, &it->dbit);
	if (result != ISC_R_SUCCESS)
		return (result);
	it->rdatasetit = NULL;
	dns_rdata_init(&it->rdata);
	dns_rdataset_init(&it->rdataset);
	dns_fixedname_init(&it->fixedname);
	INSIST(! dns_rdataset_isassociated(&it->rdataset));
	it->result = ISC_R_SUCCESS;
	return (it->result);
}

isc_result_t
dns_rriterator_first(dns_rriterator_t *it) {
	REQUIRE(VALID_RRITERATOR(it));
	/* Reset state */
	if (dns_rdataset_isassociated(&it->rdataset))
		dns_rdataset_disassociate(&it->rdataset);
	if (it->rdatasetit != NULL)
		dns_rdatasetiter_destroy(&it->rdatasetit);
	if (it->node != NULL)
		dns_db_detachnode(it->db, &it->node);
	it->result = dns_dbiterator_first(it->dbit);

	/*
	 * The top node may be empty when out of zone glue exists.
	 * Walk the tree to find the first node with data.
	 */
	while (it->result == ISC_R_SUCCESS) {
		it->result = dns_dbiterator_current(it->dbit, &it->node,
					   dns_fixedname_name(&it->fixedname));
		if (it->result != ISC_R_SUCCESS)
			return (it->result);

		it->result = dns_db_allrdatasets(it->db, it->node, it->ver,
						 it->now, &it->rdatasetit);
		if (it->result != ISC_R_SUCCESS)
			return (it->result);

		it->result = dns_rdatasetiter_first(it->rdatasetit);
		if (it->result != ISC_R_SUCCESS) {
			/*
			 * This node is empty. Try next node.
			 */
			dns_rdatasetiter_destroy(&it->rdatasetit);
			dns_db_detachnode(it->db, &it->node);
			it->result = dns_dbiterator_next(it->dbit);
			continue;
		}
		dns_rdatasetiter_current(it->rdatasetit, &it->rdataset);
		it->rdataset.attributes |= DNS_RDATASETATTR_LOADORDER;
		it->result = dns_rdataset_first(&it->rdataset);
		return (it->result);
	}
	return (it->result);
}

isc_result_t
dns_rriterator_nextrrset(dns_rriterator_t *it) {
	REQUIRE(VALID_RRITERATOR(it));
	if (dns_rdataset_isassociated(&it->rdataset))
		dns_rdataset_disassociate(&it->rdataset);
	it->result = dns_rdatasetiter_next(it->rdatasetit);
	/*
	 * The while loop body is executed more than once
	 * only when an empty dbnode needs to be skipped.
	 */
	while (it->result == ISC_R_NOMORE) {
		dns_rdatasetiter_destroy(&it->rdatasetit);
		dns_db_detachnode(it->db, &it->node);
		it->result = dns_dbiterator_next(it->dbit);
		if (it->result == ISC_R_NOMORE) {
			/* We are at the end of the entire database. */
			return (it->result);
		}
		if (it->result != ISC_R_SUCCESS)
			return (it->result);
		it->result = dns_dbiterator_current(it->dbit, &it->node,
					   dns_fixedname_name(&it->fixedname));
		if (it->result != ISC_R_SUCCESS)
			return (it->result);
		it->result = dns_db_allrdatasets(it->db, it->node, it->ver,
						 it->now, &it->rdatasetit);
		if (it->result != ISC_R_SUCCESS)
			return (it->result);
		it->result = dns_rdatasetiter_first(it->rdatasetit);
	}
	if (it->result != ISC_R_SUCCESS)
		return (it->result);
	dns_rdatasetiter_current(it->rdatasetit, &it->rdataset);
	it->rdataset.attributes |= DNS_RDATASETATTR_LOADORDER;
	it->result = dns_rdataset_first(&it->rdataset);
	return (it->result);
}

isc_result_t
dns_rriterator_next(dns_rriterator_t *it) {
	REQUIRE(VALID_RRITERATOR(it));
	if (it->result != ISC_R_SUCCESS)
		return (it->result);

	INSIST(it->dbit != NULL);
	INSIST(it->node != NULL);
	INSIST(it->rdatasetit != NULL);

	it->result = dns_rdataset_next(&it->rdataset);
	if (it->result == ISC_R_NOMORE)
		return (dns_rriterator_nextrrset(it));
	return (it->result);
}

void
dns_rriterator_pause(dns_rriterator_t *it) {
	REQUIRE(VALID_RRITERATOR(it));
	RUNTIME_CHECK(dns_dbiterator_pause(it->dbit) == ISC_R_SUCCESS);
}

void
dns_rriterator_destroy(dns_rriterator_t *it) {
	REQUIRE(VALID_RRITERATOR(it));
	if (dns_rdataset_isassociated(&it->rdataset))
		dns_rdataset_disassociate(&it->rdataset);
	if (it->rdatasetit != NULL)
		dns_rdatasetiter_destroy(&it->rdatasetit);
	if (it->node != NULL)
		dns_db_detachnode(it->db, &it->node);
	dns_dbiterator_destroy(&it->dbit);
}

void
dns_rriterator_current(dns_rriterator_t *it, dns_name_t **name,
		       isc_uint32_t *ttl, dns_rdataset_t **rdataset,
		       dns_rdata_t **rdata)
{
	REQUIRE(name != NULL && *name == NULL);
	REQUIRE(VALID_RRITERATOR(it));
	REQUIRE(it->result == ISC_R_SUCCESS);

	*name = dns_fixedname_name(&it->fixedname);
	*ttl = it->rdataset.ttl;

	dns_rdata_reset(&it->rdata);
	dns_rdataset_current(&it->rdataset, &it->rdata);

	if (rdataset)
		*rdataset = &it->rdataset;

	if (rdata)
		*rdata = &it->rdata;
}
