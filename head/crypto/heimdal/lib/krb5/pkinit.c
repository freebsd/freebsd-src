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

#include "krb5_locl.h"

RCSID("$Id: pkinit.c 22433 2008-01-13 14:11:46Z lha $");

struct krb5_dh_moduli {
    char *name;
    unsigned long bits;
    heim_integer p;
    heim_integer g;
    heim_integer q;
};

#ifdef PKINIT

#include <heim_asn1.h>
#include <rfc2459_asn1.h>
#include <cms_asn1.h>
#include <pkcs8_asn1.h>
#include <pkcs9_asn1.h>
#include <pkcs12_asn1.h>
#include <pkinit_asn1.h>
#include <asn1_err.h>

#include <der.h>

#include <hx509.h>

enum {
    COMPAT_WIN2K = 1,
    COMPAT_IETF = 2
};

struct krb5_pk_identity {
    hx509_context hx509ctx;
    hx509_verify_ctx verify_ctx;
    hx509_certs certs;
    hx509_certs anchors;
    hx509_certs certpool;
    hx509_revoke_ctx revokectx;
};

struct krb5_pk_cert {
    hx509_cert cert;
};

struct krb5_pk_init_ctx_data {
    struct krb5_pk_identity *id;
    DH *dh;
    krb5_data *clientDHNonce;
    struct krb5_dh_moduli **m;
    hx509_peer_info peer;
    int type;
    unsigned int require_binding:1;
    unsigned int require_eku:1;
    unsigned int require_krbtgt_otherName:1;
    unsigned int require_hostname_match:1;
    unsigned int trustedCertifiers:1;
};

static void
_krb5_pk_copy_error(krb5_context context,
		    hx509_context hx509ctx,
		    int hxret,
		    const char *fmt,
		    ...)
    __attribute__ ((format (printf, 4, 5)));

/*
 *
 */

void KRB5_LIB_FUNCTION
_krb5_pk_cert_free(struct krb5_pk_cert *cert)
{
    if (cert->cert) {
	hx509_cert_free(cert->cert);
    }
    free(cert);
}

static krb5_error_code
BN_to_integer(krb5_context context, BIGNUM *bn, heim_integer *integer)
{
    integer->length = BN_num_bytes(bn);
    integer->data = malloc(integer->length);
    if (integer->data == NULL) {
	krb5_clear_error_string(context);
	return ENOMEM;
    }
    BN_bn2bin(bn, integer->data);
    integer->negative = BN_is_negative(bn);
    return 0;
}

static BIGNUM *
integer_to_BN(krb5_context context, const char *field, const heim_integer *f)
{
    BIGNUM *bn;

    bn = BN_bin2bn((const unsigned char *)f->data, f->length, NULL);
    if (bn == NULL) {
	krb5_set_error_string(context, "PKINIT: parsing BN failed %s", field);
	return NULL;
    }
    BN_set_negative(bn, f->negative);
    return bn;
}


static krb5_error_code
_krb5_pk_create_sign(krb5_context context,
		     const heim_oid *eContentType,
		     krb5_data *eContent,
		     struct krb5_pk_identity *id,
		     hx509_peer_info peer,
		     krb5_data *sd_data)
{
    hx509_cert cert;
    hx509_query *q;
    int ret;

    ret = hx509_query_alloc(id->hx509ctx, &q);
    if (ret) {
	_krb5_pk_copy_error(context, id->hx509ctx, ret, 
			    "Allocate query to find signing certificate");
	return ret;
    }

    hx509_query_match_option(q, HX509_QUERY_OPTION_PRIVATE_KEY);
    hx509_query_match_option(q, HX509_QUERY_OPTION_KU_DIGITALSIGNATURE);

    ret = hx509_certs_find(id->hx509ctx, id->certs, q, &cert);
    hx509_query_free(id->hx509ctx, q);
    if (ret) {
	_krb5_pk_copy_error(context, id->hx509ctx, ret, 
			    "Find certificate to signed CMS data");
	return ret;
    }

    ret = hx509_cms_create_signed_1(id->hx509ctx,
				    0,
				    eContentType,
				    eContent->data,
				    eContent->length,
				    NULL,
				    cert,
				    peer,
				    NULL,
				    id->certs,
				    sd_data);
    if (ret)
	_krb5_pk_copy_error(context, id->hx509ctx, ret, "create CMS signedData");
    hx509_cert_free(cert);

    return ret;
}

static int
cert2epi(hx509_context context, void *ctx, hx509_cert c)
{
    ExternalPrincipalIdentifiers *ids = ctx;
    ExternalPrincipalIdentifier id;
    hx509_name subject = NULL;
    void *p;
    int ret;

    memset(&id, 0, sizeof(id));

    ret = hx509_cert_get_subject(c, &subject);
    if (ret)
	return ret;

    if (hx509_name_is_null_p(subject) != 0) {

	id.subjectName = calloc(1, sizeof(*id.subjectName));
	if (id.subjectName == NULL) {
	    hx509_name_free(&subject);
	    free_ExternalPrincipalIdentifier(&id);
	    return ENOMEM;
	}
    
	ret = hx509_name_binary(subject, id.subjectName);
	if (ret) {
	    hx509_name_free(&subject);
	    free_ExternalPrincipalIdentifier(&id);
	    return ret;
	}
    }
    hx509_name_free(&subject);


    id.issuerAndSerialNumber = calloc(1, sizeof(*id.issuerAndSerialNumber));
    if (id.issuerAndSerialNumber == NULL) {
	free_ExternalPrincipalIdentifier(&id);
	return ENOMEM;
    }

    {
	IssuerAndSerialNumber iasn;
	hx509_name issuer;
	size_t size;
	
	memset(&iasn, 0, sizeof(iasn));

	ret = hx509_cert_get_issuer(c, &issuer);
	if (ret) {
	    free_ExternalPrincipalIdentifier(&id);
	    return ret;
	}

	ret = hx509_name_to_Name(issuer, &iasn.issuer);
	hx509_name_free(&issuer);
	if (ret) {
	    free_ExternalPrincipalIdentifier(&id);
	    return ret;
	}
	
	ret = hx509_cert_get_serialnumber(c, &iasn.serialNumber);
	if (ret) {
	    free_IssuerAndSerialNumber(&iasn);
	    free_ExternalPrincipalIdentifier(&id);
	    return ret;
	}

	ASN1_MALLOC_ENCODE(IssuerAndSerialNumber,
			   id.issuerAndSerialNumber->data, 
			   id.issuerAndSerialNumber->length,
			   &iasn, &size, ret);
	free_IssuerAndSerialNumber(&iasn);
	if (ret)
	    return ret;
	if (id.issuerAndSerialNumber->length != size)
	    abort();
    }

    id.subjectKeyIdentifier = NULL;

    p = realloc(ids->val, sizeof(ids->val[0]) * (ids->len + 1)); 
    if (p == NULL) {
	free_ExternalPrincipalIdentifier(&id);
	return ENOMEM;
    }

    ids->val = p;
    ids->val[ids->len] = id;
    ids->len++;

    return 0;
}

static krb5_error_code
build_edi(krb5_context context,
	  hx509_context hx509ctx,
	  hx509_certs certs,
	  ExternalPrincipalIdentifiers *ids)
{
    return hx509_certs_iter(hx509ctx, certs, cert2epi, ids);
}

