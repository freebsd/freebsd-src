/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1999-2003 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   http://www.isc.org/
 */

#ifndef lint
static const char rcsid[] = "$Id: ns_verify.c,v 1.5.2.3 2004/06/10 17:59:42 dhankins Exp $";
#endif

#define time(x)		trace_mr_time (x)

/* Import. */

#include <sys/types.h>
#include <sys/param.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "minires/minires.h"
#include "arpa/nameser.h"
#include <isc-dhcp/dst.h>

time_t trace_mr_time (time_t *);

/* Private. */

#define BOUNDS_CHECK(ptr, count) \
	do { \
		if ((ptr) + (count) > eom) { \
			return (NS_TSIG_ERROR_FORMERR); \
		} \
	} while (0)

/* Public. */

u_char *
ns_find_tsig(u_char *msg, u_char *eom) {
	HEADER *hp = (HEADER *)msg;
	int n, type;
	u_char *cp = msg, *start;
	isc_result_t status;

	if (msg == NULL || eom == NULL || msg > eom)
		return (NULL);

	if (cp + HFIXEDSZ >= eom)
		return (NULL);

	if (hp->arcount == 0)
		return (NULL);

	cp += HFIXEDSZ;

	status = ns_skiprr(cp, eom, ns_s_qd, ntohs(hp->qdcount), &n);
	if (status != ISC_R_SUCCESS)
		return (NULL);
	cp += n;

	status = ns_skiprr(cp, eom, ns_s_an, ntohs(hp->ancount), &n);
	if (status != ISC_R_SUCCESS)
		return (NULL);
	cp += n;

	status = ns_skiprr(cp, eom, ns_s_ns, ntohs(hp->nscount), &n);
	if (status != ISC_R_SUCCESS)
		return (NULL);
	cp += n;

	status = ns_skiprr(cp, eom, ns_s_ar, ntohs(hp->arcount) - 1, &n);
	if (status != ISC_R_SUCCESS)
		return (NULL);
	cp += n;

	start = cp;
	n = dn_skipname(cp, eom);
	if (n < 0)
		return (NULL);
	cp += n;
	if (cp + INT16SZ >= eom)
		return (NULL);

	GETSHORT(type, cp);
	if (type != ns_t_tsig)
		return (NULL);
	return (start);
}

/* ns_verify
 * Parameters:
 *	statp		res stuff
 *	msg		received message
 *	msglen		length of message
 *	key		tsig key used for verifying.
 *	querysig	(response), the signature in the query
 *	querysiglen	(response), the length of the signature in the query
 *	sig		(query), a buffer to hold the signature
 *	siglen		(query), input - length of signature buffer
 *				 output - length of signature
 *
 * Errors:
 *	- bad input (-1)
 *	- invalid dns message (NS_TSIG_ERROR_FORMERR)
 *	- TSIG is not present (NS_TSIG_ERROR_NO_TSIG)
 *	- key doesn't match (-ns_r_badkey)
 *	- TSIG verification fails with BADKEY (-ns_r_badkey)
 *	- TSIG verification fails with BADSIG (-ns_r_badsig)
 *	- TSIG verification fails with BADTIME (-ns_r_badtime)
 *	- TSIG verification succeeds, error set to BAKEY (ns_r_badkey)
 *	- TSIG verification succeeds, error set to BADSIG (ns_r_badsig)
 *	- TSIG verification succeeds, error set to BADTIME (ns_r_badtime)
 */
