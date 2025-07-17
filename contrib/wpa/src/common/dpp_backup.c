/*
 * DPP configurator backup
 * Copyright (c) 2019-2020, The Linux Foundation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "crypto/aes.h"
#include "crypto/aes_siv.h"
#include "tls/asn1.h"
#include "dpp.h"
#include "dpp_i.h"

#ifdef CONFIG_DPP2

void dpp_free_asymmetric_key(struct dpp_asymmetric_key *key)
{
	while (key) {
		struct dpp_asymmetric_key *next = key->next;

		crypto_ec_key_deinit(key->csign);
		crypto_ec_key_deinit(key->pp_key);
		str_clear_free(key->config_template);
		str_clear_free(key->connector_template);
		os_free(key);
		key = next;
	}
}


static struct wpabuf * dpp_build_conf_params(struct dpp_configurator *conf)
{
	struct wpabuf *buf, *priv_key = NULL;
	size_t len;
	/* TODO: proper template values */
	const char *conf_template = "{\"wi-fi_tech\":\"infra\",\"discovery\":{\"ssid\":\"test\"},\"cred\":{\"akm\":\"dpp\"}}";
	const char *connector_template = NULL;

	if (!conf->pp_key)
		return NULL;

	priv_key = crypto_ec_key_get_ecprivate_key(conf->pp_key, false);
	if (!priv_key)
		return NULL;

	len = 100 + os_strlen(conf_template);
	if (connector_template)
		len += os_strlen(connector_template);
	if (priv_key)
		len += wpabuf_len(priv_key);
	buf = wpabuf_alloc(len);
	if (!buf)
		goto fail;

	/*
	 * DPPConfigurationParameters ::= SEQUENCE {
	 *    privacyProtectionKey      PrivateKey,
	 *    configurationTemplate	UTF8String,
	 *    connectorTemplate		UTF8String OPTIONAL}
	 */

	/* PrivateKey ::= OCTET STRING */
	asn1_put_octet_string(buf, priv_key);

	asn1_put_utf8string(buf, conf_template);
	if (connector_template)
		asn1_put_utf8string(buf, connector_template);
	wpabuf_clear_free(priv_key);
	return asn1_encaps(buf, ASN1_CLASS_UNIVERSAL, ASN1_TAG_SEQUENCE);
fail:
	wpabuf_clear_free(priv_key);
	return NULL;
}


static struct wpabuf * dpp_build_attribute(struct dpp_configurator *conf)
{
	struct wpabuf *conf_params, *attr;

	/*
	 * aa-DPPConfigurationParameters ATTRIBUTE ::=
	 * { TYPE DPPConfigurationParameters IDENTIFIED BY id-DPPConfigParams }
	 *
	 * Attribute ::= SEQUENCE {
	 *    type OBJECT IDENTIFIER,
	 *    values SET SIZE(1..MAX) OF Type
	 */
	conf_params = dpp_build_conf_params(conf);
	conf_params = asn1_encaps(conf_params, ASN1_CLASS_UNIVERSAL,
				  ASN1_TAG_SET);
	if (!conf_params)
		return NULL;

	attr = wpabuf_alloc(100 + wpabuf_len(conf_params));
	if (!attr) {
		wpabuf_clear_free(conf_params);
		return NULL;
	}

	asn1_put_oid(attr, &asn1_dpp_config_params_oid);
	wpabuf_put_buf(attr, conf_params);
	wpabuf_clear_free(conf_params);

	return asn1_encaps(attr, ASN1_CLASS_UNIVERSAL, ASN1_TAG_SEQUENCE);
}


static struct wpabuf * dpp_build_key_alg(const struct dpp_curve_params *curve)
{
	const struct asn1_oid *oid;
	struct wpabuf *params, *res;

	switch (curve->ike_group) {
	case 19:
		oid = &asn1_prime256v1_oid;
		break;
	case 20:
		oid = &asn1_secp384r1_oid;
		break;
	case 21:
		oid = &asn1_secp521r1_oid;
		break;
	case 28:
		oid = &asn1_brainpoolP256r1_oid;
		break;
	case 29:
		oid = &asn1_brainpoolP384r1_oid;
		break;
	case 30:
		oid = &asn1_brainpoolP512r1_oid;
		break;
	default:
		return NULL;
	}

	params = wpabuf_alloc(20);
	if (!params)
		return NULL;
	asn1_put_oid(params, oid); /* namedCurve */

	res = asn1_build_alg_id(&asn1_ec_public_key_oid, params);
	wpabuf_free(params);
	return res;
}


static struct wpabuf * dpp_build_key_pkg(struct dpp_authentication *auth)
{
	struct wpabuf *key = NULL, *attr, *alg, *priv_key = NULL;

	priv_key = crypto_ec_key_get_ecprivate_key(auth->conf->csign, false);
	if (!priv_key)
		return NULL;

	alg = dpp_build_key_alg(auth->conf->curve);

	/* Attributes ::= SET OF Attribute { { OneAsymmetricKeyAttributes } } */
	attr = dpp_build_attribute(auth->conf);
	attr = asn1_encaps(attr, ASN1_CLASS_UNIVERSAL, ASN1_TAG_SET);
	if (!priv_key || !attr || !alg)
		goto fail;

	/*
	 * OneAsymmetricKey ::= SEQUENCE {
	 *    version			Version,
	 *    privateKeyAlgorithm	PrivateKeyAlgorithmIdentifier,
	 *    privateKey		PrivateKey,
	 *    attributes		[0] Attributes OPTIONAL,
	 *    ...,
	 *    [[2: publicKey		[1] BIT STRING OPTIONAL ]],
	 *    ...
	 * }
	 */

	key = wpabuf_alloc(100 + wpabuf_len(alg) + wpabuf_len(priv_key) +
			   wpabuf_len(attr));
	if (!key)
		goto fail;

	asn1_put_integer(key, 0); /* version = v1(0) */

	/* PrivateKeyAlgorithmIdentifier */
	wpabuf_put_buf(key, alg);

