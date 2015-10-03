/*
 * Copyright (C) 2012, 2014, 2015  Internet Systems Consortium, Inc. ("ISC")
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

/* rfc6698.txt */

#ifndef RDATA_GENERIC_TLSA_52_C
#define RDATA_GENERIC_TLSA_52_C

#define RRTYPE_TLSA_ATTRIBUTES 0

static inline isc_result_t
fromtext_tlsa(ARGS_FROMTEXT) {
	isc_token_t token;

	REQUIRE(type == dns_rdatatype_tlsa);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(origin);
	UNUSED(options);
	UNUSED(callbacks);

	/*
	 * Certificate Usage.
	 */
	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_number,
				      ISC_FALSE));
	if (token.value.as_ulong > 0xffU)
		RETTOK(ISC_R_RANGE);
	RETERR(uint8_tobuffer(token.value.as_ulong, target));

	/*
	 * Selector.
	 */
	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_number,
				      ISC_FALSE));
	if (token.value.as_ulong > 0xffU)
		RETTOK(ISC_R_RANGE);
	RETERR(uint8_tobuffer(token.value.as_ulong, target));

	/*
	 * Matching type.
	 */
	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_number,
				      ISC_FALSE));
	if (token.value.as_ulong > 0xffU)
		RETTOK(ISC_R_RANGE);
	RETERR(uint8_tobuffer(token.value.as_ulong, target));

	/*
	 * Certificate Association Data.
	 */
	return (isc_hex_tobuffer(lexer, target, -1));
}

static inline isc_result_t
totext_tlsa(ARGS_TOTEXT) {
	isc_region_t sr;
	char buf[sizeof("64000 ")];
	unsigned int n;

	REQUIRE(rdata->type == dns_rdatatype_tlsa);
	REQUIRE(rdata->length != 0);

	UNUSED(tctx);

	dns_rdata_toregion(rdata, &sr);

	/*
	 * Certificate Usage.
	 */
	n = uint8_fromregion(&sr);
	isc_region_consume(&sr, 1);
	sprintf(buf, "%u ", n);
	RETERR(str_totext(buf, target));

	/*
	 * Selector.
	 */
	n = uint8_fromregion(&sr);
	isc_region_consume(&sr, 1);
	sprintf(buf, "%u ", n);
	RETERR(str_totext(buf, target));

	/*
	 * Matching type.
	 */
	n = uint8_fromregion(&sr);
	isc_region_consume(&sr, 1);
	sprintf(buf, "%u", n);
	RETERR(str_totext(buf, target));

	/*
	 * Certificate Association Data.
	 */
	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(str_totext(" (", target));
	RETERR(str_totext(tctx->linebreak, target));
	if (tctx->width == 0) /* No splitting */
		RETERR(isc_hex_totext(&sr, 0, "", target));
	else
		RETERR(isc_hex_totext(&sr, tctx->width - 2,
				      tctx->linebreak, target));
	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(str_totext(" )", target));
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_tlsa(ARGS_FROMWIRE) {
	isc_region_t sr;

	REQUIRE(type == dns_rdatatype_tlsa);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(dctx);
	UNUSED(options);

	isc_buffer_activeregion(source, &sr);

	if (sr.length < 3)
		return (ISC_R_UNEXPECTEDEND);

	isc_buffer_forward(source, sr.length);
	return (mem_tobuffer(target, sr.base, sr.length));
}

static inline isc_result_t
towire_tlsa(ARGS_TOWIRE) {
	isc_region_t sr;

	REQUIRE(rdata->type == dns_rdatatype_tlsa);
	REQUIRE(rdata->length != 0);

	UNUSED(cctx);

	dns_rdata_toregion(rdata, &sr);
	return (mem_tobuffer(target, sr.base, sr.length));
}

static inline int
compare_tlsa(ARGS_COMPARE) {
	isc_region_t r1;
	isc_region_t r2;

	REQUIRE(rdata1->type == rdata2->type);
	REQUIRE(rdata1->rdclass == rdata2->rdclass);
	REQUIRE(rdata1->type == dns_rdatatype_tlsa);
	REQUIRE(rdata1->length != 0);
	REQUIRE(rdata2->length != 0);

	dns_rdata_toregion(rdata1, &r1);
	dns_rdata_toregion(rdata2, &r2);
	return (isc_region_compare(&r1, &r2));
}

static inline isc_result_t
fromstruct_tlsa(ARGS_FROMSTRUCT) {
	dns_rdata_tlsa_t *tlsa = source;

	REQUIRE(type == dns_rdatatype_tlsa);
	REQUIRE(source != NULL);
	REQUIRE(tlsa->common.rdtype == type);
	REQUIRE(tlsa->common.rdclass == rdclass);

	UNUSED(type);
	UNUSED(rdclass);

	RETERR(uint8_tobuffer(tlsa->usage, target));
	RETERR(uint8_tobuffer(tlsa->selector, target));
	RETERR(uint8_tobuffer(tlsa->match, target));

	return (mem_tobuffer(target, tlsa->data, tlsa->length));
}

static inline isc_result_t
tostruct_tlsa(ARGS_TOSTRUCT) {
	dns_rdata_tlsa_t *tlsa = target;
	isc_region_t region;

	REQUIRE(rdata->type == dns_rdatatype_tlsa);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length != 0);

	tlsa->common.rdclass = rdata->rdclass;
	tlsa->common.rdtype = rdata->type;
	ISC_LINK_INIT(&tlsa->common, link);

	dns_rdata_toregion(rdata, &region);

	tlsa->usage = uint8_fromregion(&region);
	isc_region_consume(&region, 1);
	tlsa->selector = uint8_fromregion(&region);
	isc_region_consume(&region, 1);
	tlsa->match = uint8_fromregion(&region);
	isc_region_consume(&region, 1);
	tlsa->length = region.length;

	tlsa->data = mem_maybedup(mctx, region.base, region.length);
	if (tlsa->data == NULL)
		return (ISC_R_NOMEMORY);

	tlsa->mctx = mctx;
	return (ISC_R_SUCCESS);
}