isc_result_t
ns_verify(u_char *msg, unsigned *msglen, void *k,
	  const u_char *querysig, unsigned querysiglen,
	  u_char *sig, unsigned *siglen, time_t *timesigned, int nostrip)
{
	HEADER *hp = (HEADER *)msg;
	DST_KEY *key = (DST_KEY *)k;
	u_char *cp = msg, *eom;
	char name[MAXDNAME], alg[MAXDNAME];
	u_char *recstart, *rdatastart;
	u_char *sigstart, *otherstart;
	unsigned n;
	int error;
	u_int16_t type, length;
	u_int16_t fudge, sigfieldlen, id, otherfieldlen;

	dst_init();
	if (msg == NULL || msglen == NULL || *msglen < 0)
		return ISC_R_INVALIDARG;

	eom = msg + *msglen;

	recstart = ns_find_tsig(msg, eom);
	if (recstart == NULL)
		return ISC_R_NO_TSIG;

	cp = recstart;

	/* Read the key name. */
	n = dn_expand(msg, eom, cp, name, MAXDNAME);
	if (n < 0)
		return ISC_R_FORMERR;
	cp += n;

	/* Read the type. */
	BOUNDS_CHECK(cp, 2*INT16SZ + INT32SZ + INT16SZ);
	GETSHORT(type, cp);
	if (type != ns_t_tsig)
		return ISC_R_NO_TSIG;

	/* Skip the class and TTL, save the length. */
	cp += INT16SZ + INT32SZ;
	GETSHORT(length, cp);
	if (eom - cp != length)
		return ISC_R_FORMERR;

	/* Read the algorithm name. */
	rdatastart = cp;
	n = dn_expand(msg, eom, cp, alg, MAXDNAME);
	if (n < 0)
		return ISC_R_FORMERR;
	if (ns_samename(alg, NS_TSIG_ALG_HMAC_MD5) != 1)
		return ISC_R_INVALIDKEY;
	cp += n;

	/* Read the time signed and fudge. */
	BOUNDS_CHECK(cp, INT16SZ + INT32SZ + INT16SZ);
	cp += INT16SZ;
	GETLONG((*timesigned), cp);
	GETSHORT(fudge, cp);

	/* Read the signature. */
	BOUNDS_CHECK(cp, INT16SZ);
	GETSHORT(sigfieldlen, cp);
	BOUNDS_CHECK(cp, sigfieldlen);
	sigstart = cp;
	cp += sigfieldlen;

	/* Read the original id and error. */
	BOUNDS_CHECK(cp, 2*INT16SZ);
	GETSHORT(id, cp);
	GETSHORT(error, cp);

	/* Parse the other data. */
	BOUNDS_CHECK(cp, INT16SZ);
	GETSHORT(otherfieldlen, cp);
	BOUNDS_CHECK(cp, otherfieldlen);
	otherstart = cp;
	cp += otherfieldlen;

	if (cp != eom)
		return ISC_R_FORMERR;

	/* Verify that the key used is OK. */
	if (key != NULL) {
		if (key->dk_alg != KEY_HMAC_MD5)
			return ISC_R_INVALIDKEY;
		if (error != ns_r_badsig && error != ns_r_badkey) {
			if (ns_samename(key->dk_key_name, name) != 1)
				return ISC_R_INVALIDKEY;
		}
	}

	hp->arcount = htons(ntohs(hp->arcount) - 1);

	/*
	 * Do the verification.
	 */

	if (key != NULL && error != ns_r_badsig && error != ns_r_badkey) {
		void *ctx;
		u_char buf[MAXDNAME];

		/* Digest the query signature, if this is a response. */
		dst_verify_data(SIG_MODE_INIT, key, &ctx, NULL, 0, NULL, 0);
		if (querysiglen > 0 && querysig != NULL) {
			u_int16_t len_n = htons(querysiglen);
			dst_verify_data(SIG_MODE_UPDATE, key, &ctx,
					(u_char *)&len_n, INT16SZ, NULL, 0);
			dst_verify_data(SIG_MODE_UPDATE, key, &ctx,
					querysig, querysiglen, NULL, 0);
		}
		
 		/* Digest the message. */
		dst_verify_data(SIG_MODE_UPDATE, key, &ctx, msg,
				(unsigned)(recstart - msg), NULL, 0);

		/* Digest the key name. */
		n = ns_name_ntol(recstart, buf, sizeof(buf));
		dst_verify_data(SIG_MODE_UPDATE, key, &ctx, buf, n, NULL, 0);

		/* Digest the class and TTL. */
		dst_verify_data(SIG_MODE_UPDATE, key, &ctx,
				recstart + dn_skipname(recstart, eom) + INT16SZ,
				INT16SZ + INT32SZ, NULL, 0);

		/* Digest the algorithm. */
		n = ns_name_ntol(rdatastart, buf, sizeof(buf));
		dst_verify_data(SIG_MODE_UPDATE, key, &ctx, buf, n, NULL, 0);

		/* Digest the time signed and fudge. */
		dst_verify_data(SIG_MODE_UPDATE, key, &ctx,
				rdatastart + dn_skipname(rdatastart, eom),
				INT16SZ + INT32SZ + INT16SZ, NULL, 0);

		/* Digest the error and other data. */
		dst_verify_data(SIG_MODE_UPDATE, key, &ctx,
				otherstart - INT16SZ - INT16SZ,
				(unsigned)otherfieldlen + INT16SZ + INT16SZ,
				NULL, 0);

		n = dst_verify_data(SIG_MODE_FINAL, key, &ctx, NULL, 0,
				    sigstart, sigfieldlen);

		if (n < 0)
			return ISC_R_BADSIG;

		if (sig != NULL && siglen != NULL) {
			if (*siglen < sigfieldlen)
				return ISC_R_NOSPACE;
			memcpy(sig, sigstart, sigfieldlen);
			*siglen = sigfieldlen;
		}
	} else {
		if (sigfieldlen > 0)
			return ISC_R_FORMERR;
		if (sig != NULL && siglen != NULL)
			*siglen = 0;
	}

	/* Reset the counter, since we still need to check for badtime. */
	hp->arcount = htons(ntohs(hp->arcount) + 1);

	/* Verify the time. */
	if (abs((*timesigned) - time(NULL)) > fudge)
		return ISC_R_BADTIME;

	if (nostrip == 0) {
		*msglen = recstart - msg;
		hp->arcount = htons(ntohs(hp->arcount) - 1);
	}

	if (error != NOERROR)
		return ns_rcode_to_isc (error);

	return ISC_R_SUCCESS;
}