	/* PrivateKey ::= OCTET STRING */
	asn1_put_octet_string(key, priv_key);

	/* [0] Attributes OPTIONAL */
	asn1_put_hdr(key, ASN1_CLASS_CONTEXT_SPECIFIC, 1, 0, wpabuf_len(attr));
	wpabuf_put_buf(key, attr);

fail:
	wpabuf_clear_free(attr);
	wpabuf_clear_free(priv_key);
	wpabuf_free(alg);

	/*
	 * DPPAsymmetricKeyPackage ::= AsymmetricKeyPackage
	 *
	 * AsymmetricKeyPackage ::= SEQUENCE SIZE (1..MAX) OF OneAsymmetricKey
	 *
	 * OneAsymmetricKey ::= SEQUENCE
	 */
	return asn1_encaps(asn1_encaps(key,
				       ASN1_CLASS_UNIVERSAL, ASN1_TAG_SEQUENCE),
			   ASN1_CLASS_UNIVERSAL, ASN1_TAG_SEQUENCE);
}


static struct wpabuf * dpp_build_pbkdf2_alg_id(const struct wpabuf *salt,
					       size_t hash_len)
{
	struct wpabuf *params = NULL, *buf = NULL, *prf = NULL;
	const struct asn1_oid *oid;

	/*
	 * PBKDF2-params ::= SEQUENCE {
	 *    salt CHOICE {
	 *       specified OCTET STRING,
	 *       otherSource AlgorithmIdentifier}
	 *    iterationCount INTEGER (1..MAX),
	 *    keyLength INTEGER (1..MAX),
	 *    prf AlgorithmIdentifier}
	 *
	 * salt is an 64 octet value, iterationCount is 1000, keyLength is based
	 * on Configurator signing key length, prf is
	 * id-hmacWithSHA{256,384,512} based on Configurator signing key.
	 */

	if (hash_len == 32)
		oid = &asn1_pbkdf2_hmac_sha256_oid;
	else if (hash_len == 48)
		oid = &asn1_pbkdf2_hmac_sha384_oid;
	else if (hash_len == 64)
		oid = &asn1_pbkdf2_hmac_sha512_oid;
	else
		goto fail;
	prf = asn1_build_alg_id(oid, NULL);
	if (!prf)
		goto fail;
	params = wpabuf_alloc(100 + wpabuf_len(salt) + wpabuf_len(prf));
	if (!params)
		goto fail;
	asn1_put_octet_string(params, salt); /* salt.specified */
	asn1_put_integer(params, 1000); /* iterationCount */
	asn1_put_integer(params, hash_len); /* keyLength */
	wpabuf_put_buf(params, prf);
	params = asn1_encaps(params, ASN1_CLASS_UNIVERSAL, ASN1_TAG_SEQUENCE);
	if (!params)
		goto fail;
	buf = asn1_build_alg_id(&asn1_pbkdf2_oid, params);
fail:
	wpabuf_free(params);
	wpabuf_free(prf);
	return buf;
}


static struct wpabuf *
dpp_build_pw_recipient_info(struct dpp_authentication *auth, size_t hash_len,
			    const struct wpabuf *cont_enc_key)
{
	struct wpabuf *pwri = NULL, *enc_key = NULL, *key_der_alg = NULL,
		*key_enc_alg = NULL, *salt;
	u8 kek[DPP_MAX_HASH_LEN];
	u8 key[DPP_MAX_HASH_LEN];
	size_t key_len;
	int res;

	salt = wpabuf_alloc(64);
	if (!salt || os_get_random(wpabuf_put(salt, 64), 64) < 0)
		goto fail;
	wpa_hexdump_buf(MSG_DEBUG, "DPP: PBKDF2 salt", salt);

	key_len = auth->curve->hash_len;
	/* password = HKDF-Expand(bk, "Enveloped Data Password", length) */
	res = dpp_hkdf_expand(key_len, auth->bk, key_len,
			      "Enveloped Data Password", key, key_len);
	if (res < 0)
		goto fail;
	wpa_hexdump_key(MSG_DEBUG, "DPP: PBKDF2 key", key, key_len);

	if (dpp_pbkdf2(hash_len, key, key_len, wpabuf_head(salt), 64, 1000,
		       kek, hash_len)) {
		wpa_printf(MSG_DEBUG, "DPP: PBKDF2 failed");
		goto fail;
	}
	wpa_hexdump_key(MSG_DEBUG, "DPP: key-encryption key from PBKDF2",
			kek, hash_len);

	enc_key = wpabuf_alloc(hash_len + AES_BLOCK_SIZE);
	if (!enc_key ||
	    aes_siv_encrypt(kek, hash_len, wpabuf_head(cont_enc_key),
			    wpabuf_len(cont_enc_key), 0, NULL, NULL,
			    wpabuf_put(enc_key, hash_len + AES_BLOCK_SIZE)) < 0)
		goto fail;
	wpa_hexdump_buf(MSG_DEBUG, "DPP: encryptedKey", enc_key);

	/*
	 * PasswordRecipientInfo ::= SEQUENCE {
	 *    version			CMSVersion,
	 *    keyDerivationAlgorithm [0] KeyDerivationAlgorithmIdentifier OPTIONAL,
	 *    keyEncryptionAlgorithm	KeyEncryptionAlgorithmIdentifier,
	 *    encryptedKey		EncryptedKey}
	 *
	 * version is 0, keyDerivationAlgorithm is id-PKBDF2, and the
	 * parameters contains PBKDF2-params SEQUENCE.
	 */

	key_der_alg = dpp_build_pbkdf2_alg_id(salt, hash_len);
	key_enc_alg = asn1_build_alg_id(&asn1_aes_siv_cmac_aead_256_oid, NULL);
	if (!key_der_alg || !key_enc_alg)
		goto fail;
	pwri = wpabuf_alloc(100 + wpabuf_len(key_der_alg) +
			    wpabuf_len(key_enc_alg) + wpabuf_len(enc_key));
	if (!pwri)
		goto fail;

	/* version = 0 */
	asn1_put_integer(pwri, 0);

	/* [0] KeyDerivationAlgorithmIdentifier */
	asn1_put_hdr(pwri, ASN1_CLASS_CONTEXT_SPECIFIC, 1, 0,
		     wpabuf_len(key_der_alg));
	wpabuf_put_buf(pwri, key_der_alg);

	/* KeyEncryptionAlgorithmIdentifier */
	wpabuf_put_buf(pwri, key_enc_alg);

	/* EncryptedKey ::= OCTET STRING */
	asn1_put_octet_string(pwri, enc_key);

fail:
	wpabuf_clear_free(key_der_alg);
	wpabuf_free(key_enc_alg);
	wpabuf_free(enc_key);
	wpabuf_free(salt);
	forced_memzero(kek, sizeof(kek));
	return asn1_encaps(pwri, ASN1_CLASS_UNIVERSAL, ASN1_TAG_SEQUENCE);
}


