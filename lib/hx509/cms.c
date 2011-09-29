/*
 * Copyright (c) 2003 - 2007 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "hx_locl.h"
RCSID("$Id: cms.c 22327 2007-12-15 04:49:37Z lha $");

/**
 * @page page_cms CMS/PKCS7 message functions.
 *
 * CMS is defined in RFC 3369 and is an continuation of the RSA Labs
 * standard PKCS7. The basic messages in CMS is 
 *
 * - SignedData
 *   Data signed with private key (RSA, DSA, ECDSA) or secret
 *   (symmetric) key
 * - EnvelopedData
 *   Data encrypted with private key (RSA)
 * - EncryptedData
 *   Data encrypted with secret (symmetric) key.
 * - ContentInfo
 *   Wrapper structure including type and data.
 *
 *
 * See the library functions here: @ref hx509_cms
 */

#define ALLOC(X, N) (X) = calloc((N), sizeof(*(X)))
#define ALLOC_SEQ(X, N) do { (X)->len = (N); ALLOC((X)->val, (N)); } while(0)

/**
 * Wrap data and oid in a ContentInfo and encode it.
 *
 * @param oid type of the content.
 * @param buf data to be wrapped. If a NULL pointer is passed in, the
 * optional content field in the ContentInfo is not going be filled
 * in.
 * @param res the encoded buffer, the result should be freed with
 * der_free_octet_string().
 *
 * @return Returns an hx509 error code.
 * 
 * @ingroup hx509_cms
 */

int
hx509_cms_wrap_ContentInfo(const heim_oid *oid,
			   const heim_octet_string *buf,
			   heim_octet_string *res)
{
    ContentInfo ci;
    size_t size;
    int ret;

    memset(res, 0, sizeof(*res));
    memset(&ci, 0, sizeof(ci));

    ret = der_copy_oid(oid, &ci.contentType);
    if (ret)
	return ret;
    if (buf) {
	ALLOC(ci.content, 1);
	if (ci.content == NULL) {
	    free_ContentInfo(&ci);
	    return ENOMEM;
	}
	ci.content->data = malloc(buf->length);
	if (ci.content->data == NULL) {
	    free_ContentInfo(&ci);
	    return ENOMEM;
	}
	memcpy(ci.content->data, buf->data, buf->length);
	ci.content->length = buf->length;
    }

    ASN1_MALLOC_ENCODE(ContentInfo, res->data, res->length, &ci, &size, ret);
    free_ContentInfo(&ci);
    if (ret)
	return ret;
    if (res->length != size)
	_hx509_abort("internal ASN.1 encoder error");

    return 0;
}

/**
 * Decode an ContentInfo and unwrap data and oid it.
 *
 * @param in the encoded buffer.
 * @param oid type of the content.
 * @param out data to be wrapped.
 * @param have_data since the data is optional, this flags show dthe
 * diffrence between no data and the zero length data.
 *
 * @return Returns an hx509 error code.
 * 
 * @ingroup hx509_cms
 */

int
hx509_cms_unwrap_ContentInfo(const heim_octet_string *in,
			     heim_oid *oid,
			     heim_octet_string *out,
			     int *have_data)
{
    ContentInfo ci;
    size_t size;
    int ret;

    memset(oid, 0, sizeof(*oid));
    memset(out, 0, sizeof(*out));

    ret = decode_ContentInfo(in->data, in->length, &ci, &size);
    if (ret)
	return ret;

    ret = der_copy_oid(&ci.contentType, oid);
    if (ret) {
	free_ContentInfo(&ci);
	return ret;
    }
    if (ci.content) {
	ret = der_copy_octet_string(ci.content, out);
	if (ret) {
	    der_free_oid(oid);
	    free_ContentInfo(&ci);
	    return ret;
	}
    } else
	memset(out, 0, sizeof(*out));

    if (have_data)
	*have_data = (ci.content != NULL) ? 1 : 0;

    free_ContentInfo(&ci);

    return 0;
}

#define CMS_ID_SKI	0
#define CMS_ID_NAME	1

static int
fill_CMSIdentifier(const hx509_cert cert,
		   int type,
		   CMSIdentifier *id)
{
    int ret;

    switch (type) {
    case CMS_ID_SKI:
	id->element = choice_CMSIdentifier_subjectKeyIdentifier;
	ret = _hx509_find_extension_subject_key_id(_hx509_get_cert(cert),
						   &id->u.subjectKeyIdentifier);
	if (ret == 0)
	    break;
	/* FALL THOUGH */
    case CMS_ID_NAME: {
	hx509_name name;

	id->element = choice_CMSIdentifier_issuerAndSerialNumber;
	ret = hx509_cert_get_issuer(cert, &name);
	if (ret)
	    return ret;
	ret = hx509_name_to_Name(name, &id->u.issuerAndSerialNumber.issuer);
	hx509_name_free(&name);
	if (ret)
	    return ret;

	ret = hx509_cert_get_serialnumber(cert, &id->u.issuerAndSerialNumber.serialNumber);
	break;
    }
    default:
	_hx509_abort("CMS fill identifier with unknown type");
    }
    return ret;
}