static krb5_error_code
build_auth_pack(krb5_context context,
		unsigned nonce,
		krb5_pk_init_ctx ctx,
		DH *dh,
		const KDC_REQ_BODY *body,
		AuthPack *a)
{
    size_t buf_size, len;
    krb5_error_code ret;
    void *buf;
    krb5_timestamp sec;
    int32_t usec;
    Checksum checksum;

    krb5_clear_error_string(context);

    memset(&checksum, 0, sizeof(checksum));

    krb5_us_timeofday(context, &sec, &usec);
    a->pkAuthenticator.ctime = sec;
    a->pkAuthenticator.nonce = nonce;

    ASN1_MALLOC_ENCODE(KDC_REQ_BODY, buf, buf_size, body, &len, ret);
    if (ret)
	return ret;
    if (buf_size != len)
	krb5_abortx(context, "internal error in ASN.1 encoder");

    ret = krb5_create_checksum(context,
			       NULL,
			       0,
			       CKSUMTYPE_SHA1,
			       buf,
			       len,
			       &checksum);
    free(buf);
    if (ret) 
	return ret;

    ALLOC(a->pkAuthenticator.paChecksum, 1);
    if (a->pkAuthenticator.paChecksum == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }

    ret = krb5_data_copy(a->pkAuthenticator.paChecksum,
			 checksum.checksum.data, checksum.checksum.length);
    free_Checksum(&checksum);
    if (ret)
	return ret;

    if (dh) {
	DomainParameters dp;
	heim_integer dh_pub_key;
	krb5_data dhbuf;
	size_t size;

	if (1 /* support_cached_dh */) {
	    ALLOC(a->clientDHNonce, 1);
	    if (a->clientDHNonce == NULL) {
		krb5_clear_error_string(context);
		return ENOMEM;
	    }
	    ret = krb5_data_alloc(a->clientDHNonce, 40);
	    if (a->clientDHNonce == NULL) {
		krb5_clear_error_string(context);
		return ENOMEM;
	    }
	    memset(a->clientDHNonce->data, 0, a->clientDHNonce->length);
	    ret = krb5_copy_data(context, a->clientDHNonce, 
				 &ctx->clientDHNonce);
	    if (ret)
		return ret;
	}

	ALLOC(a->clientPublicValue, 1);
	if (a->clientPublicValue == NULL)
	    return ENOMEM;
	ret = der_copy_oid(oid_id_dhpublicnumber(),
			   &a->clientPublicValue->algorithm.algorithm);
	if (ret)
	    return ret;
	
	memset(&dp, 0, sizeof(dp));

	ret = BN_to_integer(context, dh->p, &dp.p);
	if (ret) {
	    free_DomainParameters(&dp);
	    return ret;
	}
	ret = BN_to_integer(context, dh->g, &dp.g);
	if (ret) {
	    free_DomainParameters(&dp);
	    return ret;
	}
	ret = BN_to_integer(context, dh->q, &dp.q);
	if (ret) {
	    free_DomainParameters(&dp);
	    return ret;
	}
	dp.j = NULL;
	dp.validationParms = NULL;

	a->clientPublicValue->algorithm.parameters = 
	    malloc(sizeof(*a->clientPublicValue->algorithm.parameters));
	if (a->clientPublicValue->algorithm.parameters == NULL) {
	    free_DomainParameters(&dp);
	    return ret;
	}

	ASN1_MALLOC_ENCODE(DomainParameters,
			   a->clientPublicValue->algorithm.parameters->data,
			   a->clientPublicValue->algorithm.parameters->length,
			   &dp, &size, ret);
	free_DomainParameters(&dp);
	if (ret)
	    return ret;
	if (size != a->clientPublicValue->algorithm.parameters->length)
	    krb5_abortx(context, "Internal ASN1 encoder error");

	ret = BN_to_integer(context, dh->pub_key, &dh_pub_key);
	if (ret)
	    return ret;

	ASN1_MALLOC_ENCODE(DHPublicKey, dhbuf.data, dhbuf.length,
			   &dh_pub_key, &size, ret);
	der_free_heim_integer(&dh_pub_key);
	if (ret)
	    return ret;
	if (size != dhbuf.length)
	    krb5_abortx(context, "asn1 internal error");

	a->clientPublicValue->subjectPublicKey.length = dhbuf.length * 8;
	a->clientPublicValue->subjectPublicKey.data = dhbuf.data;
    }

    {
	a->supportedCMSTypes = calloc(1, sizeof(*a->supportedCMSTypes));
	if (a->supportedCMSTypes == NULL)
	    return ENOMEM;

	ret = hx509_crypto_available(ctx->id->hx509ctx, HX509_SELECT_ALL, NULL,
				     &a->supportedCMSTypes->val,
				     &a->supportedCMSTypes->len);
	if (ret)
	    return ret;
    }

    return ret;
}

krb5_error_code KRB5_LIB_FUNCTION
_krb5_pk_mk_ContentInfo(krb5_context context,
			const krb5_data *buf, 
			const heim_oid *oid,
			struct ContentInfo *content_info)
{
    krb5_error_code ret;

    ret = der_copy_oid(oid, &content_info->contentType);
    if (ret)
	return ret;
    ALLOC(content_info->content, 1);
    if (content_info->content == NULL)
	return ENOMEM;
    content_info->content->data = malloc(buf->length);
    if (content_info->content->data == NULL)
	return ENOMEM;
    memcpy(content_info->content->data, buf->data, buf->length);
    content_info->content->length = buf->length;
    return 0;
}

static krb5_error_code
pk_mk_padata(krb5_context context,
	     krb5_pk_init_ctx ctx,
	     const KDC_REQ_BODY *req_body,
	     unsigned nonce,
	     METHOD_DATA *md)
{
    struct ContentInfo content_info;
    krb5_error_code ret;
    const heim_oid *oid;
    size_t size;
    krb5_data buf, sd_buf;
    int pa_type;

    krb5_data_zero(&buf);
    krb5_data_zero(&sd_buf);
    memset(&content_info, 0, sizeof(content_info));

    if (ctx->type == COMPAT_WIN2K) {
	AuthPack_Win2k ap;
	krb5_timestamp sec;
	int32_t usec;

	memset(&ap, 0, sizeof(ap));

	/* fill in PKAuthenticator */
	ret = copy_PrincipalName(req_body->sname, &ap.pkAuthenticator.kdcName);
	if (ret) {
	    free_AuthPack_Win2k(&ap);
	    krb5_clear_error_string(context);
	    goto out;
	}
	ret = copy_Realm(&req_body->realm, &ap.pkAuthenticator.kdcRealm);
	if (ret) {
	    free_AuthPack_Win2k(&ap);
	    krb5_clear_error_string(context);
	    goto out;
	}

	krb5_us_timeofday(context, &sec, &usec);
	ap.pkAuthenticator.ctime = sec;
	ap.pkAuthenticator.cusec = usec;
	ap.pkAuthenticator.nonce = nonce;

	ASN1_MALLOC_ENCODE(AuthPack_Win2k, buf.data, buf.length,
			   &ap, &size, ret);
	free_AuthPack_Win2k(&ap);
	if (ret) {
	    krb5_set_error_string(context, "AuthPack_Win2k: %d", ret);
	    goto out;
	}
	if (buf.length != size)
	    krb5_abortx(context, "internal ASN1 encoder error");

	oid = oid_id_pkcs7_data();
    } else if (ctx->type == COMPAT_IETF) {
	AuthPack ap;
	
	memset(&ap, 0, sizeof(ap));

	ret = build_auth_pack(context, nonce, ctx, ctx->dh, req_body, &ap);
	if (ret) {
	    free_AuthPack(&ap);
	    goto out;
	}

	ASN1_MALLOC_ENCODE(AuthPack, buf.data, buf.length, &ap, &size, ret);
	free_AuthPack(&ap);
	if (ret) {
	    krb5_set_error_string(context, "AuthPack: %d", ret);
	    goto out;
	}
	if (buf.length != size)
	    krb5_abortx(context, "internal ASN1 encoder error");

	oid = oid_id_pkauthdata();
    } else
	krb5_abortx(context, "internal pkinit error");

    ret = _krb5_pk_create_sign(context,
			       oid,
			       &buf,
			       ctx->id,
			       ctx->peer,
			       &sd_buf);
    krb5_data_free(&buf);
    if (ret)
	goto out;

    ret = hx509_cms_wrap_ContentInfo(oid_id_pkcs7_signedData(), &sd_buf, &buf);
    krb5_data_free(&sd_buf);
    if (ret) {
	krb5_set_error_string(context,
			      "ContentInfo wrapping of signedData failed");
	goto out;
    }

    if (ctx->type == COMPAT_WIN2K) {
	PA_PK_AS_REQ_Win2k winreq;

	pa_type = KRB5_PADATA_PK_AS_REQ_WIN;

	memset(&winreq, 0, sizeof(winreq));

	winreq.signed_auth_pack = buf;

	ASN1_MALLOC_ENCODE(PA_PK_AS_REQ_Win2k, buf.data, buf.length,
			   &winreq, &size, ret);
	free_PA_PK_AS_REQ_Win2k(&winreq);

    } else if (ctx->type == COMPAT_IETF) {
	PA_PK_AS_REQ req;

	pa_type = KRB5_PADATA_PK_AS_REQ;

	memset(&req, 0, sizeof(req));
	req.signedAuthPack = buf;	

	if (ctx->trustedCertifiers) {

	    req.trustedCertifiers = calloc(1, sizeof(*req.trustedCertifiers));
	    if (req.trustedCertifiers == NULL) {
		krb5_set_error_string(context, "malloc: out of memory");
		free_PA_PK_AS_REQ(&req);
		goto out;
	    }
	    ret = build_edi(context, ctx->id->hx509ctx, 
			    ctx->id->anchors, req.trustedCertifiers);
	    if (ret) {
		krb5_set_error_string(context, "pk-init: failed to build trustedCertifiers");
		free_PA_PK_AS_REQ(&req);
		goto out;
	    }
	}
	req.kdcPkId = NULL;

	ASN1_MALLOC_ENCODE(PA_PK_AS_REQ, buf.data, buf.length,
			   &req, &size, ret);

	free_PA_PK_AS_REQ(&req);

    } else
	krb5_abortx(context, "internal pkinit error");
    if (ret) {
	krb5_set_error_string(context, "PA-PK-AS-REQ %d", ret);
	goto out;
    }
    if (buf.length != size)
	krb5_abortx(context, "Internal ASN1 encoder error");

    ret = krb5_padata_add(context, md, pa_type, buf.data, buf.length);
    if (ret)
	free(buf.data);

    if (ret == 0 && ctx->type == COMPAT_WIN2K)
	krb5_padata_add(context, md, KRB5_PADATA_PK_AS_09_BINDING, NULL, 0);

out:
    free_ContentInfo(&content_info);

    return ret;
}