static struct wpabuf *
dpp_build_recipient_info(struct dpp_authentication *auth, size_t hash_len,
			 const struct wpabuf *cont_enc_key)
{
	struct wpabuf *pwri;

	/*
	 * RecipientInfo ::= CHOICE {
	 *    ktri		KeyTransRecipientInfo,
	 *    kari	[1]	KeyAgreeRecipientInfo,
	 *    kekri	[2]	KEKRecipientInfo,
	 *    pwri	[3]	PasswordRecipientInfo,
	 *    ori	[4]	OtherRecipientInfo}
	 *
	 * Shall always use the pwri CHOICE.
	 */

	pwri = dpp_build_pw_recipient_info(auth, hash_len, cont_enc_key);
	return asn1_encaps(pwri, ASN1_CLASS_CONTEXT_SPECIFIC, 3);
}


static struct wpabuf *
dpp_build_enc_cont_info(struct dpp_authentication *auth, size_t hash_len,
			const struct wpabuf *cont_enc_key)
{
	struct wpabuf *key_pkg, *enc_cont_info = NULL, *enc_cont = NULL,
		*enc_alg;
	const struct asn1_oid *oid;
	size_t enc_cont_len;

	/*
	 * EncryptedContentInfo ::= SEQUENCE {
	 *    contentType			ContentType,
	 *    contentEncryptionAlgorithm  ContentEncryptionAlgorithmIdentifier,
	 *    encryptedContent	[0] IMPLICIT	EncryptedContent OPTIONAL}
	 */

	if (hash_len == 32)
		oid = &asn1_aes_siv_cmac_aead_256_oid;
	else if (hash_len == 48)
		oid = &asn1_aes_siv_cmac_aead_384_oid;
	else if (hash_len == 64)
		oid = &asn1_aes_siv_cmac_aead_512_oid;
	else
		return NULL;

	key_pkg = dpp_build_key_pkg(auth);
	enc_alg = asn1_build_alg_id(oid, NULL);
	if (!key_pkg || !enc_alg)
		goto fail;

	wpa_hexdump_buf_key(MSG_MSGDUMP, "DPP: DPPAsymmetricKeyPackage",
			    key_pkg);

	enc_cont_len = wpabuf_len(key_pkg) + AES_BLOCK_SIZE;
	enc_cont = wpabuf_alloc(enc_cont_len);
	if (!enc_cont ||
	    aes_siv_encrypt(wpabuf_head(cont_enc_key), wpabuf_len(cont_enc_key),
			    wpabuf_head(key_pkg), wpabuf_len(key_pkg),
			    0, NULL, NULL,
			    wpabuf_put(enc_cont, enc_cont_len)) < 0)
		goto fail;

	enc_cont_info = wpabuf_alloc(100 + wpabuf_len(enc_alg) +
				     wpabuf_len(enc_cont));
	if (!enc_cont_info)
		goto fail;

	/* ContentType ::= OBJECT IDENTIFIER */
	asn1_put_oid(enc_cont_info, &asn1_dpp_asymmetric_key_package_oid);

	/* ContentEncryptionAlgorithmIdentifier ::= AlgorithmIdentifier */
	wpabuf_put_buf(enc_cont_info, enc_alg);

	/* encryptedContent [0] IMPLICIT EncryptedContent OPTIONAL
	 * EncryptedContent ::= OCTET STRING */
	asn1_put_hdr(enc_cont_info, ASN1_CLASS_CONTEXT_SPECIFIC, 0, 0,
		     wpabuf_len(enc_cont));
	wpabuf_put_buf(enc_cont_info, enc_cont);

fail:
	wpabuf_clear_free(key_pkg);
	wpabuf_free(enc_cont);
	wpabuf_free(enc_alg);
	return enc_cont_info;
}


static struct wpabuf * dpp_gen_random(size_t len)
{
	struct wpabuf *key;

	key = wpabuf_alloc(len);
	if (!key || os_get_random(wpabuf_put(key, len), len) < 0) {
		wpabuf_free(key);
		key = NULL;
	}
	wpa_hexdump_buf_key(MSG_DEBUG, "DPP: content-encryption key", key);
	return key;
}


struct wpabuf * dpp_build_enveloped_data(struct dpp_authentication *auth)
{
	struct wpabuf *env = NULL;
	struct wpabuf *recipient_info = NULL, *enc_cont_info = NULL;
	struct wpabuf *cont_enc_key = NULL;
	size_t hash_len;

	if (!auth->conf) {
		wpa_printf(MSG_DEBUG,
			   "DPP: No Configurator instance selected for the session - cannot build DPPEnvelopedData");
		return NULL;
	}

