/* $OpenBSD: kexc25519c.c,v 1.4 2014/01/12 08:13:13 djm Exp $ */
/*
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
 * Copyright (c) 2010 Damien Miller.  All rights reserved.
 * Copyright (c) 2013 Aris Adamantiadis.  All rights reserved.
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
#include "ssh2.h"

void
kexc25519_client(Kex *kex)
{
	Key *server_host_key;
	u_char client_key[CURVE25519_SIZE];
	u_char client_pubkey[CURVE25519_SIZE];
	u_char *server_pubkey = NULL;
	u_char *server_host_key_blob = NULL, *signature = NULL;
	u_char *hash;
	u_int slen, sbloblen, hashlen;
	Buffer shared_secret;

	kexc25519_keygen(client_key, client_pubkey);

	packet_start(SSH2_MSG_KEX_ECDH_INIT);
	packet_put_string(client_pubkey, sizeof(client_pubkey));
	packet_send();
	debug("sending SSH2_MSG_KEX_ECDH_INIT");

#ifdef DEBUG_KEXECDH
	dump_digest("client private key:", client_key, sizeof(client_key));
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
	server_pubkey = packet_get_string(&slen);
	if (slen != CURVE25519_SIZE)
		fatal("Incorrect size for server Curve25519 pubkey: %d", slen);

#ifdef DEBUG_KEXECDH
	dump_digest("server public key:", server_pubkey, CURVE25519_SIZE);
#endif

	/* signed H */
	signature = packet_get_string(&slen);
	packet_check_eom();

	buffer_init(&shared_secret);
	kexc25519_shared_key(client_key, server_pubkey, &shared_secret);

	/* calc and verify H */
	kex_c25519_hash(
	    kex->hash_alg,
	    kex->client_version_string,
	    kex->server_version_string,
	    buffer_ptr(&kex->my), buffer_len(&kex->my),
	    buffer_ptr(&kex->peer), buffer_len(&kex->peer),
	    server_host_key_blob, sbloblen,
	    client_pubkey,
	    server_pubkey,
	    buffer_ptr(&shared_secret), buffer_len(&shared_secret),
	    &hash, &hashlen
	);
	free(server_host_key_blob);
	free(server_pubkey);
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
	kex_derive_keys(kex, hash, hashlen,
	    buffer_ptr(&shared_secret), buffer_len(&shared_secret));
	buffer_free(&shared_secret);
	kex_finish(kex);
}