krb5_error_code KRB5_LIB_FUNCTION 
_krb5_pk_mk_padata(krb5_context context,
		   void *c,
		   const KDC_REQ_BODY *req_body,
		   unsigned nonce,
		   METHOD_DATA *md)
{
    krb5_pk_init_ctx ctx = c;
    int win2k_compat;

    win2k_compat = krb5_config_get_bool_default(context, NULL,
						FALSE,
						"realms",
						req_body->realm,
						"pkinit_win2k",
						NULL);

    if (win2k_compat) {
	ctx->require_binding = 
	    krb5_config_get_bool_default(context, NULL,
					 FALSE,
					 "realms",
					 req_body->realm,
					 "pkinit_win2k_require_binding",
					 NULL);
	ctx->type = COMPAT_WIN2K;
    } else
	ctx->type = COMPAT_IETF;

    ctx->require_eku = 
	krb5_config_get_bool_default(context, NULL,
				     TRUE,
				     "realms",
				     req_body->realm,
				     "pkinit_require_eku",
				     NULL);
    ctx->require_krbtgt_otherName = 
	krb5_config_get_bool_default(context, NULL,
				     TRUE,
				     "realms",
				     req_body->realm,
				     "pkinit_require_krbtgt_otherName",
				     NULL);

    ctx->require_hostname_match = 
	krb5_config_get_bool_default(context, NULL,
				     FALSE,
				     "realms",
				     req_body->realm,
				     "pkinit_require_hostname_match",
				     NULL);

    ctx->trustedCertifiers = 
	krb5_config_get_bool_default(context, NULL,
				     TRUE,
				     "realms",
				     req_body->realm,
				     "pkinit_trustedCertifiers",
				     NULL);

    return pk_mk_padata(context, ctx, req_body, nonce, md);
}

krb5_error_code KRB5_LIB_FUNCTION
_krb5_pk_verify_sign(krb5_context context,
		     const void *data,
		     size_t length,
		     struct krb5_pk_identity *id,
		     heim_oid *contentType,
		     krb5_data *content,
		     struct krb5_pk_cert **signer)
{
    hx509_certs signer_certs;
    int ret;

    *signer = NULL;

    ret = hx509_cms_verify_signed(id->hx509ctx,
				  id->verify_ctx,
				  data,
				  length,
				  NULL,
				  id->certpool,
				  contentType,
				  content,
				  &signer_certs);
    if (ret) {
	_krb5_pk_copy_error(context, id->hx509ctx, ret,
			    "CMS verify signed failed");
	return ret;
    }

    *signer = calloc(1, sizeof(**signer));
    if (*signer == NULL) {
	krb5_clear_error_string(context);
	ret = ENOMEM;
	goto out;
    }
	
    ret = hx509_get_one_cert(id->hx509ctx, signer_certs, &(*signer)->cert);
    if (ret) {
	_krb5_pk_copy_error(context, id->hx509ctx, ret,
			    "Failed to get on of the signer certs");
	goto out;
    }

out:
    hx509_certs_free(&signer_certs);
    if (ret) {
	if (*signer) {
	    hx509_cert_free((*signer)->cert);
	    free(*signer);
	    *signer = NULL;
	}
    }

    return ret;
}

static krb5_error_code
get_reply_key_win(krb5_context context,
		  const krb5_data *content,
		  unsigned nonce,
		  krb5_keyblock **key)
{
    ReplyKeyPack_Win2k key_pack;
    krb5_error_code ret;
    size_t size;

    ret = decode_ReplyKeyPack_Win2k(content->data,
				    content->length,
				    &key_pack,
				    &size);
    if (ret) {
	krb5_set_error_string(context, "PKINIT decoding reply key failed");
	free_ReplyKeyPack_Win2k(&key_pack);
	return ret;
    }
     
    if (key_pack.nonce != nonce) {
	krb5_set_error_string(context, "PKINIT enckey nonce is wrong");
	free_ReplyKeyPack_Win2k(&key_pack);
	return KRB5KRB_AP_ERR_MODIFIED;
    }

    *key = malloc (sizeof (**key));
    if (*key == NULL) {
	krb5_set_error_string(context, "PKINIT failed allocating reply key");
	free_ReplyKeyPack_Win2k(&key_pack);
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }

    ret = copy_EncryptionKey(&key_pack.replyKey, *key);
    free_ReplyKeyPack_Win2k(&key_pack);
    if (ret) {
	krb5_set_error_string(context, "PKINIT failed copying reply key");
	free(*key);
	*key = NULL;
    }

    return ret;
}

static krb5_error_code
get_reply_key(krb5_context context,
	      const krb5_data *content,
	      const krb5_data *req_buffer,
	      krb5_keyblock **key)
{
    ReplyKeyPack key_pack;
    krb5_error_code ret;
    size_t size;

    ret = decode_ReplyKeyPack(content->data,
			      content->length,
			      &key_pack,
			      &size);
    if (ret) {
	krb5_set_error_string(context, "PKINIT decoding reply key failed");
	free_ReplyKeyPack(&key_pack);
	return ret;
    }
    
    {
	krb5_crypto crypto;

	/* 
	 * XXX Verify kp.replyKey is a allowed enctype in the
	 * configuration file
	 */

	ret = krb5_crypto_init(context, &key_pack.replyKey, 0, &crypto);
	if (ret) {
	    free_ReplyKeyPack(&key_pack);
	    return ret;
	}

	ret = krb5_verify_checksum(context, crypto, 6,
				   req_buffer->data, req_buffer->length,
				   &key_pack.asChecksum);
	krb5_crypto_destroy(context, crypto);
	if (ret) {
	    free_ReplyKeyPack(&key_pack);
	    return ret;
	}
    }

    *key = malloc (sizeof (**key));
    if (*key == NULL) {
	krb5_set_error_string(context, "PKINIT failed allocating reply key");
	free_ReplyKeyPack(&key_pack);
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }

    ret = copy_EncryptionKey(&key_pack.replyKey, *key);
    free_ReplyKeyPack(&key_pack);
    if (ret) {
	krb5_set_error_string(context, "PKINIT failed copying reply key");
	free(*key);
	*key = NULL;
    }

    return ret;
}