	if (!auth->provision_configurator) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Configurator provisioning not allowed");
		return NULL;
	}

	wpa_printf(MSG_DEBUG, "DPP: Building DPPEnvelopedData");

	hash_len = auth->conf->curve->hash_len;
	cont_enc_key = dpp_gen_random(hash_len);
	if (!cont_enc_key)
		goto fail;
	recipient_info = dpp_build_recipient_info(auth, hash_len, cont_enc_key);
	enc_cont_info = dpp_build_enc_cont_info(auth, hash_len, cont_enc_key);
	if (!recipient_info || !enc_cont_info)
		goto fail;

	env = wpabuf_alloc(wpabuf_len(recipient_info) +
			   wpabuf_len(enc_cont_info) +
			   100);
	if (!env)
		goto fail;

	/*
	 * DPPEnvelopedData ::= EnvelopedData
	 *
	 * EnvelopedData ::= SEQUENCE {
	 *    version			CMSVersion,
	 *    originatorInfo	[0]	IMPLICIT OriginatorInfo OPTIONAL,
	 *    recipientInfos		RecipientInfos,
	 *    encryptedContentInfo	EncryptedContentInfo,
	 *    unprotectedAttrs  [1] IMPLICIT	UnprotectedAttributes OPTIONAL}
	 *
	 * For DPP, version is 3, both originatorInfo and
	 * unprotectedAttrs are omitted, and recipientInfos contains a single
	 * RecipientInfo.
	 */

	/* EnvelopedData.version = 3 */
	asn1_put_integer(env, 3);

	/* RecipientInfos ::= SET SIZE (1..MAX) OF RecipientInfo */
	asn1_put_set(env, recipient_info);

	/* EncryptedContentInfo ::= SEQUENCE */
	asn1_put_sequence(env, enc_cont_info);

	env = asn1_encaps(env, ASN1_CLASS_UNIVERSAL, ASN1_TAG_SEQUENCE);
	wpa_hexdump_buf(MSG_MSGDUMP, "DPP: DPPEnvelopedData", env);
out:
	wpabuf_clear_free(cont_enc_key);
	wpabuf_clear_free(recipient_info);
	wpabuf_free(enc_cont_info);
	return env;
fail:
	wpabuf_free(env);
	env = NULL;
	goto out;
}


struct dpp_enveloped_data {
	const u8 *enc_cont;
	size_t enc_cont_len;
	const u8 *enc_key;
	size_t enc_key_len;
	const u8 *salt;
	size_t pbkdf2_key_len;
	size_t prf_hash_len;
};


static int dpp_parse_recipient_infos(const u8 *pos, size_t len,
				     struct dpp_enveloped_data *data)
{
	struct asn1_hdr hdr;
	const u8 *end = pos + len;
	const u8 *next, *e_end;
	struct asn1_oid oid;
	int val;
	const u8 *params;
	size_t params_len;

	wpa_hexdump(MSG_MSGDUMP, "DPP: RecipientInfos", pos, len);

	/*
	 * RecipientInfo ::= CHOICE {
	 *    ktri		KeyTransRecipientInfo,
	 *    kari	[1]	KeyAgreeRecipientInfo,
	 *    kekri	[2]	KEKRecipientInfo,
	 *    pwri	[3]	PasswordRecipientInfo,
	 *    ori	[4]	OtherRecipientInfo}
	 *
	 * Shall always use the pwri CHOICE.
	 */

	if (asn1_get_next(pos, end - pos, &hdr) < 0 || !hdr.constructed ||
	    !asn1_is_cs_tag(&hdr, 3)) {
		asn1_unexpected(&hdr, "DPP: Expected CHOICE [3] (pwri)");
		return -1;
	}
	wpa_hexdump(MSG_MSGDUMP, "DPP: PasswordRecipientInfo",
		    hdr.payload, hdr.length);
	pos = hdr.payload;
	end = pos + hdr.length;

	/*
	 * PasswordRecipientInfo ::= SEQUENCE {
	 *    version			CMSVersion,
	 *    keyDerivationAlgorithm [0] KeyDerivationAlgorithmIdentifier OPTIONAL,
	 *    keyEncryptionAlgorithm	KeyEncryptionAlgorithmIdentifier,
	 *    encryptedKey		EncryptedKey}
	 *
	 * version is 0, keyDerivationAlgorithm is id-PKBDF2, and the
	 * parameters contains PBKDF2-params SEQUENCE.
	 */

	if (asn1_get_sequence(pos, end - pos, &hdr, &end) < 0)
		return -1;
	pos = hdr.payload;

	if (asn1_get_integer(pos, end - pos, &val, &pos) < 0)
		return -1;
	if (val != 0) {
		wpa_printf(MSG_DEBUG, "DPP: pwri.version != 0");
		return -1;
	}

	wpa_hexdump(MSG_MSGDUMP, "DPP: Remaining PasswordRecipientInfo after version",
		    pos, end - pos);

	if (asn1_get_next(pos, end - pos, &hdr) < 0 || !hdr.constructed ||
	    !asn1_is_cs_tag(&hdr, 0)) {
		asn1_unexpected(&hdr,
				"DPP: Expected keyDerivationAlgorithm [0]");
		return -1;
	}
	pos = hdr.payload;
	e_end = pos + hdr.length;

	/* KeyDerivationAlgorithmIdentifier ::= AlgorithmIdentifier */
	if (asn1_get_alg_id(pos, e_end - pos, &oid, &params, &params_len,
			    &next) < 0)
		return -1;
	if (!asn1_oid_equal(&oid, &asn1_pbkdf2_oid)) {
		char buf[80];

		asn1_oid_to_str(&oid, buf, sizeof(buf));
		wpa_printf(MSG_DEBUG,
			   "DPP: Unexpected KeyDerivationAlgorithmIdentifier %s",
			   buf);
		return -1;
	}

	/*
	 * PBKDF2-params ::= SEQUENCE {
	 *    salt CHOICE {
	 *       specified OCTET STRING,
	 *       otherSource AlgorithmIdentifier}
	 *    iterationCount INTEGER (1..MAX),
	 *    keyLength INTEGER (1..MAX),
	 *    prf AlgorithmIdentifier}
	 *
	 * salt is an 64 octet value, iterationCount is 1000, keyLength is based
	 * on Configurator signing key length, prf is
	 * id-hmacWithSHA{256,384,512} based on Configurator signing key.
	 */
	if (!params ||
	    asn1_get_sequence(params, params_len, &hdr, &e_end) < 0)
		return -1;
	pos = hdr.payload;

