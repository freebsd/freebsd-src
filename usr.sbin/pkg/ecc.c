/*-
 * Copyright (c) 2011-2013 Baptiste Daroussin <bapt@FreeBSD.org>
 * Copyright (c) 2011-2012 Julien Laffaye <jlaffaye@FreeBSD.org>
 * All rights reserved.
 * Copyright (c) 2021 Kyle Evans <kevans@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/stat.h>
#include <sys/param.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <libder.h>

#define	WITH_STDLIB
#include <libecc/libsig.h>
#undef WITH_STDLIB

#include "pkg.h"
#include "hash.h"

/* libpkg shim */
#define	STREQ(l, r)	(strcmp(l, r) == 0)

struct ecc_sign_ctx {
	struct pkgsign_ctx	sctx;
	ec_params		params;
	ec_key_pair		keypair;
	ec_alg_type		sig_alg;
	hash_alg_type		sig_hash;
	bool			loaded;
};

/* Grab the ossl context from a pkgsign_ctx. */
#define	ECC_CCTX(c)	(__containerof(c, const struct ecc_sign_ctx, sctx))
#define	ECC_CTX(c)	(__containerof(c, struct ecc_sign_ctx, sctx))

#define PUBKEY_UNCOMPRESSED	0x04

#ifndef MAX
#define	MAX(a,b)	(((a)>(b))?(a):(b))
#endif

static const uint8_t oid_ecpubkey[] = \
    { 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01 };

static const uint8_t oid_secp[] = \
    { 0x2b, 0x81, 0x04, 0x00 };
static const uint8_t oid_secp256k1[] = \
    { 0x2b, 0x81, 0x04, 0x00, 0x0a };
static const uint8_t oid_brainpoolP[] = \
    { 0x2b, 0x24, 0x03, 0x03, 0x02, 0x08, 0x01, 0x01 };

#define	ENTRY(name, params)	{ #name, sizeof(#name) - 1, params }
static const struct pkgkey_map_entry {
	const char		*name;
	size_t			 namesz;
	const ec_str_params	*params;
} pkgkey_map[] = {
	ENTRY(WEI25519, &wei25519_str_params),
	ENTRY(SECP256K1, &secp256k1_str_params),
	ENTRY(SECP384R1, &secp384r1_str_params),
	ENTRY(SECP512R1, &secp521r1_str_params),
	ENTRY(BRAINPOOLP256R1, &brainpoolp256r1_str_params),
	ENTRY(BRAINPOOLP256T1, &brainpoolp256t1_str_params),
	ENTRY(BRAINPOOLP320R1, &brainpoolp320r1_str_params),
	ENTRY(BRAINPOOLP320T1, &brainpoolp320t1_str_params),
	ENTRY(BRAINPOOLP384R1, &brainpoolp384r1_str_params),
	ENTRY(BRAINPOOLP384T1, &brainpoolp384t1_str_params),
	ENTRY(BRAINPOOLP512R1, &brainpoolp512r1_str_params),
	ENTRY(BRAINPOOLP512T1, &brainpoolp512t1_str_params),
};

static const char pkgkey_app[] = "pkg";
static const char pkgkey_signer[] = "ecc";

static const ec_str_params *
ecc_pkgkey_params(const uint8_t *curve, size_t curvesz)
{
	const struct pkgkey_map_entry *entry;

	for (size_t i = 0; i < nitems(pkgkey_map); i++) {
		entry = &pkgkey_map[i];
		if (curvesz != entry->namesz)
			continue;
		if (memcmp(curve, entry->name, curvesz) == 0)
			return (entry->params);
	}

	return (NULL);
}

