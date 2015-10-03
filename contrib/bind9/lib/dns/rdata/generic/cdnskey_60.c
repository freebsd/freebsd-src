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
	isc_result_t result;
	isc_token_t token;
	dns_secalg_t alg;
	dns_secproto_t proto;
	dns_keyflags_t flags;

	REQUIRE(type == dns_rdatatype_cdnskey);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(origin);
	UNUSED(options);
	UNUSED(callbacks);

	/* flags */
	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_string,
				      ISC_FALSE));
	RETTOK(dns_keyflags_fromtext(&flags, &token.value.as_textregion));
	RETERR(uint16_tobuffer(flags, target));

	/* protocol */
	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_string,
				      ISC_FALSE));
	RETTOK(dns_secproto_fromtext(&proto, &token.value.as_textregion));
	RETERR(mem_tobuffer(target, &proto, 1));

	/* algorithm */
	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_string,
				      ISC_FALSE));
	RETTOK(dns_secalg_fromtext(&alg, &token.value.as_textregion));
	RETERR(mem_tobuffer(target, &alg, 1));

	/* No Key? */
	if ((flags & 0xc000) == 0xc000)
		return (ISC_R_SUCCESS);

	result = isc_base64_tobuffer(lexer, target, -1);
	if (result != ISC_R_SUCCESS)
		return (result);

	/* Ensure there's at least enough data to compute a key ID for MD5 */
	if (alg == DST_ALG_RSAMD5 && isc_buffer_usedlength(target) < 7)
		return (ISC_R_UNEXPECTEDEND);

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
totext_cdnskey(ARGS_TOTEXT) {
	isc_region_t sr;
	char buf[sizeof("64000")];
	unsigned int flags;
	unsigned char algorithm;
	char algbuf[DNS_NAME_FORMATSIZE];
	const char *keyinfo;

	REQUIRE(rdata->type == dns_rdatatype_cdnskey);
	REQUIRE(rdata->length != 0);

	dns_rdata_toregion(rdata, &sr);

	/* flags */
	flags = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);
	sprintf(buf, "%u", flags);
	RETERR(str_totext(buf, target));
	RETERR(str_totext(" ", target));
	if ((flags & DNS_KEYFLAG_KSK) != 0) {
		if (flags & DNS_KEYFLAG_REVOKE)
			keyinfo = "revoked KSK";
		else
			keyinfo = "KSK";
	} else
		keyinfo = "ZSK";

	/* protocol */
	sprintf(buf, "%u", sr.base[0]);
	isc_region_consume(&sr, 1);
	RETERR(str_totext(buf, target));
	RETERR(str_totext(" ", target));

	/* algorithm */
	algorithm = sr.base[0];
	sprintf(buf, "%u", algorithm);
	isc_region_consume(&sr, 1);
	RETERR(str_totext(buf, target));

	/* No Key? */
	if ((flags & 0xc000) == 0xc000)
		return (ISC_R_SUCCESS);

	if ((tctx->flags & DNS_STYLEFLAG_RRCOMMENT) != 0 &&
	     algorithm == DNS_KEYALG_PRIVATEDNS) {
		dns_name_t name;
		dns_name_init(&name, NULL);
		dns_name_fromregion(&name, &sr);
		dns_name_format(&name, algbuf, sizeof(algbuf));
	} else {
		dns_secalg_format((dns_secalg_t) algorithm, algbuf,
				  sizeof(algbuf));
	}

	/* key */
	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(str_totext(" (", target));
	RETERR(str_totext(tctx->linebreak, target));
	if (tctx->width == 0)   /* No splitting */
		RETERR(isc_base64_totext(&sr, 0, "", target));
	else
		RETERR(isc_base64_totext(&sr, tctx->width - 2,
					 tctx->linebreak, target));

	if ((tctx->flags & DNS_STYLEFLAG_RRCOMMENT) != 0)
		RETERR(str_totext(tctx->linebreak, target));
	else if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(str_totext(" ", target));

	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(str_totext(")", target));

	if ((tctx->flags & DNS_STYLEFLAG_RRCOMMENT) != 0) {
		isc_region_t tmpr;

		RETERR(str_totext(" ; ", target));
		RETERR(str_totext(keyinfo, target));
		RETERR(str_totext("; alg = ", target));
		RETERR(str_totext(algbuf, target));
		RETERR(str_totext("; key id = ", target));
		dns_rdata_toregion(rdata, &tmpr);
		sprintf(buf, "%u", dst_region_computeid(&tmpr, algorithm));
		RETERR(str_totext(buf, target));
	}
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_cdnskey(ARGS_FROMWIRE) {
	unsigned char algorithm;
	isc_region_t sr;

	REQUIRE(type == dns_rdatatype_cdnskey);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(dctx);
	UNUSED(options);

	isc_buffer_activeregion(source, &sr);
	if (sr.length < 4)
		return (ISC_R_UNEXPECTEDEND);

	algorithm = sr.base[3];
	RETERR(mem_tobuffer(target, sr.base, 4));
	isc_region_consume(&sr, 4);
	isc_buffer_forward(source, 4);

	if (algorithm == DNS_KEYALG_PRIVATEDNS) {
		dns_name_t name;
		dns_decompress_setmethods(dctx, DNS_COMPRESS_NONE);
		dns_name_init(&name, NULL);
		RETERR(dns_name_fromwire(&name, source, dctx, options, target));
	}

	/*
	 * RSAMD5 computes key ID differently from other
	 * algorithms: we need to ensure there's enough data
	 * present for the computation
	 */
	if (algorithm == DST_ALG_RSAMD5 && sr.length < 3)
		return (ISC_R_UNEXPECTEDEND);

	isc_buffer_activeregion(source, &sr);
	isc_buffer_forward(source, sr.length);
	return (mem_tobuffer(target, sr.base, sr.length));
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
	dns_rdata_cdnskey_t *dnskey = source;

	REQUIRE(type == dns_rdatatype_cdnskey);
	REQUIRE(source != NULL);
	REQUIRE(dnskey->common.rdtype == type);
	REQUIRE(dnskey->common.rdclass == rdclass);

	UNUSED(type);
	UNUSED(rdclass);

	/* Flags */
	RETERR(uint16_tobuffer(dnskey->flags, target));

	/* Protocol */
	RETERR(uint8_tobuffer(dnskey->protocol, target));

	/* Algorithm */
	RETERR(uint8_tobuffer(dnskey->algorithm, target));

	/* Data */
	return (mem_tobuffer(target, dnskey->data, dnskey->datalen));
}