static int
unparse_CMSIdentifier(hx509_context context,
		      CMSIdentifier *id,
		      char **str)
{
    int ret;

    *str = NULL;
    switch (id->element) {
    case choice_CMSIdentifier_issuerAndSerialNumber: {
	IssuerAndSerialNumber *iasn;
	char *serial, *name;

	iasn = &id->u.issuerAndSerialNumber;

	ret = _hx509_Name_to_string(&iasn->issuer, &name);
	if(ret)
	    return ret;
	ret = der_print_hex_heim_integer(&iasn->serialNumber, &serial);
	if (ret) {
	    free(name);
	    return ret;
	}
	asprintf(str, "certificate issued by %s with serial number %s",
		 name, serial);
	free(name);
	free(serial);
	break;
    }
    case choice_CMSIdentifier_subjectKeyIdentifier: {
	KeyIdentifier *ki  = &id->u.subjectKeyIdentifier;
	char *keyid;
	ssize_t len;

	len = hex_encode(ki->data, ki->length, &keyid);
	if (len < 0)
	    return ENOMEM;

	asprintf(str, "certificate with id %s", keyid);
	free(keyid);
	break;
    }
    default:
	asprintf(str, "certificate have unknown CMSidentifier type");
	break;
    }
    if (*str == NULL)
	return ENOMEM;
    return 0;
}

static int
find_CMSIdentifier(hx509_context context,
		   CMSIdentifier *client,
		   hx509_certs certs,
		   hx509_cert *signer_cert,
		   int match)
{
    hx509_query q;
    hx509_cert cert;
    Certificate c;
    int ret;

    memset(&c, 0, sizeof(c));
    _hx509_query_clear(&q);

    *signer_cert = NULL;

    switch (client->element) {
    case choice_CMSIdentifier_issuerAndSerialNumber:
	q.serial = &client->u.issuerAndSerialNumber.serialNumber;
	q.issuer_name = &client->u.issuerAndSerialNumber.issuer;
	q.match = HX509_QUERY_MATCH_SERIALNUMBER|HX509_QUERY_MATCH_ISSUER_NAME;
	break;
    case choice_CMSIdentifier_subjectKeyIdentifier:
	q.subject_id = &client->u.subjectKeyIdentifier;
	q.match = HX509_QUERY_MATCH_SUBJECT_KEY_ID;
	break;
    default:
	hx509_set_error_string(context, 0, HX509_CMS_NO_RECIPIENT_CERTIFICATE,
			       "unknown CMS identifier element");
	return HX509_CMS_NO_RECIPIENT_CERTIFICATE;
    }

    q.match |= match;

    q.match |= HX509_QUERY_MATCH_TIME;
    q.timenow = time(NULL);

    ret = hx509_certs_find(context, certs, &q, &cert);
    if (ret == HX509_CERT_NOT_FOUND) {
	char *str;

	ret = unparse_CMSIdentifier(context, client, &str);
	if (ret == 0) {
	    hx509_set_error_string(context, 0,
				   HX509_CMS_NO_RECIPIENT_CERTIFICATE,
				   "Failed to find %s", str);
	} else
	    hx509_clear_error_string(context);
	return HX509_CMS_NO_RECIPIENT_CERTIFICATE;
    } else if (ret) {
	hx509_set_error_string(context, HX509_ERROR_APPEND,
			       HX509_CMS_NO_RECIPIENT_CERTIFICATE,
			       "Failed to find CMS id in cert store");
	return HX509_CMS_NO_RECIPIENT_CERTIFICATE;
    }

    *signer_cert = cert;

    return 0;
}

/**
 * Decode and unencrypt EnvelopedData.
 *
 * Extract data and parameteres from from the EnvelopedData. Also
 * supports using detached EnvelopedData.
 *
 * @param context A hx509 context.
 * @param certs Certificate that can decrypt the EnvelopedData
 * encryption key.
 * @param flags HX509_CMS_UE flags to control the behavior.
 * @param data pointer the structure the contains the DER/BER encoded
 * EnvelopedData stucture.
 * @param length length of the data that data point to.
 * @param encryptedContent in case of detached signature, this
 * contains the actual encrypted data, othersize its should be NULL.
 * @param contentType output type oid, should be freed with der_free_oid().
 * @param content the data, free with der_free_octet_string().
 *
 * @ingroup hx509_cms
 */

