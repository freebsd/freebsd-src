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
RCSID("$OpenBSD: kexdh.c,v 1.18 2002/03/18 17:50:31 provos Exp $");

#include <openssl/crypto.h>
#include <openssl/bn.h>

#include "xmalloc.h"
#include "buffer.h"
#include "bufaux.h"
#include "key.h"
#include "kex.h"
#include "log.h"
#include "packet.h"
#include "dh.h"
#include "ssh2.h"
#include "monitor_wrap.h"

static u_char *
kex_dh_hash(
    char *client_version_string,
    char *server_version_string,
    char *ckexinit, int ckexinitlen,
    char *skexinit, int skexinitlen,
    u_char *serverhostkeyblob, int sbloblen,
    BIGNUM *client_dh_pub,
    BIGNUM *server_dh_pub,
    BIGNUM *shared_secret)
{
	Buffer b;
	static u_char digest[EVP_MAX_MD_SIZE];
	const EVP_MD *evp_md = EVP_sha1();
	EVP_MD_CTX md;

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
	buffer_put_bignum2(&b, client_dh_pub);
	buffer_put_bignum2(&b, server_dh_pub);
	buffer_put_bignum2(&b, shared_secret);

#ifdef DEBUG_KEX
	buffer_dump(&b);
#endif
	EVP_DigestInit(&md, evp_md);
	EVP_DigestUpdate(&md, buffer_ptr(&b), buffer_len(&b));
	EVP_DigestFinal(&md, digest, NULL);

	buffer_free(&b);

#ifdef DEBUG_KEX
	dump_digest("hash", digest, EVP_MD_size(evp_md));
#endif
	return digest;
}

/* client */

static void
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

/* server */

static void
kexdh_server(Kex *kex)
{
	BIGNUM *shared_secret = NULL, *dh_client_pub = NULL;
	DH *dh;
	Key *server_host_key;
	u_char *kbuf, *hash, *signature = NULL, *server_host_key_blob = NULL;
	u_int sbloblen, klen, kout;
	u_int slen;

	/* generate server DH public key */
	dh = dh_new_group1();
	dh_gen_key(dh, kex->we_need * 8);

	debug("expecting SSH2_MSG_KEXDH_INIT");
	packet_read_expect(SSH2_MSG_KEXDH_INIT);

	if (kex->load_host_key == NULL)
		fatal("Cannot load hostkey");
	server_host_key = kex->load_host_key(kex->hostkey_type);
	if (server_host_key == NULL)
		fatal("Unsupported hostkey type %d", kex->hostkey_type);

	/* key, cert */
	if ((dh_client_pub = BN_new()) == NULL)
		fatal("dh_client_pub == NULL");
	packet_get_bignum2(dh_client_pub);
	packet_check_eom();

#ifdef DEBUG_KEXDH
	fprintf(stderr, "dh_client_pub= ");
	BN_print_fp(stderr, dh_client_pub);
	fprintf(stderr, "\n");
	debug("bits %d", BN_num_bits(dh_client_pub));
#endif

#ifdef DEBUG_KEXDH
	DHparams_print_fp(stderr, dh);
	fprintf(stderr, "pub= ");
	BN_print_fp(stderr, dh->pub_key);
	fprintf(stderr, "\n");
#endif
	if (!dh_pub_is_valid(dh, dh_client_pub))
		packet_disconnect("bad client public DH value");

	klen = DH_size(dh);
	kbuf = xmalloc(klen);
	kout = DH_compute_key(kbuf, dh_client_pub, dh);
#ifdef DEBUG_KEXDH
	dump_digest("shared secret", kbuf, kout);
#endif
	if ((shared_secret = BN_new()) == NULL)
		fatal("kexdh_server: BN_new failed");
	BN_bin2bn(kbuf, kout, shared_secret);
	memset(kbuf, 0, klen);
	xfree(kbuf);

	key_to_blob(server_host_key, &server_host_key_blob, &sbloblen);

	/* calc H */
	hash = kex_dh_hash(
	    kex->client_version_string,
	    kex->server_version_string,
	    buffer_ptr(&kex->peer), buffer_len(&kex->peer),
	    buffer_ptr(&kex->my), buffer_len(&kex->my),
	    server_host_key_blob, sbloblen,
	    dh_client_pub,
	    dh->pub_key,
	    shared_secret
	);
	BN_clear_free(dh_client_pub);

	/* save session id := H */
	/* XXX hashlen depends on KEX */
	if (kex->session_id == NULL) {
		kex->session_id_len = 20;
		kex->session_id = xmalloc(kex->session_id_len);
		memcpy(kex->session_id, hash, kex->session_id_len);
	}

	/* sign H */
	/* XXX hashlen depends on KEX */
	PRIVSEP(key_sign(server_host_key, &signature, &slen, hash, 20));

	/* destroy_sensitive_data(); */

	/* send server hostkey, DH pubkey 'f' and singed H */
	packet_start(SSH2_MSG_KEXDH_REPLY);
	packet_put_string(server_host_key_blob, sbloblen);
	packet_put_bignum2(dh->pub_key);	/* f */
	packet_put_string(signature, slen);
	packet_send();

	xfree(signature);
	xfree(server_host_key_blob);
	/* have keys, free DH */
	DH_free(dh);

	kex_derive_keys(kex, hash, shared_secret);
	BN_clear_free(shared_secret);
	kex_finish(kex);
}

void
kexdh(Kex *kex)
{
	if (kex->server)
		kexdh_server(kex);
	else
		kexdh_client(kex);
}
