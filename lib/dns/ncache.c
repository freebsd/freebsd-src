/*
 * Copyright (C) 2004, 2005  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2003  Internet Software Consortium.
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

/* $Id: ncache.c,v 1.36.18.3 2005/04/29 00:15:59 marka Exp $ */

/*! \file */

#include <config.h>

#include <isc/buffer.h>
#include <isc/util.h>

#include <dns/db.h>
#include <dns/message.h>
#include <dns/ncache.h>
#include <dns/rdata.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>

/*
 * The format of an ncache rdata is a sequence of one or more records of
 * the following format:
 *
 *	owner name
 *	type
 *	rdata count
 *		rdata length			These two occur 'rdata count'
 *		rdata				times.
 *
 */

static inline isc_result_t
copy_rdataset(dns_rdataset_t *rdataset, isc_buffer_t *buffer) {
	isc_result_t result;
	unsigned int count;
	isc_region_t ar, r;
	dns_rdata_t rdata = DNS_RDATA_INIT;

	/*
	 * Copy the rdataset count to the buffer.
	 */
	isc_buffer_availableregion(buffer, &ar);
	if (ar.length < 2)
		return (ISC_R_NOSPACE);
	count = dns_rdataset_count(rdataset);
	INSIST(count <= 65535);
	isc_buffer_putuint16(buffer, (isc_uint16_t)count);

	result = dns_rdataset_first(rdataset);
	while (result == ISC_R_SUCCESS) {
		dns_rdataset_current(rdataset, &rdata);
		dns_rdata_toregion(&rdata, &r);
		INSIST(r.length <= 65535);
		isc_buffer_availableregion(buffer, &ar);
		if (ar.length < 2)
			return (ISC_R_NOSPACE);
		/*
		 * Copy the rdata length to the buffer.
		 */
		isc_buffer_putuint16(buffer, (isc_uint16_t)r.length);
		/*
		 * Copy the rdata to the buffer.
		 */
		result = isc_buffer_copyregion(buffer, &r);
		if (result != ISC_R_SUCCESS)
			return (result);
		dns_rdata_reset(&rdata);
		result = dns_rdataset_next(rdataset);
	}
	if (result != ISC_R_NOMORE)
		return (result);

	return (ISC_R_SUCCESS);
}

