/* $OpenBSD: auth2-jpake.c,v 1.3 2009/03/05 07:18:19 djm Exp $ */
/*
 * Copyright (c) 2008 Damien Miller.  All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Server side of zero-knowledge password auth using J-PAKE protocol
 * as described in:
 *
 * F. Hao, P. Ryan, "Password Authenticated Key Exchange by Juggling",
 * 16th Workshop on Security Protocols, Cambridge, April 2008
 *
 * http://grouper.ieee.org/groups/1363/Research/contributions/hao-ryan-2008.pdf
 */

#ifdef JPAKE

#include <sys/types.h>
#include <sys/param.h>

#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <login_cap.h>

#include <openssl/bn.h>
#include <openssl/evp.h>

#include "xmalloc.h"
#include "ssh2.h"
#include "key.h"
#include "hostfile.h"
#include "auth.h"
#include "buffer.h"
#include "packet.h"
#include "dispatch.h"
#include "log.h"
#include "servconf.h"
#include "auth-options.h"
#include "canohost.h"
#ifdef GSSAPI
#include "ssh-gss.h"
#endif
#include "monitor_wrap.h"

#include "schnorr.h"
#include "jpake.h"

/*
 * XXX options->permit_empty_passwd (at the moment, they will be refused
 * anyway because they will mismatch on fake salt.
 */

/* Dispatch handlers */
static void input_userauth_jpake_client_step1(int, u_int32_t, void *);
static void input_userauth_jpake_client_step2(int, u_int32_t, void *);
static void input_userauth_jpake_client_confirm(int, u_int32_t, void *);

static int auth2_jpake_start(Authctxt *);

/* import */
extern ServerOptions options;
extern u_char *session_id2;
extern u_int session_id2_len;

/*
 * Attempt J-PAKE authentication.
 */
static int
userauth_jpake(Authctxt *authctxt)
{
	int authenticated = 0;

	packet_check_eom();

	debug("jpake-01@openssh.com requested");

	if (authctxt->user != NULL) {
		if (authctxt->jpake_ctx == NULL)
			authctxt->jpake_ctx = jpake_new();
		if (options.zero_knowledge_password_authentication)
			authenticated = auth2_jpake_start(authctxt);
	}

	return authenticated;
}

Authmethod method_jpake = {
	"jpake-01@openssh.com",
	userauth_jpake,
	&options.zero_knowledge_password_authentication
};

/* Clear context and callbacks */
void
auth2_jpake_stop(Authctxt *authctxt)
{
	/* unregister callbacks */
	dispatch_set(SSH2_MSG_USERAUTH_JPAKE_CLIENT_STEP1, NULL);
	dispatch_set(SSH2_MSG_USERAUTH_JPAKE_CLIENT_STEP2, NULL);
	dispatch_set(SSH2_MSG_USERAUTH_JPAKE_CLIENT_CONFIRM, NULL);
	if (authctxt->jpake_ctx != NULL) {
		jpake_free(authctxt->jpake_ctx);
		authctxt->jpake_ctx = NULL;
	}
}

/* Returns 1 if 'c' is a valid crypt(3) salt character, 0 otherwise */
static int
valid_crypt_salt(int c)
{
	if (c >= 'A' && c <= 'Z')
		return 1;
	if (c >= 'a' && c <= 'z')
		return 1;
	if (c >= '.' && c <= '9')
		return 1;
	return 0;
}

/*
 * Derive fake salt as H(username || first_private_host_key)
 * This provides relatively stable fake salts for non-existent
 * users and avoids the jpake method becoming an account validity
 * oracle.
 */
