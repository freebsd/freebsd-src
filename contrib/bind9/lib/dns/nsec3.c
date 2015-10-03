/*
 * Copyright (C) 2006, 2008-2015  Internet Systems Consortium, Inc. ("ISC")
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

#include <config.h>

#include <isc/base32.h>
#include <isc/buffer.h>
#include <isc/hex.h>
#include <isc/iterated_hash.h>
#include <isc/log.h>
#include <isc/string.h>
#include <isc/util.h>
#include <isc/safe.h>

#include <dst/dst.h>

#include <dns/db.h>
#include <dns/zone.h>
#include <dns/compress.h>
#include <dns/dbiterator.h>
#include <dns/diff.h>
#include <dns/fixedname.h>
#include <dns/nsec.h>
#include <dns/nsec3.h>
#include <dns/rdata.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>
#include <dns/rdatasetiter.h>
#include <dns/rdatastruct.h>
#include <dns/result.h>

#define CHECK(x) do { \
	result = (x); \
	if (result != ISC_R_SUCCESS) \
		goto failure; \
	} while (0)

#define OPTOUT(x) (((x) & DNS_NSEC3FLAG_OPTOUT) != 0)
#define CREATE(x) (((x) & DNS_NSEC3FLAG_CREATE) != 0)
#define INITIAL(x) (((x) & DNS_NSEC3FLAG_INITIAL) != 0)
#define REMOVE(x) (((x) & DNS_NSEC3FLAG_REMOVE) != 0)

isc_result_t
dns_nsec3_buildrdata(dns_db_t *db, dns_dbversion_t *version,
		     dns_dbnode_t *node, unsigned int hashalg,
		     unsigned int flags, unsigned int iterations,
		     const unsigned char *salt, size_t salt_length,
		     const unsigned char *nexthash, size_t hash_length,
		     unsigned char *buffer, dns_rdata_t *rdata)
{
	isc_result_t result;
	dns_rdataset_t rdataset;
	isc_region_t r;
	unsigned int i;
	isc_boolean_t found;
	isc_boolean_t found_ns;
	isc_boolean_t need_rrsig;

	unsigned char *nsec_bits, *bm;
	unsigned int max_type;
	dns_rdatasetiter_t *rdsiter;
	unsigned char *p;

	REQUIRE(salt_length < 256U);
	REQUIRE(hash_length < 256U);
	REQUIRE(flags <= 0xffU);
	REQUIRE(hashalg <= 0xffU);
	REQUIRE(iterations <= 0xffffU);

	switch (hashalg) {
	case dns_hash_sha1:
		REQUIRE(hash_length == ISC_SHA1_DIGESTLENGTH);
		break;
	}

	memset(buffer, 0, DNS_NSEC3_BUFFERSIZE);

	p = buffer;

	*p++ = hashalg;
	*p++ = flags;

	*p++ = iterations >> 8;
	*p++ = iterations;

	*p++ = (unsigned char)salt_length;
	memmove(p, salt, salt_length);
	p += salt_length;

	*p++ = (unsigned char)hash_length;
	memmove(p, nexthash, hash_length);
	p += hash_length;

	r.length = (unsigned int)(p - buffer);
	r.base = buffer;

	/*
	 * Use the end of the space for a raw bitmap leaving enough
	 * space for the window identifiers and length octets.
	 */
	bm = r.base + r.length + 512;
	nsec_bits = r.base + r.length;
	max_type = 0;
	if (node == NULL)
		goto collapse_bitmap;
	dns_rdataset_init(&rdataset);
	rdsiter = NULL;
	result = dns_db_allrdatasets(db, node, version, 0, &rdsiter);
	if (result != ISC_R_SUCCESS)
		return (result);
	found = found_ns = need_rrsig = ISC_FALSE;
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
			dns_nsec_setbit(bm, rdataset.type, 1);
			/*
			 * Work out if we need to set the RRSIG bit for
			 * this node.  We set the RRSIG bit if either of
			 * the following conditions are met:
			 * 1) We have a SOA or DS then we need to set
			 *    the RRSIG bit as both always will be signed.
			 * 2) We set the RRSIG bit if we don't have
			 *    a NS record but do have other data.
			 */
			if (rdataset.type == dns_rdatatype_soa ||
			    rdataset.type == dns_rdatatype_ds)
				need_rrsig = ISC_TRUE;
			else if (rdataset.type == dns_rdatatype_ns)
				found_ns = ISC_TRUE;
			else
				found = ISC_TRUE;
		}
		dns_rdataset_disassociate(&rdataset);
	}
	if ((found && !found_ns) || need_rrsig) {
		if (dns_rdatatype_rrsig > max_type)
			max_type = dns_rdatatype_rrsig;
		dns_nsec_setbit(bm, dns_rdatatype_rrsig, 1);
	}

	/*
	 * At zone cuts, deny the existence of glue in the parent zone.
	 */
	if (dns_nsec_isset(bm, dns_rdatatype_ns) &&
	    ! dns_nsec_isset(bm, dns_rdatatype_soa)) {
		for (i = 0; i <= max_type; i++) {
			if (dns_nsec_isset(bm, i) &&
			    ! dns_rdatatype_iszonecutauth((dns_rdatatype_t)i))
				dns_nsec_setbit(bm, i, 0);
		}
	}

	dns_rdatasetiter_destroy(&rdsiter);
	if (result != ISC_R_NOMORE)
		return (result);

 collapse_bitmap:
	nsec_bits += dns_nsec_compressbitmap(nsec_bits, bm, max_type);
	r.length = (unsigned int)(nsec_bits - r.base);
	INSIST(r.length <= DNS_NSEC3_BUFFERSIZE);
	dns_rdata_fromregion(rdata, dns_db_class(db), dns_rdatatype_nsec3, &r);

	return (ISC_R_SUCCESS);
}

isc_boolean_t
dns_nsec3_typepresent(dns_rdata_t *rdata, dns_rdatatype_t type) {
	dns_rdata_nsec3_t nsec3;
	isc_result_t result;
	isc_boolean_t present;
	unsigned int i, len, window;

	REQUIRE(rdata != NULL);
	REQUIRE(rdata->type == dns_rdatatype_nsec3);

	/* This should never fail */
	result = dns_rdata_tostruct(rdata, &nsec3, NULL);
	INSIST(result == ISC_R_SUCCESS);

	present = ISC_FALSE;
	for (i = 0; i < nsec3.len; i += len) {
		INSIST(i + 2 <= nsec3.len);
		window = nsec3.typebits[i];
		len = nsec3.typebits[i + 1];
		INSIST(len > 0 && len <= 32);
		i += 2;
		INSIST(i + len <= nsec3.len);
		if (window * 256 > type)
			break;
		if ((window + 1) * 256 <= type)
			continue;
		if (type < (window * 256) + len * 8)
			present = ISC_TF(dns_nsec_isset(&nsec3.typebits[i],
							type % 256));
		break;
	}
	dns_rdata_freestruct(&nsec3);
	return (present);
}

isc_result_t
dns_nsec3_hashname(dns_fixedname_t *result,
		   unsigned char rethash[NSEC3_MAX_HASH_LENGTH],
		   size_t *hash_length, dns_name_t *name, dns_name_t *origin,
		   dns_hash_t hashalg, unsigned int iterations,
		   const unsigned char *salt, size_t saltlength)
{
	unsigned char hash[NSEC3_MAX_HASH_LENGTH];
	unsigned char nametext[DNS_NAME_FORMATSIZE];
	dns_fixedname_t fixed;
	dns_name_t *downcased;
	isc_buffer_t namebuffer;
	isc_region_t region;
	size_t len;

	if (rethash == NULL)
		rethash = hash;

	memset(rethash, 0, NSEC3_MAX_HASH_LENGTH);

	dns_fixedname_init(&fixed);
	downcased = dns_fixedname_name(&fixed);
	dns_name_downcase(name, downcased, NULL);

	/* hash the node name */
	len = isc_iterated_hash(rethash, hashalg, iterations,
				salt, (int)saltlength,
				downcased->ndata, downcased->length);
	if (len == 0U)
		return (DNS_R_BADALG);

	if (hash_length != NULL)
		*hash_length = len;

	/* convert the hash to base32hex non-padded */
	region.base = rethash;
	region.length = (unsigned int)len;
	isc_buffer_init(&namebuffer, nametext, sizeof nametext);
	isc_base32hexnp_totext(&region, 1, "", &namebuffer);

	/* convert the hex to a domain name */
	dns_fixedname_init(result);
	return (dns_name_fromtext(dns_fixedname_name(result), &namebuffer,
				  origin, 0, NULL));
}

unsigned int
dns_nsec3_hashlength(dns_hash_t hash) {

	switch (hash) {
	case dns_hash_sha1:
		return(ISC_SHA1_DIGESTLENGTH);
	}
	return (0);
}