	if (asn1_get_next(pos, e_end - pos, &hdr) < 0 ||
	    !asn1_is_octetstring(&hdr)) {
		asn1_unexpected(&hdr,
				"DPP: Expected OCTETSTRING (salt.specified)");
		return -1;
	}
	wpa_hexdump(MSG_MSGDUMP, "DPP: salt.specified",
		    hdr.payload, hdr.length);
	if (hdr.length != 64) {
		wpa_printf(MSG_DEBUG, "DPP: Unexpected salt length %u",
			   hdr.length);
		return -1;
	}
	data->salt = hdr.payload;
	pos = hdr.payload + hdr.length;

	if (asn1_get_integer(pos, e_end - pos, &val, &pos) < 0)
		return -1;
	if (val != 1000) {
		wpa_printf(MSG_DEBUG, "DPP: Unexpected iterationCount %d", val);
		return -1;
	}

	if (asn1_get_integer(pos, e_end - pos, &val, &pos) < 0)
		return -1;
	if (val != 32 && val != 48 && val != 64) {
		wpa_printf(MSG_DEBUG, "DPP: Unexpected keyLength %d", val);
		return -1;
	}
	data->pbkdf2_key_len = val;

	if (asn1_get_sequence(pos, e_end - pos, &hdr, NULL) < 0 ||
	    asn1_get_oid(hdr.payload, hdr.length, &oid, &pos) < 0) {
		wpa_printf(MSG_DEBUG, "DPP: Could not parse prf");
		return -1;
	}
	if (asn1_oid_equal(&oid, &asn1_pbkdf2_hmac_sha256_oid)) {
		data->prf_hash_len = 32;
	} else if (asn1_oid_equal(&oid, &asn1_pbkdf2_hmac_sha384_oid)) {
		data->prf_hash_len = 48;
	} else if (asn1_oid_equal(&oid, &asn1_pbkdf2_hmac_sha512_oid)) {
		data->prf_hash_len = 64;
	} else {
		char buf[80];

		asn1_oid_to_str(&oid, buf, sizeof(buf));
		wpa_printf(MSG_DEBUG, "DPP: Unexpected PBKDF2-params.prf %s",
			   buf);
		return -1;
	}

	pos = next;

	/* keyEncryptionAlgorithm KeyEncryptionAlgorithmIdentifier
	 *
	 * KeyEncryptionAlgorithmIdentifier ::= AlgorithmIdentifier
	 *
	 * id-alg-AES-SIV-CMAC-aed-256, id-alg-AES-SIV-CMAC-aed-384, or
	 * id-alg-AES-SIV-CMAC-aed-512. */
	if (asn1_get_alg_id(pos, end - pos, &oid, NULL, NULL, &pos) < 0)
		return -1;
	if (!asn1_oid_equal(&oid, &asn1_aes_siv_cmac_aead_256_oid) &&
	    !asn1_oid_equal(&oid, &asn1_aes_siv_cmac_aead_384_oid) &&
	    !asn1_oid_equal(&oid, &asn1_aes_siv_cmac_aead_512_oid)) {
		char buf[80];

		asn1_oid_to_str(&oid, buf, sizeof(buf));
		wpa_printf(MSG_DEBUG,
			   "DPP: Unexpected KeyEncryptionAlgorithmIdentifier %s",
			   buf);
		return -1;
	}

	/*
	 * encryptedKey EncryptedKey
	 *
	 * EncryptedKey ::= OCTET STRING
	 */
	if (asn1_get_next(pos, end - pos, &hdr) < 0 ||
	    !asn1_is_octetstring(&hdr)) {
		asn1_unexpected(&hdr,
				"DPP: Expected OCTETSTRING (pwri.encryptedKey)");
		return -1;
	}
	wpa_hexdump(MSG_MSGDUMP, "DPP: pwri.encryptedKey",
		    hdr.payload, hdr.length);
	data->enc_key = hdr.payload;
	data->enc_key_len = hdr.length;

	return 0;
}


static int dpp_parse_encrypted_content_info(const u8 *pos, const u8 *end,
					    struct dpp_enveloped_data *data)
{
	struct asn1_hdr hdr;
	struct asn1_oid oid;

	/*
	 * EncryptedContentInfo ::= SEQUENCE {
	 *    contentType			ContentType,
	 *    contentEncryptionAlgorithm  ContentEncryptionAlgorithmIdentifier,
	 *    encryptedContent	[0] IMPLICIT	EncryptedContent OPTIONAL}
	 */
	if (asn1_get_sequence(pos, end - pos, &hdr, &pos) < 0)
		return -1;
	wpa_hexdump(MSG_MSGDUMP, "DPP: EncryptedContentInfo",
		    hdr.payload, hdr.length);
	if (pos < end) {
		wpa_hexdump(MSG_DEBUG,
			    "DPP: Unexpected extra data after EncryptedContentInfo",
			    pos, end - pos);
		return -1;
	}

	end = pos;
	pos = hdr.payload;

	/* ContentType ::= OBJECT IDENTIFIER */
	if (asn1_get_oid(pos, end - pos, &oid, &pos) < 0) {
		wpa_printf(MSG_DEBUG, "DPP: Could not parse ContentType");
		return -1;
	}
	if (!asn1_oid_equal(&oid, &asn1_dpp_asymmetric_key_package_oid)) {
		char buf[80];

		asn1_oid_to_str(&oid, buf, sizeof(buf));
		wpa_printf(MSG_DEBUG, "DPP: Unexpected ContentType %s", buf);
		return -1;
	}

