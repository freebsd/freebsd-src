/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1995-2003 by Internet Software Consortium
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

#include "minires/minires.h"
#include "arpa/nameser.h"

#include <isc-dhcp/dst.h>

/* res_nsendsigned */
isc_result_t
res_nsendsigned(res_state statp,
		double *msg, unsigned msglen, ns_tsig_key *key,
		double *answer, unsigned anslen, unsigned *anssize)
{
	res_state nstatp;
	DST_KEY *dstkey;
	int usingTCP = 0;
	double *newmsg;
	unsigned newmsglen;
	unsigned bufsize, siglen;
	u_char sig[64];
	HEADER *hp;
	time_t tsig_time;
	unsigned ret;
	isc_result_t rcode;

	dst_init();

	nstatp = (res_state) malloc(sizeof(*statp));
	if (nstatp == NULL)
		return ISC_R_NOMEMORY;
	memcpy(nstatp, statp, sizeof(*statp));

	bufsize = msglen + 1024;
	newmsg = (double *) malloc(bufsize);
	if (newmsg == NULL)
		return ISC_R_NOMEMORY;
	memcpy(newmsg, msg, msglen);
	newmsglen = msglen;

	if (ns_samename(key->alg, NS_TSIG_ALG_HMAC_MD5) != 1)
		dstkey = NULL;
	else
		dstkey = dst_buffer_to_key(key->name, KEY_HMAC_MD5,
					   NS_KEY_TYPE_AUTH_ONLY,
					   NS_KEY_PROT_ANY,
					   key->data, key->len);
	if (dstkey == NULL) {
		free(nstatp);
		free(newmsg);
		return ISC_R_BADKEY;
	}

	nstatp->nscount = 1;
	siglen = sizeof(sig);
	rcode = ns_sign((u_char *)newmsg, &newmsglen, bufsize,
			NOERROR, dstkey, NULL, 0,
			sig, &siglen, 0);
	if (rcode != ISC_R_SUCCESS) {
		free (nstatp);
		free (newmsg);
		return rcode;
	}

	if (newmsglen > PACKETSZ || (nstatp->options & RES_IGNTC))
		usingTCP = 1;
	if (usingTCP == 0)
		nstatp->options |= RES_IGNTC;
	else
		nstatp->options |= RES_USEVC;

retry:

	rcode = res_nsend(nstatp, newmsg, newmsglen, answer, anslen, &ret);
	if (rcode != ISC_R_SUCCESS) {
		free (nstatp);
		free (newmsg);
		return rcode;
	}

	anslen = ret;
	rcode = ns_verify((u_char *)answer, &anslen, dstkey, sig, siglen,
			  NULL, NULL, &tsig_time,
			  (nstatp->options & RES_KEEPTSIG) ? 1 : 0);
	if (rcode != ISC_R_SUCCESS) {
		Dprint(nstatp->pfcode & RES_PRF_REPLY,
		       (stdout, ";; TSIG invalid (%s)\n", p_rcode(ret)));
		free (nstatp);
		free (newmsg);
		return rcode;
	}
	Dprint(nstatp->pfcode & RES_PRF_REPLY, (stdout, ";; TSIG ok\n"));

	hp = (HEADER *) answer;
	if (hp->tc && usingTCP == 0) {
		nstatp->options &= ~RES_IGNTC;
		usingTCP = 1;
		goto retry;
	}

	free (nstatp);
	free (newmsg);
	*anssize = anslen;
	return ISC_R_SUCCESS;
}
