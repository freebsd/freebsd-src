/*
 * Copyright (c) 2019 Markus Friedl
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

#ifdef ENABLE_SK_INTERNAL

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#ifdef HAVE_SHA2_H
#include <sha2.h>
#endif

#ifdef WITH_OPENSSL
#include <openssl/opensslv.h>
#include <openssl/crypto.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#endif /* WITH_OPENSSL */

#include <fido.h>
#include <fido/credman.h>

#ifndef SK_STANDALONE
# include "log.h"
# include "xmalloc.h"
/*
 * If building as part of OpenSSH, then rename exported functions.
 * This must be done before including sk-api.h.
 */
# define sk_api_version		ssh_sk_api_version
# define sk_enroll		ssh_sk_enroll
# define sk_sign		ssh_sk_sign
# define sk_load_resident_keys	ssh_sk_load_resident_keys
#endif /* !SK_STANDALONE */

#include "sk-api.h"

/* #define SK_DEBUG 1 */

#define MAX_FIDO_DEVICES	256

/* Compatibility with OpenSSH 1.0.x */
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
#define ECDSA_SIG_get0(sig, pr, ps) \
	do { \
		(*pr) = sig->r; \
		(*ps) = sig->s; \
	} while (0)
#endif

/* Return the version of the middleware API */
uint32_t sk_api_version(void);

/* Enroll a U2F key (private key generation) */
int sk_enroll(uint32_t alg, const uint8_t *challenge, size_t challenge_len,
    const char *application, uint8_t flags, const char *pin,
    struct sk_option **options, struct sk_enroll_response **enroll_response);

/* Sign a challenge */
int sk_sign(uint32_t alg, const uint8_t *message, size_t message_len,
    const char *application, const uint8_t *key_handle, size_t key_handle_len,
    uint8_t flags, const char *pin, struct sk_option **options,
    struct sk_sign_response **sign_response);

/* Load resident keys */
int sk_load_resident_keys(const char *pin, struct sk_option **options,
    struct sk_resident_key ***rks, size_t *nrks);

static void skdebug(const char *func, const char *fmt, ...)
    __attribute__((__format__ (printf, 2, 3)));

static void
skdebug(const char *func, const char *fmt, ...)
{
#if !defined(SK_STANDALONE)
	char *msg;
	va_list ap;

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);
	debug("%s: %s", func, msg);
	free(msg);
#elif defined(SK_DEBUG)
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "%s: ", func);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
#else
	(void)func; /* XXX */
	(void)fmt; /* XXX */
#endif
}

uint32_t
sk_api_version(void)
{
	return SSH_SK_VERSION_MAJOR;
}

/* Select the first identified FIDO device attached to the system */
static char *
pick_first_device(void)
{
	char *ret = NULL;
	fido_dev_info_t *devlist = NULL;
	size_t olen = 0;
	int r;
	const fido_dev_info_t *di;

	if ((devlist = fido_dev_info_new(1)) == NULL) {
		skdebug(__func__, "fido_dev_info_new failed");
		goto out;
	}
	if ((r = fido_dev_info_manifest(devlist, 1, &olen)) != FIDO_OK) {
		skdebug(__func__, "fido_dev_info_manifest failed: %s",
		    fido_strerr(r));
		goto out;
	}
	if (olen != 1) {
		skdebug(__func__, "fido_dev_info_manifest bad len %zu", olen);
		goto out;
	}
	di = fido_dev_info_ptr(devlist, 0);
	if ((ret = strdup(fido_dev_info_path(di))) == NULL) {
		skdebug(__func__, "fido_dev_info_path failed");
		goto out;
	}
 out:
	fido_dev_info_free(&devlist, 1);
	return ret;
}

