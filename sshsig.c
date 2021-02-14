/*
 * Copyright (c) 2019 Google LLC
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

#include "includes.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "authfd.h"
#include "authfile.h"
#include "log.h"
#include "misc.h"
#include "sshbuf.h"
#include "sshsig.h"
#include "ssherr.h"
#include "sshkey.h"
#include "match.h"
#include "digest.h"

#define SIG_VERSION		0x01
#define MAGIC_PREAMBLE		"SSHSIG"
#define MAGIC_PREAMBLE_LEN	(sizeof(MAGIC_PREAMBLE) - 1)
#define BEGIN_SIGNATURE		"-----BEGIN SSH SIGNATURE-----\n"
#define END_SIGNATURE		"-----END SSH SIGNATURE-----"
#define RSA_SIGN_ALG		"rsa-sha2-512" /* XXX maybe make configurable */
#define RSA_SIGN_ALLOWED	"rsa-sha2-512,rsa-sha2-256"
#define HASHALG_DEFAULT		"sha512" /* XXX maybe make configurable */
#define HASHALG_ALLOWED		"sha256,sha512"

int
sshsig_armor(const struct sshbuf *blob, struct sshbuf **out)
{
	struct sshbuf *buf = NULL;
	int r = SSH_ERR_INTERNAL_ERROR;

	*out = NULL;

	if ((buf = sshbuf_new()) == NULL) {
		error("%s: sshbuf_new failed", __func__);
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}

	if ((r = sshbuf_put(buf, BEGIN_SIGNATURE,
	    sizeof(BEGIN_SIGNATURE)-1)) != 0) {
		error("%s: sshbuf_putf failed: %s", __func__, ssh_err(r));
		goto out;
	}

	if ((r = sshbuf_dtob64(blob, buf, 1)) != 0) {
		error("%s: Couldn't base64 encode signature blob: %s",
		    __func__, ssh_err(r));
		goto out;
	}

	if ((r = sshbuf_put(buf, END_SIGNATURE,
	    sizeof(END_SIGNATURE)-1)) != 0 ||
	    (r = sshbuf_put_u8(buf, '\n')) != 0) {
		error("%s: sshbuf_put failed: %s", __func__, ssh_err(r));
		goto out;
	}
	/* success */
	*out = buf;
	buf = NULL; /* transferred */
	r = 0;
 out:
	sshbuf_free(buf);
	return r;
}

int
sshsig_dearmor(struct sshbuf *sig, struct sshbuf **out)
{
	int r;
	size_t eoffset = 0;
	struct sshbuf *buf = NULL;
	struct sshbuf *sbuf = NULL;
	char *b64 = NULL;

	if ((sbuf = sshbuf_fromb(sig)) == NULL) {
		error("%s: sshbuf_fromb failed", __func__);
		return SSH_ERR_ALLOC_FAIL;
	}

	if ((r = sshbuf_cmp(sbuf, 0,
	    BEGIN_SIGNATURE, sizeof(BEGIN_SIGNATURE)-1)) != 0) {
		error("Couldn't parse signature: missing header");
		goto done;
	}

	if ((r = sshbuf_consume(sbuf, sizeof(BEGIN_SIGNATURE)-1)) != 0) {
		error("%s: sshbuf_consume failed: %s", __func__, ssh_err(r));
		goto done;
	}

	if ((r = sshbuf_find(sbuf, 0, "\n" END_SIGNATURE,
	    sizeof("\n" END_SIGNATURE)-1, &eoffset)) != 0) {
		error("Couldn't parse signature: missing footer");
		goto done;
	}

	if ((r = sshbuf_consume_end(sbuf, sshbuf_len(sbuf)-eoffset)) != 0) {
		error("%s: sshbuf_consume failed: %s", __func__, ssh_err(r));
		goto done;
	}

	if ((b64 = sshbuf_dup_string(sbuf)) == NULL) {
		error("%s: sshbuf_dup_string failed", __func__);
		r = SSH_ERR_ALLOC_FAIL;
		goto done;
	}

	if ((buf = sshbuf_new()) == NULL) {
		error("%s: sshbuf_new() failed", __func__);
		r = SSH_ERR_ALLOC_FAIL;
		goto done;
	}

	if ((r = sshbuf_b64tod(buf, b64)) != 0) {
		error("Couldn't decode signature: %s", ssh_err(r));
		goto done;
	}

	/* success */
	*out = buf;
	r = 0;
	buf = NULL; /* transferred */
done:
	sshbuf_free(buf);
	sshbuf_free(sbuf);
	free(b64);
	return r;
}

