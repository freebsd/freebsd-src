/*
 * COPYRIGHT (C) 2007
 * THE REGENTS OF THE UNIVERSITY OF MICHIGAN
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of The University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use of distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION
 * FROM THE UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY
 * PURPOSE, AND WITHOUT WARRANTY BY THE UNIVERSITY OF
 * MICHIGAN OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING
 * WITHOUT LIMITATION THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE
 * REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE LIABLE
 * FOR ANY DAMAGES, INCLUDING SPECIAL, INDIRECT, INCIDENTAL, OR
 * CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 */

/*
 * This header defines the cryptographic interface
 */

#ifndef _PKINIT_CRYPTO_H
#define _PKINIT_CRYPTO_H

#include <krb5/krb5.h>
#include <krb5/preauth_plugin.h>
#include <k5-int-pkinit.h>
#include <profile.h>
#include "pkinit_accessor.h"

/*
 * these describe the CMS message types
 */
enum cms_msg_types {
    CMS_SIGN_CLIENT,
    CMS_SIGN_SERVER,
    CMS_ENVEL_SERVER
};

/*
 * storage types for identity information
 */
#define IDTYPE_FILE     1
#define IDTYPE_DIR      2
#define IDTYPE_PKCS11   3
#define IDTYPE_ENVVAR   4
#define IDTYPE_PKCS12   5

/*
 * ca/crl types
 */
#define CATYPE_ANCHORS          1
#define CATYPE_INTERMEDIATES    2
#define CATYPE_CRLS             3

/*
 * The following represent Key Usage values that we
 * may care about in a certificate
 */
#define PKINIT_KU_DIGITALSIGNATURE      0x80000000
#define PKINIT_KU_KEYENCIPHERMENT       0x40000000

/*
 * The following represent Extended Key Usage oid values
 * that we may care about in a certificate
 */
#define PKINIT_EKU_PKINIT               0x80000000
#define PKINIT_EKU_MSSCLOGIN            0x40000000
#define PKINIT_EKU_CLIENTAUTH           0x20000000
#define PKINIT_EKU_EMAILPROTECTION      0x10000000


/* Handle to cert, opaque above crypto interface */
typedef struct _pkinit_cert_info *pkinit_cert_handle;

/* Handle to cert iteration information, opaque above crypto interface */
typedef struct _pkinit_cert_iter_info *pkinit_cert_iter_handle;

#define PKINIT_ITER_NO_MORE	0x11111111  /* XXX */

typedef struct _pkinit_cert_matching_data {
    char *subject_dn;	    /* rfc2253-style subject name string */
    char *issuer_dn;	    /* rfc2253-style issuer name string */
    unsigned int ku_bits;   /* key usage information */
    unsigned int eku_bits;  /* extended key usage information */
    krb5_principal *sans;   /* Null-terminated array of PKINIT SANs */
    char **upns;	    /* Null-terimnated array of UPN SANs */
} pkinit_cert_matching_data;

/*
 * Functions to initialize and cleanup crypto contexts
 */
krb5_error_code pkinit_init_plg_crypto(pkinit_plg_crypto_context *);
void pkinit_fini_plg_crypto(pkinit_plg_crypto_context);

krb5_error_code pkinit_init_req_crypto(pkinit_req_crypto_context *);
void pkinit_fini_req_crypto(pkinit_req_crypto_context);

krb5_error_code pkinit_init_identity_crypto(pkinit_identity_crypto_context *);
void pkinit_fini_identity_crypto(pkinit_identity_crypto_context);
/**Create a pkinit ContentInfo*/
krb5_error_code cms_contentinfo_create
	(krb5_context context,				/* IN */
	pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	pkinit_req_crypto_context req_cryptoctx,	/* IN */
	pkinit_identity_crypto_context id_cryptoctx,	/* IN */
	int cms_msg_type,
	unsigned char *in_data, unsigned int in_length,
	unsigned char **out_data, unsigned int *out_data_len);