#if 0
isc_result_t
ns_verify_tcp_init(void *k, const u_char *querysig, unsigned querysiglen,
		   ns_tcp_tsig_state *state)
{
	dst_init();
	if (state == NULL || k == NULL || querysig == NULL || querysiglen < 0)
		return ISC_R_INVALIDARG;
	state->counter = -1;
	state->key = k;
	if (state->key->dk_alg != KEY_HMAC_MD5)
		return ISC_R_BADKEY;
	if (querysiglen > sizeof(state->sig))
		return ISC_R_NOSPACE;
	memcpy(state->sig, querysig, querysiglen);
	state->siglen = querysiglen;
	return ISC_R_SUCCESS;
}

isc_result_t
ns_verify_tcp(u_char *msg, unsigned *msglen, ns_tcp_tsig_state *state,
	      int required)
{
	HEADER *hp = (HEADER *)msg;
	u_char *recstart, *rdatastart, *sigstart;
	unsigned sigfieldlen, otherfieldlen;
	u_char *cp, *eom = msg + *msglen, *cp2;
	char name[MAXDNAME], alg[MAXDNAME];
	u_char buf[MAXDNAME];
	int n, type, length, fudge, id, error;
	time_t timesigned;

	if (msg == NULL || msglen == NULL || state == NULL)
		return ISC_R_INVALIDARG;

	state->counter++;
	if (state->counter == 0)
		return (ns_verify(msg, msglen, state->key,
				  state->sig, state->siglen,
				  state->sig, &state->siglen, &timesigned, 0));

	if (state->siglen > 0) {
		u_int16_t siglen_n = htons(state->siglen);

		dst_verify_data(SIG_MODE_INIT, state->key, &state->ctx,
				NULL, 0, NULL, 0);
		dst_verify_data(SIG_MODE_UPDATE, state->key, &state->ctx,
				(u_char *)&siglen_n, INT16SZ, NULL, 0);
		dst_verify_data(SIG_MODE_UPDATE, state->key, &state->ctx,
				state->sig, state->siglen, NULL, 0);
		state->siglen = 0;
	}

	cp = recstart = ns_find_tsig(msg, eom);

	if (recstart == NULL) {
		if (required)
			return ISC_R_NO_TSIG;
		dst_verify_data(SIG_MODE_UPDATE, state->key, &state->ctx,
				msg, *msglen, NULL, 0);
		return ISC_R_SUCCESS;
	}

	hp->arcount = htons(ntohs(hp->arcount) - 1);
	dst_verify_data(SIG_MODE_UPDATE, state->key, &state->ctx,
			msg, (unsigned)(recstart - msg), NULL, 0);
	
	/* Read the key name. */
	n = dn_expand(msg, eom, cp, name, MAXDNAME);
	if (n < 0)
		return ISC_R_FORMERR;
	cp += n;

	/* Read the type. */
	BOUNDS_CHECK(cp, 2*INT16SZ + INT32SZ + INT16SZ);
	GETSHORT(type, cp);
	if (type != ns_t_tsig)
		return ISC_R_NO_TSIG;

	/* Skip the class and TTL, save the length. */
	cp += INT16SZ + INT32SZ;
	GETSHORT(length, cp);
	if (eom - cp != length)
		return ISC_R_FORMERR;

	/* Read the algorithm name. */
	rdatastart = cp;
	n = dn_expand(msg, eom, cp, alg, MAXDNAME);
	if (n < 0)
		return ISC_R_FORMERR;
	if (ns_samename(alg, NS_TSIG_ALG_HMAC_MD5) != 1)
		return ISC_R_BADKEY;
	cp += n;

	/* Verify that the key used is OK. */
	if ((ns_samename(state->key->dk_key_name, name) != 1 ||
	     state->key->dk_alg != KEY_HMAC_MD5))
		return ISC_R_BADKEY;

	/* Read the time signed and fudge. */
	BOUNDS_CHECK(cp, INT16SZ + INT32SZ + INT16SZ);
	cp += INT16SZ;
	GETLONG(timesigned, cp);
	GETSHORT(fudge, cp);

	/* Read the signature. */
	BOUNDS_CHECK(cp, INT16SZ);
	GETSHORT(sigfieldlen, cp);
	BOUNDS_CHECK(cp, sigfieldlen);
	sigstart = cp;
	cp += sigfieldlen;

	/* Read the original id and error. */
	BOUNDS_CHECK(cp, 2*INT16SZ);
	GETSHORT(id, cp);
	GETSHORT(error, cp);

	/* Parse the other data. */
	BOUNDS_CHECK(cp, INT16SZ);
	GETSHORT(otherfieldlen, cp);
	BOUNDS_CHECK(cp, otherfieldlen);
	cp += otherfieldlen;

	if (cp != eom)
		return ISC_R_FORMERR;

	/*
	 * Do the verification.
	 */

	/* Digest the time signed and fudge. */
	cp2 = buf;
	PUTSHORT(0, cp2);       /* Top 16 bits of time. */
	PUTLONG(timesigned, cp2);
	PUTSHORT(NS_TSIG_FUDGE, cp2);

	dst_verify_data(SIG_MODE_UPDATE, state->key, &state->ctx,
			buf, (unsigned)(cp2 - buf), NULL, 0);

	n = dst_verify_data(SIG_MODE_FINAL, state->key, &state->ctx, NULL, 0,
			    sigstart, sigfieldlen);
	if (n < 0)
		return ISC_R_BADSIG;

	if (sigfieldlen > sizeof(state->sig))
		return ISC_R_BADSIG;

	if (sigfieldlen > sizeof(state->sig))
		return ISC_R_NOSPACE;

	memcpy(state->sig, sigstart, sigfieldlen);
	state->siglen = sigfieldlen;

	/* Verify the time. */
	if (abs(timesigned - time(NULL)) > fudge)
		return ISC_R_BADTIME;

	*msglen = recstart - msg;

	if (error != NOERROR)
		return ns_rcode_to_isc (error);

	return ISC_R_SUCCESS;
}
#endif