int
hx509_cms_unenvelope(hx509_context context,
		     hx509_certs certs,
		     int flags,
		     const void *data,
		     size_t length,
		     const heim_octet_string *encryptedContent,
		     heim_oid *contentType,
		     heim_octet_string *content)
{
    heim_octet_string key;
    EnvelopedData ed;
    hx509_cert cert;
    AlgorithmIdentifier *ai;
    const heim_octet_string *enccontent;
    heim_octet_string *params, params_data;
    heim_octet_string ivec;
    size_t size;
    int ret, i, matched = 0, findflags = 0;


    memset(&key, 0, sizeof(key));
    memset(&ed, 0, sizeof(ed));
    memset(&ivec, 0, sizeof(ivec));
    memset(content, 0, sizeof(*content));
    memset(contentType, 0, sizeof(*contentType));

    if ((flags & HX509_CMS_UE_DONT_REQUIRE_KU_ENCIPHERMENT) == 0)
	findflags |= HX509_QUERY_KU_ENCIPHERMENT;

    ret = decode_EnvelopedData(data, length, &ed, &size);
    if (ret) {
	hx509_set_error_string(context, 0, ret,
			       "Failed to decode EnvelopedData");
	return ret;
    }

    if (ed.recipientInfos.len == 0) {
	ret = HX509_CMS_NO_RECIPIENT_CERTIFICATE;
	hx509_set_error_string(context, 0, ret,
			       "No recipient info in enveloped data");
	goto out;
    }

    enccontent = ed.encryptedContentInfo.encryptedContent;
    if (enccontent == NULL) {
	if (encryptedContent == NULL) {
	    ret = HX509_CMS_NO_DATA_AVAILABLE;
	    hx509_set_error_string(context, 0, ret,
				   "Content missing from encrypted data");
	    goto out;
	}
	enccontent = encryptedContent;
    } else if (encryptedContent != NULL) {
	ret = HX509_CMS_NO_DATA_AVAILABLE;
	hx509_set_error_string(context, 0, ret,
			       "Both internal and external encrypted data");
	goto out;
    }

    cert = NULL;
    for (i = 0; i < ed.recipientInfos.len; i++) {
	KeyTransRecipientInfo *ri;
	char *str;
	int ret2;

	ri = &ed.recipientInfos.val[i];

	ret = find_CMSIdentifier(context, &ri->rid, certs, &cert,
				 HX509_QUERY_PRIVATE_KEY|findflags);
	if (ret)
	    continue;

	matched = 1; /* found a matching certificate, let decrypt */

	ret = _hx509_cert_private_decrypt(context,
					  &ri->encryptedKey,
					  &ri->keyEncryptionAlgorithm.algorithm,
					  cert, &key);

	hx509_cert_free(cert);
	if (ret == 0)
	    break; /* succuessfully decrypted cert */
	cert = NULL;
	ret2 = unparse_CMSIdentifier(context, &ri->rid, &str);
	if (ret2 == 0) {
	    hx509_set_error_string(context, HX509_ERROR_APPEND, ret,
				   "Failed to decrypt with %s", str);
	    free(str);
	}
    }

    if (!matched) {
	ret = HX509_CMS_NO_RECIPIENT_CERTIFICATE;
	hx509_set_error_string(context, 0, ret,
			       "No private key matched any certificate");
	goto out;
    }

    if (cert == NULL) {
	ret = HX509_CMS_NO_RECIPIENT_CERTIFICATE;
	hx509_set_error_string(context, HX509_ERROR_APPEND, ret,
			       "No private key decrypted the transfer key");
	goto out;
    }

    ret = der_copy_oid(&ed.encryptedContentInfo.contentType, contentType);
    if (ret) {
	hx509_set_error_string(context, 0, ret,
			       "Failed to copy EnvelopedData content oid");
	goto out;
    }

    ai = &ed.encryptedContentInfo.contentEncryptionAlgorithm;
    if (ai->parameters) {
	params_data.data = ai->parameters->data;
	params_data.length = ai->parameters->length;
	params = &params_data;
    } else
	params = NULL;

    {
	hx509_crypto crypto;

	ret = hx509_crypto_init(context, NULL, &ai->algorithm, &crypto);
	if (ret)
	    goto out;
	
	if (params) {
	    ret = hx509_crypto_set_params(context, crypto, params, &ivec);
	    if (ret) {
		hx509_crypto_destroy(crypto);
		goto out;
	    }
	}

	ret = hx509_crypto_set_key_data(crypto, key.data, key.length);
	if (ret) {
	    hx509_crypto_destroy(crypto);
	    hx509_set_error_string(context, 0, ret,
				   "Failed to set key for decryption "
				   "of EnvelopedData");
	    goto out;
	}
	
	ret = hx509_crypto_decrypt(crypto,
				   enccontent->data,
				   enccontent->length,
				   ivec.length ? &ivec : NULL,
				   content);
	hx509_crypto_destroy(crypto);
	if (ret) {
	    hx509_set_error_string(context, 0, ret,
				   "Failed to decrypt EnvelopedData");
	    goto out;
	}
    }

out:

    free_EnvelopedData(&ed);
    der_free_octet_string(&key);
    if (ivec.length)
	der_free_octet_string(&ivec);
    if (ret) {
	der_free_oid(contentType);
	der_free_octet_string(content);
    }

    return ret;
}

/**
 * Encrypt end encode EnvelopedData.
 *
 * Encrypt and encode EnvelopedData. The data is encrypted with a
 * random key and the the random key is encrypted with the
 * certificates private key. This limits what private key type can be
 * used to RSA.
 *
 * @param context A hx509 context.
 * @param flags flags to control the behavior, no flags today
 * @param cert Certificate to encrypt the EnvelopedData encryption key
 * with.
 * @param data pointer the data to encrypt.
 * @param length length of the data that data point to.
 * @param encryption_type Encryption cipher to use for the bulk data,
 * use NULL to get default.
 * @param contentType type of the data that is encrypted
 * @param content the output of the function,
 * free with der_free_octet_string().
 *
 * @ingroup hx509_cms
 */

