/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
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
RCSID("$OpenBSD: kex.c,v 1.12 2000/10/11 20:27:23 markus Exp $");

#include "ssh.h"
#include "ssh2.h"
#include "xmalloc.h"
#include "buffer.h"
#include "bufaux.h"
#include "packet.h"
#include "compat.h"

#include <openssl/bn.h>
#include <openssl/dh.h>

#include <openssl/crypto.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/pem.h>

#include "kex.h"

#define KEX_COOKIE_LEN	16

Buffer *
kex_init(char *myproposal[PROPOSAL_MAX])
{
	int first_kex_packet_follows = 0;
	unsigned char cookie[KEX_COOKIE_LEN];
	u_int32_t rand = 0;
	int i;
	Buffer *ki = xmalloc(sizeof(*ki));
	for (i = 0; i < KEX_COOKIE_LEN; i++) {
		if (i % 4 == 0)
			rand = arc4random();
		cookie[i] = rand & 0xff;
		rand >>= 8;
	}
	buffer_init(ki);
	buffer_append(ki, (char *)cookie, sizeof cookie);
	for (i = 0; i < PROPOSAL_MAX; i++)
		buffer_put_cstring(ki, myproposal[i]);
	buffer_put_char(ki, first_kex_packet_follows);
	buffer_put_int(ki, 0);				/* uint32 reserved */
	return ki;
}

/* send kexinit, parse and save reply */
void
kex_exchange_kexinit(
    Buffer *my_kexinit, Buffer *peer_kexint,
    char *peer_proposal[PROPOSAL_MAX])
{
	int i;
	char *ptr;
	int plen;

	debug("send KEXINIT");
	packet_start(SSH2_MSG_KEXINIT);
	packet_put_raw(buffer_ptr(my_kexinit), buffer_len(my_kexinit));	
	packet_send();
	packet_write_wait();
	debug("done");

	/*
	 * read and save raw KEXINIT payload in buffer. this is used during
	 * computation of the session_id and the session keys.
	 */
	debug("wait KEXINIT");
	packet_read_expect(&plen, SSH2_MSG_KEXINIT);
	ptr = packet_get_raw(&plen);
	buffer_append(peer_kexint, ptr, plen);

	/* parse packet and save algorithm proposal */
	/* skip cookie */
	for (i = 0; i < KEX_COOKIE_LEN; i++)
		packet_get_char();
	/* extract kex init proposal strings */
	for (i = 0; i < PROPOSAL_MAX; i++) {
		peer_proposal[i] = packet_get_string(NULL);
		debug("got kexinit: %s", peer_proposal[i]);
	}
	/* first kex follow / reserved */
	i = packet_get_char();
	debug("first kex follow: %d ", i);
	i = packet_get_int();
	debug("reserved: %d ", i);
	packet_done();
	debug("done");
}

/* diffie-hellman-group1-sha1 */

int
dh_pub_is_valid(DH *dh, BIGNUM *dh_pub)
{
	int i;
	int n = BN_num_bits(dh_pub);
	int bits_set = 0;

	if (dh_pub->neg) {
		log("invalid public DH value: negativ");
		return 0;
	}
	for (i = 0; i <= n; i++)
		if (BN_is_bit_set(dh_pub, i))
			bits_set++;
	debug("bits set: %d/%d", bits_set, BN_num_bits(dh->p));

	/* if g==2 and bits_set==1 then computing log_g(dh_pub) is trivial */
	if (bits_set > 1 && (BN_cmp(dh_pub, dh->p) == -1))
		return 1;
	log("invalid public DH value (%d/%d)", bits_set, BN_num_bits(dh->p));
	return 0;
}

DH *
dh_gen_key(DH *dh)
{
	int tries = 0;

	do {
		if (DH_generate_key(dh) == 0)
			fatal("DH_generate_key");
		if (tries++ > 10)
			fatal("dh_new_group1: too many bad keys: giving up");
	} while (!dh_pub_is_valid(dh, dh->pub_key));
	return dh;
}