static int
ecc_read_pkgkey(struct libder_object *root, ec_params *params, int public,
    uint8_t *rawkey, size_t *rawlen)
{
	struct libder_object *obj;
	const uint8_t *data;
	const ec_str_params *sparams;
	size_t datasz;
	int ret;

	if (libder_obj_type_simple(root) != BT_SEQUENCE)
		return (1);

	/* Application */
	obj = libder_obj_child(root, 0);
	if (obj == NULL || libder_obj_type_simple(obj) != BT_UTF8STRING)
		return (1);
	data = libder_obj_data(obj, &datasz);
	if (datasz != sizeof(pkgkey_app) - 1 ||
	    memcmp(data, pkgkey_app, datasz) != 0)
		return (1);

	/* Version */
	obj = libder_obj_child(root, 1);
	if (obj == NULL || libder_obj_type_simple(obj) != BT_INTEGER)
		return (1);
	data = libder_obj_data(obj, &datasz);
	if (datasz != 1 || *data != 1 /* XXX */)
		return (1);

	/* Signer */
	obj = libder_obj_child(root, 2);
	if (obj == NULL || libder_obj_type_simple(obj) != BT_UTF8STRING)
		return (1);
	data = libder_obj_data(obj, &datasz);
	if (datasz != sizeof(pkgkey_signer) - 1 ||
	    memcmp(data, pkgkey_signer, datasz) != 0)
		return (1);

	/* KeyType (curve) */
	obj = libder_obj_child(root, 3);
	if (obj == NULL || libder_obj_type_simple(obj) != BT_UTF8STRING)
		return (1);
	data = libder_obj_data(obj, &datasz);
	sparams = ecc_pkgkey_params(data, datasz);
	if (sparams == NULL)
		return (1);

	ret = import_params(params, sparams);
	if (ret != 0)
		return (1);

	/* Public? */
	obj = libder_obj_child(root, 4);
	if (obj == NULL || libder_obj_type_simple(obj) != BT_BOOLEAN)
		return (1);
	data = libder_obj_data(obj, &datasz);
	if (datasz != 1 || !data[0] != !public)
		return (1);

	/* Key */
	obj = libder_obj_child(root, 5);
	if (obj == NULL || libder_obj_type_simple(obj) != BT_BITSTRING)
		return (1);
	data = libder_obj_data(obj, &datasz);
	if (datasz <= 2 || data[0] != 0 || data[1] != PUBKEY_UNCOMPRESSED)
		return (1);

	data += 2;
	datasz -= 2;

	if (datasz > *rawlen)
		return (1);


	memcpy(rawkey, data, datasz);
	*rawlen = datasz;

	return (0);
}

static int
ecc_extract_signature(const uint8_t *sig, size_t siglen, uint8_t *rawsig,
    size_t rawlen)
{
	struct libder_ctx *ctx;
	struct libder_object *obj, *root;
	const uint8_t *sigdata;
	size_t compsz, datasz, sigoff;
	int rc;

	ctx = libder_open();
	if (ctx == NULL)
		return (1);

	rc = 1;
	root = libder_read(ctx, sig, &siglen);
	if (root == NULL || libder_obj_type_simple(root) != BT_SEQUENCE)
		goto out;

	/* Descend into the sequence's payload, extract both numbers. */
	compsz = rawlen / 2;
	sigoff = 0;
	for (int i = 0; i < 2; i++) {
		obj = libder_obj_child(root, i);
		if (libder_obj_type_simple(obj) != BT_INTEGER)
			goto out;

		sigdata = libder_obj_data(obj, &datasz);
		if (datasz < 2 || datasz > compsz + 1)
			goto out;

		/*
		 * We may see an extra lead byte if our high bit of the first
		 * byte was set, since these numbers are positive by definition.
		 */
		if (sigdata[0] == 0 && (sigdata[1] & 0x80) != 0) {
			sigdata++;
			datasz--;
		}

		/* Sanity check: don't overflow the output. */
		if (sigoff + datasz > rawlen)
			goto out;

		/* Padding to the significant end if we're too small. */
		if (datasz < compsz) {
			memset(&rawsig[sigoff], 0, compsz - datasz);
			sigoff += compsz - datasz;
		}

		memcpy(&rawsig[sigoff], sigdata, datasz);
		sigoff += datasz;
	}

	/* Sanity check: must have exactly the required # of signature bits. */
	rc = (sigoff == rawlen) ? 0 : 1;

out:
	libder_obj_free(root);
	libder_close(ctx);
	return (rc);
}

static int
ecc_extract_pubkey_string(const uint8_t *data, size_t datalen, uint8_t *rawkey,
    size_t *rawlen)
{
	uint8_t prefix, usebit;

	if (datalen <= 2)
		return (1);

	usebit = *data++;
	datalen--;

	if (usebit != 0)
		return (1);

	prefix = *data++;
	datalen--;

	if (prefix != PUBKEY_UNCOMPRESSED)
		return (1);

	if (datalen > *rawlen)
		return (1);

	memcpy(rawkey, data, datalen);
	*rawlen = datalen;

	return (0);
}

