/*
 * Copyright (C) 2009, 2011-2013, 2015  Internet Systems Consortium, Inc. ("ISC")
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

#ifndef GENERIC_KEYDATA_65533_C
#define GENERIC_KEYDATA_65533_C 1

#include <isc/time.h>
#include <isc/stdtime.h>

#include <dst/dst.h>

#define RRTYPE_KEYDATA_ATTRIBUTES (0)

static inline isc_result_t
fromtext_keydata(ARGS_FROMTEXT) {
	isc_result_t result;
	isc_token_t token;
	dns_secalg_t alg;
	dns_secproto_t proto;
	dns_keyflags_t flags;
	isc_uint32_t refresh, addhd, removehd;

	REQUIRE(type == dns_rdatatype_keydata);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(origin);
	UNUSED(options);
	UNUSED(callbacks);

	/* refresh timer */
	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_string,
				      ISC_FALSE));
	RETTOK(dns_time32_fromtext(DNS_AS_STR(token), &refresh));
	RETERR(uint32_tobuffer(refresh, target));

	/* add hold-down */
	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_string,
				      ISC_FALSE));
	RETTOK(dns_time32_fromtext(DNS_AS_STR(token), &addhd));
	RETERR(uint32_tobuffer(addhd, target));

	/* remove hold-down */
	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_string,
				      ISC_FALSE));
	RETTOK(dns_time32_fromtext(DNS_AS_STR(token), &removehd));
	RETERR(uint32_tobuffer(removehd, target));

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
	if (alg == DST_ALG_RSAMD5 && isc_buffer_usedlength(target) < 19)
		return (ISC_R_UNEXPECTEDEND);

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
totext_keydata(ARGS_TOTEXT) {
	isc_region_t sr;
	char buf[sizeof("64000")];
	unsigned int flags;
	unsigned char algorithm;
	unsigned long refresh, add, delete;
	char algbuf[DNS_NAME_FORMATSIZE];
	const char *keyinfo;

	REQUIRE(rdata->type == dns_rdatatype_keydata);

	if ((tctx->flags & DNS_STYLEFLAG_KEYDATA) == 0 || rdata->length < 16)
		return (unknown_totext(rdata, tctx, target));

	dns_rdata_toregion(rdata, &sr);

	/* refresh timer */
	refresh = uint32_fromregion(&sr);
	isc_region_consume(&sr, 4);
	RETERR(dns_time32_totext(refresh, target));
	RETERR(str_totext(" ", target));

	/* add hold-down */
	add = uint32_fromregion(&sr);
	isc_region_consume(&sr, 4);
	RETERR(dns_time32_totext(add, target));
	RETERR(str_totext(" ", target));

	/* remove hold-down */
	delete = uint32_fromregion(&sr);
	isc_region_consume(&sr, 4);
	RETERR(dns_time32_totext(delete, target));
	RETERR(str_totext(" ", target));

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
		isc_region_t tmpr;
		char rbuf[ISC_FORMATHTTPTIMESTAMP_SIZE];
		char abuf[ISC_FORMATHTTPTIMESTAMP_SIZE];
		char dbuf[ISC_FORMATHTTPTIMESTAMP_SIZE];
		isc_time_t t;

		RETERR(str_totext(" ; ", target));
		RETERR(str_totext(keyinfo, target));
		dns_secalg_format((dns_secalg_t) algorithm, algbuf,
				  sizeof(algbuf));
		RETERR(str_totext("; alg = ", target));
		RETERR(str_totext(algbuf, target));
		RETERR(str_totext("; key id = ", target));
		dns_rdata_toregion(rdata, &tmpr);
		/* Skip over refresh, addhd, and removehd */
		isc_region_consume(&tmpr, 12);
		sprintf(buf, "%u", dst_region_computeid(&tmpr, algorithm));
		RETERR(str_totext(buf, target));

		if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0) {
			isc_stdtime_t now;

			isc_stdtime_get(&now);

			RETERR(str_totext(tctx->linebreak, target));
			RETERR(str_totext("; next refresh: ", target));
			isc_time_set(&t, refresh, 0);
			isc_time_formathttptimestamp(&t, rbuf, sizeof(rbuf));
			RETERR(str_totext(rbuf, target));

			if (add == 0U) {
				RETERR(str_totext(tctx->linebreak, target));
				RETERR(str_totext("; no trust", target));
			} else {
				RETERR(str_totext(tctx->linebreak, target));
				if (add < now) {
					RETERR(str_totext("; trusted since: ",
							  target));
				} else {
					RETERR(str_totext("; trust pending: ",
							  target));
				}
				isc_time_set(&t, add, 0);
				isc_time_formathttptimestamp(&t, abuf,
							     sizeof(abuf));
				RETERR(str_totext(abuf, target));
			}

			if (delete != 0U) {
				RETERR(str_totext(tctx->linebreak, target));
				RETERR(str_totext("; removal pending: ",
						  target));
				isc_time_set(&t, delete, 0);
				isc_time_formathttptimestamp(&t, dbuf,
							     sizeof(dbuf));
				RETERR(str_totext(dbuf, target));
			}
		}

	}
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_keydata(ARGS_FROMWIRE) {
	isc_region_t sr;

	REQUIRE(type == dns_rdatatype_keydata);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(dctx);
	UNUSED(options);

	isc_buffer_activeregion(source, &sr);
	isc_buffer_forward(source, sr.length);
	return (mem_tobuffer(target, sr.base, sr.length));
}

