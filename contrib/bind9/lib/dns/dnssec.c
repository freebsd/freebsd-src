/*
 * Copyright (C) 2004-2010  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2003  Internet Software Consortium.
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

/*
 * $Id: dnssec.c,v 1.119 2010-01-13 23:48:59 tbox Exp $
 */

/*! \file */

#include <config.h>

#include <stdlib.h>

#include <isc/buffer.h>
#include <isc/dir.h>
#include <isc/mem.h>
#include <isc/serial.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dns/db.h>
#include <dns/diff.h>
#include <dns/dnssec.h>
#include <dns/fixedname.h>
#include <dns/keyvalues.h>
#include <dns/log.h>
#include <dns/message.h>
#include <dns/rdata.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>
#include <dns/rdatastruct.h>
#include <dns/result.h>
#include <dns/tsig.h>		/* for DNS_TSIG_FUDGE */

#include <dst/result.h>

#define is_response(msg) (msg->flags & DNS_MESSAGEFLAG_QR)

#define RETERR(x) do { \
	result = (x); \
	if (result != ISC_R_SUCCESS) \
		goto failure; \
	} while (0)


#define TYPE_SIGN 0
#define TYPE_VERIFY 1

static isc_result_t
digest_callback(void *arg, isc_region_t *data);

static int
rdata_compare_wrapper(const void *rdata1, const void *rdata2);

static isc_result_t
rdataset_to_sortedarray(dns_rdataset_t *set, isc_mem_t *mctx,
			dns_rdata_t **rdata, int *nrdata);

static isc_result_t
digest_callback(void *arg, isc_region_t *data) {
	dst_context_t *ctx = arg;

	return (dst_context_adddata(ctx, data));
}

/*
 * Make qsort happy.
 */
static int
rdata_compare_wrapper(const void *rdata1, const void *rdata2) {
	return (dns_rdata_compare((const dns_rdata_t *)rdata1,
				  (const dns_rdata_t *)rdata2));
}

/*
 * Sort the rdataset into an array.
 */
static isc_result_t
rdataset_to_sortedarray(dns_rdataset_t *set, isc_mem_t *mctx,
			dns_rdata_t **rdata, int *nrdata)
{
	isc_result_t ret;
	int i = 0, n;
	dns_rdata_t *data;
	dns_rdataset_t rdataset;

	n = dns_rdataset_count(set);

	data = isc_mem_get(mctx, n * sizeof(dns_rdata_t));
	if (data == NULL)
		return (ISC_R_NOMEMORY);

	dns_rdataset_init(&rdataset);
	dns_rdataset_clone(set, &rdataset);
	ret = dns_rdataset_first(&rdataset);
	if (ret != ISC_R_SUCCESS) {
		dns_rdataset_disassociate(&rdataset);
		isc_mem_put(mctx, data, n * sizeof(dns_rdata_t));
		return (ret);
	}

	/*
	 * Put them in the array.
	 */
	do {
		dns_rdata_init(&data[i]);
		dns_rdataset_current(&rdataset, &data[i++]);
	} while (dns_rdataset_next(&rdataset) == ISC_R_SUCCESS);

	/*
	 * Sort the array.
	 */
	qsort(data, n, sizeof(dns_rdata_t), rdata_compare_wrapper);
	*rdata = data;
	*nrdata = n;
	dns_rdataset_disassociate(&rdataset);
	return (ISC_R_SUCCESS);
}

isc_result_t
dns_dnssec_keyfromrdata(dns_name_t *name, dns_rdata_t *rdata, isc_mem_t *mctx,
			dst_key_t **key)
{
	isc_buffer_t b;
	isc_region_t r;

	INSIST(name != NULL);
	INSIST(rdata != NULL);
	INSIST(mctx != NULL);
	INSIST(key != NULL);
	INSIST(*key == NULL);
	REQUIRE(rdata->type == dns_rdatatype_key ||
		rdata->type == dns_rdatatype_dnskey);

	dns_rdata_toregion(rdata, &r);
	isc_buffer_init(&b, r.base, r.length);
	isc_buffer_add(&b, r.length);
	return (dst_key_fromdns(name, rdata->rdclass, &b, mctx, key));
}

static isc_result_t
digest_sig(dst_context_t *ctx, dns_rdata_t *sigrdata, dns_rdata_rrsig_t *sig) {
	isc_region_t r;
	isc_result_t ret;
	dns_fixedname_t fname;

	dns_rdata_toregion(sigrdata, &r);
	INSIST(r.length >= 19);

	r.length = 18;
	ret = dst_context_adddata(ctx, &r);
	if (ret != ISC_R_SUCCESS)
		return (ret);
	dns_fixedname_init(&fname);
	RUNTIME_CHECK(dns_name_downcase(&sig->signer,
					dns_fixedname_name(&fname), NULL)
		      == ISC_R_SUCCESS);
	dns_name_toregion(dns_fixedname_name(&fname), &r);
	return (dst_context_adddata(ctx, &r));
}