/*
 * this function creates a CMS message where eContentType is SignedData
 */
krb5_error_code cms_signeddata_create
	(krb5_context context,				/* IN */
	pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	pkinit_req_crypto_context req_cryptoctx,	/* IN */
	pkinit_identity_crypto_context id_cryptoctx,	/* IN */
	int cms_msg_type,				/* IN
		    specifies CMS_SIGN_CLIENT for client-side CMS message
		    and CMS_SIGN_SERVER for kdc-side */
	unsigned char *auth_pack,			/* IN
		    contains DER encoded AuthPack (CMS_SIGN_CLIENT)
		    or DER encoded DHRepInfo (CMS_SIGN_SERVER) */
	unsigned int auth_pack_len,			/* IN
		    contains length of auth_pack */
	unsigned char **signed_data,			/* OUT
		    for CMS_SIGN_CLIENT receives DER encoded
		    SignedAuthPack (CMS_SIGN_CLIENT) or DER
		    encoded DHInfo (CMS_SIGN_SERVER) */
	unsigned int *signed_data_len);			/* OUT
		    receives length of signed_data */

/*
 * this function verifies a CMS message where eContentType is SignedData
 */
krb5_error_code cms_signeddata_verify
	(krb5_context context,				/* IN */
	pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	pkinit_req_crypto_context req_cryptoctx,	/* IN */
	pkinit_identity_crypto_context id_cryptoctx,	/* IN */
	int cms_msg_type,				/* IN
		    specifies CMS_SIGN_CLIENT for client-side
		    CMS message and CMS_SIGN_SERVER for kdc-side */
	int require_crl_checking,			/* IN
		    specifies whether CRL checking should be
		    strictly enforced, i.e. if no CRLs available
		    for the CA then fail verification.
		    note, if the value is 0, crls are still
		    checked if present */
	unsigned char *signed_data,			/* IN
		    contains DER encoded SignedAuthPack (CMS_SIGN_CLIENT)
		    or DER encoded DHInfo (CMS_SIGN_SERVER) */
	unsigned int signed_data_len,			/* IN
		    contains length of signed_data*/
	unsigned char **auth_pack,			/* OUT
		    receives DER encoded AuthPack (CMS_SIGN_CLIENT)
		    or DER encoded DHRepInfo (CMS_SIGN_SERVER)*/
	unsigned int *auth_pack_len,			/* OUT
		    receives length of auth_pack */
	unsigned char **authz_data,			/* OUT
		    receives required authorization data that
		    contains the verified certificate chain
		    (only used by the KDC) */
	unsigned int *authz_data_len,			/* OUT
		    receives length of authz_data */
	int *is_signed);                                /* OUT
		    receives whether message is signed */

/*
 * this function creates a CMS message where eContentType is EnvelopedData
 */
krb5_error_code cms_envelopeddata_create
	(krb5_context context,				/* IN */
	pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	pkinit_req_crypto_context req_cryptoctx,	/* IN */
	pkinit_identity_crypto_context id_cryptoctx,	/* IN */
	krb5_preauthtype pa_type,			/* IN */
	unsigned char *key_pack,			/* IN
		    contains DER encoded ReplyKeyPack */
	unsigned int key_pack_len,			/* IN
		    contains length of key_pack */
	unsigned char **envel_data,			/* OUT
		    receives DER encoded encKeyPack */
	unsigned int *envel_data_len);			/* OUT
		    receives length of envel_data */

/*
 * this function creates a CMS message where eContentType is EnvelopedData
 */
krb5_error_code cms_envelopeddata_verify
	(krb5_context context,				/* IN */
	pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	pkinit_req_crypto_context req_cryptoctx,	/* IN */
	pkinit_identity_crypto_context id_cryptoctx,	/* IN */
	krb5_preauthtype pa_type,			/* IN */
	int require_crl_checking,			/* IN
		    specifies whether CRL checking should be
		    strictly enforced */
	unsigned char *envel_data,			/* IN
		    contains DER encoded encKeyPack */
	unsigned int envel_data_len,			/* IN
		    contains length of envel_data */
	unsigned char **signed_data,			/* OUT
		    receives ReplyKeyPack */
	unsigned int *signed_data_len);			/* OUT
		    receives length of signed_data */

