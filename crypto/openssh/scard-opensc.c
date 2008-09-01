/*
 * Copyright (c) 2002 Juha Yrjölä.  All rights reserved.
 * Copyright (c) 2001 Markus Friedl.
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
#if defined(SMARTCARD) && defined(USE_OPENSC)

#include <sys/types.h>

#include <openssl/evp.h>
#include <openssl/x509.h>

#include <stdarg.h>
#include <string.h>

#include <opensc/opensc.h>
#include <opensc/pkcs15.h>

#include "key.h"
#include "log.h"
#include "xmalloc.h"
#include "misc.h"
#include "scard.h"

#if OPENSSL_VERSION_NUMBER < 0x00907000L && defined(CRYPTO_LOCK_ENGINE)
#define USE_ENGINE
#define RSA_get_default_method RSA_get_default_openssl_method
#else
#endif

#ifdef USE_ENGINE
#include <openssl/engine.h>
#define sc_get_rsa sc_get_engine
#else
#define sc_get_rsa sc_get_rsa_method
#endif

static int sc_reader_id;
static sc_context_t *ctx = NULL;
static sc_card_t *card = NULL;
static sc_pkcs15_card_t *p15card = NULL;

static char *sc_pin = NULL;

struct sc_priv_data
{
	struct sc_pkcs15_id cert_id;
	int ref_count;
};

void
sc_close(void)
{
	if (p15card) {
		sc_pkcs15_unbind(p15card);
		p15card = NULL;
	}
	if (card) {
		sc_disconnect_card(card, 0);
		card = NULL;
	}
	if (ctx) {
		sc_release_context(ctx);
		ctx = NULL;
	}
}

static int
sc_init(void)
{
	int r;

	r = sc_establish_context(&ctx, "openssh");
	if (r)
		goto err;
	if (sc_reader_id >= ctx->reader_count) {
		r = SC_ERROR_NO_READERS_FOUND;
		error("Illegal reader number %d (max %d)", sc_reader_id,
		    ctx->reader_count -1);
		goto err;
	}
	r = sc_connect_card(ctx->reader[sc_reader_id], 0, &card);
	if (r)
		goto err;
	r = sc_pkcs15_bind(card, &p15card);
	if (r)
		goto err;
	return 0;
err:
	sc_close();
	return r;
}

/* private key operations */

static int
sc_prkey_op_init(RSA *rsa, struct sc_pkcs15_object **key_obj_out,
	unsigned int usage)
{
	int r;
	struct sc_priv_data *priv;
	struct sc_pkcs15_object *key_obj;
	struct sc_pkcs15_prkey_info *key;
	struct sc_pkcs15_object *pin_obj;
	struct sc_pkcs15_pin_info *pin;

	priv = (struct sc_priv_data *) RSA_get_app_data(rsa);
	if (priv == NULL)
		return -1;
	if (p15card == NULL) {
		sc_close();
		r = sc_init();
		if (r) {
			error("SmartCard init failed: %s", sc_strerror(r));
			goto err;
		}
	}
	r = sc_pkcs15_find_prkey_by_id_usage(p15card, &priv->cert_id,
		usage, &key_obj);
	if (r) {
		error("Unable to find private key from SmartCard: %s",
		      sc_strerror(r));
		goto err;
	}
	key = key_obj->data;
	r = sc_pkcs15_find_pin_by_auth_id(p15card, &key_obj->auth_id,
					  &pin_obj);
	if (r == SC_ERROR_OBJECT_NOT_FOUND) {
		/* no pin required */
		r = sc_lock(card);
		if (r) {
			error("Unable to lock smartcard: %s", sc_strerror(r));
			goto err;
		}
		*key_obj_out = key_obj;
		return 0;
	} else if (r) {
		error("Unable to find PIN object from SmartCard: %s",
		      sc_strerror(r));
		goto err;
	}
	pin = pin_obj->data;
	r = sc_lock(card);
	if (r) {
		error("Unable to lock smartcard: %s", sc_strerror(r));
		goto err;
	}
	if (sc_pin != NULL) {
		r = sc_pkcs15_verify_pin(p15card, pin, sc_pin,
					 strlen(sc_pin));
		if (r) {
			sc_unlock(card);
			error("PIN code verification failed: %s",
			      sc_strerror(r));
			goto err;
		}
	}
	*key_obj_out = key_obj;
	return 0;
err:
	sc_close();
	return -1;
}