static krb5_error_code
pk_verify_host(krb5_context context,
	       const char *realm,
	       const krb5_krbhst_info *hi,
	       struct krb5_pk_init_ctx_data *ctx,
	       struct krb5_pk_cert *host)
{
    krb5_error_code ret = 0;

    if (ctx->require_eku) {
	ret = hx509_cert_check_eku(ctx->id->hx509ctx, host->cert,
				   oid_id_pkkdcekuoid(), 0);
	if (ret) {
	    krb5_set_error_string(context, "No PK-INIT KDC EKU in kdc certificate");
	    return ret;
	}
    }
    if (ctx->require_krbtgt_otherName) {
	hx509_octet_string_list list;
	int i;

	ret = hx509_cert_find_subjectAltName_otherName(ctx->id->hx509ctx,
						       host->cert,
						       oid_id_pkinit_san(),
						       &list);
	if (ret) {
	    krb5_set_error_string(context, "Failed to find the PK-INIT "
				  "subjectAltName in the KDC certificate");

	    return ret;
	}

	for (i = 0; i < list.len; i++) {
	    KRB5PrincipalName r;

	    ret = decode_KRB5PrincipalName(list.val[i].data,
					   list.val[i].length,
					   &r,
					   NULL);
	    if (ret) {
		krb5_set_error_string(context, "Failed to decode the PK-INIT "
				      "subjectAltName in the KDC certificate");

		break;
	    }

	    if (r.principalName.name_string.len != 2 ||
		strcmp(r.principalName.name_string.val[0], KRB5_TGS_NAME) != 0 ||
		strcmp(r.principalName.name_string.val[1], realm) != 0 ||
		strcmp(r.realm, realm) != 0)
	    {
		krb5_set_error_string(context, "KDC have wrong realm name in "
				      "the certificate");
		ret = KRB5_KDC_ERR_INVALID_CERTIFICATE;
	    }

	    free_KRB5PrincipalName(&r);
	    if (ret)
		break;
	}
	hx509_free_octet_string_list(&list);
    }
    if (ret)
	return ret;
    
    if (hi) {
	ret = hx509_verify_hostname(ctx->id->hx509ctx, host->cert, 
				    ctx->require_hostname_match,
				    HX509_HN_HOSTNAME,
				    hi->hostname,
				    hi->ai->ai_addr, hi->ai->ai_addrlen);

	if (ret)
	    krb5_set_error_string(context, "Address mismatch in "
				  "the KDC certificate");
    }
    return ret;
}

static krb5_error_code
pk_rd_pa_reply_enckey(krb5_context context,
		      int type,
		      const heim_octet_string *indata,
		      const heim_oid *dataType,
		      const char *realm,
		      krb5_pk_init_ctx ctx,
		      krb5_enctype etype,
		      const krb5_krbhst_info *hi,
	       	      unsigned nonce,
		      const krb5_data *req_buffer,
	       	      PA_DATA *pa,
	       	      krb5_keyblock **key) 
{
    krb5_error_code ret;
    struct krb5_pk_cert *host = NULL;
    krb5_data content;
    heim_oid contentType = { 0, NULL };

    if (der_heim_oid_cmp(oid_id_pkcs7_envelopedData(), dataType)) {
	krb5_set_error_string(context, "PKINIT: Invalid content type");
	return EINVAL;
    }

    ret = hx509_cms_unenvelope(ctx->id->hx509ctx,
			       ctx->id->certs,
			       HX509_CMS_UE_DONT_REQUIRE_KU_ENCIPHERMENT,
			       indata->data,
			       indata->length,
			       NULL,
			       &contentType,
			       &content);
    if (ret) {
	_krb5_pk_copy_error(context, ctx->id->hx509ctx, ret,
			    "Failed to unenvelope CMS data in PK-INIT reply");
	return ret;
    }
    der_free_oid(&contentType);

#if 0 /* windows LH with interesting CMS packets, leaks memory */
    {
	size_t ph = 1 + der_length_len (length);
	unsigned char *ptr = malloc(length + ph);
	size_t l;

	memcpy(ptr + ph, p, length);

	ret = der_put_length_and_tag (ptr + ph - 1, ph, length,
				      ASN1_C_UNIV, CONS, UT_Sequence, &l);
	if (ret)
	    return ret;
	ptr += ph - l;
	length += l;
	p = ptr;
    }
#endif

    /* win2k uses ContentInfo */
    if (type == COMPAT_WIN2K) {
	heim_oid type;
	heim_octet_string out;

	ret = hx509_cms_unwrap_ContentInfo(&content, &type, &out, NULL);
	if (der_heim_oid_cmp(&type, oid_id_pkcs7_signedData())) {
	    ret = EINVAL; /* XXX */
	    krb5_set_error_string(context, "PKINIT: Invalid content type");
	    der_free_oid(&type);
	    der_free_octet_string(&out);
	    goto out;
	}
	der_free_oid(&type);
	krb5_data_free(&content);
	ret = krb5_data_copy(&content, out.data, out.length);
	der_free_octet_string(&out);
	if (ret) {
	    krb5_set_error_string(context, "PKINIT: out of memory");
	    goto out;
	}
    }

    ret = _krb5_pk_verify_sign(context, 
			       content.data,
			       content.length,
			       ctx->id,
			       &contentType,
			       &content,
			       &host);
    if (ret)
	goto out;

    /* make sure that it is the kdc's certificate */
    ret = pk_verify_host(context, realm, hi, ctx, host);
    if (ret) {
	goto out;
    }

#if 0
    if (type == COMPAT_WIN2K) {
	if (der_heim_oid_cmp(&contentType, oid_id_pkcs7_data()) != 0) {
	    krb5_set_error_string(context, "PKINIT: reply key, wrong oid");
	    ret = KRB5KRB_AP_ERR_MSG_TYPE;
	    goto out;
	}
    } else {
	if (der_heim_oid_cmp(&contentType, oid_id_pkrkeydata()) != 0) {
	    krb5_set_error_string(context, "PKINIT: reply key, wrong oid");
	    ret = KRB5KRB_AP_ERR_MSG_TYPE;
	    goto out;
	}
    }
#endif

    switch(type) {
    case COMPAT_WIN2K:
	ret = get_reply_key(context, &content, req_buffer, key);
	if (ret != 0 && ctx->require_binding == 0)
	    ret = get_reply_key_win(context, &content, nonce, key);
	break;
    case COMPAT_IETF:
	ret = get_reply_key(context, &content, req_buffer, key);
	break;
    }
    if (ret)
	goto out;

    /* XXX compare given etype with key->etype */

 out:
    if (host)
	_krb5_pk_cert_free(host);
    der_free_oid(&contentType);
    krb5_data_free(&content);

    return ret;
}