isc_result_t
dns_ncache_add(dns_message_t *message, dns_db_t *cache, dns_dbnode_t *node,
	       dns_rdatatype_t covers, isc_stdtime_t now, dns_ttl_t maxttl,
	       dns_rdataset_t *addedrdataset)
{
	isc_result_t result;
	isc_buffer_t buffer;
	isc_region_t r;
	dns_rdataset_t *rdataset;
	dns_rdatatype_t type;
	dns_name_t *name;
	dns_ttl_t ttl;
	dns_trust_t trust;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdataset_t ncrdataset;
	dns_rdatalist_t ncrdatalist;
	unsigned char data[4096];

	/*
	 * Convert the authority data from 'message' into a negative cache
	 * rdataset, and store it in 'cache' at 'node'.
	 */

	REQUIRE(message != NULL);

	/*
	 * We assume that all data in the authority section has been
	 * validated by the caller.
	 */

	/*
	 * First, build an ncache rdata in buffer.
	 */
	ttl = maxttl;
	trust = 0xffff;
	isc_buffer_init(&buffer, data, sizeof(data));
	if (message->counts[DNS_SECTION_AUTHORITY])
		result = dns_message_firstname(message, DNS_SECTION_AUTHORITY);
	else
		result = ISC_R_NOMORE;
	while (result == ISC_R_SUCCESS) {
		name = NULL;
		dns_message_currentname(message, DNS_SECTION_AUTHORITY,
					&name);
		if ((name->attributes & DNS_NAMEATTR_NCACHE) != 0) {
			for (rdataset = ISC_LIST_HEAD(name->list);
			     rdataset != NULL;
			     rdataset = ISC_LIST_NEXT(rdataset, link)) {
				if ((rdataset->attributes &
				     DNS_RDATASETATTR_NCACHE) == 0)
					continue;
				type = rdataset->type;
				if (type == dns_rdatatype_rrsig)
					type = rdataset->covers;
				if (type == dns_rdatatype_soa ||
				    type == dns_rdatatype_nsec) {
					if (ttl > rdataset->ttl)
						ttl = rdataset->ttl;
					if (trust > rdataset->trust)
						trust = rdataset->trust;
					/*
					 * Copy the owner name to the buffer.
					 */
					dns_name_toregion(name, &r);
					result = isc_buffer_copyregion(&buffer,
								       &r);
					if (result != ISC_R_SUCCESS)
						return (result);
					/*
					 * Copy the type to the buffer.
					 */
					isc_buffer_availableregion(&buffer,
								   &r);
					if (r.length < 2)
						return (ISC_R_NOSPACE);
					isc_buffer_putuint16(&buffer,
							     rdataset->type);
					/*
					 * Copy the rdataset into the buffer.
					 */
					result = copy_rdataset(rdataset,
							       &buffer);
					if (result != ISC_R_SUCCESS)
						return (result);
				}
			}
		}
		result = dns_message_nextname(message, DNS_SECTION_AUTHORITY);
	}
	if (result != ISC_R_NOMORE)
		return (result);

	if (trust == 0xffff) {
		/*
		 * We didn't find any authority data from which to create a
		 * negative cache rdataset.  In particular, we have no SOA.
		 *
		 * We trust that the caller wants negative caching, so this
		 * means we have a "type 3 nxdomain" or "type 3 nodata"
		 * response (see RFC2308 for details).
		 *
		 * We will now build a suitable negative cache rdataset that
		 * will cause zero bytes to be emitted when converted to
		 * wire format.
		 */

		/*
		 * The ownername must exist, but it doesn't matter what value
		 * it has.  We use the root name.
		 */
		dns_name_toregion(dns_rootname, &r);
		result = isc_buffer_copyregion(&buffer, &r);
		if (result != ISC_R_SUCCESS)
			return (result);
		/*
		 * Copy the type and a zero rdata count to the buffer.
		 */
		isc_buffer_availableregion(&buffer, &r);
		if (r.length < 4)
			return (ISC_R_NOSPACE);
		isc_buffer_putuint16(&buffer, 0);
		isc_buffer_putuint16(&buffer, 0);
		/*
		 * RFC2308, section 5, says that negative answers without
		 * SOAs should not be cached.
		 */
		ttl = 0;
		/*
		 * Set trust.
		 */
		if ((message->flags & DNS_MESSAGEFLAG_AA) != 0 &&
		    message->counts[DNS_SECTION_ANSWER] == 0) {
			/*
			 * The response has aa set and we haven't followed
			 * any CNAME or DNAME chains.
			 */
			trust = dns_trust_authauthority;
		} else
			trust = dns_trust_additional;
	}

	/*
	 * Now add it to the cache.
	 */
	INSIST(trust != 0xffff);
	isc_buffer_usedregion(&buffer, &r);
	rdata.data = r.base;
	rdata.length = r.length;
	rdata.rdclass = dns_db_class(cache);
	rdata.type = 0;
	rdata.flags = 0;

	ncrdatalist.rdclass = rdata.rdclass;
	ncrdatalist.type = 0;
	ncrdatalist.covers = covers;
	ncrdatalist.ttl = ttl;
	ISC_LIST_INIT(ncrdatalist.rdata);
	ISC_LINK_INIT(&ncrdatalist, link);

	ISC_LIST_APPEND(ncrdatalist.rdata, &rdata, link);

	dns_rdataset_init(&ncrdataset);
	RUNTIME_CHECK(dns_rdatalist_tordataset(&ncrdatalist, &ncrdataset)
		      == ISC_R_SUCCESS);
	ncrdataset.trust = trust;
	if (message->rcode == dns_rcode_nxdomain)
		ncrdataset.attributes |= DNS_RDATASETATTR_NXDOMAIN;

	return (dns_db_addrdataset(cache, node, NULL, now, &ncrdataset,
				   0, addedrdataset));
}