isc_result_t
dns_dnssec_sign(dns_name_t *name, dns_rdataset_t *set, dst_key_t *key,
		isc_stdtime_t *inception, isc_stdtime_t *expire,
		isc_mem_t *mctx, isc_buffer_t *buffer, dns_rdata_t *sigrdata)
{
	dns_rdata_rrsig_t sig;
	dns_rdata_t tmpsigrdata;
	dns_rdata_t *rdatas;
	int nrdatas, i;
	isc_buffer_t sigbuf, envbuf;
	isc_region_t r;
	dst_context_t *ctx = NULL;
	isc_result_t ret;
	isc_buffer_t *databuf = NULL;
	char data[256 + 8];
	isc_uint32_t flags;
	unsigned int sigsize;
	dns_fixedname_t fnewname;

	REQUIRE(name != NULL);
	REQUIRE(dns_name_countlabels(name) <= 255);
	REQUIRE(set != NULL);
	REQUIRE(key != NULL);
	REQUIRE(inception != NULL);
	REQUIRE(expire != NULL);
	REQUIRE(mctx != NULL);
	REQUIRE(sigrdata != NULL);

	if (*inception >= *expire)
		return (DNS_R_INVALIDTIME);

	/*
	 * Is the key allowed to sign data?
	 */
	flags = dst_key_flags(key);
	if (flags & DNS_KEYTYPE_NOAUTH)
		return (DNS_R_KEYUNAUTHORIZED);
	if ((flags & DNS_KEYFLAG_OWNERMASK) != DNS_KEYOWNER_ZONE)
		return (DNS_R_KEYUNAUTHORIZED);

	sig.mctx = mctx;
	sig.common.rdclass = set->rdclass;
	sig.common.rdtype = dns_rdatatype_rrsig;
	ISC_LINK_INIT(&sig.common, link);

	dns_name_init(&sig.signer, NULL);
	dns_name_clone(dst_key_name(key), &sig.signer);

	sig.covered = set->type;
	sig.algorithm = dst_key_alg(key);
	sig.labels = dns_name_countlabels(name) - 1;
	if (dns_name_iswildcard(name))
		sig.labels--;
	sig.originalttl = set->ttl;
	sig.timesigned = *inception;
	sig.timeexpire = *expire;
	sig.keyid = dst_key_id(key);
	ret = dst_key_sigsize(key, &sigsize);
	if (ret != ISC_R_SUCCESS)
		return (ret);
	sig.siglen = sigsize;
	/*
	 * The actual contents of sig.signature are not important yet, since
	 * they're not used in digest_sig().
	 */
	sig.signature = isc_mem_get(mctx, sig.siglen);
	if (sig.signature == NULL)
		return (ISC_R_NOMEMORY);

	ret = isc_buffer_allocate(mctx, &databuf, sigsize + 256 + 18);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_signature;

	dns_rdata_init(&tmpsigrdata);
	ret = dns_rdata_fromstruct(&tmpsigrdata, sig.common.rdclass,
				   sig.common.rdtype, &sig, databuf);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_databuf;

	ret = dst_context_create(key, mctx, &ctx);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_databuf;

	/*
	 * Digest the SIG rdata.
	 */
	ret = digest_sig(ctx, &tmpsigrdata, &sig);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_context;

	dns_fixedname_init(&fnewname);
	RUNTIME_CHECK(dns_name_downcase(name, dns_fixedname_name(&fnewname),
					NULL) == ISC_R_SUCCESS);
	dns_name_toregion(dns_fixedname_name(&fnewname), &r);

	/*
	 * Create an envelope for each rdata: <name|type|class|ttl>.
	 */
	isc_buffer_init(&envbuf, data, sizeof(data));
	memcpy(data, r.base, r.length);
	isc_buffer_add(&envbuf, r.length);
	isc_buffer_putuint16(&envbuf, set->type);
	isc_buffer_putuint16(&envbuf, set->rdclass);
	isc_buffer_putuint32(&envbuf, set->ttl);

	ret = rdataset_to_sortedarray(set, mctx, &rdatas, &nrdatas);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_context;
	isc_buffer_usedregion(&envbuf, &r);

	for (i = 0; i < nrdatas; i++) {
		isc_uint16_t len;
		isc_buffer_t lenbuf;
		isc_region_t lenr;

		/*
		 * Skip duplicates.
		 */
		if (i > 0 && dns_rdata_compare(&rdatas[i], &rdatas[i-1]) == 0)
		    continue;

		/*
		 * Digest the envelope.
		 */
		ret = dst_context_adddata(ctx, &r);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_array;

		/*
		 * Digest the length of the rdata.
		 */
		isc_buffer_init(&lenbuf, &len, sizeof(len));
		INSIST(rdatas[i].length < 65536);
		isc_buffer_putuint16(&lenbuf, (isc_uint16_t)rdatas[i].length);
		isc_buffer_usedregion(&lenbuf, &lenr);
		ret = dst_context_adddata(ctx, &lenr);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_array;

		/*
		 * Digest the rdata.
		 */
		ret = dns_rdata_digest(&rdatas[i], digest_callback, ctx);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_array;
	}

	isc_buffer_init(&sigbuf, sig.signature, sig.siglen);
	ret = dst_context_sign(ctx, &sigbuf);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_array;
	isc_buffer_usedregion(&sigbuf, &r);
	if (r.length != sig.siglen) {
		ret = ISC_R_NOSPACE;
		goto cleanup_array;
	}
	memcpy(sig.signature, r.base, sig.siglen);

	ret = dns_rdata_fromstruct(sigrdata, sig.common.rdclass,
				  sig.common.rdtype, &sig, buffer);

cleanup_array:
	isc_mem_put(mctx, rdatas, nrdatas * sizeof(dns_rdata_t));
cleanup_context:
	dst_context_destroy(&ctx);
cleanup_databuf:
	isc_buffer_free(&databuf);
cleanup_signature:
	isc_mem_put(mctx, sig.signature, sig.siglen);

	return (ret);
}

isc_result_t
dns_dnssec_verify2(dns_name_t *name, dns_rdataset_t *set, dst_key_t *key,
		   isc_boolean_t ignoretime, isc_mem_t *mctx,
		   dns_rdata_t *sigrdata, dns_name_t *wild)
{
	dns_rdata_rrsig_t sig;
	dns_fixedname_t fnewname;
	isc_region_t r;
	isc_buffer_t envbuf;
	dns_rdata_t *rdatas;
	int nrdatas, i;
	isc_stdtime_t now;
	isc_result_t ret;
	unsigned char data[300];
	dst_context_t *ctx = NULL;
	int labels = 0;
	isc_uint32_t flags;

	REQUIRE(name != NULL);
	REQUIRE(set != NULL);
	REQUIRE(key != NULL);
	REQUIRE(mctx != NULL);
	REQUIRE(sigrdata != NULL && sigrdata->type == dns_rdatatype_rrsig);

	ret = dns_rdata_tostruct(sigrdata, &sig, NULL);
	if (ret != ISC_R_SUCCESS)
		return (ret);

	if (set->type != sig.covered)
		return (DNS_R_SIGINVALID);

	if (isc_serial_lt(sig.timeexpire, sig.timesigned))
		return (DNS_R_SIGINVALID);

	if (!ignoretime) {
		isc_stdtime_get(&now);

		/*
		 * Is SIG temporally valid?
		 */
		if (isc_serial_lt((isc_uint32_t)now, sig.timesigned))
			return (DNS_R_SIGFUTURE);
		else if (isc_serial_lt(sig.timeexpire, (isc_uint32_t)now))
			return (DNS_R_SIGEXPIRED);
	}

	/*
	 * NS, SOA and DNSSKEY records are signed by their owner.
	 * DS records are signed by the parent.
	 */
	switch (set->type) {
	case dns_rdatatype_ns:
	case dns_rdatatype_soa:
	case dns_rdatatype_dnskey:
		if (!dns_name_equal(name, &sig.signer))
			return (DNS_R_SIGINVALID);
		break;
	case dns_rdatatype_ds:
		if (dns_name_equal(name, &sig.signer))
			return (DNS_R_SIGINVALID);
		/* FALLTHROUGH */
	default:
		if (!dns_name_issubdomain(name, &sig.signer))
			return (DNS_R_SIGINVALID);
		break;
	}

	/*
	 * Is the key allowed to sign data?
	 */
	flags = dst_key_flags(key);
	if (flags & DNS_KEYTYPE_NOAUTH)
		return (DNS_R_KEYUNAUTHORIZED);
	if ((flags & DNS_KEYFLAG_OWNERMASK) != DNS_KEYOWNER_ZONE)
		return (DNS_R_KEYUNAUTHORIZED);

	ret = dst_context_create(key, mctx, &ctx);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_struct;

	/*
	 * Digest the SIG rdata (not including the signature).
	 */
	ret = digest_sig(ctx, sigrdata, &sig);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_context;

	/*
	 * If the name is an expanded wildcard, use the wildcard name.
	 */
	dns_fixedname_init(&fnewname);
	labels = dns_name_countlabels(name) - 1;
	RUNTIME_CHECK(dns_name_downcase(name, dns_fixedname_name(&fnewname),
					NULL) == ISC_R_SUCCESS);
	if (labels - sig.labels > 0)
		dns_name_split(dns_fixedname_name(&fnewname), sig.labels + 1,
			       NULL, dns_fixedname_name(&fnewname));

	dns_name_toregion(dns_fixedname_name(&fnewname), &r);

	/*
	 * Create an envelope for each rdata: <name|type|class|ttl>.
	 */
	isc_buffer_init(&envbuf, data, sizeof(data));
	if (labels - sig.labels > 0) {
		isc_buffer_putuint8(&envbuf, 1);
		isc_buffer_putuint8(&envbuf, '*');
		memcpy(data + 2, r.base, r.length);
	}
	else
		memcpy(data, r.base, r.length);
	isc_buffer_add(&envbuf, r.length);
	isc_buffer_putuint16(&envbuf, set->type);
	isc_buffer_putuint16(&envbuf, set->rdclass);
	isc_buffer_putuint32(&envbuf, sig.originalttl);

	ret = rdataset_to_sortedarray(set, mctx, &rdatas, &nrdatas);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_context;

	isc_buffer_usedregion(&envbuf, &r);

	for (i = 0; i < nrdatas; i++) {
		isc_uint16_t len;
		isc_buffer_t lenbuf;
		isc_region_t lenr;

		/*
		 * Skip duplicates.
		 */
		if (i > 0 && dns_rdata_compare(&rdatas[i], &rdatas[i-1]) == 0)
		    continue;

		/*
		 * Digest the envelope.
		 */
		ret = dst_context_adddata(ctx, &r);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_array;

		/*
		 * Digest the rdata length.
		 */
		isc_buffer_init(&lenbuf, &len, sizeof(len));
		INSIST(rdatas[i].length < 65536);
		isc_buffer_putuint16(&lenbuf, (isc_uint16_t)rdatas[i].length);
		isc_buffer_usedregion(&lenbuf, &lenr);

		/*
		 * Digest the rdata.
		 */
		ret = dst_context_adddata(ctx, &lenr);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_array;
		ret = dns_rdata_digest(&rdatas[i], digest_callback, ctx);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_array;
	}

	r.base = sig.signature;
	r.length = sig.siglen;
	ret = dst_context_verify(ctx, &r);
	if (ret == DST_R_VERIFYFAILURE)
		ret = DNS_R_SIGINVALID;

cleanup_array:
	isc_mem_put(mctx, rdatas, nrdatas * sizeof(dns_rdata_t));
cleanup_context:
	dst_context_destroy(&ctx);
cleanup_struct:
	dns_rdata_freestruct(&sig);

	if (ret == ISC_R_SUCCESS && labels - sig.labels > 0) {
		if (wild != NULL)
			RUNTIME_CHECK(dns_name_concatenate(dns_wildcardname,
						 dns_fixedname_name(&fnewname),
						 wild, NULL) == ISC_R_SUCCESS);
		ret = DNS_R_FROMWILDCARD;
	}
	return (ret);
}

