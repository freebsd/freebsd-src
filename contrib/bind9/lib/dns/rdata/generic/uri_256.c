/*
 * Copyright (C) 2011-2013  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: uri_256.c,v 1.2 2011/03/03 14:10:27 fdupont Exp $ */

#ifndef GENERIC_URI_256_C
#define GENERIC_URI_256_C 1

#define RRTYPE_URI_ATTRIBUTES (0)

static inline isc_result_t
fromtext_uri(ARGS_FROMTEXT) {
	isc_token_t token;

	REQUIRE(type == 256);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(origin);
	UNUSED(options);
	UNUSED(callbacks);

	/*
	 * Priority
	 */
	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_number,
				      ISC_FALSE));
	if (token.value.as_ulong > 0xffffU)
		RETTOK(ISC_R_RANGE);
	RETERR(uint16_tobuffer(token.value.as_ulong, target));

	/*
	 * Weight
	 */
	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_number,
				      ISC_FALSE));
	if (token.value.as_ulong > 0xffffU)
		RETTOK(ISC_R_RANGE);
	RETERR(uint16_tobuffer(token.value.as_ulong, target));

	/*
	 * Target URI
	 */
	RETERR(isc_lex_getmastertoken(lexer, &token,
				      isc_tokentype_qstring, ISC_FALSE));
	if (token.type != isc_tokentype_qstring)
		RETTOK(DNS_R_SYNTAX);
	RETTOK(multitxt_fromtext(&token.value.as_textregion, target));
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
totext_uri(ARGS_TOTEXT) {
	isc_region_t region;
	unsigned short priority, weight;
	char buf[sizeof("65000 ")];

	UNUSED(tctx);

	REQUIRE(rdata->type == 256);
	REQUIRE(rdata->length != 0);

	dns_rdata_toregion(rdata, &region);

	/*
	 * Priority
	 */
	priority = uint16_fromregion(&region);
	isc_region_consume(&region, 2);
	sprintf(buf, "%u ", priority);
	RETERR(str_totext(buf, target));

	/*
	 * Weight
	 */
	weight = uint16_fromregion(&region);
	isc_region_consume(&region, 2);
	sprintf(buf, "%u ", weight);
	RETERR(str_totext(buf, target));

	/*
	 * Target URI
	 */
	RETERR(multitxt_totext(&region, target));
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_uri(ARGS_FROMWIRE) {
	isc_region_t region;

	REQUIRE(type == 256);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(dctx);
	UNUSED(options);

	/*
	 * Priority, weight
	 */
	isc_buffer_activeregion(source, &region);
	if (region.length < 4)
		return (ISC_R_UNEXPECTEDEND);
	RETERR(mem_tobuffer(target, region.base, 4));
	isc_buffer_forward(source, 4);

	/*
	 * Target URI
	 */
	RETERR(multitxt_fromwire(source, target));

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
towire_uri(ARGS_TOWIRE) {
	isc_region_t region;

	REQUIRE(rdata->type == 256);
	REQUIRE(rdata->length != 0);

	UNUSED(cctx);

	dns_rdata_toregion(rdata, &region);
	return (mem_tobuffer(target, region.base, region.length));
}

static inline int
compare_uri(ARGS_COMPARE) {
	isc_region_t r1;
	isc_region_t r2;
	int order;

	REQUIRE(rdata1->type == rdata2->type);
	REQUIRE(rdata1->rdclass == rdata2->rdclass);
	REQUIRE(rdata1->type == 256);
	REQUIRE(rdata1->length != 0);
	REQUIRE(rdata2->length != 0);

	dns_rdata_toregion(rdata1, &r1);
	dns_rdata_toregion(rdata2, &r2);

	/*
	 * Priority
	 */
	order = memcmp(r1.base, r2.base, 2);
	if (order != 0)
		return (order < 0 ? -1 : 1);
	isc_region_consume(&r1, 2);
	isc_region_consume(&r2, 2);

	/*
	 * Weight
	 */
	order = memcmp(r1.base, r2.base, 2);
	if (order != 0)
		return (order < 0 ? -1 : 1);
	isc_region_consume(&r1, 2);
	isc_region_consume(&r2, 2);

	return (isc_region_compare(&r1, &r2));
}

static inline isc_result_t
fromstruct_uri(ARGS_FROMSTRUCT) {
	dns_rdata_uri_t *uri = source;
	isc_region_t region;
	isc_uint8_t len;

	REQUIRE(type == 256);
	REQUIRE(source != NULL);
	REQUIRE(uri->common.rdtype == type);
	REQUIRE(uri->common.rdclass == rdclass);
	REQUIRE(uri->target != NULL && uri->tgt_len != 0);

	UNUSED(type);
	UNUSED(rdclass);

	/*
	 * Priority
	 */
	RETERR(uint16_tobuffer(uri->priority, target));

	/*
	 * Weight
	 */
	RETERR(uint16_tobuffer(uri->weight, target));

	/*
	 * Target URI
	 */
	len = 255U;
	region.base = uri->target;
	region.length = uri->tgt_len;
	while (region.length > 0) {
		REQUIRE(len == 255U);
		len = uint8_fromregion(&region);
		isc_region_consume(&region, 1);
		if (region.length < len)
			return (ISC_R_UNEXPECTEDEND);
		isc_region_consume(&region, len);
	}

	return (mem_tobuffer(target, uri->target, uri->tgt_len));
}

static inline isc_result_t
tostruct_uri(ARGS_TOSTRUCT) {
	dns_rdata_uri_t *uri = target;
	isc_region_t sr;

	REQUIRE(rdata->type == 256);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length != 0);

	uri->common.rdclass = rdata->rdclass;
	uri->common.rdtype = rdata->type;
	ISC_LINK_INIT(&uri->common, link);

	dns_rdata_toregion(rdata, &sr);

	/*
	 * Priority
	 */
	if (sr.length < 2)
		return (ISC_R_UNEXPECTEDEND);
	uri->priority = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);

	/*
	 * Weight
	 */
	if (sr.length < 2)
		return (ISC_R_UNEXPECTEDEND);
	uri->weight = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);

	/*
	 * Target URI
	 */
	uri->tgt_len = sr.length;
	uri->target = mem_maybedup(mctx, sr.base, sr.length);
	if (uri->target == NULL)
		return (ISC_R_NOMEMORY);

	uri->mctx = mctx;
	return (ISC_R_SUCCESS);
}