DH *
dh_new_group_asc(const char *gen, const char *modulus)
{
	DH *dh;
	int ret;

	dh = DH_new();
	if (dh == NULL)
		fatal("DH_new");

	if ((ret = BN_hex2bn(&dh->p, modulus)) < 0)
		fatal("BN_hex2bn p");
	if ((ret = BN_hex2bn(&dh->g, gen)) < 0)
		fatal("BN_hex2bn g");

	return (dh_gen_key(dh));
}

DH *
dh_new_group(BIGNUM *gen, BIGNUM *modulus)
{
	DH *dh;

	dh = DH_new();
	if (dh == NULL)
		fatal("DH_new");
	dh->p = modulus;
	dh->g = gen;

	return (dh_gen_key(dh));
}

DH *
dh_new_group1()
{
	static char *gen = "2", *group1 =
	    "FFFFFFFF" "FFFFFFFF" "C90FDAA2" "2168C234" "C4C6628B" "80DC1CD1"
	    "29024E08" "8A67CC74" "020BBEA6" "3B139B22" "514A0879" "8E3404DD"
	    "EF9519B3" "CD3A431B" "302B0A6D" "F25F1437" "4FE1356D" "6D51C245"
	    "E485B576" "625E7EC6" "F44C42E9" "A637ED6B" "0BFF5CB6" "F406B7ED"
	    "EE386BFB" "5A899FA5" "AE9F2411" "7C4B1FE6" "49286651" "ECE65381"
	    "FFFFFFFF" "FFFFFFFF";

	return (dh_new_group_asc(gen, group1));
}

void
dump_digest(unsigned char *digest, int len)
{
	int i;
	for (i = 0; i< len; i++){
		fprintf(stderr, "%02x", digest[i]);
		if(i%2!=0)
			fprintf(stderr, " ");
	}
	fprintf(stderr, "\n");
}

unsigned char *
kex_hash(
    char *client_version_string,
    char *server_version_string,
    char *ckexinit, int ckexinitlen,
    char *skexinit, int skexinitlen,
    char *serverhostkeyblob, int sbloblen,
    BIGNUM *client_dh_pub,
    BIGNUM *server_dh_pub,
    BIGNUM *shared_secret)
{
	Buffer b;
	static unsigned char digest[EVP_MAX_MD_SIZE];
	EVP_MD *evp_md = EVP_sha1();
	EVP_MD_CTX md;

	buffer_init(&b);
	buffer_put_string(&b, client_version_string, strlen(client_version_string));
	buffer_put_string(&b, server_version_string, strlen(server_version_string));

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
	dump_digest(digest, evp_md->md_size);
#endif
	return digest;
}

unsigned char *
kex_hash_gex(
    char *client_version_string,
    char *server_version_string,
    char *ckexinit, int ckexinitlen,
    char *skexinit, int skexinitlen,
    char *serverhostkeyblob, int sbloblen,
    int minbits, BIGNUM *prime, BIGNUM *gen,
    BIGNUM *client_dh_pub,
    BIGNUM *server_dh_pub,
    BIGNUM *shared_secret)
{
	Buffer b;
	static unsigned char digest[EVP_MAX_MD_SIZE];
	EVP_MD *evp_md = EVP_sha1();
	EVP_MD_CTX md;

	buffer_init(&b);
	buffer_put_string(&b, client_version_string, strlen(client_version_string));
	buffer_put_string(&b, server_version_string, strlen(server_version_string));

	/* kexinit messages: fake header: len+SSH2_MSG_KEXINIT */
	buffer_put_int(&b, ckexinitlen+1);
	buffer_put_char(&b, SSH2_MSG_KEXINIT);
	buffer_append(&b, ckexinit, ckexinitlen);
	buffer_put_int(&b, skexinitlen+1);
	buffer_put_char(&b, SSH2_MSG_KEXINIT);
	buffer_append(&b, skexinit, skexinitlen);

	buffer_put_string(&b, serverhostkeyblob, sbloblen);
	buffer_put_int(&b, minbits);
	buffer_put_bignum2(&b, prime);
	buffer_put_bignum2(&b, gen);
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
	dump_digest(digest, evp_md->md_size);
#endif
	return digest;
}