	/* ContentEncryptionAlgorithmIdentifier ::= AlgorithmIdentifier */
	if (asn1_get_alg_id(pos, end - pos, &oid, NULL, NULL, &pos) < 0)
		return -1;
	if (!asn1_oid_equal(&oid, &asn1_aes_siv_cmac_aead_256_oid) &&
	    !asn1_oid_equal(&oid, &asn1_aes_siv_cmac_aead_384_oid) &&
	    !asn1_oid_equal(&oid, &asn1_aes_siv_cmac_aead_512_oid)) {
		char buf[80];

		asn1_oid_to_str(&oid, buf, sizeof(buf));
		wpa_printf(MSG_DEBUG,
			   "DPP: Unexpected ContentEncryptionAlgorithmIdentifier %s",
			   buf);
		return -1;
	}
	/* ignore optional parameters */

	/* encryptedContent [0] IMPLICIT EncryptedContent OPTIONAL
	 * EncryptedContent ::= OCTET STRING */
	if (asn1_get_next(pos, end - pos, &hdr) < 0 || hdr.constructed ||
	    !asn1_is_cs_tag(&hdr, 0)) {
		asn1_unexpected(&hdr,
				"DPP: Expected [0] IMPLICIT (EncryptedContent)");
		return -1;
	}
	wpa_hexdump(MSG_MSGDUMP, "DPP: EncryptedContent",
		    hdr.payload, hdr.length);
	data->enc_cont = hdr.payload;
	data->enc_cont_len = hdr.length;
	return 0;
}


static int dpp_parse_enveloped_data(const u8 *env_data, size_t env_data_len,
				    struct dpp_enveloped_data *data)
{
	struct asn1_hdr hdr;
	const u8 *pos, *end;
	int val;

	os_memset(data, 0, sizeof(*data));

	/*
	 * DPPEnvelopedData ::= EnvelopedData
	 *
	 * EnvelopedData ::= SEQUENCE {
	 *    version			CMSVersion,
	 *    originatorInfo	[0]	IMPLICIT OriginatorInfo OPTIONAL,
	 *    recipientInfos		RecipientInfos,
	 *    encryptedContentInfo	EncryptedContentInfo,
	 *    unprotectedAttrs  [1] IMPLICIT	UnprotectedAttributes OPTIONAL}
	 *
	 * CMSVersion ::= INTEGER
	 *
	 * RecipientInfos ::= SET SIZE (1..MAX) OF RecipientInfo
	 *
	 * For DPP, version is 3, both originatorInfo and
	 * unprotectedAttrs are omitted, and recipientInfos contains a single
	 * RecipientInfo.
	 */
	if (asn1_get_sequence(env_data, env_data_len, &hdr, &end) < 0)
		return -1;
	pos = hdr.payload;
	if (end < env_data + env_data_len) {
		wpa_hexdump(MSG_DEBUG,
			    "DPP: Unexpected extra data after DPPEnvelopedData",
			    end, env_data + env_data_len - end);
		return -1;
	}

	if (asn1_get_integer(pos, end - pos, &val, &pos) < 0)
		return -1;
	if (val != 3) {
		wpa_printf(MSG_DEBUG, "DPP: EnvelopedData.version != 3");
		return -1;
	}

	if (asn1_get_next(pos, end - pos, &hdr) < 0 || !asn1_is_set(&hdr)) {
		asn1_unexpected(&hdr,
				"DPP: Expected SET (RecipientInfos)");
		return -1;
	}

	if (dpp_parse_recipient_infos(hdr.payload, hdr.length, data) < 0)
		return -1;
	return dpp_parse_encrypted_content_info(hdr.payload + hdr.length, end,
						data);
}


