/*
 * Copyright (C) 2009, 2010  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: tsec.c,v 1.7 2010-12-09 00:54:34 marka Exp $ */

#include <config.h>

#include <isc/mem.h>

#include <dns/tsec.h>
#include <dns/tsig.h>
#include <dns/result.h>

#include <dst/dst.h>

#define DNS_TSEC_MAGIC			ISC_MAGIC('T', 's', 'e', 'c')
#define DNS_TSEC_VALID(t)		ISC_MAGIC_VALID(t, DNS_TSEC_MAGIC)

/*%
 * DNS Transaction Security object.  We assume this is not shared by
 * multiple threads, and so the structure does not contain a lock.
 */
struct dns_tsec {
	unsigned int		magic;
	dns_tsectype_t		type;
	isc_mem_t		*mctx;
	union {
		dns_tsigkey_t	*tsigkey;
		dst_key_t	*key;
	} ukey;
};

isc_result_t
dns_tsec_create(isc_mem_t *mctx, dns_tsectype_t type, dst_key_t *key,
		dns_tsec_t **tsecp)
{
	isc_result_t result;
	dns_tsec_t *tsec;
	dns_tsigkey_t *tsigkey = NULL;
	dns_name_t *algname;

	REQUIRE(mctx != NULL);
	REQUIRE(tsecp != NULL && *tsecp == NULL);

	tsec = isc_mem_get(mctx, sizeof(*tsec));
	if (tsec == NULL)
		return (ISC_R_NOMEMORY);

	tsec->type = type;
	tsec->mctx = mctx;

	switch (type) {
	case dns_tsectype_tsig:
		switch (dst_key_alg(key)) {
		case DST_ALG_HMACMD5:
			algname = dns_tsig_hmacmd5_name;
			break;
		case DST_ALG_HMACSHA1:
			algname = dns_tsig_hmacsha1_name;
			break;
		case DST_ALG_HMACSHA224:
			algname = dns_tsig_hmacsha224_name;
			break;
		case DST_ALG_HMACSHA256:
			algname = dns_tsig_hmacsha256_name;
			break;
		case DST_ALG_HMACSHA384:
			algname = dns_tsig_hmacsha384_name;
			break;
		case DST_ALG_HMACSHA512:
			algname = dns_tsig_hmacsha512_name;
			break;
		default:
			isc_mem_put(mctx, tsec, sizeof(*tsec));
			return (DNS_R_BADALG);
		}
		result = dns_tsigkey_createfromkey(dst_key_name(key),
						   algname, key, ISC_FALSE,
						   NULL, 0, 0, mctx, NULL,
						   &tsigkey);
		if (result != ISC_R_SUCCESS) {
			isc_mem_put(mctx, tsec, sizeof(*tsec));
			return (result);
		}
		tsec->ukey.tsigkey = tsigkey;
		break;
	case dns_tsectype_sig0:
		tsec->ukey.key = key;
		break;
	default:
		INSIST(0);
	}

	tsec->magic = DNS_TSEC_MAGIC;

	*tsecp = tsec;
	return (ISC_R_SUCCESS);
}

void
dns_tsec_destroy(dns_tsec_t **tsecp) {
	dns_tsec_t *tsec;

	REQUIRE(tsecp != NULL && *tsecp != NULL);
	tsec = *tsecp;
	REQUIRE(DNS_TSEC_VALID(tsec));

	switch (tsec->type) {
	case dns_tsectype_tsig:
		dns_tsigkey_detach(&tsec->ukey.tsigkey);
		break;
	case dns_tsectype_sig0:
		dst_key_free(&tsec->ukey.key);
		break;
	default:
		INSIST(0);
	}

	tsec->magic = 0;
	isc_mem_put(tsec->mctx, tsec, sizeof(*tsec));

	*tsecp = NULL;
}

dns_tsectype_t
dns_tsec_gettype(dns_tsec_t *tsec) {
	REQUIRE(DNS_TSEC_VALID(tsec));

	return (tsec->type);
}

void
dns_tsec_getkey(dns_tsec_t *tsec, void *keyp) {
	REQUIRE(DNS_TSEC_VALID(tsec));
	REQUIRE(keyp != NULL);

	switch (tsec->type) {
	case dns_tsectype_tsig:
		dns_tsigkey_attach(tsec->ukey.tsigkey, (dns_tsigkey_t **)keyp);
		break;
	case dns_tsectype_sig0:
		*(dst_key_t **)keyp = tsec->ukey.key;
		break;
	default:
		INSIST(0);
	}
}