static void
derive_rawsalt(const char *username, u_char *rawsalt, u_int len)
{
	u_char *digest;
	u_int digest_len;
	Buffer b;
	Key *k;

	buffer_init(&b);
	buffer_put_cstring(&b, username);
	if ((k = get_hostkey_by_index(0)) == NULL ||
	    (k->flags & KEY_FLAG_EXT))
		fatal("%s: no hostkeys", __func__);
	switch (k->type) {
	case KEY_RSA1:
	case KEY_RSA:
		if (k->rsa->p == NULL || k->rsa->q == NULL)
			fatal("%s: RSA key missing p and/or q", __func__);
		buffer_put_bignum2(&b, k->rsa->p);
		buffer_put_bignum2(&b, k->rsa->q);
		break;
	case KEY_DSA:
		if (k->dsa->priv_key == NULL)
			fatal("%s: DSA key missing priv_key", __func__);
		buffer_put_bignum2(&b, k->dsa->priv_key);
		break;
	default:
		fatal("%s: unknown key type %d", __func__, k->type);
	}
	if (hash_buffer(buffer_ptr(&b), buffer_len(&b), EVP_sha256(),
	    &digest, &digest_len) != 0)
		fatal("%s: hash_buffer", __func__);
	buffer_free(&b);
	if (len > digest_len)
		fatal("%s: not enough bytes for rawsalt (want %u have %u)",
		    __func__, len, digest_len);
	memcpy(rawsalt, digest, len);
	bzero(digest, digest_len);
	xfree(digest);
}

/* ASCII an integer [0, 64) for inclusion in a password/salt */
static char
pw_encode64(u_int i64)
{
	const u_char e64[] =
	    "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	return e64[i64 % 64];
}

/* Generate ASCII salt bytes for user */
static char *
makesalt(u_int want, const char *user)
{
	u_char rawsalt[32];
	static char ret[33];
	u_int i;

	if (want > sizeof(ret) - 1)
		fatal("%s: want %u", __func__, want);

	derive_rawsalt(user, rawsalt, sizeof(rawsalt));
	bzero(ret, sizeof(ret));
	for (i = 0; i < want; i++)
		ret[i] = pw_encode64(rawsalt[i]);
	bzero(rawsalt, sizeof(rawsalt));

	return ret;
}

/*
 * Select the system's default password hashing scheme and generate
 * a stable fake salt under it for use by a non-existent account.
 * Prevents jpake method being used to infer the validity of accounts.
 */
static void
fake_salt_and_scheme(Authctxt *authctxt, char **salt, char **scheme)
{
	char *rounds_s, *style;
	long long rounds;
	login_cap_t *lc;


	if ((lc = login_getclass(authctxt->pw->pw_class)) == NULL &&
	    (lc = login_getclass(NULL)) == NULL)
		fatal("%s: login_getclass failed", __func__);
	style = login_getcapstr(lc, "localcipher", NULL, NULL);
	if (style == NULL)
		style = xstrdup("blowfish,6");
	login_close(lc);
	
	if ((rounds_s = strchr(style, ',')) != NULL)
		*rounds_s++ = '\0';
	rounds = strtonum(rounds_s, 1, 1<<31, NULL);
	
	if (strcmp(style, "md5") == 0) {
		xasprintf(salt, "$1$%s$", makesalt(8, authctxt->user));
		*scheme = xstrdup("md5");
	} else if (strcmp(style, "old") == 0) {
		*salt = xstrdup(makesalt(2, authctxt->user));
		*scheme = xstrdup("crypt");
	} else if (strcmp(style, "newsalt") == 0) {
		rounds = MAX(rounds, 7250);
		rounds = MIN(rounds, (1<<24) - 1);
		xasprintf(salt, "_%c%c%c%c%s",
		    pw_encode64(rounds), pw_encode64(rounds >> 6),
		    pw_encode64(rounds >> 12), pw_encode64(rounds >> 18),
		    makesalt(4, authctxt->user));
		*scheme = xstrdup("crypt-extended");
	} else {
		/* Default to blowfish */
		rounds = MAX(rounds, 3);
		rounds = MIN(rounds, 31);
		xasprintf(salt, "$2a$%02lld$%s", rounds,
		    makesalt(22, authctxt->user));
		*scheme = xstrdup("bcrypt");
	}
	xfree(style);
	debug3("%s: fake %s salt for user %s: %s",
	    __func__, *scheme, authctxt->user, *salt);
}

/*
 * Fetch password hashing scheme, password salt and derive shared secret
 * for user. If user does not exist, a fake but stable and user-unique
 * salt will be returned.
 */
