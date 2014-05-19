/* $OpenBSD: kexgex.c,v 1.28 2014/01/09 23:20:00 djm Exp $ */
/*
 * Copyright (c) 2000 Niels Provos.  All rights reserved.
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
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
 */

#include "includes.h"

#include <sys/types.h>

#include <openssl/evp.h>
#include <signal.h>

#include "buffer.h"
#include "key.h"
#include "cipher.h"
#include "kex.h"
#include "ssh2.h"
#include "digest.h"
#include "log.h"

void
kexgex_hash(
    int hash_alg,
    char *client_version_string,
    char *server_version_string,
    char *ckexinit, int ckexinitlen,
    char *skexinit, int skexinitlen,
    u_char *serverhostkeyblob, int sbloblen,
    int min, int wantbits, int max, BIGNUM *prime, BIGNUM *gen,
    BIGNUM *client_dh_pub,
    BIGNUM *server_dh_pub,
    BIGNUM *shared_secret,
    u_char **hash, u_int *hashlen)
{
	Buffer b;
	static u_char digest[SSH_DIGEST_MAX_LENGTH];

	buffer_init(&b);
	buffer_put_cstring(&b, client_version_string);
	buffer_put_cstring(&b, server_version_string);

	/* kexinit messages: fake header: len+SSH2_MSG_KEXINIT */
	buffer_put_int(&b, ckexinitlen+1);
	buffer_put_char(&b, SSH2_MSG_KEXINIT);
	buffer_append(&b, ckexinit, ckexinitlen);
	buffer_put_int(&b, skexinitlen+1);
	buffer_put_char(&b, SSH2_MSG_KEXINIT);
	buffer_append(&b, skexinit, skexinitlen);

	buffer_put_string(&b, serverhostkeyblob, sbloblen);
	if (min == -1 || max == -1)
		buffer_put_int(&b, wantbits);
	else {
		buffer_put_int(&b, min);
		buffer_put_int(&b, wantbits);
		buffer_put_int(&b, max);
	}
	buffer_put_bignum2(&b, prime);
	buffer_put_bignum2(&b, gen);
	buffer_put_bignum2(&b, client_dh_pub);
	buffer_put_bignum2(&b, server_dh_pub);
	buffer_put_bignum2(&b, shared_secret);

#ifdef DEBUG_KEXDH
	buffer_dump(&b);
#endif
	if (ssh_digest_buffer(hash_alg, &b, digest, sizeof(digest)) != 0)
		fatal("%s: ssh_digest_buffer failed", __func__);

	buffer_free(&b);

#ifdef DEBUG_KEX
	dump_digest("hash", digest, ssh_digest_bytes(hash_alg));
#endif
	*hash = digest;
	*hashlen = ssh_digest_bytes(hash_alg);
}