/* Check if the specified key handle exists on a given device. */
static int
try_device(fido_dev_t *dev, const uint8_t *message, size_t message_len,
    const char *application, const uint8_t *key_handle, size_t key_handle_len)
{
	fido_assert_t *assert = NULL;
	int r = FIDO_ERR_INTERNAL;

	if ((assert = fido_assert_new()) == NULL) {
		skdebug(__func__, "fido_assert_new failed");
		goto out;
	}
	if ((r = fido_assert_set_clientdata_hash(assert, message,
	    message_len)) != FIDO_OK) {
		skdebug(__func__, "fido_assert_set_clientdata_hash: %s",
		    fido_strerr(r));
		goto out;
	}
	if ((r = fido_assert_set_rp(assert, application)) != FIDO_OK) {
		skdebug(__func__, "fido_assert_set_rp: %s", fido_strerr(r));
		goto out;
	}
	if ((r = fido_assert_allow_cred(assert, key_handle,
	    key_handle_len)) != FIDO_OK) {
		skdebug(__func__, "fido_assert_allow_cred: %s", fido_strerr(r));
		goto out;
	}
	if ((r = fido_assert_set_up(assert, FIDO_OPT_FALSE)) != FIDO_OK) {
		skdebug(__func__, "fido_assert_up: %s", fido_strerr(r));
		goto out;
	}
	r = fido_dev_get_assert(dev, assert, NULL);
	skdebug(__func__, "fido_dev_get_assert: %s", fido_strerr(r));
	if (r == FIDO_ERR_USER_PRESENCE_REQUIRED) {
		/* U2F tokens may return this */
		r = FIDO_OK;
	}
 out:
	fido_assert_free(&assert);

	return r != FIDO_OK ? -1 : 0;
}

/* Iterate over configured devices looking for a specific key handle */
static fido_dev_t *
find_device(const char *path, const uint8_t *message, size_t message_len,
    const char *application, const uint8_t *key_handle, size_t key_handle_len)
{
	fido_dev_info_t *devlist = NULL;
	fido_dev_t *dev = NULL;
	size_t devlist_len = 0, i;
	int r;

	if (path != NULL) {
		if ((dev = fido_dev_new()) == NULL) {
			skdebug(__func__, "fido_dev_new failed");
			return NULL;
		}
		if ((r = fido_dev_open(dev, path)) != FIDO_OK) {
			skdebug(__func__, "fido_dev_open failed");
			fido_dev_free(&dev);
			return NULL;
		}
		return dev;
	}

	if ((devlist = fido_dev_info_new(MAX_FIDO_DEVICES)) == NULL) {
		skdebug(__func__, "fido_dev_info_new failed");
		goto out;
	}
	if ((r = fido_dev_info_manifest(devlist, MAX_FIDO_DEVICES,
	    &devlist_len)) != FIDO_OK) {
		skdebug(__func__, "fido_dev_info_manifest: %s", fido_strerr(r));
		goto out;
	}

	skdebug(__func__, "found %zu device(s)", devlist_len);

	for (i = 0; i < devlist_len; i++) {
		const fido_dev_info_t *di = fido_dev_info_ptr(devlist, i);

		if (di == NULL) {
			skdebug(__func__, "fido_dev_info_ptr %zu failed", i);
			continue;
		}
		if ((path = fido_dev_info_path(di)) == NULL) {
			skdebug(__func__, "fido_dev_info_path %zu failed", i);
			continue;
		}
		skdebug(__func__, "trying device %zu: %s", i, path);
		if ((dev = fido_dev_new()) == NULL) {
			skdebug(__func__, "fido_dev_new failed");
			continue;
		}
		if ((r = fido_dev_open(dev, path)) != FIDO_OK) {
			skdebug(__func__, "fido_dev_open failed");
			fido_dev_free(&dev);
			continue;
		}
		if (try_device(dev, message, message_len, application,
		    key_handle, key_handle_len) == 0) {
			skdebug(__func__, "found key");
			break;
		}
		fido_dev_close(dev);
		fido_dev_free(&dev);
	}

 out:
	if (devlist != NULL)
		fido_dev_info_free(&devlist, MAX_FIDO_DEVICES);

	return dev;
}

#ifdef WITH_OPENSSL
/*
 * The key returned via fido_cred_pubkey_ptr() is in affine coordinates,
 * but the API expects a SEC1 octet string.
 */
static int
pack_public_key_ecdsa(const fido_cred_t *cred,
    struct sk_enroll_response *response)
{
	const uint8_t *ptr;
	BIGNUM *x = NULL, *y = NULL;
	EC_POINT *q = NULL;
	EC_GROUP *g = NULL;
	int ret = -1;

	response->public_key = NULL;
	response->public_key_len = 0;

