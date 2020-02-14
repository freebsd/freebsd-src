/* $OpenBSD: kex.c,v 1.150 2019/01/21 12:08:13 djm Exp $ */
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

#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>

#ifdef WITH_OPENSSL
#include <openssl/crypto.h>
#include <openssl/dh.h>
#endif

#include "ssh.h"
#include "ssh2.h"
#include "atomicio.h"
#include "version.h"
#include "packet.h"
#include "compat.h"
#include "cipher.h"
#include "sshkey.h"
#include "kex.h"
#include "log.h"
#include "mac.h"
#include "match.h"
#include "misc.h"
#include "dispatch.h"
#include "monitor.h"

#include "ssherr.h"
#include "sshbuf.h"
#include "digest.h"

/* prototype */
static int kex_choose_conf(struct ssh *);
static int kex_input_newkeys(int, u_int32_t, struct ssh *);

static const char *proposal_names[PROPOSAL_MAX] = {
	"KEX algorithms",
	"host key algorithms",
	"ciphers ctos",
	"ciphers stoc",
	"MACs ctos",
	"MACs stoc",
	"compression ctos",
	"compression stoc",
	"languages ctos",
	"languages stoc",
};

struct kexalg {
	char *name;
	u_int type;
	int ec_nid;
	int hash_alg;
};
static const struct kexalg kexalgs[] = {
#ifdef WITH_OPENSSL
	{ KEX_DH1, KEX_DH_GRP1_SHA1, 0, SSH_DIGEST_SHA1 },
	{ KEX_DH14_SHA1, KEX_DH_GRP14_SHA1, 0, SSH_DIGEST_SHA1 },
	{ KEX_DH14_SHA256, KEX_DH_GRP14_SHA256, 0, SSH_DIGEST_SHA256 },
	{ KEX_DH16_SHA512, KEX_DH_GRP16_SHA512, 0, SSH_DIGEST_SHA512 },
	{ KEX_DH18_SHA512, KEX_DH_GRP18_SHA512, 0, SSH_DIGEST_SHA512 },
	{ KEX_DHGEX_SHA1, KEX_DH_GEX_SHA1, 0, SSH_DIGEST_SHA1 },
#ifdef HAVE_EVP_SHA256
	{ KEX_DHGEX_SHA256, KEX_DH_GEX_SHA256, 0, SSH_DIGEST_SHA256 },
#endif /* HAVE_EVP_SHA256 */
#ifdef OPENSSL_HAS_ECC
	{ KEX_ECDH_SHA2_NISTP256, KEX_ECDH_SHA2,
	    NID_X9_62_prime256v1, SSH_DIGEST_SHA256 },
	{ KEX_ECDH_SHA2_NISTP384, KEX_ECDH_SHA2, NID_secp384r1,
	    SSH_DIGEST_SHA384 },
# ifdef OPENSSL_HAS_NISTP521
	{ KEX_ECDH_SHA2_NISTP521, KEX_ECDH_SHA2, NID_secp521r1,
	    SSH_DIGEST_SHA512 },
# endif /* OPENSSL_HAS_NISTP521 */
#endif /* OPENSSL_HAS_ECC */
#endif /* WITH_OPENSSL */
#if defined(HAVE_EVP_SHA256) || !defined(WITH_OPENSSL)
	{ KEX_CURVE25519_SHA256, KEX_C25519_SHA256, 0, SSH_DIGEST_SHA256 },
	{ KEX_CURVE25519_SHA256_OLD, KEX_C25519_SHA256, 0, SSH_DIGEST_SHA256 },
	{ KEX_SNTRUP4591761X25519_SHA512, KEX_KEM_SNTRUP4591761X25519_SHA512, 0,
	    SSH_DIGEST_SHA512 },
#endif /* HAVE_EVP_SHA256 || !WITH_OPENSSL */
	{ NULL, -1, -1, -1},
};