int
hx509_cms_envelope_1(hx509_context context,
		     int flags,
		     hx509_cert cert,
		     const void *data,
		     size_t length,
		     const heim_oid *encryption_type,
		     const heim_oid *contentType,
		     heim_octet_string *content)
{
    KeyTransRecipientInfo *ri;
    heim_octet_string ivec;
    heim_octet_string key;
    hx509_crypto crypto = NULL;
    EnvelopedData ed;
    size_t size;
    int ret;

    memset(&ivec, 0, sizeof(ivec));
    memset(&key, 0, sizeof(key));
    memset(&ed, 0, sizeof(ed));
    memset(content, 0, sizeof(*content));

    if (encryption_type == NULL)
	encryption_type = oid_id_aes_256_cbc();

    ret = _hx509_check_key_usage(context, cert, 1 << 2, TRUE);
    if (ret)
	goto out;

    ret = hx509_crypto_init(context, NULL, encryption_type, &crypto);
    if (ret)
	goto out;

    ret = hx509_crypto_set_random_key(crypto, &key);
    if (ret) {
	hx509_set_error_string(context, 0, ret,
			       "Create random key for EnvelopedData content");
	goto out;
    }

    ret = hx509_crypto_random_iv(crypto, &ivec);
    if (ret) {
	hx509_set_error_string(context, 0, ret,
			       "Failed to create a random iv");
	goto out;
    }

    ret = hx509_crypto_encrypt(crypto,
			       data,
			       length,
			       &ivec,
			       &ed.encryptedContentInfo.encryptedContent);
    if (ret) {
	hx509_set_error_string(context, 0, ret,
			       "Failed to encrypt EnvelopedData content");
	goto out;
    }

    {
	AlgorithmIdentifier *enc_alg;
	enc_alg = &ed.encryptedContentInfo.contentEncryptionAlgorithm;
	ret = der_copy_oid(encryption_type, &enc_alg->algorithm);
	if (ret) {
	    hx509_set_error_string(context, 0, ret,
				   "Failed to set crypto oid "
				   "for EnvelopedData");
	    goto out;
	}	
	ALLOC(enc_alg->parameters, 1);
	if (enc_alg->parameters == NULL) {
	    ret = ENOMEM;
	    hx509_set_error_string(context, 0, ret,
				   "Failed to allocate crypto paramaters "
				   "for EnvelopedData");
	    goto out;
	}

	ret = hx509_crypto_get_params(context,
				      crypto,
				      &ivec,
				      enc_alg->parameters);
	if (ret) {
	    goto out;
	}
    }

    ALLOC_SEQ(&ed.recipientInfos, 1);
    if (ed.recipientInfos.val == NULL) {
	ret = ENOMEM;
	hx509_set_error_string(context, 0, ret,
			       "Failed to allocate recipients info "
			       "for EnvelopedData");
	goto out;
    }

    ri = &ed.recipientInfos.val[0];

    ri->version = 0;
    ret = fill_CMSIdentifier(cert, CMS_ID_SKI, &ri->rid);
    if (ret) {
	hx509_set_error_string(context, 0, ret,
			       "Failed to set CMS identifier info "
			       "for EnvelopedData");
	goto out;
    }

    ret = _hx509_cert_public_encrypt(context,
				     &key, cert,
				     &ri->keyEncryptionAlgorithm.algorithm,
				     &ri->encryptedKey);
    if (ret) {
	hx509_set_error_string(context, HX509_ERROR_APPEND, ret,
			       "Failed to encrypt transport key for "
			       "EnvelopedData");
	goto out;
    }

    /*
     *
     */

    ed.version = 0;
    ed.originatorInfo = NULL;

    ret = der_copy_oid(contentType, &ed.encryptedContentInfo.contentType);
    if (ret) {
	hx509_set_error_string(context, 0, ret,
			       "Failed to copy content oid for "
			       "EnvelopedData");
	goto out;
    }

    ed.unprotectedAttrs = NULL;

    ASN1_MALLOC_ENCODE(EnvelopedData, content->data, content->length,
		       &ed, &size, ret);
    if (ret) {
	hx509_set_error_string(context, 0, ret,
			       "Failed to encode EnvelopedData");
	goto out;
    }
    if (size != content->length)
	_hx509_abort("internal ASN.1 encoder error");

out:
    if (crypto)
	hx509_crypto_destroy(crypto);
    if (ret)
	der_free_octet_string(content);
    der_free_octet_string(&key);
    der_free_octet_string(&ivec);
    free_EnvelopedData(&ed);

    return ret;
}

static int
any_to_certs(hx509_context context, const SignedData *sd, hx509_certs certs)
{
    int ret, i;

    if (sd->certificates == NULL)
	return 0;

    for (i = 0; i < sd->certificates->len; i++) {
	hx509_cert c;

	ret = hx509_cert_init_data(context, 
				   sd->certificates->val[i].data, 
				   sd->certificates->val[i].length,
				   &c);
	if (ret)
	    return ret;
	ret = hx509_certs_add(context, certs, c);
	hx509_cert_free(c);
	if (ret)
	    return ret;
    }

    return 0;
}

