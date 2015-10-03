/*
 * Copyright (C) 2013, 2015  Internet Systems Consortium, Inc. ("ISC")
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

#ifndef RDATA_GENERIC_LP_107_C
#define RDATA_GENERIC_LP_107_C

#include <string.h>

#include <isc/net.h>

#define RRTYPE_LP_ATTRIBUTES (0)

static inline isc_result_t
fromtext_lp(ARGS_FROMTEXT) {
	isc_token_t token;
	dns_name_t name;
	isc_buffer_t buffer;

	REQUIRE(type == dns_rdatatype_lp);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(callbacks);

	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_number,
				      ISC_FALSE));
	if (token.value.as_ulong > 0xffffU)
		RETTOK(ISC_R_RANGE);
	RETERR(uint16_tobuffer(token.value.as_ulong, target));

	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_string,
				      ISC_FALSE));

	dns_name_init(&name, NULL);
	buffer_fromregion(&buffer, &token.value.as_region);
	origin = (origin != NULL) ? origin : dns_rootname;
	return (dns_name_fromtext(&name, &buffer, origin, options, target));
}

static inline isc_result_t
totext_lp(ARGS_TOTEXT) {
	isc_region_t region;
	dns_name_t name;
	dns_name_t prefix;
	isc_boolean_t sub;
	char buf[sizeof("64000")];
	unsigned short num;

	REQUIRE(rdata->type == dns_rdatatype_lp);
	REQUIRE(rdata->length != 0);

	dns_name_init(&name, NULL);
	dns_name_init(&prefix, NULL);

	dns_rdata_toregion(rdata, &region);
	num = uint16_fromregion(&region);
	isc_region_consume(&region, 2);
	sprintf(buf, "%u", num);
	RETERR(str_totext(buf, target));

	RETERR(str_totext(" ", target));

	dns_name_fromregion(&name, &region);
	sub = name_prefix(&name, tctx->origin, &prefix);
	return (dns_name_totext(&prefix, sub, target));
}

static inline isc_result_t
fromwire_lp(ARGS_FROMWIRE) {
	dns_name_t name;
	isc_region_t sregion;

	REQUIRE(type == dns_rdatatype_lp);

	UNUSED(type);
	UNUSED(rdclass);

	dns_decompress_setmethods(dctx, DNS_COMPRESS_GLOBAL14);

	dns_name_init(&name, NULL);

	isc_buffer_activeregion(source, &sregion);
	if (sregion.length < 2)
		return (ISC_R_UNEXPECTEDEND);
	RETERR(mem_tobuffer(target, sregion.base, 2));
	isc_buffer_forward(source, 2);
	return (dns_name_fromwire(&name, source, dctx, options, target));
}

static inline isc_result_t
towire_lp(ARGS_TOWIRE) {

	REQUIRE(rdata->type == dns_rdatatype_lp);
	REQUIRE(rdata->length != 0);

	UNUSED(cctx);

	return (mem_tobuffer(target, rdata->data, rdata->length));
}

static inline int
compare_lp(ARGS_COMPARE) {
	isc_region_t region1;
	isc_region_t region2;

	REQUIRE(rdata1->type == rdata2->type);
	REQUIRE(rdata1->rdclass == rdata2->rdclass);
	REQUIRE(rdata1->type == dns_rdatatype_lp);
	REQUIRE(rdata1->length != 0);
	REQUIRE(rdata2->length != 0);

	dns_rdata_toregion(rdata1, &region1);
	dns_rdata_toregion(rdata2, &region2);

	return (isc_region_compare(&region1, &region2));
}

static inline isc_result_t
fromstruct_lp(ARGS_FROMSTRUCT) {
	dns_rdata_lp_t *lp = source;
	isc_region_t region;

	REQUIRE(type == dns_rdatatype_lp);
	REQUIRE(source != NULL);
	REQUIRE(lp->common.rdtype == type);
	REQUIRE(lp->common.rdclass == rdclass);

	UNUSED(type);
	UNUSED(rdclass);

	RETERR(uint16_tobuffer(lp->pref, target));
	dns_name_toregion(&lp->lp, &region);
	return (isc_buffer_copyregion(target, &region));
}

static inline isc_result_t
tostruct_lp(ARGS_TOSTRUCT) {
	isc_region_t region;
	dns_rdata_lp_t *lp = target;
	dns_name_t name;

	REQUIRE(rdata->type == dns_rdatatype_lp);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length != 0);

	lp->common.rdclass = rdata->rdclass;
	lp->common.rdtype = rdata->type;
	ISC_LINK_INIT(&lp->common, link);

	dns_name_init(&name, NULL);
	dns_rdata_toregion(rdata, &region);
	lp->pref = uint16_fromregion(&region);
	isc_region_consume(&region, 2);
	dns_name_fromregion(&name, &region);
	dns_name_init(&lp->lp, NULL);
	RETERR(name_duporclone(&name, mctx, &lp->lp));
	lp->mctx = mctx;
	return (ISC_R_SUCCESS);
}

static inline void
freestruct_lp(ARGS_FREESTRUCT) {
	dns_rdata_lp_t *lp = source;

	REQUIRE(source != NULL);
	REQUIRE(lp->common.rdtype == dns_rdatatype_lp);

	if (lp->mctx == NULL)
		return;

	dns_name_free(&lp->lp, lp->mctx);
	lp->mctx = NULL;
}

static inline isc_result_t
additionaldata_lp(ARGS_ADDLDATA) {
	dns_name_t name;
	dns_offsets_t offsets;
	isc_region_t region;
	isc_result_t result;

	REQUIRE(rdata->type == dns_rdatatype_lp);

	dns_name_init(&name, offsets);
	dns_rdata_toregion(rdata, &region);
	isc_region_consume(&region, 2);
	dns_name_fromregion(&name, &region);

	result = (add)(arg, &name, dns_rdatatype_l32);
	if (result != ISC_R_SUCCESS)
		return (result);
	return ((add)(arg, &name, dns_rdatatype_l64));
}

static inline isc_result_t
digest_lp(ARGS_DIGEST) {
	isc_region_t region;

	REQUIRE(rdata->type == dns_rdatatype_lp);

	dns_rdata_toregion(rdata, &region);
	return ((digest)(arg, &region));
}

static inline isc_boolean_t
checkowner_lp(ARGS_CHECKOWNER) {

	REQUIRE(type == dns_rdatatype_lp);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(name);
	UNUSED(wildcard);

	return (ISC_TRUE);
}

static inline isc_boolean_t
checknames_lp(ARGS_CHECKNAMES) {

	REQUIRE(rdata->type == dns_rdatatype_lp);

	UNUSED(bad);
	UNUSED(owner);

	return (ISC_TRUE);
}

static inline int
casecompare_lp(ARGS_COMPARE) {
	dns_name_t name1;
	dns_name_t name2;
	isc_region_t region1;
	isc_region_t region2;
	int order;

	REQUIRE(rdata1->type == rdata2->type);
	REQUIRE(rdata1->rdclass == rdata2->rdclass);
	REQUIRE(rdata1->type == dns_rdatatype_lp);
	REQUIRE(rdata1->length != 0);
	REQUIRE(rdata2->length != 0);

	order = memcmp(rdata1->data, rdata2->data, 2);
	if (order != 0)
		return (order < 0 ? -1 : 1);

	dns_name_init(&name1, NULL);
	dns_name_init(&name2, NULL);

	dns_rdata_toregion(rdata1, &region1);
	dns_rdata_toregion(rdata2, &region2);

	isc_region_consume(&region1, 2);
	isc_region_consume(&region2, 2);

	dns_name_fromregion(&name1, &region1);
	dns_name_fromregion(&name2, &region2);

	return (dns_name_rdatacompare(&name1, &name2));
}

#endif	/* RDATA_GENERIC_LP_107_C */
