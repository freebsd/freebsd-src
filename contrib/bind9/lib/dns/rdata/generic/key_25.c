/*
 * Copyright (C) 2004, 2005, 2007, 2009, 2011-2013, 2015  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2003  Internet Software Consortium.
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

#ifndef RDATA_GENERIC_KEY_25_C
#define RDATA_GENERIC_KEY_25_C

#include <dst/dst.h>

#define RRTYPE_KEY_ATTRIBUTES (0)

static inline isc_result_t
generic_fromtext_key(ARGS_FROMTEXT) {
	isc_result_t result;
	isc_token_t token;
	dns_secalg_t alg;
	dns_secproto_t proto;
	dns_keyflags_t flags;

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
generic_totext_key(ARGS_TOTEXT) {
	isc_region_t sr;
	char buf[sizeof("[key id = 64000]")];
	unsigned int flags;
	unsigned char algorithm;
	char algbuf[DNS_NAME_FORMATSIZE];
	const char *keyinfo;
	isc_region_t tmpr;

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
		RETERR(isc_base64_totext(&sr, 60, "", target));
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

		if (rdata->type == dns_rdatatype_dnskey ||
		    rdata->type == dns_rdatatype_cdnskey) {
			RETERR(str_totext(" ; ", target));
			RETERR(str_totext(keyinfo, target));
		}
		RETERR(str_totext("; alg = ", target));
		RETERR(str_totext(algbuf, target));
		RETERR(str_totext(" ; key id = ", target));
		dns_rdata_toregion(rdata, &tmpr);
		sprintf(buf, "%u", dst_region_computeid(&tmpr, algorithm));
		RETERR(str_totext(buf, target));
	}
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
generic_fromwire_key(ARGS_FROMWIRE) {
	unsigned char algorithm;
	isc_region_t sr;

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
fromtext_key(ARGS_FROMTEXT) {

	REQUIRE(type == dns_rdatatype_key);

	return (generic_fromtext_key(rdclass, type, lexer, origin,
				     options, target, callbacks));
}

static inline isc_result_t
totext_key(ARGS_TOTEXT) {

	REQUIRE(rdata != NULL);
	REQUIRE(rdata->type == dns_rdatatype_key);

	return (generic_totext_key(rdata, tctx, target));
}

static inline isc_result_t
fromwire_key(ARGS_FROMWIRE) {

	REQUIRE(type == dns_rdatatype_key);

	return (generic_fromwire_key(rdclass, type, source, dctx,
				     options, target));
}

static inline isc_result_t
towire_key(ARGS_TOWIRE) {
	isc_region_t sr;

	REQUIRE(rdata != NULL);
	REQUIRE(rdata->type == dns_rdatatype_key);
	REQUIRE(rdata->length != 0);

	UNUSED(cctx);

	dns_rdata_toregion(rdata, &sr);
	return (mem_tobuffer(target, sr.base, sr.length));
}

static inline int
compare_key(ARGS_COMPARE) {
	isc_region_t r1;
	isc_region_t r2;

	REQUIRE(rdata1 != NULL);
	REQUIRE(rdata2 != NULL);
	REQUIRE(rdata1->type == rdata2->type);
	REQUIRE(rdata1->rdclass == rdata2->rdclass);
	REQUIRE(rdata1->type == dns_rdatatype_key);
	REQUIRE(rdata1->length != 0);
	REQUIRE(rdata2->length != 0);

	dns_rdata_toregion(rdata1, &r1);
	dns_rdata_toregion(rdata2, &r2);
	return (isc_region_compare(&r1, &r2));
}

static inline isc_result_t
generic_fromstruct_key(ARGS_FROMSTRUCT) {
	dns_rdata_key_t *key = source;

	REQUIRE(key != NULL);
	REQUIRE(key->common.rdtype == type);
	REQUIRE(key->common.rdclass == rdclass);

	UNUSED(type);
	UNUSED(rdclass);

	/* Flags */
	RETERR(uint16_tobuffer(key->flags, target));

	/* Protocol */
	RETERR(uint8_tobuffer(key->protocol, target));

	/* Algorithm */
	RETERR(uint8_tobuffer(key->algorithm, target));

	/* Data */
	return (mem_tobuffer(target, key->data, key->datalen));
}