static krb5_error_code
pk_rd_pa_reply_dh(krb5_context context,
		  const heim_octet_string *indata,
		  const heim_oid *dataType,
		  const char *realm,
		  krb5_pk_init_ctx ctx,
		  krb5_enctype etype,
		  const krb5_krbhst_info *hi,
		  const DHNonce *c_n,
		  const DHNonce *k_n,
                  unsigned nonce,
                  PA_DATA *pa,
                  krb5_keyblock **key)
{
    unsigned char *p, *dh_gen_key = NULL;
    struct krb5_pk_cert *host = NULL;
    BIGNUM *kdc_dh_pubkey = NULL;
    KDCDHKeyInfo kdc_dh_info;
    heim_oid contentType = { 0, NULL };
    krb5_data content;
    krb5_error_code ret;
    int dh_gen_keylen;
    size_t size;

    krb5_data_zero(&content);
    memset(&kdc_dh_info, 0, sizeof(kdc_dh_info));

    if (der_heim_oid_cmp(oid_id_pkcs7_signedData(), dataType)) {
	krb5_set_error_string(context, "PKINIT: Invalid content type");
	return EINVAL;
    }

    ret = _krb5_pk_verify_sign(context, 
			       indata->data,
			       indata->length,
			       ctx->id,
			       &contentType,
			       &content,
			       &host);
    if (ret)
	goto out;

    /* make sure that it is the kdc's certificate */
    ret = pk_verify_host(context, realm, hi, ctx, host);
    if (ret)
	goto out;

    if (der_heim_oid_cmp(&contentType, oid_id_pkdhkeydata())) {
	krb5_set_error_string(context, "pkinit - dh reply contains wrong oid");
	ret = KRB5KRB_AP_ERR_MSG_TYPE;
	goto out;
    }

    ret = decode_KDCDHKeyInfo(content.data,
			      content.length,
			      &kdc_dh_info,
			      &size);

    if (ret) {
	krb5_set_error_string(context, "pkinit - "
			      "failed to decode KDC DH Key Info");
	goto out;
    }

    if (kdc_dh_info.nonce != nonce) {
	krb5_set_error_string(context, "PKINIT: DH nonce is wrong");
	ret = KRB5KRB_AP_ERR_MODIFIED;
	goto out;
    }

    if (kdc_dh_info.dhKeyExpiration) {
	if (k_n == NULL) {
	    krb5_set_error_string(context, "pkinit; got key expiration "
				  "without server nonce");
	    ret = KRB5KRB_ERR_GENERIC;
	    goto out;
	}
	if (c_n == NULL) {
	    krb5_set_error_string(context, "pkinit; got DH reuse but no "
				  "client nonce");
	    ret = KRB5KRB_ERR_GENERIC;
	    goto out;
	}
    } else {
	if (k_n) {
	    krb5_set_error_string(context, "pkinit: got server nonce "
				  "without key expiration");
	    ret = KRB5KRB_ERR_GENERIC;
	    goto out;
	}
	c_n = NULL;
    }


    p = kdc_dh_info.subjectPublicKey.data;
    size = (kdc_dh_info.subjectPublicKey.length + 7) / 8;

    {
	DHPublicKey k;
	ret = decode_DHPublicKey(p, size, &k, NULL);
	if (ret) {
	    krb5_set_error_string(context, "pkinit: can't decode "
				  "without key expiration");
	    goto out;
	}

	kdc_dh_pubkey = integer_to_BN(context, "DHPublicKey", &k);
	free_DHPublicKey(&k);
	if (kdc_dh_pubkey == NULL) {
	    ret = KRB5KRB_ERR_GENERIC;
	    goto out;
	}
    }
    
    dh_gen_keylen = DH_size(ctx->dh);
    size = BN_num_bytes(ctx->dh->p);
    if (size < dh_gen_keylen)
	size = dh_gen_keylen;

    dh_gen_key = malloc(size);
    if (dh_gen_key == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	ret = ENOMEM;
	goto out;
    }
    memset(dh_gen_key, 0, size - dh_gen_keylen);

    dh_gen_keylen = DH_compute_key(dh_gen_key + (size - dh_gen_keylen),
				   kdc_dh_pubkey, ctx->dh);
    if (dh_gen_keylen == -1) {
	krb5_set_error_string(context, 
			      "PKINIT: Can't compute Diffie-Hellman key");
	ret = KRB5KRB_ERR_GENERIC;
	goto out;
    }

    *key = malloc (sizeof (**key));
    if (*key == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	ret = ENOMEM;
	goto out;
    }

    ret = _krb5_pk_octetstring2key(context,
				   etype,
				   dh_gen_key, dh_gen_keylen,
				   c_n, k_n,
				   *key);
    if (ret) {
	krb5_set_error_string(context,
			      "PKINIT: can't create key from DH key");
	free(*key);
	*key = NULL;
	goto out;
    }

 out:
    if (kdc_dh_pubkey)
	BN_free(kdc_dh_pubkey);
    if (dh_gen_key) {
	memset(dh_gen_key, 0, DH_size(ctx->dh));
	free(dh_gen_key);
    }
    if (host)
	_krb5_pk_cert_free(host);
    if (content.data)
	krb5_data_free(&content);
    der_free_oid(&contentType);
    free_KDCDHKeyInfo(&kdc_dh_info);

    return ret;
}

krb5_error_code KRB5_LIB_FUNCTION
_krb5_pk_rd_pa_reply(krb5_context context,
		     const char *realm,
		     void *c,
		     krb5_enctype etype,
		     const krb5_krbhst_info *hi,
		     unsigned nonce,
		     const krb5_data *req_buffer,
		     PA_DATA *pa,
		     krb5_keyblock **key)
{
    krb5_pk_init_ctx ctx = c;
    krb5_error_code ret;
    size_t size;

    /* Check for IETF PK-INIT first */
    if (ctx->type == COMPAT_IETF) {
	PA_PK_AS_REP rep;
	heim_octet_string os, data;
	heim_oid oid;
	
	if (pa->padata_type != KRB5_PADATA_PK_AS_REP) {
	    krb5_set_error_string(context, "PKINIT: wrong padata recv");
	    return EINVAL;
	}

	ret = decode_PA_PK_AS_REP(pa->padata_value.data,
				  pa->padata_value.length,
				  &rep,
				  &size);
	if (ret) {
	    krb5_set_error_string(context, "Failed to decode pkinit AS rep");
	    return ret;
	}

	switch (rep.element) {
	case choice_PA_PK_AS_REP_dhInfo:
	    os = rep.u.dhInfo.dhSignedData;
	    break;
	case choice_PA_PK_AS_REP_encKeyPack:
	    os = rep.u.encKeyPack;
	    break;
	default:
	    free_PA_PK_AS_REP(&rep);
	    krb5_set_error_string(context, "PKINIT: -27 reply "
				  "invalid content type");
	    return EINVAL;
	}

	ret = hx509_cms_unwrap_ContentInfo(&os, &oid, &data, NULL);
	if (ret) {
	    free_PA_PK_AS_REP(&rep);
	    krb5_set_error_string(context, "PKINIT: failed to unwrap CI");
	    return ret;
	}

	switch (rep.element) {
	case choice_PA_PK_AS_REP_dhInfo:
	    ret = pk_rd_pa_reply_dh(context, &data, &oid, realm, ctx, etype, hi,
				    ctx->clientDHNonce,
				    rep.u.dhInfo.serverDHNonce,
				    nonce, pa, key);
	    break;
	case choice_PA_PK_AS_REP_encKeyPack:
	    ret = pk_rd_pa_reply_enckey(context, COMPAT_IETF, &data, &oid, realm, 
					ctx, etype, hi, nonce, req_buffer, pa, key);
	    break;
	default:
	    krb5_abortx(context, "pk-init as-rep case not possible to happen");
	}
	der_free_octet_string(&data);
	der_free_oid(&oid);
	free_PA_PK_AS_REP(&rep);

    } else if (ctx->type == COMPAT_WIN2K) {
	PA_PK_AS_REP_Win2k w2krep;

	/* Check for Windows encoding of the AS-REP pa data */ 

#if 0 /* should this be ? */
	if (pa->padata_type != KRB5_PADATA_PK_AS_REP) {
	    krb5_set_error_string(context, "PKINIT: wrong padata recv");
	    return EINVAL;
	}
#endif

	memset(&w2krep, 0, sizeof(w2krep));
	
	ret = decode_PA_PK_AS_REP_Win2k(pa->padata_value.data,
					pa->padata_value.length,
					&w2krep,
					&size);
	if (ret) {
	    krb5_set_error_string(context, "PKINIT: Failed decoding windows "
				  "pkinit reply %d", ret);
	    return ret;
	}

	krb5_clear_error_string(context);
	
	switch (w2krep.element) {
	case choice_PA_PK_AS_REP_Win2k_encKeyPack: {
	    heim_octet_string data;
	    heim_oid oid;
	    
	    ret = hx509_cms_unwrap_ContentInfo(&w2krep.u.encKeyPack, 
					       &oid, &data, NULL);
	    free_PA_PK_AS_REP_Win2k(&w2krep);
	    if (ret) {
		krb5_set_error_string(context, "PKINIT: failed to unwrap CI");
		return ret;
	    }

	    ret = pk_rd_pa_reply_enckey(context, COMPAT_WIN2K, &data, &oid, realm,
					ctx, etype, hi, nonce, req_buffer, pa, key);
	    der_free_octet_string(&data);
	    der_free_oid(&oid);

	    break;
	}
	default:
	    free_PA_PK_AS_REP_Win2k(&w2krep);
	    krb5_set_error_string(context, "PKINIT: win2k reply invalid "
				  "content type");
	    ret = EINVAL;
	    break;
	}
    
    } else {
	krb5_set_error_string(context, "PKINIT: unknown reply type");
	ret = EINVAL;
    }

    return ret;
}

