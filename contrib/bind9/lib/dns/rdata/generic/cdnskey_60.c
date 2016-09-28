/*
 * Copyright (C) 2014, 2015  Internet Systems Consortium, Inc. ("ISC")
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

#ifndef RDATA_GENERIC_CDNSKEY_60_C
#define RDATA_GENERIC_CDNSKEY_60_C

#include <dst/dst.h>

#define RRTYPE_CDNSKEY_ATTRIBUTES 0

static inline isc_result_t
fromtext_cdnskey(ARGS_FROMTEXT) {

	REQUIRE(type == dns_rdatatype_cdnskey);

	return (generic_fromtext_key(rdclass, type, lexer, origin,
				     options, target, callbacks));
}

static inline isc_result_t
totext_cdnskey(ARGS_TOTEXT) {

	REQUIRE(rdata != NULL);
	REQUIRE(rdata->type == dns_rdatatype_cdnskey);

	return (generic_totext_key(rdata, tctx, target));
}

static inline isc_result_t
fromwire_cdnskey(ARGS_FROMWIRE) {

	REQUIRE(type == dns_rdatatype_cdnskey);

	return (generic_fromwire_key(rdclass, type, source, dctx,
				     options, target));
}

static inline isc_result_t
towire_cdnskey(ARGS_TOWIRE) {
	isc_region_t sr;

	REQUIRE(rdata->type == dns_rdatatype_cdnskey);
	REQUIRE(rdata->length != 0);

	UNUSED(cctx);

	dns_rdata_toregion(rdata, &sr);
	return (mem_tobuffer(target, sr.base, sr.length));
}

static inline int
compare_cdnskey(ARGS_COMPARE) {
	isc_region_t r1;
	isc_region_t r2;

	REQUIRE(rdata1 != NULL);
	REQUIRE(rdata2 != NULL);
	REQUIRE(rdata1->type == rdata2->type);
	REQUIRE(rdata1->rdclass == rdata2->rdclass);
	REQUIRE(rdata1->type == dns_rdatatype_cdnskey);
	REQUIRE(rdata1->length != 0);
	REQUIRE(rdata2->length != 0);

	dns_rdata_toregion(rdata1, &r1);
	dns_rdata_toregion(rdata2, &r2);
	return (isc_region_compare(&r1, &r2));
}

static inline isc_result_t
fromstruct_cdnskey(ARGS_FROMSTRUCT) {

	REQUIRE(type == dns_rdatatype_cdnskey);

	return (generic_fromstruct_key(rdclass, type, source, target));
}

static inline isc_result_t
tostruct_cdnskey(ARGS_TOSTRUCT) {
	dns_rdata_cdnskey_t *dnskey = target;

	REQUIRE(dnskey != NULL);
	REQUIRE(rdata != NULL);
	REQUIRE(rdata->type == dns_rdatatype_cdnskey);

	dnskey->common.rdclass = rdata->rdclass;
	dnskey->common.rdtype = rdata->type;
	ISC_LINK_INIT(&dnskey->common, link);

	return (generic_tostruct_key(rdata, target, mctx));
}

static inline void
freestruct_cdnskey(ARGS_FREESTRUCT) {
	dns_rdata_cdnskey_t *dnskey = (dns_rdata_cdnskey_t *) source;

	REQUIRE(dnskey != NULL);
	REQUIRE(dnskey->common.rdtype == dns_rdatatype_cdnskey);

	generic_freestruct_key(source);
}

static inline isc_result_t
additionaldata_cdnskey(ARGS_ADDLDATA) {
	REQUIRE(rdata->type == dns_rdatatype_cdnskey);

	UNUSED(rdata);
	UNUSED(add);
	UNUSED(arg);

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
digest_cdnskey(ARGS_DIGEST) {
	isc_region_t r;

	REQUIRE(rdata != NULL);
	REQUIRE(rdata->type == dns_rdatatype_cdnskey);

	dns_rdata_toregion(rdata, &r);

	return ((digest)(arg, &r));
}

static inline isc_boolean_t
checkowner_cdnskey(ARGS_CHECKOWNER) {

	REQUIRE(type == dns_rdatatype_cdnskey);

	UNUSED(name);
	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(wildcard);

	return (ISC_TRUE);
}

static inline isc_boolean_t
checknames_cdnskey(ARGS_CHECKNAMES) {

	REQUIRE(rdata != NULL);
	REQUIRE(rdata->type == dns_rdatatype_cdnskey);

	UNUSED(rdata);
	UNUSED(owner);
	UNUSED(bad);

	return (ISC_TRUE);
}

static inline int
casecompare_cdnskey(ARGS_COMPARE) {

	/*
	 * Treat ALG 253 (private DNS) subtype name case sensistively.
	 */
	return (compare_cdnskey(rdata1, rdata2));
}

#endif	/* RDATA_GENERIC_CDNSKEY_60_C */