isc_result_t
dns_dnssec_verify(dns_name_t *name, dns_rdataset_t *set, dst_key_t *key,
		  isc_boolean_t ignoretime, isc_mem_t *mctx,
		  dns_rdata_t *sigrdata)
{
	isc_result_t result;

	result = dns_dnssec_verify2(name, set, key, ignoretime, mctx,
				    sigrdata, NULL);
	if (result == DNS_R_FROMWILDCARD)
		result = ISC_R_SUCCESS;
	return (result);
}

static isc_boolean_t
key_active(dst_key_t *key) {
	isc_result_t result;
	isc_stdtime_t now, publish, active, revoke, inactive, delete;
	isc_boolean_t pubset = ISC_FALSE, actset = ISC_FALSE;
	isc_boolean_t revset = ISC_FALSE, inactset = ISC_FALSE;
	isc_boolean_t delset = ISC_FALSE;
	int major, minor;

	/* Is this an old-style key? */
	result = dst_key_getprivateformat(key, &major, &minor);

	/*
	 * Smart signing started with key format 1.3; prior to that, all
	 * keys are assumed active
	 */
	if (major == 1 && minor <= 2)
		return (ISC_TRUE);

	isc_stdtime_get(&now);

	result = dst_key_gettime(key, DST_TIME_PUBLISH, &publish);
	if (result == ISC_R_SUCCESS)
		pubset = ISC_TRUE;

	result = dst_key_gettime(key, DST_TIME_ACTIVATE, &active);
	if (result == ISC_R_SUCCESS)
		actset = ISC_TRUE;

	result = dst_key_gettime(key, DST_TIME_REVOKE, &revoke);
	if (result == ISC_R_SUCCESS)
		revset = ISC_TRUE;

	result = dst_key_gettime(key, DST_TIME_INACTIVE, &inactive);
	if (result == ISC_R_SUCCESS)
		inactset = ISC_TRUE;

	result = dst_key_gettime(key, DST_TIME_DELETE, &delete);
	if (result == ISC_R_SUCCESS)
		delset = ISC_TRUE;

	if ((inactset && inactive <= now) || (delset && delete <= now))
		return (ISC_FALSE);

	if (revset && revoke <= now && pubset && publish <= now)
		return (ISC_TRUE);

	if (actset && active <= now)
		return (ISC_TRUE);

	return (ISC_FALSE);
}

#define is_zone_key(key) ((dst_key_flags(key) & DNS_KEYFLAG_OWNERMASK) \
			  == DNS_KEYOWNER_ZONE)

isc_result_t
dns_dnssec_findzonekeys2(dns_db_t *db, dns_dbversion_t *ver,
			 dns_dbnode_t *node, dns_name_t *name,
			 const char *directory, isc_mem_t *mctx,
			 unsigned int maxkeys, dst_key_t **keys,
			 unsigned int *nkeys)
{
	dns_rdataset_t rdataset;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_result_t result;
	dst_key_t *pubkey = NULL;
	unsigned int count = 0;

	REQUIRE(nkeys != NULL);
	REQUIRE(keys != NULL);

	*nkeys = 0;
	dns_rdataset_init(&rdataset);
	RETERR(dns_db_findrdataset(db, node, ver, dns_rdatatype_dnskey, 0, 0,
				   &rdataset, NULL));
	RETERR(dns_rdataset_first(&rdataset));
	while (result == ISC_R_SUCCESS && count < maxkeys) {
		pubkey = NULL;
		dns_rdataset_current(&rdataset, &rdata);
		RETERR(dns_dnssec_keyfromrdata(name, &rdata, mctx, &pubkey));
		if (!is_zone_key(pubkey) ||
		    (dst_key_flags(pubkey) & DNS_KEYTYPE_NOAUTH) != 0)
			goto next;
		/* Corrupted .key file? */
		if (!dns_name_equal(name, dst_key_name(pubkey)))
			goto next;
		keys[count] = NULL;
		result = dst_key_fromfile(dst_key_name(pubkey),
					  dst_key_id(pubkey),
					  dst_key_alg(pubkey),
					  DST_TYPE_PUBLIC|DST_TYPE_PRIVATE,
					  directory,
					  mctx, &keys[count]);

		/*
		 * If the key was revoked and the private file
		 * doesn't exist, maybe it was revoked internally
		 * by named.  Try loading the unrevoked version.
		 */
		if (result == ISC_R_FILENOTFOUND) {
			isc_uint32_t flags;
			flags = dst_key_flags(pubkey);
			if ((flags & DNS_KEYFLAG_REVOKE) != 0) {
				dst_key_setflags(pubkey,
						 flags & ~DNS_KEYFLAG_REVOKE);
				result = dst_key_fromfile(dst_key_name(pubkey),
							  dst_key_id(pubkey),
							  dst_key_alg(pubkey),
							  DST_TYPE_PUBLIC|
							  DST_TYPE_PRIVATE,
							  directory,
							  mctx, &keys[count]);
				if (result == ISC_R_SUCCESS &&
				    dst_key_pubcompare(pubkey, keys[count],
						       ISC_FALSE)) {
					dst_key_setflags(keys[count], flags);
				}
				dst_key_setflags(pubkey, flags);
			}
		}

		if (result != ISC_R_SUCCESS) {
			char keybuf[DNS_NAME_FORMATSIZE];
			char algbuf[DNS_SECALG_FORMATSIZE];
			dns_name_format(dst_key_name(pubkey), keybuf,
					sizeof(keybuf));
			dns_secalg_format(dst_key_alg(pubkey), algbuf,
					  sizeof(algbuf));
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_GENERAL,
				      DNS_LOGMODULE_DNSSEC, ISC_LOG_WARNING,
				      "dns_dnssec_findzonekeys2: error "
				      "reading private key file %s/%s/%d: %s",
				      keybuf, algbuf, dst_key_id(pubkey),
				      isc_result_totext(result));
		}

		if (result == ISC_R_FILENOTFOUND || result == ISC_R_NOPERM) {
			keys[count] = pubkey;
			pubkey = NULL;
			count++;
			goto next;
		}

		if (result != ISC_R_SUCCESS)
			goto failure;

		/*
		 * If a key is marked inactive, skip it
		 */
		if (!key_active(keys[count])) {
			dst_key_free(&keys[count]);
			keys[count] = pubkey;
			pubkey = NULL;
			count++;
			goto next;
		}

		if ((dst_key_flags(keys[count]) & DNS_KEYTYPE_NOAUTH) != 0) {
			/* We should never get here. */
			dst_key_free(&keys[count]);
			goto next;
		}
		count++;
 next:
		if (pubkey != NULL)
			dst_key_free(&pubkey);
		dns_rdata_reset(&rdata);
		result = dns_rdataset_next(&rdataset);
	}
	if (result != ISC_R_NOMORE)
		goto failure;
	if (count == 0)
		result = ISC_R_NOTFOUND;
	else
		result = ISC_R_SUCCESS;

 failure:
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	if (pubkey != NULL)
		dst_key_free(&pubkey);
	if (result != ISC_R_SUCCESS)
		while (count > 0)
			dst_key_free(&keys[--count]);
	*nkeys = count;
	return (result);
}