isc_boolean_t
dns_nsec3_supportedhash(dns_hash_t hash) {
	switch (hash) {
	case dns_hash_sha1:
		return (ISC_TRUE);
	}
	return (ISC_FALSE);
}

/*%
 * Update a single RR in version 'ver' of 'db' and log the
 * update in 'diff'.
 *
 * Ensures:
 * \li  '*tuple' == NULL.  Either the tuple is freed, or its
 *      ownership has been transferred to the diff.
 */
static isc_result_t
do_one_tuple(dns_difftuple_t **tuple, dns_db_t *db, dns_dbversion_t *ver,
	     dns_diff_t *diff)
{
	dns_diff_t temp_diff;
	isc_result_t result;

	/*
	 * Create a singleton diff.
	 */
	dns_diff_init(diff->mctx, &temp_diff);
	ISC_LIST_APPEND(temp_diff.tuples, *tuple, link);

	/*
	 * Apply it to the database.
	 */
	result = dns_diff_apply(&temp_diff, db, ver);
	ISC_LIST_UNLINK(temp_diff.tuples, *tuple, link);
	if (result != ISC_R_SUCCESS) {
		dns_difftuple_free(tuple);
		return (result);
	}

	/*
	 * Merge it into the current pending journal entry.
	 */
	dns_diff_appendminimal(diff, tuple);

	/*
	 * Do not clear temp_diff.
	 */
	return (ISC_R_SUCCESS);
}

/*%
 * Set '*exists' to true iff the given name exists, to false otherwise.
 */
static isc_result_t
name_exists(dns_db_t *db, dns_dbversion_t *version, dns_name_t *name,
	    isc_boolean_t *exists)
{
	isc_result_t result;
	dns_dbnode_t *node = NULL;
	dns_rdatasetiter_t *iter = NULL;

	result = dns_db_findnode(db, name, ISC_FALSE, &node);
	if (result == ISC_R_NOTFOUND) {
		*exists = ISC_FALSE;
		return (ISC_R_SUCCESS);
	}
	if (result != ISC_R_SUCCESS)
		return (result);

	result = dns_db_allrdatasets(db, node, version,
				     (isc_stdtime_t) 0, &iter);
	if (result != ISC_R_SUCCESS)
		goto cleanup_node;

	result = dns_rdatasetiter_first(iter);
	if (result == ISC_R_SUCCESS) {
		*exists = ISC_TRUE;
	} else if (result == ISC_R_NOMORE) {
		*exists = ISC_FALSE;
		result = ISC_R_SUCCESS;
	} else
		*exists = ISC_FALSE;
	dns_rdatasetiter_destroy(&iter);

 cleanup_node:
	dns_db_detachnode(db, &node);
	return (result);
}

static isc_boolean_t
match_nsec3param(const dns_rdata_nsec3_t *nsec3,
		 const dns_rdata_nsec3param_t *nsec3param)
{
	if (nsec3->hash == nsec3param->hash &&
	    nsec3->iterations == nsec3param->iterations &&
	    nsec3->salt_length == nsec3param->salt_length &&
	    !memcmp(nsec3->salt, nsec3param->salt, nsec3->salt_length))
		return (ISC_TRUE);
	return (ISC_FALSE);
}

/*%
 * Delete NSEC3 records at "name" which match "param", recording the
 * change in "diff".
 */
static isc_result_t
delete(dns_db_t *db, dns_dbversion_t *version, dns_name_t *name,
       const dns_rdata_nsec3param_t *nsec3param, dns_diff_t *diff)
{
	dns_dbnode_t *node = NULL ;
	dns_difftuple_t *tuple = NULL;
	dns_rdata_nsec3_t nsec3;
	dns_rdataset_t rdataset;
	isc_result_t result;

	result = dns_db_findnsec3node(db, name, ISC_FALSE, &node);
	if (result == ISC_R_NOTFOUND)
		return (ISC_R_SUCCESS);
	if (result != ISC_R_SUCCESS)
		return (result);

	dns_rdataset_init(&rdataset);
	result = dns_db_findrdataset(db, node, version, dns_rdatatype_nsec3, 0,
				     (isc_stdtime_t) 0, &rdataset, NULL);

	if (result == ISC_R_NOTFOUND) {
		result = ISC_R_SUCCESS;
		goto cleanup_node;
	}
	if (result != ISC_R_SUCCESS)
		goto cleanup_node;

	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset))
	{
		dns_rdata_t rdata = DNS_RDATA_INIT;
		dns_rdataset_current(&rdataset, &rdata);
		CHECK(dns_rdata_tostruct(&rdata, &nsec3, NULL));

		if (!match_nsec3param(&nsec3, nsec3param))
			continue;

		result = dns_difftuple_create(diff->mctx, DNS_DIFFOP_DEL, name,
					      rdataset.ttl, &rdata, &tuple);
		if (result != ISC_R_SUCCESS)
			goto failure;
		result = do_one_tuple(&tuple, db, version, diff);
		if (result != ISC_R_SUCCESS)
			goto failure;
	}
	if (result != ISC_R_NOMORE)
		goto failure;
	result = ISC_R_SUCCESS;

 failure:
	dns_rdataset_disassociate(&rdataset);
 cleanup_node:
	dns_db_detachnode(db, &node);

	return (result);
}

static isc_boolean_t
better_param(dns_rdataset_t *nsec3paramset, dns_rdata_t *param) {
	dns_rdataset_t rdataset;
	isc_result_t result;

	if (REMOVE(param->data[1]))
		return (ISC_TRUE);

	dns_rdataset_init(&rdataset);
	dns_rdataset_clone(nsec3paramset, &rdataset);
	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		dns_rdata_t rdata =  DNS_RDATA_INIT;
		unsigned char buf[DNS_NSEC3PARAM_BUFFERSIZE];

		if (rdataset.type != dns_rdatatype_nsec3param) {
			dns_rdata_t tmprdata =  DNS_RDATA_INIT;
			dns_rdataset_current(&rdataset, &tmprdata);
			if (!dns_nsec3param_fromprivate(&tmprdata, &rdata,
							buf, sizeof(buf)))
				continue;
		} else
			dns_rdataset_current(&rdataset, &rdata);

		if (rdata.length != param->length)
			continue;
		if (rdata.data[0] != param->data[0] ||
		    REMOVE(rdata.data[1]) ||
		    rdata.data[2] != param->data[2] ||
		    rdata.data[3] != param->data[3] ||
		    rdata.data[4] != param->data[4] ||
		    memcmp(&rdata.data[5], &param->data[5], param->data[4]))
			continue;
		if (CREATE(rdata.data[1]) && !CREATE(param->data[1])) {
			dns_rdataset_disassociate(&rdataset);
			return (ISC_TRUE);
		}
	}
	dns_rdataset_disassociate(&rdataset);
	return (ISC_FALSE);
}