static int
sshsig_wrap_sign(struct sshkey *key, const char *hashalg,
    const struct sshbuf *h_message, const char *sig_namespace,
    struct sshbuf **out, sshsig_signer *signer, void *signer_ctx)
{
	int r;
	size_t slen = 0;
	u_char *sig = NULL;
	struct sshbuf *blob = NULL;
	struct sshbuf *tosign = NULL;
	const char *sign_alg = NULL;

	if ((tosign = sshbuf_new()) == NULL ||
	    (blob = sshbuf_new()) == NULL) {
		error("%s: sshbuf_new failed", __func__);
		r = SSH_ERR_ALLOC_FAIL;
		goto done;
	}

	if ((r = sshbuf_put(tosign, MAGIC_PREAMBLE, MAGIC_PREAMBLE_LEN)) != 0 ||
	    (r = sshbuf_put_cstring(tosign, sig_namespace)) != 0 ||
	    (r = sshbuf_put_string(tosign, NULL, 0)) != 0 || /* reserved */
	    (r = sshbuf_put_cstring(tosign, hashalg)) != 0 ||
	    (r = sshbuf_put_stringb(tosign, h_message)) != 0) {
		error("Couldn't construct message to sign: %s", ssh_err(r));
		goto done;
	}

	/* If using RSA keys then default to a good signature algorithm */
	if (sshkey_type_plain(key->type) == KEY_RSA)
		sign_alg = RSA_SIGN_ALG;

	if (signer != NULL) {
		if ((r = signer(key, &sig, &slen,
		    sshbuf_ptr(tosign), sshbuf_len(tosign),
		    sign_alg, 0, signer_ctx)) != 0) {
			error("Couldn't sign message: %s", ssh_err(r));
			goto done;
		}
	} else {
		if ((r = sshkey_sign(key, &sig, &slen,
		    sshbuf_ptr(tosign), sshbuf_len(tosign),
		    sign_alg, 0)) != 0) {
			error("Couldn't sign message: %s", ssh_err(r));
			goto done;
		}
	}

	if ((r = sshbuf_put(blob, MAGIC_PREAMBLE, MAGIC_PREAMBLE_LEN)) != 0 ||
	    (r = sshbuf_put_u32(blob, SIG_VERSION)) != 0 ||
	    (r = sshkey_puts(key, blob)) != 0 ||
	    (r = sshbuf_put_cstring(blob, sig_namespace)) != 0 ||
	    (r = sshbuf_put_string(blob, NULL, 0)) != 0 || /* reserved */
	    (r = sshbuf_put_cstring(blob, hashalg)) != 0 ||
	    (r = sshbuf_put_string(blob, sig, slen)) != 0) {
		error("Couldn't populate blob: %s", ssh_err(r));
		goto done;
	}

	*out = blob;
	blob = NULL;
	r = 0;
done:
	free(sig);
	sshbuf_free(blob);
	sshbuf_free(tosign);
	return r;
}