isc_result_t
dns_dnssec_findzonekeys(dns_db_t *db, dns_dbversion_t *ver,
			dns_dbnode_t *node, dns_name_t *name, isc_mem_t *mctx,
			unsigned int maxkeys, dst_key_t **keys,
			unsigned int *nkeys)
{
	return (dns_dnssec_findzonekeys2(db, ver, node, name, NULL, mctx,
					 maxkeys, keys, nkeys));
}

isc_result_t
dns_dnssec_signmessage(dns_message_t *msg, dst_key_t *key) {
	dns_rdata_sig_t sig;	/* SIG(0) */
	unsigned char data[512];
	unsigned char header[DNS_MESSAGE_HEADERLEN];
	isc_buffer_t headerbuf, databuf, sigbuf;
	unsigned int sigsize;
	isc_buffer_t *dynbuf = NULL;
	dns_rdata_t *rdata;
	dns_rdatalist_t *datalist;
	dns_rdataset_t *dataset;
	isc_region_t r;
	isc_stdtime_t now;
	dst_context_t *ctx = NULL;
	isc_mem_t *mctx;
	isc_result_t result;
	isc_boolean_t signeedsfree = ISC_TRUE;

	REQUIRE(msg != NULL);
	REQUIRE(key != NULL);

	if (is_response(msg))
		REQUIRE(msg->query.base != NULL);

	mctx = msg->mctx;

	memset(&sig, 0, sizeof(sig));

	sig.mctx = mctx;
	sig.common.rdclass = dns_rdataclass_any;
	sig.common.rdtype = dns_rdatatype_sig;	/* SIG(0) */
	ISC_LINK_INIT(&sig.common, link);

	sig.covered = 0;
	sig.algorithm = dst_key_alg(key);
	sig.labels = 0; /* the root name */
	sig.originalttl = 0;

	isc_stdtime_get(&now);
	sig.timesigned = now - DNS_TSIG_FUDGE;
	sig.timeexpire = now + DNS_TSIG_FUDGE;

	sig.keyid = dst_key_id(key);

	dns_name_init(&sig.signer, NULL);
	dns_name_clone(dst_key_name(key), &sig.signer);

	sig.siglen = 0;
	sig.signature = NULL;

	isc_buffer_init(&databuf, data, sizeof(data));

	RETERR(dst_context_create(key, mctx, &ctx));

	/*
	 * Digest the fields of the SIG - we can cheat and use
	 * dns_rdata_fromstruct.  Since siglen is 0, the digested data
	 * is identical to dns format.
	 */
	RETERR(dns_rdata_fromstruct(NULL, dns_rdataclass_any,
				    dns_rdatatype_sig /* SIG(0) */,
				    &sig, &databuf));
	isc_buffer_usedregion(&databuf, &r);
	RETERR(dst_context_adddata(ctx, &r));

	/*
	 * If this is a response, digest the query.
	 */
	if (is_response(msg))
		RETERR(dst_context_adddata(ctx, &msg->query));

	/*
	 * Digest the header.
	 */
	isc_buffer_init(&headerbuf, header, sizeof(header));
	dns_message_renderheader(msg, &headerbuf);
	isc_buffer_usedregion(&headerbuf, &r);
	RETERR(dst_context_adddata(ctx, &r));

	/*
	 * Digest the remainder of the message.
	 */
	isc_buffer_usedregion(msg->buffer, &r);
	isc_region_consume(&r, DNS_MESSAGE_HEADERLEN);
	RETERR(dst_context_adddata(ctx, &r));

	RETERR(dst_key_sigsize(key, &sigsize));
	sig.siglen = sigsize;
	sig.signature = (unsigned char *) isc_mem_get(mctx, sig.siglen);
	if (sig.signature == NULL) {
		result = ISC_R_NOMEMORY;
		goto failure;
	}

	isc_buffer_init(&sigbuf, sig.signature, sig.siglen);
	RETERR(dst_context_sign(ctx, &sigbuf));
	dst_context_destroy(&ctx);

	rdata = NULL;
	RETERR(dns_message_gettemprdata(msg, &rdata));
	RETERR(isc_buffer_allocate(msg->mctx, &dynbuf, 1024));
	RETERR(dns_rdata_fromstruct(rdata, dns_rdataclass_any,
				    dns_rdatatype_sig /* SIG(0) */,
				    &sig, dynbuf));

	isc_mem_put(mctx, sig.signature, sig.siglen);
	signeedsfree = ISC_FALSE;

	dns_message_takebuffer(msg, &dynbuf);

	datalist = NULL;
	RETERR(dns_message_gettemprdatalist(msg, &datalist));
	datalist->rdclass = dns_rdataclass_any;
	datalist->type = dns_rdatatype_sig;	/* SIG(0) */
	datalist->covers = 0;
	datalist->ttl = 0;
	ISC_LIST_INIT(datalist->rdata);
	ISC_LIST_APPEND(datalist->rdata, rdata, link);
	dataset = NULL;
	RETERR(dns_message_gettemprdataset(msg, &dataset));
	dns_rdataset_init(dataset);
	RUNTIME_CHECK(dns_rdatalist_tordataset(datalist, dataset) == ISC_R_SUCCESS);
	msg->sig0 = dataset;

	return (ISC_R_SUCCESS);

failure:
	if (dynbuf != NULL)
		isc_buffer_free(&dynbuf);
	if (signeedsfree)
		isc_mem_put(mctx, sig.signature, sig.siglen);
	if (ctx != NULL)
		dst_context_destroy(&ctx);

	return (result);
}

