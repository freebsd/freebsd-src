/*
 * Copyright (c) 2000, 2001 Markus Friedl.  All rights reserved.
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
RCSID("$OpenBSD: kex.c,v 1.51 2002/06/24 14:55:38 markus Exp $");

#include <openssl/crypto.h>

#include "ssh2.h"
#include "xmalloc.h"
#include "buffer.h"
#include "bufaux.h"
#include "packet.h"
#include "compat.h"
#include "cipher.h"
#include "kex.h"
#include "key.h"
#include "log.h"
#include "mac.h"
#include "match.h"
#include "dispatch.h"
#include "monitor.h"

#define KEX_COOKIE_LEN	16

/* Use privilege separation for sshd */
int use_privsep;
struct monitor *pmonitor;


/* prototype */
static void kex_kexinit_finish(Kex *);
static void kex_choose_conf(Kex *);

/* put algorithm proposal into buffer */
static void
kex_prop2buf(Buffer *b, char *proposal[PROPOSAL_MAX])
{
	int i;

	buffer_clear(b);
	/*
	 * add a dummy cookie, the cookie will be overwritten by
	 * kex_send_kexinit(), each time a kexinit is set
	 */
	for (i = 0; i < KEX_COOKIE_LEN; i++)
		buffer_put_char(b, 0);
	for (i = 0; i < PROPOSAL_MAX; i++)
		buffer_put_cstring(b, proposal[i]);
	buffer_put_char(b, 0);			/* first_kex_packet_follows */
	buffer_put_int(b, 0);			/* uint32 reserved */
}

/* parse buffer and return algorithm proposal */
static char **
kex_buf2prop(Buffer *raw)
{
	Buffer b;
	int i;
	char **proposal;

	proposal = xmalloc(PROPOSAL_MAX * sizeof(char *));

	buffer_init(&b);
	buffer_append(&b, buffer_ptr(raw), buffer_len(raw));
	/* skip cookie */
	for (i = 0; i < KEX_COOKIE_LEN; i++)
		buffer_get_char(&b);
	/* extract kex init proposal strings */
	for (i = 0; i < PROPOSAL_MAX; i++) {
		proposal[i] = buffer_get_string(&b,NULL);
		debug2("kex_parse_kexinit: %s", proposal[i]);
	}
	/* first kex follows / reserved */
	i = buffer_get_char(&b);
	debug2("kex_parse_kexinit: first_kex_follows %d ", i);
	i = buffer_get_int(&b);
	debug2("kex_parse_kexinit: reserved %d ", i);
	buffer_free(&b);
	return proposal;
}

static void
kex_prop_free(char **proposal)
{
	int i;

	for (i = 0; i < PROPOSAL_MAX; i++)
		xfree(proposal[i]);
	xfree(proposal);
}

static void
kex_protocol_error(int type, u_int32_t seq, void *ctxt)
{
	error("Hm, kex protocol error: type %d seq %u", type, seq);
}

static void
kex_reset_dispatch(void)
{
	dispatch_range(SSH2_MSG_TRANSPORT_MIN,
	    SSH2_MSG_TRANSPORT_MAX, &kex_protocol_error);
	dispatch_set(SSH2_MSG_KEXINIT, &kex_input_kexinit);
}

void
kex_finish(Kex *kex)
{
	kex_reset_dispatch();

	packet_start(SSH2_MSG_NEWKEYS);
	packet_send();
	/* packet_write_wait(); */
	debug("SSH2_MSG_NEWKEYS sent");

	debug("waiting for SSH2_MSG_NEWKEYS");
	packet_read_expect(SSH2_MSG_NEWKEYS);
	packet_check_eom();
	debug("SSH2_MSG_NEWKEYS received");

	kex->done = 1;
	buffer_clear(&kex->peer);
	/* buffer_clear(&kex->my); */
	kex->flags &= ~KEX_INIT_SENT;
	xfree(kex->name);
	kex->name = NULL;
}

void
kex_send_kexinit(Kex *kex)
{
	u_int32_t rand = 0;
	u_char *cookie;
	int i;

	if (kex == NULL) {
		error("kex_send_kexinit: no kex, cannot rekey");
		return;
	}
	if (kex->flags & KEX_INIT_SENT) {
		debug("KEX_INIT_SENT");
		return;
	}
	kex->done = 0;

	/* generate a random cookie */
	if (buffer_len(&kex->my) < KEX_COOKIE_LEN)
		fatal("kex_send_kexinit: kex proposal too short");
	cookie = buffer_ptr(&kex->my);
	for (i = 0; i < KEX_COOKIE_LEN; i++) {
		if (i % 4 == 0)
			rand = arc4random();
		cookie[i] = rand;
		rand >>= 8;
	}
	packet_start(SSH2_MSG_KEXINIT);
	packet_put_raw(buffer_ptr(&kex->my), buffer_len(&kex->my));
	packet_send();
	debug("SSH2_MSG_KEXINIT sent");
	kex->flags |= KEX_INIT_SENT;
}