unsigned char *
derive_key(int id, int need, char unsigned *hash, BIGNUM *shared_secret)
{
	Buffer b;
	EVP_MD *evp_md = EVP_sha1();
	EVP_MD_CTX md;
	char c = id;
	int have;
	int mdsz = evp_md->md_size;
	unsigned char *digest = xmalloc(((need+mdsz-1)/mdsz)*mdsz);

	buffer_init(&b);
	buffer_put_bignum2(&b, shared_secret);

	EVP_DigestInit(&md, evp_md);
	EVP_DigestUpdate(&md, buffer_ptr(&b), buffer_len(&b));	/* shared_secret K */
	EVP_DigestUpdate(&md, hash, mdsz);		/* transport-06 */
	EVP_DigestUpdate(&md, &c, 1);			/* key id */
	EVP_DigestUpdate(&md, hash, mdsz);		/* session id */
	EVP_DigestFinal(&md, digest, NULL);

	/* expand */
	for (have = mdsz; need > have; have += mdsz) {
		EVP_DigestInit(&md, evp_md);
		EVP_DigestUpdate(&md, buffer_ptr(&b), buffer_len(&b));
		EVP_DigestUpdate(&md, hash, mdsz);
		EVP_DigestUpdate(&md, digest, have);
		EVP_DigestFinal(&md, digest + have, NULL);
	}
	buffer_free(&b);
#ifdef DEBUG_KEX
	fprintf(stderr, "Digest '%c'== ", c);
	dump_digest(digest, need);
#endif
	return digest;
}

#define NKEYS	6

#define	MAX_PROP	20
#define	SEP	","