	if ((x = BN_new()) == NULL ||
	    (y = BN_new()) == NULL ||
	    (g = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1)) == NULL ||
	    (q = EC_POINT_new(g)) == NULL) {
		skdebug(__func__, "libcrypto setup failed");
		goto out;
	}
	if ((ptr = fido_cred_pubkey_ptr(cred)) == NULL) {
		skdebug(__func__, "fido_cred_pubkey_ptr failed");
		goto out;
	}
	if (fido_cred_pubkey_len(cred) != 64) {
		skdebug(__func__, "bad fido_cred_pubkey_len %zu",
		    fido_cred_pubkey_len(cred));
		goto out;
	}

	if (BN_bin2bn(ptr, 32, x) == NULL ||
	    BN_bin2bn(ptr + 32, 32, y) == NULL) {
		skdebug(__func__, "BN_bin2bn failed");
		goto out;
	}
	if (EC_POINT_set_affine_coordinates_GFp(g, q, x, y, NULL) != 1) {
		skdebug(__func__, "EC_POINT_set_affine_coordinates_GFp failed");
		goto out;
	}
	response->public_key_len = EC_POINT_point2oct(g, q,
	    POINT_CONVERSION_UNCOMPRESSED, NULL, 0, NULL);
	if (response->public_key_len == 0 || response->public_key_len > 2048) {
		skdebug(__func__, "bad pubkey length %zu",
		    response->public_key_len);
		goto out;
	}
	if ((response->public_key = malloc(response->public_key_len)) == NULL) {
		skdebug(__func__, "malloc pubkey failed");
		goto out;
	}
	if (EC_POINT_point2oct(g, q, POINT_CONVERSION_UNCOMPRESSED,
	    response->public_key, response->public_key_len, NULL) == 0) {
		skdebug(__func__, "EC_POINT_point2oct failed");
		goto out;
	}
	/* success */
	ret = 0;
 out:
	if (ret != 0 && response->public_key != NULL) {
		memset(response->public_key, 0, response->public_key_len);
		free(response->public_key);
		response->public_key = NULL;
	}
	EC_POINT_free(q);
	EC_GROUP_free(g);
	BN_clear_free(x);
	BN_clear_free(y);
	return ret;
}
#endif /* WITH_OPENSSL */

static int
pack_public_key_ed25519(const fido_cred_t *cred,
    struct sk_enroll_response *response)
{
	const uint8_t *ptr;
	size_t len;
	int ret = -1;

	response->public_key = NULL;
	response->public_key_len = 0;

	if ((len = fido_cred_pubkey_len(cred)) != 32) {
		skdebug(__func__, "bad fido_cred_pubkey_len len %zu", len);
		goto out;
	}
	if ((ptr = fido_cred_pubkey_ptr(cred)) == NULL) {
		skdebug(__func__, "fido_cred_pubkey_ptr failed");
		goto out;
	}
	response->public_key_len = len;
	if ((response->public_key = malloc(response->public_key_len)) == NULL) {
		skdebug(__func__, "malloc pubkey failed");
		goto out;
	}
	memcpy(response->public_key, ptr, len);
	ret = 0;
 out:
	if (ret != 0)
		free(response->public_key);
	return ret;
}

static int
pack_public_key(uint32_t alg, const fido_cred_t *cred,
    struct sk_enroll_response *response)
{
	switch(alg) {
#ifdef WITH_OPENSSL
	case SSH_SK_ECDSA:
		return pack_public_key_ecdsa(cred, response);
#endif /* WITH_OPENSSL */
	case SSH_SK_ED25519:
		return pack_public_key_ed25519(cred, response);
	default:
		return -1;
	}
}

static int
fidoerr_to_skerr(int fidoerr)
{
	switch (fidoerr) {
	case FIDO_ERR_UNSUPPORTED_OPTION:
	case FIDO_ERR_UNSUPPORTED_ALGORITHM:
		return SSH_SK_ERR_UNSUPPORTED;
	case FIDO_ERR_PIN_REQUIRED:
	case FIDO_ERR_PIN_INVALID:
		return SSH_SK_ERR_PIN_REQUIRED;
	default:
		return -1;
	}
}

static int
check_enroll_options(struct sk_option **options, char **devicep,
    uint8_t *user_id, size_t user_id_len)
{
	size_t i;

	if (options == NULL)
		return 0;
	for (i = 0; options[i] != NULL; i++) {
		if (strcmp(options[i]->name, "device") == 0) {
			if ((*devicep = strdup(options[i]->value)) == NULL) {
				skdebug(__func__, "strdup device failed");
				return -1;
			}
			skdebug(__func__, "requested device %s", *devicep);
		} else if (strcmp(options[i]->name, "user") == 0) {
			if (strlcpy(user_id, options[i]->value, user_id_len) >=
			    user_id_len) {
				skdebug(__func__, "user too long");
				return -1;
			}
			skdebug(__func__, "requested user %s",
			    (char *)user_id);
		} else {
			skdebug(__func__, "requested unsupported option %s",
			    options[i]->name);
			if (options[i]->required) {
				skdebug(__func__, "unknown required option");
				return -1;
			}
		}
	}
	return 0;
}

