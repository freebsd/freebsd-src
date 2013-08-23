/*
 * Copyright (C) 2013  Internet Systems Consortium, Inc. ("ISC")
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

#ifndef RDATA_GENERIC_NID_104_C
#define RDATA_GENERIC_NID_104_C

#include <string.h>

#include <isc/net.h>

#define RRTYPE_NID_ATTRIBUTES (0)

static inline isc_result_t
fromtext_nid(ARGS_FROMTEXT) {
	isc_token_t token;
	unsigned char locator[NS_LOCATORSZ];

	REQUIRE(type == 104);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(origin);
	UNUSED(options);
	UNUSED(callbacks);

	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_number,
				      ISC_FALSE));
	if (token.value.as_ulong > 0xffffU)
		RETTOK(ISC_R_RANGE);
	RETERR(uint16_tobuffer(token.value.as_ulong, target));

	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_string,
				      ISC_FALSE));

	if (locator_pton(DNS_AS_STR(token), locator) != 1)
		RETTOK(DNS_R_SYNTAX);
	return (mem_tobuffer(target, locator, NS_LOCATORSZ));
}

static inline isc_result_t
totext_nid(ARGS_TOTEXT) {
	isc_region_t region;
	char buf[sizeof("xxxx:xxxx:xxxx:xxxx")];
	unsigned short num;

	REQUIRE(rdata->type == 104);
	REQUIRE(rdata->length != 0);

	UNUSED(tctx);

	dns_rdata_toregion(rdata, &region);
	num = uint16_fromregion(&region);
	isc_region_consume(&region, 2);
	sprintf(buf, "%u", num);
	RETERR(str_totext(buf, target));

	RETERR(str_totext(" ", target));

	sprintf(buf, "%x:%x:%x:%x",
		region.base[0]<<8 | region.base[1],
		region.base[2]<<8 | region.base[3],
		region.base[4]<<8 | region.base[5],
		region.base[6]<<8 | region.base[7]);
	return (str_totext(buf, target));
}

static inline isc_result_t
fromwire_nid(ARGS_FROMWIRE) {
	isc_region_t sregion;

	REQUIRE(type == 104);

	UNUSED(type);
	UNUSED(options);
	UNUSED(rdclass);
	UNUSED(dctx);

	isc_buffer_activeregion(source, &sregion);
	if (sregion.length != 10)
		return (DNS_R_FORMERR);
	isc_buffer_forward(source, sregion.length);
	return (mem_tobuffer(target, sregion.base, sregion.length));
}

static inline isc_result_t
towire_nid(ARGS_TOWIRE) {

	REQUIRE(rdata->type == 104);
	REQUIRE(rdata->length == 10);

	UNUSED(cctx);

	return (mem_tobuffer(target, rdata->data, rdata->length));
}

static inline int
compare_nid(ARGS_COMPARE) {
	isc_region_t region1;
	isc_region_t region2;

	REQUIRE(rdata1->type == rdata2->type);
	REQUIRE(rdata1->rdclass == rdata2->rdclass);
	REQUIRE(rdata1->type == 104);
	REQUIRE(rdata1->length == 10);
	REQUIRE(rdata2->length == 10);

	dns_rdata_toregion(rdata1, &region1);
	dns_rdata_toregion(rdata2, &region2);
	return (isc_region_compare(&region1, &region2));
}

static inline isc_result_t
fromstruct_nid(ARGS_FROMSTRUCT) {
	dns_rdata_nid_t *nid = source;

	REQUIRE(type == 104);
	REQUIRE(source != NULL);
	REQUIRE(nid->common.rdtype == type);
	REQUIRE(nid->common.rdclass == rdclass);

	UNUSED(type);
	UNUSED(rdclass);

	RETERR(uint16_tobuffer(nid->pref, target));
	return (mem_tobuffer(target, nid->nid, sizeof(nid->nid)));
}

static inline isc_result_t
tostruct_nid(ARGS_TOSTRUCT) {
	isc_region_t region;
	dns_rdata_nid_t *nid = target;

	REQUIRE(rdata->type == 104);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length == 10);

	UNUSED(mctx);

	nid->common.rdclass = rdata->rdclass;
	nid->common.rdtype = rdata->type;
	ISC_LINK_INIT(&nid->common, link);

	dns_rdata_toregion(rdata, &region);
	nid->pref = uint16_fromregion(&region);
	memcpy(nid->nid, region.base, region.length);
	return (ISC_R_SUCCESS);
}

static inline void
freestruct_nid(ARGS_FREESTRUCT) {
	dns_rdata_nid_t *nid = source;

	REQUIRE(source != NULL);
	REQUIRE(nid->common.rdtype == 104);

	return;
}

static inline isc_result_t
additionaldata_nid(ARGS_ADDLDATA) {

	REQUIRE(rdata->type == 104);
	REQUIRE(rdata->length == 10);

	UNUSED(rdata);
	UNUSED(add);
	UNUSED(arg);

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
digest_nid(ARGS_DIGEST) {
	isc_region_t r;

	REQUIRE(rdata->type == 104);
	REQUIRE(rdata->length == 10);

	dns_rdata_toregion(rdata, &r);

	return ((digest)(arg, &r));
}

static inline isc_boolean_t
checkowner_nid(ARGS_CHECKOWNER) {

	REQUIRE(type == 104);

	UNUSED(name);
	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(wildcard);

	return (ISC_TRUE);
}

static inline isc_boolean_t
checknames_nid(ARGS_CHECKNAMES) {

	REQUIRE(rdata->type == 104);
	REQUIRE(rdata->length == 10);

	UNUSED(rdata);
	UNUSED(owner);
	UNUSED(bad);

	return (ISC_TRUE);
}

static inline int
casecompare_nid(ARGS_COMPARE) {
	return (compare_nid(rdata1, rdata2));
}

#endif	/* RDATA_GENERIC_NID_104_C */
