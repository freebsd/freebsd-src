/* $OpenBSD: kexgexc.c,v 1.38 2021/12/19 22:08:06 djm Exp $ */
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

#ifdef WITH_OPENSSL

#include <sys/types.h>

#include <openssl/dh.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "openbsd-compat/openssl-compat.h"

#include "sshkey.h"
#include "cipher.h"
#include "digest.h"
#include "kex.h"
#include "log.h"
#include "packet.h"
#include "dh.h"
#include "ssh2.h"
#include "compat.h"
#include "dispatch.h"
#include "ssherr.h"
#include "sshbuf.h"
#include "misc.h"

static int input_kex_dh_gex_group(int, u_int32_t, struct ssh *);
static int input_kex_dh_gex_reply(int, u_int32_t, struct ssh *);

int
kexgex_client(struct ssh *ssh)
{
	struct kex *kex = ssh->kex;
	int r;
	u_int nbits;

	nbits = dh_estimate(kex->dh_need * 8);

	kex->min = DH_GRP_MIN;
	kex->max = DH_GRP_MAX;
	kex->nbits = nbits;
	if (ssh->compat & SSH_BUG_DHGEX_LARGE)
		kex->nbits = MINIMUM(kex->nbits, 4096);
	/* New GEX request */
	if ((r = sshpkt_start(ssh, SSH2_MSG_KEX_DH_GEX_REQUEST)) != 0 ||
	    (r = sshpkt_put_u32(ssh, kex->min)) != 0 ||
	    (r = sshpkt_put_u32(ssh, kex->nbits)) != 0 ||
	    (r = sshpkt_put_u32(ssh, kex->max)) != 0 ||
	    (r = sshpkt_send(ssh)) != 0)
		goto out;
	debug("SSH2_MSG_KEX_DH_GEX_REQUEST(%u<%u<%u) sent",
	    kex->min, kex->nbits, kex->max);
#ifdef DEBUG_KEXDH
	fprintf(stderr, "\nmin = %d, nbits = %d, max = %d\n",
	    kex->min, kex->nbits, kex->max);
#endif
	debug("expecting SSH2_MSG_KEX_DH_GEX_GROUP");
	ssh_dispatch_set(ssh, SSH2_MSG_KEX_DH_GEX_GROUP,
	    &input_kex_dh_gex_group);
	r = 0;
 out:
	return r;
}