static int
ecc_extract_key_params(const uint8_t *oid, size_t oidlen,
    ec_params *rawparams)
{
	int ret;

	if (oidlen >= sizeof(oid_secp) &&
	    memcmp(oid, oid_secp, sizeof(oid_secp)) >= 0) {
		oid += sizeof(oid_secp);
		oidlen -= sizeof(oid_secp);

		if (oidlen != 1)
			return (1);

		ret = -1;
		switch (*oid) {
		case 0x0a:	/* secp256k1 */
			ret = import_params(rawparams, &secp256k1_str_params);
			break;
		case 0x22:	/* secp384r1 */
			ret = import_params(rawparams, &secp384r1_str_params);
			break;
		case 0x23:	/* secp521r1 */
			ret = import_params(rawparams, &secp521r1_str_params);
			break;
		default:
			return (1);
		}

		if (ret == 0)
			return (0);
		return (1);
	}

	if (oidlen >= sizeof(oid_brainpoolP) &&
	    memcmp(oid, oid_brainpoolP, sizeof(oid_brainpoolP)) >= 0) {
		oid += sizeof(oid_brainpoolP);
		oidlen -= sizeof(oid_brainpoolP);

		if (oidlen != 1)
			return (1);

		ret = -1;
		switch (*oid) {
		case 0x07:	/* brainpoolP256r1 */
			ret = import_params(rawparams, &brainpoolp256r1_str_params);
			break;
		case 0x08:	/* brainpoolP256t1 */
			ret = import_params(rawparams, &brainpoolp256t1_str_params);
			break;
		case 0x09:	/* brainpoolP320r1 */
			ret = import_params(rawparams, &brainpoolp320r1_str_params);
			break;
		case 0x0a:	/* brainpoolP320t1 */
			ret = import_params(rawparams, &brainpoolp320t1_str_params);
			break;
		case 0x0b:	/* brainpoolP384r1 */
			ret = import_params(rawparams, &brainpoolp384r1_str_params);
			break;
		case 0x0c:	/* brainpoolP384t1 */
			ret = import_params(rawparams, &brainpoolp384t1_str_params);
			break;
		case 0x0d:	/* brainpoolP512r1 */
			ret = import_params(rawparams, &brainpoolp512r1_str_params);
			break;
		case 0x0e:	/* brainpoolP512t1 */
			ret = import_params(rawparams, &brainpoolp512t1_str_params);
			break;
		default:
			return (1);
		}

		if (ret == 0)
			return (0);
		return (1);
	}

#ifdef ECC_DEBUG
	for (size_t i = 0; i < oidlen; i++) {
		fprintf(stderr, "%.02x ", oid[i]);
	}

	fprintf(stderr, "\n");
#endif

	return (1);
}

/*
 * On entry, *rawparams should point to an ec_params that we can import the
 * key parameters to.  We'll either do that, or we'll set it to NULL if we could
 * not deduce the curve.
 */
static int
ecc_extract_pubkey(FILE *keyfp, const uint8_t *key, size_t keylen,
    uint8_t *rawkey, size_t *rawlen, ec_params *rawparams)
{
	const uint8_t *oidp;
	struct libder_ctx *ctx;
	struct libder_object *keydata, *oid, *params, *root;
	size_t oidsz;
	int rc;

	ctx = libder_open();
	if (ctx == NULL)
		return (1);

	rc = 1;
	assert((keyfp != NULL) ^ (key != NULL));
	if (keyfp != NULL) {
		root = libder_read_file(ctx, keyfp, &keylen);
	} else {
		root = libder_read(ctx, key, &keylen);
	}

	if (root == NULL || libder_obj_type_simple(root) != BT_SEQUENCE)
		goto out;

	params = libder_obj_child(root, 0);

	if (params == NULL) {
		goto out;
	} else if (libder_obj_type_simple(params) != BT_SEQUENCE) {
		rc = ecc_read_pkgkey(root, rawparams, 1, rawkey, rawlen);
		goto out;
	}

	/* Is a sequence */
	keydata = libder_obj_child(root, 1);
	if (keydata == NULL || libder_obj_type_simple(keydata) != BT_BITSTRING)
		goto out;

	/* Key type */
	oid = libder_obj_child(params, 0);
	if (oid == NULL || libder_obj_type_simple(oid) != BT_OID)
		goto out;

	oidp = libder_obj_data(oid, &oidsz);
	if (oidsz != sizeof(oid_ecpubkey) ||
	    memcmp(oidp, oid_ecpubkey, oidsz) != 0)
		return (1);

	/* Curve */
	oid = libder_obj_child(params, 1);
	if (oid == NULL || libder_obj_type_simple(oid) != BT_OID)
		goto out;

	oidp = libder_obj_data(oid, &oidsz);
	if (ecc_extract_key_params(oidp, oidsz, rawparams) != 0)
		goto out;

	/* Finally, peel off the key material */
	key = libder_obj_data(keydata, &keylen);
	if (ecc_extract_pubkey_string(key, keylen, rawkey, rawlen) != 0)
		goto out;

	rc = 0;
out:
	libder_obj_free(root);
	libder_close(ctx);
	return (rc);
}