static inline isc_result_t
towire_keydata(ARGS_TOWIRE) {
	isc_region_t sr;

	REQUIRE(rdata->type == dns_rdatatype_keydata);

	UNUSED(cctx);

	dns_rdata_toregion(rdata, &sr);
	return (mem_tobuffer(target, sr.base, sr.length));
}

static inline int
compare_keydata(ARGS_COMPARE) {
	isc_region_t r1;
	isc_region_t r2;

	REQUIRE(rdata1->type == rdata2->type);
	REQUIRE(rdata1->rdclass == rdata2->rdclass);
	REQUIRE(rdata1->type == dns_rdatatype_keydata);

	dns_rdata_toregion(rdata1, &r1);
	dns_rdata_toregion(rdata2, &r2);
	return (isc_region_compare(&r1, &r2));
}

static inline isc_result_t
fromstruct_keydata(ARGS_FROMSTRUCT) {
	dns_rdata_keydata_t *keydata = source;

	REQUIRE(type == dns_rdatatype_keydata);
	REQUIRE(source != NULL);
	REQUIRE(keydata->common.rdtype == type);
	REQUIRE(keydata->common.rdclass == rdclass);

	UNUSED(type);
	UNUSED(rdclass);

	/* Refresh timer */
	RETERR(uint32_tobuffer(keydata->refresh, target));

	/* Add hold-down */
	RETERR(uint32_tobuffer(keydata->addhd, target));

	/* Remove hold-down */
	RETERR(uint32_tobuffer(keydata->removehd, target));

	/* Flags */
	RETERR(uint16_tobuffer(keydata->flags, target));

	/* Protocol */
	RETERR(uint8_tobuffer(keydata->protocol, target));

	/* Algorithm */
	RETERR(uint8_tobuffer(keydata->algorithm, target));

	/* Data */
	return (mem_tobuffer(target, keydata->data, keydata->datalen));
}

static inline isc_result_t
tostruct_keydata(ARGS_TOSTRUCT) {
	dns_rdata_keydata_t *keydata = target;
	isc_region_t sr;

	REQUIRE(rdata->type == dns_rdatatype_keydata);
	REQUIRE(target != NULL);

	keydata->common.rdclass = rdata->rdclass;
	keydata->common.rdtype = rdata->type;
	ISC_LINK_INIT(&keydata->common, link);

	dns_rdata_toregion(rdata, &sr);

	/* Refresh timer */
	if (sr.length < 4)
		return (ISC_R_UNEXPECTEDEND);
	keydata->refresh = uint32_fromregion(&sr);
	isc_region_consume(&sr, 4);

	/* Add hold-down */
	if (sr.length < 4)
		return (ISC_R_UNEXPECTEDEND);
	keydata->addhd = uint32_fromregion(&sr);
	isc_region_consume(&sr, 4);

	/* Remove hold-down */
	if (sr.length < 4)
		return (ISC_R_UNEXPECTEDEND);
	keydata->removehd = uint32_fromregion(&sr);
	isc_region_consume(&sr, 4);

	/* Flags */
	if (sr.length < 2)
		return (ISC_R_UNEXPECTEDEND);
	keydata->flags = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);

	/* Protocol */
	if (sr.length < 1)
		return (ISC_R_UNEXPECTEDEND);
	keydata->protocol = uint8_fromregion(&sr);
	isc_region_consume(&sr, 1);

	/* Algorithm */
	if (sr.length < 1)
		return (ISC_R_UNEXPECTEDEND);
	keydata->algorithm = uint8_fromregion(&sr);
	isc_region_consume(&sr, 1);

	/* Data */
	keydata->datalen = sr.length;
	keydata->data = mem_maybedup(mctx, sr.base, keydata->datalen);
	if (keydata->data == NULL)
		return (ISC_R_NOMEMORY);

	keydata->mctx = mctx;
	return (ISC_R_SUCCESS);
}

static inline void
freestruct_keydata(ARGS_FREESTRUCT) {
	dns_rdata_keydata_t *keydata = (dns_rdata_keydata_t *) source;

	REQUIRE(source != NULL);
	REQUIRE(keydata->common.rdtype == dns_rdatatype_keydata);

	if (keydata->mctx == NULL)
		return;

	if (keydata->data != NULL)
		isc_mem_free(keydata->mctx, keydata->data);
	keydata->mctx = NULL;
}

static inline isc_result_t
additionaldata_keydata(ARGS_ADDLDATA) {
	REQUIRE(rdata->type == dns_rdatatype_keydata);

	UNUSED(rdata);
	UNUSED(add);
	UNUSED(arg);

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
digest_keydata(ARGS_DIGEST) {
	isc_region_t r;

	REQUIRE(rdata->type == dns_rdatatype_keydata);

	dns_rdata_toregion(rdata, &r);

	return ((digest)(arg, &r));
}

static inline isc_boolean_t
checkowner_keydata(ARGS_CHECKOWNER) {

	REQUIRE(type == dns_rdatatype_keydata);

	UNUSED(name);
	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(wildcard);

	return (ISC_TRUE);
}

static inline isc_boolean_t
checknames_keydata(ARGS_CHECKNAMES) {

	REQUIRE(rdata->type == dns_rdatatype_keydata);

	UNUSED(rdata);
	UNUSED(owner);
	UNUSED(bad);

	return (ISC_TRUE);
}

static inline int
casecompare_keydata(ARGS_COMPARE) {
	return (compare_keydata(rdata1, rdata2));
}

#endif /* GENERIC_KEYDATA_65533_C */