isc_result_t
dns_dnssec_verifymessage(isc_buffer_t *source, dns_message_t *msg,
			 dst_key_t *key)
{
	dns_rdata_sig_t sig;	/* SIG(0) */
	unsigned char header[DNS_MESSAGE_HEADERLEN];
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_region_t r, source_r, sig_r, header_r;
	isc_stdtime_t now;
	dst_context_t *ctx = NULL;
	isc_mem_t *mctx;
	isc_result_t result;
	isc_uint16_t addcount;
	isc_boolean_t signeedsfree = ISC_FALSE;

	REQUIRE(source != NULL);
	REQUIRE(msg != NULL);
	REQUIRE(key != NULL);

	mctx = msg->mctx;

	msg->verify_attempted = 1;

	if (is_response(msg)) {
		if (msg->query.base == NULL)
			return (DNS_R_UNEXPECTEDTSIG);
	}

	isc_buffer_usedregion(source, &source_r);

	RETERR(dns_rdataset_first(msg->sig0));
	dns_rdataset_current(msg->sig0, &rdata);

	RETERR(dns_rdata_tostruct(&rdata, &sig, NULL));
	signeedsfree = ISC_TRUE;

	if (sig.labels != 0) {
		result = DNS_R_SIGINVALID;
		goto failure;
	}

	if (isc_serial_lt(sig.timeexpire, sig.timesigned)) {
		result = DNS_R_SIGINVALID;
		msg->sig0status = dns_tsigerror_badtime;
		goto failure;
	}

	isc_stdtime_get(&now);
	if (isc_serial_lt((isc_uint32_t)now, sig.timesigned)) {
		result = DNS_R_SIGFUTURE;
		msg->sig0status = dns_tsigerror_badtime;
		goto failure;
	}
	else if (isc_serial_lt(sig.timeexpire, (isc_uint32_t)now)) {
		result = DNS_R_SIGEXPIRED;
		msg->sig0status = dns_tsigerror_badtime;
		goto failure;
	}

	if (!dns_name_equal(dst_key_name(key), &sig.signer)) {
		result = DNS_R_SIGINVALID;
		msg->sig0status = dns_tsigerror_badkey;
		goto failure;
	}

	RETERR(dst_context_create(key, mctx, &ctx));

	/*
	 * Digest the SIG(0) record, except for the signature.
	 */
	dns_rdata_toregion(&rdata, &r);
	r.length -= sig.siglen;
	RETERR(dst_context_adddata(ctx, &r));

	/*
	 * If this is a response, digest the query.
	 */
	if (is_response(msg))
		RETERR(dst_context_adddata(ctx, &msg->query));

	/*
	 * Extract the header.
	 */
	memcpy(header, source_r.base, DNS_MESSAGE_HEADERLEN);

	/*
	 * Decrement the additional field counter.
	 */
	memcpy(&addcount, &header[DNS_MESSAGE_HEADERLEN - 2], 2);
	addcount = htons((isc_uint16_t)(ntohs(addcount) - 1));
	memcpy(&header[DNS_MESSAGE_HEADERLEN - 2], &addcount, 2);

	/*
	 * Digest the modified header.
	 */
	header_r.base = (unsigned char *) header;
	header_r.length = DNS_MESSAGE_HEADERLEN;
	RETERR(dst_context_adddata(ctx, &header_r));

	/*
	 * Digest all non-SIG(0) records.
	 */
	r.base = source_r.base + DNS_MESSAGE_HEADERLEN;
	r.length = msg->sigstart - DNS_MESSAGE_HEADERLEN;
	RETERR(dst_context_adddata(ctx, &r));

	sig_r.base = sig.signature;
	sig_r.length = sig.siglen;
	result = dst_context_verify(ctx, &sig_r);
	if (result != ISC_R_SUCCESS) {
		msg->sig0status = dns_tsigerror_badsig;
		goto failure;
	}

	msg->verified_sig = 1;

	dst_context_destroy(&ctx);
	dns_rdata_freestruct(&sig);

	return (ISC_R_SUCCESS);

failure:
	if (signeedsfree)
		dns_rdata_freestruct(&sig);
	if (ctx != NULL)
		dst_context_destroy(&ctx);

	return (result);
}

/*%
 * Does this key ('rdata') self sign the rrset ('rdataset')?
 */
isc_boolean_t
dns_dnssec_selfsigns(dns_rdata_t *rdata, dns_name_t *name,
		     dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset,
		     isc_boolean_t ignoretime, isc_mem_t *mctx)
{
	dst_key_t *dstkey = NULL;
	dns_keytag_t keytag;
	dns_rdata_dnskey_t key;
	dns_rdata_rrsig_t sig;
	dns_rdata_t sigrdata = DNS_RDATA_INIT;
	isc_result_t result;

	INSIST(rdataset->type == dns_rdatatype_key ||
	       rdataset->type == dns_rdatatype_dnskey);
	if (rdataset->type == dns_rdatatype_key) {
		INSIST(sigrdataset->type == dns_rdatatype_sig);
		INSIST(sigrdataset->covers == dns_rdatatype_key);
	} else {
		INSIST(sigrdataset->type == dns_rdatatype_rrsig);
		INSIST(sigrdataset->covers == dns_rdatatype_dnskey);
	}

	result = dns_dnssec_keyfromrdata(name, rdata, mctx, &dstkey);
	if (result != ISC_R_SUCCESS)
		return (ISC_FALSE);
	result = dns_rdata_tostruct(rdata, &key, NULL);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);

	keytag = dst_key_id(dstkey);
	for (result = dns_rdataset_first(sigrdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(sigrdataset))
	{
		dns_rdata_reset(&sigrdata);
		dns_rdataset_current(sigrdataset, &sigrdata);
		result = dns_rdata_tostruct(&sigrdata, &sig, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);

		if (sig.algorithm == key.algorithm &&
		    sig.keyid == keytag) {
			result = dns_dnssec_verify2(name, rdataset, dstkey,
						    ignoretime, mctx,
						    &sigrdata, NULL);
			if (result == ISC_R_SUCCESS) {
				dst_key_free(&dstkey);
				return (ISC_TRUE);
			}
		}
	}
	dst_key_free(&dstkey);
	return (ISC_FALSE);
}

isc_result_t
dns_dnsseckey_create(isc_mem_t *mctx, dst_key_t **dstkey,
		     dns_dnsseckey_t **dkp)
{
	isc_result_t result;
	dns_dnsseckey_t *dk;
	int major, minor;

	REQUIRE(dkp != NULL && *dkp == NULL);
	dk = isc_mem_get(mctx, sizeof(dns_dnsseckey_t));
	if (dk == NULL)
		return (ISC_R_NOMEMORY);

	dk->key = *dstkey;
	*dstkey = NULL;
	dk->force_publish = ISC_FALSE;
	dk->force_sign = ISC_FALSE;
	dk->hint_publish = ISC_FALSE;
	dk->hint_sign = ISC_FALSE;
	dk->hint_remove = ISC_FALSE;
	dk->first_sign = ISC_FALSE;
	dk->is_active = ISC_FALSE;
	dk->prepublish = 0;
	dk->source = dns_keysource_unknown;
	dk->index = 0;

	/* KSK or ZSK? */
	dk->ksk = ISC_TF((dst_key_flags(dk->key) & DNS_KEYFLAG_KSK) != 0);

	/* Is this an old-style key? */
	result = dst_key_getprivateformat(dk->key, &major, &minor);

	/* Smart signing started with key format 1.3 */
	dk->legacy = ISC_TF(major == 1 && minor <= 2);

	ISC_LINK_INIT(dk, link);
	*dkp = dk;
	return (ISC_R_SUCCESS);
}

void
dns_dnsseckey_destroy(isc_mem_t *mctx, dns_dnsseckey_t **dkp) {
	dns_dnsseckey_t *dk;

	REQUIRE(dkp != NULL && *dkp != NULL);
	dk = *dkp;
	if (dk->key != NULL)
		dst_key_free(&dk->key);
	isc_mem_put(mctx, dk, sizeof(dns_dnsseckey_t));
	*dkp = NULL;
}