struct prompter {
    krb5_context context;
    krb5_prompter_fct prompter;
    void *prompter_data;
};

static int 
hx_pass_prompter(void *data, const hx509_prompt *prompter)
{
    krb5_error_code ret;
    krb5_prompt prompt;
    krb5_data password_data;
    struct prompter *p = data;
   
    password_data.data   = prompter->reply.data;
    password_data.length = prompter->reply.length;

    prompt.prompt = prompter->prompt;
    prompt.hidden = hx509_prompt_hidden(prompter->type);
    prompt.reply  = &password_data;

    switch (prompter->type) {
    case HX509_PROMPT_TYPE_INFO:
	prompt.type   = KRB5_PROMPT_TYPE_INFO;
	break;
    case HX509_PROMPT_TYPE_PASSWORD:
    case HX509_PROMPT_TYPE_QUESTION:
    default:
	prompt.type   = KRB5_PROMPT_TYPE_PASSWORD;
	break;
    }	
   
    ret = (*p->prompter)(p->context, p->prompter_data, NULL, NULL, 1, &prompt);
    if (ret) {
	memset (prompter->reply.data, 0, prompter->reply.length);
	return 1;
    }
    return 0;
}


void KRB5_LIB_FUNCTION
_krb5_pk_allow_proxy_certificate(struct krb5_pk_identity *id,
				 int boolean)
{
    hx509_verify_set_proxy_certificate(id->verify_ctx, boolean);
}


krb5_error_code KRB5_LIB_FUNCTION
_krb5_pk_load_id(krb5_context context,
		 struct krb5_pk_identity **ret_id,
		 const char *user_id,
		 const char *anchor_id,
		 char * const *chain_list,
		 char * const *revoke_list,
		 krb5_prompter_fct prompter,
		 void *prompter_data,
		 char *password)
{
    struct krb5_pk_identity *id = NULL;
    hx509_lock lock = NULL;
    struct prompter p;
    int ret;

    *ret_id = NULL;

    if (anchor_id == NULL) {
	krb5_set_error_string(context, "PKINIT: No anchor given");
	return HEIM_PKINIT_NO_VALID_CA;
    }

    if (user_id == NULL) {
	krb5_set_error_string(context,
			      "PKINIT: No user certificate given");
	return HEIM_PKINIT_NO_PRIVATE_KEY;
    }

    /* load cert */

    id = calloc(1, sizeof(*id));
    if (id == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }	

    ret = hx509_context_init(&id->hx509ctx);
    if (ret)
	goto out;

    ret = hx509_lock_init(id->hx509ctx, &lock);
    if (password && password[0])
	hx509_lock_add_password(lock, password);

    if (prompter) {
	p.context = context;
	p.prompter = prompter;
	p.prompter_data = prompter_data;

	ret = hx509_lock_set_prompter(lock, hx_pass_prompter, &p);
	if (ret)
	    goto out;
    }

    ret = hx509_certs_init(id->hx509ctx, user_id, 0, lock, &id->certs);
    if (ret) {
	_krb5_pk_copy_error(context, id->hx509ctx, ret,
			    "Failed to init cert certs");
	goto out;
    }

    ret = hx509_certs_init(id->hx509ctx, anchor_id, 0, NULL, &id->anchors);
    if (ret) {
	_krb5_pk_copy_error(context, id->hx509ctx, ret,
			    "Failed to init anchors");
	goto out;
    }

    ret = hx509_certs_init(id->hx509ctx, "MEMORY:pkinit-cert-chain", 
			   0, NULL, &id->certpool);
    if (ret) {
	_krb5_pk_copy_error(context, id->hx509ctx, ret,
			    "Failed to init chain");
	goto out;
    }

    while (chain_list && *chain_list) {
	ret = hx509_certs_append(id->hx509ctx, id->certpool,
				 NULL, *chain_list);
	if (ret) {
	    _krb5_pk_copy_error(context, id->hx509ctx, ret,
				"Failed to laod chain %s",
				*chain_list);
	    goto out;
	}
	chain_list++;
    }

    if (revoke_list) {
	ret = hx509_revoke_init(id->hx509ctx, &id->revokectx);
	if (ret) {
	    _krb5_pk_copy_error(context, id->hx509ctx, ret,
				"Failed init revoke list");
	    goto out;
	}

	while (*revoke_list) {
	    ret = hx509_revoke_add_crl(id->hx509ctx, 
				       id->revokectx,
				       *revoke_list);
	    if (ret) {
		_krb5_pk_copy_error(context, id->hx509ctx, ret, 
				    "Failed load revoke list");
		goto out;
	    }
	    revoke_list++;
	}
    } else
	hx509_context_set_missing_revoke(id->hx509ctx, 1);

    ret = hx509_verify_init_ctx(id->hx509ctx, &id->verify_ctx);
    if (ret) {
	_krb5_pk_copy_error(context, id->hx509ctx, ret, 
			    "Failed init verify context");
	goto out;
    }

    hx509_verify_attach_anchors(id->verify_ctx, id->anchors);
    hx509_verify_attach_revoke(id->verify_ctx, id->revokectx);

out:
    if (ret) {
	hx509_verify_destroy_ctx(id->verify_ctx);
	hx509_certs_free(&id->certs);
	hx509_certs_free(&id->anchors);
	hx509_certs_free(&id->certpool);
	hx509_revoke_free(&id->revokectx);
	hx509_context_free(&id->hx509ctx);
	free(id);
    } else
	*ret_id = id;

    hx509_lock_free(lock);

    return ret;
}

static krb5_error_code
select_dh_group(krb5_context context, DH *dh, unsigned long bits, 
		struct krb5_dh_moduli **moduli)
{
    const struct krb5_dh_moduli *m;

    if (bits == 0) {
	m = moduli[1]; /* XXX */
	if (m == NULL)
	    m = moduli[0]; /* XXX */
    } else {
	int i;
	for (i = 0; moduli[i] != NULL; i++) {
	    if (bits < moduli[i]->bits)
		break;
	}
	if (moduli[i] == NULL) {
	    krb5_set_error_string(context, 
				  "Did not find a DH group parameter "
				  "matching requirement of %lu bits",
				  bits);
	    return EINVAL;
	}
	m = moduli[i];
    }

    dh->p = integer_to_BN(context, "p", &m->p);
    if (dh->p == NULL)
	return ENOMEM;
    dh->g = integer_to_BN(context, "g", &m->g);
    if (dh->g == NULL)
	return ENOMEM;
    dh->q = integer_to_BN(context, "q", &m->q);
    if (dh->q == NULL)
	return ENOMEM;

    return 0;
}

#endif /* PKINIT */

