/* $OpenBSD: kexecdhc.c,v 1.4 2013/05/17 00:13:13 djm Exp $ */
/*
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
 * Copyright (c) 2010 Damien Miller.  All rights reserved.
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

#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "xmalloc.h"
#include "buffer.h"
#include "key.h"
#include "cipher.h"
#include "kex.h"
#include "log.h"
#include "packet.h"
#include "dh.h"
#include "ssh2.h"

#ifdef OPENSSL_HAS_ECC

#include <openssl/ecdh.h>

void
kexecdh_client(Kex *kex)
{
	EC_KEY *client_key;
	EC_POINT *server_public;
	const EC_GROUP *group;
	BIGNUM *shared_secret;
	Key *server_host_key;
	u_char *server_host_key_blob = NULL, *signature = NULL;
	u_char *kbuf, *hash;
	u_int klen, slen, sbloblen, hashlen;

	if ((client_key = EC_KEY_new_by_curve_name(kex->ec_nid)) == NULL)
		fatal("%s: EC_KEY_new_by_curve_name failed", __func__);
	if (EC_KEY_generate_key(client_key) != 1)
		fatal("%s: EC_KEY_generate_key failed", __func__);
	group = EC_KEY_get0_group(client_key);

	packet_start(SSH2_MSG_KEX_ECDH_INIT);
	packet_put_ecpoint(group, EC_KEY_get0_public_key(client_key));
	packet_send();
	debug("sending SSH2_MSG_KEX_ECDH_INIT");

#ifdef DEBUG_KEXECDH
	fputs("client private key:\n", stderr);
	key_dump_ec_key(client_key);
#endif

	debug("expecting SSH2_MSG_KEX_ECDH_REPLY");
	packet_read_expect(SSH2_MSG_KEX_ECDH_REPLY);

	/* hostkey */
	server_host_key_blob = packet_get_string(&sbloblen);
	server_host_key = key_from_blob(server_host_key_blob, sbloblen);
	if (server_host_key == NULL)
		fatal("cannot decode server_host_key_blob");
	if (server_host_key->type != kex->hostkey_type)
		fatal("type mismatch for decoded server_host_key_blob");
	if (kex->verify_host_key == NULL)
		fatal("cannot verify server_host_key");
	if (kex->verify_host_key(server_host_key) == -1)
		fatal("server_host_key verification failed");

	/* Q_S, server public key */
	if ((server_public = EC_POINT_new(group)) == NULL)
		fatal("%s: EC_POINT_new failed", __func__);
	packet_get_ecpoint(group, server_public);

	if (key_ec_validate_public(group, server_public) != 0)
		fatal("%s: invalid server public key", __func__);

#ifdef DEBUG_KEXECDH
	fputs("server public key:\n", stderr);
	key_dump_ec_point(group, server_public);
#endif

	/* signed H */
	signature = packet_get_string(&slen);
	packet_check_eom();

	klen = (EC_GROUP_get_degree(group) + 7) / 8;
	kbuf = xmalloc(klen);
	if (ECDH_compute_key(kbuf, klen, server_public,
	    client_key, NULL) != (int)klen)
		fatal("%s: ECDH_compute_key failed", __func__);

#ifdef DEBUG_KEXECDH
	dump_digest("shared secret", kbuf, klen);
#endif
	if ((shared_secret = BN_new()) == NULL)
		fatal("%s: BN_new failed", __func__);
	if (BN_bin2bn(kbuf, klen, shared_secret) == NULL)
		fatal("%s: BN_bin2bn failed", __func__);
	memset(kbuf, 0, klen);
	free(kbuf);

	/* calc and verify H */
	kex_ecdh_hash(
	    kex->evp_md,
	    group,
	    kex->client_version_string,
	    kex->server_version_string,
	    buffer_ptr(&kex->my), buffer_len(&kex->my),
	    buffer_ptr(&kex->peer), buffer_len(&kex->peer),
	    server_host_key_blob, sbloblen,
	    EC_KEY_get0_public_key(client_key),
	    server_public,
	    shared_secret,
	    &hash, &hashlen
	);
	free(server_host_key_blob);
	EC_POINT_clear_free(server_public);
	EC_KEY_free(client_key);

	if (key_verify(server_host_key, signature, slen, hash, hashlen) != 1)
		fatal("key_verify failed for server_host_key");
	key_free(server_host_key);
	free(signature);

	/* save session id */
	if (kex->session_id == NULL) {
		kex->session_id_len = hashlen;
		kex->session_id = xmalloc(kex->session_id_len);
		memcpy(kex->session_id, hash, kex->session_id_len);
	}

	kex_derive_keys(kex, hash, hashlen, shared_secret);
	BN_clear_free(shared_secret);
	kex_finish(kex);
}
#else /* OPENSSL_HAS_ECC */
void
kexecdh_client(Kex *kex)
{
	fatal("ECC support is not enabled");
}
#endif /* OPENSSL_HAS_ECC */