static void
get_hints(dns_dnsseckey_t *key) {
	isc_result_t result;
	isc_stdtime_t now, publish, active, revoke, inactive, delete;
	isc_boolean_t pubset = ISC_FALSE, actset = ISC_FALSE;
	isc_boolean_t revset = ISC_FALSE, inactset = ISC_FALSE;
	isc_boolean_t delset = ISC_FALSE;

	REQUIRE(key != NULL && key->key != NULL);

	isc_stdtime_get(&now);

	result = dst_key_gettime(key->key, DST_TIME_PUBLISH, &publish);
	if (result == ISC_R_SUCCESS)
		pubset = ISC_TRUE;

	result = dst_key_gettime(key->key, DST_TIME_ACTIVATE, &active);
	if (result == ISC_R_SUCCESS)
		actset = ISC_TRUE;

	result = dst_key_gettime(key->key, DST_TIME_REVOKE, &revoke);
	if (result == ISC_R_SUCCESS)
		revset = ISC_TRUE;

	result = dst_key_gettime(key->key, DST_TIME_INACTIVE, &inactive);
	if (result == ISC_R_SUCCESS)
		inactset = ISC_TRUE;

	result = dst_key_gettime(key->key, DST_TIME_DELETE, &delete);
	if (result == ISC_R_SUCCESS)
		delset = ISC_TRUE;

	/* Metadata says publish (but possibly not activate) */
	if (pubset && publish <= now)
		key->hint_publish = ISC_TRUE;

	/* Metadata says activate (so we must also publish) */
	if (actset && active <= now) {
		key->hint_sign = ISC_TRUE;
		key->hint_publish = ISC_TRUE;
	}

	/*
	 * Activation date is set (maybe in the future), but
	 * publication date isn't. Most likely the user wants to
	 * publish now and activate later.
	 */
	if (actset && !pubset)
		key->hint_publish = ISC_TRUE;

	/*
	 * If activation date is in the future, make note of how far off
	 */
	if (key->hint_publish && actset && active > now) {
		key->prepublish = active - now;
	}

	/*
	 * Key has been marked inactive: we can continue publishing,
	 * but don't sign.
	 */
	if (key->hint_publish && inactset && inactive <= now) {
		key->hint_sign = ISC_FALSE;
	}

	/*
	 * Metadata says revoke.  If the key is published,
	 * we *have to* sign with it per RFC5011--even if it was
	 * not active before.
	 *
	 * If it hasn't already been done, we should also revoke it now.
	 */
	if (key->hint_publish && (revset && revoke <= now)) {
		isc_uint32_t flags;
		key->hint_sign = ISC_TRUE;
		flags = dst_key_flags(key->key);
		if ((flags & DNS_KEYFLAG_REVOKE) == 0) {
			flags |= DNS_KEYFLAG_REVOKE;
			dst_key_setflags(key->key, flags);
		}
	}

	/*
	 * Metadata says delete, so don't publish this key or sign with it.
	 */
	if (delset && delete <= now) {
		key->hint_publish = ISC_FALSE;
		key->hint_sign = ISC_FALSE;
		key->hint_remove = ISC_TRUE;
	}
}

/*%
 * Get a list of DNSSEC keys from the key repository
 */
isc_result_t
dns_dnssec_findmatchingkeys(dns_name_t *origin, const char *directory,
			    isc_mem_t *mctx, dns_dnsseckeylist_t *keylist)
{
	isc_result_t result = ISC_R_SUCCESS;
	isc_boolean_t dir_open = ISC_FALSE;
	dns_dnsseckeylist_t list;
	isc_dir_t dir;
	dns_dnsseckey_t *key = NULL;
	dst_key_t *dstkey = NULL;
	char namebuf[DNS_NAME_FORMATSIZE], *p;
	isc_buffer_t b;
	unsigned int len;

	REQUIRE(keylist != NULL);
	ISC_LIST_INIT(list);
	isc_dir_init(&dir);

	isc_buffer_init(&b, namebuf, sizeof(namebuf) - 1);
	RETERR(dns_name_totext(origin, ISC_FALSE, &b));
	len = isc_buffer_usedlength(&b);
	namebuf[len] = '\0';

	if (directory == NULL)
		directory = ".";
	RETERR(isc_dir_open(&dir, directory));
	dir_open = ISC_TRUE;

	while (isc_dir_read(&dir) == ISC_R_SUCCESS) {
		if (dir.entry.name[0] == 'K' &&
		    dir.entry.length > len + 1 &&
		    dir.entry.name[len + 1] == '+' &&
		    strncasecmp(dir.entry.name + 1, namebuf, len) == 0) {
			p = strrchr(dir.entry.name, '.');
			if (p != NULL && strcmp(p, ".private") != 0)
				continue;

			dstkey = NULL;
			result = dst_key_fromnamedfile(dir.entry.name,
						       directory,
						       DST_TYPE_PUBLIC |
						       DST_TYPE_PRIVATE,
						       mctx, &dstkey);

			if (result != ISC_R_SUCCESS) {
				isc_log_write(dns_lctx,
					      DNS_LOGCATEGORY_GENERAL,
					      DNS_LOGMODULE_DNSSEC,
					      ISC_LOG_WARNING,
					      "dns_dnssec_findmatchingkeys: "
					      "error reading key file %s: %s",
					      dir.entry.name,
					      isc_result_totext(result));
				continue;
			}

			RETERR(dns_dnsseckey_create(mctx, &dstkey, &key));
			key->source = dns_keysource_repository;
			get_hints(key);

			if (key->legacy) {
				dns_dnsseckey_destroy(mctx, &key);
			} else {
				ISC_LIST_APPEND(list, key, link);
				key = NULL;
			}
		}
	}

	if (!ISC_LIST_EMPTY(list))
		ISC_LIST_APPENDLIST(*keylist, list, link);
	else
		result = ISC_R_NOTFOUND;

 failure:
	if (dir_open)
		isc_dir_close(&dir);
	INSIST(key == NULL);
	while ((key = ISC_LIST_HEAD(list)) != NULL) {
		ISC_LIST_UNLINK(list, key, link);
		INSIST(key->key != NULL);
		dst_key_free(&key->key);
		dns_dnsseckey_destroy(mctx, &key);
	}
	if (dstkey != NULL)
		dst_key_free(&dstkey);
	return (result);
}

/*%
 * Add 'newkey' to 'keylist' if it's not already there.
 *
 * If 'savekeys' is ISC_TRUE, then we need to preserve all
 * the keys in the keyset, regardless of whether they have
 * metadata indicating they should be deactivated or removed.
 */
static void
addkey(dns_dnsseckeylist_t *keylist, dst_key_t **newkey,
       isc_boolean_t savekeys, isc_mem_t *mctx)
{
	dns_dnsseckey_t *key;

	/* Skip duplicates */
	for (key = ISC_LIST_HEAD(*keylist);
	     key != NULL;
	     key = ISC_LIST_NEXT(key, link)) {
		if (dst_key_id(key->key) == dst_key_id(*newkey) &&
		    dst_key_alg(key->key) == dst_key_alg(*newkey) &&
		    dns_name_equal(dst_key_name(key->key),
				   dst_key_name(*newkey)))
			break;
	}

	if (key != NULL) {
		/*
		 * Found a match.  If the old key was only public and the
		 * new key is private, replace the old one; otherwise
		 * leave it.  But either way, mark the key as having
		 * been found in the zone.
		 */
		if (dst_key_isprivate(key->key)) {
			dst_key_free(newkey);
		} else if (dst_key_isprivate(*newkey)) {
			dst_key_free(&key->key);
			key->key = *newkey;
		}

		key->source = dns_keysource_zoneapex;
		return;
	}

	dns_dnsseckey_create(mctx, newkey, &key);
	if (key->legacy || savekeys) {
		key->force_publish = ISC_TRUE;
		key->force_sign = dst_key_isprivate(key->key);
	}
	key->source = dns_keysource_zoneapex;
	ISC_LIST_APPEND(*keylist, key, link);
	*newkey = NULL;
}