static int
parse_integer(krb5_context context, char **p, const char *file, int lineno, 
	      const char *name, heim_integer *integer)
{
    int ret;
    char *p1;
    p1 = strsep(p, " \t");
    if (p1 == NULL) {
	krb5_set_error_string(context, "moduli file %s missing %s on line %d",
			      file, name, lineno);
	return EINVAL;
    }
    ret = der_parse_hex_heim_integer(p1, integer);
    if (ret) {
	krb5_set_error_string(context, "moduli file %s failed parsing %s "
			      "on line %d",
			      file, name, lineno);
	return ret;
    }

    return 0;
}

krb5_error_code
_krb5_parse_moduli_line(krb5_context context, 
			const char *file,
			int lineno,
			char *p,
			struct krb5_dh_moduli **m)
{
    struct krb5_dh_moduli *m1;
    char *p1;
    int ret;

    *m = NULL;

    m1 = calloc(1, sizeof(*m1));
    if (m1 == NULL) {
	krb5_set_error_string(context, "malloc - out of memory");
	return ENOMEM;
    }

    while (isspace((unsigned char)*p))
	p++;
    if (*p  == '#')
	return 0;
    ret = EINVAL;

    p1 = strsep(&p, " \t");
    if (p1 == NULL) {
	krb5_set_error_string(context, "moduli file %s missing name "
			      "on line %d", file, lineno);
	goto out;
    }
    m1->name = strdup(p1);
    if (p1 == NULL) {
	krb5_set_error_string(context, "malloc - out of memeory");
	ret = ENOMEM;
	goto out;
    }

    p1 = strsep(&p, " \t");
    if (p1 == NULL) {
	krb5_set_error_string(context, "moduli file %s missing bits on line %d",
			      file, lineno);
	goto out;
    }

    m1->bits = atoi(p1);
    if (m1->bits == 0) {
	krb5_set_error_string(context, "moduli file %s have un-parsable "
			      "bits on line %d", file, lineno);
	goto out;
    }
	
    ret = parse_integer(context, &p, file, lineno, "p", &m1->p);
    if (ret)
	goto out;
    ret = parse_integer(context, &p, file, lineno, "g", &m1->g);
    if (ret)
	goto out;
    ret = parse_integer(context, &p, file, lineno, "q", &m1->q);
    if (ret)
	goto out;

    *m = m1;

    return 0;
out:
    free(m1->name);
    der_free_heim_integer(&m1->p);
    der_free_heim_integer(&m1->g);
    der_free_heim_integer(&m1->q);
    free(m1);
    return ret;
}

void
_krb5_free_moduli(struct krb5_dh_moduli **moduli)
{
    int i;
    for (i = 0; moduli[i] != NULL; i++) {
	free(moduli[i]->name);
	der_free_heim_integer(&moduli[i]->p);
	der_free_heim_integer(&moduli[i]->g);
	der_free_heim_integer(&moduli[i]->q);
	free(moduli[i]);
    }
    free(moduli);
}

static const char *default_moduli_RFC2412_MODP_group2 =
    /* name */
    "RFC2412-MODP-group2 "
    /* bits */
    "1024 "
    /* p */
    "FFFFFFFF" "FFFFFFFF" "C90FDAA2" "2168C234" "C4C6628B" "80DC1CD1"
    "29024E08" "8A67CC74" "020BBEA6" "3B139B22" "514A0879" "8E3404DD"
    "EF9519B3" "CD3A431B" "302B0A6D" "F25F1437" "4FE1356D" "6D51C245"
    "E485B576" "625E7EC6" "F44C42E9" "A637ED6B" "0BFF5CB6" "F406B7ED"
    "EE386BFB" "5A899FA5" "AE9F2411" "7C4B1FE6" "49286651" "ECE65381"
    "FFFFFFFF" "FFFFFFFF "
    /* g */
    "02 "
    /* q */
    "7FFFFFFF" "FFFFFFFF" "E487ED51" "10B4611A" "62633145" "C06E0E68"
    "94812704" "4533E63A" "0105DF53" "1D89CD91" "28A5043C" "C71A026E"
    "F7CA8CD9" "E69D218D" "98158536" "F92F8A1B" "A7F09AB6" "B6A8E122"
    "F242DABB" "312F3F63" "7A262174" "D31BF6B5" "85FFAE5B" "7A035BF6"
    "F71C35FD" "AD44CFD2" "D74F9208" "BE258FF3" "24943328" "F67329C0"
    "FFFFFFFF" "FFFFFFFF";

static const char *default_moduli_rfc3526_MODP_group14 =
    /* name */
    "rfc3526-MODP-group14 "
    /* bits */
    "1760 "
    /* p */
    "FFFFFFFF" "FFFFFFFF" "C90FDAA2" "2168C234" "C4C6628B" "80DC1CD1"
    "29024E08" "8A67CC74" "020BBEA6" "3B139B22" "514A0879" "8E3404DD"
    "EF9519B3" "CD3A431B" "302B0A6D" "F25F1437" "4FE1356D" "6D51C245"
    "E485B576" "625E7EC6" "F44C42E9" "A637ED6B" "0BFF5CB6" "F406B7ED"
    "EE386BFB" "5A899FA5" "AE9F2411" "7C4B1FE6" "49286651" "ECE45B3D"
    "C2007CB8" "A163BF05" "98DA4836" "1C55D39A" "69163FA8" "FD24CF5F"
    "83655D23" "DCA3AD96" "1C62F356" "208552BB" "9ED52907" "7096966D"
    "670C354E" "4ABC9804" "F1746C08" "CA18217C" "32905E46" "2E36CE3B"
    "E39E772C" "180E8603" "9B2783A2" "EC07A28F" "B5C55DF0" "6F4C52C9"
    "DE2BCBF6" "95581718" "3995497C" "EA956AE5" "15D22618" "98FA0510"
    "15728E5A" "8AACAA68" "FFFFFFFF" "FFFFFFFF "
    /* g */
    "02 "
    /* q */
    "7FFFFFFF" "FFFFFFFF" "E487ED51" "10B4611A" "62633145" "C06E0E68"
    "94812704" "4533E63A" "0105DF53" "1D89CD91" "28A5043C" "C71A026E"
    "F7CA8CD9" "E69D218D" "98158536" "F92F8A1B" "A7F09AB6" "B6A8E122"
    "F242DABB" "312F3F63" "7A262174" "D31BF6B5" "85FFAE5B" "7A035BF6"
    "F71C35FD" "AD44CFD2" "D74F9208" "BE258FF3" "24943328" "F6722D9E"
    "E1003E5C" "50B1DF82" "CC6D241B" "0E2AE9CD" "348B1FD4" "7E9267AF"
    "C1B2AE91" "EE51D6CB" "0E3179AB" "1042A95D" "CF6A9483" "B84B4B36"
    "B3861AA7" "255E4C02" "78BA3604" "650C10BE" "19482F23" "171B671D"
    "F1CF3B96" "0C074301" "CD93C1D1" "7603D147" "DAE2AEF8" "37A62964"
    "EF15E5FB" "4AAC0B8C" "1CCAA4BE" "754AB572" "8AE9130C" "4C7D0288"
    "0AB9472D" "45565534" "7FFFFFFF" "FFFFFFFF";

krb5_error_code
_krb5_parse_moduli(krb5_context context, const char *file,
		   struct krb5_dh_moduli ***moduli)
{
    /* name bits P G Q */
    krb5_error_code ret;
    struct krb5_dh_moduli **m = NULL, **m2;
    char buf[4096];
    FILE *f;
    int lineno = 0, n = 0;

    *moduli = NULL;

    m = calloc(1, sizeof(m[0]) * 3);
    if (m == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }

    strlcpy(buf, default_moduli_rfc3526_MODP_group14, sizeof(buf));
    ret = _krb5_parse_moduli_line(context, "builtin", 1, buf,  &m[0]);
    if (ret) {
	_krb5_free_moduli(m);
	return ret;
    }
    n++;

    strlcpy(buf, default_moduli_RFC2412_MODP_group2, sizeof(buf));
    ret = _krb5_parse_moduli_line(context, "builtin", 1, buf,  &m[1]);
    if (ret) {
	_krb5_free_moduli(m);
	return ret;
    }
    n++;


    if (file == NULL)
	file = MODULI_FILE;

