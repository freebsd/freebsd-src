/*
 * Copyright (C) 2004, 2005, 2007, 2009, 2011-2013, 2015  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2003  Internet Software Consortium.
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

/*
 * Reviewed: Wed Mar 15 16:47:10 PST 2000 by halley.
 */

/* RFC2535 */

#ifndef RDATA_GENERIC_DNSKEY_48_C
#define RDATA_GENERIC_DNSKEY_48_C

#include <dst/dst.h>

#define RRTYPE_DNSKEY_ATTRIBUTES (DNS_RDATATYPEATTR_DNSSEC)

static inline isc_result_t
fromtext_dnskey(ARGS_FROMTEXT) {

	REQUIRE(type == dns_rdatatype_dnskey);

	return (generic_fromtext_key(rdclass, type, lexer, origin,
				     options, target, callbacks));
}

static inline isc_result_t
totext_dnskey(ARGS_TOTEXT) {

	REQUIRE(rdata != NULL);
	REQUIRE(rdata->type == dns_rdatatype_dnskey);

	return (generic_totext_key(rdata, tctx, target));
}

static inline isc_result_t
fromwire_dnskey(ARGS_FROMWIRE) {

	REQUIRE(type == dns_rdatatype_dnskey);

	return (generic_fromwire_key(rdclass, type, source, dctx,
				     options, target));
}

static inline isc_result_t
towire_dnskey(ARGS_TOWIRE) {
	isc_region_t sr;

	REQUIRE(rdata != NULL);
	REQUIRE(rdata->type == dns_rdatatype_dnskey);
	REQUIRE(rdata->length != 0);

	UNUSED(cctx);

	dns_rdata_toregion(rdata, &sr);
	return (mem_tobuffer(target, sr.base, sr.length));
}

static inline int
compare_dnskey(ARGS_COMPARE) {
	isc_region_t r1;
	isc_region_t r2;

	REQUIRE(rdata1 != NULL);
	REQUIRE(rdata2 != NULL);
	REQUIRE(rdata1->type == rdata2->type);
	REQUIRE(rdata1->rdclass == rdata2->rdclass);
	REQUIRE(rdata1->type == dns_rdatatype_dnskey);
	REQUIRE(rdata1->length != 0);
	REQUIRE(rdata2->length != 0);

	dns_rdata_toregion(rdata1, &r1);
	dns_rdata_toregion(rdata2, &r2);
	return (isc_region_compare(&r1, &r2));
}

static inline isc_result_t
fromstruct_dnskey(ARGS_FROMSTRUCT) {

	REQUIRE(type == dns_rdatatype_dnskey);

	return (generic_fromstruct_key(rdclass, type, source, target));
}

static inline isc_result_t
tostruct_dnskey(ARGS_TOSTRUCT) {
	dns_rdata_dnskey_t *dnskey = target;

	REQUIRE(dnskey != NULL);
	REQUIRE(rdata != NULL);
	REQUIRE(rdata->type == dns_rdatatype_dnskey);

	dnskey->common.rdclass = rdata->rdclass;
	dnskey->common.rdtype = rdata->type;
	ISC_LINK_INIT(&dnskey->common, link);

	return (generic_tostruct_key(rdata, target, mctx));
}

static inline void
freestruct_dnskey(ARGS_FREESTRUCT) {
	dns_rdata_dnskey_t *dnskey = (dns_rdata_dnskey_t *) source;

	REQUIRE(dnskey != NULL);
	REQUIRE(dnskey->common.rdtype == dns_rdatatype_dnskey);

	generic_freestruct_key(source);
}

static inline isc_result_t
additionaldata_dnskey(ARGS_ADDLDATA) {

	REQUIRE(rdata->type == dns_rdatatype_dnskey);

	UNUSED(rdata);
	UNUSED(add);
	UNUSED(arg);

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
digest_dnskey(ARGS_DIGEST) {
	isc_region_t r;

	REQUIRE(rdata != NULL);
	REQUIRE(rdata->type == dns_rdatatype_dnskey);

	dns_rdata_toregion(rdata, &r);

	return ((digest)(arg, &r));
}

static inline isc_boolean_t
checkowner_dnskey(ARGS_CHECKOWNER) {

	REQUIRE(type == dns_rdatatype_dnskey);

	UNUSED(name);
	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(wildcard);

	return (ISC_TRUE);
}

static inline isc_boolean_t
checknames_dnskey(ARGS_CHECKNAMES) {

	REQUIRE(rdata != NULL);
	REQUIRE(rdata->type == dns_rdatatype_dnskey);

	UNUSED(rdata);
	UNUSED(owner);
	UNUSED(bad);

	return (ISC_TRUE);
}

static inline int
casecompare_dnskey(ARGS_COMPARE) {

	/*
	 * Treat ALG 253 (private DNS) subtype name case sensistively.
	 */
	return (compare_dnskey(rdata1, rdata2));
}

#endif	/* RDATA_GENERIC_DNSKEY_48_C */