/*%
 * Mark all keys which signed the DNSKEY/SOA RRsets as "active",
 * for future reference.
 */
static isc_result_t
mark_active_keys(dns_dnsseckeylist_t *keylist, dns_rdataset_t *rrsigs) {
	isc_result_t result = ISC_R_SUCCESS;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdataset_t sigs;
	dns_dnsseckey_t *key;

	REQUIRE(rrsigs != NULL && dns_rdataset_isassociated(rrsigs));

	dns_rdataset_init(&sigs);
	dns_rdataset_clone(rrsigs, &sigs);
	for (key = ISC_LIST_HEAD(*keylist);
	     key != NULL;
	     key = ISC_LIST_NEXT(key, link)) {
		isc_uint16_t keyid, sigid;
		dns_secalg_t keyalg, sigalg;
		keyid = dst_key_id(key->key);
		keyalg = dst_key_alg(key->key);

		for (result = dns_rdataset_first(&sigs);
		     result == ISC_R_SUCCESS;
		     result = dns_rdataset_next(&sigs)) {
			dns_rdata_rrsig_t sig;

			dns_rdata_reset(&rdata);
			dns_rdataset_current(&sigs, &rdata);
			result = dns_rdata_tostruct(&rdata, &sig, NULL);
			RUNTIME_CHECK(result == ISC_R_SUCCESS);
			sigalg = sig.algorithm;
			sigid = sig.keyid;
			if (keyid == sigid && keyalg == sigalg) {
				key->is_active = ISC_TRUE;
				break;
			}
		}
	}

	if (result == ISC_R_NOMORE)
		result = ISC_R_SUCCESS;

	if (dns_rdataset_isassociated(&sigs))
		dns_rdataset_disassociate(&sigs);
	return (result);
}

/*%
 * Add the contents of a DNSKEY rdataset 'keyset' to 'keylist'.
 */
isc_result_t
dns_dnssec_keylistfromrdataset(dns_name_t *origin,
			       const char *directory, isc_mem_t *mctx,
			       dns_rdataset_t *keyset, dns_rdataset_t *keysigs,
			       dns_rdataset_t *soasigs, isc_boolean_t savekeys,
			       isc_boolean_t public,
			       dns_dnsseckeylist_t *keylist)
{
	dns_rdataset_t keys;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dst_key_t *pubkey = NULL, *privkey = NULL;
	isc_result_t result;

	REQUIRE(keyset != NULL && dns_rdataset_isassociated(keyset));

	dns_rdataset_init(&keys);

	dns_rdataset_clone(keyset, &keys);
	for (result = dns_rdataset_first(&keys);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&keys)) {
		dns_rdata_reset(&rdata);
		dns_rdataset_current(&keys, &rdata);
		RETERR(dns_dnssec_keyfromrdata(origin, &rdata, mctx, &pubkey));

		if (!is_zone_key(pubkey) ||
		    (dst_key_flags(pubkey) & DNS_KEYTYPE_NOAUTH) != 0)
			goto skip;

		/* Corrupted .key file? */
		if (!dns_name_equal(origin, dst_key_name(pubkey)))
			goto skip;

		if (public) {
			addkey(keylist, &pubkey, savekeys, mctx);
			goto skip;
		}

		result = dst_key_fromfile(dst_key_name(pubkey),
					  dst_key_id(pubkey),
					  dst_key_alg(pubkey),
					  DST_TYPE_PUBLIC|DST_TYPE_PRIVATE,
					  directory, mctx, &privkey);

		/*
		 * If the key was revoked and the private file
		 * doesn't exist, maybe it was revoked internally
		 * by named.  Try loading the unrevoked version.
		 */
		if (result == ISC_R_FILENOTFOUND) {
			isc_uint32_t flags;
			flags = dst_key_flags(pubkey);
			if ((flags & DNS_KEYFLAG_REVOKE) != 0) {
				dst_key_setflags(pubkey,
						 flags & ~DNS_KEYFLAG_REVOKE);
				result = dst_key_fromfile(dst_key_name(pubkey),
							  dst_key_id(pubkey),
							  dst_key_alg(pubkey),
							  DST_TYPE_PUBLIC|
							  DST_TYPE_PRIVATE,
							  directory,
							  mctx, &privkey);
				if (result == ISC_R_SUCCESS &&
				    dst_key_pubcompare(pubkey, privkey,
						       ISC_FALSE)) {
					dst_key_setflags(privkey, flags);
				}
				dst_key_setflags(pubkey, flags);
			}
		}

		if (result != ISC_R_SUCCESS) {
			char keybuf[DNS_NAME_FORMATSIZE];
			char algbuf[DNS_SECALG_FORMATSIZE];
			dns_name_format(dst_key_name(pubkey), keybuf,
					sizeof(keybuf));
			dns_secalg_format(dst_key_alg(pubkey), algbuf,
					  sizeof(algbuf));
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_GENERAL,
				      DNS_LOGMODULE_DNSSEC, ISC_LOG_WARNING,
				      "dns_dnssec_keylistfromrdataset: error "
				      "reading private key file %s/%s/%d: %s",
				      keybuf, algbuf, dst_key_id(pubkey),
				      isc_result_totext(result));
		}

		if (result == ISC_R_FILENOTFOUND || result == ISC_R_NOPERM) {
			addkey(keylist, &pubkey, savekeys, mctx);
			goto skip;
		}
		RETERR(result);

		/* This should never happen. */
		if ((dst_key_flags(privkey) & DNS_KEYTYPE_NOAUTH) != 0)
			goto skip;

		addkey(keylist, &privkey, savekeys, mctx);
 skip:
		if (pubkey != NULL)
			dst_key_free(&pubkey);
		if (privkey != NULL)
			dst_key_free(&privkey);
	}

	if (result != ISC_R_NOMORE)
		RETERR(result);

	if (keysigs != NULL && dns_rdataset_isassociated(keysigs))
		RETERR(mark_active_keys(keylist, keysigs));

	if (soasigs != NULL && dns_rdataset_isassociated(soasigs))
		RETERR(mark_active_keys(keylist, soasigs));

	result = ISC_R_SUCCESS;

 failure:
	if (dns_rdataset_isassociated(&keys))
		dns_rdataset_disassociate(&keys);
	if (pubkey != NULL)
		dst_key_free(&pubkey);
	if (privkey != NULL)
		dst_key_free(&privkey);
	return (result);
}

static isc_result_t
make_dnskey(dst_key_t *key, unsigned char *buf, int bufsize,
	    dns_rdata_t *target)
{
	isc_result_t result;
	isc_buffer_t b;
	isc_region_t r;

	isc_buffer_init(&b, buf, bufsize);
	result = dst_key_todns(key, &b);
	if (result != ISC_R_SUCCESS)
		return (result);

	dns_rdata_reset(target);
	isc_buffer_usedregion(&b, &r);
	dns_rdata_fromregion(target, dst_key_class(key),
			     dns_rdatatype_dnskey, &r);
	return (ISC_R_SUCCESS);
}