static const Attribute *
find_attribute(const CMSAttributes *attr, const heim_oid *oid)
{
    int i;
    for (i = 0; i < attr->len; i++)
	if (der_heim_oid_cmp(&attr->val[i].type, oid) == 0)
	    return &attr->val[i];
    return NULL;
}

/**
 * Decode SignedData and verify that the signature is correct.
 *
 * @param context A hx509 context.
 * @param ctx a hx509 version context
 * @param data
 * @param length length of the data that data point to.
 * @param signedContent
 * @param pool certificate pool to build certificates paths.
 * @param contentType free with der_free_oid()
 * @param content the output of the function, free with
 * der_free_octet_string().
 * @param signer_certs list of the cerficates used to sign this
 * request, free with hx509_certs_free().
 *
 * @ingroup hx509_cms
 */

int
hx509_cms_verify_signed(hx509_context context,
			hx509_verify_ctx ctx,
			const void *data,
			size_t length,
			const heim_octet_string *signedContent,
			hx509_certs pool,
			heim_oid *contentType,
			heim_octet_string *content,
			hx509_certs *signer_certs)
{
    SignerInfo *signer_info;
    hx509_cert cert = NULL;
    hx509_certs certs = NULL;
    SignedData sd;
    size_t size;
    int ret, i, found_valid_sig;

    *signer_certs = NULL;
    content->data = NULL;
    content->length = 0;
    contentType->length = 0;
    contentType->components = NULL;

    memset(&sd, 0, sizeof(sd));

    ret = decode_SignedData(data, length, &sd, &size);
    if (ret) {
	hx509_set_error_string(context, 0, ret,
			       "Failed to decode SignedData");
	goto out;
    }

    if (sd.encapContentInfo.eContent == NULL && signedContent == NULL) {
	ret = HX509_CMS_NO_DATA_AVAILABLE;
	hx509_set_error_string(context, 0, ret,
			       "No content data in SignedData");
	goto out;
    }
    if (sd.encapContentInfo.eContent && signedContent) {
	ret = HX509_CMS_NO_DATA_AVAILABLE;
	hx509_set_error_string(context, 0, ret,
			       "Both external and internal SignedData");
	goto out;
    }
    if (sd.encapContentInfo.eContent)
	signedContent = sd.encapContentInfo.eContent;

    ret = hx509_certs_init(context, "MEMORY:cms-cert-buffer",
			   0, NULL, &certs);
    if (ret)
	goto out;

    ret = hx509_certs_init(context, "MEMORY:cms-signer-certs",
			   0, NULL, signer_certs);
    if (ret)
	goto out;

    /* XXX Check CMS version */

    ret = any_to_certs(context, &sd, certs);
    if (ret)
	goto out;

    if (pool) {
	ret = hx509_certs_merge(context, certs, pool);
	if (ret)
	    goto out;
    }

    for (found_valid_sig = 0, i = 0; i < sd.signerInfos.len; i++) {
	heim_octet_string *signed_data;
	const heim_oid *match_oid;
	heim_oid decode_oid;

	signer_info = &sd.signerInfos.val[i];
	match_oid = NULL;

	if (signer_info->signature.length == 0) {
	    ret = HX509_CMS_MISSING_SIGNER_DATA;
	    hx509_set_error_string(context, 0, ret,
				   "SignerInfo %d in SignedData "
				   "missing sigature", i);
	    continue;
	}

	ret = find_CMSIdentifier(context, &signer_info->sid, certs, &cert,
				 HX509_QUERY_KU_DIGITALSIGNATURE);
	if (ret)
	    continue;

	if (signer_info->signedAttrs) {
	    const Attribute *attr;
	
	    CMSAttributes sa;
	    heim_octet_string os;

	    sa.val = signer_info->signedAttrs->val;
	    sa.len = signer_info->signedAttrs->len;

	    /* verify that sigature exists */
	    attr = find_attribute(&sa, oid_id_pkcs9_messageDigest());
	    if (attr == NULL) {
		ret = HX509_CRYPTO_SIGNATURE_MISSING;
		hx509_set_error_string(context, 0, ret,
				       "SignerInfo have signed attributes "
				       "but messageDigest (signature) "
				       "is missing");
		goto next_sigature;
	    }
	    if (attr->value.len != 1) {
		ret = HX509_CRYPTO_SIGNATURE_MISSING;
		hx509_set_error_string(context, 0, ret,
				       "SignerInfo have more then one "
				       "messageDigest (signature)");
		goto next_sigature;
	    }
	
	    ret = decode_MessageDigest(attr->value.val[0].data,
				       attr->value.val[0].length,
				       &os,
				       &size);
	    if (ret) {
		hx509_set_error_string(context, 0, ret,
				       "Failed to decode "
				       "messageDigest (signature)");
		goto next_sigature;
	    }

	    ret = _hx509_verify_signature(context,
					  NULL,
					  &signer_info->digestAlgorithm,
					  signedContent,
					  &os);
	    der_free_octet_string(&os);
	    if (ret) {
		hx509_set_error_string(context, HX509_ERROR_APPEND, ret,
				       "Failed to verify messageDigest");
		goto next_sigature;
	    }

	    /*
	     * Fetch content oid inside signedAttrs or set it to
	     * id-pkcs7-data.
	     */
	    attr = find_attribute(&sa, oid_id_pkcs9_contentType());
	    if (attr == NULL) {
		match_oid = oid_id_pkcs7_data();
	    } else {
		if (attr->value.len != 1) {
		    ret = HX509_CMS_DATA_OID_MISMATCH;
		    hx509_set_error_string(context, 0, ret,
					   "More then one oid in signedAttrs");
		    goto next_sigature;

		}
		ret = decode_ContentType(attr->value.val[0].data,
					 attr->value.val[0].length,
					 &decode_oid,
					 &size);
		if (ret) {
		    hx509_set_error_string(context, 0, ret,
					   "Failed to decode "
					   "oid in signedAttrs");
		    goto next_sigature;
		}
		match_oid = &decode_oid;
	    }

	    ALLOC(signed_data, 1);
	    if (signed_data == NULL) {
		if (match_oid == &decode_oid)
		    der_free_oid(&decode_oid);
		ret = ENOMEM;
		hx509_clear_error_string(context);
		goto next_sigature;
	    }
	
	    ASN1_MALLOC_ENCODE(CMSAttributes,
			       signed_data->data,
			       signed_data->length,
			       &sa,
			       &size, ret);
	    if (ret) {
		if (match_oid == &decode_oid)
		    der_free_oid(&decode_oid);
		free(signed_data);
		hx509_clear_error_string(context);
		goto next_sigature;
	    }
	    if (size != signed_data->length)
		_hx509_abort("internal ASN.1 encoder error");

	} else {
	    signed_data = rk_UNCONST(signedContent);
	    match_oid = oid_id_pkcs7_data();
	}

	if (der_heim_oid_cmp(match_oid, &sd.encapContentInfo.eContentType)) {
	    ret = HX509_CMS_DATA_OID_MISMATCH;
	    hx509_set_error_string(context, 0, ret,
				   "Oid in message mismatch from the expected");
	}
	if (match_oid == &decode_oid)
	    der_free_oid(&decode_oid);

	if (ret == 0) {
	    ret = hx509_verify_signature(context,
					 cert,
					 &signer_info->signatureAlgorithm,
					 signed_data,
					 &signer_info->signature);
	    if (ret)
		hx509_set_error_string(context, HX509_ERROR_APPEND, ret,
				       "Failed to verify sigature in "
				       "CMS SignedData");
	}
	if (signed_data != signedContent) {
	    der_free_octet_string(signed_data);
	    free(signed_data);
	}
	if (ret)
	    goto next_sigature;

	ret = hx509_verify_path(context, ctx, cert, certs);
	if (ret)
	    goto next_sigature;

	ret = hx509_certs_add(context, *signer_certs, cert);
	if (ret)
	    goto next_sigature;

	found_valid_sig++;

    next_sigature:
	if (cert)
	    hx509_cert_free(cert);
	cert = NULL;
    }
    if (found_valid_sig == 0) {
	if (ret == 0) {
	    ret = HX509_CMS_SIGNER_NOT_FOUND;
	    hx509_set_error_string(context, 0, ret,
				   "No signers where found");
	}
	goto out;
    }

    ret = der_copy_oid(&sd.encapContentInfo.eContentType, contentType);
    if (ret) {
	hx509_clear_error_string(context);
	goto out;
    }

    content->data = malloc(signedContent->length);
    if (content->data == NULL) {
	hx509_clear_error_string(context);
	ret = ENOMEM;
	goto out;
    }
    content->length = signedContent->length;
    memcpy(content->data, signedContent->data, content->length);

out:
    free_SignedData(&sd);
    if (certs)
	hx509_certs_free(&certs);
    if (ret) {
	if (*signer_certs)
	    hx509_certs_free(signer_certs);
	der_free_oid(contentType);
	der_free_octet_string(content);
    }

    return ret;
}

