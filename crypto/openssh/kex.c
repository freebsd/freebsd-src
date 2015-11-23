/* $OpenBSD: kex.c,v 1.98 2014/02/02 03:44:31 djm Exp $ */
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

#include <sys/param.h>

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/crypto.h>

#include "xmalloc.h"
#include "ssh2.h"
#include "buffer.h"
#include "packet.h"
#include "compat.h"
#include "cipher.h"
#include "key.h"
#include "kex.h"
#include "log.h"
#include "mac.h"
#include "match.h"
#include "dispatch.h"
#include "monitor.h"
#include "roaming.h"
#include "digest.h"

#if OPENSSL_VERSION_NUMBER >= 0x00907000L
# if defined(HAVE_EVP_SHA256)
# define evp_ssh_sha256 EVP_sha256
# else
extern const EVP_MD *evp_ssh_sha256(void);
# endif
#endif

/* prototype */
static void kex_kexinit_finish(Kex *);
static void kex_choose_conf(Kex *);

struct kexalg {
	char *name;
	int type;
	int ec_nid;
	int hash_alg;
};
static const struct kexalg kexalgs[] = {
	{ KEX_DH1, KEX_DH_GRP1_SHA1, 0, SSH_DIGEST_SHA1 },
	{ KEX_DH14, KEX_DH_GRP14_SHA1, 0, SSH_DIGEST_SHA1 },
	{ KEX_DHGEX_SHA1, KEX_DH_GEX_SHA1, 0, SSH_DIGEST_SHA1 },
#ifdef HAVE_EVP_SHA256
	{ KEX_DHGEX_SHA256, KEX_DH_GEX_SHA256, 0, SSH_DIGEST_SHA256 },
#endif
#ifdef OPENSSL_HAS_ECC
	{ KEX_ECDH_SHA2_NISTP256, KEX_ECDH_SHA2,
	    NID_X9_62_prime256v1, SSH_DIGEST_SHA256 },
	{ KEX_ECDH_SHA2_NISTP384, KEX_ECDH_SHA2, NID_secp384r1,
	    SSH_DIGEST_SHA384 },
# ifdef OPENSSL_HAS_NISTP521
	{ KEX_ECDH_SHA2_NISTP521, KEX_ECDH_SHA2, NID_secp521r1,
	    SSH_DIGEST_SHA512 },
# endif
#endif
	{ KEX_DH1, KEX_DH_GRP1_SHA1, 0, SSH_DIGEST_SHA1 },
#ifdef HAVE_EVP_SHA256
	{ KEX_CURVE25519_SHA256, KEX_C25519_SHA256, 0, SSH_DIGEST_SHA256 },
#endif
	{ NULL, -1, -1, -1},
};

char *
kex_alg_list(char sep)
{
	char *ret = NULL;
	size_t nlen, rlen = 0;
	const struct kexalg *k;

	for (k = kexalgs; k->name != NULL; k++) {
		if (ret != NULL)
			ret[rlen++] = sep;
		nlen = strlen(k->name);
		ret = xrealloc(ret, 1, rlen + nlen + 2);
		memcpy(ret + rlen, k->name, nlen + 1);
		rlen += nlen;
	}
	return ret;
}

static const struct kexalg *
kex_alg_by_name(const char *name)
{
	const struct kexalg *k;

	for (k = kexalgs; k->name != NULL; k++) {
		if (strcmp(k->name, name) == 0)
			return k;
	}
	return NULL;
}

/* Validate KEX method name list */
int
kex_names_valid(const char *names)
{
	char *s, *cp, *p;

	if (names == NULL || strcmp(names, "") == 0)
		return 0;
	s = cp = xstrdup(names);
	for ((p = strsep(&cp, ",")); p && *p != '\0';
	    (p = strsep(&cp, ","))) {
		if (kex_alg_by_name(p) == NULL) {
			error("Unsupported KEX algorithm \"%.100s\"", p);
			free(s);
			return 0;
		}
	}
	debug3("kex names ok: [%s]", names);
	free(s);
	return 1;
}