/* Check preamble and version. */
static int
sshsig_parse_preamble(struct sshbuf *buf)
{
	int r = SSH_ERR_INTERNAL_ERROR;
	uint32_t sversion;

	if ((r = sshbuf_cmp(buf, 0, MAGIC_PREAMBLE, MAGIC_PREAMBLE_LEN)) != 0 ||
	    (r = sshbuf_consume(buf, (sizeof(MAGIC_PREAMBLE)-1))) != 0 ||
	    (r = sshbuf_get_u32(buf, &sversion)) != 0) {
		error("Couldn't verify signature: invalid format");
		return r;
	}

	if (sversion > SIG_VERSION) {
		error("Signature version %lu is larger than supported "
		    "version %u", (unsigned long)sversion, SIG_VERSION);
		return SSH_ERR_INVALID_FORMAT;
	}
	return 0;
}

static int
sshsig_check_hashalg(const char *hashalg)
{
	if (hashalg == NULL ||
	    match_pattern_list(hashalg, HASHALG_ALLOWED, 0) == 1)
		return 0;
	error("%s: unsupported hash algorithm \"%.100s\"", __func__, hashalg);
	return SSH_ERR_SIGN_ALG_UNSUPPORTED;
}

static int
sshsig_peek_hashalg(struct sshbuf *signature, char **hashalgp)
{
	struct sshbuf *buf = NULL;
	char *hashalg = NULL;
	int r = SSH_ERR_INTERNAL_ERROR;

	if (hashalgp != NULL)
		*hashalgp = NULL;
	if ((buf = sshbuf_fromb(signature)) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((r = sshsig_parse_preamble(buf)) != 0)
		goto done;
	if ((r = sshbuf_get_string_direct(buf, NULL, NULL)) != 0 ||
	    (r = sshbuf_get_string_direct(buf, NULL, NULL)) != 0 ||
	    (r = sshbuf_get_string(buf, NULL, NULL)) != 0 ||
	    (r = sshbuf_get_cstring(buf, &hashalg, NULL)) != 0 ||
	    (r = sshbuf_get_string_direct(buf, NULL, NULL)) != 0) {
		error("Couldn't parse signature blob: %s", ssh_err(r));
		goto done;
	}

	/* success */
	r = 0;
	*hashalgp = hashalg;
	hashalg = NULL;
 done:
	free(hashalg);
	sshbuf_free(buf);
	return r;
}

static int
sshsig_wrap_verify(struct sshbuf *signature, const char *hashalg,
    const struct sshbuf *h_message, const char *expect_namespace,
    struct sshkey **sign_keyp)
{
	int r = SSH_ERR_INTERNAL_ERROR;
	struct sshbuf *buf = NULL, *toverify = NULL;
	struct sshkey *key = NULL;
	const u_char *sig;
	char *got_namespace = NULL, *sigtype = NULL, *sig_hashalg = NULL;
	size_t siglen;

	debug("%s: verify message length %zu", __func__, sshbuf_len(h_message));
	if (sign_keyp != NULL)
		*sign_keyp = NULL;

	if ((toverify = sshbuf_new()) == NULL) {
		error("%s: sshbuf_new failed", __func__);
		r = SSH_ERR_ALLOC_FAIL;
		goto done;
	}
	if ((r = sshbuf_put(toverify, MAGIC_PREAMBLE,
	    MAGIC_PREAMBLE_LEN)) != 0 ||
	    (r = sshbuf_put_cstring(toverify, expect_namespace)) != 0 ||
	    (r = sshbuf_put_string(toverify, NULL, 0)) != 0 || /* reserved */
	    (r = sshbuf_put_cstring(toverify, hashalg)) != 0 ||
	    (r = sshbuf_put_stringb(toverify, h_message)) != 0) {
		error("Couldn't construct message to verify: %s", ssh_err(r));
		goto done;
	}

	if ((r = sshsig_parse_preamble(signature)) != 0)
		goto done;

	if ((r = sshkey_froms(signature, &key)) != 0 ||
	    (r = sshbuf_get_cstring(signature, &got_namespace, NULL)) != 0 ||
	    (r = sshbuf_get_string(signature, NULL, NULL)) != 0 ||
	    (r = sshbuf_get_cstring(signature, &sig_hashalg, NULL)) != 0 ||
	    (r = sshbuf_get_string_direct(signature, &sig, &siglen)) != 0) {
		error("Couldn't parse signature blob: %s", ssh_err(r));
		goto done;
	}

	if (sshbuf_len(signature) != 0) {
		error("Signature contains trailing data");
		r = SSH_ERR_INVALID_FORMAT;
		goto done;
	}

	if (strcmp(expect_namespace, got_namespace) != 0) {
		error("Couldn't verify signature: namespace does not match");
		debug("%s: expected namespace \"%s\" received \"%s\"",
		    __func__, expect_namespace, got_namespace);
		r = SSH_ERR_SIGNATURE_INVALID;
		goto done;
	}
	if (strcmp(hashalg, sig_hashalg) != 0) {
		error("Couldn't verify signature: hash algorithm mismatch");
		debug("%s: expected algorithm \"%s\" received \"%s\"",
		    __func__, hashalg, sig_hashalg);
		r = SSH_ERR_SIGNATURE_INVALID;
		goto done;
	}
	/* Ensure that RSA keys use an acceptable signature algorithm */
	if (sshkey_type_plain(key->type) == KEY_RSA) {
		if ((r = sshkey_get_sigtype(sig, siglen, &sigtype)) != 0) {
			error("Couldn't verify signature: unable to get "
			    "signature type: %s", ssh_err(r));
			goto done;
		}
		if (match_pattern_list(sigtype, RSA_SIGN_ALLOWED, 0) != 1) {
			error("Couldn't verify signature: unsupported RSA "
			    "signature algorithm %s", sigtype);
			r = SSH_ERR_SIGN_ALG_UNSUPPORTED;
			goto done;
		}
	}
	if ((r = sshkey_verify(key, sig, siglen, sshbuf_ptr(toverify),
	    sshbuf_len(toverify), NULL, 0)) != 0) {
		error("Signature verification failed: %s", ssh_err(r));
		goto done;
	}

	/* success */
	r = 0;
	if (sign_keyp != NULL) {
		*sign_keyp = key;
		key = NULL; /* transferred */
	}
done:
	free(got_namespace);
	free(sigtype);
	free(sig_hashalg);
	sshbuf_free(buf);
	sshbuf_free(toverify);
	sshkey_free(key);
	return r;
}

static int
hash_buffer(const struct sshbuf *m, const char *hashalg, struct sshbuf **bp)
{
	char *hex, hash[SSH_DIGEST_MAX_LENGTH];
	int alg, r = SSH_ERR_INTERNAL_ERROR;
	struct sshbuf *b = NULL;

	*bp = NULL;
	memset(hash, 0, sizeof(hash));

	if ((r = sshsig_check_hashalg(hashalg)) != 0)
		return r;
	if ((alg = ssh_digest_alg_by_name(hashalg)) == -1) {
		error("%s: can't look up hash algorithm %s",
		    __func__, hashalg);
		return SSH_ERR_INTERNAL_ERROR;
	}
	if ((r = ssh_digest_buffer(alg, m, hash, sizeof(hash))) != 0) {
		error("%s: ssh_digest_buffer failed: %s", __func__, ssh_err(r));
		return r;
	}
	if ((hex = tohex(hash, ssh_digest_bytes(alg))) != NULL) {
		debug3("%s: final hash: %s", __func__, hex);
		freezero(hex, strlen(hex));
	}
	if ((b = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((r = sshbuf_put(b, hash, ssh_digest_bytes(alg))) != 0) {
		error("%s: sshbuf_put: %s", __func__, ssh_err(r));
		goto out;
	}
	*bp = b;
	b = NULL; /* transferred */
	/* success */
	r = 0;
 out:
	sshbuf_free(b);
	explicit_bzero(hash, sizeof(hash));
	return 0;
}

int
sshsig_signb(struct sshkey *key, const char *hashalg,
    const struct sshbuf *message, const char *sig_namespace,
    struct sshbuf **out, sshsig_signer *signer, void *signer_ctx)
{
	struct sshbuf *b = NULL;
	int r = SSH_ERR_INTERNAL_ERROR;

	if (hashalg == NULL)
		hashalg = HASHALG_DEFAULT;
	if (out != NULL)
		*out = NULL;
	if ((r = hash_buffer(message, hashalg, &b)) != 0) {
		error("%s: hash_buffer failed: %s", __func__, ssh_err(r));
		goto out;
	}
	if ((r = sshsig_wrap_sign(key, hashalg, b, sig_namespace, out,
	    signer, signer_ctx)) != 0)
		goto out;
	/* success */
	r = 0;
 out:
	sshbuf_free(b);
	return r;
}

int
sshsig_verifyb(struct sshbuf *signature, const struct sshbuf *message,
    const char *expect_namespace, struct sshkey **sign_keyp)
{
	struct sshbuf *b = NULL;
	int r = SSH_ERR_INTERNAL_ERROR;
	char *hashalg = NULL;

	if (sign_keyp != NULL)
		*sign_keyp = NULL;

	if ((r = sshsig_peek_hashalg(signature, &hashalg)) != 0)
		return r;
	debug("%s: signature made with hash \"%s\"", __func__, hashalg);
	if ((r = hash_buffer(message, hashalg, &b)) != 0) {
		error("%s: hash_buffer failed: %s", __func__, ssh_err(r));
		goto out;
	}
	if ((r = sshsig_wrap_verify(signature, hashalg, b, expect_namespace,
	    sign_keyp)) != 0)
		goto out;
	/* success */
	r = 0;
 out:
	sshbuf_free(b);
	free(hashalg);
	return r;
}

static int
hash_file(int fd, const char *hashalg, struct sshbuf **bp)
{
	char *hex, rbuf[8192], hash[SSH_DIGEST_MAX_LENGTH];
	ssize_t n, total = 0;
	struct ssh_digest_ctx *ctx;
	int alg, oerrno, r = SSH_ERR_INTERNAL_ERROR;
	struct sshbuf *b = NULL;

	*bp = NULL;
	memset(hash, 0, sizeof(hash));

	if ((r = sshsig_check_hashalg(hashalg)) != 0)
		return r;
	if ((alg = ssh_digest_alg_by_name(hashalg)) == -1) {
		error("%s: can't look up hash algorithm %s",
		    __func__, hashalg);
		return SSH_ERR_INTERNAL_ERROR;
	}
	if ((ctx = ssh_digest_start(alg)) == NULL) {
		error("%s: ssh_digest_start failed", __func__);
		return SSH_ERR_INTERNAL_ERROR;
	}
	for (;;) {
		if ((n = read(fd, rbuf, sizeof(rbuf))) == -1) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			oerrno = errno;
			error("%s: read: %s", __func__, strerror(errno));
			ssh_digest_free(ctx);
			errno = oerrno;
			r = SSH_ERR_SYSTEM_ERROR;
			goto out;
		} else if (n == 0) {
			debug2("%s: hashed %zu bytes", __func__, total);
			break; /* EOF */
		}
		total += (size_t)n;
		if ((r = ssh_digest_update(ctx, rbuf, (size_t)n)) != 0) {
			error("%s: ssh_digest_update: %s",
			    __func__, ssh_err(r));
			goto out;
		}
	}
	if ((r = ssh_digest_final(ctx, hash, sizeof(hash))) != 0) {
		error("%s: ssh_digest_final: %s", __func__, ssh_err(r));
		goto out;
	}
	if ((hex = tohex(hash, ssh_digest_bytes(alg))) != NULL) {
		debug3("%s: final hash: %s", __func__, hex);
		freezero(hex, strlen(hex));
	}
	if ((b = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((r = sshbuf_put(b, hash, ssh_digest_bytes(alg))) != 0) {
		error("%s: sshbuf_put: %s", __func__, ssh_err(r));
		goto out;
	}
	*bp = b;
	b = NULL; /* transferred */
	/* success */
	r = 0;
 out:
	sshbuf_free(b);
	ssh_digest_free(ctx);
	explicit_bzero(hash, sizeof(hash));
	return 0;
}

int
sshsig_sign_fd(struct sshkey *key, const char *hashalg,
    int fd, const char *sig_namespace, struct sshbuf **out,
    sshsig_signer *signer, void *signer_ctx)
{
	struct sshbuf *b = NULL;
	int r = SSH_ERR_INTERNAL_ERROR;

	if (hashalg == NULL)
		hashalg = HASHALG_DEFAULT;
	if (out != NULL)
		*out = NULL;
	if ((r = hash_file(fd, hashalg, &b)) != 0) {
		error("%s: hash_file failed: %s", __func__, ssh_err(r));
		return r;
	}
	if ((r = sshsig_wrap_sign(key, hashalg, b, sig_namespace, out,
	    signer, signer_ctx)) != 0)
		goto out;
	/* success */
	r = 0;
 out:
	sshbuf_free(b);
	return r;
}

int
sshsig_verify_fd(struct sshbuf *signature, int fd,
    const char *expect_namespace, struct sshkey **sign_keyp)
{
	struct sshbuf *b = NULL;
	int r = SSH_ERR_INTERNAL_ERROR;
	char *hashalg = NULL;

	if (sign_keyp != NULL)
		*sign_keyp = NULL;

	if ((r = sshsig_peek_hashalg(signature, &hashalg)) != 0)
		return r;
	debug("%s: signature made with hash \"%s\"", __func__, hashalg);
	if ((r = hash_file(fd, hashalg, &b)) != 0) {
		error("%s: hash_file failed: %s", __func__, ssh_err(r));
		goto out;
	}
	if ((r = sshsig_wrap_verify(signature, hashalg, b, expect_namespace,
	    sign_keyp)) != 0)
		goto out;
	/* success */
	r = 0;
 out:
	sshbuf_free(b);
	free(hashalg);
	return r;
}

struct sshsigopt {
	int ca;
	char *namespaces;
};

struct sshsigopt *
sshsigopt_parse(const char *opts, const char *path, u_long linenum,
    const char **errstrp)
{
	struct sshsigopt *ret;
	int r;
	const char *errstr = NULL;

	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;
	if (opts == NULL || *opts == '\0')
		return ret; /* Empty options yields empty options :) */

	while (*opts && *opts != ' ' && *opts != '\t') {
		/* flag options */
		if ((r = opt_flag("cert-authority", 0, &opts)) != -1) {
			ret->ca = 1;
		} else if (opt_match(&opts, "namespaces")) {
			if (ret->namespaces != NULL) {
				errstr = "multiple \"namespaces\" clauses";
				goto fail;
			}
			ret->namespaces = opt_dequote(&opts, &errstr);
			if (ret->namespaces == NULL)
				goto fail;
		}
		/*
		 * Skip the comma, and move to the next option
		 * (or break out if there are no more).
		 */
		if (*opts == '\0' || *opts == ' ' || *opts == '\t')
			break;		/* End of options. */
		/* Anything other than a comma is an unknown option */
		if (*opts != ',') {
			errstr = "unknown key option";
			goto fail;
		}
		opts++;
		if (*opts == '\0') {
			errstr = "unexpected end-of-options";
			goto fail;
		}
	}
	/* success */
	return ret;
 fail:
	if (errstrp != NULL)
		*errstrp = errstr;
	sshsigopt_free(ret);
	return NULL;
}

void
sshsigopt_free(struct sshsigopt *opts)
{
	if (opts == NULL)
		return;
	free(opts->namespaces);
	free(opts);
}

static int
check_allowed_keys_line(const char *path, u_long linenum, char *line,
    const struct sshkey *sign_key, const char *principal,
    const char *sig_namespace)
{
	struct sshkey *found_key = NULL;
	char *cp, *opts = NULL, *identities = NULL;
	int r, found = 0;
	const char *reason = NULL;
	struct sshsigopt *sigopts = NULL;

	if ((found_key = sshkey_new(KEY_UNSPEC)) == NULL) {
		error("%s: sshkey_new failed", __func__);
		return SSH_ERR_ALLOC_FAIL;
	}

	/* format: identity[,identity...] [option[,option...]] key */
	cp = line;
	cp = cp + strspn(cp, " \t"); /* skip leading whitespace */
	if (*cp == '#' || *cp == '\0')
		goto done;
	if ((identities = strdelimw(&cp)) == NULL) {
		error("%s:%lu: invalid line", path, linenum);
		goto done;
	}
	if (match_pattern_list(principal, identities, 0) != 1) {
		/* principal didn't match */
		goto done;
	}
	debug("%s: %s:%lu: matched principal \"%s\"",
	    __func__, path, linenum, principal);

	if (sshkey_read(found_key, &cp) != 0) {
		/* no key? Check for options */
		opts = cp;
		if (sshkey_advance_past_options(&cp) != 0) {
			error("%s:%lu: invalid options",
			    path, linenum);
			goto done;
		}
		*cp++ = '\0';
		skip_space(&cp);
		if (sshkey_read(found_key, &cp) != 0) {
			error("%s:%lu: invalid key", path,
			    linenum);
			goto done;
		}
	}
	debug3("%s:%lu: options %s", path, linenum, opts == NULL ? "" : opts);
	if ((sigopts = sshsigopt_parse(opts, path, linenum, &reason)) == NULL) {
		error("%s:%lu: bad options: %s", path, linenum, reason);
		goto done;
	}

	/* Check whether options preclude the use of this key */
	if (sigopts->namespaces != NULL &&
	    match_pattern_list(sig_namespace, sigopts->namespaces, 0) != 1) {
		error("%s:%lu: key is not permitted for use in signature "
		    "namespace \"%s\"", path, linenum, sig_namespace);
		goto done;
	}

	if (!sigopts->ca && sshkey_equal(found_key, sign_key)) {
		/* Exact match of key */
		debug("%s:%lu: matched key and principal", path, linenum);
		/* success */
		found = 1;
	} else if (sigopts->ca && sshkey_is_cert(sign_key) &&
	    sshkey_equal_public(sign_key->cert->signature_key, found_key)) {
		/* Match of certificate's CA key */
		if ((r = sshkey_cert_check_authority(sign_key, 0, 1,
		    principal, &reason)) != 0) {
			error("%s:%lu: certificate not authorized: %s",
			    path, linenum, reason);
			goto done;
		}
		debug("%s:%lu: matched certificate CA key", path, linenum);
		/* success */
		found = 1;
	} else {
		/* Principal matched but key didn't */
		goto done;
	}
 done:
	sshkey_free(found_key);
	sshsigopt_free(sigopts);
	return found ? 0 : SSH_ERR_KEY_NOT_FOUND;
}

int
sshsig_check_allowed_keys(const char *path, const struct sshkey *sign_key,
    const char *principal, const char *sig_namespace)
{
	FILE *f = NULL;
	char *line = NULL;
	size_t linesize = 0;
	u_long linenum = 0;
	int r, oerrno;

	/* Check key and principal against file */
	if ((f = fopen(path, "r")) == NULL) {
		oerrno = errno;
		error("Unable to open allowed keys file \"%s\": %s",
		    path, strerror(errno));
		errno = oerrno;
		return SSH_ERR_SYSTEM_ERROR;
	}

	while (getline(&line, &linesize, f) != -1) {
		linenum++;
		r = check_allowed_keys_line(path, linenum, line, sign_key,
		    principal, sig_namespace);
		free(line);
		line = NULL;
		if (r == SSH_ERR_KEY_NOT_FOUND)
			continue;
		else if (r == 0) {
			/* success */
			fclose(f);
			return 0;
		} else
			break;
	}
	/* Either we hit an error parsing or we simply didn't find the key */
	fclose(f);
	free(line);
	return r == 0 ? SSH_ERR_KEY_NOT_FOUND : r;
}
