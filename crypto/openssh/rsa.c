/* $OpenBSD: rsa.c,v 1.30 2013/05/17 00:13:14 djm Exp $ */
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

#include "xmalloc.h"
#include "rsa.h"
#include "log.h"

void
rsa_public_encrypt(BIGNUM *out, BIGNUM *in, RSA *key)
{
	u_char *inbuf, *outbuf;
	int len, ilen, olen;

	if (BN_num_bits(key->e) < 2 || !BN_is_odd(key->e))
		fatal("rsa_public_encrypt() exponent too small or not odd");

	olen = BN_num_bytes(key->n);
	outbuf = xmalloc(olen);

	ilen = BN_num_bytes(in);
	inbuf = xmalloc(ilen);
	BN_bn2bin(in, inbuf);

	if ((len = RSA_public_encrypt(ilen, inbuf, outbuf, key,
	    RSA_PKCS1_PADDING)) <= 0)
		fatal("rsa_public_encrypt() failed");

	if (BN_bin2bn(outbuf, len, out) == NULL)
		fatal("rsa_public_encrypt: BN_bin2bn failed");

	memset(outbuf, 0, olen);
	memset(inbuf, 0, ilen);
	free(outbuf);
	free(inbuf);
}

int
rsa_private_decrypt(BIGNUM *out, BIGNUM *in, RSA *key)
{
	u_char *inbuf, *outbuf;
	int len, ilen, olen;

	olen = BN_num_bytes(key->n);
	outbuf = xmalloc(olen);

	ilen = BN_num_bytes(in);
	inbuf = xmalloc(ilen);
	BN_bn2bin(in, inbuf);

	if ((len = RSA_private_decrypt(ilen, inbuf, outbuf, key,
	    RSA_PKCS1_PADDING)) <= 0) {
		error("rsa_private_decrypt() failed");
	} else {
		if (BN_bin2bn(outbuf, len, out) == NULL)
			fatal("rsa_private_decrypt: BN_bin2bn failed");
	}
	memset(outbuf, 0, olen);
	memset(inbuf, 0, ilen);
	free(outbuf);
	free(inbuf);
	return len;
}

/* calculate p-1 and q-1 */
void
rsa_generate_additional_parameters(RSA *rsa)
{
	BIGNUM *aux;
	BN_CTX *ctx;

	if ((aux = BN_new()) == NULL)
		fatal("rsa_generate_additional_parameters: BN_new failed");
	if ((ctx = BN_CTX_new()) == NULL)
		fatal("rsa_generate_additional_parameters: BN_CTX_new failed");

	if ((BN_sub(aux, rsa->q, BN_value_one()) == 0) ||
	    (BN_mod(rsa->dmq1, rsa->d, aux, ctx) == 0) ||
	    (BN_sub(aux, rsa->p, BN_value_one()) == 0) ||
	    (BN_mod(rsa->dmp1, rsa->d, aux, ctx) == 0))
		fatal("rsa_generate_additional_parameters: BN_sub/mod failed");

	BN_clear_free(aux);
	BN_CTX_free(ctx);
}