void
auth2_jpake_get_pwdata(Authctxt *authctxt, BIGNUM **s,
    char **hash_scheme, char **salt)
{
	char *cp;
	u_char *secret;
	u_int secret_len, salt_len;

#ifdef JPAKE_DEBUG
	debug3("%s: valid %d pw %.5s...", __func__,
	    authctxt->valid, authctxt->pw->pw_passwd);
#endif

	*salt = NULL;
	*hash_scheme = NULL;
	if (authctxt->valid) {
		if (strncmp(authctxt->pw->pw_passwd, "$2$", 3) == 0 &&
		    strlen(authctxt->pw->pw_passwd) > 28) {
			/*
			 * old-variant bcrypt:
			 *     "$2$", 2 digit rounds, "$", 22 bytes salt
			 */
			salt_len = 3 + 2 + 1 + 22 + 1;
			*salt = xmalloc(salt_len);
			strlcpy(*salt, authctxt->pw->pw_passwd, salt_len);
			*hash_scheme = xstrdup("bcrypt");
		} else if (strncmp(authctxt->pw->pw_passwd, "$2a$", 4) == 0 &&
		    strlen(authctxt->pw->pw_passwd) > 29) {
			/*
			 * current-variant bcrypt:
			 *     "$2a$", 2 digit rounds, "$", 22 bytes salt
			 */
			salt_len = 4 + 2 + 1 + 22 + 1;
			*salt = xmalloc(salt_len);
			strlcpy(*salt, authctxt->pw->pw_passwd, salt_len);
			*hash_scheme = xstrdup("bcrypt");
		} else if (strncmp(authctxt->pw->pw_passwd, "$1$", 3) == 0 &&
		    strlen(authctxt->pw->pw_passwd) > 5) {
			/*
			 * md5crypt:
			 *     "$1$", salt until "$"
			 */
			cp = strchr(authctxt->pw->pw_passwd + 3, '$');
			if (cp != NULL) {
				salt_len = (cp - authctxt->pw->pw_passwd) + 1;
				*salt = xmalloc(salt_len);
				strlcpy(*salt, authctxt->pw->pw_passwd,
				    salt_len);
				*hash_scheme = xstrdup("md5crypt");
			}
		} else if (strncmp(authctxt->pw->pw_passwd, "_", 1) == 0 &&
		    strlen(authctxt->pw->pw_passwd) > 9) {
			/*
			 * BSDI extended crypt:
			 *     "_", 4 digits count, 4 chars salt
			 */
			salt_len = 1 + 4 + 4 + 1;
			*salt = xmalloc(salt_len);
			strlcpy(*salt, authctxt->pw->pw_passwd, salt_len);
			*hash_scheme = xstrdup("crypt-extended");
		} else if (strlen(authctxt->pw->pw_passwd) == 13  &&
		    valid_crypt_salt(authctxt->pw->pw_passwd[0]) &&
		    valid_crypt_salt(authctxt->pw->pw_passwd[1])) {
			/*
			 * traditional crypt:
			 *     2 chars salt
			 */
			salt_len = 2 + 1;
			*salt = xmalloc(salt_len);
			strlcpy(*salt, authctxt->pw->pw_passwd, salt_len);
			*hash_scheme = xstrdup("crypt");
		}
		if (*salt == NULL) {
			debug("%s: unrecognised crypt scheme for user %s",
			    __func__, authctxt->pw->pw_name);
		}
	}
	if (*salt == NULL)
		fake_salt_and_scheme(authctxt, salt, hash_scheme);

	if (hash_buffer(authctxt->pw->pw_passwd,
	    strlen(authctxt->pw->pw_passwd), EVP_sha256(),
	    &secret, &secret_len) != 0)
		fatal("%s: hash_buffer", __func__);
	if ((*s = BN_bin2bn(secret, secret_len, NULL)) == NULL)
		fatal("%s: BN_bin2bn (secret)", __func__);
#ifdef JPAKE_DEBUG
	debug3("%s: salt = %s (len %u)", __func__,
	    *salt, (u_int)strlen(*salt));
	debug3("%s: scheme = %s", __func__, *hash_scheme);
	JPAKE_DEBUG_BN((*s, "%s: s = ", __func__));
#endif
	bzero(secret, secret_len);
	xfree(secret);
}

