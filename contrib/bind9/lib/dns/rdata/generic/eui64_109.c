/*
 * Copyright (C) 2013-2015  Internet Systems Consortium, Inc. ("ISC")
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

#ifndef RDATA_GENERIC_EUI64_109_C
#define RDATA_GENERIC_EUI64_109_C

#include <string.h>

#define RRTYPE_EUI64_ATTRIBUTES (0)

static inline isc_result_t
fromtext_eui64(ARGS_FROMTEXT) {
	isc_token_t token;
	unsigned char eui64[8];
	unsigned int l0, l1, l2, l3, l4, l5, l6, l7;
	int n;

	REQUIRE(type == dns_rdatatype_eui64);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(origin);
	UNUSED(options);
	UNUSED(callbacks);

	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_string,
				      ISC_FALSE));
	n = sscanf(DNS_AS_STR(token), "%2x-%2x-%2x-%2x-%2x-%2x-%2x-%2x",
		   &l0, &l1, &l2, &l3, &l4, &l5, &l6, &l7);
	if (n != 8 || l0 > 255U || l1 > 255U || l2 > 255U || l3 > 255U ||
	    l4 > 255U || l5 > 255U || l6 > 255U || l7 > 255U)
		return (DNS_R_BADEUI);

	eui64[0] = l0;
	eui64[1] = l1;
	eui64[2] = l2;
	eui64[3] = l3;
	eui64[4] = l4;
	eui64[5] = l5;
	eui64[6] = l6;
	eui64[7] = l7;
	return (mem_tobuffer(target, eui64, sizeof(eui64)));
}

static inline isc_result_t
totext_eui64(ARGS_TOTEXT) {
	char buf[sizeof("xx-xx-xx-xx-xx-xx-xx-xx")];

	REQUIRE(rdata->type == dns_rdatatype_eui64);
	REQUIRE(rdata->length == 8);

	UNUSED(tctx);

	(void)snprintf(buf, sizeof(buf),
		       "%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x",
		       rdata->data[0], rdata->data[1],
		       rdata->data[2], rdata->data[3],
		       rdata->data[4], rdata->data[5],
		       rdata->data[6], rdata->data[7]);
	return (str_totext(buf, target));
}

static inline isc_result_t
fromwire_eui64(ARGS_FROMWIRE) {
	isc_region_t sregion;

	REQUIRE(type == dns_rdatatype_eui64);

	UNUSED(type);
	UNUSED(options);
	UNUSED(rdclass);
	UNUSED(dctx);

	isc_buffer_activeregion(source, &sregion);
	if (sregion.length != 8)
		return (DNS_R_FORMERR);
	isc_buffer_forward(source, sregion.length);
	return (mem_tobuffer(target, sregion.base, sregion.length));
}

static inline isc_result_t
towire_eui64(ARGS_TOWIRE) {

	REQUIRE(rdata->type == dns_rdatatype_eui64);
	REQUIRE(rdata->length == 8);

	UNUSED(cctx);

	return (mem_tobuffer(target, rdata->data, rdata->length));
}

static inline int
compare_eui64(ARGS_COMPARE) {
	isc_region_t region1;
	isc_region_t region2;

	REQUIRE(rdata1->type == rdata2->type);
	REQUIRE(rdata1->rdclass == rdata2->rdclass);
	REQUIRE(rdata1->type == dns_rdatatype_eui64);
	REQUIRE(rdata1->length == 8);
	REQUIRE(rdata2->length == 8);

	dns_rdata_toregion(rdata1, &region1);
	dns_rdata_toregion(rdata2, &region2);
	return (isc_region_compare(&region1, &region2));
}

static inline isc_result_t
fromstruct_eui64(ARGS_FROMSTRUCT) {
	dns_rdata_eui64_t *eui64 = source;

	REQUIRE(type == dns_rdatatype_eui64);
	REQUIRE(source != NULL);
	REQUIRE(eui64->common.rdtype == type);
	REQUIRE(eui64->common.rdclass == rdclass);

	UNUSED(type);
	UNUSED(rdclass);

	return (mem_tobuffer(target, eui64->eui64, sizeof(eui64->eui64)));
}

static inline isc_result_t
tostruct_eui64(ARGS_TOSTRUCT) {
	dns_rdata_eui64_t *eui64 = target;

	REQUIRE(rdata->type == dns_rdatatype_eui64);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length == 8);

	UNUSED(mctx);

	eui64->common.rdclass = rdata->rdclass;
	eui64->common.rdtype = rdata->type;
	ISC_LINK_INIT(&eui64->common, link);

	memmove(eui64->eui64, rdata->data, rdata->length);
	return (ISC_R_SUCCESS);
}

static inline void
freestruct_eui64(ARGS_FREESTRUCT) {
	dns_rdata_eui64_t *eui64 = source;

	REQUIRE(source != NULL);
	REQUIRE(eui64->common.rdtype == dns_rdatatype_eui64);

	return;
}

static inline isc_result_t
additionaldata_eui64(ARGS_ADDLDATA) {

	REQUIRE(rdata->type == dns_rdatatype_eui64);
	REQUIRE(rdata->length == 8);

	UNUSED(rdata);
	UNUSED(add);
	UNUSED(arg);

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
digest_eui64(ARGS_DIGEST) {
	isc_region_t r;

	REQUIRE(rdata->type == dns_rdatatype_eui64);
	REQUIRE(rdata->length == 8);

	dns_rdata_toregion(rdata, &r);

	return ((digest)(arg, &r));
}

static inline isc_boolean_t
checkowner_eui64(ARGS_CHECKOWNER) {

	REQUIRE(type == dns_rdatatype_eui64);

	UNUSED(name);
	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(wildcard);

	return (ISC_TRUE);
}

static inline isc_boolean_t
checknames_eui64(ARGS_CHECKNAMES) {

	REQUIRE(rdata->type == dns_rdatatype_eui64);
	REQUIRE(rdata->length == 8);

	UNUSED(rdata);
	UNUSED(owner);
	UNUSED(bad);

	return (ISC_TRUE);
}

static inline int
casecompare_eui64(ARGS_COMPARE) {
	return (compare_eui64(rdata1, rdata2));
}

#endif	/* RDATA_GENERIC_EUI64_109_C */