static int
add_one_attribute(Attribute **attr,
		  unsigned int *len,
		  const heim_oid *oid,
		  heim_octet_string *data)
{
    void *d;
    int ret;

    d = realloc(*attr, sizeof((*attr)[0]) * (*len + 1));
    if (d == NULL)
	return ENOMEM;
    (*attr) = d;

    ret = der_copy_oid(oid, &(*attr)[*len].type);
    if (ret)
	return ret;

    ALLOC_SEQ(&(*attr)[*len].value, 1);
    if ((*attr)[*len].value.val == NULL) {
	der_free_oid(&(*attr)[*len].type);
	return ENOMEM;
    }

    (*attr)[*len].value.val[0].data = data->data;
    (*attr)[*len].value.val[0].length = data->length;

    *len += 1;

    return 0;
}
	
/**
 * Decode SignedData and verify that the signature is correct.
 *
 * @param context A hx509 context.
 * @param flags
 * @param eContentType the type of the data.
 * @param data data to sign
 * @param length length of the data that data point to.
 * @param digest_alg digest algorithm to use, use NULL to get the
 * default or the peer determined algorithm.
 * @param cert certificate to use for sign the data.
 * @param peer info about the peer the message to send the message to,
 * like what digest algorithm to use.
 * @param anchors trust anchors that the client will use, used to
 * polulate the certificates included in the message
 * @param pool certificates to use in try to build the path to the
 * trust anchors.
 * @param signed_data the output of the function, free with
 * der_free_octet_string().
 *
 * @ingroup hx509_cms
 */