int
sk_enroll(uint32_t alg, const uint8_t *challenge, size_t challenge_len,
    const char *application, uint8_t flags, const char *pin,
    struct sk_option **options, struct sk_enroll_response **enroll_response)
{
	fido_cred_t *cred = NULL;
	fido_dev_t *dev = NULL;
	const uint8_t *ptr;
	uint8_t user_id[32];
	struct sk_enroll_response *response = NULL;
	size_t len;
	int cose_alg;
	int ret = SSH_SK_ERR_GENERAL;
	int r;
	char *device = NULL;

#ifdef SK_DEBUG
	fido_init(FIDO_DEBUG);
#endif
	if (enroll_response == NULL) {
		skdebug(__func__, "enroll_response == NULL");
		goto out;
	}
	memset(user_id, 0, sizeof(user_id));
	if (check_enroll_options(options, &device,
	    user_id, sizeof(user_id)) != 0)
		goto out; /* error already logged */

	*enroll_response = NULL;
	switch(alg) {
#ifdef WITH_OPENSSL
	case SSH_SK_ECDSA:
		cose_alg = COSE_ES256;
		break;
#endif /* WITH_OPENSSL */
	case SSH_SK_ED25519:
		cose_alg = COSE_EDDSA;
		break;
	default:
		skdebug(__func__, "unsupported key type %d", alg);
		goto out;
	}
	if (device == NULL && (device = pick_first_device()) == NULL) {
		ret = SSH_SK_ERR_DEVICE_NOT_FOUND;
		skdebug(__func__, "pick_first_device failed");
		goto out;
	}
	skdebug(__func__, "using device %s", device);
	if ((cred = fido_cred_new()) == NULL) {
		skdebug(__func__, "fido_cred_new failed");
		goto out;
	}
	if ((r = fido_cred_set_type(cred, cose_alg)) != FIDO_OK) {
		skdebug(__func__, "fido_cred_set_type: %s", fido_strerr(r));
		goto out;
	}
	if ((r = fido_cred_set_clientdata_hash(cred, challenge,
	    challenge_len)) != FIDO_OK) {
		skdebug(__func__, "fido_cred_set_clientdata_hash: %s",
		    fido_strerr(r));
		goto out;
	}
	if ((r = fido_cred_set_rk(cred, (flags & SSH_SK_RESIDENT_KEY) != 0 ?
	    FIDO_OPT_TRUE : FIDO_OPT_OMIT)) != FIDO_OK) {
		skdebug(__func__, "fido_cred_set_rk: %s", fido_strerr(r));
		goto out;
	}
	if ((r = fido_cred_set_user(cred, user_id, sizeof(user_id),
	    "openssh", "openssh", NULL)) != FIDO_OK) {
		skdebug(__func__, "fido_cred_set_user: %s", fido_strerr(r));
		goto out;
	}
	if ((r = fido_cred_set_rp(cred, application, NULL)) != FIDO_OK) {
		skdebug(__func__, "fido_cred_set_rp: %s", fido_strerr(r));
		goto out;
	}
	if ((dev = fido_dev_new()) == NULL) {
		skdebug(__func__, "fido_dev_new failed");
		goto out;
	}
	if ((r = fido_dev_open(dev, device)) != FIDO_OK) {
		skdebug(__func__, "fido_dev_open: %s", fido_strerr(r));
		goto out;
	}
	if ((r = fido_dev_make_cred(dev, cred, pin)) != FIDO_OK) {
		skdebug(__func__, "fido_dev_make_cred: %s", fido_strerr(r));
		ret = fidoerr_to_skerr(r);
		goto out;
	}
	if (fido_cred_x5c_ptr(cred) != NULL) {
		if ((r = fido_cred_verify(cred)) != FIDO_OK) {
			skdebug(__func__, "fido_cred_verify: %s",
			    fido_strerr(r));
			goto out;
		}
	} else {
		skdebug(__func__, "self-attested credential");
		if ((r = fido_cred_verify_self(cred)) != FIDO_OK) {
			skdebug(__func__, "fido_cred_verify_self: %s",
			    fido_strerr(r));
			goto out;
		}
	}
	if ((response = calloc(1, sizeof(*response))) == NULL) {
		skdebug(__func__, "calloc response failed");
		goto out;
	}
	if (pack_public_key(alg, cred, response) != 0) {
		skdebug(__func__, "pack_public_key failed");
		goto out;
	}
	if ((ptr = fido_cred_id_ptr(cred)) != NULL) {
		len = fido_cred_id_len(cred);
		if ((response->key_handle = calloc(1, len)) == NULL) {
			skdebug(__func__, "calloc key handle failed");
			goto out;
		}
		memcpy(response->key_handle, ptr, len);
		response->key_handle_len = len;
	}
	if ((ptr = fido_cred_sig_ptr(cred)) != NULL) {
		len = fido_cred_sig_len(cred);
		if ((response->signature = calloc(1, len)) == NULL) {
			skdebug(__func__, "calloc signature failed");
			goto out;
		}
		memcpy(response->signature, ptr, len);
		response->signature_len = len;
	}
	if ((ptr = fido_cred_x5c_ptr(cred)) != NULL) {
		len = fido_cred_x5c_len(cred);
		debug3("%s: attestation cert len=%zu", __func__, len);
		if ((response->attestation_cert = calloc(1, len)) == NULL) {
			skdebug(__func__, "calloc attestation cert failed");
			goto out;
		}
		memcpy(response->attestation_cert, ptr, len);
		response->attestation_cert_len = len;
	}
	*enroll_response = response;
	response = NULL;
	ret = 0;
 out:
	free(device);
	if (response != NULL) {
		free(response->public_key);
		free(response->key_handle);
		free(response->signature);
		free(response->attestation_cert);
		free(response);
	}
	if (dev != NULL) {
		fido_dev_close(dev);
		fido_dev_free(&dev);
	}
	if (cred != NULL) {
		fido_cred_free(&cred);
	}
	return ret;
}