void
kex_input_kexinit(int type, u_int32_t seq, void *ctxt)
{
	char *ptr;
	int dlen;
	int i;
	Kex *kex = (Kex *)ctxt;

	debug("SSH2_MSG_KEXINIT received");
	if (kex == NULL)
		fatal("kex_input_kexinit: no kex, cannot rekey");

	ptr = packet_get_raw(&dlen);
	buffer_append(&kex->peer, ptr, dlen);

	/* discard packet */
	for (i = 0; i < KEX_COOKIE_LEN; i++)
		packet_get_char();
	for (i = 0; i < PROPOSAL_MAX; i++)
		xfree(packet_get_string(NULL));
	(void) packet_get_char();
	(void) packet_get_int();
	packet_check_eom();

	kex_kexinit_finish(kex);
}

Kex *
kex_setup(char *proposal[PROPOSAL_MAX])
{
	Kex *kex;

	kex = xmalloc(sizeof(*kex));
	memset(kex, 0, sizeof(*kex));
	buffer_init(&kex->peer);
	buffer_init(&kex->my);
	kex_prop2buf(&kex->my, proposal);
	kex->done = 0;

	kex_send_kexinit(kex);					/* we start */
	kex_reset_dispatch();

	return kex;
}

static void
kex_kexinit_finish(Kex *kex)
{
	if (!(kex->flags & KEX_INIT_SENT))
		kex_send_kexinit(kex);

	kex_choose_conf(kex);

	switch (kex->kex_type) {
	case DH_GRP1_SHA1:
		kexdh(kex);
		break;
	case DH_GEX_SHA1:
		kexgex(kex);
		break;
	default:
		fatal("Unsupported key exchange %d", kex->kex_type);
	}
}

static void
choose_enc(Enc *enc, char *client, char *server)
{
	char *name = match_list(client, server, NULL);
	if (name == NULL)
		fatal("no matching cipher found: client %s server %s", client, server);
	if ((enc->cipher = cipher_by_name(name)) == NULL)
		fatal("matching cipher is not supported: %s", name);
	enc->name = name;
	enc->enabled = 0;
	enc->iv = NULL;
	enc->key = NULL;
	enc->key_len = cipher_keylen(enc->cipher);
	enc->block_size = cipher_blocksize(enc->cipher);
}
static void
choose_mac(Mac *mac, char *client, char *server)
{
	char *name = match_list(client, server, NULL);
	if (name == NULL)
		fatal("no matching mac found: client %s server %s", client, server);
	if (mac_init(mac, name) < 0)
		fatal("unsupported mac %s", name);
	/* truncate the key */
	if (datafellows & SSH_BUG_HMAC)
		mac->key_len = 16;
	mac->name = name;
	mac->key = NULL;
	mac->enabled = 0;
}
static void
choose_comp(Comp *comp, char *client, char *server)
{
	char *name = match_list(client, server, NULL);
	if (name == NULL)
		fatal("no matching comp found: client %s server %s", client, server);
	if (strcmp(name, "zlib") == 0) {
		comp->type = 1;
	} else if (strcmp(name, "none") == 0) {
		comp->type = 0;
	} else {
		fatal("unsupported comp %s", name);
	}
	comp->name = name;
}
static void
choose_kex(Kex *k, char *client, char *server)
{
	k->name = match_list(client, server, NULL);
	if (k->name == NULL)
		fatal("no kex alg");
	if (strcmp(k->name, KEX_DH1) == 0) {
		k->kex_type = DH_GRP1_SHA1;
	} else if (strcmp(k->name, KEX_DHGEX) == 0) {
		k->kex_type = DH_GEX_SHA1;
	} else
		fatal("bad kex alg %s", k->name);
}
static void
choose_hostkeyalg(Kex *k, char *client, char *server)
{
	char *hostkeyalg = match_list(client, server, NULL);
	if (hostkeyalg == NULL)
		fatal("no hostkey alg");
	k->hostkey_type = key_type_from_name(hostkeyalg);
	if (k->hostkey_type == KEY_UNSPEC)
		fatal("bad hostkey alg '%s'", hostkeyalg);
	xfree(hostkeyalg);
}