static isc_result_t
find_nsec3(dns_rdata_nsec3_t *nsec3, dns_rdataset_t *rdataset,
	   const dns_rdata_nsec3param_t *nsec3param)
{
	isc_result_t result;
	for (result = dns_rdataset_first(rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(rdataset)) {
		dns_rdata_t rdata = DNS_RDATA_INIT;

		dns_rdataset_current(rdataset, &rdata);
		CHECK(dns_rdata_tostruct(&rdata, nsec3, NULL));
		dns_rdata_reset(&rdata);
		if (match_nsec3param(nsec3, nsec3param))
			break;
	}
 failure:
	return (result);
}

isc_result_t
dns_nsec3_addnsec3(dns_db_t *db, dns_dbversion_t *version,
		   dns_name_t *name, const dns_rdata_nsec3param_t *nsec3param,
		   dns_ttl_t nsecttl, isc_boolean_t unsecure, dns_diff_t *diff)
{
	dns_dbiterator_t *dbit = NULL;
	dns_dbnode_t *node = NULL;
	dns_dbnode_t *newnode = NULL;
	dns_difftuple_t *tuple = NULL;
	dns_fixedname_t fixed;
	dns_fixedname_t fprev;
	dns_hash_t hash;
	dns_name_t *hashname;
	dns_name_t *origin;
	dns_name_t *prev;
	dns_name_t empty;
	dns_rdata_nsec3_t nsec3;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdataset_t rdataset;
	int pass;
	isc_boolean_t exists = ISC_FALSE;
	isc_boolean_t maybe_remove_unsecure = ISC_FALSE;
	isc_uint8_t flags;
	isc_buffer_t buffer;
	isc_result_t result;
	unsigned char *old_next;
	unsigned char *salt;
	unsigned char nexthash[NSEC3_MAX_HASH_LENGTH];
	unsigned char nsec3buf[DNS_NSEC3_BUFFERSIZE];
	unsigned int iterations;
	unsigned int labels;
	size_t next_length;
	unsigned int old_length;
	unsigned int salt_length;

	dns_fixedname_init(&fixed);
	hashname = dns_fixedname_name(&fixed);
	dns_fixedname_init(&fprev);
	prev = dns_fixedname_name(&fprev);

	dns_rdataset_init(&rdataset);

	origin = dns_db_origin(db);

	/*
	 * Chain parameters.
	 */
	hash = nsec3param->hash;
	iterations = nsec3param->iterations;
	salt_length = nsec3param->salt_length;
	salt = nsec3param->salt;

	/*
	 * Default flags for a new chain.
	 */
	flags = nsec3param->flags & DNS_NSEC3FLAG_OPTOUT;

	/*
	 * If this is the first NSEC3 in the chain nexthash will
	 * remain pointing to itself.
	 */
	next_length = sizeof(nexthash);
	CHECK(dns_nsec3_hashname(&fixed, nexthash, &next_length,
				 name, origin, hash, iterations,
				 salt, salt_length));
	INSIST(next_length <= sizeof(nexthash));

	/*
	 * Create the node if it doesn't exist and hold
	 * a reference to it until we have added the NSEC3.
	 */
	CHECK(dns_db_findnsec3node(db, hashname, ISC_TRUE, &newnode));

	/*
	 * Seek the iterator to the 'newnode'.
	 */
	CHECK(dns_db_createiterator(db, DNS_DB_NSEC3ONLY, &dbit));
	CHECK(dns_dbiterator_seek(dbit, hashname));
	CHECK(dns_dbiterator_pause(dbit));
	result = dns_db_findrdataset(db, newnode, version, dns_rdatatype_nsec3,
				     0, (isc_stdtime_t) 0, &rdataset, NULL);
	/*
	 * If we updating a existing NSEC3 then find its
	 * next field.
	 */
	if (result == ISC_R_SUCCESS) {
		result = find_nsec3(&nsec3, &rdataset, nsec3param);
		if (result == ISC_R_SUCCESS) {
			if (!CREATE(nsec3param->flags))
				flags = nsec3.flags;
			next_length = nsec3.next_length;
			INSIST(next_length <= sizeof(nexthash));
			memmove(nexthash, nsec3.next, next_length);
			dns_rdataset_disassociate(&rdataset);
			/*
			 * If the NSEC3 is not for a unsecure delegation then
			 * we are just updating it.  If it is for a unsecure
			 * delegation then we need find out if we need to
			 * remove the NSEC3 record or not by examining the
			 * previous NSEC3 record.
			 */
			if (!unsecure)
				goto addnsec3;
			else if (CREATE(nsec3param->flags) && OPTOUT(flags)) {
				result = dns_nsec3_delnsec3(db, version, name,
							    nsec3param, diff);
				goto failure;
			} else
				maybe_remove_unsecure = ISC_TRUE;
		} else {
			dns_rdataset_disassociate(&rdataset);
			if (result != ISC_R_NOMORE)
				goto failure;
		}
	}

	/*
	 * Find the previous NSEC3 (if any) and update it if required.
	 */
	pass = 0;
	do {
		result = dns_dbiterator_prev(dbit);
		if (result == ISC_R_NOMORE) {
			pass++;
			CHECK(dns_dbiterator_last(dbit));
		}
		CHECK(dns_dbiterator_current(dbit, &node, prev));
		CHECK(dns_dbiterator_pause(dbit));
		result = dns_db_findrdataset(db, node, version,
					     dns_rdatatype_nsec3, 0,
					     (isc_stdtime_t) 0, &rdataset,
					     NULL);
		dns_db_detachnode(db, &node);
		if (result != ISC_R_SUCCESS)
			continue;

		result = find_nsec3(&nsec3, &rdataset, nsec3param);
		if (result == ISC_R_NOMORE) {
			dns_rdataset_disassociate(&rdataset);
			continue;
		}
		if (result != ISC_R_SUCCESS)
			goto failure;

		if (maybe_remove_unsecure) {
			dns_rdataset_disassociate(&rdataset);
			/*
			 * If we have OPTOUT set in the previous NSEC3 record
			 * we actually need to delete the NSEC3 record.
			 * Otherwise we just need to replace the NSEC3 record.
			 */
			if (OPTOUT(nsec3.flags)) {
				result = dns_nsec3_delnsec3(db, version, name,
							    nsec3param, diff);
				goto failure;
			}
			goto addnsec3;
		} else {
			/*
			 * Is this is a unsecure delegation we are adding?
			 * If so no change is required.
			 */
			if (OPTOUT(nsec3.flags) && unsecure) {
				dns_rdataset_disassociate(&rdataset);
				goto failure;
			}
		}

		old_next = nsec3.next;
		old_length = nsec3.next_length;

		/*
		 * Delete the old previous NSEC3.
		 */
		CHECK(delete(db, version, prev, nsec3param, diff));

		/*
		 * Fixup the previous NSEC3.
		 */
		nsec3.next = nexthash;
		nsec3.next_length = (unsigned char)next_length;
		isc_buffer_init(&buffer, nsec3buf, sizeof(nsec3buf));
		CHECK(dns_rdata_fromstruct(&rdata, rdataset.rdclass,
					   dns_rdatatype_nsec3, &nsec3,
					   &buffer));
		CHECK(dns_difftuple_create(diff->mctx, DNS_DIFFOP_ADD, prev,
					   rdataset.ttl, &rdata, &tuple));
		CHECK(do_one_tuple(&tuple, db, version, diff));
		INSIST(old_length <= sizeof(nexthash));
		memmove(nexthash, old_next, old_length);
		if (!CREATE(nsec3param->flags))
			flags = nsec3.flags;
		dns_rdata_reset(&rdata);
		dns_rdataset_disassociate(&rdataset);
		break;
	} while (pass < 2);

 addnsec3:
	/*
	 * Create the NSEC3 RDATA.
	 */
	CHECK(dns_db_findnode(db, name, ISC_FALSE, &node));
	CHECK(dns_nsec3_buildrdata(db, version, node, hash, flags, iterations,
				   salt, salt_length, nexthash, next_length,
				   nsec3buf, &rdata));
	dns_db_detachnode(db, &node);

	/*
	 * Delete the old NSEC3 and record the change.
	 */
	CHECK(delete(db, version, hashname, nsec3param, diff));
	/*
	 * Add the new NSEC3 and record the change.
	 */
	CHECK(dns_difftuple_create(diff->mctx, DNS_DIFFOP_ADD,
				   hashname, nsecttl, &rdata, &tuple));
	CHECK(do_one_tuple(&tuple, db, version, diff));
	INSIST(tuple == NULL);
	dns_rdata_reset(&rdata);
	dns_db_detachnode(db, &newnode);

	/*
	 * Add missing NSEC3 records for empty nodes
	 */
	dns_name_init(&empty, NULL);
	dns_name_clone(name, &empty);
	do {
		labels = dns_name_countlabels(&empty) - 1;
		if (labels <= dns_name_countlabels(origin))
			break;
		dns_name_getlabelsequence(&empty, 1, labels, &empty);
		CHECK(name_exists(db, version, &empty, &exists));
		if (exists)
			break;
		CHECK(dns_nsec3_hashname(&fixed, nexthash, &next_length,
					 &empty, origin, hash, iterations,
					 salt, salt_length));

		/*
		 * Create the node if it doesn't exist and hold
		 * a reference to it until we have added the NSEC3
		 * or we discover we don't need to add make a change.
		 */
		CHECK(dns_db_findnsec3node(db, hashname, ISC_TRUE, &newnode));
		result = dns_db_findrdataset(db, newnode, version,
					     dns_rdatatype_nsec3, 0,
					     (isc_stdtime_t) 0, &rdataset,
					     NULL);
		if (result == ISC_R_SUCCESS) {
			result = find_nsec3(&nsec3, &rdataset, nsec3param);
			dns_rdataset_disassociate(&rdataset);
			if (result == ISC_R_SUCCESS) {
				dns_db_detachnode(db, &newnode);
				break;
			}
			if (result != ISC_R_NOMORE)
				goto failure;
		}

		/*
		 * Find the previous NSEC3 and update it.
		 */
		CHECK(dns_dbiterator_seek(dbit, hashname));
		pass = 0;
		do {
			result = dns_dbiterator_prev(dbit);
			if (result == ISC_R_NOMORE) {
				pass++;
				CHECK(dns_dbiterator_last(dbit));
			}
			CHECK(dns_dbiterator_current(dbit, &node, prev));
			CHECK(dns_dbiterator_pause(dbit));
			result = dns_db_findrdataset(db, node, version,
						     dns_rdatatype_nsec3, 0,
						     (isc_stdtime_t) 0,
						     &rdataset, NULL);
			dns_db_detachnode(db, &node);
			if (result != ISC_R_SUCCESS)
				continue;
			result = find_nsec3(&nsec3, &rdataset, nsec3param);
			if (result == ISC_R_NOMORE) {
				dns_rdataset_disassociate(&rdataset);
				continue;
			}
			if (result != ISC_R_SUCCESS)
				goto failure;

			old_next = nsec3.next;
			old_length = nsec3.next_length;

			/*
			 * Delete the old previous NSEC3.
			 */
			CHECK(delete(db, version, prev, nsec3param, diff));

			/*
			 * Fixup the previous NSEC3.
			 */
			nsec3.next = nexthash;
			nsec3.next_length = (unsigned char)next_length;
			isc_buffer_init(&buffer, nsec3buf,
					sizeof(nsec3buf));
			CHECK(dns_rdata_fromstruct(&rdata, rdataset.rdclass,
						   dns_rdatatype_nsec3, &nsec3,
						   &buffer));
			CHECK(dns_difftuple_create(diff->mctx, DNS_DIFFOP_ADD,
						   prev, rdataset.ttl, &rdata,
						   &tuple));
			CHECK(do_one_tuple(&tuple, db, version, diff));
			INSIST(old_length <= sizeof(nexthash));
			memmove(nexthash, old_next, old_length);
			if (!CREATE(nsec3param->flags))
				flags = nsec3.flags;
			dns_rdata_reset(&rdata);
			dns_rdataset_disassociate(&rdataset);
			break;
		} while (pass < 2);

		INSIST(pass < 2);

		/*
		 * Create the NSEC3 RDATA for the empty node.
		 */
		CHECK(dns_nsec3_buildrdata(db, version, NULL, hash, flags,
					   iterations, salt, salt_length,
					   nexthash, next_length, nsec3buf,
					   &rdata));
		/*
		 * Delete the old NSEC3 and record the change.
		 */
		CHECK(delete(db, version, hashname, nsec3param, diff));

		/*
		 * Add the new NSEC3 and record the change.
		 */
		CHECK(dns_difftuple_create(diff->mctx, DNS_DIFFOP_ADD,
					   hashname, nsecttl, &rdata, &tuple));
		CHECK(do_one_tuple(&tuple, db, version, diff));
		INSIST(tuple == NULL);
		dns_rdata_reset(&rdata);
		dns_db_detachnode(db, &newnode);
	} while (1);

	/* result cannot be ISC_R_NOMORE here */
	INSIST(result != ISC_R_NOMORE);

 failure:
	if (dbit != NULL)
		dns_dbiterator_destroy(&dbit);
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	if (node != NULL)
		dns_db_detachnode(db, &node);
	if (newnode != NULL)
		dns_db_detachnode(db, &newnode);
	return (result);
}

/*%
 * Add NSEC3 records for "name", recording the change in "diff".
 * The existing NSEC3 records are removed.
 */
isc_result_t
dns_nsec3_addnsec3s(dns_db_t *db, dns_dbversion_t *version,
		    dns_name_t *name, dns_ttl_t nsecttl,
		    isc_boolean_t unsecure, dns_diff_t *diff)
{
	dns_dbnode_t *node = NULL;
	dns_rdata_nsec3param_t nsec3param;
	dns_rdataset_t rdataset;
	isc_result_t result;

	dns_rdataset_init(&rdataset);

	/*
	 * Find the NSEC3 parameters for this zone.
	 */
	result = dns_db_getoriginnode(db, &node);
	if (result != ISC_R_SUCCESS)
		return (result);

	result = dns_db_findrdataset(db, node, version,
				     dns_rdatatype_nsec3param, 0, 0,
				     &rdataset, NULL);
	dns_db_detachnode(db, &node);
	if (result == ISC_R_NOTFOUND)
		return (ISC_R_SUCCESS);
	if (result != ISC_R_SUCCESS)
		return (result);

	/*
	 * Update each active NSEC3 chain.
	 */
	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		dns_rdata_t rdata = DNS_RDATA_INIT;

		dns_rdataset_current(&rdataset, &rdata);
		CHECK(dns_rdata_tostruct(&rdata, &nsec3param, NULL));

		if (nsec3param.flags != 0)
			continue;
		/*
		 * We have a active chain.  Update it.
		 */
		CHECK(dns_nsec3_addnsec3(db, version, name, &nsec3param,
					 nsecttl, unsecure, diff));
	}
	if (result == ISC_R_NOMORE)
		result = ISC_R_SUCCESS;

 failure:
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	if (node != NULL)
		dns_db_detachnode(db, &node);

	return (result);
}