int
hx509_cms_create_signed_1(hx509_context context,
			  int flags,
			  const heim_oid *eContentType,
			  const void *data, size_t length,
			  const AlgorithmIdentifier *digest_alg,
			  hx509_cert cert,
			  hx509_peer_info peer,
			  hx509_certs anchors,
			  hx509_certs pool,
			  heim_octet_string *signed_data)
{
    AlgorithmIdentifier digest;
    hx509_name name;
    SignerInfo *signer_info;
    heim_octet_string buf, content, sigdata = { 0, NULL };
    SignedData sd;
    int ret;
    size_t size;
    hx509_path path;
    int cmsidflag = CMS_ID_SKI;

    memset(&sd, 0, sizeof(sd));
    memset(&name, 0, sizeof(name));
    memset(&path, 0, sizeof(path));
    memset(&digest, 0, sizeof(digest));

    content.data = rk_UNCONST(data);
    content.length = length;

    if (flags & HX509_CMS_SIGATURE_ID_NAME)
	cmsidflag = CMS_ID_NAME;

    if (_hx509_cert_private_key(cert) == NULL) {
	hx509_set_error_string(context, 0, HX509_PRIVATE_KEY_MISSING,
			       "Private key missing for signing");
	return HX509_PRIVATE_KEY_MISSING;
    }

    if (digest_alg == NULL) {
	ret = hx509_crypto_select(context, HX509_SELECT_DIGEST,
				  _hx509_cert_private_key(cert), peer, &digest);
    } else {
	ret = copy_AlgorithmIdentifier(digest_alg, &digest);
	if (ret)
	    hx509_clear_error_string(context);
    }
    if (ret)
	goto out;

    sd.version = CMSVersion_v3;

    if (eContentType == NULL)
	eContentType = oid_id_pkcs7_data();

    der_copy_oid(eContentType, &sd.encapContentInfo.eContentType);

    /* */
    if ((flags & HX509_CMS_SIGATURE_DETACHED) == 0) {
	ALLOC(sd.encapContentInfo.eContent, 1);
	if (sd.encapContentInfo.eContent == NULL) {
	    hx509_clear_error_string(context);
	    ret = ENOMEM;
	    goto out;
	}
	
	sd.encapContentInfo.eContent->data = malloc(length);
	if (sd.encapContentInfo.eContent->data == NULL) {
	    hx509_clear_error_string(context);
	    ret = ENOMEM;
	    goto out;
	}
	memcpy(sd.encapContentInfo.eContent->data, data, length);
	sd.encapContentInfo.eContent->length = length;
    }

    ALLOC_SEQ(&sd.signerInfos, 1);
    if (sd.signerInfos.val == NULL) {
	hx509_clear_error_string(context);
	ret = ENOMEM;
	goto out;
    }

    signer_info = &sd.signerInfos.val[0];

    signer_info->version = 1;

    ret = fill_CMSIdentifier(cert, cmsidflag, &signer_info->sid);
    if (ret) {
	hx509_clear_error_string(context);
	goto out;
    }			

    signer_info->signedAttrs = NULL;
    signer_info->unsignedAttrs = NULL;


    ret = copy_AlgorithmIdentifier(&digest, &signer_info->digestAlgorithm);
    if (ret) {
	hx509_clear_error_string(context);
	goto out;
    }

    /*
     * If it isn't pkcs7-data send signedAttributes
     */

    if (der_heim_oid_cmp(eContentType, oid_id_pkcs7_data()) != 0) {
	CMSAttributes sa;	
	heim_octet_string sig;

	ALLOC(signer_info->signedAttrs, 1);
	if (signer_info->signedAttrs == NULL) {
	    ret = ENOMEM;
	    goto out;
	}

	ret = _hx509_create_signature(context,
				      NULL,
				      &digest,
				      &content,
				      NULL,
				      &sig);
	if (ret)
	    goto out;

	ASN1_MALLOC_ENCODE(MessageDigest,
			   buf.data,
			   buf.length,
			   &sig,
			   &size,
			   ret);
	der_free_octet_string(&sig);
	if (ret) {
	    hx509_clear_error_string(context);
	    goto out;
	}
	if (size != buf.length)
	    _hx509_abort("internal ASN.1 encoder error");

	ret = add_one_attribute(&signer_info->signedAttrs->val,
				&signer_info->signedAttrs->len,
				oid_id_pkcs9_messageDigest(),
				&buf);
	if (ret) {
	    hx509_clear_error_string(context);
	    goto out;
	}


	ASN1_MALLOC_ENCODE(ContentType,
			   buf.data,
			   buf.length,
			   eContentType,
			   &size,
			   ret);
	if (ret)
	    goto out;
	if (size != buf.length)
	    _hx509_abort("internal ASN.1 encoder error");

	ret = add_one_attribute(&signer_info->signedAttrs->val,
				&signer_info->signedAttrs->len,
				oid_id_pkcs9_contentType(),
				&buf);
	if (ret) {
	    hx509_clear_error_string(context);
	    goto out;
	}

	sa.val = signer_info->signedAttrs->val;
	sa.len = signer_info->signedAttrs->len;
	
	ASN1_MALLOC_ENCODE(CMSAttributes,
			   sigdata.data,
			   sigdata.length,
			   &sa,
			   &size,
			   ret);
	if (ret) {
	    hx509_clear_error_string(context);
	    goto out;
	}
	if (size != sigdata.length)
	    _hx509_abort("internal ASN.1 encoder error");
    } else {
	sigdata.data = content.data;
	sigdata.length = content.length;
    }


    {
	AlgorithmIdentifier sigalg;

	ret = hx509_crypto_select(context, HX509_SELECT_PUBLIC_SIG,
				  _hx509_cert_private_key(cert), peer,
				  &sigalg);
	if (ret)
	    goto out;

	ret = _hx509_create_signature(context,
				      _hx509_cert_private_key(cert),
				      &sigalg,
				      &sigdata,
				      &signer_info->signatureAlgorithm,
				      &signer_info->signature);
	free_AlgorithmIdentifier(&sigalg);
	if (ret)
	    goto out;
    }

    ALLOC_SEQ(&sd.digestAlgorithms, 1);
    if (sd.digestAlgorithms.val == NULL) {
	ret = ENOMEM;
	hx509_clear_error_string(context);
	goto out;
    }

    ret = copy_AlgorithmIdentifier(&digest, &sd.digestAlgorithms.val[0]);
    if (ret) {
	hx509_clear_error_string(context);
	goto out;
    }

    /*
     * Provide best effort path
     */
    if (pool) {
	_hx509_calculate_path(context,
			      HX509_CALCULATE_PATH_NO_ANCHOR,			      
			      time(NULL),
			      anchors,
			      0,
			      cert,
			      pool,
			      &path);
    } else
	_hx509_path_append(context, &path, cert);


    if (path.len) {
	int i;

	ALLOC(sd.certificates, 1);
	if (sd.certificates == NULL) {
	    hx509_clear_error_string(context);
	    ret = ENOMEM;
	    goto out;
	}
	ALLOC_SEQ(sd.certificates, path.len);
	if (sd.certificates->val == NULL) {
	    hx509_clear_error_string(context);
	    ret = ENOMEM;
	    goto out;
	}

	for (i = 0; i < path.len; i++) {
	    ret = hx509_cert_binary(context, path.val[i],
				    &sd.certificates->val[i]);
	    if (ret) {
		hx509_clear_error_string(context);
		goto out;
	    }
	}
    }

    ASN1_MALLOC_ENCODE(SignedData,
		       signed_data->data, signed_data->length,
		       &sd, &size, ret);
    if (ret) {
	hx509_clear_error_string(context);
	goto out;
    }
    if (signed_data->length != size)
	_hx509_abort("internal ASN.1 encoder error");

out:
    if (sigdata.data != content.data)
	der_free_octet_string(&sigdata);
    free_AlgorithmIdentifier(&digest);
    _hx509_path_free(&path);
    free_SignedData(&sd);

    return ret;
}