/*
 * This function retrieves the signer's identity, in a form that could
 * be passed back in to a future invocation of this module as a candidate
 * client identity location.
 */
krb5_error_code crypto_retrieve_signer_identity
	(krb5_context context,				/* IN */
	pkinit_identity_crypto_context id_cryptoctx,	/* IN */
	const char **identity);				/* OUT */

/*
 * this function returns SAN information found in the
 * received certificate.  at least one of pkinit_sans,
 * upn_sans, or kdc_hostnames must be non-NULL.
 */
krb5_error_code crypto_retrieve_cert_sans
	(krb5_context context,				/* IN */
	pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	pkinit_req_crypto_context req_cryptoctx,	/* IN */
	pkinit_identity_crypto_context id_cryptoctx,	/* IN */
	krb5_principal **pkinit_sans,			/* OUT
		    if non-NULL, a null-terminated array of
		    id-pkinit-san values found in the certificate
		    are returned */
	char ***upn_sans,				/* OUT
		    if non-NULL, a null-terminated array of
		    id-ms-upn-san values found in the certificate
		    are returned */
	unsigned char ***kdc_hostname);			/* OUT
		    if non-NULL, a null-terminated array of
		    dNSName (hostname) SAN values found in the
		    certificate are returned */

/*
 * this function checks for acceptable key usage values
 * in the received certificate.
 *
 * when checking a received kdc certificate, it looks for
 * the kpKdc key usage.  if allow_secondary_usage is
 * non-zero, it will also accept kpServerAuth.
 *
 * when checking a received user certificate, it looks for
 * kpClientAuth key usage.  if allow_secondary_usage is
 * non-zero, it will also accept id-ms-sc-logon EKU.
 *
 * this function must also assert that the digitalSignature
 * key usage is consistent.
 */
krb5_error_code crypto_check_cert_eku
	(krb5_context context,				/* IN */
	pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	pkinit_req_crypto_context req_cryptoctx,	/* IN */
	pkinit_identity_crypto_context id_cryptoctx,	/* IN */
	int checking_kdc_cert,				/* IN
		    specifies if the received certificate is
		    a KDC certificate (non-zero),
		    or a user certificate (zero) */
	int allow_secondary_usage,			/* IN
		    specifies if the secondary key usage
		    should be accepted or not (see above) */
	int *eku_valid);				/* OUT
		    receives non-zero if an acceptable EKU was found */

/*
 * this functions takes in generated DH secret key and converts
 * it in to a kerberos session key. it takes into the account the
 * enc type and then follows the procedure specified in the RFC p 22.
 */
krb5_error_code pkinit_octetstring2key
	(krb5_context context,				/* IN */
	krb5_enctype etype,				/* IN
		    specifies the enc type */
	unsigned char *key,				/* IN
		    contains the DH secret key */
	unsigned int key_len,				/* IN
		    contains length of key */
	krb5_keyblock * krb5key);			/* OUT
		    receives kerberos session key */

/*
 * this function implements clients first part of the DH protocol.
 * client selects its DH parameters and pub key
 */
krb5_error_code client_create_dh
	(krb5_context context,				/* IN */
	pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	pkinit_req_crypto_context req_cryptoctx,	/* IN */
	pkinit_identity_crypto_context id_cryptoctx,	/* IN */
	int dh_size,					/* IN
		    specifies the DH modulous, eg 1024, 2048, or 4096 */
	krb5_data *spki_out);				/* OUT
		    receives SubjectPublicKeyInfo encoding */

/*
 * this function completes client's the DH protocol. client
 * processes received DH pub key from the KDC and computes
 * the DH secret key
 */
