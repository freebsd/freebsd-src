/*
 * Copyright (C) 2014  Internet Systems Consortium, Inc. ("ISC")
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

#ifndef RDATA_GENERIC_OPENPGPKEY_61_C
#define RDATA_GENERIC_OPENPGPKEY_61_C

#define RRTYPE_OPENPGPKEY_ATTRIBUTES 0

static inline isc_result_t
fromtext_openpgpkey(ARGS_FROMTEXT) {

	REQUIRE(type == 61);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(callbacks);
	UNUSED(options);
	UNUSED(origin);

	/*
	 * Keyring.
	 */
	return (isc_base64_tobuffer(lexer, target, -1));
}

static inline isc_result_t
totext_openpgpkey(ARGS_TOTEXT) {
	isc_region_t sr;

	REQUIRE(rdata->type == 61);
	REQUIRE(rdata->length != 0);

	dns_rdata_toregion(rdata, &sr);

	/*
	 * Keyring
	 */
	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(str_totext("( ", target));
	if (tctx->width == 0)   /* No splitting */
		RETERR(isc_base64_totext(&sr, 60, "", target));
	else
		RETERR(isc_base64_totext(&sr, tctx->width - 2,
					 tctx->linebreak, target));
	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(str_totext(" )", target));

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_openpgpkey(ARGS_FROMWIRE) {
	isc_region_t sr;

	REQUIRE(type == 61);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(dctx);
	UNUSED(options);

	/*
	 * Keyring.
	 */
	isc_buffer_activeregion(source, &sr);
	if (sr.length < 1)
		return (ISC_R_UNEXPECTEDEND);
	isc_buffer_forward(source, sr.length);
	return (mem_tobuffer(target, sr.base, sr.length));
}

static inline isc_result_t
towire_openpgpkey(ARGS_TOWIRE) {
	isc_region_t sr;

	REQUIRE(rdata->type == 61);
	REQUIRE(rdata->length != 0);

	UNUSED(cctx);

	dns_rdata_toregion(rdata, &sr);
	return (mem_tobuffer(target, sr.base, sr.length));
}

static inline int
compare_openpgpkey(ARGS_COMPARE) {
	isc_region_t r1;
	isc_region_t r2;

	REQUIRE(rdata1->type == rdata2->type);
	REQUIRE(rdata1->rdclass == rdata2->rdclass);
	REQUIRE(rdata1->type == 61);
	REQUIRE(rdata1->length != 0);
	REQUIRE(rdata2->length != 0);

	dns_rdata_toregion(rdata1, &r1);
	dns_rdata_toregion(rdata2, &r2);
	return (isc_region_compare(&r1, &r2));
}

static inline isc_result_t
fromstruct_openpgpkey(ARGS_FROMSTRUCT) {
	dns_rdata_openpgpkey_t *sig = source;

	REQUIRE(type == 61);
	REQUIRE(source != NULL);
	REQUIRE(sig->common.rdtype == type);
	REQUIRE(sig->common.rdclass == rdclass);
	REQUIRE(sig->keyring != NULL && sig->length != 0);

	UNUSED(type);
	UNUSED(rdclass);

	/*
	 * Keyring.
	 */
	return (mem_tobuffer(target, sig->keyring, sig->length));
}

static inline isc_result_t
tostruct_openpgpkey(ARGS_TOSTRUCT) {
	isc_region_t sr;
	dns_rdata_openpgpkey_t *sig = target;

	REQUIRE(rdata->type == 61);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length != 0);

	sig->common.rdclass = rdata->rdclass;
	sig->common.rdtype = rdata->type;
	ISC_LINK_INIT(&sig->common, link);

	dns_rdata_toregion(rdata, &sr);

	/*
	 * Keyring.
	 */
	sig->length = sr.length;
	sig->keyring = mem_maybedup(mctx, sr.base, sig->length);
	if (sig->keyring == NULL)
		goto cleanup;

	sig->mctx = mctx;
	return (ISC_R_SUCCESS);

 cleanup:
	return (ISC_R_NOMEMORY);
}

static inline void
freestruct_openpgpkey(ARGS_FREESTRUCT) {
	dns_rdata_openpgpkey_t *sig = (dns_rdata_openpgpkey_t *) source;

	REQUIRE(source != NULL);
	REQUIRE(sig->common.rdtype == 61);

	if (sig->mctx == NULL)
		return;

	if (sig->keyring != NULL)
		isc_mem_free(sig->mctx, sig->keyring);
	sig->mctx = NULL;
}

static inline isc_result_t
additionaldata_openpgpkey(ARGS_ADDLDATA) {
	REQUIRE(rdata->type == 61);

	UNUSED(rdata);
	UNUSED(add);
	UNUSED(arg);

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
digest_openpgpkey(ARGS_DIGEST) {
	isc_region_t r;

	REQUIRE(rdata->type == 61);

	dns_rdata_toregion(rdata, &r);

	return ((digest)(arg, &r));
}

static inline isc_boolean_t
checkowner_openpgpkey(ARGS_CHECKOWNER) {

	REQUIRE(type == 61);

	UNUSED(name);
	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(wildcard);

	return (ISC_TRUE);
}

static inline isc_boolean_t
checknames_openpgpkey(ARGS_CHECKNAMES) {

	REQUIRE(rdata->type == 61);

	UNUSED(rdata);
	UNUSED(owner);
	UNUSED(bad);

	return (ISC_TRUE);
}

static inline int
casecompare_openpgpkey(ARGS_COMPARE) {
	isc_region_t r1;
	isc_region_t r2;

	REQUIRE(rdata1->type == rdata2->type);
	REQUIRE(rdata1->rdclass == rdata2->rdclass);
	REQUIRE(rdata1->type == 61);
	REQUIRE(rdata1->length != 0);
	REQUIRE(rdata2->length != 0);

	dns_rdata_toregion(rdata1, &r1);
	dns_rdata_toregion(rdata2, &r2);

	return (isc_region_compare(&r1, &r2));
}

#endif	/* RDATA_GENERIC_OPENPGPKEY_61_C */