isc_boolean_t
dns_nsec3param_fromprivate(dns_rdata_t *src, dns_rdata_t *target,
			   unsigned char *buf, size_t buflen)
{
	dns_decompress_t dctx;
	isc_result_t result;
	isc_buffer_t buf1;
	isc_buffer_t buf2;

	/*
	 * Algorithm 0 (reserved by RFC 4034) is used to identify
	 * NSEC3PARAM records from DNSKEY pointers.
	 */
	if (src->length < 1 || src->data[0] != 0)
		return (ISC_FALSE);

	isc_buffer_init(&buf1, src->data + 1, src->length - 1);
	isc_buffer_add(&buf1, src->length - 1);
	isc_buffer_setactive(&buf1, src->length - 1);
	isc_buffer_init(&buf2, buf, (unsigned int)buflen);
	dns_decompress_init(&dctx, -1, DNS_DECOMPRESS_NONE);
	result = dns_rdata_fromwire(target, src->rdclass,
				    dns_rdatatype_nsec3param,
				    &buf1, &dctx, 0, &buf2);
	dns_decompress_invalidate(&dctx);

	return (ISC_TF(result == ISC_R_SUCCESS));
}

void
dns_nsec3param_toprivate(dns_rdata_t *src, dns_rdata_t *target,
			 dns_rdatatype_t privatetype,
			 unsigned char *buf, size_t buflen)
{
	REQUIRE(buflen >= src->length + 1);

	REQUIRE(DNS_RDATA_INITIALIZED(target));

	memmove(buf + 1, src->data, src->length);
	buf[0] = 0;
	target->data = buf;
	target->length = src->length + 1;
	target->type = privatetype;
	target->rdclass = src->rdclass;
	target->flags = 0;
	ISC_LINK_INIT(target, link);
}

#ifdef BIND9
static isc_result_t
rr_exists(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *name,
	  const dns_rdata_t *rdata, isc_boolean_t *flag)
{
	dns_rdataset_t rdataset;
	dns_dbnode_t *node = NULL;
	isc_result_t result;

	dns_rdataset_init(&rdataset);
	if (rdata->type == dns_rdatatype_nsec3)
		CHECK(dns_db_findnsec3node(db, name, ISC_FALSE, &node));
	else
		CHECK(dns_db_findnode(db, name, ISC_FALSE, &node));
	result = dns_db_findrdataset(db, node, ver, rdata->type, 0,
				     (isc_stdtime_t) 0, &rdataset, NULL);
	if (result == ISC_R_NOTFOUND) {
		*flag = ISC_FALSE;
		result = ISC_R_SUCCESS;
		goto failure;
	}

	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		dns_rdata_t myrdata = DNS_RDATA_INIT;
		dns_rdataset_current(&rdataset, &myrdata);
		if (!dns_rdata_casecompare(&myrdata, rdata))
			break;
	}
	dns_rdataset_disassociate(&rdataset);
	if (result == ISC_R_SUCCESS) {
		*flag = ISC_TRUE;
	} else if (result == ISC_R_NOMORE) {
		*flag = ISC_FALSE;
		result = ISC_R_SUCCESS;
	}

 failure:
	if (node != NULL)
		dns_db_detachnode(db, &node);
	return (result);
}
#endif

