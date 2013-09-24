/*
 * Copyright (C) 2004, 2005, 2007-2009, 2011-2013  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2001, 2003  Internet Software Consortium.
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

/* $Id$ */

/*! \file */

#include <config.h>

#include <isc/log.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dns/db.h>
#include <dns/nsec.h>
#include <dns/rdata.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>
#include <dns/rdatasetiter.h>
#include <dns/rdatastruct.h>
#include <dns/result.h>

#include <dst/dst.h>

#define RETERR(x) do { \
	result = (x); \
	if (result != ISC_R_SUCCESS) \
		goto failure; \
	} while (0)

static void
set_bit(unsigned char *array, unsigned int index, unsigned int bit) {
	unsigned int shift, mask;

	shift = 7 - (index % 8);
	mask = 1 << shift;

	if (bit != 0)
		array[index / 8] |= mask;
	else
		array[index / 8] &= (~mask & 0xFF);
}

static unsigned int
bit_isset(unsigned char *array, unsigned int index) {
	unsigned int byte, shift, mask;

	byte = array[index / 8];
	shift = 7 - (index % 8);
	mask = 1 << shift;

	return ((byte & mask) != 0);
}

isc_result_t
dns_nsec_buildrdata(dns_db_t *db, dns_dbversion_t *version,
		    dns_dbnode_t *node, dns_name_t *target,
		    unsigned char *buffer, dns_rdata_t *rdata)
{
	isc_result_t result;
	dns_rdataset_t rdataset;
	isc_region_t r;
	unsigned int i, window;
	int octet;

	unsigned char *nsec_bits, *bm;
	unsigned int max_type;
	dns_rdatasetiter_t *rdsiter;

	memset(buffer, 0, DNS_NSEC_BUFFERSIZE);
	dns_name_toregion(target, &r);
	memcpy(buffer, r.base, r.length);
	r.base = buffer;
	/*
	 * Use the end of the space for a raw bitmap leaving enough
	 * space for the window identifiers and length octets.
	 */
	bm = r.base + r.length + 512;
	nsec_bits = r.base + r.length;
	set_bit(bm, dns_rdatatype_rrsig, 1);
	set_bit(bm, dns_rdatatype_nsec, 1);
	max_type = dns_rdatatype_nsec;
	dns_rdataset_init(&rdataset);
	rdsiter = NULL;
	result = dns_db_allrdatasets(db, node, version, 0, &rdsiter);
	if (result != ISC_R_SUCCESS)
		return (result);
	for (result = dns_rdatasetiter_first(rdsiter);
	     result == ISC_R_SUCCESS;
	     result = dns_rdatasetiter_next(rdsiter))
	{
		dns_rdatasetiter_current(rdsiter, &rdataset);
		if (rdataset.type != dns_rdatatype_nsec &&
		    rdataset.type != dns_rdatatype_nsec3 &&
		    rdataset.type != dns_rdatatype_rrsig) {
			if (rdataset.type > max_type)
				max_type = rdataset.type;
			set_bit(bm, rdataset.type, 1);
		}
		dns_rdataset_disassociate(&rdataset);
	}

	/*
	 * At zone cuts, deny the existence of glue in the parent zone.
	 */
	if (bit_isset(bm, dns_rdatatype_ns) &&
	    ! bit_isset(bm, dns_rdatatype_soa)) {
		for (i = 0; i <= max_type; i++) {
			if (bit_isset(bm, i) &&
			    ! dns_rdatatype_iszonecutauth((dns_rdatatype_t)i))
				set_bit(bm, i, 0);
		}
	}

	dns_rdatasetiter_destroy(&rdsiter);
	if (result != ISC_R_NOMORE)
		return (result);

	for (window = 0; window < 256; window++) {
		if (window * 256 > max_type)
			break;
		for (octet = 31; octet >= 0; octet--)
			if (bm[window * 32 + octet] != 0)
				break;
		if (octet < 0)
			continue;
		nsec_bits[0] = window;
		nsec_bits[1] = octet + 1;
		/*
		 * Note: potential overlapping move.
		 */
		memmove(&nsec_bits[2], &bm[window * 32], octet + 1);
		nsec_bits += 3 + octet;
	}
	r.length = nsec_bits - r.base;
	INSIST(r.length <= DNS_NSEC_BUFFERSIZE);
	dns_rdata_fromregion(rdata,
			     dns_db_class(db),
			     dns_rdatatype_nsec,
			     &r);

	return (ISC_R_SUCCESS);
}