int
hx509_cms_decrypt_encrypted(hx509_context context,
			    hx509_lock lock,
			    const void *data,
			    size_t length,
			    heim_oid *contentType,
			    heim_octet_string *content)
{
    heim_octet_string cont;
    CMSEncryptedData ed;
    AlgorithmIdentifier *ai;
    int ret;

    memset(content, 0, sizeof(*content));
    memset(&cont, 0, sizeof(cont));

    ret = decode_CMSEncryptedData(data, length, &ed, NULL);
    if (ret) {
	hx509_set_error_string(context, 0, ret,
			       "Failed to decode CMSEncryptedData");
	return ret;
    }

    if (ed.encryptedContentInfo.encryptedContent == NULL) {
	ret = HX509_CMS_NO_DATA_AVAILABLE;
	hx509_set_error_string(context, 0, ret,
			       "No content in EncryptedData");
	goto out;
    }

    ret = der_copy_oid(&ed.encryptedContentInfo.contentType, contentType);
    if (ret) {
	hx509_clear_error_string(context);
	goto out;
    }

    ai = &ed.encryptedContentInfo.contentEncryptionAlgorithm;
    if (ai->parameters == NULL) {
	ret = HX509_ALG_NOT_SUPP;
	hx509_clear_error_string(context);
	goto out;
    }

    ret = _hx509_pbe_decrypt(context,
			     lock,
			     ai,
			     ed.encryptedContentInfo.encryptedContent,
			     &cont);
    if (ret)
	goto out;

    *content = cont;

out:
    if (ret) {
	if (cont.data)
	    free(cont.data);
    }
    free_CMSEncryptedData(&ed);
    return ret;
}