char *
kex_alg_list(char sep)
{
	char *ret = NULL, *tmp;
	size_t nlen, rlen = 0;
	const struct kexalg *k;

	for (k = kexalgs; k->name != NULL; k++) {
		if (ret != NULL)
			ret[rlen++] = sep;
		nlen = strlen(k->name);
		if ((tmp = realloc(ret, rlen + nlen + 2)) == NULL) {
			free(ret);
			return NULL;
		}
		ret = tmp;
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
	if ((s = cp = strdup(names)) == NULL)
		return 0;
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

/*
 * Concatenate algorithm names, avoiding duplicates in the process.
 * Caller must free returned string.
 */
char *
kex_names_cat(const char *a, const char *b)
{
	char *ret = NULL, *tmp = NULL, *cp, *p, *m;
	size_t len;

	if (a == NULL || *a == '\0')
		return strdup(b);
	if (b == NULL || *b == '\0')
		return strdup(a);
	if (strlen(b) > 1024*1024)
		return NULL;
	len = strlen(a) + strlen(b) + 2;
	if ((tmp = cp = strdup(b)) == NULL ||
	    (ret = calloc(1, len)) == NULL) {
		free(tmp);
		return NULL;
	}
	strlcpy(ret, a, len);
	for ((p = strsep(&cp, ",")); p && *p != '\0'; (p = strsep(&cp, ","))) {
		if ((m = match_list(ret, p, NULL)) != NULL) {
			free(m);
			continue; /* Algorithm already present */
		}
		if (strlcat(ret, ",", len) >= len ||
		    strlcat(ret, p, len) >= len) {
			free(tmp);
			free(ret);
			return NULL; /* Shouldn't happen */
		}
	}
	free(tmp);
	return ret;
}

/*
 * Assemble a list of algorithms from a default list and a string from a
 * configuration file. The user-provided string may begin with '+' to
 * indicate that it should be appended to the default or '-' that the
 * specified names should be removed.
 */
int
kex_assemble_names(char **listp, const char *def, const char *all)
{
	char *cp, *tmp, *patterns;
	char *list = NULL, *ret = NULL, *matching = NULL, *opatterns = NULL;
	int r = SSH_ERR_INTERNAL_ERROR;

	if (listp == NULL || *listp == NULL || **listp == '\0') {
		if ((*listp = strdup(def)) == NULL)
			return SSH_ERR_ALLOC_FAIL;
		return 0;
	}

	list = *listp;
	*listp = NULL;
	if (*list == '+') {
		/* Append names to default list */
		if ((tmp = kex_names_cat(def, list + 1)) == NULL) {
			r = SSH_ERR_ALLOC_FAIL;
			goto fail;
		}
		free(list);
		list = tmp;
	} else if (*list == '-') {
		/* Remove names from default list */
		if ((*listp = match_filter_blacklist(def, list + 1)) == NULL) {
			r = SSH_ERR_ALLOC_FAIL;
			goto fail;
		}
		free(list);
		/* filtering has already been done */
		return 0;
	} else {
		/* Explicit list, overrides default - just use "list" as is */
	}

	/*
	 * The supplied names may be a pattern-list. For the -list case,
	 * the patterns are applied above. For the +list and explicit list
	 * cases we need to do it now.
	 */
	ret = NULL;
	if ((patterns = opatterns = strdup(list)) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto fail;
	}
	/* Apply positive (i.e. non-negated) patterns from the list */
	while ((cp = strsep(&patterns, ",")) != NULL) {
		if (*cp == '!') {
			/* negated matches are not supported here */
			r = SSH_ERR_INVALID_ARGUMENT;
			goto fail;
		}
		free(matching);
		if ((matching = match_filter_whitelist(all, cp)) == NULL) {
			r = SSH_ERR_ALLOC_FAIL;
			goto fail;
		}
		if ((tmp = kex_names_cat(ret, matching)) == NULL) {
			r = SSH_ERR_ALLOC_FAIL;
			goto fail;
		}
		free(ret);
		ret = tmp;
	}
	if (ret == NULL || *ret == '\0') {
		/* An empty name-list is an error */
		/* XXX better error code? */
		r = SSH_ERR_INVALID_ARGUMENT;
		goto fail;
	}

	/* success */
	*listp = ret;
	ret = NULL;
	r = 0;

 fail:
	free(matching);
	free(opatterns);
	free(list);
	free(ret);
	return r;
}

/* put algorithm proposal into buffer */
int
kex_prop2buf(struct sshbuf *b, char *proposal[PROPOSAL_MAX])
{
	u_int i;
	int r;

	sshbuf_reset(b);

	/*
	 * add a dummy cookie, the cookie will be overwritten by
	 * kex_send_kexinit(), each time a kexinit is set
	 */
	for (i = 0; i < KEX_COOKIE_LEN; i++) {
		if ((r = sshbuf_put_u8(b, 0)) != 0)
			return r;
	}
	for (i = 0; i < PROPOSAL_MAX; i++) {
		if ((r = sshbuf_put_cstring(b, proposal[i])) != 0)
			return r;
	}
	if ((r = sshbuf_put_u8(b, 0)) != 0 ||	/* first_kex_packet_follows */
	    (r = sshbuf_put_u32(b, 0)) != 0)	/* uint32 reserved */
		return r;
	return 0;
}

/* parse buffer and return algorithm proposal */
int
kex_buf2prop(struct sshbuf *raw, int *first_kex_follows, char ***propp)
{
	struct sshbuf *b = NULL;
	u_char v;
	u_int i;
	char **proposal = NULL;
	int r;

	*propp = NULL;
	if ((proposal = calloc(PROPOSAL_MAX, sizeof(char *))) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((b = sshbuf_fromb(raw)) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((r = sshbuf_consume(b, KEX_COOKIE_LEN)) != 0) /* skip cookie */
		goto out;
	/* extract kex init proposal strings */
	for (i = 0; i < PROPOSAL_MAX; i++) {
		if ((r = sshbuf_get_cstring(b, &(proposal[i]), NULL)) != 0)
			goto out;
		debug2("%s: %s", proposal_names[i], proposal[i]);
	}
	/* first kex follows / reserved */
	if ((r = sshbuf_get_u8(b, &v)) != 0 ||	/* first_kex_follows */
	    (r = sshbuf_get_u32(b, &i)) != 0)	/* reserved */
		goto out;
	if (first_kex_follows != NULL)
		*first_kex_follows = v;
	debug2("first_kex_follows %d ", v);
	debug2("reserved %u ", i);
	r = 0;
	*propp = proposal;
 out:
	if (r != 0 && proposal != NULL)
		kex_prop_free(proposal);
	sshbuf_free(b);
	return r;
}

void
kex_prop_free(char **proposal)
{
	u_int i;

	if (proposal == NULL)
		return;
	for (i = 0; i < PROPOSAL_MAX; i++)
		free(proposal[i]);
	free(proposal);
}

/* ARGSUSED */
static int
kex_protocol_error(int type, u_int32_t seq, struct ssh *ssh)
{
	int r;

	error("kex protocol error: type %d seq %u", type, seq);
	if ((r = sshpkt_start(ssh, SSH2_MSG_UNIMPLEMENTED)) != 0 ||
	    (r = sshpkt_put_u32(ssh, seq)) != 0 ||
	    (r = sshpkt_send(ssh)) != 0)
		return r;
	return 0;
}

static void
kex_reset_dispatch(struct ssh *ssh)
{
	ssh_dispatch_range(ssh, SSH2_MSG_TRANSPORT_MIN,
	    SSH2_MSG_TRANSPORT_MAX, &kex_protocol_error);
}

static int
kex_send_ext_info(struct ssh *ssh)
{
	int r;
	char *algs;

	if ((algs = sshkey_alg_list(0, 1, 1, ',')) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	/* XXX filter algs list by allowed pubkey/hostbased types */
	if ((r = sshpkt_start(ssh, SSH2_MSG_EXT_INFO)) != 0 ||
	    (r = sshpkt_put_u32(ssh, 1)) != 0 ||
	    (r = sshpkt_put_cstring(ssh, "server-sig-algs")) != 0 ||
	    (r = sshpkt_put_cstring(ssh, algs)) != 0 ||
	    (r = sshpkt_send(ssh)) != 0)
		goto out;
	/* success */
	r = 0;
 out:
	free(algs);
	return r;
}

int
kex_send_newkeys(struct ssh *ssh)
{
	int r;

	kex_reset_dispatch(ssh);
	if ((r = sshpkt_start(ssh, SSH2_MSG_NEWKEYS)) != 0 ||
	    (r = sshpkt_send(ssh)) != 0)
		return r;
	debug("SSH2_MSG_NEWKEYS sent");
	debug("expecting SSH2_MSG_NEWKEYS");
	ssh_dispatch_set(ssh, SSH2_MSG_NEWKEYS, &kex_input_newkeys);
	if (ssh->kex->ext_info_c)
		if ((r = kex_send_ext_info(ssh)) != 0)
			return r;
	return 0;
}

int
kex_input_ext_info(int type, u_int32_t seq, struct ssh *ssh)
{
	struct kex *kex = ssh->kex;
	u_int32_t i, ninfo;
	char *name;
	u_char *val;
	size_t vlen;
	int r;

	debug("SSH2_MSG_EXT_INFO received");
	ssh_dispatch_set(ssh, SSH2_MSG_EXT_INFO, &kex_protocol_error);
	if ((r = sshpkt_get_u32(ssh, &ninfo)) != 0)
		return r;
	for (i = 0; i < ninfo; i++) {
		if ((r = sshpkt_get_cstring(ssh, &name, NULL)) != 0)
			return r;
		if ((r = sshpkt_get_string(ssh, &val, &vlen)) != 0) {
			free(name);
			return r;
		}
		if (strcmp(name, "server-sig-algs") == 0) {
			/* Ensure no \0 lurking in value */
			if (memchr(val, '\0', vlen) != NULL) {
				error("%s: nul byte in %s", __func__, name);
				return SSH_ERR_INVALID_FORMAT;
			}
			debug("%s: %s=<%s>", __func__, name, val);
			kex->server_sig_algs = val;
			val = NULL;
		} else
			debug("%s: %s (unrecognised)", __func__, name);
		free(name);
		free(val);
	}
	return sshpkt_get_end(ssh);
}

static int
kex_input_newkeys(int type, u_int32_t seq, struct ssh *ssh)
{
	struct kex *kex = ssh->kex;
	int r;

	debug("SSH2_MSG_NEWKEYS received");
	ssh_dispatch_set(ssh, SSH2_MSG_NEWKEYS, &kex_protocol_error);
	ssh_dispatch_set(ssh, SSH2_MSG_KEXINIT, &kex_input_kexinit);
	if ((r = sshpkt_get_end(ssh)) != 0)
		return r;
	if ((r = ssh_set_newkeys(ssh, MODE_IN)) != 0)
		return r;
	kex->done = 1;
	kex->flags &= ~KEX_INITIAL;
	sshbuf_reset(kex->peer);
	/* sshbuf_reset(kex->my); */
	kex->flags &= ~KEX_INIT_SENT;
	free(kex->name);
	kex->name = NULL;
	return 0;
}

int
kex_send_kexinit(struct ssh *ssh)
{
	u_char *cookie;
	struct kex *kex = ssh->kex;
	int r;

	if (kex == NULL)
		return SSH_ERR_INTERNAL_ERROR;
	if (kex->flags & KEX_INIT_SENT)
		return 0;
	kex->done = 0;

	/* generate a random cookie */
	if (sshbuf_len(kex->my) < KEX_COOKIE_LEN)
		return SSH_ERR_INVALID_FORMAT;
	if ((cookie = sshbuf_mutable_ptr(kex->my)) == NULL)
		return SSH_ERR_INTERNAL_ERROR;
	arc4random_buf(cookie, KEX_COOKIE_LEN);

	if ((r = sshpkt_start(ssh, SSH2_MSG_KEXINIT)) != 0 ||
	    (r = sshpkt_putb(ssh, kex->my)) != 0 ||
	    (r = sshpkt_send(ssh)) != 0)
		return r;
	debug("SSH2_MSG_KEXINIT sent");
	kex->flags |= KEX_INIT_SENT;
	return 0;
}

/* ARGSUSED */
int
kex_input_kexinit(int type, u_int32_t seq, struct ssh *ssh)
{
	struct kex *kex = ssh->kex;
	const u_char *ptr;
	u_int i;
	size_t dlen;
	int r;

	debug("SSH2_MSG_KEXINIT received");
	if (kex == NULL)
		return SSH_ERR_INVALID_ARGUMENT;

	ssh_dispatch_set(ssh, SSH2_MSG_KEXINIT, NULL);
	ptr = sshpkt_ptr(ssh, &dlen);
	if ((r = sshbuf_put(kex->peer, ptr, dlen)) != 0)
		return r;

	/* discard packet */
	for (i = 0; i < KEX_COOKIE_LEN; i++)
		if ((r = sshpkt_get_u8(ssh, NULL)) != 0)
			return r;
	for (i = 0; i < PROPOSAL_MAX; i++)
		if ((r = sshpkt_get_string(ssh, NULL, NULL)) != 0)
			return r;
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
	if ((r = sshpkt_get_u8(ssh, NULL)) != 0 ||	/* first_kex_follows */
	    (r = sshpkt_get_u32(ssh, NULL)) != 0 ||	/* reserved */
	    (r = sshpkt_get_end(ssh)) != 0)
			return r;

	if (!(kex->flags & KEX_INIT_SENT))
		if ((r = kex_send_kexinit(ssh)) != 0)
			return r;
	if ((r = kex_choose_conf(ssh)) != 0)
		return r;

	if (kex->kex_type < KEX_MAX && kex->kex[kex->kex_type] != NULL)
		return (kex->kex[kex->kex_type])(ssh);

	return SSH_ERR_INTERNAL_ERROR;
}

struct kex *
kex_new(void)
{
	struct kex *kex;

	if ((kex = calloc(1, sizeof(*kex))) == NULL ||
	    (kex->peer = sshbuf_new()) == NULL ||
	    (kex->my = sshbuf_new()) == NULL ||
	    (kex->client_version = sshbuf_new()) == NULL ||
	    (kex->server_version = sshbuf_new()) == NULL) {
		kex_free(kex);
		return NULL;
	}
	return kex;
}

void
kex_free_newkeys(struct newkeys *newkeys)
{
	if (newkeys == NULL)
		return;
	if (newkeys->enc.key) {
		explicit_bzero(newkeys->enc.key, newkeys->enc.key_len);
		free(newkeys->enc.key);
		newkeys->enc.key = NULL;
	}
	if (newkeys->enc.iv) {
		explicit_bzero(newkeys->enc.iv, newkeys->enc.iv_len);
		free(newkeys->enc.iv);
		newkeys->enc.iv = NULL;
	}
	free(newkeys->enc.name);
	explicit_bzero(&newkeys->enc, sizeof(newkeys->enc));
	free(newkeys->comp.name);
	explicit_bzero(&newkeys->comp, sizeof(newkeys->comp));
	mac_clear(&newkeys->mac);
	if (newkeys->mac.key) {
		explicit_bzero(newkeys->mac.key, newkeys->mac.key_len);
		free(newkeys->mac.key);
		newkeys->mac.key = NULL;
	}
	free(newkeys->mac.name);
	explicit_bzero(&newkeys->mac, sizeof(newkeys->mac));
	explicit_bzero(newkeys, sizeof(*newkeys));
	free(newkeys);
}

void
kex_free(struct kex *kex)
{
	u_int mode;

	if (kex == NULL)
		return;

#ifdef WITH_OPENSSL
	DH_free(kex->dh);
#ifdef OPENSSL_HAS_ECC
	EC_KEY_free(kex->ec_client_key);
#endif /* OPENSSL_HAS_ECC */
#endif /* WITH_OPENSSL */
	for (mode = 0; mode < MODE_MAX; mode++) {
		kex_free_newkeys(kex->newkeys[mode]);
		kex->newkeys[mode] = NULL;
	}
	sshbuf_free(kex->peer);
	sshbuf_free(kex->my);
	sshbuf_free(kex->client_version);
	sshbuf_free(kex->server_version);
	sshbuf_free(kex->client_pub);
	free(kex->session_id);
	free(kex->failed_choice);
	free(kex->hostkey_alg);
	free(kex->name);
	free(kex);
}

int
kex_ready(struct ssh *ssh, char *proposal[PROPOSAL_MAX])
{
	int r;

	if ((r = kex_prop2buf(ssh->kex->my, proposal)) != 0)
		return r;
	ssh->kex->flags = KEX_INITIAL;
	kex_reset_dispatch(ssh);
	ssh_dispatch_set(ssh, SSH2_MSG_KEXINIT, &kex_input_kexinit);
	return 0;
}

int
kex_setup(struct ssh *ssh, char *proposal[PROPOSAL_MAX])
{
	int r;

	if ((r = kex_ready(ssh, proposal)) != 0)
		return r;
	if ((r = kex_send_kexinit(ssh)) != 0) {		/* we start */
		kex_free(ssh->kex);
		ssh->kex = NULL;
		return r;
	}
	return 0;
}

/*
 * Request key re-exchange, returns 0 on success or a ssherr.h error
 * code otherwise. Must not be called if KEX is incomplete or in-progress.
 */
int
kex_start_rekex(struct ssh *ssh)
{
	if (ssh->kex == NULL) {
		error("%s: no kex", __func__);
		return SSH_ERR_INTERNAL_ERROR;
	}
	if (ssh->kex->done == 0) {
		error("%s: requested twice", __func__);
		return SSH_ERR_INTERNAL_ERROR;
	}
	ssh->kex->done = 0;
	return kex_send_kexinit(ssh);
}

static int
choose_enc(struct sshenc *enc, char *client, char *server)
{
	char *name = match_list(client, server, NULL);

	if (name == NULL)
		return SSH_ERR_NO_CIPHER_ALG_MATCH;
	if ((enc->cipher = cipher_by_name(name)) == NULL) {
		free(name);
		return SSH_ERR_INTERNAL_ERROR;
	}
	enc->name = name;
	enc->enabled = 0;
	enc->iv = NULL;
	enc->iv_len = cipher_ivlen(enc->cipher);
	enc->key = NULL;
	enc->key_len = cipher_keylen(enc->cipher);
	enc->block_size = cipher_blocksize(enc->cipher);
	return 0;
}

static int
choose_mac(struct ssh *ssh, struct sshmac *mac, char *client, char *server)
{
	char *name = match_list(client, server, NULL);

	if (name == NULL)
		return SSH_ERR_NO_MAC_ALG_MATCH;
	if (mac_setup(mac, name) < 0) {
		free(name);
		return SSH_ERR_INTERNAL_ERROR;
	}
	mac->name = name;
	mac->key = NULL;
	mac->enabled = 0;
	return 0;
}

static int
choose_comp(struct sshcomp *comp, char *client, char *server)
{
	char *name = match_list(client, server, NULL);

	if (name == NULL)
		return SSH_ERR_NO_COMPRESS_ALG_MATCH;
	if (strcmp(name, "zlib@openssh.com") == 0) {
		comp->type = COMP_DELAYED;
	} else if (strcmp(name, "zlib") == 0) {
		comp->type = COMP_ZLIB;
	} else if (strcmp(name, "none") == 0) {
		comp->type = COMP_NONE;
	} else {
		free(name);
		return SSH_ERR_INTERNAL_ERROR;
	}
	comp->name = name;
	return 0;
}

static int
choose_kex(struct kex *k, char *client, char *server)
{
	const struct kexalg *kexalg;

	k->name = match_list(client, server, NULL);

	debug("kex: algorithm: %s", k->name ? k->name : "(no match)");
	if (k->name == NULL)
		return SSH_ERR_NO_KEX_ALG_MATCH;
	if ((kexalg = kex_alg_by_name(k->name)) == NULL)
		return SSH_ERR_INTERNAL_ERROR;
	k->kex_type = kexalg->type;
	k->hash_alg = kexalg->hash_alg;
	k->ec_nid = kexalg->ec_nid;
	return 0;
}

static int
choose_hostkeyalg(struct kex *k, char *client, char *server)
{
	k->hostkey_alg = match_list(client, server, NULL);

	debug("kex: host key algorithm: %s",
	    k->hostkey_alg ? k->hostkey_alg : "(no match)");
	if (k->hostkey_alg == NULL)
		return SSH_ERR_NO_HOSTKEY_ALG_MATCH;
	k->hostkey_type = sshkey_type_from_name(k->hostkey_alg);
	if (k->hostkey_type == KEY_UNSPEC)
		return SSH_ERR_INTERNAL_ERROR;
	k->hostkey_nid = sshkey_ecdsa_nid_from_name(k->hostkey_alg);
	return 0;
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

static int
kex_choose_conf(struct ssh *ssh)
{
	struct kex *kex = ssh->kex;
	struct newkeys *newkeys;
	char **my = NULL, **peer = NULL;
	char **cprop, **sprop;
	int nenc, nmac, ncomp;
	u_int mode, ctos, need, dh_need, authlen;
	int r, first_kex_follows;

	debug2("local %s KEXINIT proposal", kex->server ? "server" : "client");
	if ((r = kex_buf2prop(kex->my, NULL, &my)) != 0)
		goto out;
	debug2("peer %s KEXINIT proposal", kex->server ? "client" : "server");
	if ((r = kex_buf2prop(kex->peer, &first_kex_follows, &peer)) != 0)
		goto out;

	if (kex->server) {
		cprop=peer;
		sprop=my;
	} else {
		cprop=my;
		sprop=peer;
	}

	/* Check whether client supports ext_info_c */
	if (kex->server && (kex->flags & KEX_INITIAL)) {
		char *ext;

		ext = match_list("ext-info-c", peer[PROPOSAL_KEX_ALGS], NULL);
		kex->ext_info_c = (ext != NULL);
		free(ext);
	}

	/* Algorithm Negotiation */
	if ((r = choose_kex(kex, cprop[PROPOSAL_KEX_ALGS],
	    sprop[PROPOSAL_KEX_ALGS])) != 0) {
		kex->failed_choice = peer[PROPOSAL_KEX_ALGS];
		peer[PROPOSAL_KEX_ALGS] = NULL;
		goto out;
	}
	if ((r = choose_hostkeyalg(kex, cprop[PROPOSAL_SERVER_HOST_KEY_ALGS],
	    sprop[PROPOSAL_SERVER_HOST_KEY_ALGS])) != 0) {
		kex->failed_choice = peer[PROPOSAL_SERVER_HOST_KEY_ALGS];
		peer[PROPOSAL_SERVER_HOST_KEY_ALGS] = NULL;
		goto out;
	}
	for (mode = 0; mode < MODE_MAX; mode++) {
		if ((newkeys = calloc(1, sizeof(*newkeys))) == NULL) {
			r = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		kex->newkeys[mode] = newkeys;
		ctos = (!kex->server && mode == MODE_OUT) ||
		    (kex->server && mode == MODE_IN);
		nenc  = ctos ? PROPOSAL_ENC_ALGS_CTOS  : PROPOSAL_ENC_ALGS_STOC;
		nmac  = ctos ? PROPOSAL_MAC_ALGS_CTOS  : PROPOSAL_MAC_ALGS_STOC;
		ncomp = ctos ? PROPOSAL_COMP_ALGS_CTOS : PROPOSAL_COMP_ALGS_STOC;
		if ((r = choose_enc(&newkeys->enc, cprop[nenc],
		    sprop[nenc])) != 0) {
			kex->failed_choice = peer[nenc];
			peer[nenc] = NULL;
			goto out;
		}
		authlen = cipher_authlen(newkeys->enc.cipher);
		/* ignore mac for authenticated encryption */
		if (authlen == 0 &&
		    (r = choose_mac(ssh, &newkeys->mac, cprop[nmac],
		    sprop[nmac])) != 0) {
			kex->failed_choice = peer[nmac];
			peer[nmac] = NULL;
			goto out;
		}
		if ((r = choose_comp(&newkeys->comp, cprop[ncomp],
		    sprop[ncomp])) != 0) {
			kex->failed_choice = peer[ncomp];
			peer[ncomp] = NULL;
			goto out;
		}
		debug("kex: %s cipher: %s MAC: %s compression: %s",
		    ctos ? "client->server" : "server->client",
		    newkeys->enc.name,
		    authlen == 0 ? newkeys->mac.name : "<implicit>",
		    newkeys->comp.name);
	}
	need = dh_need = 0;
	for (mode = 0; mode < MODE_MAX; mode++) {
		newkeys = kex->newkeys[mode];
		need = MAXIMUM(need, newkeys->enc.key_len);
		need = MAXIMUM(need, newkeys->enc.block_size);
		need = MAXIMUM(need, newkeys->enc.iv_len);
		need = MAXIMUM(need, newkeys->mac.key_len);
		dh_need = MAXIMUM(dh_need, cipher_seclen(newkeys->enc.cipher));
		dh_need = MAXIMUM(dh_need, newkeys->enc.block_size);
		dh_need = MAXIMUM(dh_need, newkeys->enc.iv_len);
		dh_need = MAXIMUM(dh_need, newkeys->mac.key_len);
	}
	/* XXX need runden? */
	kex->we_need = need;
	kex->dh_need = dh_need;

	/* ignore the next message if the proposals do not match */
	if (first_kex_follows && !proposals_match(my, peer))
		ssh->dispatch_skip_packets = 1;
	r = 0;
 out:
	kex_prop_free(my);
	kex_prop_free(peer);
	return r;
}

static int
derive_key(struct ssh *ssh, int id, u_int need, u_char *hash, u_int hashlen,
    const struct sshbuf *shared_secret, u_char **keyp)
{
	struct kex *kex = ssh->kex;
	struct ssh_digest_ctx *hashctx = NULL;
	char c = id;
	u_int have;
	size_t mdsz;
	u_char *digest;
	int r;

	if ((mdsz = ssh_digest_bytes(kex->hash_alg)) == 0)
		return SSH_ERR_INVALID_ARGUMENT;
	if ((digest = calloc(1, ROUNDUP(need, mdsz))) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}

	/* K1 = HASH(K || H || "A" || session_id) */
	if ((hashctx = ssh_digest_start(kex->hash_alg)) == NULL ||
	    ssh_digest_update_buffer(hashctx, shared_secret) != 0 ||
	    ssh_digest_update(hashctx, hash, hashlen) != 0 ||
	    ssh_digest_update(hashctx, &c, 1) != 0 ||
	    ssh_digest_update(hashctx, kex->session_id,
	    kex->session_id_len) != 0 ||
	    ssh_digest_final(hashctx, digest, mdsz) != 0) {
		r = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	ssh_digest_free(hashctx);
	hashctx = NULL;

	/*
	 * expand key:
	 * Kn = HASH(K || H || K1 || K2 || ... || Kn-1)
	 * Key = K1 || K2 || ... || Kn
	 */
	for (have = mdsz; need > have; have += mdsz) {
		if ((hashctx = ssh_digest_start(kex->hash_alg)) == NULL ||
		    ssh_digest_update_buffer(hashctx, shared_secret) != 0 ||
		    ssh_digest_update(hashctx, hash, hashlen) != 0 ||
		    ssh_digest_update(hashctx, digest, have) != 0 ||
		    ssh_digest_final(hashctx, digest + have, mdsz) != 0) {
			r = SSH_ERR_LIBCRYPTO_ERROR;
			goto out;
		}
		ssh_digest_free(hashctx);
		hashctx = NULL;
	}
#ifdef DEBUG_KEX
	fprintf(stderr, "key '%c'== ", c);
	dump_digest("key", digest, need);
#endif
	*keyp = digest;
	digest = NULL;
	r = 0;
 out:
	free(digest);
	ssh_digest_free(hashctx);
	return r;
}

#define NKEYS	6
int
kex_derive_keys(struct ssh *ssh, u_char *hash, u_int hashlen,
    const struct sshbuf *shared_secret)
{
	struct kex *kex = ssh->kex;
	u_char *keys[NKEYS];
	u_int i, j, mode, ctos;
	int r;

	/* save initial hash as session id */
	if (kex->session_id == NULL) {
		kex->session_id_len = hashlen;
		kex->session_id = malloc(kex->session_id_len);
		if (kex->session_id == NULL)
			return SSH_ERR_ALLOC_FAIL;
		memcpy(kex->session_id, hash, kex->session_id_len);
	}
	for (i = 0; i < NKEYS; i++) {
		if ((r = derive_key(ssh, 'A'+i, kex->we_need, hash, hashlen,
		    shared_secret, &keys[i])) != 0) {
			for (j = 0; j < i; j++)
				free(keys[j]);
			return r;
		}
	}
	for (mode = 0; mode < MODE_MAX; mode++) {
		ctos = (!kex->server && mode == MODE_OUT) ||
		    (kex->server && mode == MODE_IN);
		kex->newkeys[mode]->enc.iv  = keys[ctos ? 0 : 1];
		kex->newkeys[mode]->enc.key = keys[ctos ? 2 : 3];
		kex->newkeys[mode]->mac.key = keys[ctos ? 4 : 5];
	}
	return 0;
}

int
kex_load_hostkey(struct ssh *ssh, struct sshkey **prvp, struct sshkey **pubp)
{
	struct kex *kex = ssh->kex;

	*pubp = NULL;
	*prvp = NULL;
	if (kex->load_host_public_key == NULL ||
	    kex->load_host_private_key == NULL)
		return SSH_ERR_INVALID_ARGUMENT;
	*pubp = kex->load_host_public_key(kex->hostkey_type,
	    kex->hostkey_nid, ssh);
	*prvp = kex->load_host_private_key(kex->hostkey_type,
	    kex->hostkey_nid, ssh);
	if (*pubp == NULL)
		return SSH_ERR_NO_HOSTKEY_LOADED;
	return 0;
}

int
kex_verify_host_key(struct ssh *ssh, struct sshkey *server_host_key)
{
	struct kex *kex = ssh->kex;

	if (kex->verify_host_key == NULL)
		return SSH_ERR_INVALID_ARGUMENT;
	if (server_host_key->type != kex->hostkey_type ||
	    (kex->hostkey_type == KEY_ECDSA &&
	    server_host_key->ecdsa_nid != kex->hostkey_nid))
		return SSH_ERR_KEY_TYPE_MISMATCH;
	if (kex->verify_host_key(server_host_key, ssh) == -1)
		return  SSH_ERR_SIGNATURE_INVALID;
	return 0;
}

#if defined(DEBUG_KEX) || defined(DEBUG_KEXDH) || defined(DEBUG_KEXECDH)
void
dump_digest(const char *msg, const u_char *digest, int len)
{
	fprintf(stderr, "%s\n", msg);
	sshbuf_dump_data(digest, len, stderr);
}
#endif

/*
 * Send a plaintext error message to the peer, suffixed by \r\n.
 * Only used during banner exchange, and there only for the server.
 */
static void
send_error(struct ssh *ssh, char *msg)
{
	char *crnl = "\r\n";

	if (!ssh->kex->server)
		return;

	if (atomicio(vwrite, ssh_packet_get_connection_out(ssh),
	    msg, strlen(msg)) != strlen(msg) ||
	    atomicio(vwrite, ssh_packet_get_connection_out(ssh),
	    crnl, strlen(crnl)) != strlen(crnl))
		error("%s: write: %.100s", __func__, strerror(errno));
}

/*
 * Sends our identification string and waits for the peer's. Will block for
 * up to timeout_ms (or indefinitely if timeout_ms <= 0).
 * Returns on 0 success or a ssherr.h code on failure.
 */
int
kex_exchange_identification(struct ssh *ssh, int timeout_ms,
    const char *version_addendum)
{
	int remote_major, remote_minor, mismatch;
	size_t len, i, n;
	int r, expect_nl;
	u_char c;
	struct sshbuf *our_version = ssh->kex->server ?
	    ssh->kex->server_version : ssh->kex->client_version;
	struct sshbuf *peer_version = ssh->kex->server ?
	    ssh->kex->client_version : ssh->kex->server_version;
	char *our_version_string = NULL, *peer_version_string = NULL;
	char *cp, *remote_version = NULL;

	/* Prepare and send our banner */
	sshbuf_reset(our_version);
	if (version_addendum != NULL && *version_addendum == '\0')
		version_addendum = NULL;
	if ((r = sshbuf_putf(our_version, "SSH-%d.%d-%.100s%s%s\r\n",
	   PROTOCOL_MAJOR_2, PROTOCOL_MINOR_2, SSH_VERSION,
	    version_addendum == NULL ? "" : " ",
	    version_addendum == NULL ? "" : version_addendum)) != 0) {
		error("%s: sshbuf_putf: %s", __func__, ssh_err(r));
		goto out;
	}

	if (atomicio(vwrite, ssh_packet_get_connection_out(ssh),
	    sshbuf_mutable_ptr(our_version),
	    sshbuf_len(our_version)) != sshbuf_len(our_version)) {
		error("%s: write: %.100s", __func__, strerror(errno));
		r = SSH_ERR_SYSTEM_ERROR;
		goto out;
	}
	if ((r = sshbuf_consume_end(our_version, 2)) != 0) { /* trim \r\n */
		error("%s: sshbuf_consume_end: %s", __func__, ssh_err(r));
		goto out;
	}
	our_version_string = sshbuf_dup_string(our_version);
	if (our_version_string == NULL) {
		error("%s: sshbuf_dup_string failed", __func__);
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	debug("Local version string %.100s", our_version_string);

	/* Read other side's version identification. */
	for (n = 0; ; n++) {
		if (n >= SSH_MAX_PRE_BANNER_LINES) {
			send_error(ssh, "No SSH identification string "
			    "received.");
			error("%s: No SSH version received in first %u lines "
			    "from server", __func__, SSH_MAX_PRE_BANNER_LINES);
			r = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
		sshbuf_reset(peer_version);
		expect_nl = 0;
		for (i = 0; ; i++) {
			if (timeout_ms > 0) {
				r = waitrfd(ssh_packet_get_connection_in(ssh),
				    &timeout_ms);
				if (r == -1 && errno == ETIMEDOUT) {
					send_error(ssh, "Timed out waiting "
					    "for SSH identification string.");
					error("Connection timed out during "
					    "banner exchange");
					r = SSH_ERR_CONN_TIMEOUT;
					goto out;
				} else if (r == -1) {
					error("%s: %s",
					    __func__, strerror(errno));
					r = SSH_ERR_SYSTEM_ERROR;
					goto out;
				}
			}

			len = atomicio(read, ssh_packet_get_connection_in(ssh),
			    &c, 1);
			if (len != 1 && errno == EPIPE) {
				error("%s: Connection closed by remote host",
				    __func__);
				r = SSH_ERR_CONN_CLOSED;
				goto out;
			} else if (len != 1) {
				error("%s: read: %.100s",
				    __func__, strerror(errno));
				r = SSH_ERR_SYSTEM_ERROR;
				goto out;
			}
			if (c == '\r') {
				expect_nl = 1;
				continue;
			}
			if (c == '\n')
				break;
			if (c == '\0' || expect_nl) {
				error("%s: banner line contains invalid "
				    "characters", __func__);
				goto invalid;
			}
			if ((r = sshbuf_put_u8(peer_version, c)) != 0) {
				error("%s: sshbuf_put: %s",
				    __func__, ssh_err(r));
				goto out;
			}
			if (sshbuf_len(peer_version) > SSH_MAX_BANNER_LEN) {
				error("%s: banner line too long", __func__);
				goto invalid;
			}
		}
		/* Is this an actual protocol banner? */
		if (sshbuf_len(peer_version) > 4 &&
		    memcmp(sshbuf_ptr(peer_version), "SSH-", 4) == 0)
			break;
		/* If not, then just log the line and continue */
		if ((cp = sshbuf_dup_string(peer_version)) == NULL) {
			error("%s: sshbuf_dup_string failed", __func__);
			r = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		/* Do not accept lines before the SSH ident from a client */
		if (ssh->kex->server) {
			error("%s: client sent invalid protocol identifier "
			    "\"%.256s\"", __func__, cp);
			free(cp);
			goto invalid;
		}
		debug("%s: banner line %zu: %s", __func__, n, cp);
		free(cp);
	}
	peer_version_string = sshbuf_dup_string(peer_version);
	if (peer_version_string == NULL)
		error("%s: sshbuf_dup_string failed", __func__);
	/* XXX must be same size for sscanf */
	if ((remote_version = calloc(1, sshbuf_len(peer_version))) == NULL) {
		error("%s: calloc failed", __func__);
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}

	/*
	 * Check that the versions match.  In future this might accept
	 * several versions and set appropriate flags to handle them.
	 */
	if (sscanf(peer_version_string, "SSH-%d.%d-%[^\n]\n",
	    &remote_major, &remote_minor, remote_version) != 3) {
		error("Bad remote protocol version identification: '%.100s'",
		    peer_version_string);
 invalid:
		send_error(ssh, "Invalid SSH identification string.");
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	debug("Remote protocol version %d.%d, remote software version %.100s",
	    remote_major, remote_minor, remote_version);
	ssh->compat = compat_datafellows(remote_version);

	mismatch = 0;
	switch (remote_major) {
	case 2:
		break;
	case 1:
		if (remote_minor != 99)
			mismatch = 1;
		break;
	default:
		mismatch = 1;
		break;
	}
	if (mismatch) {
		error("Protocol major versions differ: %d vs. %d",
		    PROTOCOL_MAJOR_2, remote_major);
		send_error(ssh, "Protocol major versions differ.");
		r = SSH_ERR_NO_PROTOCOL_VERSION;
		goto out;
	}

	if (ssh->kex->server && (ssh->compat & SSH_BUG_PROBE) != 0) {
		logit("probed from %s port %d with %s.  Don't panic.",
		    ssh_remote_ipaddr(ssh), ssh_remote_port(ssh),
		    peer_version_string);
		r = SSH_ERR_CONN_CLOSED; /* XXX */
		goto out;
	}
	if (ssh->kex->server && (ssh->compat & SSH_BUG_SCANNER) != 0) {
		logit("scanned from %s port %d with %s.  Don't panic.",
		    ssh_remote_ipaddr(ssh), ssh_remote_port(ssh),
		    peer_version_string);
		r = SSH_ERR_CONN_CLOSED; /* XXX */
		goto out;
	}
	if ((ssh->compat & SSH_BUG_RSASIGMD5) != 0) {
		logit("Remote version \"%.100s\" uses unsafe RSA signature "
		    "scheme; disabling use of RSA keys", remote_version);
	}
	/* success */
	r = 0;
 out:
	free(our_version_string);
	free(peer_version_string);
	free(remote_version);
	return r;
}