/* put algorithm proposal into buffer */
static void
kex_prop2buf(Buffer *b, char *proposal[PROPOSAL_MAX])
{
	u_int i;

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
kex_buf2prop(Buffer *raw, int *first_kex_follows)
{
	Buffer b;
	u_int i;
	char **proposal;

	proposal = xcalloc(PROPOSAL_MAX, sizeof(char *));

	buffer_init(&b);
	buffer_append(&b, buffer_ptr(raw), buffer_len(raw));
	/* skip cookie */
	for (i = 0; i < KEX_COOKIE_LEN; i++)
		buffer_get_char(&b);
	/* extract kex init proposal strings */
	for (i = 0; i < PROPOSAL_MAX; i++) {
		proposal[i] = buffer_get_cstring(&b,NULL);
		debug2("kex_parse_kexinit: %s", proposal[i]);
	}
	/* first kex follows / reserved */
	i = buffer_get_char(&b);
	if (first_kex_follows != NULL)
		*first_kex_follows = i;
	debug2("kex_parse_kexinit: first_kex_follows %d ", i);
	i = buffer_get_int(&b);
	debug2("kex_parse_kexinit: reserved %u ", i);
	buffer_free(&b);
	return proposal;
}

static void
kex_prop_free(char **proposal)
{
	u_int i;

	for (i = 0; i < PROPOSAL_MAX; i++)
		free(proposal[i]);
	free(proposal);
}

/* ARGSUSED */
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

	debug("expecting SSH2_MSG_NEWKEYS");
	packet_read_expect(SSH2_MSG_NEWKEYS);
	packet_check_eom();
	debug("SSH2_MSG_NEWKEYS received");

	kex->done = 1;
	buffer_clear(&kex->peer);
	/* buffer_clear(&kex->my); */
	kex->flags &= ~KEX_INIT_SENT;
	free(kex->name);
	kex->name = NULL;
}

void
kex_send_kexinit(Kex *kex)
{
	u_int32_t rnd = 0;
	u_char *cookie;
	u_int i;

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
			rnd = arc4random();
		cookie[i] = rnd;
		rnd >>= 8;
	}
	packet_start(SSH2_MSG_KEXINIT);
	packet_put_raw(buffer_ptr(&kex->my), buffer_len(&kex->my));
	packet_send();
	debug("SSH2_MSG_KEXINIT sent");
	kex->flags |= KEX_INIT_SENT;
}

/* ARGSUSED */
void
kex_input_kexinit(int type, u_int32_t seq, void *ctxt)
{
	char *ptr;
	u_int i, dlen;
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
		free(packet_get_string(NULL));
	/*
	 * XXX RFC4253 sec 7: "each side MAY guess" - currently no supported
	 * KEX method has the server move first, but a server might be using
	 * a custom method or one that we otherwise don't support. We should
	 * be prepared to remember first_kex_follows here so we can eat a
	 * packet later.
	 * XXX2 - RFC4253 is kind of ambiguous on what first_kex_follows means
	 * for cases where the server *doesn't* go first. I guess we should
	 * ignore it when it is set for these cases, which is what we do now.
	 */
	(void) packet_get_char();	/* first_kex_follows */
	(void) packet_get_int();	/* reserved */
	packet_check_eom();

	kex_kexinit_finish(kex);
}

Kex *
kex_setup(char *proposal[PROPOSAL_MAX])
{
	Kex *kex;

	kex = xcalloc(1, sizeof(*kex));
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

	if (kex->kex_type >= 0 && kex->kex_type < KEX_MAX &&
	    kex->kex[kex->kex_type] != NULL) {
		(kex->kex[kex->kex_type])(kex);
	} else {
		fatal("Unsupported key exchange %d", kex->kex_type);
	}
}