#ifdef BIND9
isc_result_t
dns_nsec3param_deletechains(dns_db_t *db, dns_dbversion_t *ver,
			    dns_zone_t *zone, isc_boolean_t nonsec,
			    dns_diff_t *diff)
{
	dns_dbnode_t *node = NULL;
	dns_difftuple_t *tuple = NULL;
	dns_name_t next;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdataset_t rdataset;
	isc_boolean_t flag;
	isc_result_t result = ISC_R_SUCCESS;
	unsigned char buf[DNS_NSEC3PARAM_BUFFERSIZE + 1];
	dns_name_t *origin = dns_zone_getorigin(zone);
	dns_rdatatype_t privatetype = dns_zone_getprivatetype(zone);

	dns_name_init(&next, NULL);
	dns_rdataset_init(&rdataset);

	result = dns_db_getoriginnode(db, &node);
	if (result != ISC_R_SUCCESS)
		return (result);

	/*
	 * Cause all NSEC3 chains to be deleted.
	 */
	result = dns_db_findrdataset(db, node, ver, dns_rdatatype_nsec3param,
				     0, (isc_stdtime_t) 0, &rdataset, NULL);
	if (result == ISC_R_NOTFOUND)
		goto try_private;
	if (result != ISC_R_SUCCESS)
		goto failure;

	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		dns_rdata_t private = DNS_RDATA_INIT;

		dns_rdataset_current(&rdataset, &rdata);

		CHECK(dns_difftuple_create(diff->mctx, DNS_DIFFOP_DEL, origin,
					   rdataset.ttl, &rdata, &tuple));
		CHECK(do_one_tuple(&tuple, db, ver, diff));
		INSIST(tuple == NULL);

		dns_nsec3param_toprivate(&rdata, &private, privatetype,
					 buf, sizeof(buf));
		buf[2] = DNS_NSEC3FLAG_REMOVE;
		if (nonsec)
			buf[2] |= DNS_NSEC3FLAG_NONSEC;

		CHECK(rr_exists(db, ver, origin, &private, &flag));

		if (!flag) {
			CHECK(dns_difftuple_create(diff->mctx, DNS_DIFFOP_ADD,
						   origin, 0, &private,
						   &tuple));
			CHECK(do_one_tuple(&tuple, db, ver, diff));
			INSIST(tuple == NULL);
		}
		dns_rdata_reset(&rdata);
	}
	if (result != ISC_R_NOMORE)
		goto failure;

	dns_rdataset_disassociate(&rdataset);

 try_private:
	if (privatetype == 0)
		goto success;
	result = dns_db_findrdataset(db, node, ver, privatetype, 0,
				     (isc_stdtime_t) 0, &rdataset, NULL);
	if (result == ISC_R_NOTFOUND)
		goto success;
	if (result != ISC_R_SUCCESS)
		goto failure;

	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		dns_rdata_reset(&rdata);
		dns_rdataset_current(&rdataset, &rdata);
		INSIST(rdata.length <= sizeof(buf));
		memmove(buf, rdata.data, rdata.length);

		/*
		 * Private NSEC3 record length >= 6.
		 * <0(1), hash(1), flags(1), iterations(2), saltlen(1)>
		 */
		if (rdata.length < 6 || buf[0] != 0 ||
		    (buf[2] & DNS_NSEC3FLAG_REMOVE) != 0 ||
		    (nonsec && (buf[2] & DNS_NSEC3FLAG_NONSEC) != 0))
			continue;

		CHECK(dns_difftuple_create(diff->mctx, DNS_DIFFOP_DEL, origin,
					   0, &rdata, &tuple));
		CHECK(do_one_tuple(&tuple, db, ver, diff));
		INSIST(tuple == NULL);

		rdata.data = buf;
		buf[2] = DNS_NSEC3FLAG_REMOVE;
		if (nonsec)
			buf[2] |= DNS_NSEC3FLAG_NONSEC;

		CHECK(rr_exists(db, ver, origin, &rdata, &flag));

		if (!flag) {
			CHECK(dns_difftuple_create(diff->mctx, DNS_DIFFOP_ADD,
						   origin, 0, &rdata, &tuple));
			CHECK(do_one_tuple(&tuple, db, ver, diff));
			INSIST(tuple == NULL);
		}
	}
	if (result != ISC_R_NOMORE)
		goto failure;
 success:
	result = ISC_R_SUCCESS;

 failure:
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	dns_db_detachnode(db, &node);
	return (result);
}
#endif

isc_result_t
dns_nsec3_addnsec3sx(dns_db_t *db, dns_dbversion_t *version,
		     dns_name_t *name, dns_ttl_t nsecttl,
		     isc_boolean_t unsecure, dns_rdatatype_t type,
		     dns_diff_t *diff)
{
	dns_dbnode_t *node = NULL;
	dns_rdata_nsec3param_t nsec3param;
	dns_rdataset_t rdataset;
	dns_rdataset_t prdataset;
	isc_result_t result;

	dns_rdataset_init(&rdataset);
	dns_rdataset_init(&prdataset);

	/*
	 * Find the NSEC3 parameters for this zone.
	 */
	result = dns_db_getoriginnode(db, &node);
	if (result != ISC_R_SUCCESS)
		return (result);

	result = dns_db_findrdataset(db, node, version, type, 0, 0,
				     &prdataset, NULL);
	if (result != ISC_R_SUCCESS && result != ISC_R_NOTFOUND)
		goto failure;

	result = dns_db_findrdataset(db, node, version,
				     dns_rdatatype_nsec3param, 0, 0,
				     &rdataset, NULL);
	if (result == ISC_R_NOTFOUND)
		goto try_private;
	if (result != ISC_R_SUCCESS)
		goto failure;

	/*
	 * Update each active NSEC3 chain.
	 */
	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		dns_rdata_t rdata = DNS_RDATA_INIT;

		dns_rdataset_current(&rdataset, &rdata);
		CHECK(dns_rdata_tostruct(&rdata, &nsec3param, NULL));

		if (nsec3param.flags != 0)
			continue;

		/*
		 * We have a active chain.  Update it.
		 */
		CHECK(dns_nsec3_addnsec3(db, version, name, &nsec3param,
					 nsecttl, unsecure, diff));
	}
	if (result != ISC_R_NOMORE)
		goto failure;

	dns_rdataset_disassociate(&rdataset);

 try_private:
	if (!dns_rdataset_isassociated(&prdataset))
		goto success;
	/*
	 * Update each active NSEC3 chain.
	 */
	for (result = dns_rdataset_first(&prdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&prdataset)) {
		dns_rdata_t rdata1 = DNS_RDATA_INIT;
		dns_rdata_t rdata2 = DNS_RDATA_INIT;
		unsigned char buf[DNS_NSEC3PARAM_BUFFERSIZE];

		dns_rdataset_current(&prdataset, &rdata1);
		if (!dns_nsec3param_fromprivate(&rdata1, &rdata2,
						buf, sizeof(buf)))
			continue;
		CHECK(dns_rdata_tostruct(&rdata2, &nsec3param, NULL));

		if ((nsec3param.flags & DNS_NSEC3FLAG_REMOVE) != 0)
			continue;
		if (better_param(&prdataset, &rdata2))
			continue;

		/*
		 * We have a active chain.  Update it.
		 */
		CHECK(dns_nsec3_addnsec3(db, version, name, &nsec3param,
					 nsecttl, unsecure, diff));
	}
	if (result == ISC_R_NOMORE)
 success:
		result = ISC_R_SUCCESS;
 failure:
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	if (dns_rdataset_isassociated(&prdataset))
		dns_rdataset_disassociate(&prdataset);
	if (node != NULL)
		dns_db_detachnode(db, &node);

	return (result);
}

/*%
 * Determine whether any NSEC3 records that were associated with
 * 'name' should be deleted or if they should continue to exist.
 * ISC_TRUE indicates they should be deleted.
 * ISC_FALSE indicates they should be retained.
 */
static isc_result_t
deleteit(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *name,
	 isc_boolean_t *yesno)
{
	isc_result_t result;
	dns_fixedname_t foundname;
	dns_fixedname_init(&foundname);

	result = dns_db_find(db, name, ver, dns_rdatatype_any,
			     DNS_DBFIND_GLUEOK | DNS_DBFIND_NOWILD,
			     (isc_stdtime_t) 0, NULL,
			     dns_fixedname_name(&foundname),
			     NULL, NULL);
	if (result == DNS_R_EMPTYNAME || result == ISC_R_SUCCESS ||
	    result ==  DNS_R_ZONECUT) {
		*yesno = ISC_FALSE;
		return (ISC_R_SUCCESS);
	}
	if (result == DNS_R_GLUE || result == DNS_R_DNAME ||
	    result == DNS_R_DELEGATION || result == DNS_R_NXDOMAIN) {
		*yesno = ISC_TRUE;
		return (ISC_R_SUCCESS);
	}
	/*
	 * Silence compiler.
	 */
	*yesno = ISC_TRUE;
	return (result);
}

