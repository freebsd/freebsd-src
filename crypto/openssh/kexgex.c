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
RCSID("$OpenBSD: kexgex.c,v 1.22 2002/03/24 17:27:03 stevesk Exp $");

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
#include "compat.h"
#include "monitor_wrap.h"

static u_char *
kexgex_hash(
    char *client_version_string,
    char *server_version_string,
    char *ckexinit, int ckexinitlen,
    char *skexinit, int skexinitlen,
    u_char *serverhostkeyblob, int sbloblen,
    int min, int wantbits, int max, BIGNUM *prime, BIGNUM *gen,
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
	EVP_DigestInit(&md, evp_md);
	EVP_DigestUpdate(&md, buffer_ptr(&b), buffer_len(&b));
	EVP_DigestFinal(&md, digest, NULL);

	buffer_free(&b);

#ifdef DEBUG_KEXDH
	dump_digest("hash", digest, EVP_MD_size(evp_md));
#endif
	return digest;
}

/* client */

static void
kexgex_client(Kex *kex)
{
	BIGNUM *dh_server_pub = NULL, *shared_secret = NULL;
	BIGNUM *p = NULL, *g = NULL;
	Key *server_host_key;
	u_char *kbuf, *hash, *signature = NULL, *server_host_key_blob = NULL;
	u_int klen, kout, slen, sbloblen;
	int min, max, nbits;
	DH *dh;

	nbits = dh_estimate(kex->we_need * 8);

	if (datafellows & SSH_OLD_DHGEX) {
		debug("SSH2_MSG_KEX_DH_GEX_REQUEST_OLD sent");

		/* Old GEX request */
		packet_start(SSH2_MSG_KEX_DH_GEX_REQUEST_OLD);
		packet_put_int(nbits);
		min = DH_GRP_MIN;
		max = DH_GRP_MAX;
	} else {
		debug("SSH2_MSG_KEX_DH_GEX_REQUEST sent");

		/* New GEX request */
		min = DH_GRP_MIN;
		max = DH_GRP_MAX;
		packet_start(SSH2_MSG_KEX_DH_GEX_REQUEST);
		packet_put_int(min);
		packet_put_int(nbits);
		packet_put_int(max);
	}
#ifdef DEBUG_KEXDH
	fprintf(stderr, "\nmin = %d, nbits = %d, max = %d\n",
	    min, nbits, max);
#endif
	packet_send();

	debug("expecting SSH2_MSG_KEX_DH_GEX_GROUP");
	packet_read_expect(SSH2_MSG_KEX_DH_GEX_GROUP);

	if ((p = BN_new()) == NULL)
		fatal("BN_new");
	packet_get_bignum2(p);
	if ((g = BN_new()) == NULL)
		fatal("BN_new");
	packet_get_bignum2(g);
	packet_check_eom();

	if (BN_num_bits(p) < min || BN_num_bits(p) > max)
		fatal("DH_GEX group out of range: %d !< %d !< %d",
		    min, BN_num_bits(p), max);

	dh = dh_new_group(g, p);
	dh_gen_key(dh, kex->we_need * 8);

#ifdef DEBUG_KEXDH
	DHparams_print_fp(stderr, dh);
	fprintf(stderr, "pub= ");
	BN_print_fp(stderr, dh->pub_key);
	fprintf(stderr, "\n");
#endif

	debug("SSH2_MSG_KEX_DH_GEX_INIT sent");
	/* generate and send 'e', client DH public key */
	packet_start(SSH2_MSG_KEX_DH_GEX_INIT);
	packet_put_bignum2(dh->pub_key);
	packet_send();

	debug("expecting SSH2_MSG_KEX_DH_GEX_REPLY");
	packet_read_expect(SSH2_MSG_KEX_DH_GEX_REPLY);

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
		fatal("kexgex_client: BN_new failed");
	BN_bin2bn(kbuf, kout, shared_secret);
	memset(kbuf, 0, klen);
	xfree(kbuf);

	if (datafellows & SSH_OLD_DHGEX)
		min = max = -1;

	/* calc and verify H */
	hash = kexgex_hash(
	    kex->client_version_string,
	    kex->server_version_string,
	    buffer_ptr(&kex->my), buffer_len(&kex->my),
	    buffer_ptr(&kex->peer), buffer_len(&kex->peer),
	    server_host_key_blob, sbloblen,
	    min, nbits, max,
	    dh->p, dh->g,
	    dh->pub_key,
	    dh_server_pub,
	    shared_secret
	);
	/* have keys, free DH */
	DH_free(dh);
	xfree(server_host_key_blob);
	BN_clear_free(dh_server_pub);

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
kexgex_server(Kex *kex)
{
	BIGNUM *shared_secret = NULL, *dh_client_pub = NULL;
	Key *server_host_key;
	DH *dh;
	u_char *kbuf, *hash, *signature = NULL, *server_host_key_blob = NULL;
	u_int sbloblen, klen, kout, slen;
	int min = -1, max = -1, nbits = -1, type;

	if (kex->load_host_key == NULL)
		fatal("Cannot load hostkey");
	server_host_key = kex->load_host_key(kex->hostkey_type);
	if (server_host_key == NULL)
		fatal("Unsupported hostkey type %d", kex->hostkey_type);

	type = packet_read();
	switch (type) {
	case SSH2_MSG_KEX_DH_GEX_REQUEST:
		debug("SSH2_MSG_KEX_DH_GEX_REQUEST received");
		min = packet_get_int();
		nbits = packet_get_int();
		max = packet_get_int();
		min = MAX(DH_GRP_MIN, min);
		max = MIN(DH_GRP_MAX, max);
		break;
	case SSH2_MSG_KEX_DH_GEX_REQUEST_OLD:
		debug("SSH2_MSG_KEX_DH_GEX_REQUEST_OLD received");
		nbits = packet_get_int();
		min = DH_GRP_MIN;
		max = DH_GRP_MAX;
		/* unused for old GEX */
		break;
	default:
		fatal("protocol error during kex, no DH_GEX_REQUEST: %d", type);
	}
	packet_check_eom();

	if (max < min || nbits < min || max < nbits)
		fatal("DH_GEX_REQUEST, bad parameters: %d !< %d !< %d",
		    min, nbits, max);

	/* Contact privileged parent */
	dh = PRIVSEP(choose_dh(min, nbits, max));
	if (dh == NULL)
		packet_disconnect("Protocol error: no matching DH grp found");

	debug("SSH2_MSG_KEX_DH_GEX_GROUP sent");
	packet_start(SSH2_MSG_KEX_DH_GEX_GROUP);
	packet_put_bignum2(dh->p);
	packet_put_bignum2(dh->g);
	packet_send();

	/* flush */
	packet_write_wait();

	/* Compute our exchange value in parallel with the client */
	dh_gen_key(dh, kex->we_need * 8);

	debug("expecting SSH2_MSG_KEX_DH_GEX_INIT");
	packet_read_expect(SSH2_MSG_KEX_DH_GEX_INIT);

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
		fatal("kexgex_server: BN_new failed");
	BN_bin2bn(kbuf, kout, shared_secret);
	memset(kbuf, 0, klen);
	xfree(kbuf);

	key_to_blob(server_host_key, &server_host_key_blob, &sbloblen);

	if (type == SSH2_MSG_KEX_DH_GEX_REQUEST_OLD)
		min = max = -1;

	/* calc H */			/* XXX depends on 'kex' */
	hash = kexgex_hash(
	    kex->client_version_string,
	    kex->server_version_string,
	    buffer_ptr(&kex->peer), buffer_len(&kex->peer),
	    buffer_ptr(&kex->my), buffer_len(&kex->my),
	    server_host_key_blob, sbloblen,
	    min, nbits, max,
	    dh->p, dh->g,
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
	debug("SSH2_MSG_KEX_DH_GEX_REPLY sent");
	packet_start(SSH2_MSG_KEX_DH_GEX_REPLY);
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
kexgex(Kex *kex)
{
	if (kex->server)
		kexgex_server(kex);
	else
		kexgex_client(kex);
}