krb5_error_code client_process_dh
	(krb5_context context,				/* IN */
	pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	pkinit_req_crypto_context req_cryptoctx,	/* IN */
	pkinit_identity_crypto_context id_cryptoctx,	/* IN */
	unsigned char *dh_pubkey,			/* IN
		    contains client's DER encoded DH pub key */
	unsigned int dh_pubkey_len,			/* IN
		    contains length of dh_pubkey */
	unsigned char **client_key_out,			/* OUT
		    receives DH secret key */
	unsigned int *client_key_len_out);		/* OUT
		    receives length of DH secret key */

/*
 * this function implements the KDC first part of the DH protocol.
 * it decodes the client's DH parameters and pub key and checks
 * if they are acceptable.
 */
krb5_error_code server_check_dh
	(krb5_context context,				/* IN */
	pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	pkinit_req_crypto_context req_cryptoctx,	/* IN */
	pkinit_identity_crypto_context id_cryptoctx,	/* IN */
	const krb5_data *client_spki,			/* IN
		    SubjectPublicKeyInfo encoding from client */
	int minbits);					/* IN
		    the minimum number of key bits acceptable */

/*
 * this function completes the KDC's DH protocol. The KDC generates
 * its DH pub key and computes the DH secret key
 */
krb5_error_code server_process_dh
	(krb5_context context,				/* IN */
	pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	pkinit_req_crypto_context req_cryptoctx,	/* IN */
	pkinit_identity_crypto_context id_cryptoctx,	/* IN */
	unsigned char **dh_pubkey_out,			/* OUT
		    receives KDC's DER encoded DH pub key */
	unsigned int *dh_pubkey_len_out,		/* OUT
		    receives length of dh_pubkey */
	unsigned char **server_key_out,			/* OUT
		    receives DH secret key */
	unsigned int *server_key_len_out);		/* OUT
		    receives length of DH secret key */

/*
 * this functions takes in crypto specific representation of
 * supportedCMSTypes and creates a list of
 * krb5_algorithm_identifier
 */
krb5_error_code create_krb5_supportedCMSTypes
	(krb5_context context,				/* IN */
	pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	pkinit_req_crypto_context req_cryptoctx,	/* IN */
	pkinit_identity_crypto_context id_cryptoctx,	/* IN */
	krb5_algorithm_identifier ***supportedCMSTypes); /* OUT */

/*
 * this functions takes in crypto specific representation of
 * trustedCertifiers and creates a list of
 * krb5_external_principal_identifier
 */
krb5_error_code create_krb5_trustedCertifiers
	(krb5_context context,				/* IN */
	pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	pkinit_req_crypto_context req_cryptoctx,	/* IN */
	pkinit_identity_crypto_context id_cryptoctx,	/* IN */
	krb5_external_principal_identifier ***trustedCertifiers); /* OUT */

/*
 * this functions takes in crypto specific representation of the
 * KDC's certificate and creates a DER encoded kdcPKId
 */
krb5_error_code create_issuerAndSerial
	(krb5_context context,				/* IN */
	pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	pkinit_req_crypto_context req_cryptoctx,	/* IN */
	pkinit_identity_crypto_context id_cryptoctx,	/* IN */
	unsigned char **kdcId_buf,			/* OUT
		    receives DER encoded kdcPKId */
	unsigned int *kdcId_len);			/* OUT
		    receives length of encoded kdcPKId */

/*
 * These functions manipulate the deferred-identities list in the identity
 * context, which is opaque outside of the crypto-specific bits.
 */
const pkinit_deferred_id * crypto_get_deferred_ids
	(krb5_context context, pkinit_identity_crypto_context id_cryptoctx);
krb5_error_code crypto_set_deferred_id
	(krb5_context context,
	 pkinit_identity_crypto_context id_cryptoctx,
	 const char *identity, const char *password);

/*
 * process the values from idopts and obtain the cert(s)
 * specified by those options, populating the id_cryptoctx.
 */