/*
 * Begin authentication attempt.
 * Note, sets authctxt->postponed while in subprotocol
 */
static int
auth2_jpake_start(Authctxt *authctxt)
{
	struct jpake_ctx *pctx = authctxt->jpake_ctx;
	u_char *x3_proof, *x4_proof;
	u_int x3_proof_len, x4_proof_len;
	char *salt, *hash_scheme;

	debug("%s: start", __func__);

	PRIVSEP(jpake_step1(pctx->grp,
	    &pctx->server_id, &pctx->server_id_len,
	    &pctx->x3, &pctx->x4, &pctx->g_x3, &pctx->g_x4,
	    &x3_proof, &x3_proof_len,
	    &x4_proof, &x4_proof_len));

	PRIVSEP(auth2_jpake_get_pwdata(authctxt, &pctx->s,
	    &hash_scheme, &salt));

	if (!use_privsep)
		JPAKE_DEBUG_CTX((pctx, "step 1 sending in %s", __func__));

	packet_start(SSH2_MSG_USERAUTH_JPAKE_SERVER_STEP1);
	packet_put_cstring(hash_scheme);
	packet_put_cstring(salt);
	packet_put_string(pctx->server_id, pctx->server_id_len);
	packet_put_bignum2(pctx->g_x3);
	packet_put_bignum2(pctx->g_x4);
	packet_put_string(x3_proof, x3_proof_len);
	packet_put_string(x4_proof, x4_proof_len);
	packet_send();
	packet_write_wait();

	bzero(hash_scheme, strlen(hash_scheme));
	bzero(salt, strlen(salt));
	xfree(hash_scheme);
	xfree(salt);
	bzero(x3_proof, x3_proof_len);
	bzero(x4_proof, x4_proof_len);
	xfree(x3_proof);
	xfree(x4_proof);

	/* Expect step 1 packet from peer */
	dispatch_set(SSH2_MSG_USERAUTH_JPAKE_CLIENT_STEP1,
	    input_userauth_jpake_client_step1);

	authctxt->postponed = 1;
	return 0;
}

/* ARGSUSED */
static void
input_userauth_jpake_client_step1(int type, u_int32_t seq, void *ctxt)
{
	Authctxt *authctxt = ctxt;
	struct jpake_ctx *pctx = authctxt->jpake_ctx;
	u_char *x1_proof, *x2_proof, *x4_s_proof;
	u_int x1_proof_len, x2_proof_len, x4_s_proof_len;

	/* Disable this message */
	dispatch_set(SSH2_MSG_USERAUTH_JPAKE_CLIENT_STEP1, NULL);

	/* Fetch step 1 values */
	if ((pctx->g_x1 = BN_new()) == NULL ||
	    (pctx->g_x2 = BN_new()) == NULL)
		fatal("%s: BN_new", __func__);
	pctx->client_id = packet_get_string(&pctx->client_id_len);
	packet_get_bignum2(pctx->g_x1);
	packet_get_bignum2(pctx->g_x2);
	x1_proof = packet_get_string(&x1_proof_len);
	x2_proof = packet_get_string(&x2_proof_len);
	packet_check_eom();

	if (!use_privsep)
		JPAKE_DEBUG_CTX((pctx, "step 1 received in %s", __func__));

	PRIVSEP(jpake_step2(pctx->grp, pctx->s, pctx->g_x3,
	    pctx->g_x1, pctx->g_x2, pctx->x4,
	    pctx->client_id, pctx->client_id_len,
	    pctx->server_id, pctx->server_id_len,
	    x1_proof, x1_proof_len,
	    x2_proof, x2_proof_len,
	    &pctx->b,
	    &x4_s_proof, &x4_s_proof_len));

	bzero(x1_proof, x1_proof_len);
	bzero(x2_proof, x2_proof_len);
	xfree(x1_proof);
	xfree(x2_proof);

	if (!use_privsep)
		JPAKE_DEBUG_CTX((pctx, "step 2 sending in %s", __func__));

	/* Send values for step 2 */
	packet_start(SSH2_MSG_USERAUTH_JPAKE_SERVER_STEP2);
	packet_put_bignum2(pctx->b);
	packet_put_string(x4_s_proof, x4_s_proof_len);
	packet_send();
	packet_write_wait();

	bzero(x4_s_proof, x4_s_proof_len);
	xfree(x4_s_proof);

	/* Expect step 2 packet from peer */
	dispatch_set(SSH2_MSG_USERAUTH_JPAKE_CLIENT_STEP2,
	    input_userauth_jpake_client_step2);
}

