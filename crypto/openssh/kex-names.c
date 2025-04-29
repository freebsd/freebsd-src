/* $OpenBSD: kex-names.c,v 1.4 2024/09/09 02:39:57 djm Exp $ */
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#ifdef WITH_OPENSSL
#include <openssl/crypto.h>
#include <openssl/evp.h>
#endif

#include "kex.h"
#include "log.h"
#include "match.h"
#include "digest.h"
#include "misc.h"

#include "ssherr.h"
#include "xmalloc.h"

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
#ifdef USE_SNTRUP761X25519
	{ KEX_SNTRUP761X25519_SHA512, KEX_KEM_SNTRUP761X25519_SHA512, 0,
	    SSH_DIGEST_SHA512 },
	{ KEX_SNTRUP761X25519_SHA512_OLD, KEX_KEM_SNTRUP761X25519_SHA512, 0,
	    SSH_DIGEST_SHA512 },
#endif
#ifdef USE_MLKEM768X25519
	{ KEX_MLKEM768X25519_SHA256, KEX_KEM_MLKEM768X25519_SHA256, 0,
	    SSH_DIGEST_SHA256 },
#endif
#endif /* HAVE_EVP_SHA256 || !WITH_OPENSSL */
	{ NULL, 0, -1, -1},
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

int
kex_name_valid(const char *name)
{
	return kex_alg_by_name(name) != NULL;
}

u_int
kex_type_from_name(const char *name)
{
	const struct kexalg *k;

	if ((k = kex_alg_by_name(name)) == NULL)
		return 0;
	return k->type;
}

int
kex_hash_from_name(const char *name)
{
	const struct kexalg *k;

	if ((k = kex_alg_by_name(name)) == NULL)
		return -1;
	return k->hash_alg;
}

int
kex_nid_from_name(const char *name)
{
	const struct kexalg *k;

	if ((k = kex_alg_by_name(name)) == NULL)
		return -1;
	return k->ec_nid;
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

/* returns non-zero if proposal contains any algorithm from algs */
int
kex_has_any_alg(const char *proposal, const char *algs)
{
	char *cp;

	if ((cp = match_list(proposal, algs, NULL)) == NULL)
		return 0;
	free(cp);
	return 1;
}

/*
 * Concatenate algorithm names, avoiding duplicates in the process.
 * Caller must free returned string.
 */
char *
kex_names_cat(const char *a, const char *b)
{
	char *ret = NULL, *tmp = NULL, *cp, *p;
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
		if (kex_has_any_alg(ret, p))
			continue; /* Algorithm already present */
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
 * indicate that it should be appended to the default, '-' that the
 * specified names should be removed, or '^' that they should be placed
 * at the head.
 */
int
kex_assemble_names(char **listp, const char *def, const char *all)
{
	char *cp, *tmp, *patterns;
	char *list = NULL, *ret = NULL, *matching = NULL, *opatterns = NULL;
	int r = SSH_ERR_INTERNAL_ERROR;

	if (listp == NULL || def == NULL || all == NULL)
		return SSH_ERR_INVALID_ARGUMENT;

	if (*listp == NULL || **listp == '\0') {
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
		if ((*listp = match_filter_denylist(def, list + 1)) == NULL) {
			r = SSH_ERR_ALLOC_FAIL;
			goto fail;
		}
		free(list);
		/* filtering has already been done */
		return 0;
	} else if (*list == '^') {
		/* Place names at head of default list */
		if ((tmp = kex_names_cat(list + 1, def)) == NULL) {
			r = SSH_ERR_ALLOC_FAIL;
			goto fail;
		}
		free(list);
		list = tmp;
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
		if ((matching = match_filter_allowlist(all, cp)) == NULL) {
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