krb5_error_code crypto_load_certs
	(krb5_context context,				/* IN */
	pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	pkinit_req_crypto_context req_cryptoctx,	/* IN */
	pkinit_identity_opts *idopts,			/* IN */
	pkinit_identity_crypto_context id_cryptoctx,	/* IN/OUT */
	krb5_principal princ,				/* IN */
	krb5_boolean defer_id_prompts);			/* IN */

/*
 * Free up information held from crypto_load_certs()
 */
krb5_error_code crypto_free_cert_info
	(krb5_context context,
	pkinit_plg_crypto_context plg_cryptoctx,
	pkinit_req_crypto_context req_cryptoctx,
	pkinit_identity_crypto_context id_cryptoctx);


/*
 * Get a null-terminated list of certificate matching data objects for the
 * certificates loaded in id_cryptoctx.
 */
krb5_error_code
crypto_cert_get_matching_data(krb5_context context,
			      pkinit_plg_crypto_context plg_cryptoctx,
			      pkinit_req_crypto_context req_cryptoctx,
			      pkinit_identity_crypto_context id_cryptoctx,
			      pkinit_cert_matching_data ***md_out);

/*
 * Free a matching data object.
 */
void
crypto_cert_free_matching_data(krb5_context context,
			       pkinit_cert_matching_data *md);

/*
 * Free a list of matching data objects.
 */
void
crypto_cert_free_matching_data_list(krb5_context context,
				    pkinit_cert_matching_data **matchdata);

/*
 * Choose one of the certificates loaded in idctx to use for PKINIT client
 * operations.  cred_index must be an index into the array of matching objects
 * returned by crypto_cert_get_matching_data().
 */
krb5_error_code
crypto_cert_select(krb5_context context, pkinit_identity_crypto_context idctx,
		   size_t cred_index);

/*
 * Select the default certificate as "the chosen one"
 */
krb5_error_code crypto_cert_select_default
	(krb5_context context,				/* IN */
	pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	pkinit_req_crypto_context req_cryptoctx,	/* IN */
	pkinit_identity_crypto_context id_cryptoctx);	/* IN */

/*
 * process the values from idopts and obtain the anchor or
 * intermediate certificates, or crls specified by idtype,
 * catype, and id
 */
krb5_error_code crypto_load_cas_and_crls
	(krb5_context context,				/* IN */
	pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	pkinit_req_crypto_context req_cryptoctx,	/* IN */
	pkinit_identity_opts *idopts,			/* IN */
	pkinit_identity_crypto_context id_cryptoctx,	/* IN/OUT */
	int idtype,					/* IN
		    defines the storage type (file, directory, etc) */
	int catype,					/* IN
		    defines the ca type (anchor, intermediate, crls) */
	char *id);					/* IN
		    defines the location (filename, directory name, etc) */

/*
 * on the client, obtain the kdc's certificate to include
 * in a request
 */
krb5_error_code pkinit_get_kdc_cert
	(krb5_context context,				/* IN */
	pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	pkinit_req_crypto_context req_cryptoctx,	/* IN */
	pkinit_identity_crypto_context id_cryptoctx,	/* IN/OUT */
	krb5_principal princ);				/* IN */

/*
 * this function creates edata that contains TD-DH-PARAMETERS
 */
krb5_error_code pkinit_create_td_dh_parameters
	(krb5_context context,				/* IN */
	pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	pkinit_req_crypto_context req_cryptoctx,	/* IN */
	pkinit_identity_crypto_context id_cryptoctx,	/* IN */
	pkinit_plg_opts *opts,				/* IN */
	krb5_pa_data ***e_data_out);			/* OUT */

/*
 * this function processes edata that contains TD-DH-PARAMETERS.
 * the client processes the received acceptable by KDC DH
 * parameters and picks the first acceptable to it. it matches
 * them against the known DH parameters.
 */