    f = fopen(file, "r");
    if (f == NULL) {
	*moduli = m;
	return 0;
    }

    while(fgets(buf, sizeof(buf), f) != NULL) {
	struct krb5_dh_moduli *element;

	buf[strcspn(buf, "\n")] = '\0';
	lineno++;

	m2 = realloc(m, (n + 2) * sizeof(m[0]));
	if (m2 == NULL) {
	    krb5_set_error_string(context, "malloc: out of memory");
	    _krb5_free_moduli(m);
	    return ENOMEM;
	}
	m = m2;
	
	m[n] = NULL;

	ret = _krb5_parse_moduli_line(context, file, lineno, buf,  &element);
	if (ret) {
	    _krb5_free_moduli(m);
	    return ret;
	}
	if (element == NULL)
	    continue;

	m[n] = element;
	m[n + 1] = NULL;
	n++;
    }
    *moduli = m;
    return 0;
}

krb5_error_code
_krb5_dh_group_ok(krb5_context context, unsigned long bits,
		  heim_integer *p, heim_integer *g, heim_integer *q,
		  struct krb5_dh_moduli **moduli,
		  char **name)
{
    int i;

    if (name)
	*name = NULL;

    for (i = 0; moduli[i] != NULL; i++) {
	if (der_heim_integer_cmp(&moduli[i]->g, g) == 0 &&
	    der_heim_integer_cmp(&moduli[i]->p, p) == 0 &&
	    (q == NULL || der_heim_integer_cmp(&moduli[i]->q, q) == 0))
	{
	    if (bits && bits > moduli[i]->bits) {
		krb5_set_error_string(context, "PKINIT: DH group parameter %s "
				      "no accepted, not enough bits generated",
				      moduli[i]->name);
		return KRB5_KDC_ERR_DH_KEY_PARAMETERS_NOT_ACCEPTED;
	    }
	    if (name)
		*name = strdup(moduli[i]->name);
	    return 0;
	}
    }
    krb5_set_error_string(context, "PKINIT: DH group parameter no ok");
    return KRB5_KDC_ERR_DH_KEY_PARAMETERS_NOT_ACCEPTED;
}

void KRB5_LIB_FUNCTION
_krb5_get_init_creds_opt_free_pkinit(krb5_get_init_creds_opt *opt)
{
#ifdef PKINIT
    krb5_pk_init_ctx ctx;

    if (opt->opt_private == NULL || opt->opt_private->pk_init_ctx == NULL)
	return;
    ctx = opt->opt_private->pk_init_ctx;
    if (ctx->dh)
	DH_free(ctx->dh);
	ctx->dh = NULL;
    if (ctx->id) {
	hx509_verify_destroy_ctx(ctx->id->verify_ctx);
	hx509_certs_free(&ctx->id->certs);
	hx509_certs_free(&ctx->id->anchors);
	hx509_certs_free(&ctx->id->certpool);
	hx509_context_free(&ctx->id->hx509ctx);

	if (ctx->clientDHNonce) {
	    krb5_free_data(NULL, ctx->clientDHNonce);
	    ctx->clientDHNonce = NULL;
	}
	if (ctx->m)
	    _krb5_free_moduli(ctx->m);
	free(ctx->id);
	ctx->id = NULL;
    }
    free(opt->opt_private->pk_init_ctx);
    opt->opt_private->pk_init_ctx = NULL;
#endif
}
    
krb5_error_code KRB5_LIB_FUNCTION
krb5_get_init_creds_opt_set_pkinit(krb5_context context,
				   krb5_get_init_creds_opt *opt,
				   krb5_principal principal,
				   const char *user_id,
				   const char *x509_anchors,
				   char * const * pool,
				   char * const * pki_revoke,
				   int flags,
				   krb5_prompter_fct prompter,
				   void *prompter_data,
				   char *password)
{
#ifdef PKINIT
    krb5_error_code ret;
    char *anchors = NULL;

    if (opt->opt_private == NULL) {
	krb5_set_error_string(context, "PKINIT: on non extendable opt");
	return EINVAL;
    }

    opt->opt_private->pk_init_ctx = 
	calloc(1, sizeof(*opt->opt_private->pk_init_ctx));
    if (opt->opt_private->pk_init_ctx == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    opt->opt_private->pk_init_ctx->dh = NULL;
    opt->opt_private->pk_init_ctx->id = NULL;
    opt->opt_private->pk_init_ctx->clientDHNonce = NULL;
    opt->opt_private->pk_init_ctx->require_binding = 0;
    opt->opt_private->pk_init_ctx->require_eku = 1;
    opt->opt_private->pk_init_ctx->require_krbtgt_otherName = 1;
    opt->opt_private->pk_init_ctx->peer = NULL;

    /* XXX implement krb5_appdefault_strings  */
    if (pool == NULL)
	pool = krb5_config_get_strings(context, NULL,
				       "appdefaults", 
				       "pkinit_pool", 
				       NULL);

    if (pki_revoke == NULL)
	pki_revoke = krb5_config_get_strings(context, NULL,
					     "appdefaults", 
					     "pkinit_revoke", 
					     NULL);

    if (x509_anchors == NULL) {
	krb5_appdefault_string(context, "kinit",
			       krb5_principal_get_realm(context, principal), 
			       "pkinit_anchors", NULL, &anchors);
	x509_anchors = anchors;
    }

    ret = _krb5_pk_load_id(context,
			   &opt->opt_private->pk_init_ctx->id,
			   user_id,
			   x509_anchors,
			   pool,
			   pki_revoke,
			   prompter,
			   prompter_data,
			   password);
    if (ret) {
	free(opt->opt_private->pk_init_ctx);
	opt->opt_private->pk_init_ctx = NULL;
	return ret;
    }

    if ((flags & 2) == 0) {
	const char *moduli_file;
	unsigned long dh_min_bits;

	moduli_file = krb5_config_get_string(context, NULL,
					     "libdefaults",
					     "moduli",
					     NULL);

	dh_min_bits =
	    krb5_config_get_int_default(context, NULL, 0,
					"libdefaults",
					"pkinit_dh_min_bits",
					NULL);

	ret = _krb5_parse_moduli(context, moduli_file, 
				 &opt->opt_private->pk_init_ctx->m);
	if (ret) {
	    _krb5_get_init_creds_opt_free_pkinit(opt);
	    return ret;
	}
	
	opt->opt_private->pk_init_ctx->dh = DH_new();
	if (opt->opt_private->pk_init_ctx->dh == NULL) {
	    krb5_set_error_string(context, "malloc: out of memory");
	    _krb5_get_init_creds_opt_free_pkinit(opt);
	    return ENOMEM;
	}

	ret = select_dh_group(context, opt->opt_private->pk_init_ctx->dh,
			      dh_min_bits, 
			      opt->opt_private->pk_init_ctx->m);
	if (ret) {
	    _krb5_get_init_creds_opt_free_pkinit(opt);
	    return ret;
	}

	if (DH_generate_key(opt->opt_private->pk_init_ctx->dh) != 1) {
	    krb5_set_error_string(context, "pkinit: failed to generate DH key");
	    _krb5_get_init_creds_opt_free_pkinit(opt);
	    return ENOMEM;
	}
    }

    return 0;
#else
    krb5_set_error_string(context, "no support for PKINIT compiled in");
    return EINVAL;
#endif
}

/*
 *
 */

static void
_krb5_pk_copy_error(krb5_context context,
		    hx509_context hx509ctx,
		    int hxret,
		    const char *fmt,
		    ...)
{
    va_list va;
    char *s, *f;

    va_start(va, fmt);
    vasprintf(&f, fmt, va);
    va_end(va);
    if (f == NULL) {
	krb5_clear_error_string(context);
	return;
    }

    s = hx509_get_error_string(hx509ctx, hxret);
    if (s == NULL) {
	krb5_clear_error_string(context);
	free(f);
	return;
    }
    krb5_set_error_string(context, "%s: %s", f, s);
    free(s);
    free(f);
}
