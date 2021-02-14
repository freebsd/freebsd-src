/* $OpenBSD: kexgen.c,v 1.3 2019/09/06 05:23:55 djm Exp $ */
/*
 * Copyright (c) 2019 Markus Friedl.  All rights reserved.
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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "sshkey.h"
#include "kex.h"
#include "log.h"
#include "packet.h"
#include "ssh2.h"
#include "sshbuf.h"
#include "digest.h"
#include "ssherr.h"

static int input_kex_gen_init(int, u_int32_t, struct ssh *);
static int input_kex_gen_reply(int type, u_int32_t seq, struct ssh *ssh);

static int
kex_gen_hash(
    int hash_alg,
    const struct sshbuf *client_version,
    const struct sshbuf *server_version,
    const struct sshbuf *client_kexinit,
    const struct sshbuf *server_kexinit,
    const struct sshbuf *server_host_key_blob,
    const struct sshbuf *client_pub,
    const struct sshbuf *server_pub,
    const struct sshbuf *shared_secret,
    u_char *hash, size_t *hashlen)
{
	struct sshbuf *b;
	int r;

	if (*hashlen < ssh_digest_bytes(hash_alg))
		return SSH_ERR_INVALID_ARGUMENT;
	if ((b = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((r = sshbuf_put_stringb(b, client_version)) != 0 ||
	    (r = sshbuf_put_stringb(b, server_version)) != 0 ||
	    /* kexinit messages: fake header: len+SSH2_MSG_KEXINIT */
	    (r = sshbuf_put_u32(b, sshbuf_len(client_kexinit) + 1)) != 0 ||
	    (r = sshbuf_put_u8(b, SSH2_MSG_KEXINIT)) != 0 ||
	    (r = sshbuf_putb(b, client_kexinit)) != 0 ||
	    (r = sshbuf_put_u32(b, sshbuf_len(server_kexinit) + 1)) != 0 ||
	    (r = sshbuf_put_u8(b, SSH2_MSG_KEXINIT)) != 0 ||
	    (r = sshbuf_putb(b, server_kexinit)) != 0 ||
	    (r = sshbuf_put_stringb(b, server_host_key_blob)) != 0 ||
	    (r = sshbuf_put_stringb(b, client_pub)) != 0 ||
	    (r = sshbuf_put_stringb(b, server_pub)) != 0 ||
	    (r = sshbuf_putb(b, shared_secret)) != 0) {
		sshbuf_free(b);
		return r;
	}
#ifdef DEBUG_KEX
	sshbuf_dump(b, stderr);
#endif
	if (ssh_digest_buffer(hash_alg, b, hash, *hashlen) != 0) {
		sshbuf_free(b);
		return SSH_ERR_LIBCRYPTO_ERROR;
	}
	sshbuf_free(b);
	*hashlen = ssh_digest_bytes(hash_alg);
#ifdef DEBUG_KEX
	dump_digest("hash", hash, *hashlen);
#endif
	return 0;
}

int
kex_gen_client(struct ssh *ssh)
{
	struct kex *kex = ssh->kex;
	int r;

	switch (kex->kex_type) {
#ifdef WITH_OPENSSL
	case KEX_DH_GRP1_SHA1:
	case KEX_DH_GRP14_SHA1:
	case KEX_DH_GRP14_SHA256:
	case KEX_DH_GRP16_SHA512:
	case KEX_DH_GRP18_SHA512:
		r = kex_dh_keypair(kex);
		break;
	case KEX_ECDH_SHA2:
		r = kex_ecdh_keypair(kex);
		break;
#endif
	case KEX_C25519_SHA256:
		r = kex_c25519_keypair(kex);
		break;
	case KEX_KEM_SNTRUP4591761X25519_SHA512:
		r = kex_kem_sntrup4591761x25519_keypair(kex);
		break;
	default:
		r = SSH_ERR_INVALID_ARGUMENT;
		break;
	}
	if (r != 0)
		return r;
	if ((r = sshpkt_start(ssh, SSH2_MSG_KEX_ECDH_INIT)) != 0 ||
	    (r = sshpkt_put_stringb(ssh, kex->client_pub)) != 0 ||
	    (r = sshpkt_send(ssh)) != 0)
		return r;
	debug("expecting SSH2_MSG_KEX_ECDH_REPLY");
	ssh_dispatch_set(ssh, SSH2_MSG_KEX_ECDH_REPLY, &input_kex_gen_reply);
	return 0;
}

