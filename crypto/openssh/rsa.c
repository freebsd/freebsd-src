/* $OpenBSD: rsa.c,v 1.32 2014/06/24 01:13:21 djm Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 *
 * Copyright (c) 1999 Niels Provos.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Description of the RSA algorithm can be found e.g. from the following
 * sources:
 *
 *   Bruce Schneier: Applied Cryptography.  John Wiley & Sons, 1994.
 *
 *   Jennifer Seberry and Josed Pieprzyk: Cryptography: An Introduction to
 *   Computer Security.  Prentice-Hall, 1989.
 *
 *   Man Young Rhee: Cryptography and Secure Data Communications.  McGraw-Hill,
 *   1994.
 *
 *   R. Rivest, A. Shamir, and L. M. Adleman: Cryptographic Communications
 *   System and Method.  US Patent 4,405,829, 1983.
 *
 *   Hans Riesel: Prime Numbers and Computer Methods for Factorization.
 *   Birkhauser, 1994.
 *
 *   The RSA Frequently Asked Questions document by RSA Data Security,
 *   Inc., 1995.
 *
 *   RSA in 3 lines of perl by Adam Back <aba@atlax.ex.ac.uk>, 1995, as
 * included below:
 *
 *     [gone - had to be deleted - what a pity]
 */

#include "includes.h"

#include <sys/types.h>

#include <stdarg.h>
#include <string.h>

#include "rsa.h"
#include "log.h"
#include "ssherr.h"

int
rsa_public_encrypt(BIGNUM *out, BIGNUM *in, RSA *key)
{
	u_char *inbuf = NULL, *outbuf = NULL;
	int len, ilen, olen, r = SSH_ERR_INTERNAL_ERROR;

	if (BN_num_bits(key->e) < 2 || !BN_is_odd(key->e))
		return SSH_ERR_INVALID_ARGUMENT;

	olen = BN_num_bytes(key->n);
	if ((outbuf = malloc(olen)) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}

	ilen = BN_num_bytes(in);
	if ((inbuf = malloc(ilen)) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	BN_bn2bin(in, inbuf);

	if ((len = RSA_public_encrypt(ilen, inbuf, outbuf, key,
	    RSA_PKCS1_PADDING)) <= 0) {
		r = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}

	if (BN_bin2bn(outbuf, len, out) == NULL) {
		r = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	r = 0;

 out:
	if (outbuf != NULL) {
		explicit_bzero(outbuf, olen);
		free(outbuf);
	}
	if (inbuf != NULL) {
		explicit_bzero(inbuf, ilen);
		free(inbuf);
	}
	return r;
}

int
rsa_private_decrypt(BIGNUM *out, BIGNUM *in, RSA *key)
{
	u_char *inbuf = NULL, *outbuf = NULL;
	int len, ilen, olen, r = SSH_ERR_INTERNAL_ERROR;

	olen = BN_num_bytes(key->n);
	if ((outbuf = malloc(olen)) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}

	ilen = BN_num_bytes(in);
	if ((inbuf = malloc(ilen)) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	BN_bn2bin(in, inbuf);

	if ((len = RSA_private_decrypt(ilen, inbuf, outbuf, key,
	    RSA_PKCS1_PADDING)) <= 0) {
		r = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	} else if (BN_bin2bn(outbuf, len, out) == NULL) {
		r = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	r = 0;
 out:
	if (outbuf != NULL) {
		explicit_bzero(outbuf, olen);
		free(outbuf);
	}
	if (inbuf != NULL) {
		explicit_bzero(inbuf, ilen);
		free(inbuf);
	}
	return r;
}

/* calculate p-1 and q-1 */
int
rsa_generate_additional_parameters(RSA *rsa)
{
	BIGNUM *aux = NULL;
	BN_CTX *ctx = NULL;
	int r;

	if ((ctx = BN_CTX_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((aux = BN_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}

	if ((BN_sub(aux, rsa->q, BN_value_one()) == 0) ||
	    (BN_mod(rsa->dmq1, rsa->d, aux, ctx) == 0) ||
	    (BN_sub(aux, rsa->p, BN_value_one()) == 0) ||
	    (BN_mod(rsa->dmp1, rsa->d, aux, ctx) == 0)) {
		r = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	r = 0;
 out:
	BN_clear_free(aux);
	BN_CTX_free(ctx);
	return r;
}