isc_result_t
dns_nsec3_delnsec3(dns_db_t *db, dns_dbversion_t *version, dns_name_t *name,
		   const dns_rdata_nsec3param_t *nsec3param, dns_diff_t *diff)
{
	dns_dbiterator_t *dbit = NULL;
	dns_dbnode_t *node = NULL;
	dns_difftuple_t *tuple = NULL;
	dns_fixedname_t fixed;
	dns_fixedname_t fprev;
	dns_hash_t hash;
	dns_name_t *hashname;
	dns_name_t *origin;
	dns_name_t *prev;
	dns_name_t empty;
	dns_rdata_nsec3_t nsec3;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdataset_t rdataset;
	int pass;
	isc_boolean_t yesno;
	isc_buffer_t buffer;
	isc_result_t result;
	unsigned char *salt;
	unsigned char nexthash[NSEC3_MAX_HASH_LENGTH];
	unsigned char nsec3buf[DNS_NSEC3_BUFFERSIZE];
	unsigned int iterations;
	unsigned int labels;
	size_t next_length;
	unsigned int salt_length;

	dns_fixedname_init(&fixed);
	hashname = dns_fixedname_name(&fixed);
	dns_fixedname_init(&fprev);
	prev = dns_fixedname_name(&fprev);

	dns_rdataset_init(&rdataset);

	origin = dns_db_origin(db);

	/*
	 * Chain parameters.
	 */
	hash = nsec3param->hash;
	iterations = nsec3param->iterations;
	salt_length = nsec3param->salt_length;
	salt = nsec3param->salt;

	/*
	 * If this is the first NSEC3 in the chain nexthash will
	 * remain pointing to itself.
	 */
	next_length = sizeof(nexthash);
	CHECK(dns_nsec3_hashname(&fixed, nexthash, &next_length,
				 name, origin, hash, iterations,
				 salt, salt_length));

	CHECK(dns_db_createiterator(db, DNS_DB_NSEC3ONLY, &dbit));

	result = dns_dbiterator_seek(dbit, hashname);
	if (result == ISC_R_NOTFOUND)
		goto success;
	if (result != ISC_R_SUCCESS)
		goto failure;

	CHECK(dns_dbiterator_current(dbit, &node, NULL));
	CHECK(dns_dbiterator_pause(dbit));
	result = dns_db_findrdataset(db, node, version, dns_rdatatype_nsec3,
				     0, (isc_stdtime_t) 0, &rdataset, NULL);
	dns_db_detachnode(db, &node);
	if (result == ISC_R_NOTFOUND)
		goto success;
	if (result != ISC_R_SUCCESS)
		goto failure;

	/*
	 * If we find a existing NSEC3 for this chain then save the
	 * next field.
	 */
	result = find_nsec3(&nsec3, &rdataset, nsec3param);
	if (result == ISC_R_SUCCESS) {
		next_length = nsec3.next_length;
		INSIST(next_length <= sizeof(nexthash));
		memmove(nexthash, nsec3.next, next_length);
	}
	dns_rdataset_disassociate(&rdataset);
	if (result == ISC_R_NOMORE)
		goto success;
	if (result != ISC_R_SUCCESS)
		goto failure;

	/*
	 * Find the previous NSEC3 and update it.
	 */
	pass = 0;
	do {
		result = dns_dbiterator_prev(dbit);
		if (result == ISC_R_NOMORE) {
			pass++;
			CHECK(dns_dbiterator_last(dbit));
		}
		CHECK(dns_dbiterator_current(dbit, &node, prev));
		CHECK(dns_dbiterator_pause(dbit));
		result = dns_db_findrdataset(db, node, version,
					     dns_rdatatype_nsec3, 0,
					     (isc_stdtime_t) 0, &rdataset,
					     NULL);
		dns_db_detachnode(db, &node);
		if (result != ISC_R_SUCCESS)
			continue;
		result = find_nsec3(&nsec3, &rdataset, nsec3param);
		if (result == ISC_R_NOMORE) {
			dns_rdataset_disassociate(&rdataset);
			continue;
		}
		if (result != ISC_R_SUCCESS)
			goto failure;

		/*
		 * Delete the old previous NSEC3.
		 */
		CHECK(delete(db, version, prev, nsec3param, diff));

		/*
		 * Fixup the previous NSEC3.
		 */
		nsec3.next = nexthash;
		nsec3.next_length = (unsigned char)next_length;
		if (CREATE(nsec3param->flags))
			nsec3.flags = nsec3param->flags & DNS_NSEC3FLAG_OPTOUT;
		isc_buffer_init(&buffer, nsec3buf, sizeof(nsec3buf));
		CHECK(dns_rdata_fromstruct(&rdata, rdataset.rdclass,
					   dns_rdatatype_nsec3, &nsec3,
					   &buffer));
		CHECK(dns_difftuple_create(diff->mctx, DNS_DIFFOP_ADD, prev,
					   rdataset.ttl, &rdata, &tuple));
		CHECK(do_one_tuple(&tuple, db, version, diff));
		dns_rdata_reset(&rdata);
		dns_rdataset_disassociate(&rdataset);
		break;
	} while (pass < 2);

	/*
	 * Delete the old NSEC3 and record the change.
	 */
	CHECK(delete(db, version, hashname, nsec3param, diff));

	/*
	 *  Delete NSEC3 records for now non active nodes.
	 */
	dns_name_init(&empty, NULL);
	dns_name_clone(name, &empty);
	do {
		labels = dns_name_countlabels(&empty) - 1;
		if (labels <= dns_name_countlabels(origin))
			break;
		dns_name_getlabelsequence(&empty, 1, labels, &empty);
		CHECK(deleteit(db, version, &empty, &yesno));
		if (!yesno)
			break;

		CHECK(dns_nsec3_hashname(&fixed, nexthash, &next_length,
					 &empty, origin, hash, iterations,
					 salt, salt_length));
		result = dns_dbiterator_seek(dbit, hashname);
		if (result == ISC_R_NOTFOUND)
			goto success;
		if (result != ISC_R_SUCCESS)
			goto failure;

		CHECK(dns_dbiterator_current(dbit, &node, NULL));
		CHECK(dns_dbiterator_pause(dbit));
		result = dns_db_findrdataset(db, node, version,
					     dns_rdatatype_nsec3, 0,
					     (isc_stdtime_t) 0, &rdataset,
					     NULL);
		dns_db_detachnode(db, &node);
		if (result == ISC_R_NOTFOUND)
			goto success;
		if (result != ISC_R_SUCCESS)
			goto failure;

		result = find_nsec3(&nsec3, &rdataset, nsec3param);
		if (result == ISC_R_SUCCESS) {
			next_length = nsec3.next_length;
			INSIST(next_length <= sizeof(nexthash));
			memmove(nexthash, nsec3.next, next_length);
		}
		dns_rdataset_disassociate(&rdataset);
		if (result == ISC_R_NOMORE)
			goto success;
		if (result != ISC_R_SUCCESS)
			goto failure;

		pass = 0;
		do {
			result = dns_dbiterator_prev(dbit);
			if (result == ISC_R_NOMORE) {
				pass++;
				CHECK(dns_dbiterator_last(dbit));
			}
			CHECK(dns_dbiterator_current(dbit, &node, prev));
			CHECK(dns_dbiterator_pause(dbit));
			result = dns_db_findrdataset(db, node, version,
						     dns_rdatatype_nsec3, 0,
						     (isc_stdtime_t) 0,
						     &rdataset, NULL);
			dns_db_detachnode(db, &node);
			if (result != ISC_R_SUCCESS)
				continue;
			result = find_nsec3(&nsec3, &rdataset, nsec3param);
			if (result == ISC_R_NOMORE) {
				dns_rdataset_disassociate(&rdataset);
				continue;
			}
			if (result != ISC_R_SUCCESS)
				goto failure;

			/*
			 * Delete the old previous NSEC3.
			 */
			CHECK(delete(db, version, prev, nsec3param, diff));

			/*
			 * Fixup the previous NSEC3.
			 */
			nsec3.next = nexthash;
			nsec3.next_length = (unsigned char)next_length;
			isc_buffer_init(&buffer, nsec3buf,
					sizeof(nsec3buf));
			CHECK(dns_rdata_fromstruct(&rdata, rdataset.rdclass,
						   dns_rdatatype_nsec3, &nsec3,
						   &buffer));
			CHECK(dns_difftuple_create(diff->mctx, DNS_DIFFOP_ADD,
						   prev, rdataset.ttl, &rdata,
						   &tuple));
			CHECK(do_one_tuple(&tuple, db, version, diff));
			dns_rdata_reset(&rdata);
			dns_rdataset_disassociate(&rdataset);
			break;
		} while (pass < 2);

		INSIST(pass < 2);

		/*
		 * Delete the old NSEC3 and record the change.
		 */
		CHECK(delete(db, version, hashname, nsec3param, diff));
	} while (1);

 success:
	result = ISC_R_SUCCESS;

 failure:
	if (dbit != NULL)
		dns_dbiterator_destroy(&dbit);
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	if (node != NULL)
		dns_db_detachnode(db, &node);
	return (result);
}

isc_result_t
dns_nsec3_delnsec3s(dns_db_t *db, dns_dbversion_t *version, dns_name_t *name,
		    dns_diff_t *diff)
{
	return (dns_nsec3_delnsec3sx(db, version, name, 0, diff));
}