char *
get_match(char *client, char *server)
{
	char *sproposals[MAX_PROP];
	char *c, *s, *p, *ret, *cp, *sp;
	int i, j, nproposals;

	c = cp = xstrdup(client);
	s = sp = xstrdup(server);

	for ((p = strsep(&sp, SEP)), i=0; p && *p != '\0'; 
	     (p = strsep(&sp, SEP)), i++) {
		if (i < MAX_PROP)
			sproposals[i] = p;
		else
			break;
	}
	nproposals = i;

	for ((p = strsep(&cp, SEP)), i=0; p && *p != '\0'; 
	     (p = strsep(&cp, SEP)), i++) {
		for (j = 0; j < nproposals; j++) {
			if (strcmp(p, sproposals[j]) == 0) {
				ret = xstrdup(p);
				xfree(c);
				xfree(s);
				return ret;
			}
		}
	}
	xfree(c);
	xfree(s);
	return NULL;
}
void
choose_enc(Enc *enc, char *client, char *server)
{
	char *name = get_match(client, server);
	if (name == NULL)
		fatal("no matching cipher found: client %s server %s", client, server);
	enc->cipher = cipher_by_name(name);
	if (enc->cipher == NULL)
		fatal("matching cipher is not supported: %s", name);
	enc->name = name;
	enc->enabled = 0;
	enc->iv = NULL;
	enc->key = NULL;
}
void
choose_mac(Mac *mac, char *client, char *server)
{
	char *name = get_match(client, server);
	if (name == NULL)
		fatal("no matching mac found: client %s server %s", client, server);
	if (strcmp(name, "hmac-md5") == 0) {
		mac->md = EVP_md5();
	} else if (strcmp(name, "hmac-sha1") == 0) {
		mac->md = EVP_sha1();
	} else if (strcmp(name, "hmac-ripemd160@openssh.com") == 0) {
		mac->md = EVP_ripemd160();
	} else {
		fatal("unsupported mac %s", name);
	}
	mac->name = name;
	mac->mac_len = mac->md->md_size;
	mac->key_len = (datafellows & SSH_BUG_HMAC) ? 16 : mac->mac_len;
	mac->key = NULL;
	mac->enabled = 0;
}
void
choose_comp(Comp *comp, char *client, char *server)
{
	char *name = get_match(client, server);
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
void
choose_kex(Kex *k, char *client, char *server)
{
	k->name = get_match(client, server);
	if (k->name == NULL)
		fatal("no kex alg");
	if (strcmp(k->name, KEX_DH1) == 0) {
		k->kex_type = DH_GRP1_SHA1;
	} else if (strcmp(k->name, KEX_DHGEX) == 0) {
		k->kex_type = DH_GEX_SHA1;
	} else
		fatal("bad kex alg %s", k->name);
}
void
choose_hostkeyalg(Kex *k, char *client, char *server)
{
	k->hostkeyalg = get_match(client, server);
	if (k->hostkeyalg == NULL)
		fatal("no hostkey alg");
	if (strcmp(k->hostkeyalg, KEX_DSS) != 0)
		fatal("bad hostkey alg %s", k->hostkeyalg);
}

Kex *
kex_choose_conf(char *cprop[PROPOSAL_MAX], char *sprop[PROPOSAL_MAX], int server)
{
	int mode;
	int ctos;				/* direction: if true client-to-server */
	int need;
	Kex *k;

	k = xmalloc(sizeof(*k));
	memset(k, 0, sizeof(*k));
	k->server = server;

	for (mode = 0; mode < MODE_MAX; mode++) {
		int nenc, nmac, ncomp;
		ctos = (!k->server && mode == MODE_OUT) || (k->server && mode == MODE_IN);
		nenc  = ctos ? PROPOSAL_ENC_ALGS_CTOS  : PROPOSAL_ENC_ALGS_STOC;
		nmac  = ctos ? PROPOSAL_MAC_ALGS_CTOS  : PROPOSAL_MAC_ALGS_STOC;
		ncomp = ctos ? PROPOSAL_COMP_ALGS_CTOS : PROPOSAL_COMP_ALGS_STOC;
		choose_enc (&k->enc [mode], cprop[nenc],  sprop[nenc]);
		choose_mac (&k->mac [mode], cprop[nmac],  sprop[nmac]);
		choose_comp(&k->comp[mode], cprop[ncomp], sprop[ncomp]);
		debug("kex: %s %s %s %s",
		    ctos ? "client->server" : "server->client",
		    k->enc[mode].name,
		    k->mac[mode].name,
		    k->comp[mode].name);
	}
	choose_kex(k, cprop[PROPOSAL_KEX_ALGS], sprop[PROPOSAL_KEX_ALGS]);
	choose_hostkeyalg(k, cprop[PROPOSAL_SERVER_HOST_KEY_ALGS],
	    sprop[PROPOSAL_SERVER_HOST_KEY_ALGS]);
	need = 0;
	for (mode = 0; mode < MODE_MAX; mode++) {
	    if (need < k->enc[mode].cipher->key_len)
		    need = k->enc[mode].cipher->key_len;
	    if (need < k->enc[mode].cipher->block_size)
		    need = k->enc[mode].cipher->block_size;
	    if (need < k->mac[mode].key_len)
		    need = k->mac[mode].key_len;
	}
	/* XXX need runden? */
	k->we_need = need;
	return k;
}

int
kex_derive_keys(Kex *k, unsigned char *hash, BIGNUM *shared_secret)
{
	int i;
	int mode;
	int ctos;
	unsigned char *keys[NKEYS];

	for (i = 0; i < NKEYS; i++)
		keys[i] = derive_key('A'+i, k->we_need, hash, shared_secret);

	for (mode = 0; mode < MODE_MAX; mode++) {
		ctos = (!k->server && mode == MODE_OUT) || (k->server && mode == MODE_IN);
		k->enc[mode].iv  = keys[ctos ? 0 : 1];
		k->enc[mode].key = keys[ctos ? 2 : 3];
		k->mac[mode].key = keys[ctos ? 4 : 5];
	}
	return 0;
}