/* ARGSUSED */
static void
input_userauth_jpake_client_step2(int type, u_int32_t seq, void *ctxt)
{
	Authctxt *authctxt = ctxt;
	struct jpake_ctx *pctx = authctxt->jpake_ctx;
	u_char *x2_s_proof;
	u_int x2_s_proof_len;

	/* Disable this message */
	dispatch_set(SSH2_MSG_USERAUTH_JPAKE_CLIENT_STEP2, NULL);

	if ((pctx->a = BN_new()) == NULL)
		fatal("%s: BN_new", __func__);

	/* Fetch step 2 values */
	packet_get_bignum2(pctx->a);
	x2_s_proof = packet_get_string(&x2_s_proof_len);
	packet_check_eom();

	if (!use_privsep)
		JPAKE_DEBUG_CTX((pctx, "step 2 received in %s", __func__));

	/* Derive shared key and calculate confirmation hash */
	PRIVSEP(jpake_key_confirm(pctx->grp, pctx->s, pctx->a,
	    pctx->x4, pctx->g_x3, pctx->g_x4, pctx->g_x1, pctx->g_x2,
	    pctx->server_id, pctx->server_id_len,
	    pctx->client_id, pctx->client_id_len,
	    session_id2, session_id2_len,
	    x2_s_proof, x2_s_proof_len,
	    &pctx->k,
	    &pctx->h_k_sid_sessid, &pctx->h_k_sid_sessid_len));

	bzero(x2_s_proof, x2_s_proof_len);
	xfree(x2_s_proof);

	if (!use_privsep)
		JPAKE_DEBUG_CTX((pctx, "confirm sending in %s", __func__));

	/* Send key confirmation proof */
	packet_start(SSH2_MSG_USERAUTH_JPAKE_SERVER_CONFIRM);
	packet_put_string(pctx->h_k_sid_sessid, pctx->h_k_sid_sessid_len);
	packet_send();
	packet_write_wait();

	/* Expect confirmation from peer */
	dispatch_set(SSH2_MSG_USERAUTH_JPAKE_CLIENT_CONFIRM,
	    input_userauth_jpake_client_confirm);
}

/* ARGSUSED */
static void
input_userauth_jpake_client_confirm(int type, u_int32_t seq, void *ctxt)
{
	Authctxt *authctxt = ctxt;
	struct jpake_ctx *pctx = authctxt->jpake_ctx;
	int authenticated = 0;

	/* Disable this message */
	dispatch_set(SSH2_MSG_USERAUTH_JPAKE_CLIENT_CONFIRM, NULL);

	pctx->h_k_cid_sessid = packet_get_string(&pctx->h_k_cid_sessid_len);
	packet_check_eom();

	if (!use_privsep)
		JPAKE_DEBUG_CTX((pctx, "confirm received in %s", __func__));

	/* Verify expected confirmation hash */
	if (PRIVSEP(jpake_check_confirm(pctx->k,
	    pctx->client_id, pctx->client_id_len,
	    session_id2, session_id2_len,
	    pctx->h_k_cid_sessid, pctx->h_k_cid_sessid_len)) == 1)
		authenticated = authctxt->valid ? 1 : 0;
	else
		debug("%s: confirmation mismatch", __func__);
		
	/* done */
	authctxt->postponed = 0;
	jpake_free(authctxt->jpake_ctx);
	authctxt->jpake_ctx = NULL;
	userauth_finish(authctxt, authenticated, method_jpake.name);
}

#endif /* JPAKE */