isc_result_t
dns_ncache_towire(dns_rdataset_t *rdataset, dns_compress_t *cctx,
		  isc_buffer_t *target, unsigned int options,
		  unsigned int *countp)
{
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_result_t result;
	isc_region_t remaining, tavailable;
	isc_buffer_t source, savedbuffer, rdlen;
	dns_name_t name;
	dns_rdatatype_t type;
	unsigned int i, rcount, count;

	/*
	 * Convert the negative caching rdataset 'rdataset' to wire format,
	 * compressing names as specified in 'cctx', and storing the result in
	 * 'target'.
	 */

	REQUIRE(rdataset != NULL);
	REQUIRE(rdataset->type == 0);

	result = dns_rdataset_first(rdataset);
	if (result != ISC_R_SUCCESS)
		return (result);
	dns_rdataset_current(rdataset, &rdata);
	INSIST(dns_rdataset_next(rdataset) == ISC_R_NOMORE);
	isc_buffer_init(&source, rdata.data, rdata.length);
	isc_buffer_add(&source, rdata.length);

	savedbuffer = *target;

	count = 0;
	do {
		dns_name_init(&name, NULL);
		isc_buffer_remainingregion(&source, &remaining);
		dns_name_fromregion(&name, &remaining);
		INSIST(remaining.length >= name.length);
		isc_buffer_forward(&source, name.length);
		remaining.length -= name.length;

		INSIST(remaining.length >= 4);
		type = isc_buffer_getuint16(&source);
		rcount = isc_buffer_getuint16(&source);

		for (i = 0; i < rcount; i++) {
			/*
			 * Get the length of this rdata and set up an
			 * rdata structure for it.
			 */
			isc_buffer_remainingregion(&source, &remaining);
			INSIST(remaining.length >= 2);
			dns_rdata_reset(&rdata);
			rdata.length = isc_buffer_getuint16(&source);
			isc_buffer_remainingregion(&source, &remaining);
			rdata.data = remaining.base;
			rdata.type = type;
			rdata.rdclass = rdataset->rdclass;
			INSIST(remaining.length >= rdata.length);
			isc_buffer_forward(&source, rdata.length);

			if ((options & DNS_NCACHETOWIRE_OMITDNSSEC) != 0 &&
			    dns_rdatatype_isdnssec(type))
				continue;

			/*
			 * Write the name.
			 */
			dns_compress_setmethods(cctx, DNS_COMPRESS_GLOBAL14);
			result = dns_name_towire(&name, cctx, target);
			if (result != ISC_R_SUCCESS)
				goto rollback;

			/*
			 * See if we have space for type, class, ttl, and
			 * rdata length.  Write the type, class, and ttl.
			 */
			isc_buffer_availableregion(target, &tavailable);
			if (tavailable.length < 10) {
				result = ISC_R_NOSPACE;
				goto rollback;
			}
			isc_buffer_putuint16(target, type);
			isc_buffer_putuint16(target, rdataset->rdclass);
			isc_buffer_putuint32(target, rdataset->ttl);

			/*
			 * Save space for rdata length.
			 */
			rdlen = *target;
			isc_buffer_add(target, 2);

			/*
			 * Write the rdata.
			 */
			result = dns_rdata_towire(&rdata, cctx, target);
			if (result != ISC_R_SUCCESS)
				goto rollback;

			/*
			 * Set the rdata length field to the compressed
			 * length.
			 */
			INSIST((target->used >= rdlen.used + 2) &&
			       (target->used - rdlen.used - 2 < 65536));
			isc_buffer_putuint16(&rdlen,
					     (isc_uint16_t)(target->used -
							    rdlen.used - 2));

			count++;
		}
		isc_buffer_remainingregion(&source, &remaining);
	} while (remaining.length > 0);

	*countp = count;

	return (ISC_R_SUCCESS);

 rollback:
	INSIST(savedbuffer.used < 65536);
	dns_compress_rollback(cctx, (isc_uint16_t)savedbuffer.used);
	*countp = 0;
	*target = savedbuffer;

	return (result);
}

static void
rdataset_disassociate(dns_rdataset_t *rdataset) {
	UNUSED(rdataset);
}

static isc_result_t
rdataset_first(dns_rdataset_t *rdataset) {
	unsigned char *raw = rdataset->private3;
	unsigned int count;

	count = raw[0] * 256 + raw[1];
	if (count == 0) {
		rdataset->private5 = NULL;
		return (ISC_R_NOMORE);
	}
	raw += 2;
	/*
	 * The privateuint4 field is the number of rdata beyond the cursor
	 * position, so we decrement the total count by one before storing
	 * it.
	 */
	count--;
	rdataset->privateuint4 = count;
	rdataset->private5 = raw;

	return (ISC_R_SUCCESS);
}

