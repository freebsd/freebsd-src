/*
 * Copyright (C) 2015  Internet Systems Consortium, Inc. ("ISC")
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

#ifndef RDATA_GENERIC_TALINK_58_C
#define RDATA_GENERIC_TALINK_58_C

#define RRTYPE_TALINK_ATTRIBUTES 0

static inline isc_result_t
fromtext_talink(ARGS_FROMTEXT) {
	isc_token_t token;
	dns_name_t name;
	isc_buffer_t buffer;
	int i;

	REQUIRE(type == dns_rdatatype_talink);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(callbacks);

	if (origin == NULL)
		origin = dns_rootname;

	for (i = 0; i < 2; i++) {
		RETERR(isc_lex_getmastertoken(lexer, &token,
					      isc_tokentype_string,
					      ISC_FALSE));

		dns_name_init(&name, NULL);
		buffer_fromregion(&buffer, &token.value.as_region);
		RETTOK(dns_name_fromtext(&name, &buffer, origin,
					 options, target));
	}

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
totext_talink(ARGS_TOTEXT) {
	isc_region_t dregion;
	dns_name_t prev;
	dns_name_t next;
	dns_name_t prefix;
	isc_boolean_t sub;

	REQUIRE(rdata->type == dns_rdatatype_talink);
	REQUIRE(rdata->length != 0);

	dns_name_init(&prev, NULL);
	dns_name_init(&next, NULL);
	dns_name_init(&prefix, NULL);

	dns_rdata_toregion(rdata, &dregion);

	dns_name_fromregion(&prev, &dregion);
	isc_region_consume(&dregion, name_length(&prev));

	dns_name_fromregion(&next, &dregion);
	isc_region_consume(&dregion, name_length(&next));

	sub = name_prefix(&prev, tctx->origin, &prefix);
	RETERR(dns_name_totext(&prefix, sub, target));

	RETERR(str_totext(" ", target));

	sub = name_prefix(&next, tctx->origin, &prefix);
	return(dns_name_totext(&prefix, sub, target));
}

static inline isc_result_t
fromwire_talink(ARGS_FROMWIRE) {
	dns_name_t prev;
	dns_name_t next;

	REQUIRE(type == dns_rdatatype_talink);

	UNUSED(type);
	UNUSED(rdclass);

	dns_decompress_setmethods(dctx, DNS_COMPRESS_NONE);

	dns_name_init(&prev, NULL);
	dns_name_init(&next, NULL);

	RETERR(dns_name_fromwire(&prev, source, dctx, options, target));
	return(dns_name_fromwire(&next, source, dctx, options, target));
}

static inline isc_result_t
towire_talink(ARGS_TOWIRE) {
	isc_region_t sregion;
	dns_name_t prev;
	dns_name_t next;
	dns_offsets_t moffsets;
	dns_offsets_t roffsets;

	REQUIRE(rdata->type == dns_rdatatype_talink);
	REQUIRE(rdata->length != 0);

	dns_compress_setmethods(cctx, DNS_COMPRESS_NONE);

	dns_name_init(&prev, moffsets);
	dns_name_init(&next, roffsets);

	dns_rdata_toregion(rdata, &sregion);

	dns_name_fromregion(&prev, &sregion);
	isc_region_consume(&sregion, name_length(&prev));
	RETERR(dns_name_towire(&prev, cctx, target));

	dns_name_fromregion(&next, &sregion);
	isc_region_consume(&sregion, name_length(&next));
	return(dns_name_towire(&next, cctx, target));
}

static inline int
compare_talink(ARGS_COMPARE) {
	isc_region_t region1;
	isc_region_t region2;

	REQUIRE(rdata1->type == rdata2->type);
	REQUIRE(rdata1->rdclass == rdata2->rdclass);
	REQUIRE(rdata1->type == dns_rdatatype_talink);
	REQUIRE(rdata1->length != 0);
	REQUIRE(rdata2->length != 0);

	dns_rdata_toregion(rdata1, &region1);
	dns_rdata_toregion(rdata2, &region2);
	return (isc_region_compare(&region1, &region2));
}

static inline isc_result_t
fromstruct_talink(ARGS_FROMSTRUCT) {
	dns_rdata_talink_t *talink = source;
	isc_region_t region;

	REQUIRE(type == dns_rdatatype_talink);
	REQUIRE(source != NULL);
	REQUIRE(talink->common.rdtype == type);
	REQUIRE(talink->common.rdclass == rdclass);

	UNUSED(type);
	UNUSED(rdclass);

	dns_name_toregion(&talink->prev, &region);
	RETERR(isc_buffer_copyregion(target, &region));
	dns_name_toregion(&talink->next, &region);
	return(isc_buffer_copyregion(target, &region));
}

static inline isc_result_t
tostruct_talink(ARGS_TOSTRUCT) {
	isc_region_t region;
	dns_rdata_talink_t *talink = target;
	dns_name_t name;
	isc_result_t result;

	REQUIRE(rdata->type == dns_rdatatype_talink);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length != 0);

	talink->common.rdclass = rdata->rdclass;
	talink->common.rdtype = rdata->type;
	ISC_LINK_INIT(&talink->common, link);

	dns_rdata_toregion(rdata, &region);

	dns_name_init(&name, NULL);
	dns_name_fromregion(&name, &region);
	isc_region_consume(&region, name_length(&name));
	dns_name_init(&talink->prev, NULL);
	RETERR(name_duporclone(&name, mctx, &talink->prev));

	dns_name_fromregion(&name, &region);
	isc_region_consume(&region, name_length(&name));
	dns_name_init(&talink->next, NULL);
	result = name_duporclone(&name, mctx, &talink->next);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	talink->mctx = mctx;
	return (ISC_R_SUCCESS);

 cleanup:
	if (mctx != NULL)
		dns_name_free(&talink->prev, mctx);
	return (ISC_R_NOMEMORY);
}

static inline void
freestruct_talink(ARGS_FREESTRUCT) {
	dns_rdata_talink_t *talink = source;

	REQUIRE(source != NULL);
	REQUIRE(talink->common.rdtype == dns_rdatatype_talink);

	if (talink->mctx == NULL)
		return;

	dns_name_free(&talink->prev, talink->mctx);
	dns_name_free(&talink->next, talink->mctx);
	talink->mctx = NULL;
}

static inline isc_result_t
additionaldata_talink(ARGS_ADDLDATA) {
	UNUSED(rdata);
	UNUSED(add);
	UNUSED(arg);

	REQUIRE(rdata->type == dns_rdatatype_talink);

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
digest_talink(ARGS_DIGEST) {
	isc_region_t r;

	REQUIRE(rdata->type == dns_rdatatype_talink);

	dns_rdata_toregion(rdata, &r);
	return ((digest)(arg, &r));
}

static inline isc_boolean_t
checkowner_talink(ARGS_CHECKOWNER) {

	REQUIRE(type == dns_rdatatype_talink);

	UNUSED(name);
	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(wildcard);

	return (ISC_TRUE);
}

static inline isc_boolean_t
checknames_talink(ARGS_CHECKNAMES) {

	REQUIRE(rdata->type == dns_rdatatype_talink);

	UNUSED(bad);
	UNUSED(owner);

	return (ISC_TRUE);
}

static inline int
casecompare_talink(ARGS_COMPARE) {
	return (compare_talink(rdata1, rdata2));
}

#endif	/* RDATA_GENERIC_TALINK_58_C */