static inline void
freestruct_uri(ARGS_FREESTRUCT) {
	dns_rdata_uri_t *uri = (dns_rdata_uri_t *) source;

	REQUIRE(source != NULL);
	REQUIRE(uri->common.rdtype == 256);

	if (uri->mctx == NULL)
		return;

	if (uri->target != NULL)
		isc_mem_free(uri->mctx, uri->target);
	uri->mctx = NULL;
}

static inline isc_result_t
additionaldata_uri(ARGS_ADDLDATA) {
	REQUIRE(rdata->type == 256);

	UNUSED(rdata);
	UNUSED(add);
	UNUSED(arg);

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
digest_uri(ARGS_DIGEST) {
	isc_region_t r;

	REQUIRE(rdata->type == 256);

	dns_rdata_toregion(rdata, &r);

	return ((digest)(arg, &r));
}

static inline isc_boolean_t
checkowner_uri(ARGS_CHECKOWNER) {

	REQUIRE(type == 256);

	UNUSED(name);
	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(wildcard);

	return (ISC_TRUE);
}

static inline isc_boolean_t
checknames_uri(ARGS_CHECKNAMES) {

	REQUIRE(rdata->type == 256);

	UNUSED(rdata);
	UNUSED(owner);
	UNUSED(bad);

	return (ISC_TRUE);
}

static inline int
casecompare_uri(ARGS_COMPARE) {
	return (compare_uri(rdata1, rdata2));
}

#endif /* GENERIC_URI_256_C */