static inline void
freestruct_tlsa(ARGS_FREESTRUCT) {
	dns_rdata_tlsa_t *tlsa = source;

	REQUIRE(tlsa != NULL);
	REQUIRE(tlsa->common.rdtype == dns_rdatatype_tlsa);

	if (tlsa->mctx == NULL)
		return;

	if (tlsa->data != NULL)
		isc_mem_free(tlsa->mctx, tlsa->data);
	tlsa->mctx = NULL;
}

static inline isc_result_t
additionaldata_tlsa(ARGS_ADDLDATA) {
	REQUIRE(rdata->type == dns_rdatatype_tlsa);

	UNUSED(rdata);
	UNUSED(add);
	UNUSED(arg);

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
digest_tlsa(ARGS_DIGEST) {
	isc_region_t r;

	REQUIRE(rdata->type == dns_rdatatype_tlsa);

	dns_rdata_toregion(rdata, &r);

	return ((digest)(arg, &r));
}

static inline isc_boolean_t
checkowner_tlsa(ARGS_CHECKOWNER) {

	REQUIRE(type == dns_rdatatype_tlsa);

	UNUSED(name);
	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(wildcard);

	return (ISC_TRUE);
}

static inline isc_boolean_t
checknames_tlsa(ARGS_CHECKNAMES) {

	REQUIRE(rdata->type == dns_rdatatype_tlsa);

	UNUSED(rdata);
	UNUSED(owner);
	UNUSED(bad);

	return (ISC_TRUE);
}

static inline int
casecompare_tlsa(ARGS_COMPARE) {
	return (compare_tlsa(rdata1, rdata2));
}

#endif	/* RDATA_GENERIC_TLSA_52_C */