static void
choose_enc(Enc *enc, char *client, char *server)
{
	char *name = match_list(client, server, NULL);
	if (name == NULL)
		fatal("no matching cipher found: client %s server %s",
		    client, server);
	if ((enc->cipher = cipher_by_name(name)) == NULL)
		fatal("matching cipher is not supported: %s", name);
	enc->name = name;
	enc->enabled = 0;
	enc->iv = NULL;
	enc->iv_len = cipher_ivlen(enc->cipher);
	enc->key = NULL;
	enc->key_len = cipher_keylen(enc->cipher);
	enc->block_size = cipher_blocksize(enc->cipher);
}

static void
choose_mac(Mac *mac, char *client, char *server)
{
	char *name = match_list(client, server, NULL);
	if (name == NULL)
		fatal("no matching mac found: client %s server %s",
		    client, server);
	if (mac_setup(mac, name) < 0)
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
	if (strcmp(name, "zlib@openssh.com") == 0) {
		comp->type = COMP_DELAYED;
	} else if (strcmp(name, "zlib") == 0) {
		comp->type = COMP_ZLIB;
	} else if (strcmp(name, "none") == 0) {
		comp->type = COMP_NONE;
	} else {
		fatal("unsupported comp %s", name);
	}
	comp->name = name;
}

static void
choose_kex(Kex *k, char *client, char *server)
{
	const struct kexalg *kexalg;

	k->name = match_list(client, server, NULL);
	if (k->name == NULL)
		fatal("Unable to negotiate a key exchange method");
	if ((kexalg = kex_alg_by_name(k->name)) == NULL)
		fatal("unsupported kex alg %s", k->name);
	k->kex_type = kexalg->type;
	k->hash_alg = kexalg->hash_alg;
	k->ec_nid = kexalg->ec_nid;
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
	free(hostkeyalg);
}

static int
proposals_match(char *my[PROPOSAL_MAX], char *peer[PROPOSAL_MAX])
{
	static int check[] = {
		PROPOSAL_KEX_ALGS, PROPOSAL_SERVER_HOST_KEY_ALGS, -1
	};
	int *idx;
	char *p;

	for (idx = &check[0]; *idx != -1; idx++) {
		if ((p = strchr(my[*idx], ',')) != NULL)
			*p = '\0';
		if ((p = strchr(peer[*idx], ',')) != NULL)
			*p = '\0';
		if (strcmp(my[*idx], peer[*idx]) != 0) {
			debug2("proposal mismatch: my %s peer %s",
			    my[*idx], peer[*idx]);
			return (0);
		}
	}
	debug2("proposals match");
	return (1);
}

