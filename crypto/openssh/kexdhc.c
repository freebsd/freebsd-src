/*
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
RCSID("$OpenBSD: kexdhc.c,v 1.1 2003/02/16 17:09:57 markus Exp $");

#include "xmalloc.h"
#include "key.h"
#include "kex.h"
#include "log.h"
#include "packet.h"
#include "dh.h"
#include "ssh2.h"

void
kexdh_client(Kex *kex)
{
	BIGNUM *dh_server_pub = NULL, *shared_secret = NULL;
	DH *dh;
	Key *server_host_key;
	u_char *server_host_key_blob = NULL, *signature = NULL;
	u_char *kbuf, *hash;
	u_int klen, kout, slen, sbloblen;

	/* generate and send 'e', client DH public key */
	dh = dh_new_group1();
	dh_gen_key(dh, kex->we_need * 8);
	packet_start(SSH2_MSG_KEXDH_INIT);
	packet_put_bignum2(dh->pub_key);
	packet_send();

	debug("sending SSH2_MSG_KEXDH_INIT");
#ifdef DEBUG_KEXDH
	DHparams_print_fp(stderr, dh);
	fprintf(stderr, "pub= ");
	BN_print_fp(stderr, dh->pub_key);
	fprintf(stderr, "\n");
#endif

	debug("expecting SSH2_MSG_KEXDH_REPLY");
	packet_read_expect(SSH2_MSG_KEXDH_REPLY);

	/* key, cert */
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

	/* DH paramter f, server public DH key */
	if ((dh_server_pub = BN_new()) == NULL)
		fatal("dh_server_pub == NULL");
	packet_get_bignum2(dh_server_pub);

#ifdef DEBUG_KEXDH
	fprintf(stderr, "dh_server_pub= ");
	BN_print_fp(stderr, dh_server_pub);
	fprintf(stderr, "\n");
	debug("bits %d", BN_num_bits(dh_server_pub));
#endif

	/* signed H */
	signature = packet_get_string(&slen);
	packet_check_eom();

	if (!dh_pub_is_valid(dh, dh_server_pub))
		packet_disconnect("bad server public DH value");

	klen = DH_size(dh);
	kbuf = xmalloc(klen);
	kout = DH_compute_key(kbuf, dh_server_pub, dh);
#ifdef DEBUG_KEXDH
	dump_digest("shared secret", kbuf, kout);
#endif
	if ((shared_secret = BN_new()) == NULL)
		fatal("kexdh_client: BN_new failed");
	BN_bin2bn(kbuf, kout, shared_secret);
	memset(kbuf, 0, klen);
	xfree(kbuf);

	/* calc and verify H */
	hash = kex_dh_hash(
	    kex->client_version_string,
	    kex->server_version_string,
	    buffer_ptr(&kex->my), buffer_len(&kex->my),
	    buffer_ptr(&kex->peer), buffer_len(&kex->peer),
	    server_host_key_blob, sbloblen,
	    dh->pub_key,
	    dh_server_pub,
	    shared_secret
	);
	xfree(server_host_key_blob);
	BN_clear_free(dh_server_pub);
	DH_free(dh);

	if (key_verify(server_host_key, signature, slen, hash, 20) != 1)
		fatal("key_verify failed for server_host_key");
	key_free(server_host_key);
	xfree(signature);

	/* save session id */
	if (kex->session_id == NULL) {
		kex->session_id_len = 20;
		kex->session_id = xmalloc(kex->session_id_len);
		memcpy(kex->session_id, hash, kex->session_id_len);
	}

	kex_derive_keys(kex, hash, shared_secret);
	BN_clear_free(shared_secret);
	kex_finish(kex);
}
