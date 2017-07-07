/*
 * COPYRIGHT (C) 2006,2007
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

#ifndef _PKINIT_CRYPTO_OPENSSL_H
#define _PKINIT_CRYPTO_OPENSSL_H

#include "pkinit.h"

#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/x509.h>
#include <openssl/pkcs7.h>
#include <openssl/pkcs12.h>
#include <openssl/obj_mac.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/asn1.h>
#include <openssl/pem.h>

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
#include <openssl/asn1t.h>
#else
#include <openssl/asn1_mac.h>
#endif

#define DN_BUF_LEN  256
#define MAX_CREDS_ALLOWED 20

struct _pkinit_cred_info {
    char *name;
    X509 *cert;
    EVP_PKEY *key;
#ifndef WITHOUT_PKCS11
    CK_BYTE_PTR cert_id;
    int cert_id_len;
#endif
};
typedef struct _pkinit_cred_info * pkinit_cred_info;

struct _pkinit_identity_crypto_context {
    pkinit_cred_info creds[MAX_CREDS_ALLOWED+1];
    STACK_OF(X509) *my_certs;   /* available user certs */
    char *identity;             /* identity name for user cert */
    int cert_index;             /* cert to use out of available certs*/
    EVP_PKEY *my_key;           /* available user keys if in filesystem */
    STACK_OF(X509) *trustedCAs; /* available trusted ca certs */
    STACK_OF(X509) *intermediateCAs;   /* available intermediate ca certs */
    STACK_OF(X509_CRL) *revoked;    /* available crls */
    int pkcs11_method;
    krb5_prompter_fct prompter;
    void *prompter_data;
#ifndef WITHOUT_PKCS11
    char *p11_module_name;
    CK_SLOT_ID slotid;
    char *token_label;
    char *cert_label;
    /* These are crypto-specific */
    void *p11_module;
    CK_SESSION_HANDLE session;
    CK_FUNCTION_LIST_PTR p11;
    CK_BYTE_PTR cert_id;
    int cert_id_len;
    CK_MECHANISM_TYPE mech;
#endif
    krb5_boolean defer_id_prompt;
    pkinit_deferred_id *deferred_ids;
};

struct _pkinit_plg_crypto_context {
    DH *dh_1024;
    DH *dh_2048;
    DH *dh_4096;
    ASN1_OBJECT *id_pkinit_authData;
    ASN1_OBJECT *id_pkinit_DHKeyData;
    ASN1_OBJECT *id_pkinit_rkeyData;
    ASN1_OBJECT *id_pkinit_san;
    ASN1_OBJECT *id_ms_san_upn;
    ASN1_OBJECT *id_pkinit_KPClientAuth;
    ASN1_OBJECT *id_pkinit_KPKdc;
    ASN1_OBJECT *id_ms_kp_sc_logon;
    ASN1_OBJECT *id_kp_serverAuth;
};

struct _pkinit_req_crypto_context {
    X509 *received_cert;
    DH *dh;
};

#define CERT_MAGIC 0x53534c43
struct _pkinit_cert_data {
    unsigned int magic;
    pkinit_plg_crypto_context plgctx;
    pkinit_req_crypto_context reqctx;
    pkinit_identity_crypto_context idctx;
    pkinit_cred_info cred;
    unsigned int index;	    /* Index of this cred in the creds[] array */
};

#define ITER_MAGIC 0x53534c49
struct _pkinit_cert_iter_data {
    unsigned int magic;
    pkinit_plg_crypto_context plgctx;
    pkinit_req_crypto_context reqctx;
    pkinit_identity_crypto_context idctx;
    unsigned int index;
};

#endif	/* _PKINIT_CRYPTO_OPENSSL_H */