#ifdef WITH_OPENSSL
static int
pack_sig_ecdsa(fido_assert_t *assert, struct sk_sign_response *response)
{
	ECDSA_SIG *sig = NULL;
	const BIGNUM *sig_r, *sig_s;
	const unsigned char *cp;
	size_t sig_len;
	int ret = -1;

	cp = fido_assert_sig_ptr(assert, 0);
	sig_len = fido_assert_sig_len(assert, 0);
	if ((sig = d2i_ECDSA_SIG(NULL, &cp, sig_len)) == NULL) {
		skdebug(__func__, "d2i_ECDSA_SIG failed");
		goto out;
	}
	ECDSA_SIG_get0(sig, &sig_r, &sig_s);
	response->sig_r_len = BN_num_bytes(sig_r);
	response->sig_s_len = BN_num_bytes(sig_s);
	if ((response->sig_r = calloc(1, response->sig_r_len)) == NULL ||
	    (response->sig_s = calloc(1, response->sig_s_len)) == NULL) {
		skdebug(__func__, "calloc signature failed");
		goto out;
	}
	BN_bn2bin(sig_r, response->sig_r);
	BN_bn2bin(sig_s, response->sig_s);
	ret = 0;
 out:
	ECDSA_SIG_free(sig);
	if (ret != 0) {
		free(response->sig_r);
		free(response->sig_s);
		response->sig_r = NULL;
		response->sig_s = NULL;
	}
	return ret;
}
#endif /* WITH_OPENSSL */

static int
pack_sig_ed25519(fido_assert_t *assert, struct sk_sign_response *response)
{
	const unsigned char *ptr;
	size_t len;
	int ret = -1;

	ptr = fido_assert_sig_ptr(assert, 0);
	len = fido_assert_sig_len(assert, 0);
	if (len != 64) {
		skdebug(__func__, "bad length %zu", len);
		goto out;
	}
	response->sig_r_len = len;
	if ((response->sig_r = calloc(1, response->sig_r_len)) == NULL) {
		skdebug(__func__, "calloc signature failed");
		goto out;
	}
	memcpy(response->sig_r, ptr, len);
	ret = 0;
 out:
	if (ret != 0) {
		free(response->sig_r);
		response->sig_r = NULL;
	}
	return ret;
}

static int
pack_sig(uint32_t  alg, fido_assert_t *assert,
    struct sk_sign_response *response)
{
	switch(alg) {
#ifdef WITH_OPENSSL
	case SSH_SK_ECDSA:
		return pack_sig_ecdsa(assert, response);
#endif /* WITH_OPENSSL */
	case SSH_SK_ED25519:
		return pack_sig_ed25519(assert, response);
	default:
		return -1;
	}
}