static int
input_kex_gen_reply(int type, u_int32_t seq, struct ssh *ssh)
{
	struct kex *kex = ssh->kex;
	struct sshkey *server_host_key = NULL;
	struct sshbuf *shared_secret = NULL;
	struct sshbuf *server_blob = NULL;
	struct sshbuf *tmp = NULL, *server_host_key_blob = NULL;
	u_char *signature = NULL;
	u_char hash[SSH_DIGEST_MAX_LENGTH];
	size_t slen, hashlen;
	int r;

	/* hostkey */
	if ((r = sshpkt_getb_froms(ssh, &server_host_key_blob)) != 0)
		goto out;
	/* sshkey_fromb() consumes its buffer, so make a copy */
	if ((tmp = sshbuf_fromb(server_host_key_blob)) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((r = sshkey_fromb(tmp, &server_host_key)) != 0)
		goto out;
	if ((r = kex_verify_host_key(ssh, server_host_key)) != 0)
		goto out;

	/* Q_S, server public key */
	/* signed H */
	if ((r = sshpkt_getb_froms(ssh, &server_blob)) != 0 ||
	    (r = sshpkt_get_string(ssh, &signature, &slen)) != 0 ||
	    (r = sshpkt_get_end(ssh)) != 0)
		goto out;

	/* compute shared secret */
	switch (kex->kex_type) {
#ifdef WITH_OPENSSL
	case KEX_DH_GRP1_SHA1:
	case KEX_DH_GRP14_SHA1:
	case KEX_DH_GRP14_SHA256:
	case KEX_DH_GRP16_SHA512:
	case KEX_DH_GRP18_SHA512:
		r = kex_dh_dec(kex, server_blob, &shared_secret);
		break;
	case KEX_ECDH_SHA2:
		r = kex_ecdh_dec(kex, server_blob, &shared_secret);
		break;
#endif
	case KEX_C25519_SHA256:
		r = kex_c25519_dec(kex, server_blob, &shared_secret);
		break;
	case KEX_KEM_SNTRUP4591761X25519_SHA512:
		r = kex_kem_sntrup4591761x25519_dec(kex, server_blob,
		    &shared_secret);
		break;
	default:
		r = SSH_ERR_INVALID_ARGUMENT;
		break;
	}
	if (r !=0 )
		goto out;

	/* calc and verify H */
	hashlen = sizeof(hash);
	if ((r = kex_gen_hash(
	    kex->hash_alg,
	    kex->client_version,
	    kex->server_version,
	    kex->my,
	    kex->peer,
	    server_host_key_blob,
	    kex->client_pub,
	    server_blob,
	    shared_secret,
	    hash, &hashlen)) != 0)
		goto out;

	if ((r = sshkey_verify(server_host_key, signature, slen, hash, hashlen,
	    kex->hostkey_alg, ssh->compat)) != 0)
		goto out;

	if ((r = kex_derive_keys(ssh, hash, hashlen, shared_secret)) == 0)
		r = kex_send_newkeys(ssh);
out:
	explicit_bzero(hash, sizeof(hash));
	explicit_bzero(kex->c25519_client_key, sizeof(kex->c25519_client_key));
	explicit_bzero(kex->sntrup4591761_client_key,
	    sizeof(kex->sntrup4591761_client_key));
	sshbuf_free(server_host_key_blob);
	free(signature);
	sshbuf_free(tmp);
	sshkey_free(server_host_key);
	sshbuf_free(server_blob);
	sshbuf_free(shared_secret);
	sshbuf_free(kex->client_pub);
	kex->client_pub = NULL;
	return r;
}

int
kex_gen_server(struct ssh *ssh)
{
	debug("expecting SSH2_MSG_KEX_ECDH_INIT");
	ssh_dispatch_set(ssh, SSH2_MSG_KEX_ECDH_INIT, &input_kex_gen_init);
	return 0;
}

static int
input_kex_gen_init(int type, u_int32_t seq, struct ssh *ssh)
{
	struct kex *kex = ssh->kex;
	struct sshkey *server_host_private, *server_host_public;
	struct sshbuf *shared_secret = NULL;
	struct sshbuf *server_pubkey = NULL;
	struct sshbuf *client_pubkey = NULL;
	struct sshbuf *server_host_key_blob = NULL;
	u_char *signature = NULL, hash[SSH_DIGEST_MAX_LENGTH];
	size_t slen, hashlen;
	int r;

	if ((r = kex_load_hostkey(ssh, &server_host_private,
	    &server_host_public)) != 0)
		goto out;

	if ((r = sshpkt_getb_froms(ssh, &client_pubkey)) != 0 ||
	    (r = sshpkt_get_end(ssh)) != 0)
		goto out;

	/* compute shared secret */
	switch (kex->kex_type) {
#ifdef WITH_OPENSSL
	case KEX_DH_GRP1_SHA1:
	case KEX_DH_GRP14_SHA1:
	case KEX_DH_GRP14_SHA256:
	case KEX_DH_GRP16_SHA512:
	case KEX_DH_GRP18_SHA512:
		r = kex_dh_enc(kex, client_pubkey, &server_pubkey,
		    &shared_secret);
		break;
	case KEX_ECDH_SHA2:
		r = kex_ecdh_enc(kex, client_pubkey, &server_pubkey,
		    &shared_secret);
		break;
#endif
	case KEX_C25519_SHA256:
		r = kex_c25519_enc(kex, client_pubkey, &server_pubkey,
		    &shared_secret);
		break;
	case KEX_KEM_SNTRUP4591761X25519_SHA512:
		r = kex_kem_sntrup4591761x25519_enc(kex, client_pubkey,
		    &server_pubkey, &shared_secret);
		break;
	default:
		r = SSH_ERR_INVALID_ARGUMENT;
		break;
	}
	if (r !=0 )
		goto out;

	/* calc H */
	if ((server_host_key_blob = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((r = sshkey_putb(server_host_public, server_host_key_blob)) != 0)
		goto out;
	hashlen = sizeof(hash);
	if ((r = kex_gen_hash(
	    kex->hash_alg,
	    kex->client_version,
	    kex->server_version,
	    kex->peer,
	    kex->my,
	    server_host_key_blob,
	    client_pubkey,
	    server_pubkey,
	    shared_secret,
	    hash, &hashlen)) != 0)
		goto out;

	/* sign H */
	if ((r = kex->sign(ssh, server_host_private, server_host_public,
	     &signature, &slen, hash, hashlen, kex->hostkey_alg)) != 0)
		goto out;

	/* send server hostkey, ECDH pubkey 'Q_S' and signed H */
	if ((r = sshpkt_start(ssh, SSH2_MSG_KEX_ECDH_REPLY)) != 0 ||
	    (r = sshpkt_put_stringb(ssh, server_host_key_blob)) != 0 ||
	    (r = sshpkt_put_stringb(ssh, server_pubkey)) != 0 ||
	    (r = sshpkt_put_string(ssh, signature, slen)) != 0 ||
	    (r = sshpkt_send(ssh)) != 0)
		goto out;

	if ((r = kex_derive_keys(ssh, hash, hashlen, shared_secret)) == 0)
		r = kex_send_newkeys(ssh);
out:
	explicit_bzero(hash, sizeof(hash));
	sshbuf_free(server_host_key_blob);
	free(signature);
	sshbuf_free(shared_secret);
	sshbuf_free(client_pubkey);
	sshbuf_free(server_pubkey);
	return r;
}