#define SC_USAGE_DECRYPT	SC_PKCS15_PRKEY_USAGE_DECRYPT | \
				SC_PKCS15_PRKEY_USAGE_UNWRAP

static int
sc_private_decrypt(int flen, u_char *from, u_char *to, RSA *rsa,
    int padding)
{
	struct sc_pkcs15_object *key_obj;
	int r;

	if (padding != RSA_PKCS1_PADDING)
		return -1;
	r = sc_prkey_op_init(rsa, &key_obj, SC_USAGE_DECRYPT);
	if (r)
		return -1;
	r = sc_pkcs15_decipher(p15card, key_obj, SC_ALGORITHM_RSA_PAD_PKCS1,
	    from, flen, to, flen);
	sc_unlock(card);
	if (r < 0) {
		error("sc_pkcs15_decipher() failed: %s", sc_strerror(r));
		goto err;
	}
	return r;
err:
	sc_close();
	return -1;
}

#define SC_USAGE_SIGN 		SC_PKCS15_PRKEY_USAGE_SIGN | \
				SC_PKCS15_PRKEY_USAGE_SIGNRECOVER

static int
sc_sign(int type, u_char *m, unsigned int m_len,
	unsigned char *sigret, unsigned int *siglen, RSA *rsa)
{
	struct sc_pkcs15_object *key_obj;
	int r;
	unsigned long flags = 0;

	/* XXX: sc_prkey_op_init will search for a pkcs15 private
	 * key object with the sign or signrecover usage flag set.
	 * If the signing key has only the non-repudiation flag set
	 * the key will be rejected as using a non-repudiation key
	 * for authentication is not recommended. Note: This does not
	 * prevent the use of a non-repudiation key for authentication
	 * if the sign or signrecover flag is set as well.
	 */
	r = sc_prkey_op_init(rsa, &key_obj, SC_USAGE_SIGN);
	if (r)
		return -1;
	/* FIXME: length of sigret correct? */
	/* FIXME: check 'type' and modify flags accordingly */
	flags = SC_ALGORITHM_RSA_PAD_PKCS1 | SC_ALGORITHM_RSA_HASH_SHA1;
	r = sc_pkcs15_compute_signature(p15card, key_obj, flags,
					m, m_len, sigret, RSA_size(rsa));
	sc_unlock(card);
	if (r < 0) {
		error("sc_pkcs15_compute_signature() failed: %s",
		      sc_strerror(r));
		goto err;
	}
	*siglen = r;
	return 1;
err:
	sc_close();
	return 0;
}

static int
sc_private_encrypt(int flen, u_char *from, u_char *to, RSA *rsa,
    int padding)
{
	error("Private key encryption not supported");
	return -1;
}

/* called on free */

static int (*orig_finish)(RSA *rsa) = NULL;

static int
sc_finish(RSA *rsa)
{
	struct sc_priv_data *priv;

	priv = RSA_get_app_data(rsa);
	priv->ref_count--;
	if (priv->ref_count == 0) {
		free(priv);
		sc_close();
	}
	if (orig_finish)
		orig_finish(rsa);
	return 1;
}

/* engine for overloading private key operations */

