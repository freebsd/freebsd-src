/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2006 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <opencrypto/xform.h>

#include "glxsb.h"

/*
 * Implementation notes.
 *
 * The Geode LX Security Block provides AES-128-CBC acceleration.
 * We implement all HMAC algorithms provided by crypto(9) framework so glxsb can work
 * with ipsec(4)
 *
 * This code was stolen from crypto/via/padlock_hash.c
 */

MALLOC_DECLARE(M_GLXSB);

static void
glxsb_hash_key_setup(struct glxsb_session *ses, const char *key, int klen)
{
	const struct auth_hash *axf;

	axf = ses->ses_axf;
	hmac_init_ipad(axf, key, klen, ses->ses_ictx);
	hmac_init_opad(axf, key, klen, ses->ses_octx);
}

/*
 * Compute keyed-hash authenticator.
 */
static int
glxsb_authcompute(struct glxsb_session *ses, struct cryptop *crp)
{
	u_char hash[HASH_MAX_LEN];
	const struct auth_hash *axf;
	union authctx ctx;
	int error;

	axf = ses->ses_axf;
	bcopy(ses->ses_ictx, &ctx, axf->ctxsize);
	error = crypto_apply(crp, crp->crp_aad_start, crp->crp_aad_length,
	    axf->Update, &ctx);
	if (error != 0)
		return (error);
	error = crypto_apply(crp, crp->crp_payload_start,
	    crp->crp_payload_length, axf->Update, &ctx);
	if (error != 0)
		return (error);
	
	axf->Final(hash, &ctx);

	bcopy(ses->ses_octx, &ctx, axf->ctxsize);
	axf->Update(&ctx, hash, axf->hashsize);
	axf->Final(hash, &ctx);
	explicit_bzero(&ctx, sizeof(ctx));

	/* Verify or inject the authentication data */
	if (crp->crp_op & CRYPTO_OP_VERIFY_DIGEST) {
		u_char hash2[HASH_MAX_LEN];

		crypto_copydata(crp, crp->crp_digest_start, ses->ses_mlen,
		    hash2);
		if (timingsafe_bcmp(hash, hash2, ses->ses_mlen) != 0)
			error = EBADMSG;
		explicit_bzero(hash2, sizeof(hash2));
	} else
		crypto_copyback(crp, crp->crp_digest_start, ses->ses_mlen,
		    hash);
	explicit_bzero(hash, sizeof(hash));
	return (error);
}

int
glxsb_hash_setup(struct glxsb_session *ses,
    const struct crypto_session_params *csp)
{

	ses->ses_axf = crypto_auth_hash(csp);
	if (csp->csp_auth_mlen == 0)
		ses->ses_mlen = ses->ses_axf->hashsize;
	else
		ses->ses_mlen = csp->csp_auth_mlen;

	/* Allocate memory for HMAC inner and outer contexts. */
	ses->ses_ictx = malloc(ses->ses_axf->ctxsize, M_GLXSB,
	    M_ZERO | M_NOWAIT);
	ses->ses_octx = malloc(ses->ses_axf->ctxsize, M_GLXSB,
	    M_ZERO | M_NOWAIT);
	if (ses->ses_ictx == NULL || ses->ses_octx == NULL)
		return (ENOMEM);

	/* Setup key if given. */
	if (csp->csp_auth_key != NULL) {
		glxsb_hash_key_setup(ses, csp->csp_auth_key,
		    csp->csp_auth_klen);
	}
	return (0);
}

int
glxsb_hash_process(struct glxsb_session *ses,
    const struct crypto_session_params *csp, struct cryptop *crp)
{
	int error;

	if (crp->crp_auth_key != NULL)
		glxsb_hash_key_setup(ses, crp->crp_auth_key,
		    csp->csp_auth_klen);

	error = glxsb_authcompute(ses, crp);
	return (error);
}

void
glxsb_hash_free(struct glxsb_session *ses)
{

	if (ses->ses_ictx != NULL) {
		zfree(ses->ses_ictx, M_GLXSB);
		ses->ses_ictx = NULL;
	}
	if (ses->ses_octx != NULL) {
		zfree(ses->ses_octx, M_GLXSB);
		ses->ses_octx = NULL;
	}
}