struct ecc_verify_cbdata {
	const struct pkgsign_ctx *sctx;
	FILE *keyfp;
	const unsigned char *key;
	size_t keylen;
	unsigned char *sig;
	size_t siglen;
};

static int
ecc_verify_internal(struct ecc_verify_cbdata *cbdata, const uint8_t *hash,
    size_t hashsz)
{
	ec_pub_key pubkey;
	ec_params derparams;
	const struct ecc_sign_ctx *keyinfo = ECC_CCTX(cbdata->sctx);
	uint8_t keybuf[EC_PUB_KEY_MAX_SIZE];
	uint8_t rawsig[EC_MAX_SIGLEN];
	size_t keysz;
	int ret;
	uint8_t ecsiglen;

	keysz = MIN(sizeof(keybuf), cbdata->keylen / 2);

	keysz = sizeof(keybuf);
	if (ecc_extract_pubkey(cbdata->keyfp, cbdata->key, cbdata->keylen,
	    keybuf, &keysz, &derparams) != 0) {
		warnx("failed to parse key");
		return (1);
	}

	ret = ec_get_sig_len(&derparams, keyinfo->sig_alg, keyinfo->sig_hash,
	    &ecsiglen);
	if (ret != 0)
		return (1);

	/*
	 * Signatures are DER-encoded, whether by OpenSSL or pkg.
	 */
	if (ecc_extract_signature(cbdata->sig, cbdata->siglen,
	    rawsig, ecsiglen) != 0) {
		warnx("failed to decode signature");
		return (1);
	}

	ret = ec_pub_key_import_from_aff_buf(&pubkey, &derparams,
	    keybuf, keysz, keyinfo->sig_alg);
	if (ret != 0) {
		warnx("failed to import key");
		return (1);
	}

	ret = ec_verify(rawsig, ecsiglen, &pubkey, hash, hashsz, keyinfo->sig_alg,
	    keyinfo->sig_hash, NULL, 0);
	if (ret != 0) {
		warnx("failed to verify signature");
		return (1);
	}

	return (0);
}

static bool
ecc_verify_data(const struct pkgsign_ctx *sctx,
    const char *data, size_t datasz, const char *sigfile,
    const unsigned char *key, int keylen,
    unsigned char *sig, int siglen)
{
	int ret;
	struct ecc_verify_cbdata cbdata;

	ret = 1;

	if (sigfile != NULL) {
		cbdata.keyfp = fopen(sigfile, "r");
		if (cbdata.keyfp == NULL) {
			warn("fopen: %s", sigfile);
			return (false);
		}
	} else {
		cbdata.keyfp = NULL;
		cbdata.key = key;
		cbdata.keylen = keylen;
	}

	cbdata.sctx = sctx;
	cbdata.sig = sig;
	cbdata.siglen = siglen;

	ret = ecc_verify_internal(&cbdata, data, datasz);

	if (cbdata.keyfp != NULL)
		fclose(cbdata.keyfp);

	return (ret == 0);
}

static bool
ecc_verify_cert(const struct pkgsign_ctx *sctx, int fd,
    const char *sigfile, const unsigned char *key, int keylen,
    unsigned char *sig, int siglen)
{
	bool ret;
	char *sha256;

	ret = false;
	if (lseek(fd, 0, SEEK_SET) == -1) {
		warn("lseek");
		return (false);
	}

	if ((sha256 = sha256_fd(fd)) != NULL) {
		ret = ecc_verify_data(sctx, sha256, strlen(sha256), sigfile, key,
		    keylen, sig, siglen);
		free(sha256);
	}

	return (ret);
}

static int
ecc_new(const char *name __unused, struct pkgsign_ctx *sctx)
{
	struct ecc_sign_ctx *keyinfo = ECC_CTX(sctx);
	int ret;

	ret = 1;
	if (STREQ(name, "ecc") || STREQ(name, "eddsa")) {
		keyinfo->sig_alg = EDDSA25519;
		keyinfo->sig_hash = SHA512;
		ret = import_params(&keyinfo->params, &wei25519_str_params);
	} else if (STREQ(name, "ecdsa")) {
		keyinfo->sig_alg = ECDSA;
		keyinfo->sig_hash = SHA256;
		ret = import_params(&keyinfo->params, &secp256k1_str_params);
	}

	if (ret != 0)
		return (1);

	return (0);
}

const struct pkgsign_ops pkgsign_ecc = {
	.pkgsign_ctx_size = sizeof(struct ecc_sign_ctx),
	.pkgsign_new = ecc_new,
	.pkgsign_verify_cert = ecc_verify_cert,
	.pkgsign_verify_data = ecc_verify_data,
};