static isc_result_t
rdataset_next(dns_rdataset_t *rdataset) {
	unsigned int count;
	unsigned int length;
	unsigned char *raw;

	count = rdataset->privateuint4;
	if (count == 0)
		return (ISC_R_NOMORE);
	count--;
	rdataset->privateuint4 = count;
	raw = rdataset->private5;
	length = raw[0] * 256 + raw[1];
	raw += length + 2;
	rdataset->private5 = raw;

	return (ISC_R_SUCCESS);
}

static void
rdataset_current(dns_rdataset_t *rdataset, dns_rdata_t *rdata) {
	unsigned char *raw = rdataset->private5;
	isc_region_t r;

	REQUIRE(raw != NULL);

	r.length = raw[0] * 256 + raw[1];
	raw += 2;
	r.base = raw;
	dns_rdata_fromregion(rdata, rdataset->rdclass, rdataset->type, &r);
}

static void
rdataset_clone(dns_rdataset_t *source, dns_rdataset_t *target) {
	*target = *source;

	/*
	 * Reset iterator state.
	 */
	target->privateuint4 = 0;
	target->private5 = NULL;
}

static unsigned int
rdataset_count(dns_rdataset_t *rdataset) {
	unsigned char *raw = rdataset->private3;
	unsigned int count;

	count = raw[0] * 256 + raw[1];

	return (count);
}

static dns_rdatasetmethods_t rdataset_methods = {
	rdataset_disassociate,
	rdataset_first,
	rdataset_next,
	rdataset_current,
	rdataset_clone,
	rdataset_count,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

isc_result_t
dns_ncache_getrdataset(dns_rdataset_t *ncacherdataset, dns_name_t *name,
		       dns_rdatatype_t type, dns_rdataset_t *rdataset)
{
	isc_result_t result;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_region_t remaining;
	isc_buffer_t source;
	dns_name_t tname;
	dns_rdatatype_t ttype;
	unsigned int i, rcount;
	isc_uint16_t length;

	REQUIRE(ncacherdataset != NULL);
	REQUIRE(ncacherdataset->type == 0);
	REQUIRE(name != NULL);
	REQUIRE(!dns_rdataset_isassociated(rdataset));
	REQUIRE(type != dns_rdatatype_rrsig);

	result = dns_rdataset_first(ncacherdataset);
	if (result != ISC_R_SUCCESS)
		return (result);
	dns_rdataset_current(ncacherdataset, &rdata);
	INSIST(dns_rdataset_next(ncacherdataset) == ISC_R_NOMORE);
	isc_buffer_init(&source, rdata.data, rdata.length);
	isc_buffer_add(&source, rdata.length);

	do {
		dns_name_init(&tname, NULL);
		isc_buffer_remainingregion(&source, &remaining);
		dns_name_fromregion(&tname, &remaining);
		INSIST(remaining.length >= tname.length);
		isc_buffer_forward(&source, tname.length);
		remaining.length -= tname.length;

		INSIST(remaining.length >= 4);
		ttype = isc_buffer_getuint16(&source);

		if (ttype == type && dns_name_equal(&tname, name)) {
			isc_buffer_remainingregion(&source, &remaining);
			break;
		}

		rcount = isc_buffer_getuint16(&source);
		for (i = 0; i < rcount; i++) {
			isc_buffer_remainingregion(&source, &remaining);
			INSIST(remaining.length >= 2);
			length = isc_buffer_getuint16(&source);
			isc_buffer_remainingregion(&source, &remaining);
			INSIST(remaining.length >= length);
			isc_buffer_forward(&source, length);
		}
		isc_buffer_remainingregion(&source, &remaining);
	} while (remaining.length > 0);

	if (remaining.length == 0)
		return (ISC_R_NOTFOUND);

	rdataset->methods = &rdataset_methods;
	rdataset->rdclass = ncacherdataset->rdclass;
	rdataset->type = type;
	rdataset->covers = 0;
	rdataset->ttl = ncacherdataset->ttl;
	rdataset->trust = ncacherdataset->trust;
	rdataset->private1 = NULL;
	rdataset->private2 = NULL;

	rdataset->private3 = remaining.base;

	/*
	 * Reset iterator state.
	 */
	rdataset->privateuint4 = 0;
	rdataset->private5 = NULL;
	return (ISC_R_SUCCESS);
}