isc_result_t
dns_nsec3_delnsec3sx(dns_db_t *db, dns_dbversion_t *version, dns_name_t *name,
		     dns_rdatatype_t privatetype, dns_diff_t *diff)
{
	dns_dbnode_t *node = NULL;
	dns_rdata_nsec3param_t nsec3param;
	dns_rdataset_t rdataset;
	isc_result_t result;

	dns_rdataset_init(&rdataset);

	/*
	 * Find the NSEC3 parameters for this zone.
	 */
	result = dns_db_getoriginnode(db, &node);
	if (result != ISC_R_SUCCESS)
		return (result);

	result = dns_db_findrdataset(db, node, version,
				     dns_rdatatype_nsec3param, 0, 0,
				     &rdataset, NULL);
	if (result == ISC_R_NOTFOUND)
		goto try_private;
	if (result != ISC_R_SUCCESS)
		goto failure;

	/*
	 * Update each active NSEC3 chain.
	 */
	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		dns_rdata_t rdata = DNS_RDATA_INIT;

		dns_rdataset_current(&rdataset, &rdata);
		CHECK(dns_rdata_tostruct(&rdata, &nsec3param, NULL));

		if (nsec3param.flags != 0)
			continue;
		/*
		 * We have a active chain.  Update it.
		 */
		CHECK(dns_nsec3_delnsec3(db, version, name, &nsec3param, diff));
	}
	dns_rdataset_disassociate(&rdataset);

 try_private:
	if (privatetype == 0)
		goto success;
	result = dns_db_findrdataset(db, node, version, privatetype, 0, 0,
				     &rdataset, NULL);
	if (result == ISC_R_NOTFOUND)
		goto success;
	if (result != ISC_R_SUCCESS)
		goto failure;

	/*
	 * Update each NSEC3 chain being built.
	 */
	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		dns_rdata_t rdata1 = DNS_RDATA_INIT;
		dns_rdata_t rdata2 = DNS_RDATA_INIT;
		unsigned char buf[DNS_NSEC3PARAM_BUFFERSIZE];

		dns_rdataset_current(&rdataset, &rdata1);
		if (!dns_nsec3param_fromprivate(&rdata1,  &rdata2,
						buf, sizeof(buf)))
			continue;
		CHECK(dns_rdata_tostruct(&rdata2, &nsec3param, NULL));

		if ((nsec3param.flags & DNS_NSEC3FLAG_REMOVE) != 0)
			continue;
		if (better_param(&rdataset, &rdata2))
			continue;

		/*
		 * We have a active chain.  Update it.
		 */
		CHECK(dns_nsec3_delnsec3(db, version, name, &nsec3param, diff));
	}
	if (result == ISC_R_NOMORE)
 success:
		result = ISC_R_SUCCESS;

 failure:
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	if (node != NULL)
		dns_db_detachnode(db, &node);

	return (result);
}

isc_result_t
dns_nsec3_active(dns_db_t *db, dns_dbversion_t *version,
		 isc_boolean_t complete, isc_boolean_t *answer)
{
	return (dns_nsec3_activex(db, version, complete, 0, answer));
}

isc_result_t
dns_nsec3_activex(dns_db_t *db, dns_dbversion_t *version,
		  isc_boolean_t complete, dns_rdatatype_t privatetype,
		  isc_boolean_t *answer)
{
	dns_dbnode_t *node = NULL;
	dns_rdataset_t rdataset;
	dns_rdata_nsec3param_t nsec3param;
	isc_result_t result;

	REQUIRE(answer != NULL);

	dns_rdataset_init(&rdataset);

	result = dns_db_getoriginnode(db, &node);
	if (result != ISC_R_SUCCESS)
		return (result);

	result = dns_db_findrdataset(db, node, version,
				     dns_rdatatype_nsec3param, 0, 0,
				     &rdataset, NULL);

	if (result == ISC_R_NOTFOUND)
		goto try_private;

	if (result != ISC_R_SUCCESS) {
		dns_db_detachnode(db, &node);
		return (result);
	}
	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		dns_rdata_t rdata = DNS_RDATA_INIT;

		dns_rdataset_current(&rdataset, &rdata);
		result = dns_rdata_tostruct(&rdata, &nsec3param, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);

		if (nsec3param.flags == 0)
			break;
	}
	dns_rdataset_disassociate(&rdataset);
	if (result == ISC_R_SUCCESS) {
		dns_db_detachnode(db, &node);
		*answer = ISC_TRUE;
		return (ISC_R_SUCCESS);
	}
	if (result == ISC_R_NOMORE)
		*answer = ISC_FALSE;

 try_private:
	if (privatetype == 0 || complete) {
		*answer = ISC_FALSE;
		return (ISC_R_SUCCESS);
	}
	result = dns_db_findrdataset(db, node, version, privatetype, 0, 0,
				     &rdataset, NULL);

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
		dns_rdata_t rdata1 = DNS_RDATA_INIT;
		dns_rdata_t rdata2 = DNS_RDATA_INIT;
		unsigned char buf[DNS_NSEC3PARAM_BUFFERSIZE];

		dns_rdataset_current(&rdataset, &rdata1);
		if (!dns_nsec3param_fromprivate(&rdata1, &rdata2,
						buf, sizeof(buf)))
			continue;
		result = dns_rdata_tostruct(&rdata2, &nsec3param, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);

		if (!complete && CREATE(nsec3param.flags))
			break;
	}
	dns_rdataset_disassociate(&rdataset);
	if (result == ISC_R_SUCCESS) {
		*answer = ISC_TRUE;
		result = ISC_R_SUCCESS;
	}
	if (result == ISC_R_NOMORE) {
		*answer = ISC_FALSE;
		result = ISC_R_SUCCESS;
	}

	return (result);
}

isc_result_t
dns_nsec3_maxiterations(dns_db_t *db, dns_dbversion_t *version,
			isc_mem_t *mctx, unsigned int *iterationsp)
{
	dns_dbnode_t *node = NULL;
	dns_rdataset_t rdataset;
	dst_key_t *key = NULL;
	isc_buffer_t buffer;
	isc_result_t result;
	unsigned int bits, minbits = 4096;

	result = dns_db_getoriginnode(db, &node);
	if (result != ISC_R_SUCCESS)
		return (result);

	dns_rdataset_init(&rdataset);
	result = dns_db_findrdataset(db, node, version, dns_rdatatype_dnskey,
				     0, 0, &rdataset, NULL);
	dns_db_detachnode(db, &node);
	if (result == ISC_R_NOTFOUND) {
		*iterationsp = 0;
		return (ISC_R_SUCCESS);
	}
	if (result != ISC_R_SUCCESS)
		goto failure;

	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		dns_rdata_t rdata = DNS_RDATA_INIT;

		dns_rdataset_current(&rdataset, &rdata);
		isc_buffer_init(&buffer, rdata.data, rdata.length);
		isc_buffer_add(&buffer, rdata.length);
		CHECK(dst_key_fromdns(dns_db_origin(db), rdataset.rdclass,
				      &buffer, mctx, &key));
		bits = dst_key_size(key);
		dst_key_free(&key);
		if (minbits > bits)
			minbits = bits;
	}
	if (result != ISC_R_NOMORE)
		goto failure;

	if (minbits <= 1024)
		*iterationsp = 150;
	else if (minbits <= 2048)
		*iterationsp = 500;
	else
		*iterationsp = 2500;
	result = ISC_R_SUCCESS;

 failure:
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	return (result);
}