static isc_result_t
publish_key(dns_diff_t *diff, dns_dnsseckey_t *key, dns_name_t *origin,
	    dns_ttl_t ttl, isc_mem_t *mctx, isc_boolean_t allzsk,
	    void (*report)(const char *, ...))
{
	isc_result_t result;
	dns_difftuple_t *tuple = NULL;
	unsigned char buf[DST_KEY_MAXSIZE];
	dns_rdata_t dnskey = DNS_RDATA_INIT;
	char alg[80];

	dns_rdata_reset(&dnskey);
	RETERR(make_dnskey(key->key, buf, sizeof(buf), &dnskey));

	dns_secalg_format(dst_key_alg(key->key), alg, sizeof(alg));
	report("Fetching %s %d/%s from key %s.",
	       key->ksk ?  (allzsk ?  "KSK/ZSK" : "KSK") : "ZSK",
	       dst_key_id(key->key), alg,
	       key->source == dns_keysource_user ?  "file" : "repository");

	if (key->prepublish && ttl > key->prepublish) {
		char keystr[DST_KEY_FORMATSIZE];
		isc_stdtime_t now;

		dst_key_format(key->key, keystr, sizeof(keystr));
		report("Key %s: Delaying activation to match the DNSKEY TTL.\n",
		       keystr, ttl);

		isc_stdtime_get(&now);
		dst_key_settime(key->key, DST_TIME_ACTIVATE, now + ttl);
	}

	/* publish key */
	RETERR(dns_difftuple_create(mctx, DNS_DIFFOP_ADD, origin, ttl,
				    &dnskey, &tuple));
	dns_diff_appendminimal(diff, &tuple);
	result = ISC_R_SUCCESS;

 failure:
	return (result);
}

static isc_result_t
remove_key(dns_diff_t *diff, dns_dnsseckey_t *key, dns_name_t *origin,
	  dns_ttl_t ttl, isc_mem_t *mctx, const char *reason,
	  void (*report)(const char *, ...))
{
	isc_result_t result;
	dns_difftuple_t *tuple = NULL;
	unsigned char buf[DST_KEY_MAXSIZE];
	dns_rdata_t dnskey = DNS_RDATA_INIT;
	char alg[80];

	dns_secalg_format(dst_key_alg(key->key), alg, sizeof(alg));
	report("Removing %s key %d/%s from DNSKEY RRset.",
	       reason, dst_key_id(key->key), alg);

	RETERR(make_dnskey(key->key, buf, sizeof(buf), &dnskey));
	RETERR(dns_difftuple_create(mctx, DNS_DIFFOP_DEL, origin, ttl, &dnskey,
				    &tuple));
	dns_diff_appendminimal(diff, &tuple);
	result = ISC_R_SUCCESS;

 failure:
	return (result);
}

/*
 * Update 'keys' with information from 'newkeys'.
 *
 * If 'removed' is not NULL, any keys that are being removed from
 * the zone will be added to the list for post-removal processing.
 */
isc_result_t
dns_dnssec_updatekeys(dns_dnsseckeylist_t *keys, dns_dnsseckeylist_t *newkeys,
		      dns_dnsseckeylist_t *removed, dns_name_t *origin,
		      dns_ttl_t ttl, dns_diff_t *diff, isc_boolean_t allzsk,
		      isc_mem_t *mctx, void (*report)(const char *, ...))
{
	isc_result_t result;
	dns_dnsseckey_t *key, *key1, *key2, *next;

	/*
	 * First, look through the existing key list to find keys
	 * supplied from the command line which are not in the zone.
	 * Update the zone to include them.
	 */
	for (key = ISC_LIST_HEAD(*keys);
	     key != NULL;
	     key = ISC_LIST_NEXT(key, link)) {
		if (key->source == dns_keysource_user &&
		    (key->hint_publish || key->force_publish)) {
			RETERR(publish_key(diff, key, origin, ttl,
					   mctx, allzsk, report));
		}
	}

	/*
	 * Second, scan the list of newly found keys looking for matches
	 * with known keys, and update accordingly.
	 */
	for (key1 = ISC_LIST_HEAD(*newkeys); key1 != NULL; key1 = next) {
		isc_boolean_t key_revoked = ISC_FALSE;

		next = ISC_LIST_NEXT(key1, link);

		for (key2 = ISC_LIST_HEAD(*keys);
		     key2 != NULL;
		     key2 = ISC_LIST_NEXT(key2, link)) {
			if (dst_key_pubcompare(key1->key, key2->key,
					       ISC_TRUE)) {
				int r1, r2;
				r1 = dst_key_flags(key1->key) &
					DNS_KEYFLAG_REVOKE;
				r2 = dst_key_flags(key2->key) &
					DNS_KEYFLAG_REVOKE;
				key_revoked = ISC_TF(r1 != r2);
				break;
			}
		}

		/* No match found in keys; add the new key. */
		if (key2 == NULL) {
			dns_dnsseckey_t *next;

			next = ISC_LIST_NEXT(key1, link);
			ISC_LIST_UNLINK(*newkeys, key1, link);
			ISC_LIST_APPEND(*keys, key1, link);

			if (key1->source != dns_keysource_zoneapex &&
			    (key1->hint_publish || key1->force_publish)) {
				RETERR(publish_key(diff, key1, origin, ttl,
						   mctx, allzsk, report));
				if (key1->hint_sign || key1->force_sign)
					key1->first_sign = ISC_TRUE;
			}

			continue;
		}

		/* Match found: remove or update it as needed */
		if (key1->hint_remove) {
			RETERR(remove_key(diff, key2, origin, ttl, mctx,
					  "expired", report));
			ISC_LIST_UNLINK(*keys, key2, link);
			if (removed != NULL)
				ISC_LIST_APPEND(*removed, key2, link);
			else
				dns_dnsseckey_destroy(mctx, &key2);
		} else if (key_revoked &&
			 (dst_key_flags(key1->key) & DNS_KEYFLAG_REVOKE) != 0) {

			/*
			 * A previously valid key has been revoked.
			 * We need to remove the old version and pull
			 * in the new one.
			 */
			RETERR(remove_key(diff, key2, origin, ttl, mctx,
					  "revoked", report));
			ISC_LIST_UNLINK(*keys, key2, link);
			if (removed != NULL)
				ISC_LIST_APPEND(*removed, key2, link);
			else
				dns_dnsseckey_destroy(mctx, &key2);

			RETERR(publish_key(diff, key1, origin, ttl,
					   mctx, allzsk, report));
			ISC_LIST_UNLINK(*newkeys, key1, link);
			ISC_LIST_APPEND(*keys, key1, link);

			/*
			 * XXX: The revoke flag is only defined for trust
			 * anchors.  Setting the flag on a non-KSK is legal,
			 * but not defined in any RFC.  It seems reasonable
			 * to treat it the same as a KSK: keep it in the
			 * zone, sign the DNSKEY set with it, but not
			 * sign other records with it.
			 */
			key1->ksk = ISC_TRUE;
			continue;
		} else {
			if (!key2->is_active &&
			    (key1->hint_sign || key1->force_sign))
				key2->first_sign = ISC_TRUE;
			key2->hint_sign = key1->hint_sign;
			key2->hint_publish = key1->hint_publish;
		}
	}

	/* Free any leftover keys in newkeys */
	while (!ISC_LIST_EMPTY(*newkeys)) {
		key1 = ISC_LIST_HEAD(*newkeys);
		ISC_LIST_UNLINK(*newkeys, key1, link);
		dns_dnsseckey_destroy(mctx, &key1);
	}

	result = ISC_R_SUCCESS;

 failure:
	return (result);
}