static void
kex_choose_conf(Kex *kex)
{
	Newkeys *newkeys;
	char **my, **peer;
	char **cprop, **sprop;
	int nenc, nmac, ncomp;
	u_int mode, ctos, need, dh_need, authlen;
	int first_kex_follows, type;

	my   = kex_buf2prop(&kex->my, NULL);
	peer = kex_buf2prop(&kex->peer, &first_kex_follows);

	if (kex->server) {
		cprop=peer;
		sprop=my;
	} else {
		cprop=my;
		sprop=peer;
	}

	/* Check whether server offers roaming */
	if (!kex->server) {
		char *roaming;
		roaming = match_list(KEX_RESUME, peer[PROPOSAL_KEX_ALGS], NULL);
		if (roaming) {
			kex->roaming = 1;
			free(roaming);
		}
	}

	/* Algorithm Negotiation */
	for (mode = 0; mode < MODE_MAX; mode++) {
		newkeys = xcalloc(1, sizeof(*newkeys));
		kex->newkeys[mode] = newkeys;
		ctos = (!kex->server && mode == MODE_OUT) ||
		    (kex->server && mode == MODE_IN);
		nenc  = ctos ? PROPOSAL_ENC_ALGS_CTOS  : PROPOSAL_ENC_ALGS_STOC;
		nmac  = ctos ? PROPOSAL_MAC_ALGS_CTOS  : PROPOSAL_MAC_ALGS_STOC;
		ncomp = ctos ? PROPOSAL_COMP_ALGS_CTOS : PROPOSAL_COMP_ALGS_STOC;
		choose_enc(&newkeys->enc, cprop[nenc], sprop[nenc]);
		/* ignore mac for authenticated encryption */
		authlen = cipher_authlen(newkeys->enc.cipher);
		if (authlen == 0)
			choose_mac(&newkeys->mac, cprop[nmac], sprop[nmac]);
		choose_comp(&newkeys->comp, cprop[ncomp], sprop[ncomp]);
		debug("kex: %s %s %s %s",
		    ctos ? "client->server" : "server->client",
		    newkeys->enc.name,
		    authlen == 0 ? newkeys->mac.name : "<implicit>",
		    newkeys->comp.name);
	}
	choose_kex(kex, cprop[PROPOSAL_KEX_ALGS], sprop[PROPOSAL_KEX_ALGS]);
	choose_hostkeyalg(kex, cprop[PROPOSAL_SERVER_HOST_KEY_ALGS],
	    sprop[PROPOSAL_SERVER_HOST_KEY_ALGS]);
	need = dh_need = 0;
	for (mode = 0; mode < MODE_MAX; mode++) {
		newkeys = kex->newkeys[mode];
		need = MAX(need, newkeys->enc.key_len);
		need = MAX(need, newkeys->enc.block_size);
		need = MAX(need, newkeys->enc.iv_len);
		need = MAX(need, newkeys->mac.key_len);
		dh_need = MAX(dh_need, cipher_seclen(newkeys->enc.cipher));
		dh_need = MAX(dh_need, newkeys->enc.block_size);
		dh_need = MAX(dh_need, newkeys->enc.iv_len);
		dh_need = MAX(dh_need, newkeys->mac.key_len);
	}
	/* XXX need runden? */
	kex->we_need = need;
	kex->dh_need = dh_need;

	/* ignore the next message if the proposals do not match */
	if (first_kex_follows && !proposals_match(my, peer) &&
	    !(datafellows & SSH_BUG_FIRSTKEX)) {
		type = packet_read();
		debug2("skipping next packet (type %u)", type);
	}

	kex_prop_free(my);
	kex_prop_free(peer);
}