krb5_error_code pkinit_process_td_dh_params
	(krb5_context context,				/* IN */
	pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	pkinit_req_crypto_context req_cryptoctx,	/* IN */
	pkinit_identity_crypto_context id_cryptoctx,	/* IN */
	krb5_algorithm_identifier **algId,		/* IN */
	int *new_dh_size);				/* OUT
		    receives the new DH modulus to use in the new AS-REQ */

/*
 * this function creates edata that contains TD-INVALID-CERTIFICATES
 */
krb5_error_code pkinit_create_td_invalid_certificate
	(krb5_context context,				/* IN */
	pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	pkinit_req_crypto_context req_cryptoctx,	/* IN */
	pkinit_identity_crypto_context id_cryptoctx,	/* IN */
	krb5_pa_data ***e_data_out);			/* OUT */

/*
 * this function creates edata that contains TD-TRUSTED-CERTIFIERS
 */
krb5_error_code pkinit_create_td_trusted_certifiers
	(krb5_context context,				/* IN */
	pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	pkinit_req_crypto_context req_cryptoctx,	/* IN */
	pkinit_identity_crypto_context id_cryptoctx,	/* IN */
	krb5_pa_data ***e_data_out);			/* OUT */

/*
 * this function processes edata that contains either
 * TD-TRUSTED-CERTIFICATES or TD-INVALID-CERTIFICATES.
 * current implementation only decodes the received message
 * but does not act on it
 */
krb5_error_code pkinit_process_td_trusted_certifiers
	(krb5_context context,				/* IN */
	pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	pkinit_req_crypto_context req_cryptoctx,	/* IN */
	pkinit_identity_crypto_context id_cryptoctx,	/* IN */
	krb5_external_principal_identifier **trustedCertifiers, /* IN */
	int td_type);					/* IN */

/*
 * this function checks if the received kdcPKId matches
 * the KDC's certificate
 */
krb5_error_code pkinit_check_kdc_pkid
	(krb5_context context,				/* IN */
	pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	pkinit_req_crypto_context req_cryptoctx,	/* IN */
	pkinit_identity_crypto_context id_cryptoctx,	/* IN */
	unsigned char *pdid_buf,			/* IN
		    contains DER encoded kdcPKId */
	unsigned int pkid_len,				/* IN
		    contains length of pdid_buf */
	int *valid_kdcPkId);				/* OUT
		    1 if kdcPKId matches, otherwise 0 */

krb5_error_code pkinit_identity_set_prompter
	(pkinit_identity_crypto_context id_cryptoctx,	/* IN */
	krb5_prompter_fct prompter,			/* IN */
	void *prompter_data);				/* IN */

krb5_error_code
pkinit_alg_agility_kdf(krb5_context context,
                       krb5_data *secret,
                       krb5_data *alg_oid,
                       krb5_const_principal party_u_info,
                       krb5_const_principal party_v_info,
                       krb5_enctype enctype,
                       krb5_data *as_req,
                       krb5_data *pk_as_rep,
                       krb5_keyblock *key_block);

extern const krb5_data sha1_id;
extern const krb5_data sha256_id;
extern const krb5_data sha512_id;
extern const krb5_data oakley_1024;
extern const krb5_data oakley_2048;
extern const krb5_data oakley_4096;

/**
 * An ordered set of OIDs, stored as krb5_data, of KDF algorithms
 * supported by this implementation. The order of this array controls
 * the order in which the server will pick.
 */
extern krb5_data const * const supported_kdf_alg_ids[];

/* CMS signature algorithms supported by this implementation, in order of
 * decreasing preference. */
extern krb5_data const * const supported_cms_algs[];

krb5_error_code
crypto_encode_der_cert(krb5_context context, pkinit_req_crypto_context reqctx,
		       uint8_t **der_out, size_t *der_len);

krb5_error_code
crypto_req_cert_matching_data(krb5_context context,
			      pkinit_plg_crypto_context plgctx,
			      pkinit_req_crypto_context reqctx,
			      pkinit_cert_matching_data **md_out);

#endif	/* _PKINIT_CRYPTO_H */
