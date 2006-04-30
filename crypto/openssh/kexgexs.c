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
RCSID("$OpenBSD: kexgexs.c,v 1.1 2003/02/16 17:09:57 markus Exp $");

#include "xmalloc.h"
#include "key.h"
#include "kex.h"
#include "log.h"
#include "packet.h"
#include "dh.h"
#include "ssh2.h"
#include "compat.h"
#include "monitor_wrap.h"

void
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