static RSA_METHOD *
sc_get_rsa_method(void)
{
	static RSA_METHOD smart_rsa;
	const RSA_METHOD *def = RSA_get_default_method();

	/* use the OpenSSL version */
	memcpy(&smart_rsa, def, sizeof(smart_rsa));

	smart_rsa.name		= "opensc";

	/* overload */
	smart_rsa.rsa_priv_enc	= sc_private_encrypt;
	smart_rsa.rsa_priv_dec	= sc_private_decrypt;
	smart_rsa.rsa_sign	= sc_sign;

	/* save original */
	orig_finish		= def->finish;
	smart_rsa.finish	= sc_finish;

	return &smart_rsa;
}

#ifdef USE_ENGINE
static ENGINE *
sc_get_engine(void)
{
	static ENGINE *smart_engine = NULL;

	if ((smart_engine = ENGINE_new()) == NULL)
		fatal("ENGINE_new failed");

	ENGINE_set_id(smart_engine, "opensc");
	ENGINE_set_name(smart_engine, "OpenSC");

	ENGINE_set_RSA(smart_engine, sc_get_rsa_method());
	ENGINE_set_DSA(smart_engine, DSA_get_default_openssl_method());
	ENGINE_set_DH(smart_engine, DH_get_default_openssl_method());
	ENGINE_set_RAND(smart_engine, RAND_SSLeay());
	ENGINE_set_BN_mod_exp(smart_engine, BN_mod_exp);

	return smart_engine;
}
#endif

static void
convert_rsa_to_rsa1(Key * in, Key * out)
{
	struct sc_priv_data *priv;

	out->rsa->flags = in->rsa->flags;
	out->flags = in->flags;
	RSA_set_method(out->rsa, RSA_get_method(in->rsa));
	BN_copy(out->rsa->n, in->rsa->n);
	BN_copy(out->rsa->e, in->rsa->e);
	priv = RSA_get_app_data(in->rsa);
	priv->ref_count++;
	RSA_set_app_data(out->rsa, priv);
	return;
}

static int
sc_read_pubkey(Key * k, const struct sc_pkcs15_object *cert_obj)
{
	int r;
	sc_pkcs15_cert_t *cert = NULL;
	struct sc_priv_data *priv = NULL;
	sc_pkcs15_cert_info_t *cinfo = cert_obj->data;

	X509 *x509 = NULL;
	EVP_PKEY *pubkey = NULL;
	u8 *p;
	char *tmp;

	debug("sc_read_pubkey() with cert id %02X", cinfo->id.value[0]);
	r = sc_pkcs15_read_certificate(p15card, cinfo, &cert);
	if (r) {
		logit("Certificate read failed: %s", sc_strerror(r));
		goto err;
	}
	x509 = X509_new();
	if (x509 == NULL) {
		r = -1;
		goto err;
	}
	p = cert->data;
	if (!d2i_X509(&x509, &p, cert->data_len)) {
		logit("Unable to parse X.509 certificate");
		r = -1;
		goto err;
	}
	sc_pkcs15_free_certificate(cert);
	cert = NULL;
	pubkey = X509_get_pubkey(x509);
	X509_free(x509);
	x509 = NULL;
	if (pubkey->type != EVP_PKEY_RSA) {
		logit("Public key is of unknown type");
		r = -1;
		goto err;
	}
	k->rsa = EVP_PKEY_get1_RSA(pubkey);
	EVP_PKEY_free(pubkey);

	k->rsa->flags |= RSA_FLAG_SIGN_VER;
	RSA_set_method(k->rsa, sc_get_rsa_method());
	priv = xmalloc(sizeof(struct sc_priv_data));
	priv->cert_id = cinfo->id;
	priv->ref_count = 1;
	RSA_set_app_data(k->rsa, priv);

	k->flags = KEY_FLAG_EXT;
	tmp = key_fingerprint(k, SSH_FP_MD5, SSH_FP_HEX);
	debug("fingerprint %d %s", key_size(k), tmp);
	xfree(tmp);

	return 0;
err:
	if (cert)
		sc_pkcs15_free_certificate(cert);
	if (pubkey)
		EVP_PKEY_free(pubkey);
	if (x509)
		X509_free(x509);
	return r;
}