static u_char *
derive_key(Kex *kex, int id, u_int need, u_char *hash, u_int hashlen,
    const u_char *shared_secret, u_int slen)
{
	Buffer b;
	struct ssh_digest_ctx *hashctx;
	char c = id;
	u_int have;
	size_t mdsz;
	u_char *digest;

	if ((mdsz = ssh_digest_bytes(kex->hash_alg)) == 0)
		fatal("bad kex md size %zu", mdsz);
	digest = xmalloc(roundup(need, mdsz));

	buffer_init(&b);
	buffer_append(&b, shared_secret, slen);

	/* K1 = HASH(K || H || "A" || session_id) */
	if ((hashctx = ssh_digest_start(kex->hash_alg)) == NULL)
		fatal("%s: ssh_digest_start failed", __func__);
	if (ssh_digest_update_buffer(hashctx, &b) != 0 ||
	    ssh_digest_update(hashctx, hash, hashlen) != 0 ||
	    ssh_digest_update(hashctx, &c, 1) != 0 ||
	    ssh_digest_update(hashctx, kex->session_id,
	    kex->session_id_len) != 0)
		fatal("%s: ssh_digest_update failed", __func__);
	if (ssh_digest_final(hashctx, digest, mdsz) != 0)
		fatal("%s: ssh_digest_final failed", __func__);
	ssh_digest_free(hashctx);

	/*
	 * expand key:
	 * Kn = HASH(K || H || K1 || K2 || ... || Kn-1)
	 * Key = K1 || K2 || ... || Kn
	 */
	for (have = mdsz; need > have; have += mdsz) {
		if ((hashctx = ssh_digest_start(kex->hash_alg)) == NULL)
			fatal("%s: ssh_digest_start failed", __func__);
		if (ssh_digest_update_buffer(hashctx, &b) != 0 ||
		    ssh_digest_update(hashctx, hash, hashlen) != 0 ||
		    ssh_digest_update(hashctx, digest, have) != 0)
			fatal("%s: ssh_digest_update failed", __func__);
		if (ssh_digest_final(hashctx, digest + have, mdsz) != 0)
			fatal("%s: ssh_digest_final failed", __func__);
		ssh_digest_free(hashctx);
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
kex_derive_keys(Kex *kex, u_char *hash, u_int hashlen,
    const u_char *shared_secret, u_int slen)
{
	u_char *keys[NKEYS];
	u_int i, mode, ctos;

	for (i = 0; i < NKEYS; i++) {
		keys[i] = derive_key(kex, 'A'+i, kex->we_need, hash, hashlen,
		    shared_secret, slen);
	}

	debug2("kex_derive_keys");
	for (mode = 0; mode < MODE_MAX; mode++) {
		current_keys[mode] = kex->newkeys[mode];
		kex->newkeys[mode] = NULL;
		ctos = (!kex->server && mode == MODE_OUT) ||
		    (kex->server && mode == MODE_IN);
		current_keys[mode]->enc.iv  = keys[ctos ? 0 : 1];
		current_keys[mode]->enc.key = keys[ctos ? 2 : 3];
		current_keys[mode]->mac.key = keys[ctos ? 4 : 5];
	}
}

void
kex_derive_keys_bn(Kex *kex, u_char *hash, u_int hashlen, const BIGNUM *secret)
{
	Buffer shared_secret;

	buffer_init(&shared_secret);
	buffer_put_bignum2(&shared_secret, secret);
	kex_derive_keys(kex, hash, hashlen,
	    buffer_ptr(&shared_secret), buffer_len(&shared_secret));
	buffer_free(&shared_secret);
}

Newkeys *
kex_get_newkeys(int mode)
{
	Newkeys *ret;

	ret = current_keys[mode];
	current_keys[mode] = NULL;
	return ret;
}

void
derive_ssh1_session_id(BIGNUM *host_modulus, BIGNUM *server_modulus,
    u_int8_t cookie[8], u_int8_t id[16])
{
	u_int8_t nbuf[2048], obuf[SSH_DIGEST_MAX_LENGTH];
	int len;
	struct ssh_digest_ctx *hashctx;

	if ((hashctx = ssh_digest_start(SSH_DIGEST_MD5)) == NULL)
		fatal("%s: ssh_digest_start", __func__);

	len = BN_num_bytes(host_modulus);
	if (len < (512 / 8) || (u_int)len > sizeof(nbuf))
		fatal("%s: bad host modulus (len %d)", __func__, len);
	BN_bn2bin(host_modulus, nbuf);
	if (ssh_digest_update(hashctx, nbuf, len) != 0)
		fatal("%s: ssh_digest_update failed", __func__);

	len = BN_num_bytes(server_modulus);
	if (len < (512 / 8) || (u_int)len > sizeof(nbuf))
		fatal("%s: bad server modulus (len %d)", __func__, len);
	BN_bn2bin(server_modulus, nbuf);
	if (ssh_digest_update(hashctx, nbuf, len) != 0 ||
	    ssh_digest_update(hashctx, cookie, 8) != 0)
		fatal("%s: ssh_digest_update failed", __func__);
	if (ssh_digest_final(hashctx, obuf, sizeof(obuf)) != 0)
		fatal("%s: ssh_digest_final failed", __func__);
	memcpy(id, obuf, ssh_digest_bytes(SSH_DIGEST_MD5));

	explicit_bzero(nbuf, sizeof(nbuf));
	explicit_bzero(obuf, sizeof(obuf));
}

#if defined(DEBUG_KEX) || defined(DEBUG_KEXDH) || defined(DEBUG_KEXECDH)
void
dump_digest(char *msg, u_char *digest, int len)
{
	int i;

	fprintf(stderr, "%s\n", msg);
	for (i = 0; i < len; i++) {
		fprintf(stderr, "%02x", digest[i]);
		if (i%32 == 31)
			fprintf(stderr, "\n");
		else if (i%8 == 7)
			fprintf(stderr, " ");
	}
	fprintf(stderr, "\n");
}
#endif