static inline isc_result_t
tostruct_cdnskey(ARGS_TOSTRUCT) {
	dns_rdata_cdnskey_t *dnskey = target;
	isc_region_t sr;

	REQUIRE(rdata->type == dns_rdatatype_cdnskey);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length != 0);

	dnskey->common.rdclass = rdata->rdclass;
	dnskey->common.rdtype = rdata->type;
	ISC_LINK_INIT(&dnskey->common, link);

	dns_rdata_toregion(rdata, &sr);

	/* Flags */
	if (sr.length < 2)
		return (ISC_R_UNEXPECTEDEND);
	dnskey->flags = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);

	/* Protocol */
	if (sr.length < 1)
		return (ISC_R_UNEXPECTEDEND);
	dnskey->protocol = uint8_fromregion(&sr);
	isc_region_consume(&sr, 1);

	/* Algorithm */
	if (sr.length < 1)
		return (ISC_R_UNEXPECTEDEND);
	dnskey->algorithm = uint8_fromregion(&sr);
	isc_region_consume(&sr, 1);

	/* Data */
	dnskey->datalen = sr.length;
	dnskey->data = mem_maybedup(mctx, sr.base, dnskey->datalen);
	if (dnskey->data == NULL)
		return (ISC_R_NOMEMORY);

	dnskey->mctx = mctx;
	return (ISC_R_SUCCESS);
}

static inline void
freestruct_cdnskey(ARGS_FREESTRUCT) {
	dns_rdata_cdnskey_t *dnskey = (dns_rdata_cdnskey_t *) source;

	REQUIRE(source != NULL);
	REQUIRE(dnskey->common.rdtype == dns_rdatatype_cdnskey);

	if (dnskey->mctx == NULL)
		return;

	if (dnskey->data != NULL)
		isc_mem_free(dnskey->mctx, dnskey->data);
	dnskey->mctx = NULL;
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