static struct dpp_asymmetric_key *
dpp_parse_one_asymmetric_key(const u8 *buf, size_t len)
{
	struct asn1_hdr hdr;
	const u8 *pos = buf, *end = buf + len, *next;
	int val;
	const u8 *params;
	size_t params_len;
	struct asn1_oid oid;
	char txt[80];
	struct dpp_asymmetric_key *key;

	wpa_hexdump_key(MSG_MSGDUMP, "DPP: OneAsymmetricKey", buf, len);

	key = os_zalloc(sizeof(*key));
	if (!key)
		return NULL;

	/*
	 * OneAsymmetricKey ::= SEQUENCE {
	 *    version			Version,
	 *    privateKeyAlgorithm	PrivateKeyAlgorithmIdentifier,
	 *    privateKey		PrivateKey,
	 *    attributes		[0] Attributes OPTIONAL,
	 *    ...,
	 *    [[2: publicKey		[1] BIT STRING OPTIONAL ]],
	 *    ...
	 * }
	 */
	if (asn1_get_sequence(pos, end - pos, &hdr, &end) < 0)
		goto fail;
	pos = hdr.payload;

	/* Version ::= INTEGER { v1(0), v2(1) } (v1, ..., v2) */
	if (asn1_get_integer(pos, end - pos, &val, &pos) < 0)
		goto fail;
	if (val != 0 && val != 1) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Unsupported DPPAsymmetricKeyPackage version %d",
			   val);
		goto fail;
	}

	/* PrivateKeyAlgorithmIdentifier ::= AlgorithmIdentifier */
	if (asn1_get_alg_id(pos, end - pos, &oid, &params, &params_len,
			    &pos) < 0)
		goto fail;
	if (!asn1_oid_equal(&oid, &asn1_ec_public_key_oid)) {
		asn1_oid_to_str(&oid, txt, sizeof(txt));
		wpa_printf(MSG_DEBUG,
			   "DPP: Unsupported PrivateKeyAlgorithmIdentifier %s",
			   txt);
		goto fail;
	}
	wpa_hexdump(MSG_MSGDUMP, "DPP: PrivateKeyAlgorithmIdentifier params",
		    params, params_len);
	/*
	 * ECParameters ::= CHOICE {
	 *    namedCurve	OBJECT IDENTIFIER
	 *    -- implicitCurve	NULL
	 *    -- specifiedCurve	SpecifiedECDomain}
	 */
	if (!params || asn1_get_oid(params, params_len, &oid, &next) < 0) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Could not parse ECParameters.namedCurve");
		goto fail;
	}
	asn1_oid_to_str(&oid, txt, sizeof(txt));
	wpa_printf(MSG_MSGDUMP, "DPP: namedCurve %s", txt);
	/* Assume the curve is identified within ECPrivateKey, so that this
	 * separate indication is not really needed. */

	/*
	 * PrivateKey ::= OCTET STRING
	 *    (Contains DER encoding of ECPrivateKey)
	 */
	if (asn1_get_next(pos, end - pos, &hdr) < 0 ||
	    !asn1_is_octetstring(&hdr)) {
		asn1_unexpected(&hdr,
				"DPP: Expected OCTETSTRING (PrivateKey)");
		goto fail;
	}
	wpa_hexdump_key(MSG_MSGDUMP, "DPP: PrivateKey",
			hdr.payload, hdr.length);
	pos = hdr.payload + hdr.length;
	key->csign = crypto_ec_key_parse_priv(hdr.payload, hdr.length);
	if (!key->csign)
		goto fail;
	if (wpa_debug_show_keys)
		dpp_debug_print_key("DPP: Received c-sign-key", key->csign);

	/*
	 * Attributes ::= SET OF Attribute { { OneAsymmetricKeyAttributes } }
	 *
	 * Exactly one instance of type Attribute in OneAsymmetricKey.
	 */
	if (asn1_get_next(pos, end - pos, &hdr) < 0 || !hdr.constructed ||
	    !asn1_is_cs_tag(&hdr, 0)) {
		asn1_unexpected(&hdr, "DPP: Expected [0] Attributes");
		goto fail;
	}
	wpa_hexdump_key(MSG_MSGDUMP, "DPP: Attributes",
			hdr.payload, hdr.length);
	if (hdr.payload + hdr.length < end) {
		wpa_hexdump_key(MSG_MSGDUMP,
				"DPP: Ignore additional data at the end of OneAsymmetricKey",
				hdr.payload + hdr.length,
				end - (hdr.payload + hdr.length));
	}
	pos = hdr.payload;
	end = hdr.payload + hdr.length;

	if (asn1_get_next(pos, end - pos, &hdr) < 0 || !asn1_is_set(&hdr)) {
		asn1_unexpected(&hdr, "DPP: Expected SET (Attributes)");
		goto fail;
	}
	if (hdr.payload + hdr.length < end) {
		wpa_hexdump_key(MSG_MSGDUMP,
				"DPP: Ignore additional data at the end of OneAsymmetricKey (after SET)",
				hdr.payload + hdr.length,
				end - (hdr.payload + hdr.length));
	}
	pos = hdr.payload;
	end = hdr.payload + hdr.length;

	/*
	 * OneAsymmetricKeyAttributes ATTRIBUTE ::= {
	 *    aa-DPPConfigurationParameters,
	 *    ... -- For local profiles
	 * }
	 *
	 * aa-DPPConfigurationParameters ATTRIBUTE ::=
	 * { TYPE DPPConfigurationParameters IDENTIFIED BY id-DPPConfigParams }
	 *
	 * Attribute ::= SEQUENCE {
	 *    type OBJECT IDENTIFIER,
	 *    values SET SIZE(1..MAX) OF Type
	 *
	 * Exactly one instance of ATTRIBUTE in attrValues.
	 */
	if (asn1_get_sequence(pos, end - pos, &hdr, &pos) < 0)
		goto fail;
	if (pos < end) {
		wpa_hexdump_key(MSG_MSGDUMP,
				"DPP: Ignore additional data at the end of ATTRIBUTE",
				pos, end - pos);
	}
	end = pos;
	pos = hdr.payload;

	if (asn1_get_oid(pos, end - pos, &oid, &pos) < 0)
		goto fail;
	if (!asn1_oid_equal(&oid, &asn1_dpp_config_params_oid)) {
		asn1_oid_to_str(&oid, txt, sizeof(txt));
		wpa_printf(MSG_DEBUG,
			   "DPP: Unexpected Attribute identifier %s", txt);
		goto fail;
	}

	if (asn1_get_next(pos, end - pos, &hdr) < 0 || !asn1_is_set(&hdr)) {
		asn1_unexpected(&hdr, "DPP: Expected SET (Attribute)");
		goto fail;
	}
	pos = hdr.payload;
	end = hdr.payload + hdr.length;

	/*
	 * DPPConfigurationParameters ::= SEQUENCE {
	 *    privacyProtectionKey      PrivateKey,
	 *    configurationTemplate	UTF8String,
	 *    connectorTemplate		UTF8String OPTIONAL}
	 */

	wpa_hexdump_key(MSG_MSGDUMP, "DPP: DPPConfigurationParameters",
			pos, end - pos);
	if (asn1_get_sequence(pos, end - pos, &hdr, &pos) < 0)
		goto fail;
	if (pos < end) {
		wpa_hexdump_key(MSG_MSGDUMP,
				"DPP: Ignore additional data after DPPConfigurationParameters",
				pos, end - pos);
	}
	end = pos;
	pos = hdr.payload;

	/*
	 * PrivateKey ::= OCTET STRING
	 *    (Contains DER encoding of ECPrivateKey)
	 */
	if (asn1_get_next(pos, end - pos, &hdr) < 0 ||
	    !asn1_is_octetstring(&hdr)) {
		asn1_unexpected(&hdr, "DPP: Expected OCTETSTRING (PrivateKey)");
		goto fail;
	}
	wpa_hexdump_key(MSG_MSGDUMP, "DPP: privacyProtectionKey",
			hdr.payload, hdr.length);
	pos = hdr.payload + hdr.length;
	key->pp_key = crypto_ec_key_parse_priv(hdr.payload, hdr.length);
	if (!key->pp_key)
		goto fail;
	if (wpa_debug_show_keys)
		dpp_debug_print_key("DPP: Received privacyProtectionKey",
				    key->pp_key);

	if (asn1_get_next(pos, end - pos, &hdr) < 0 ||
	    !asn1_is_utf8string(&hdr)) {
		asn1_unexpected(&hdr,
				"DPP: Expected UTF8STRING (configurationTemplate)");
		goto fail;
	}
	wpa_hexdump_ascii_key(MSG_MSGDUMP, "DPP: configurationTemplate",
			      hdr.payload, hdr.length);
	key->config_template = os_zalloc(hdr.length + 1);
	if (!key->config_template)
		goto fail;
	os_memcpy(key->config_template, hdr.payload, hdr.length);

	pos = hdr.payload + hdr.length;

	if (pos < end) {
		if (asn1_get_next(pos, end - pos, &hdr) < 0 ||
		    !asn1_is_utf8string(&hdr)) {
			asn1_unexpected(&hdr,
					"DPP: Expected UTF8STRING (connectorTemplate)");
			goto fail;
		}
		wpa_hexdump_ascii_key(MSG_MSGDUMP, "DPP: connectorTemplate",
				      hdr.payload, hdr.length);
		key->connector_template = os_zalloc(hdr.length + 1);
		if (!key->connector_template)
			goto fail;
		os_memcpy(key->connector_template, hdr.payload, hdr.length);
	}

	return key;