isc_result_t
dns_nsec_build(dns_db_t *db, dns_dbversion_t *version, dns_dbnode_t *node,
	       dns_name_t *target, dns_ttl_t ttl)
{
	isc_result_t result;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	unsigned char data[DNS_NSEC_BUFFERSIZE];
	dns_rdatalist_t rdatalist;
	dns_rdataset_t rdataset;

	dns_rdataset_init(&rdataset);
	dns_rdata_init(&rdata);

	RETERR(dns_nsec_buildrdata(db, version, node, target, data, &rdata));

	rdatalist.rdclass = dns_db_class(db);
	rdatalist.type = dns_rdatatype_nsec;
	rdatalist.covers = 0;
	rdatalist.ttl = ttl;
	ISC_LIST_INIT(rdatalist.rdata);
	ISC_LIST_APPEND(rdatalist.rdata, &rdata, link);
	RETERR(dns_rdatalist_tordataset(&rdatalist, &rdataset));
	result = dns_db_addrdataset(db, node, version, 0, &rdataset,
				    0, NULL);
	if (result == DNS_R_UNCHANGED)
		result = ISC_R_SUCCESS;

 failure:
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	return (result);
}

isc_boolean_t
dns_nsec_typepresent(dns_rdata_t *nsec, dns_rdatatype_t type) {
	dns_rdata_nsec_t nsecstruct;
	isc_result_t result;
	isc_boolean_t present;
	unsigned int i, len, window;

	REQUIRE(nsec != NULL);
	REQUIRE(nsec->type == dns_rdatatype_nsec);

	/* This should never fail */
	result = dns_rdata_tostruct(nsec, &nsecstruct, NULL);
	INSIST(result == ISC_R_SUCCESS);

	present = ISC_FALSE;
	for (i = 0; i < nsecstruct.len; i += len) {
		INSIST(i + 2 <= nsecstruct.len);
		window = nsecstruct.typebits[i];
		len = nsecstruct.typebits[i + 1];
		INSIST(len > 0 && len <= 32);
		i += 2;
		INSIST(i + len <= nsecstruct.len);
		if (window * 256 > type)
			break;
		if ((window + 1) * 256 <= type)
			continue;
		if (type < (window * 256) + len * 8)
			present = ISC_TF(bit_isset(&nsecstruct.typebits[i],
						   type % 256));
		break;
	}
	dns_rdata_freestruct(&nsecstruct);
	return (present);
}

isc_result_t
dns_nsec_nseconly(dns_db_t *db, dns_dbversion_t *version,
		  isc_boolean_t *answer)
{
	dns_dbnode_t *node = NULL;
	dns_rdataset_t rdataset;
	dns_rdata_dnskey_t dnskey;
	isc_result_t result;

	REQUIRE(answer != NULL);

	dns_rdataset_init(&rdataset);

	result = dns_db_getoriginnode(db, &node);
	if (result != ISC_R_SUCCESS)
		return (result);

	result = dns_db_findrdataset(db, node, version, dns_rdatatype_dnskey,
				     0, 0, &rdataset, NULL);
	dns_db_detachnode(db, &node);

	if (result == ISC_R_NOTFOUND) {
		*answer = ISC_FALSE;
		return (ISC_R_SUCCESS);
	}
	if (result != ISC_R_SUCCESS)
		return (result);
	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		dns_rdata_t rdata = DNS_RDATA_INIT;

		dns_rdataset_current(&rdataset, &rdata);
		result = dns_rdata_tostruct(&rdata, &dnskey, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);

		if (dnskey.algorithm == DST_ALG_RSAMD5 ||
		    dnskey.algorithm == DST_ALG_RSASHA1 ||
		    dnskey.algorithm == DST_ALG_DSA ||
		    dnskey.algorithm == DST_ALG_ECC)
			break;
	}
	dns_rdataset_disassociate(&rdataset);
	if (result == ISC_R_SUCCESS)
		*answer = ISC_TRUE;
	if (result == ISC_R_NOMORE) {
		*answer = ISC_FALSE;
		result = ISC_R_SUCCESS;
	}
	return (result);
}

/*%
 * Return ISC_R_SUCCESS if we can determine that the name doesn't exist
 * or we can determine whether there is data or not at the name.
 * If the name does not exist return the wildcard name.
 *
 * Return ISC_R_IGNORE when the NSEC is not the appropriate one.
 */