/* Checks sk_options for sk_sign() and sk_load_resident_keys() */
static int
check_sign_load_resident_options(struct sk_option **options, char **devicep)
{
	size_t i;

	if (options == NULL)
		return 0;
	for (i = 0; options[i] != NULL; i++) {
		if (strcmp(options[i]->name, "device") == 0) {
			if ((*devicep = strdup(options[i]->value)) == NULL) {
				skdebug(__func__, "strdup device failed");
				return -1;
			}
			skdebug(__func__, "requested device %s", *devicep);
		} else {
			skdebug(__func__, "requested unsupported option %s",
			    options[i]->name);
			if (options[i]->required) {
				skdebug(__func__, "unknown required option");
				return -1;
			}
		}
	}
	return 0;
}

/* Calculate SHA256(m) */
static int
sha256_mem(const void *m, size_t mlen, u_char *d, size_t dlen)
{
#ifdef WITH_OPENSSL
	u_int mdlen;
#endif

	if (dlen != 32)
		return -1;
#ifdef WITH_OPENSSL
	mdlen = dlen;
	if (!EVP_Digest(m, mlen, d, &mdlen, EVP_sha256(), NULL))
		return -1;
#else
	SHA256Data(m, mlen, d);
#endif
	return 0;
}

int
sk_sign(uint32_t alg, const uint8_t *data, size_t datalen,
    const char *application,
    const uint8_t *key_handle, size_t key_handle_len,
    uint8_t flags, const char *pin, struct sk_option **options,
    struct sk_sign_response **sign_response)
{
	fido_assert_t *assert = NULL;
	char *device = NULL;
	fido_dev_t *dev = NULL;
	struct sk_sign_response *response = NULL;
	uint8_t message[32];
	int ret = SSH_SK_ERR_GENERAL;
	int r;

#ifdef SK_DEBUG
	fido_init(FIDO_DEBUG);
#endif

	if (sign_response == NULL) {
		skdebug(__func__, "sign_response == NULL");
		goto out;
	}
	*sign_response = NULL;
	if (check_sign_load_resident_options(options, &device) != 0)
		goto out; /* error already logged */
	/* hash data to be signed before it goes to the security key */
	if ((r = sha256_mem(data, datalen, message, sizeof(message))) != 0) {
		skdebug(__func__, "hash message failed");
		goto out;
	}
	if ((dev = find_device(device, message, sizeof(message),
	    application, key_handle, key_handle_len)) == NULL) {
		skdebug(__func__, "couldn't find device for key handle");
		goto out;
	}
	if ((assert = fido_assert_new()) == NULL) {
		skdebug(__func__, "fido_assert_new failed");
		goto out;
	}
	if ((r = fido_assert_set_clientdata_hash(assert, message,
	    sizeof(message))) != FIDO_OK) {
		skdebug(__func__, "fido_assert_set_clientdata_hash: %s",
		    fido_strerr(r));
		goto out;
	}
	if ((r = fido_assert_set_rp(assert, application)) != FIDO_OK) {
		skdebug(__func__, "fido_assert_set_rp: %s", fido_strerr(r));
		goto out;
	}
	if ((r = fido_assert_allow_cred(assert, key_handle,
	    key_handle_len)) != FIDO_OK) {
		skdebug(__func__, "fido_assert_allow_cred: %s", fido_strerr(r));
		goto out;
	}
	if ((r = fido_assert_set_up(assert,
	    (flags & SSH_SK_USER_PRESENCE_REQD) ?
	    FIDO_OPT_TRUE : FIDO_OPT_FALSE)) != FIDO_OK) {
		skdebug(__func__, "fido_assert_set_up: %s", fido_strerr(r));
		goto out;
	}
	if ((r = fido_dev_get_assert(dev, assert, NULL)) != FIDO_OK) {
		skdebug(__func__, "fido_dev_get_assert: %s", fido_strerr(r));
		goto out;
	}
	if ((response = calloc(1, sizeof(*response))) == NULL) {
		skdebug(__func__, "calloc response failed");
		goto out;
	}
	response->flags = fido_assert_flags(assert, 0);
	response->counter = fido_assert_sigcount(assert, 0);
	if (pack_sig(alg, assert, response) != 0) {
		skdebug(__func__, "pack_sig failed");
		goto out;
	}
	*sign_response = response;
	response = NULL;
	ret = 0;
 out:
	explicit_bzero(message, sizeof(message));
	free(device);
	if (response != NULL) {
		free(response->sig_r);
		free(response->sig_s);
		free(response);
	}
	if (dev != NULL) {
		fido_dev_close(dev);
		fido_dev_free(&dev);
	}
	if (assert != NULL) {
		fido_assert_free(&assert);
	}
	return ret;
}