isc_result_t
dns_nsec3_noexistnodata(dns_rdatatype_t type, dns_name_t* name,
			dns_name_t *nsec3name, dns_rdataset_t *nsec3set,
			dns_name_t *zonename, isc_boolean_t *exists,
			isc_boolean_t *data, isc_boolean_t *optout,
			isc_boolean_t *unknown, isc_boolean_t *setclosest,
			isc_boolean_t *setnearest, dns_name_t *closest,
			dns_name_t *nearest, dns_nseclog_t logit, void *arg)
{
	char namebuf[DNS_NAME_FORMATSIZE];
	dns_fixedname_t fzone;
	dns_fixedname_t qfixed;
	dns_label_t hashlabel;
	dns_name_t *qname;
	dns_name_t *zone;
	dns_rdata_nsec3_t nsec3;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	int order;
	int scope;
	isc_boolean_t atparent;
	isc_boolean_t first;
	isc_boolean_t ns;
	isc_boolean_t soa;
	isc_buffer_t buffer;
	isc_result_t answer = ISC_R_IGNORE;
	isc_result_t result;
	unsigned char hash[NSEC3_MAX_HASH_LENGTH];
	unsigned char owner[NSEC3_MAX_HASH_LENGTH];
	unsigned int length;
	unsigned int qlabels;
	unsigned int zlabels;

	REQUIRE((exists == NULL && data == NULL) ||
		(exists != NULL && data != NULL));
	REQUIRE(nsec3set != NULL && nsec3set->type == dns_rdatatype_nsec3);
	REQUIRE((setclosest == NULL && closest == NULL) ||
		(setclosest != NULL && closest != NULL));
	REQUIRE((setnearest == NULL && nearest == NULL) ||
		(setnearest != NULL && nearest != NULL));

	result = dns_rdataset_first(nsec3set);
	if (result != ISC_R_SUCCESS) {
		(*logit)(arg, ISC_LOG_DEBUG(3), "failure processing NSEC3 set");
		return (result);
	}

	dns_rdataset_current(nsec3set, &rdata);

	result = dns_rdata_tostruct(&rdata, &nsec3, NULL);
	if (result != ISC_R_SUCCESS)
		return (result);

	(*logit)(arg, ISC_LOG_DEBUG(3), "looking for relevant NSEC3");

	dns_fixedname_init(&fzone);
	zone = dns_fixedname_name(&fzone);
	zlabels = dns_name_countlabels(nsec3name);

	/*
	 * NSEC3 records must have two or more labels to be valid.
	 */
	if (zlabels < 2)
		return (ISC_R_IGNORE);

	/*
	 * Strip off the NSEC3 hash to get the zone.
	 */
	zlabels--;
	dns_name_split(nsec3name, zlabels, NULL, zone);

	/*
	 * If not below the zone name we can ignore this record.
	 */
	if (!dns_name_issubdomain(name, zone))
		return (ISC_R_IGNORE);

	/*
	 * Is this zone the same or deeper than the current zone?
	 */
	if (dns_name_countlabels(zonename) == 0 ||
	    dns_name_issubdomain(zone, zonename))
		dns_name_copy(zone, zonename, NULL);

	if (!dns_name_equal(zone, zonename))
		return (ISC_R_IGNORE);

	/*
	 * Are we only looking for the most enclosing zone?
	 */
	if (exists == NULL || data == NULL)
		return (ISC_R_SUCCESS);

	/*
	 * Only set unknown once we are sure that this NSEC3 is from
	 * the deepest covering zone.
	 */
	if (!dns_nsec3_supportedhash(nsec3.hash)) {
		if (unknown != NULL)
			*unknown = ISC_TRUE;
		return (ISC_R_IGNORE);
	}

	/*
	 * Recover the hash from the first label.
	 */
	dns_name_getlabel(nsec3name, 0, &hashlabel);
	isc_region_consume(&hashlabel, 1);
	isc_buffer_init(&buffer, owner, sizeof(owner));
	result = isc_base32hex_decoderegion(&hashlabel, &buffer);
	if (result != ISC_R_SUCCESS)
		return (result);

	/*
	 * The hash lengths should match.  If not ignore the record.
	 */
	if (isc_buffer_usedlength(&buffer) != nsec3.next_length)
		return (ISC_R_IGNORE);

	/*
	 * Work out what this NSEC3 covers.
	 * Inside (<0) or outside (>=0).
	 */
	scope = isc_safe_memcompare(owner, nsec3.next, nsec3.next_length);

	/*
	 * Prepare to compute all the hashes.
	 */
	dns_fixedname_init(&qfixed);
	qname = dns_fixedname_name(&qfixed);
	dns_name_downcase(name, qname, NULL);
	qlabels = dns_name_countlabels(qname);
	first = ISC_TRUE;

	while (qlabels >= zlabels) {
		length = isc_iterated_hash(hash, nsec3.hash, nsec3.iterations,
					   nsec3.salt, nsec3.salt_length,
					   qname->ndata, qname->length);
		/*
		 * The computed hash length should match.
		 */
		if (length != nsec3.next_length) {
			(*logit)(arg, ISC_LOG_DEBUG(3),
				 "ignoring NSEC bad length %u vs %u",
				 length, nsec3.next_length);
			return (ISC_R_IGNORE);
		}

		order = isc_safe_memcompare(hash, owner, length);
		if (first && order == 0) {
			/*
			 * The hashes are the same.
			 */
			atparent = dns_rdatatype_atparent(type);
			ns = dns_nsec3_typepresent(&rdata, dns_rdatatype_ns);
			soa = dns_nsec3_typepresent(&rdata, dns_rdatatype_soa);
			if (ns && !soa) {
				if (!atparent) {
					/*
					 * This NSEC3 record is from somewhere
					 * higher in the DNS, and at the
					 * parent of a delegation. It can not
					 * be legitimately used here.
					 */
					(*logit)(arg, ISC_LOG_DEBUG(3),
						 "ignoring parent NSEC3");
					return (ISC_R_IGNORE);
				}
			} else if (atparent && ns && soa) {
				/*
				 * This NSEC3 record is from the child.
				 * It can not be legitimately used here.
				 */
				(*logit)(arg, ISC_LOG_DEBUG(3),
					 "ignoring child NSEC3");
				return (ISC_R_IGNORE);
			}
			if (type == dns_rdatatype_cname ||
			    type == dns_rdatatype_nxt ||
			    type == dns_rdatatype_nsec ||
			    type == dns_rdatatype_key ||
			    !dns_nsec3_typepresent(&rdata, dns_rdatatype_cname)) {
				*exists = ISC_TRUE;
				*data = dns_nsec3_typepresent(&rdata, type);
				(*logit)(arg, ISC_LOG_DEBUG(3),
					 "NSEC3 proves name exists (owner) "
					 "data=%d", *data);
				return (ISC_R_SUCCESS);
			}
			(*logit)(arg, ISC_LOG_DEBUG(3),
				 "NSEC3 proves CNAME exists");
			return (ISC_R_IGNORE);
		}

		if (order == 0 &&
		    dns_nsec3_typepresent(&rdata, dns_rdatatype_ns) &&
		    !dns_nsec3_typepresent(&rdata, dns_rdatatype_soa))
		{
			/*
			 * This NSEC3 record is from somewhere higher in
			 * the DNS, and at the parent of a delegation.
			 * It can not be legitimately used here.
			 */
			(*logit)(arg, ISC_LOG_DEBUG(3),
				 "ignoring parent NSEC3");
			return (ISC_R_IGNORE);
		}

		/*
		 * Potential closest encloser.
		 */
		if (order == 0) {
			if (closest != NULL &&
			    (dns_name_countlabels(closest) == 0 ||
			     dns_name_issubdomain(qname, closest)) &&
			    !dns_nsec3_typepresent(&rdata, dns_rdatatype_ds) &&
			    !dns_nsec3_typepresent(&rdata, dns_rdatatype_dname) &&
			    (dns_nsec3_typepresent(&rdata, dns_rdatatype_soa) ||
			     !dns_nsec3_typepresent(&rdata, dns_rdatatype_ns)))
			{

				dns_name_format(qname, namebuf,
						sizeof(namebuf));
				(*logit)(arg, ISC_LOG_DEBUG(3),
					 "NSEC3 indicates potential closest "
					 "encloser: '%s'", namebuf);
				dns_name_copy(qname, closest, NULL);
				*setclosest = ISC_TRUE;
			}
			dns_name_format(qname, namebuf, sizeof(namebuf));
			(*logit)(arg, ISC_LOG_DEBUG(3),
				 "NSEC3 at super-domain %s", namebuf);
			return (answer);
		}

		/*
		 * Find if the name does not exist.
		 *
		 * We continue as we need to find the name closest to the
		 * closest encloser that doesn't exist.
		 *
		 * We also need to continue to ensure that we are not
		 * proving the non-existence of a record in a sub-zone.
		 * If that would be the case we will return ISC_R_IGNORE
		 * above.
		 */
		if ((scope < 0 && order > 0 &&
		     memcmp(hash, nsec3.next, length) < 0) ||
		    (scope >= 0 && (order > 0 ||
				    memcmp(hash, nsec3.next, length) < 0)))
		{
			dns_name_format(qname, namebuf, sizeof(namebuf));
			(*logit)(arg, ISC_LOG_DEBUG(3), "NSEC3 proves "
				 "name does not exist: '%s'", namebuf);
			if (nearest != NULL &&
			    (dns_name_countlabels(nearest) == 0 ||
			     dns_name_issubdomain(nearest, qname))) {
				dns_name_copy(qname, nearest, NULL);
				*setnearest = ISC_TRUE;
			}

			*exists = ISC_FALSE;
			*data = ISC_FALSE;
			if (optout != NULL) {
				if ((nsec3.flags & DNS_NSEC3FLAG_OPTOUT) != 0)
					(*logit)(arg, ISC_LOG_DEBUG(3),
						 "NSEC3 indicates optout");
				else
					(*logit)(arg, ISC_LOG_DEBUG(3),
						 "NSEC3 indicates secure range");
				*optout =
				    ISC_TF(nsec3.flags & DNS_NSEC3FLAG_OPTOUT);
			}
			answer = ISC_R_SUCCESS;
		}

		qlabels--;
		if (qlabels > 0)
			dns_name_split(qname, qlabels, NULL, qname);
		first = ISC_FALSE;
	}
	return (answer);
}