isc_result_t
dns_nsec_noexistnodata(dns_rdatatype_t type, dns_name_t *name,
		       dns_name_t *nsecname, dns_rdataset_t *nsecset,
		       isc_boolean_t *exists, isc_boolean_t *data,
		       dns_name_t *wild, dns_nseclog_t logit, void *arg)
{
	int order;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_result_t result;
	dns_namereln_t relation;
	unsigned int olabels, nlabels, labels;
	dns_rdata_nsec_t nsec;
	isc_boolean_t atparent;
	isc_boolean_t ns;
	isc_boolean_t soa;

	REQUIRE(exists != NULL);
	REQUIRE(data != NULL);
	REQUIRE(nsecset != NULL &&
		nsecset->type == dns_rdatatype_nsec);

	result = dns_rdataset_first(nsecset);
	if (result != ISC_R_SUCCESS) {
		(*logit)(arg, ISC_LOG_DEBUG(3), "failure processing NSEC set");
		return (result);
	}
	dns_rdataset_current(nsecset, &rdata);

	(*logit)(arg, ISC_LOG_DEBUG(3), "looking for relevant NSEC");
	relation = dns_name_fullcompare(name, nsecname, &order, &olabels);

	if (order < 0) {
		/*
		 * The name is not within the NSEC range.
		 */
		(*logit)(arg, ISC_LOG_DEBUG(3),
			      "NSEC does not cover name, before NSEC");
		return (ISC_R_IGNORE);
	}

	if (order == 0) {
		/*
		 * The names are the same.   If we are validating "."
		 * then atparent should not be set as there is no parent.
		 */
		atparent = (olabels != 1) && dns_rdatatype_atparent(type);
		ns = dns_nsec_typepresent(&rdata, dns_rdatatype_ns);
		soa = dns_nsec_typepresent(&rdata, dns_rdatatype_soa);
		if (ns && !soa) {
			if (!atparent) {
				/*
				 * This NSEC record is from somewhere higher in
				 * the DNS, and at the parent of a delegation.
				 * It can not be legitimately used here.
				 */
				(*logit)(arg, ISC_LOG_DEBUG(3),
					      "ignoring parent nsec");
				return (ISC_R_IGNORE);
			}
		} else if (atparent && ns && soa) {
			/*
			 * This NSEC record is from the child.
			 * It can not be legitimately used here.
			 */
			(*logit)(arg, ISC_LOG_DEBUG(3),
				      "ignoring child nsec");
			return (ISC_R_IGNORE);
		}
		if (type == dns_rdatatype_cname || type == dns_rdatatype_nxt ||
		    type == dns_rdatatype_nsec || type == dns_rdatatype_key ||
		    !dns_nsec_typepresent(&rdata, dns_rdatatype_cname)) {
			*exists = ISC_TRUE;
			*data = dns_nsec_typepresent(&rdata, type);
			(*logit)(arg, ISC_LOG_DEBUG(3),
				      "nsec proves name exists (owner) data=%d",
				      *data);
			return (ISC_R_SUCCESS);
		}
		(*logit)(arg, ISC_LOG_DEBUG(3), "NSEC proves CNAME exists");
		return (ISC_R_IGNORE);
	}

	if (relation == dns_namereln_subdomain &&
	    dns_nsec_typepresent(&rdata, dns_rdatatype_ns) &&
	    !dns_nsec_typepresent(&rdata, dns_rdatatype_soa))
	{
		/*
		 * This NSEC record is from somewhere higher in
		 * the DNS, and at the parent of a delegation.
		 * It can not be legitimately used here.
		 */
		(*logit)(arg, ISC_LOG_DEBUG(3), "ignoring parent nsec");
		return (ISC_R_IGNORE);
	}

	result = dns_rdata_tostruct(&rdata, &nsec, NULL);
	if (result != ISC_R_SUCCESS)
		return (result);
	relation = dns_name_fullcompare(&nsec.next, name, &order, &nlabels);
	if (order == 0) {
		dns_rdata_freestruct(&nsec);
		(*logit)(arg, ISC_LOG_DEBUG(3),
			      "ignoring nsec matches next name");
		return (ISC_R_IGNORE);
	}

	if (order < 0 && !dns_name_issubdomain(nsecname, &nsec.next)) {
		/*
		 * The name is not within the NSEC range.
		 */
		dns_rdata_freestruct(&nsec);
		(*logit)(arg, ISC_LOG_DEBUG(3),
			    "ignoring nsec because name is past end of range");
		return (ISC_R_IGNORE);
	}

	if (order > 0 && relation == dns_namereln_subdomain) {
		(*logit)(arg, ISC_LOG_DEBUG(3),
			      "nsec proves name exist (empty)");
		dns_rdata_freestruct(&nsec);
		*exists = ISC_TRUE;
		*data = ISC_FALSE;
		return (ISC_R_SUCCESS);
	}
	if (wild != NULL) {
		dns_name_t common;
		dns_name_init(&common, NULL);
		if (olabels > nlabels) {
			labels = dns_name_countlabels(nsecname);
			dns_name_getlabelsequence(nsecname, labels - olabels,
						  olabels, &common);
		} else {
			labels = dns_name_countlabels(&nsec.next);
			dns_name_getlabelsequence(&nsec.next, labels - nlabels,
						  nlabels, &common);
		}
		result = dns_name_concatenate(dns_wildcardname, &common,
					       wild, NULL);
		if (result != ISC_R_SUCCESS) {
			dns_rdata_freestruct(&nsec);
			(*logit)(arg, ISC_LOG_DEBUG(3),
				    "failure generating wildcard name");
			return (result);
		}
	}
	dns_rdata_freestruct(&nsec);
	(*logit)(arg, ISC_LOG_DEBUG(3), "nsec range ok");
	*exists = ISC_FALSE;
	return (ISC_R_SUCCESS);
}