static int
read_rks(const char *devpath, const char *pin,
    struct sk_resident_key ***rksp, size_t *nrksp)
{
	int ret = SSH_SK_ERR_GENERAL, r = -1;
	fido_dev_t *dev = NULL;
	fido_credman_metadata_t *metadata = NULL;
	fido_credman_rp_t *rp = NULL;
	fido_credman_rk_t *rk = NULL;
	size_t i, j, nrp, nrk;
	const fido_cred_t *cred;
	struct sk_resident_key *srk = NULL, **tmp;

	if ((dev = fido_dev_new()) == NULL) {
		skdebug(__func__, "fido_dev_new failed");
		return ret;
	}
	if ((r = fido_dev_open(dev, devpath)) != FIDO_OK) {
		skdebug(__func__, "fido_dev_open %s failed: %s",
		    devpath, fido_strerr(r));
		fido_dev_free(&dev);
		return ret;
	}
	if ((metadata = fido_credman_metadata_new()) == NULL) {
		skdebug(__func__, "alloc failed");
		goto out;
	}

	if ((r = fido_credman_get_dev_metadata(dev, metadata, pin)) != 0) {
		if (r == FIDO_ERR_INVALID_COMMAND) {
			skdebug(__func__, "device %s does not support "
			    "resident keys", devpath);
			ret = 0;
			goto out;
		}
		skdebug(__func__, "get metadata for %s failed: %s",
		    devpath, fido_strerr(r));
		ret = fidoerr_to_skerr(r);
		goto out;
	}
	skdebug(__func__, "existing %llu, remaining %llu",
	    (unsigned long long)fido_credman_rk_existing(metadata),
	    (unsigned long long)fido_credman_rk_remaining(metadata));
	if ((rp = fido_credman_rp_new()) == NULL) {
		skdebug(__func__, "alloc rp failed");
		goto out;
	}
	if ((r = fido_credman_get_dev_rp(dev, rp, pin)) != 0) {
		skdebug(__func__, "get RPs for %s failed: %s",
		    devpath, fido_strerr(r));
		goto out;
	}
	nrp = fido_credman_rp_count(rp);
	skdebug(__func__, "Device %s has resident keys for %zu RPs",
	    devpath, nrp);

	/* Iterate over RP IDs that have resident keys */
	for (i = 0; i < nrp; i++) {
		skdebug(__func__, "rp %zu: name=\"%s\" id=\"%s\" hashlen=%zu",
		    i, fido_credman_rp_name(rp, i), fido_credman_rp_id(rp, i),
		    fido_credman_rp_id_hash_len(rp, i));

		/* Skip non-SSH RP IDs */
		if (strncasecmp(fido_credman_rp_id(rp, i), "ssh:", 4) != 0)
			continue;

		fido_credman_rk_free(&rk);
		if ((rk = fido_credman_rk_new()) == NULL) {
			skdebug(__func__, "alloc rk failed");
			goto out;
		}
		if ((r = fido_credman_get_dev_rk(dev, fido_credman_rp_id(rp, i),
		    rk, pin)) != 0) {
			skdebug(__func__, "get RKs for %s slot %zu failed: %s",
			    devpath, i, fido_strerr(r));
			goto out;
		}
		nrk = fido_credman_rk_count(rk);
		skdebug(__func__, "RP \"%s\" has %zu resident keys",
		    fido_credman_rp_id(rp, i), nrk);

		/* Iterate over resident keys for this RP ID */
		for (j = 0; j < nrk; j++) {
			if ((cred = fido_credman_rk(rk, j)) == NULL) {
				skdebug(__func__, "no RK in slot %zu", j);
				continue;
			}
			skdebug(__func__, "Device %s RP \"%s\" slot %zu: "
			    "type %d", devpath, fido_credman_rp_id(rp, i), j,
			    fido_cred_type(cred));

			/* build response entry */
			if ((srk = calloc(1, sizeof(*srk))) == NULL ||
			    (srk->key.key_handle = calloc(1,
			    fido_cred_id_len(cred))) == NULL ||
			    (srk->application = strdup(fido_credman_rp_id(rp,
			    i))) == NULL) {
				skdebug(__func__, "alloc sk_resident_key");
				goto out;
			}

			srk->key.key_handle_len = fido_cred_id_len(cred);
			memcpy(srk->key.key_handle,
			    fido_cred_id_ptr(cred),
			    srk->key.key_handle_len);

			switch (fido_cred_type(cred)) {
			case COSE_ES256:
				srk->alg = SSH_SK_ECDSA;
				break;
			case COSE_EDDSA:
				srk->alg = SSH_SK_ED25519;
				break;
			default:
				skdebug(__func__, "unsupported key type %d",
				    fido_cred_type(cred));
				goto out; /* XXX free rk and continue */
			}

			if ((r = pack_public_key(srk->alg, cred,
			    &srk->key)) != 0) {
				skdebug(__func__, "pack public key failed");
				goto out;
			}
			/* append */
			if ((tmp = recallocarray(*rksp, *nrksp, (*nrksp) + 1,
			    sizeof(**rksp))) == NULL) {
				skdebug(__func__, "alloc rksp");
				goto out;
			}
			*rksp = tmp;
			(*rksp)[(*nrksp)++] = srk;
			srk = NULL;
		}
	}
	/* Success */
	ret = 0;
 out:
	if (srk != NULL) {
		free(srk->application);
		freezero(srk->key.public_key, srk->key.public_key_len);
		freezero(srk->key.key_handle, srk->key.key_handle_len);
		freezero(srk, sizeof(*srk));
	}
	fido_credman_rp_free(&rp);
	fido_credman_rk_free(&rk);
	fido_dev_close(dev);
	fido_dev_free(&dev);
	fido_credman_metadata_free(&metadata);
	return ret;
}