static int
input_kex_dh_gex_group(int type, u_int32_t seq, struct ssh *ssh)
{
	struct kex *kex = ssh->kex;
	BIGNUM *p = NULL, *g = NULL;
	const BIGNUM *pub_key;
	int r, bits;

	debug("SSH2_MSG_KEX_DH_GEX_GROUP received");
	ssh_dispatch_set(ssh, SSH2_MSG_KEX_DH_GEX_GROUP, &kex_protocol_error);

	if ((r = sshpkt_get_bignum2(ssh, &p)) != 0 ||
	    (r = sshpkt_get_bignum2(ssh, &g)) != 0 ||
	    (r = sshpkt_get_end(ssh)) != 0)
		goto out;
	if ((bits = BN_num_bits(p)) < 0 ||
	    (u_int)bits < kex->min || (u_int)bits > kex->max) {
		r = SSH_ERR_DH_GEX_OUT_OF_RANGE;
		goto out;
	}
	if ((kex->dh = dh_new_group(g, p)) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	p = g = NULL; /* belong to kex->dh now */

	/* generate and send 'e', client DH public key */
	if ((r = dh_gen_key(kex->dh, kex->we_need * 8)) != 0)
		goto out;
	DH_get0_key(kex->dh, &pub_key, NULL);
	if ((r = sshpkt_start(ssh, SSH2_MSG_KEX_DH_GEX_INIT)) != 0 ||
	    (r = sshpkt_put_bignum2(ssh, pub_key)) != 0 ||
	    (r = sshpkt_send(ssh)) != 0)
		goto out;
	debug("SSH2_MSG_KEX_DH_GEX_INIT sent");
#ifdef DEBUG_KEXDH
	DHparams_print_fp(stderr, kex->dh);
	fprintf(stderr, "pub= ");
	BN_print_fp(stderr, pub_key);
	fprintf(stderr, "\n");
#endif
	debug("expecting SSH2_MSG_KEX_DH_GEX_REPLY");
	ssh_dispatch_set(ssh, SSH2_MSG_KEX_DH_GEX_REPLY, &input_kex_dh_gex_reply);
	r = 0;
out:
	BN_clear_free(p);
	BN_clear_free(g);
	return r;
}

static int
input_kex_dh_gex_reply(int type, u_int32_t seq, struct ssh *ssh)
{
	struct kex *kex = ssh->kex;
	BIGNUM *dh_server_pub = NULL;
	const BIGNUM *pub_key, *dh_p, *dh_g;
	struct sshbuf *shared_secret = NULL;
	struct sshbuf *tmp = NULL, *server_host_key_blob = NULL;
	struct sshkey *server_host_key = NULL;
	u_char *signature = NULL;
	u_char hash[SSH_DIGEST_MAX_LENGTH];
	size_t slen, hashlen;
	int r;

	debug("SSH2_MSG_KEX_DH_GEX_REPLY received");
	ssh_dispatch_set(ssh, SSH2_MSG_KEX_DH_GEX_REPLY, &kex_protocol_error);

	/* key, cert */
	if ((r = sshpkt_getb_froms(ssh, &server_host_key_blob)) != 0)
		goto out;
	/* sshkey_fromb() consumes its buffer, so make a copy */
	if ((tmp = sshbuf_fromb(server_host_key_blob)) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((r = sshkey_fromb(tmp, &server_host_key)) != 0 ||
	    (r = kex_verify_host_key(ssh, server_host_key)) != 0)
		goto out;
	/* DH parameter f, server public DH key, signed H */
	if ((r = sshpkt_get_bignum2(ssh, &dh_server_pub)) != 0 ||
	    (r = sshpkt_get_string(ssh, &signature, &slen)) != 0 ||
	    (r = sshpkt_get_end(ssh)) != 0)
		goto out;
	if ((shared_secret = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((r = kex_dh_compute_key(kex, dh_server_pub, shared_secret)) != 0)
		goto out;
	if (ssh->compat & SSH_OLD_DHGEX)
		kex->min = kex->max = -1;

	/* calc and verify H */
	DH_get0_key(kex->dh, &pub_key, NULL);
	DH_get0_pqg(kex->dh, &dh_p, NULL, &dh_g);
	hashlen = sizeof(hash);
	if ((r = kexgex_hash(
	    kex->hash_alg,
	    kex->client_version,
	    kex->server_version,
	    kex->my,
	    kex->peer,
	    server_host_key_blob,
	    kex->min, kex->nbits, kex->max,
	    dh_p, dh_g,
	    pub_key,
	    dh_server_pub,
	    sshbuf_ptr(shared_secret), sshbuf_len(shared_secret),
	    hash, &hashlen)) != 0)
		goto out;

	if ((r = sshkey_verify(server_host_key, signature, slen, hash,
	    hashlen, kex->hostkey_alg, ssh->compat, NULL)) != 0)
		goto out;

	if ((r = kex_derive_keys(ssh, hash, hashlen, shared_secret)) != 0 ||
	    (r = kex_send_newkeys(ssh)) != 0)
		goto out;

	/* save initial signature and hostkey */
	if ((kex->flags & KEX_INITIAL) != 0) {
		if (kex->initial_hostkey != NULL || kex->initial_sig != NULL) {
			r = SSH_ERR_INTERNAL_ERROR;
			goto out;
		}
		if ((kex->initial_sig = sshbuf_new()) == NULL) {
			r = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		if ((r = sshbuf_put(kex->initial_sig, signature, slen)) != 0)
			goto out;
		kex->initial_hostkey = server_host_key;
		server_host_key = NULL;
	}
	/* success */
 out:
	explicit_bzero(hash, sizeof(hash));
	DH_free(kex->dh);
	kex->dh = NULL;
	BN_clear_free(dh_server_pub);
	sshbuf_free(shared_secret);
	sshkey_free(server_host_key);
	sshbuf_free(tmp);
	sshbuf_free(server_host_key_blob);
	free(signature);
	return r;
}
#endif /* WITH_OPENSSL */