fail:
	wpa_printf(MSG_DEBUG, "DPP: Failed to parse OneAsymmetricKey");
	dpp_free_asymmetric_key(key);
	return NULL;
}


static struct dpp_asymmetric_key *
dpp_parse_dpp_asymmetric_key_package(const u8 *key_pkg, size_t key_pkg_len)
{
	struct asn1_hdr hdr;
	const u8 *pos = key_pkg, *end = key_pkg + key_pkg_len;
	struct dpp_asymmetric_key *first = NULL, *last = NULL, *key;

	wpa_hexdump_key(MSG_MSGDUMP, "DPP: DPPAsymmetricKeyPackage",
			key_pkg, key_pkg_len);

	/*
	 * DPPAsymmetricKeyPackage ::= AsymmetricKeyPackage
	 *
	 * AsymmetricKeyPackage ::= SEQUENCE SIZE (1..MAX) OF OneAsymmetricKey
	 */
	while (pos < end) {
		if (asn1_get_sequence(pos, end - pos, &hdr, &pos) < 0 ||
		    !(key = dpp_parse_one_asymmetric_key(hdr.payload,
							 hdr.length))) {
			dpp_free_asymmetric_key(first);
			return NULL;
		}
		if (!last) {
			first = last = key;
		} else {
			last->next = key;
			last = key;
		}
	}

	return first;
}


int dpp_conf_resp_env_data(struct dpp_authentication *auth,
			   const u8 *env_data, size_t env_data_len)
{
	u8 key[DPP_MAX_HASH_LEN];
	size_t key_len;
	u8 kek[DPP_MAX_HASH_LEN];
	u8 cont_encr_key[DPP_MAX_HASH_LEN];
	size_t cont_encr_key_len;
	int res;
	u8 *key_pkg;
	size_t key_pkg_len;
	struct dpp_enveloped_data data;
	struct dpp_asymmetric_key *keys;

	wpa_hexdump(MSG_DEBUG, "DPP: DPPEnvelopedData", env_data, env_data_len);

	if (dpp_parse_enveloped_data(env_data, env_data_len, &data) < 0)
		return -1;

	key_len = auth->curve->hash_len;
	/* password = HKDF-Expand(bk, "Enveloped Data Password", length) */
	res = dpp_hkdf_expand(key_len, auth->bk, key_len,
			      "Enveloped Data Password", key, key_len);
	if (res < 0)
		return -1;
	wpa_hexdump_key(MSG_DEBUG, "DPP: PBKDF2 key", key, key_len);

	if (dpp_pbkdf2(data.prf_hash_len, key, key_len, data.salt, 64, 1000,
		       kek, data.pbkdf2_key_len)) {
		wpa_printf(MSG_DEBUG, "DPP: PBKDF2 failed");
		return -1;
	}
	wpa_hexdump_key(MSG_DEBUG, "DPP: key-encryption key from PBKDF2",
			kek, data.pbkdf2_key_len);

	if (data.enc_key_len < AES_BLOCK_SIZE ||
	    data.enc_key_len > sizeof(cont_encr_key) + AES_BLOCK_SIZE) {
		wpa_printf(MSG_DEBUG, "DPP: Invalid encryptedKey length");
		return -1;
	}
	res = aes_siv_decrypt(kek, data.pbkdf2_key_len,
			      data.enc_key, data.enc_key_len,
			      0, NULL, NULL, cont_encr_key);
	forced_memzero(kek, data.pbkdf2_key_len);
	if (res < 0) {
		wpa_printf(MSG_DEBUG,
			   "DPP: AES-SIV decryption of encryptedKey failed");
		return -1;
	}
	cont_encr_key_len = data.enc_key_len - AES_BLOCK_SIZE;
	wpa_hexdump_key(MSG_DEBUG, "DPP: content-encryption key",
			cont_encr_key, cont_encr_key_len);

	if (data.enc_cont_len < AES_BLOCK_SIZE)
		return -1;
	key_pkg_len = data.enc_cont_len - AES_BLOCK_SIZE;
	key_pkg = os_malloc(key_pkg_len);
	if (!key_pkg)
		return -1;
	res = aes_siv_decrypt(cont_encr_key, cont_encr_key_len,
			      data.enc_cont, data.enc_cont_len,
			      0, NULL, NULL, key_pkg);
	forced_memzero(cont_encr_key, cont_encr_key_len);
	if (res < 0) {
		bin_clear_free(key_pkg, key_pkg_len);
		wpa_printf(MSG_DEBUG,
			   "DPP: AES-SIV decryption of encryptedContent failed");
		return -1;
	}

	keys = dpp_parse_dpp_asymmetric_key_package(key_pkg, key_pkg_len);
	bin_clear_free(key_pkg, key_pkg_len);
	dpp_free_asymmetric_key(auth->conf_key_pkg);
	auth->conf_key_pkg = keys;

	return keys != NULL;
}

#endif /* CONFIG_DPP2 */