int
sk_load_resident_keys(const char *pin, struct sk_option **options,
    struct sk_resident_key ***rksp, size_t *nrksp)
{
	int ret = SSH_SK_ERR_GENERAL, r = -1;
	fido_dev_info_t *devlist = NULL;
	size_t i, ndev = 0, nrks = 0;
	const fido_dev_info_t *di;
	struct sk_resident_key **rks = NULL;
	char *device = NULL;
	*rksp = NULL;
	*nrksp = 0;

	if (check_sign_load_resident_options(options, &device) != 0)
		goto out; /* error already logged */
	if (device != NULL) {
		skdebug(__func__, "trying %s", device);
		if ((r = read_rks(device, pin, &rks, &nrks)) != 0) {
			skdebug(__func__, "read_rks failed for %s", device);
			ret = r;
			goto out;
		}
	} else {
		/* Try all devices */
		if ((devlist = fido_dev_info_new(MAX_FIDO_DEVICES)) == NULL) {
			skdebug(__func__, "fido_dev_info_new failed");
			goto out;
		}
		if ((r = fido_dev_info_manifest(devlist,
		    MAX_FIDO_DEVICES, &ndev)) != FIDO_OK) {
			skdebug(__func__, "fido_dev_info_manifest failed: %s",
			    fido_strerr(r));
			goto out;
		}
		for (i = 0; i < ndev; i++) {
			if ((di = fido_dev_info_ptr(devlist, i)) == NULL) {
				skdebug(__func__, "no dev info at %zu", i);
				continue;
			}
			skdebug(__func__, "trying %s", fido_dev_info_path(di));
			if ((r = read_rks(fido_dev_info_path(di), pin,
			    &rks, &nrks)) != 0) {
				skdebug(__func__, "read_rks failed for %s",
				    fido_dev_info_path(di));
				/* remember last error */
				ret = r;
				continue;
			}
		}
	}
	/* success, unless we have no keys but a specific error */
	if (nrks > 0 || ret == SSH_SK_ERR_GENERAL)
		ret = 0;
	*rksp = rks;
	*nrksp = nrks;
	rks = NULL;
	nrks = 0;
 out:
	free(device);
	for (i = 0; i < nrks; i++) {
		free(rks[i]->application);
		freezero(rks[i]->key.public_key, rks[i]->key.public_key_len);
		freezero(rks[i]->key.key_handle, rks[i]->key.key_handle_len);
		freezero(rks[i], sizeof(*rks[i]));
	}
	free(rks);
	fido_dev_info_free(&devlist, MAX_FIDO_DEVICES);
	return ret;
}

#endif /* ENABLE_SK_INTERNAL */