static void
kex_choose_conf(Kex *kex)
{
	Newkeys *newkeys;
	char **my, **peer;
	char **cprop, **sprop;
	int nenc, nmac, ncomp;
	int mode;
	int ctos;				/* direction: if true client-to-server */
	int need;

	my   = kex_buf2prop(&kex->my);
	peer = kex_buf2prop(&kex->peer);

	if (kex->server) {
		cprop=peer;
		sprop=my;
	} else {
		cprop=my;
		sprop=peer;
	}

	/* Algorithm Negotiation */
	for (mode = 0; mode < MODE_MAX; mode++) {
		newkeys = xmalloc(sizeof(*newkeys));
		memset(newkeys, 0, sizeof(*newkeys));
		kex->newkeys[mode] = newkeys;
		ctos = (!kex->server && mode == MODE_OUT) || (kex->server && mode == MODE_IN);
		nenc  = ctos ? PROPOSAL_ENC_ALGS_CTOS  : PROPOSAL_ENC_ALGS_STOC;
		nmac  = ctos ? PROPOSAL_MAC_ALGS_CTOS  : PROPOSAL_MAC_ALGS_STOC;
		ncomp = ctos ? PROPOSAL_COMP_ALGS_CTOS : PROPOSAL_COMP_ALGS_STOC;
		choose_enc (&newkeys->enc,  cprop[nenc],  sprop[nenc]);
		choose_mac (&newkeys->mac,  cprop[nmac],  sprop[nmac]);
		choose_comp(&newkeys->comp, cprop[ncomp], sprop[ncomp]);
		debug("kex: %s %s %s %s",
		    ctos ? "client->server" : "server->client",
		    newkeys->enc.name,
		    newkeys->mac.name,
		    newkeys->comp.name);
	}
	choose_kex(kex, cprop[PROPOSAL_KEX_ALGS], sprop[PROPOSAL_KEX_ALGS]);
	choose_hostkeyalg(kex, cprop[PROPOSAL_SERVER_HOST_KEY_ALGS],
	    sprop[PROPOSAL_SERVER_HOST_KEY_ALGS]);
	need = 0;
	for (mode = 0; mode < MODE_MAX; mode++) {
		newkeys = kex->newkeys[mode];
		if (need < newkeys->enc.key_len)
			need = newkeys->enc.key_len;
		if (need < newkeys->enc.block_size)
			need = newkeys->enc.block_size;
		if (need < newkeys->mac.key_len)
			need = newkeys->mac.key_len;
	}
	/* XXX need runden? */
	kex->we_need = need;

	kex_prop_free(my);
	kex_prop_free(peer);
}

static u_char *
derive_key(Kex *kex, int id, int need, u_char *hash, BIGNUM *shared_secret)
{
	Buffer b;
	const EVP_MD *evp_md = EVP_sha1();
	EVP_MD_CTX md;
	char c = id;
	int have;
	int mdsz = EVP_MD_size(evp_md);
	u_char *digest = xmalloc(roundup(need, mdsz));

	buffer_init(&b);
	buffer_put_bignum2(&b, shared_secret);

	/* K1 = HASH(K || H || "A" || session_id) */
	EVP_DigestInit(&md, evp_md);
	if (!(datafellows & SSH_BUG_DERIVEKEY))
		EVP_DigestUpdate(&md, buffer_ptr(&b), buffer_len(&b));
	EVP_DigestUpdate(&md, hash, mdsz);
	EVP_DigestUpdate(&md, &c, 1);
	EVP_DigestUpdate(&md, kex->session_id, kex->session_id_len);
	EVP_DigestFinal(&md, digest, NULL);

	/*
	 * expand key:
	 * Kn = HASH(K || H || K1 || K2 || ... || Kn-1)
	 * Key = K1 || K2 || ... || Kn
	 */
	for (have = mdsz; need > have; have += mdsz) {
		EVP_DigestInit(&md, evp_md);
		if (!(datafellows & SSH_BUG_DERIVEKEY))
			EVP_DigestUpdate(&md, buffer_ptr(&b), buffer_len(&b));
		EVP_DigestUpdate(&md, hash, mdsz);
		EVP_DigestUpdate(&md, digest, have);
		EVP_DigestFinal(&md, digest + have, NULL);
	}
	buffer_free(&b);
#ifdef DEBUG_KEX
	fprintf(stderr, "key '%c'== ", c);
	dump_digest("key", digest, need);
#endif
	return digest;
}

Newkeys *current_keys[MODE_MAX];

#define NKEYS	6
void
kex_derive_keys(Kex *kex, u_char *hash, BIGNUM *shared_secret)
{
	u_char *keys[NKEYS];
	int i, mode, ctos;

	for (i = 0; i < NKEYS; i++)
		keys[i] = derive_key(kex, 'A'+i, kex->we_need, hash, shared_secret);

	debug("kex_derive_keys");
	for (mode = 0; mode < MODE_MAX; mode++) {
		current_keys[mode] = kex->newkeys[mode];
		kex->newkeys[mode] = NULL;
		ctos = (!kex->server && mode == MODE_OUT) || (kex->server && mode == MODE_IN);
		current_keys[mode]->enc.iv  = keys[ctos ? 0 : 1];
		current_keys[mode]->enc.key = keys[ctos ? 2 : 3];
		current_keys[mode]->mac.key = keys[ctos ? 4 : 5];
	}
}

Newkeys *
kex_get_newkeys(int mode)
{
	Newkeys *ret;

	ret = current_keys[mode];
	current_keys[mode] = NULL;
	return ret;
}

#if defined(DEBUG_KEX) || defined(DEBUG_KEXDH)
void
dump_digest(char *msg, u_char *digest, int len)
{
	int i;

	fprintf(stderr, "%s\n", msg);
	for (i = 0; i< len; i++) {
		fprintf(stderr, "%02x", digest[i]);
		if (i%32 == 31)
			fprintf(stderr, "\n");
		else if (i%8 == 7)
			fprintf(stderr, " ");
	}
	fprintf(stderr, "\n");
}
#endif