static inline isc_result_t
generic_tostruct_key(ARGS_TOSTRUCT) {
	dns_rdata_key_t *key = target;
	isc_region_t sr;

	REQUIRE(rdata != NULL);
	REQUIRE(rdata->length != 0);

	REQUIRE(key != NULL);
	REQUIRE(key->common.rdclass == rdata->rdclass);
	REQUIRE(key->common.rdtype == rdata->type);
	REQUIRE(!ISC_LINK_LINKED(&key->common, link));

	dns_rdata_toregion(rdata, &sr);

	/* Flags */
	if (sr.length < 2)
		return (ISC_R_UNEXPECTEDEND);
	key->flags = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);

	/* Protocol */
	if (sr.length < 1)
		return (ISC_R_UNEXPECTEDEND);
	key->protocol = uint8_fromregion(&sr);
	isc_region_consume(&sr, 1);

	/* Algorithm */
	if (sr.length < 1)
		return (ISC_R_UNEXPECTEDEND);
	key->algorithm = uint8_fromregion(&sr);
	isc_region_consume(&sr, 1);

	/* Data */
	key->datalen = sr.length;
	key->data = mem_maybedup(mctx, sr.base, key->datalen);
	if (key->data == NULL)
		return (ISC_R_NOMEMORY);

	key->mctx = mctx;
	return (ISC_R_SUCCESS);
}

static inline void
generic_freestruct_key(ARGS_FREESTRUCT) {
	dns_rdata_key_t *key = (dns_rdata_key_t *) source;

	REQUIRE(key != NULL);

	if (key->mctx == NULL)
		return;

	if (key->data != NULL)
		isc_mem_free(key->mctx, key->data);
	key->mctx = NULL;
}

static inline isc_result_t
fromstruct_key(ARGS_FROMSTRUCT) {

	REQUIRE(type == dns_rdatatype_key);

	return (generic_fromstruct_key(rdclass, type, source, target));
}

static inline isc_result_t
tostruct_key(ARGS_TOSTRUCT) {
	dns_rdata_key_t *key = target;

	REQUIRE(key != NULL);
	REQUIRE(rdata != NULL);
	REQUIRE(rdata->type == dns_rdatatype_key);

	key->common.rdclass = rdata->rdclass;
	key->common.rdtype = rdata->type;
	ISC_LINK_INIT(&key->common, link);

	return (generic_tostruct_key(rdata, target, mctx));
}

static inline void
freestruct_key(ARGS_FREESTRUCT) {
	dns_rdata_key_t *key = (dns_rdata_key_t *) source;

	REQUIRE(key != NULL);
	REQUIRE(key->common.rdtype == dns_rdatatype_key);

	generic_freestruct_key(source);
}

static inline isc_result_t
additionaldata_key(ARGS_ADDLDATA) {

	REQUIRE(rdata != NULL);
	REQUIRE(rdata->type == dns_rdatatype_key);

	UNUSED(rdata);
	UNUSED(add);
	UNUSED(arg);

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
digest_key(ARGS_DIGEST) {
	isc_region_t r;

	REQUIRE(rdata != NULL);
	REQUIRE(rdata->type == dns_rdatatype_key);

	dns_rdata_toregion(rdata, &r);

	return ((digest)(arg, &r));
}

static inline isc_boolean_t
checkowner_key(ARGS_CHECKOWNER) {

	REQUIRE(type == dns_rdatatype_key);

	UNUSED(name);
	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(wildcard);

	return (ISC_TRUE);
}

static inline isc_boolean_t
checknames_key(ARGS_CHECKNAMES) {

	REQUIRE(rdata != NULL);
	REQUIRE(rdata->type == dns_rdatatype_key);

	UNUSED(rdata);
	UNUSED(owner);
	UNUSED(bad);

	return (ISC_TRUE);
}

static inline int
casecompare_key(ARGS_COMPARE) {
	return (compare_key(rdata1, rdata2));
}

#endif	/* RDATA_GENERIC_KEY_25_C */