Key **
sc_get_keys(const char *id, const char *pin)
{
	Key *k, **keys;
	int i, r, real_count = 0, key_count;
	sc_pkcs15_id_t cert_id;
	sc_pkcs15_object_t *certs[32];
	char *buf = xstrdup(id), *p;

	debug("sc_get_keys called: id = %s", id);

	if (sc_pin != NULL)
		xfree(sc_pin);
	sc_pin = (pin == NULL) ? NULL : xstrdup(pin);

	cert_id.len = 0;
	if ((p = strchr(buf, ':')) != NULL) {
		*p = 0;
		p++;
		sc_pkcs15_hex_string_to_id(p, &cert_id);
	}
	r = sscanf(buf, "%d", &sc_reader_id);
	xfree(buf);
	if (r != 1)
		goto err;
	if (p15card == NULL) {
		sc_close();
		r = sc_init();
		if (r) {
			error("Smartcard init failed: %s", sc_strerror(r));
			goto err;
		}
	}
	if (cert_id.len) {
		r = sc_pkcs15_find_cert_by_id(p15card, &cert_id, &certs[0]);
		if (r < 0)
			goto err;
		key_count = 1;
	} else {
		r = sc_pkcs15_get_objects(p15card, SC_PKCS15_TYPE_CERT_X509,
					  certs, 32);
		if (r == 0) {
			logit("No certificates found on smartcard");
			r = -1;
			goto err;
		} else if (r < 0) {
			error("Certificate enumeration failed: %s",
			      sc_strerror(r));
			goto err;
		}
		key_count = r;
	}
	if (key_count > 1024)
		fatal("Too many keys (%u), expected <= 1024", key_count);
	keys = xcalloc(key_count * 2 + 1, sizeof(Key *));
	for (i = 0; i < key_count; i++) {
		sc_pkcs15_object_t *tmp_obj = NULL;
		cert_id = ((sc_pkcs15_cert_info_t *)(certs[i]->data))->id;
		if (sc_pkcs15_find_prkey_by_id(p15card, &cert_id, &tmp_obj))
			/* skip the public key (certificate) if no
			 * corresponding private key is present */
			continue;
		k = key_new(KEY_RSA);
		if (k == NULL)
			break;
		r = sc_read_pubkey(k, certs[i]);
		if (r) {
			error("sc_read_pubkey failed: %s", sc_strerror(r));
			key_free(k);
			continue;
		}
		keys[real_count] = k;
		real_count++;
		k = key_new(KEY_RSA1);
		if (k == NULL)
			break;
		convert_rsa_to_rsa1(keys[real_count-1], k);
		keys[real_count] = k;
		real_count++;
	}
	keys[real_count] = NULL;

	return keys;
err:
	sc_close();
	return NULL;
}

int
sc_put_key(Key *prv, const char *id)
{
	error("key uploading not yet supported");
	return -1;
}

char *
sc_get_key_label(Key *key)
{
	int r;
	const struct sc_priv_data *priv;
	struct sc_pkcs15_object *key_obj;

	priv = (const struct sc_priv_data *) RSA_get_app_data(key->rsa);
	if (priv == NULL || p15card == NULL) {
		logit("SmartCard key not loaded");
		/* internal error => return default label */
		return xstrdup("smartcard key");
	}
	r = sc_pkcs15_find_prkey_by_id(p15card, &priv->cert_id, &key_obj);
	if (r) {
		logit("Unable to find private key from SmartCard: %s",
		      sc_strerror(r));
		return xstrdup("smartcard key");
	}
	if (key_obj == NULL || key_obj->label == NULL)
		/* the optional PKCS#15 label does not exists
		 * => return the default label */
		return xstrdup("smartcard key");
	return xstrdup(key_obj->label);
}

#endif /* SMARTCARD */
