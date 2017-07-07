/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *  Copyright (c) 2006,2007,2010,2011 Red Hat, Inc.
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 *  * Neither the name of Red Hat, Inc., nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 *  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *  PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 *  OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "k5-platform.h"
#include "k5-buf.h"
#include "k5-utf8.h"
#include "krb5.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include <prerror.h>
#include <prmem.h>
#include <prprf.h>
#include <nss.h>
#include <cert.h>
#include <certdb.h>
#include <ciferfam.h>
#include <cms.h>
#include <keyhi.h>
#include <nssb64.h>
#include <ocsp.h>
#include <p12plcy.h>
#include <p12.h>
#include <pk11pub.h>
#include <pkcs12.h>
#include <secerr.h>
#include <secmodt.h>
#include <secmod.h>
#include <secoidt.h>
#include <secoid.h>

/* Avoid including our local copy of "pkcs11.h" from one of the local headers,
 * since the definitions we want to use are going to be the ones that NSS
 * provides. */

#define PKCS11_H
#include "pkinit.h"
#include "pkinit_crypto.h"

/* We should probably avoid using the default location for certificate trusts,
 * unless we can be sure that the list of trusted roots isn't being shared
 * with general-purpose SSL/TLS configuration, even though we're leaning on
 * SSL/TLS trust settings. */
#define DEFAULT_CONFIGDIR "/etc/pki/nssdb"

/* #define DEBUG_DER "/usr/lib64/nss/unsupported-tools/derdump" */
/* #define DEBUG_SENSITIVE */

/* Define to create a temporary on-disk database when we need to import PKCS12
 * identities. */
#define PKCS12_HACK

/* Prefix to mark the nicknames we make up for pkcs12 bundles that don't
 * include a friendly name. */
#define PKCS12_PREFIX "pkinit-pkcs12"

/* The library name of the NSSPEM module. */
#define PEM_MODULE "nsspem"

/* Forward declaration. */
static krb5_error_code cert_retrieve_cert_sans(krb5_context context,
                                               CERTCertificate *cert,
                                               krb5_principal **pkinit_sans,
                                               krb5_principal **upn_sans,
                                               unsigned char ***kdc_hostname);
static void crypto_update_signer_identity(krb5_context,
                                          pkinit_identity_crypto_context);

/* DomainParameters: RFC 2459, 7.3.2. */
struct domain_parameters {
    SECItem p, g, q, j;
    struct validation_parms *validation_parms;
};

/* Plugin and request state. */
struct _pkinit_plg_crypto_context {
    PLArenaPool *pool;
    NSSInitContext *ncontext;
};

struct _pkinit_req_crypto_context {
    PLArenaPool *pool;
    SECKEYPrivateKey *client_dh_privkey;        /* used by clients */
    SECKEYPublicKey *client_dh_pubkey;  /* used by clients */
    struct domain_parameters client_dh_params;  /* used by KDCs */
    CERTCertificate *peer_cert; /* the other party */
};

struct _pkinit_identity_crypto_context {
    PLArenaPool *pool;
    const char *identity;
    SECMODModule *pem_module;   /* used for FILE: and DIR: */
    struct _pkinit_identity_crypto_module {
        char *name;
        char *spec;
        SECMODModule *module;
    } **id_modules;             /* used for PKCS11: */
    struct _pkinit_identity_crypto_userdb {
        char *name;
        PK11SlotInfo *userdb;
    } **id_userdbs;             /* used for NSS: */
    struct _pkinit_identity_crypto_p12slot {
        char *p12name;
        PK11SlotInfo *slot;
    } id_p12_slot;             /* used for PKCS12: */
    struct _pkinit_identity_crypto_file {
        char *name;
        PK11GenericObject *obj;
        CERTCertificate *cert;
    } **id_objects;             /* used with FILE: and DIR: */
    SECItem **id_crls;
    CERTCertList *id_certs, *ca_certs;
    CERTCertificate *id_cert;
    struct {
        krb5_context context;
        krb5_prompter_fct prompter;
        void *prompter_data;
        const char *identity;
    } pwcb_args;
    krb5_boolean defer_id_prompt;
    pkinit_deferred_id *deferred_ids;
    krb5_boolean defer_with_dummy_password;
};

struct _pkinit_cert_info {      /* aka _pkinit_cert_handle */
    PLArenaPool *pool;
    struct _pkinit_identity_crypto_context *id_cryptoctx;
    CERTCertificate *cert;
};

struct _pkinit_cert_iter_info { /* aka _pkinit_cert_iter_handle */
    PLArenaPool *pool;
    struct _pkinit_identity_crypto_context *id_cryptoctx;
    CERTCertListNode *node;
};

/* Protocol elements that we need to encode or decode. */

/* DH parameters: draft-ietf-cat-kerberos-pk-init-08.txt, 3.1.2.2. */
struct dh_parameters {
    SECItem p, g, private_value_length;
};
static const SEC_ASN1Template dh_parameters_template[] = {
    {
        SEC_ASN1_SEQUENCE,
        0,
        NULL,
        sizeof(struct dh_parameters),
    },
    {
        SEC_ASN1_INTEGER,
        offsetof(struct dh_parameters, p),
        &SEC_IntegerTemplate,
        sizeof(SECItem),
    },
    {
        SEC_ASN1_INTEGER,
        offsetof(struct dh_parameters, g),
        &SEC_IntegerTemplate,
        sizeof(SECItem),
    },
    {
        SEC_ASN1_INTEGER | SEC_ASN1_OPTIONAL,
        offsetof(struct dh_parameters, private_value_length),
        &SEC_IntegerTemplate,
        sizeof(SECItem),
    },
    {0, 0, NULL, 0}
};

/* ValidationParms: RFC 2459, 7.3.2. */
struct validation_parms {
    SECItem seed, pgen_counter;
};
static const SEC_ASN1Template validation_parms_template[] = {
    {
        SEC_ASN1_SEQUENCE,
        0,
        NULL,
        sizeof(struct validation_parms),
    },
    {
        SEC_ASN1_BIT_STRING,
        offsetof(struct validation_parms, seed),
        &SEC_BitStringTemplate,
        sizeof(SECItem),
    },
    {
        SEC_ASN1_INTEGER,
        offsetof(struct validation_parms, pgen_counter),
        &SEC_IntegerTemplate,
        sizeof(SECItem),
    },
    {0, 0, NULL, 0}
};

/* DomainParameters: RFC 2459, 7.3.2. */
struct domain_parameters;
static const SEC_ASN1Template domain_parameters_template[] = {
    {
        SEC_ASN1_SEQUENCE,
        0,
        NULL,
        sizeof(struct domain_parameters),
    },
    {
        SEC_ASN1_INTEGER,
        offsetof(struct domain_parameters, p),
        &SEC_IntegerTemplate,
        sizeof(SECItem),
    },
    {
        SEC_ASN1_INTEGER,
        offsetof(struct domain_parameters, g),
        &SEC_IntegerTemplate,
        sizeof(SECItem),
    },
    {
        SEC_ASN1_INTEGER,
        offsetof(struct domain_parameters, q),
        &SEC_IntegerTemplate,
        sizeof(SECItem),
    },
    {
        SEC_ASN1_INTEGER | SEC_ASN1_OPTIONAL,
        offsetof(struct domain_parameters, j),
        &SEC_IntegerTemplate,
        sizeof(SECItem),
    },
    {
        SEC_ASN1_INLINE | SEC_ASN1_POINTER | SEC_ASN1_OPTIONAL,
        offsetof(struct domain_parameters, validation_parms),
        &validation_parms_template,
        sizeof(struct validation_parms *),
    },
    {0, 0, NULL, 0}
};

/* IssuerAndSerialNumber: RFC 3852, 10.2.4. */
struct issuer_and_serial_number {
    SECItem issuer;
    SECItem serial;
};
static const SEC_ASN1Template issuer_and_serial_number_template[] = {
    {
        SEC_ASN1_SEQUENCE,
        0,
        NULL,
        sizeof(struct issuer_and_serial_number),
    },
    {
        SEC_ASN1_ANY,
        offsetof(struct issuer_and_serial_number, issuer),
        &SEC_AnyTemplate,
        sizeof(SECItem),
    },
    {
        SEC_ASN1_INTEGER,
        offsetof(struct issuer_and_serial_number, serial),
        &SEC_IntegerTemplate,
        sizeof(SECItem),
    },
    {0, 0, NULL, 0}
};

/* KerberosString: RFC 4120, 5.2.1. */
static const SEC_ASN1Template kerberos_string_template[] = {
    {
        SEC_ASN1_GENERAL_STRING,
        0,
        NULL,
        sizeof(SECItem),
    }
};

/* Realm: RFC 4120, 5.2.2. */
struct realm {
    SECItem name;
};
static const SEC_ASN1Template realm_template[] = {
    {
        SEC_ASN1_GENERAL_STRING,
        0,
        NULL,
        sizeof(SECItem),
    }
};

/* PrincipalName: RFC 4120, 5.2.2. */
static const SEC_ASN1Template sequence_of_kerberos_string_template[] = {
    {
        SEC_ASN1_SEQUENCE_OF,
        0,
        &kerberos_string_template,
        0,
    }
};

struct principal_name {
    SECItem name_type;
    SECItem **name_string;
};
static const SEC_ASN1Template principal_name_template[] = {
    {
        SEC_ASN1_SEQUENCE,
        0,
        NULL,
        sizeof(struct principal_name),
    },
    {
        SEC_ASN1_CONTEXT_SPECIFIC | 0 | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT,
        offsetof(struct principal_name, name_type),
        &SEC_IntegerTemplate,
        sizeof(SECItem),
    },
    {
        SEC_ASN1_CONTEXT_SPECIFIC | 1 | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT,
        offsetof(struct principal_name, name_string),
        sequence_of_kerberos_string_template,
        sizeof(struct SECItem **),
    },
    {0, 0, NULL, 0},
};

/* KRB5PrincipalName: RFC 4556, 3.2.2. */
struct kerberos_principal_name {
    SECItem realm;
    struct principal_name principal_name;
};
static const SEC_ASN1Template kerberos_principal_name_template[] = {
    {
        SEC_ASN1_SEQUENCE,
        0,
        NULL,
        sizeof(struct kerberos_principal_name),
    },
    {
        SEC_ASN1_CONTEXT_SPECIFIC | 0 | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT,
        offsetof(struct kerberos_principal_name, realm),
        &realm_template,
        sizeof(struct realm),
    },
    {
        SEC_ASN1_CONTEXT_SPECIFIC | 1 | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT,
        offsetof(struct kerberos_principal_name, principal_name),
        &principal_name_template,
        sizeof(struct principal_name),
    },
    {0, 0, NULL, 0}
};

/* ContentInfo: RFC 3852, 3. */
struct content_info {
    SECItem content_type, content;
};
static const SEC_ASN1Template content_info_template[] = {
    {
        SEC_ASN1_SEQUENCE,
        0,
        NULL,
        sizeof(struct content_info),
    },
    {
        SEC_ASN1_OBJECT_ID,
        offsetof(struct content_info, content_type),
        &SEC_ObjectIDTemplate,
        sizeof(SECItem),
    },
    {
        SEC_ASN1_CONTEXT_SPECIFIC | 0 | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT,
        offsetof(struct content_info, content),
        &SEC_OctetStringTemplate,
        sizeof(SECItem),
    },
    {0, 0, NULL, 0}
};

/* OIDs. */
static unsigned char oid_pkinit_key_purpose_client_bytes[] = {
    0x2b, 0x06, 0x01, 0x05, 0x02, 0x03, 0x04
};
static SECItem pkinit_kp_client = {
    siDEROID,
    oid_pkinit_key_purpose_client_bytes,
    7,
};
static unsigned char oid_pkinit_key_purpose_kdc_bytes[] = {
    0x2b, 0x06, 0x01, 0x05, 0x02, 0x03, 0x05
};
static SECItem pkinit_kp_kdc = {
    siDEROID,
    oid_pkinit_key_purpose_kdc_bytes,
    7,
};
static unsigned char oid_ms_sc_login_key_purpose_bytes[] = {
    0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x14, 0x02, 0x02
};
static SECItem pkinit_kp_mssclogin = {
    siDEROID,
    oid_ms_sc_login_key_purpose_bytes,
    10,
};
static unsigned char oid_pkinit_name_type_principal_bytes[] = {
    0x2b, 0x06, 0x01, 0x05, 0x02, 0x02
};
static SECItem pkinit_nt_principal = {
    siDEROID,
    oid_pkinit_name_type_principal_bytes,
    6,
};
static unsigned char oid_pkinit_name_type_upn_bytes[] = {
    0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x14, 0x02, 0x03
};
static SECItem pkinit_nt_upn = {
    siDEROID,
    oid_pkinit_name_type_upn_bytes,
    10,
};

static SECOidTag
get_pkinit_data_auth_data_tag(void)
{
    static unsigned char oid_pkinit_auth_data_bytes[] = {
        0x2b, 0x06, 0x01, 0x05, 0x02, 0x03, 0x01
    };
    static SECOidData oid_pkinit_auth_data = {
        {
            siDEROID,
            oid_pkinit_auth_data_bytes,
            7,
        },
        SEC_OID_UNKNOWN,
        "PKINIT Client Authentication Data",
        CKM_INVALID_MECHANISM,
        UNSUPPORTED_CERT_EXTENSION,
    };
    if (oid_pkinit_auth_data.offset == SEC_OID_UNKNOWN)
        oid_pkinit_auth_data.offset = SECOID_AddEntry(&oid_pkinit_auth_data);
    return oid_pkinit_auth_data.offset;
}

static SECOidTag
get_pkinit_data_auth_data9_tag(void)
{
    static unsigned char oid_pkinit_auth_data9_bytes[] =
        { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01 };
    static SECOidData oid_pkinit_auth_data9 = {
        {
            siDEROID,
            oid_pkinit_auth_data9_bytes,
            9,
        },
        SEC_OID_UNKNOWN,
        "PKINIT Client Authentication Data (Draft 9)",
        CKM_INVALID_MECHANISM,
        UNSUPPORTED_CERT_EXTENSION,
    };
    if (oid_pkinit_auth_data9.offset == SEC_OID_UNKNOWN)
        oid_pkinit_auth_data9.offset = SECOID_AddEntry(&oid_pkinit_auth_data9);
    return oid_pkinit_auth_data9.offset;
}

static SECOidTag
get_pkinit_data_rkey_data_tag(void)
{
    static unsigned char oid_pkinit_rkey_data_bytes[] = {
        0x2b, 0x06, 0x01, 0x05, 0x02, 0x03, 0x03
    };
    static SECOidData oid_pkinit_rkey_data = {
        {
            siDEROID,
            oid_pkinit_rkey_data_bytes,
            7,
        },
        SEC_OID_UNKNOWN,
        "PKINIT Reply Key Data",
        CKM_INVALID_MECHANISM,
        UNSUPPORTED_CERT_EXTENSION,
    };
    if (oid_pkinit_rkey_data.offset == SEC_OID_UNKNOWN)
        oid_pkinit_rkey_data.offset = SECOID_AddEntry(&oid_pkinit_rkey_data);
    return oid_pkinit_rkey_data.offset;
}

static SECOidTag
get_pkinit_data_dhkey_data_tag(void)
{
    static unsigned char oid_pkinit_dhkey_data_bytes[] = {
        0x2b, 0x06, 0x01, 0x05, 0x02, 0x03, 0x02
    };
    static SECOidData oid_pkinit_dhkey_data = {
        {
            siDEROID,
            oid_pkinit_dhkey_data_bytes,
            7,
        },
        SEC_OID_UNKNOWN,
        "PKINIT DH Reply Key Data",
        CKM_INVALID_MECHANISM,
        UNSUPPORTED_CERT_EXTENSION,
    };
    if (oid_pkinit_dhkey_data.offset == SEC_OID_UNKNOWN)
        oid_pkinit_dhkey_data.offset = SECOID_AddEntry(&oid_pkinit_dhkey_data);
    return oid_pkinit_dhkey_data.offset;
}

static SECItem *
get_oid_from_tag(SECOidTag tag)
{
    SECOidData *data;
    data = SECOID_FindOIDByTag(tag);
    if (data != NULL)
        return &data->oid;
    else
        return NULL;
}

#ifdef DEBUG_DER
static void
derdump(unsigned char *data, unsigned int length)
{
    FILE *p;

    p = popen(DEBUG_DER, "w");
    if (p != NULL) {
        fwrite(data, 1, length, p);
        pclose(p);
    }
}
#endif
#ifdef DEBUG_CMS
static void
cmsdump(unsigned char *data, unsigned int length)
{
    FILE *p;

    p = popen(DEBUG_CMS, "w");
    if (p != NULL) {
        fwrite(data, 1, length, p);
        pclose(p);
    }
}
#endif

/* A password-prompt callback for NSS that calls the libkrb5 callback. */
static char *
crypto_pwfn(const char *what, PRBool is_hardware, CK_FLAGS token_flags,
            PRBool retry, void *arg)
{
    int ret;
    pkinit_identity_crypto_context id;
    krb5_prompt prompt;
    krb5_prompt_type prompt_types[2];
    krb5_data reply;
    char *text, *answer;
    const char *warning, *password;
    size_t text_size;
    void *data;

    /* We only want to be called once. */
    if (retry)
        return NULL;
    /* We need our callback arguments. */
    if (arg == NULL)
        return NULL;
    id = arg;

    /* If we need to warn about the PIN, figure out the text. */
    if (token_flags & CKF_USER_PIN_LOCKED)
        warning = "PIN locked";
    else if (token_flags & CKF_USER_PIN_FINAL_TRY)
        warning = "PIN final try";
    else if (token_flags & CKF_USER_PIN_COUNT_LOW)
        warning = "PIN count low";
    else
        warning = NULL;

    /*
     * If we have the name of an identity here, then we're either supposed to
     * save its name, or attempt to use a password, if one was supplied.
     */
    if (id->pwcb_args.identity != NULL) {
        if (id->defer_id_prompt) {
            /* If we're in the defer-prompts step, just save the identity name
             * and "fail". */
            if (!is_hardware)
                token_flags = 0;
            pkinit_set_deferred_id(&id->deferred_ids, id->pwcb_args.identity,
                                   token_flags, NULL);
            if (id->defer_with_dummy_password) {
                /* Return a useless result. */
                answer = PR_Malloc(1);
                if (answer != NULL) {
                    *answer = '\0';
                    return answer;
                }
            }
        } else {
            /* Check if we already have a password for this identity.  If so,
             * just return a copy of it. */
            password = pkinit_find_deferred_id(id->deferred_ids,
                                               id->pwcb_args.identity);
            if (password != NULL) {
                /* The result will be freed with PR_Free, so return a copy. */
                text_size = strlen(password) + 1;
                answer = PR_Malloc(text_size);
                if (answer != NULL) {
                    memcpy(answer, password, text_size);
                    pkiDebug("%s: returning %ld-char answer\n", __FUNCTION__,
                             (long)strlen(answer));
                    return answer;
                }
            }
        }
    }

    if (id->pwcb_args.prompter == NULL)
        return NULL;

    /* Set up the prompt. */
    text_size = strlen(what) + 100;
    text = PORT_ArenaZAlloc(id->pool, text_size);
    if (text == NULL) {
        pkiDebug("out of memory");
        return NULL;
    }
    if (is_hardware) {
        if (warning != NULL)
            snprintf(text, text_size, "%s PIN (%s)", what, warning);
        else
            snprintf(text, text_size, "%s PIN", what);
    } else {
        snprintf(text, text_size, "%s %s", _("Pass phrase for"), what);
    }
    memset(&prompt, 0, sizeof(prompt));
    prompt.prompt = text;
    prompt.hidden = 1;
    prompt.reply = &reply;
    reply.length = 256;
    data = malloc(reply.length);
    reply.data = data;
    what = NULL;
    answer = NULL;

    /* Call the prompter callback. */
    prompt_types[0] = KRB5_PROMPT_TYPE_PREAUTH;
    prompt_types[1] = 0;
    (*k5int_set_prompt_types)(id->pwcb_args.context, prompt_types);
    fflush(NULL);
    ret = (*id->pwcb_args.prompter)(id->pwcb_args.context,
                                    id->pwcb_args.prompter_data,
                                    what, answer, 1, &prompt);
    (*k5int_set_prompt_types)(id->pwcb_args.context, NULL);
    answer = NULL;
    if ((ret == 0) && (reply.data != NULL)) {
        /* The result will be freed with PR_Free, so return a copy. */
        answer = PR_Malloc(reply.length + 1);
        memcpy(answer, reply.data, reply.length);
        answer[reply.length] = '\0';
        answer[strcspn(answer, "\r\n")] = '\0';
#ifdef DEBUG_SENSITIVE
        pkiDebug("%s: returning \"%s\"\n", __FUNCTION__, answer);
#else
        pkiDebug("%s: returning %ld-char answer\n", __FUNCTION__,
                 (long) strlen(answer));
#endif
    }

    if (reply.data == data)
        free(reply.data);

    return answer;
}

/* A password-prompt callback for NSS that calls the libkrb5 callback. */
static char *
crypto_pwcb(PK11SlotInfo *slot, PRBool retry, void *arg)
{
    pkinit_identity_crypto_context id;
    const char *what = NULL;
    CK_TOKEN_INFO tinfo;
    CK_FLAGS tflags;

    if (PK11_GetTokenInfo(slot, &tinfo) == SECSuccess)
        tflags = tinfo.flags;
    else
        tflags = 0;
    if (arg != NULL) {
        id = arg;
        what = id->pwcb_args.identity;
    }
    return crypto_pwfn((what != NULL) ? what : PK11_GetTokenName(slot),
                       PK11_IsHW(slot), tflags, retry, arg);
}

/*
 * Make sure we're using our callback, and set up the callback data.
 */
static void *
crypto_pwcb_prep(pkinit_identity_crypto_context id_cryptoctx,
                 const char *identity, krb5_context context)
{
    PK11_SetPasswordFunc(crypto_pwcb);
    id_cryptoctx->pwcb_args.context = context;
    id_cryptoctx->pwcb_args.identity = identity;
    return id_cryptoctx;
}

krb5_error_code
pkinit_init_identity_crypto(pkinit_identity_crypto_context *id_cryptoctx)
{
    PLArenaPool *pool;
    pkinit_identity_crypto_context id;

    pkiDebug("%s\n", __FUNCTION__);
    pool = PORT_NewArena(sizeof(double));
    if (pool == NULL)
        return ENOMEM;
    id = PORT_ArenaZAlloc(pool, sizeof(*id));
    if (id == NULL) {
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
    id->pool = pool;
    id->id_certs = CERT_NewCertList();
    id->ca_certs = CERT_NewCertList();
    if ((id->id_certs != NULL) && (id->ca_certs != NULL)) {
        *id_cryptoctx = id;
        return 0;
    }
    if (id->ca_certs != NULL)
        CERT_DestroyCertList(id->ca_certs);
    if (id->id_certs != NULL)
        CERT_DestroyCertList(id->id_certs);
    PORT_FreeArena(pool, PR_TRUE);
    return ENOMEM;
}

/* Return the slot which we'll use for holding imported PKCS12 certificates
 * and keys.  Open the module if we need to, first. */
static PK11SlotInfo *
crypto_get_p12_slot(struct _pkinit_identity_crypto_context *id)
{
    char *configdir, *spec;
    size_t spec_size;
    int attempts;

    if (id->id_p12_slot.slot == NULL) {
        configdir = DEFAULT_CONFIGDIR;
#ifdef PKCS12_HACK
        /* Figure out where to put the temporary userdb. */
        attempts = 0;
        while ((attempts < TMP_MAX) &&
               (spec = tempnam(NULL, "pk12-")) != NULL) {
            if (spec != NULL) {
                if (mkdir(spec, S_IRWXU) == 0) {
                    configdir = spec;
                    break;
                } else {
                    free(spec);
                    if (errno != EEXIST)
                        break;
                }
                attempts++;
            }
        }
#endif
        spec_size = strlen("configDir='' flags=readOnly") +
            strlen(configdir) + 1;
        spec = PORT_ArenaZAlloc(id->pool, spec_size);
        if (spec != NULL) {
            if (strcmp(configdir, DEFAULT_CONFIGDIR) != 0)
                snprintf(spec, spec_size, "configDir='%s'", configdir);
            else
                snprintf(spec, spec_size, "configDir='%s' flags=readOnly",
                         configdir);
            id->id_p12_slot.slot = SECMOD_OpenUserDB(spec);
        }
#ifdef PKCS12_HACK
        if (strcmp(configdir, DEFAULT_CONFIGDIR) != 0) {
            DIR *dir;
            struct dirent *ent;
            char *path;
            /* First, initialize the slot. */
            if (id->id_p12_slot.slot != NULL)
                if (PK11_NeedUserInit(id->id_p12_slot.slot))
                    PK11_InitPin(id->id_p12_slot.slot, "", "");
            /* Scan the directory, deleting all of the contents. */
            dir = opendir(configdir);
            if (dir == NULL)
                pkiDebug("%s: error removing directory \"%s\": %s\n",
                         __FUNCTION__, configdir, strerror(errno));
            else {
                while ((ent = readdir(dir)) != NULL) {
                    if ((strcmp(ent->d_name, ".") == 0) ||
                        (strcmp(ent->d_name, "..") == 0)) {
                        continue;
                    }
                    if (k5_path_join(configdir, ent->d_name, &path) == 0) {
                        remove(path);
                        free(path);
                    }
                }
                closedir(dir);
            }
            /* Remove the directory itself. */
            rmdir(configdir);
            free(configdir);
        }
    }
#endif
    return id->id_p12_slot.slot;
}

/* Close the slot which we've been using for holding imported PKCS12
 * certificates and keys. */
static void
crypto_close_p12_slot(struct _pkinit_identity_crypto_context *id)
{
    PK11_FreeSlot(id->id_p12_slot.slot);
    id->id_p12_slot.slot = NULL;
}

void
pkinit_fini_identity_crypto(pkinit_identity_crypto_context id_cryptoctx)
{
    int i;

    pkiDebug("%s\n", __FUNCTION__);
    /* The order of cleanup here is intended to ensure that nothing gets
     * freed before anything that might have a reference to it. */
    if (id_cryptoctx->deferred_ids != NULL)
        pkinit_free_deferred_ids(id_cryptoctx->deferred_ids);
    if (id_cryptoctx->id_cert != NULL)
        CERT_DestroyCertificate(id_cryptoctx->id_cert);
    CERT_DestroyCertList(id_cryptoctx->ca_certs);
    CERT_DestroyCertList(id_cryptoctx->id_certs);
    if (id_cryptoctx->id_objects != NULL)
        for (i = 0; id_cryptoctx->id_objects[i] != NULL; i++) {
            if (id_cryptoctx->id_objects[i]->cert != NULL)
                CERT_DestroyCertificate(id_cryptoctx->id_objects[i]->cert);
            PK11_DestroyGenericObject(id_cryptoctx->id_objects[i]->obj);
        }
    if (id_cryptoctx->id_p12_slot.slot != NULL)
        crypto_close_p12_slot(id_cryptoctx);
    if (id_cryptoctx->id_userdbs != NULL)
        for (i = 0; id_cryptoctx->id_userdbs[i] != NULL; i++)
            PK11_FreeSlot(id_cryptoctx->id_userdbs[i]->userdb);
    if (id_cryptoctx->id_modules != NULL) {
        for (i = 0; id_cryptoctx->id_modules[i] != NULL; i++) {
            if (id_cryptoctx->id_modules[i]->module != NULL)
                SECMOD_DestroyModule(id_cryptoctx->id_modules[i]->module);
        }
    }
    if (id_cryptoctx->id_crls != NULL)
        for (i = 0; id_cryptoctx->id_crls[i] != NULL; i++)
            CERT_UncacheCRL(CERT_GetDefaultCertDB(), id_cryptoctx->id_crls[i]);
    if (id_cryptoctx->pem_module != NULL)
        SECMOD_DestroyModule(id_cryptoctx->pem_module);
    PORT_FreeArena(id_cryptoctx->pool, PR_TRUE);
}

static SECStatus
crypto_register_any(SECOidTag tag)
{
    if (NSS_CMSType_RegisterContentType(tag,
                                        NULL,
                                        0,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL, NULL, PR_TRUE) != SECSuccess)
        return ENOMEM;
    return 0;
}

krb5_error_code
pkinit_init_plg_crypto(pkinit_plg_crypto_context *plg_cryptoctx)
{
    PLArenaPool *pool;
    SECOidTag tag;

    pkiDebug("%s\n", __FUNCTION__);
    pool = PORT_NewArena(sizeof(double));
    if (pool != NULL) {
        *plg_cryptoctx = PORT_ArenaZAlloc(pool, sizeof(**plg_cryptoctx));
        if (*plg_cryptoctx != NULL) {
            (*plg_cryptoctx)->pool = pool;
            (*plg_cryptoctx)->ncontext = NSS_InitContext(DEFAULT_CONFIGDIR,
                                                         NULL,
                                                         NULL,
                                                         NULL,
                                                         NULL,
                                                         NSS_INIT_READONLY |
                                                         NSS_INIT_NOCERTDB |
                                                         NSS_INIT_NOMODDB |
                                                         NSS_INIT_FORCEOPEN |
                                                         NSS_INIT_NOROOTINIT |
                                                         NSS_INIT_PK11RELOAD);
            if ((*plg_cryptoctx)->ncontext != NULL) {
                tag = get_pkinit_data_auth_data9_tag();
                if (crypto_register_any(tag) != SECSuccess) {
                    PORT_FreeArena(pool, PR_TRUE);
                    return ENOMEM;
                }
                tag = get_pkinit_data_auth_data_tag();
                if (crypto_register_any(tag) != SECSuccess) {
                    PORT_FreeArena(pool, PR_TRUE);
                    return ENOMEM;
                }
                tag = get_pkinit_data_rkey_data_tag();
                if (crypto_register_any(tag) != SECSuccess) {
                    PORT_FreeArena(pool, PR_TRUE);
                    return ENOMEM;
                }
                tag = get_pkinit_data_dhkey_data_tag();
                if (crypto_register_any(tag) != SECSuccess) {
                    PORT_FreeArena(pool, PR_TRUE);
                    return ENOMEM;
                }
                return 0;
            }
        }
        PORT_FreeArena(pool, PR_TRUE);
    }
    return ENOMEM;
}

void
pkinit_fini_plg_crypto(pkinit_plg_crypto_context plg_cryptoctx)
{
    pkiDebug("%s\n", __FUNCTION__);
    if (plg_cryptoctx == NULL)
        return;
    if (NSS_ShutdownContext(plg_cryptoctx->ncontext) != SECSuccess)
        pkiDebug("%s: error shutting down context\n", __FUNCTION__);
    PORT_FreeArena(plg_cryptoctx->pool, PR_TRUE);
}

krb5_error_code
pkinit_init_req_crypto(pkinit_req_crypto_context *req_cryptoctx)
{
    PLArenaPool *pool;

    pkiDebug("%s\n", __FUNCTION__);
    pool = PORT_NewArena(sizeof(double));
    if (pool != NULL) {
        *req_cryptoctx = PORT_ArenaZAlloc(pool, sizeof(**req_cryptoctx));
        if (*req_cryptoctx != NULL) {
            (*req_cryptoctx)->pool = pool;
            return 0;
        }
        PORT_FreeArena(pool, PR_TRUE);
    }
    return ENOMEM;
}

void
pkinit_fini_req_crypto(pkinit_req_crypto_context req_cryptoctx)
{
    pkiDebug("%s\n", __FUNCTION__);
    if (req_cryptoctx->client_dh_privkey != NULL)
        SECKEY_DestroyPrivateKey(req_cryptoctx->client_dh_privkey);
    if (req_cryptoctx->client_dh_pubkey != NULL)
        SECKEY_DestroyPublicKey(req_cryptoctx->client_dh_pubkey);
    if (req_cryptoctx->peer_cert != NULL)
        CERT_DestroyCertificate(req_cryptoctx->peer_cert);
    PORT_FreeArena(req_cryptoctx->pool, PR_TRUE);
}

/* Duplicate the memory from the SECItem into a malloc()d buffer. */
static int
secitem_to_buf_len(SECItem *item, unsigned char **out, unsigned int *len)
{
    *out = malloc(item->len);
    if (*out == NULL)
        return ENOMEM;
    memcpy(*out, item->data, item->len);
    *len = item->len;
    return 0;
}

/* Encode the raw buffer as an unsigned integer.  If the first byte in the
 * buffer has its high bit set, we need to prepend a zero byte to make sure it
 * isn't treated as a negative value. */
static int
secitem_to_dh_pubval(SECItem *item, unsigned char **out, unsigned int *len)
{
    PLArenaPool *pool;
    SECItem *uval, uinteger;
    int i;

    pool = PORT_NewArena(sizeof(double));
    if (pool == NULL)
        return ENOMEM;

    if (item->data[0] & 0x80) {
        uval = SECITEM_AllocItem(pool, NULL, item->len + 1);
        if (uval == NULL) {
            PORT_FreeArena(pool, PR_TRUE);
            return ENOMEM;
        }
        uval->data[0] = '\0';
        memcpy(uval->data + 1, item->data, item->len);
    } else {
        uval = item;
    }

    memset(&uinteger, 0, sizeof(uinteger));
    if (SEC_ASN1EncodeItem(pool, &uinteger, uval,
                           SEC_ASN1_GET(SEC_IntegerTemplate)) != &uinteger) {
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }

    i = secitem_to_buf_len(&uinteger, out, len);

    PORT_FreeArena(pool, PR_TRUE);
    return i;
}

/* Decode a DER unsigned integer, and return just the bits that make up that
 * integer. */
static int
secitem_from_dh_pubval(PLArenaPool *pool,
                       unsigned char *dh_pubkey, unsigned int dh_pubkey_len,
                       SECItem *bits_out)
{
    SECItem tmp;

    tmp.data = dh_pubkey;
    tmp.len = dh_pubkey_len;
    memset(bits_out, 0, sizeof(*bits_out));
    if (SEC_ASN1DecodeItem(pool, bits_out,
                           SEC_ASN1_GET(SEC_IntegerTemplate),
                           &tmp) != SECSuccess)
        return ENOMEM;
    return 0;
}

/* Load the contents of a file into a SECitem.  If it looks like a PEM-wrapped
 * item, maybe try to undo the base64 encoding. */
enum secitem_from_file_type {
    secitem_from_file_plain,
    secitem_from_file_decode
};
static int
secitem_from_file(PLArenaPool *pool, const char *filename,
                  enum secitem_from_file_type secitem_from_file_type,
                  SECItem *item_out)
{
    SECItem tmp, *decoded;
    struct stat st;
    int fd, i, n;
    const char *encoded, *p;
    char *what, *q;

    memset(item_out, 0, sizeof(*item_out));
    fd = open(filename, O_RDONLY);
    if (fd == -1)
        return errno;
    if (fstat(fd, &st) == -1) {
        i = errno;
        close(fd);
        return i;
    }
    memset(&tmp, 0, sizeof(tmp));
    tmp.data = PORT_ArenaZAlloc(pool, st.st_size + 1);
    if (tmp.data == NULL) {
        close(fd);
        return ENOMEM;
    }
    n = 0;
    while (n < st.st_size) {
        i = read(fd, tmp.data + n, st.st_size - n);
        if (i <= 0)
            break;
        n += i;
    }
    close(fd);
    if (n < st.st_size)
        return ENOMEM;
    tmp.data[n] = '\0';
    tmp.len = n;
    encoded = (const char *) tmp.data;
    if ((secitem_from_file_type == secitem_from_file_decode) &&
        (tmp.len > 11) &&
        ((strncmp(encoded, "-----BEGIN ", 11) == 0) ||
         ((encoded = strstr((char *)tmp.data, "\n-----BEGIN")) != NULL))) {
        if (encoded[0] == '\n')
            encoded++;
        /* find the beginning of the next line */
        p = encoded;
        p += strcspn(p, "\r\n");
        p += strspn(p, "\r\n");
        q = NULL;
        what = PORT_ArenaZAlloc(pool, p - (encoded + 2) + 1);
        if (what != NULL) {
            /* construct the matching end-of-item and look for it */
            memcpy(what, "-----END ", 9);
            memcpy(what + 9, encoded + 11, p - (encoded + 11));
            what[p - (encoded + 2)] = '\0';
            q = strstr(p, what);
        }
        if (q != NULL) {
            *q = '\0';
            decoded = NSSBase64_DecodeBuffer(pool, NULL, p, q - p);
            if (decoded != NULL)
                tmp = *decoded;
        }
    }
    *item_out = tmp;
    return 0;
}

static struct oakley_group
{
    int identifier;
    int bits;                   /* shortest prime first, so that a
                                 * sequential search for a set with a
                                 * length that exceeds the minimum will
                                 * find the entry with the shortest
                                 * suitable prime */
    char name[32];
    char prime[4096];           /* large enough to hold that prime */
    long generator;             /* note: oakley_parse_group() assumes that this
                                 * number fits into a long */
    char subprime[4096];        /* large enough to hold its subprime
                                 * ((p-1)/2) */
} oakley_groups[] = {
    {
        1, 768,
        "Oakley MODP Group 1",
        "FFFFFFFF FFFFFFFF C90FDAA2 2168C234 C4C6628B 80DC1CD1"
        "29024E08 8A67CC74 020BBEA6 3B139B22 514A0879 8E3404DD"
        "EF9519B3 CD3A431B 302B0A6D F25F1437 4FE1356D 6D51C245"
        "E485B576 625E7EC6 F44C42E9 A63A3620 FFFFFFFF FFFFFFFF",
        2,
        "7FFFFFFF FFFFFFFF E487ED51 10B4611A 62633145 C06E0E68"
        "94812704 4533E63A 0105DF53 1D89CD91 28A5043C C71A026E"
        "F7CA8CD9 E69D218D 98158536 F92F8A1B A7F09AB6 B6A8E122"
        "F242DABB 312F3F63 7A262174 D31D1B10 7FFFFFFF FFFFFFFF",
    },
    {
        2, 1024,
        "Oakley MODP Group 2",
        "FFFFFFFF FFFFFFFF C90FDAA2 2168C234 C4C6628B 80DC1CD1"
        "29024E08 8A67CC74 020BBEA6 3B139B22 514A0879 8E3404DD"
        "EF9519B3 CD3A431B 302B0A6D F25F1437 4FE1356D 6D51C245"
        "E485B576 625E7EC6 F44C42E9 A637ED6B 0BFF5CB6 F406B7ED"
        "EE386BFB 5A899FA5 AE9F2411 7C4B1FE6 49286651 ECE65381"
        "FFFFFFFF FFFFFFFF",
        2,
        "7FFFFFFF FFFFFFFF E487ED51 10B4611A 62633145 C06E0E68"
        "94812704 4533E63A 0105DF53 1D89CD91 28A5043C C71A026E"
        "F7CA8CD9 E69D218D 98158536 F92F8A1B A7F09AB6 B6A8E122"
        "F242DABB 312F3F63 7A262174 D31BF6B5 85FFAE5B 7A035BF6"
        "F71C35FD AD44CFD2 D74F9208 BE258FF3 24943328 F67329C0"
        "FFFFFFFF FFFFFFFF",
    },
    {
        5, 1536,
        "Oakley MODP Group 5",
        "FFFFFFFF FFFFFFFF C90FDAA2 2168C234 C4C6628B 80DC1CD1"
        "29024E08 8A67CC74 020BBEA6 3B139B22 514A0879 8E3404DD"
        "EF9519B3 CD3A431B 302B0A6D F25F1437 4FE1356D 6D51C245"
        "E485B576 625E7EC6 F44C42E9 A637ED6B 0BFF5CB6 F406B7ED"
        "EE386BFB 5A899FA5 AE9F2411 7C4B1FE6 49286651 ECE45B3D"
        "C2007CB8 A163BF05 98DA4836 1C55D39A 69163FA8 FD24CF5F"
        "83655D23 DCA3AD96 1C62F356 208552BB 9ED52907 7096966D"
        "670C354E 4ABC9804 F1746C08 CA237327 FFFFFFFF FFFFFFFF",
        2,
        "7FFFFFFF FFFFFFFF E487ED51 10B4611A 62633145 C06E0E68"
        "94812704 4533E63A 0105DF53 1D89CD91 28A5043C C71A026E"
        "F7CA8CD9 E69D218D 98158536 F92F8A1B A7F09AB6 B6A8E122"
        "F242DABB 312F3F63 7A262174 D31BF6B5 85FFAE5B 7A035BF6"
        "F71C35FD AD44CFD2 D74F9208 BE258FF3 24943328 F6722D9E"
        "E1003E5C 50B1DF82 CC6D241B 0E2AE9CD 348B1FD4 7E9267AF"
        "C1B2AE91 EE51D6CB 0E3179AB 1042A95D CF6A9483 B84B4B36"
        "B3861AA7 255E4C02 78BA3604 6511B993 FFFFFFFF FFFFFFFF",
    },
    {
        14, 2048,
        "Oakley MODP Group 14",
        "FFFFFFFF FFFFFFFF C90FDAA2 2168C234 C4C6628B 80DC1CD1"
        "29024E08 8A67CC74 020BBEA6 3B139B22 514A0879 8E3404DD"
        "EF9519B3 CD3A431B 302B0A6D F25F1437 4FE1356D 6D51C245"
        "E485B576 625E7EC6 F44C42E9 A637ED6B 0BFF5CB6 F406B7ED"
        "EE386BFB 5A899FA5 AE9F2411 7C4B1FE6 49286651 ECE45B3D"
        "C2007CB8 A163BF05 98DA4836 1C55D39A 69163FA8 FD24CF5F"
        "83655D23 DCA3AD96 1C62F356 208552BB 9ED52907 7096966D"
        "670C354E 4ABC9804 F1746C08 CA18217C 32905E46 2E36CE3B"
        "E39E772C 180E8603 9B2783A2 EC07A28F B5C55DF0 6F4C52C9"
        "DE2BCBF6 95581718 3995497C EA956AE5 15D22618 98FA0510"
        "15728E5A 8AACAA68 FFFFFFFF FFFFFFFF",
        2,
        "7FFFFFFF FFFFFFFF E487ED51 10B4611A 62633145 C06E0E68"
        "94812704 4533E63A 0105DF53 1D89CD91 28A5043C C71A026E"
        "F7CA8CD9 E69D218D 98158536 F92F8A1B A7F09AB6 B6A8E122"
        "F242DABB 312F3F63 7A262174 D31BF6B5 85FFAE5B 7A035BF6"
        "F71C35FD AD44CFD2 D74F9208 BE258FF3 24943328 F6722D9E"
        "E1003E5C 50B1DF82 CC6D241B 0E2AE9CD 348B1FD4 7E9267AF"
        "C1B2AE91 EE51D6CB 0E3179AB 1042A95D CF6A9483 B84B4B36"
        "B3861AA7 255E4C02 78BA3604 650C10BE 19482F23 171B671D"
        "F1CF3B96 0C074301 CD93C1D1 7603D147 DAE2AEF8 37A62964"
        "EF15E5FB 4AAC0B8C 1CCAA4BE 754AB572 8AE9130C 4C7D0288"
        "0AB9472D 45565534 7FFFFFFF FFFFFFFF",
    },
    {
        15, 3072,
        "Oakley MODP Group 15",
        "FFFFFFFF FFFFFFFF C90FDAA2 2168C234 C4C6628B 80DC1CD1"
        "29024E08 8A67CC74 020BBEA6 3B139B22 514A0879 8E3404DD"
        "EF9519B3 CD3A431B 302B0A6D F25F1437 4FE1356D 6D51C245"
        "E485B576 625E7EC6 F44C42E9 A637ED6B 0BFF5CB6 F406B7ED"
        "EE386BFB 5A899FA5 AE9F2411 7C4B1FE6 49286651 ECE45B3D"
        "C2007CB8 A163BF05 98DA4836 1C55D39A 69163FA8 FD24CF5F"
        "83655D23 DCA3AD96 1C62F356 208552BB 9ED52907 7096966D"
        "670C354E 4ABC9804 F1746C08 CA18217C 32905E46 2E36CE3B"
        "E39E772C 180E8603 9B2783A2 EC07A28F B5C55DF0 6F4C52C9"
        "DE2BCBF6 95581718 3995497C EA956AE5 15D22618 98FA0510"
        "15728E5A 8AAAC42D AD33170D 04507A33 A85521AB DF1CBA64"
        "ECFB8504 58DBEF0A 8AEA7157 5D060C7D B3970F85 A6E1E4C7"
        "ABF5AE8C DB0933D7 1E8C94E0 4A25619D CEE3D226 1AD2EE6B"
        "F12FFA06 D98A0864 D8760273 3EC86A64 521F2B18 177B200C"
        "BBE11757 7A615D6C 770988C0 BAD946E2 08E24FA0 74E5AB31"
        "43DB5BFC E0FD108E 4B82D120 A93AD2CA FFFFFFFF FFFFFFFF",
        2,
        "7FFFFFFF FFFFFFFF E487ED51 10B4611A 62633145 C06E0E68"
        "94812704 4533E63A 0105DF53 1D89CD91 28A5043C C71A026E"
        "F7CA8CD9 E69D218D 98158536 F92F8A1B A7F09AB6 B6A8E122"
        "F242DABB 312F3F63 7A262174 D31BF6B5 85FFAE5B 7A035BF6"
        "F71C35FD AD44CFD2 D74F9208 BE258FF3 24943328 F6722D9E"
        "E1003E5C 50B1DF82 CC6D241B 0E2AE9CD 348B1FD4 7E9267AF"
        "C1B2AE91 EE51D6CB 0E3179AB 1042A95D CF6A9483 B84B4B36"
        "B3861AA7 255E4C02 78BA3604 650C10BE 19482F23 171B671D"
        "F1CF3B96 0C074301 CD93C1D1 7603D147 DAE2AEF8 37A62964"
        "EF15E5FB 4AAC0B8C 1CCAA4BE 754AB572 8AE9130C 4C7D0288"
        "0AB9472D 45556216 D6998B86 82283D19 D42A90D5 EF8E5D32"
        "767DC282 2C6DF785 457538AB AE83063E D9CB87C2 D370F263"
        "D5FAD746 6D8499EB 8F464A70 2512B0CE E771E913 0D697735"
        "F897FD03 6CC50432 6C3B0139 9F643532 290F958C 0BBD9006"
        "5DF08BAB BD30AEB6 3B84C460 5D6CA371 047127D0 3A72D598"
        "A1EDADFE 707E8847 25C16890 549D6965 7FFFFFFF FFFFFFFF",
    },
    {
        16, 4096,
        "Oakley MODP Group 16",
        "FFFFFFFF FFFFFFFF C90FDAA2 2168C234 C4C6628B 80DC1CD1"
        "29024E08 8A67CC74 020BBEA6 3B139B22 514A0879 8E3404DD"
        "EF9519B3 CD3A431B 302B0A6D F25F1437 4FE1356D 6D51C245"
        "E485B576 625E7EC6 F44C42E9 A637ED6B 0BFF5CB6 F406B7ED"
        "EE386BFB 5A899FA5 AE9F2411 7C4B1FE6 49286651 ECE45B3D"
        "C2007CB8 A163BF05 98DA4836 1C55D39A 69163FA8 FD24CF5F"
        "83655D23 DCA3AD96 1C62F356 208552BB 9ED52907 7096966D"
        "670C354E 4ABC9804 F1746C08 CA18217C 32905E46 2E36CE3B"
        "E39E772C 180E8603 9B2783A2 EC07A28F B5C55DF0 6F4C52C9"
        "DE2BCBF6 95581718 3995497C EA956AE5 15D22618 98FA0510"
        "15728E5A 8AAAC42D AD33170D 04507A33 A85521AB DF1CBA64"
        "ECFB8504 58DBEF0A 8AEA7157 5D060C7D B3970F85 A6E1E4C7"
        "ABF5AE8C DB0933D7 1E8C94E0 4A25619D CEE3D226 1AD2EE6B"
        "F12FFA06 D98A0864 D8760273 3EC86A64 521F2B18 177B200C"
        "BBE11757 7A615D6C 770988C0 BAD946E2 08E24FA0 74E5AB31"
        "43DB5BFC E0FD108E 4B82D120 A9210801 1A723C12 A787E6D7"
        "88719A10 BDBA5B26 99C32718 6AF4E23C 1A946834 B6150BDA"
        "2583E9CA 2AD44CE8 DBBBC2DB 04DE8EF9 2E8EFC14 1FBECAA6"
        "287C5947 4E6BC05D 99B2964F A090C3A2 233BA186 515BE7ED"
        "1F612970 CEE2D7AF B81BDD76 2170481C D0069127 D5B05AA9"
        "93B4EA98 8D8FDDC1 86FFB7DC 90A6C08F 4DF435C9 34063199"
        "FFFFFFFF FFFFFFFF",
        2,
        "7FFFFFFF FFFFFFFF E487ED51 10B4611A 62633145 C06E0E68"
        "94812704 4533E63A 0105DF53 1D89CD91 28A5043C C71A026E"
        "F7CA8CD9 E69D218D 98158536 F92F8A1B A7F09AB6 B6A8E122"
        "F242DABB 312F3F63 7A262174 D31BF6B5 85FFAE5B 7A035BF6"
        "F71C35FD AD44CFD2 D74F9208 BE258FF3 24943328 F6722D9E"
        "E1003E5C 50B1DF82 CC6D241B 0E2AE9CD 348B1FD4 7E9267AF"
        "C1B2AE91 EE51D6CB 0E3179AB 1042A95D CF6A9483 B84B4B36"
        "B3861AA7 255E4C02 78BA3604 650C10BE 19482F23 171B671D"
        "F1CF3B96 0C074301 CD93C1D1 7603D147 DAE2AEF8 37A62964"
        "EF15E5FB 4AAC0B8C 1CCAA4BE 754AB572 8AE9130C 4C7D0288"
        "0AB9472D 45556216 D6998B86 82283D19 D42A90D5 EF8E5D32"
        "767DC282 2C6DF785 457538AB AE83063E D9CB87C2 D370F263"
        "D5FAD746 6D8499EB 8F464A70 2512B0CE E771E913 0D697735"
        "F897FD03 6CC50432 6C3B0139 9F643532 290F958C 0BBD9006"
        "5DF08BAB BD30AEB6 3B84C460 5D6CA371 047127D0 3A72D598"
        "A1EDADFE 707E8847 25C16890 54908400 8D391E09 53C3F36B"
        "C438CD08 5EDD2D93 4CE1938C 357A711E 0D4A341A 5B0A85ED"
        "12C1F4E5 156A2674 6DDDE16D 826F477C 97477E0A 0FDF6553"
        "143E2CA3 A735E02E CCD94B27 D04861D1 119DD0C3 28ADF3F6"
        "8FB094B8 67716BD7 DC0DEEBB 10B8240E 68034893 EAD82D54"
        "C9DA754C 46C7EEE0 C37FDBEE 48536047 A6FA1AE4 9A0318CC"
        "FFFFFFFF FFFFFFFF",
    }
};

/* Convert a string of hexadecimal characters to a binary integer. */
static SECItem *
hex_to_secitem(const char *hex, SECItem *item)
{
    int count, i;
    unsigned int j;
    unsigned char c, acc;

    j = 0;
    c = hex[0];
    /* If the high bit would be set, prepend a zero byte to keep the result
     * from being negative. */
    if ((c == '8') ||
        (c == '9') ||
        ((c >= 'a') && (c <= 'f')) || ((c >= 'A') && (c <= 'F'))) {
        item->data[j] = 0;
        j++;
    }
    count = 0;
    acc = 0;
    for (i = 0; hex[i] != '\0'; i++) {
        if ((count % 2) == 0)
            acc = 0;
        c = hex[i];
        if ((c >= '0') && (c <= '9'))
            acc = (acc << 4) | (c - '0');
        else if ((c >= 'a') && (c <= 'f'))
            acc = (acc << 4) | (c - 'a' + 10);
        else if ((c >= 'A') && (c <= 'F'))
            acc = (acc << 4) | (c - 'A' + 10);
        else
            continue;
        count++;
        if ((count % 2) == 0) {
            item->data[j] = acc & 0xff;
            acc = 0;
            j++;
        }
        if (j >= item->len) {
            /* overrun */
            return NULL;
            break;
        }
    }
    if (hex[i] != '\0')     /* unused bytes? */
        return NULL;
    item->len = j;
    return item;
}

static int
oakley_parse_group(PLArenaPool *pool, struct oakley_group *group,
                   struct domain_parameters **domain_params_out)
{
    unsigned int bytes;
    struct domain_parameters *params;
    SECItem *t;

    params = PORT_ArenaZAlloc(pool, sizeof(*params));
    if (params == NULL)
        return ENOMEM;

    /* Allocate more memory than we'll probably need. */
    bytes = group->bits;

    /* Encode the prime (p). */
    t = SECITEM_AllocItem(pool, NULL, bytes);
    if (t == NULL)
        return ENOMEM;
    if (hex_to_secitem(group->prime, t) != t)
        return ENOMEM;
    params->p = *t;
    /* Encode the generator. */
    if (SEC_ASN1EncodeInteger(pool, &params->g,
                              group->generator) != &params->g)
        return ENOMEM;
    /* Encode the subprime. */
    t = SECITEM_AllocItem(pool, NULL, bytes);
    if (t == NULL)
        return ENOMEM;
    if (hex_to_secitem(group->subprime, t) != t)
        return ENOMEM;
    params->q = *t;
    *domain_params_out = params;
    return 0;
}

static struct domain_parameters *
oakley_get_group(PLArenaPool *pool, int minimum_prime_size)
{
    unsigned int i;
    struct domain_parameters *params;

    params = PORT_ArenaZAlloc(pool, sizeof(*params));
    if (params == NULL)
        return NULL;
    for (i = 0; i < sizeof(oakley_groups) / sizeof(oakley_groups[0]); i++)
        if (oakley_groups[i].bits >= minimum_prime_size)
            if (oakley_parse_group(pool, &oakley_groups[i], &params) == 0)
                return params;
    return NULL;
}

/* Create DH parameters to be sent to the KDC.  On success, dh_params should
 * contain an encoded DomainParameters structure (per RFC3280, the "parameters"
 * in an AlgorithmIdentifier), and dh_pubkey should contain the public value
 * we're prepared to send to the KDC, encoded as an integer (per RFC3280, the
 * "subjectPublicKey" field of a SubjectPublicKeyInfo -- the integer is wrapped
 * up into a bitstring elsewhere). */
krb5_error_code
client_create_dh(krb5_context context,
                 pkinit_plg_crypto_context plg_cryptoctx,
                 pkinit_req_crypto_context req_cryptoctx,
                 pkinit_identity_crypto_context id_cryptoctx,
                 int dh_size_bits,
                 unsigned char **dh_params,
                 unsigned int *dh_params_len,
                 unsigned char **dh_pubkey, unsigned int *dh_pubkey_len)
{
    PLArenaPool *pool;
    PK11SlotInfo *slot;
    SECKEYPrivateKey *priv;
    SECKEYPublicKey *pub;
    SECKEYDHParams dh_param;
    struct domain_parameters *params;
    SECItem encoded;

    pool = PORT_NewArena(sizeof(double));
    if (pool == NULL)
        return ENOMEM;
    memset(&params, 0, sizeof(params));

    /* Find suitable domain parameters. */
    params = oakley_get_group(pool, dh_size_bits);
    if (params == NULL) {
        pkiDebug("%s: error finding suitable parameters\n", __FUNCTION__);
        return ENOENT;
    }

    /* Set up to generate the public key. */
    memset(&dh_param, 0, sizeof(dh_param));
    dh_param.arena = pool;
    dh_param.prime = params->p;
    dh_param.base = params->g;

    /* Generate a public value and a private key. */
    slot = PK11_GetBestSlot(CKM_DH_PKCS_KEY_PAIR_GEN,
                            crypto_pwcb_prep(id_cryptoctx, NULL, context));
    if (slot == NULL) {
        PORT_FreeArena(pool, PR_TRUE);
        pkiDebug("%s: error selecting slot\n", __FUNCTION__);
        return ENOMEM;
    }
    pub = NULL;
    priv = PK11_GenerateKeyPair(slot, CKM_DH_PKCS_KEY_PAIR_GEN,
                                &dh_param, &pub, PR_FALSE, PR_FALSE,
                                crypto_pwcb_prep(id_cryptoctx, NULL, context));

    /* Finish building the return values. */
    memset(&encoded, 0, sizeof(encoded));
    if (SEC_ASN1EncodeItem(pool, &encoded, params,
                           domain_parameters_template) != &encoded) {
        PK11_FreeSlot(slot);
        PORT_FreeArena(pool, PR_TRUE);
        pkiDebug("%s: error encoding parameters\n", __FUNCTION__);
        return ENOMEM;
    }

    /* Export the return values. */
    if (secitem_to_buf_len(&encoded, dh_params, dh_params_len) != 0) {
        PK11_FreeSlot(slot);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
    if (secitem_to_dh_pubval(&pub->u.dh.publicValue, dh_pubkey,
                             dh_pubkey_len) != 0) {
        free(*dh_params);
        *dh_params = NULL;
        PK11_FreeSlot(slot);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }

    /* Save our private and public keys for reuse later. */
    if (req_cryptoctx->client_dh_privkey != NULL)
        SECKEY_DestroyPrivateKey(req_cryptoctx->client_dh_privkey);
    req_cryptoctx->client_dh_privkey = priv;
    if (req_cryptoctx->client_dh_pubkey != NULL)
        SECKEY_DestroyPublicKey(req_cryptoctx->client_dh_pubkey);
    req_cryptoctx->client_dh_pubkey = pub;

    PK11_FreeSlot(slot);
    PORT_FreeArena(pool, PR_TRUE);
    return 0;
}

/* Combine the KDC's public key value with our copy of the parameters and our
 * secret key to generate the session key. */
krb5_error_code
client_process_dh(krb5_context context,
                  pkinit_plg_crypto_context plg_cryptoctx,
                  pkinit_req_crypto_context req_cryptoctx,
                  pkinit_identity_crypto_context id_cryptoctx,
                  unsigned char *dh_pubkey,
                  unsigned int dh_pubkey_len,
                  unsigned char **dh_session_key,
                  unsigned int *dh_session_key_len)
{
    PLArenaPool *pool;
    PK11SlotInfo *slot;
    SECKEYPublicKey *pub, pub2;
    PK11SymKey *sym;
    SECItem *bits;

    pool = PORT_NewArena(sizeof(double));
    if (pool == NULL)
        return ENOMEM;

    /* Rebuild the KDC's public key using our parameters and the supplied
     * public value (subjectPublicKey). */
    pub = SECKEY_CopyPublicKey(req_cryptoctx->client_dh_pubkey);
    if (pub == NULL) {
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
    pub2 = *pub;
    if (secitem_from_dh_pubval(pool, dh_pubkey, dh_pubkey_len,
                               &pub2.u.dh.publicValue) != 0) {
        SECKEY_DestroyPublicKey(pub);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }

    /* Generate the shared value using our private key and the KDC's
     * public key. */
    slot = PK11_GetBestSlot(CKM_DH_PKCS_KEY_PAIR_GEN,
                            crypto_pwcb_prep(id_cryptoctx, NULL, context));
    if (slot == NULL) {
        SECKEY_DestroyPublicKey(pub);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
    sym = PK11_PubDerive(req_cryptoctx->client_dh_privkey, &pub2, PR_FALSE,
                         NULL, NULL,
                         CKM_DH_PKCS_DERIVE,
                         CKM_TLS_MASTER_KEY_DERIVE_DH,
                         CKA_DERIVE,
                         0, crypto_pwcb_prep(id_cryptoctx, NULL, context));
    if (sym == NULL) {
        PK11_FreeSlot(slot);
        SECKEY_DestroyPublicKey(pub);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }

    /* Export the shared value. */
    if ((PK11_ExtractKeyValue(sym) != SECSuccess) ||
        ((bits = PK11_GetKeyData(sym)) == NULL) ||
        (secitem_to_buf_len(bits, dh_session_key, dh_session_key_len) != 0)) {
        PK11_FreeSymKey(sym);
        PK11_FreeSlot(slot);
        SECKEY_DestroyPublicKey(pub);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
    PK11_FreeSymKey(sym);
    PK11_FreeSlot(slot);
    SECKEY_DestroyPublicKey(pub);
    PORT_FreeArena(pool, PR_TRUE);
    return 0;
}

/* Given a binary-encoded integer, count the number of bits. */
static int
get_integer_bits(SECItem *integer)
{
    unsigned int i;
    unsigned char c;
    int size = 0;

    for (i = 0; i < integer->len; i++) {
        c = integer->data[i];
        if (c != 0) {
            size = (integer->len - i - 1) * 8;
            while (c != 0) {
                c >>= 1;
                size++;
            }
            break;
        }
    }
    return size;
}

/* Verify that the client-supplied parameters include a prime of sufficient
 * size. */
krb5_error_code
server_check_dh(krb5_context context,
                pkinit_plg_crypto_context plg_cryptoctx,
                pkinit_req_crypto_context req_cryptoctx,
                pkinit_identity_crypto_context id_cryptoctx,
                krb5_data *dh_params, int minbits)
{
    PLArenaPool *pool;
    SECItem item;

    pool = PORT_NewArena(sizeof(double));
    if (pool == NULL)
        return ENOMEM;

    item.data = (unsigned char *)dh_params->data;
    item.len = dh_params->length;
    memset(&req_cryptoctx->client_dh_params, 0,
           sizeof(req_cryptoctx->client_dh_params));
    if (SEC_ASN1DecodeItem(req_cryptoctx->pool,
                           &req_cryptoctx->client_dh_params,
                           domain_parameters_template, &item) != SECSuccess) {
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }

    if (get_integer_bits(&req_cryptoctx->client_dh_params.p) < minbits) {
        PORT_FreeArena(pool, PR_TRUE);
        return KRB5KDC_ERR_DH_KEY_PARAMETERS_NOT_ACCEPTED;
    }

    PORT_FreeArena(pool, PR_TRUE);
    return 0;
}

/* Take apart the client-supplied SubjectPublicKeyInfo, which contains both an
 * encoded DomainParameters structure (per RFC3279), and a public value, and
 * generate our own private key and public value using the supplied parameters.
 * Use our private key and the client's public value to derive the session key,
 * and hand our public value and the session key back to our caller. */
krb5_error_code
server_process_dh(krb5_context context,
                  pkinit_plg_crypto_context plg_cryptoctx,
                  pkinit_req_crypto_context req_cryptoctx,
                  pkinit_identity_crypto_context id_cryptoctx,
                  unsigned char *received_pubkey,
                  unsigned int received_pub_len,
                  unsigned char **dh_pubkey,
                  unsigned int *dh_pubkey_len,
                  unsigned char **server_key,
                  unsigned int *server_key_len)
{
    PLArenaPool *pool;
    SECKEYPrivateKey *priv;
    SECKEYPublicKey *pub, pub2;
    SECKEYDHParams dh_params;
    PK11SymKey *sym;
    SECItem pubval, *bits;
    PK11SlotInfo *slot;

    pool = PORT_NewArena(sizeof(double));
    if (pool == NULL)
        return ENOMEM;

    /* Store the client's public value. */
    pubval.data = received_pubkey;
    pubval.len = received_pub_len;

    /* Set up DH parameters the using client's domain parameters. */
    memset(&dh_params, 0, sizeof(dh_params));
    dh_params.arena = pool;
    dh_params.prime = req_cryptoctx->client_dh_params.p;
    dh_params.base = req_cryptoctx->client_dh_params.g;

    /* Generate a public value and a private key using the parameters. */
    slot = PK11_GetBestSlot(CKM_DH_PKCS_KEY_PAIR_GEN,
                            crypto_pwcb_prep(id_cryptoctx, NULL, context));
    if (slot == NULL) {
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
    pub = NULL;
    priv = PK11_GenerateKeyPair(slot, CKM_DH_PKCS_KEY_PAIR_GEN,
                                &dh_params, &pub, PR_FALSE, PR_FALSE,
                                crypto_pwcb_prep(id_cryptoctx, NULL, context));
    if (priv == NULL) {
        PK11_FreeSlot(slot);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }

    /* Build the client's public key using the client's parameters and
     * public value. */
    pub2 = *pub;
    if (SEC_ASN1DecodeItem(pool, &pub2.u.dh.publicValue,
                           SEC_ASN1_GET(SEC_IntegerTemplate),
                           &pubval) != SECSuccess) {
        SECKEY_DestroyPrivateKey(priv);
        SECKEY_DestroyPublicKey(pub);
        PK11_FreeSlot(slot);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }

    /* Generate the shared value using our private key and the client's
     * public key. */
    sym = PK11_PubDerive(priv, &pub2, PR_FALSE,
                         NULL, NULL,
                         CKM_DH_PKCS_DERIVE,
                         CKM_TLS_MASTER_KEY_DERIVE_DH,
                         CKA_DERIVE,
                         0, crypto_pwcb_prep(id_cryptoctx, NULL, context));
    if (sym == NULL) {
        SECKEY_DestroyPrivateKey(priv);
        SECKEY_DestroyPublicKey(pub);
        PK11_FreeSlot(slot);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }

    /* Export the shared value for our use and our public value for
     * transmission back to the client. */
    *server_key = NULL;
    *dh_pubkey = NULL;
    if ((PK11_ExtractKeyValue(sym) != SECSuccess) ||
        ((bits = PK11_GetKeyData(sym)) == NULL) ||
        (secitem_to_buf_len(bits, server_key, server_key_len) != 0) ||
        (secitem_to_dh_pubval(&pub->u.dh.publicValue,
                              dh_pubkey, dh_pubkey_len) != 0)) {
        free(*server_key);
        free(*dh_pubkey);
        PK11_FreeSymKey(sym);
        SECKEY_DestroyPrivateKey(priv);
        SECKEY_DestroyPublicKey(pub);
        PK11_FreeSlot(slot);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
    PK11_FreeSymKey(sym);
    SECKEY_DestroyPrivateKey(priv);
    SECKEY_DestroyPublicKey(pub);
    PK11_FreeSlot(slot);
    PORT_FreeArena(pool, PR_TRUE);
    return 0;
}

/* Create the issuer-and-serial portion of an external principal identifier for
 * a KDC's cert that we already have. */
krb5_error_code
create_issuerAndSerial(krb5_context context,
                       pkinit_plg_crypto_context plg_cryptoctx,
                       pkinit_req_crypto_context req_cryptoctx,
                       pkinit_identity_crypto_context id_cryptoctx,
                       unsigned char **kdcId_buf, unsigned int *kdcId_len)
{
    PLArenaPool *pool;
    struct issuer_and_serial_number isn;
    SECItem item;

    /* Check if we have a peer cert.  If we don't have one, that's okay. */
    if (req_cryptoctx->peer_cert == NULL)
        return 0;

    /* Scratch arena. */
    pool = PORT_NewArena(sizeof(double));
    if (pool == NULL)
        return ENOMEM;

    /* Encode the peer's issuer/serial. */
    isn.issuer = req_cryptoctx->peer_cert->derIssuer;
    isn.serial = req_cryptoctx->peer_cert->serialNumber;
    memset(&item, 0, sizeof(item));
    if (SEC_ASN1EncodeItem(id_cryptoctx->id_cert->arena, &item, &isn,
                           issuer_and_serial_number_template) != &item) {
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }

    /* Export the value. */
    if (secitem_to_buf_len(&item, kdcId_buf, kdcId_len) != 0) {
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }

    PORT_FreeArena(pool, PR_TRUE);
    return 0;
}

/* Populate a list of AlgorithmIdentifier structures with the OIDs of the key
 * wrap algorithms that we support. */
static void
free_n_algorithm_identifiers(krb5_algorithm_identifier **ids, int i)
{
    while (i >= 0) {
        free(ids[i]->algorithm.data);
        free(ids[i]);
        i--;
    }
    free(ids);
}

krb5_error_code
create_krb5_supportedCMSTypes(krb5_context context,
                              pkinit_plg_crypto_context plg_cryptoctx,
                              pkinit_req_crypto_context req_cryptoctx,
                              pkinit_identity_crypto_context id_cryptoctx,
                              krb5_algorithm_identifier ***supportedCMSTypes)
{
    SECOidData *oid;
    SECOidTag oids[] = {
        SEC_OID_CMS_3DES_KEY_WRAP,      /* no parameters */
        SEC_OID_AES_128_KEY_WRAP,       /* no parameters */
        SEC_OID_AES_192_KEY_WRAP,       /* no parameters */
        SEC_OID_AES_256_KEY_WRAP,       /* no parameters */
        /* RC2 key wrap requires parameters, so skip it */
    };
    krb5_algorithm_identifier **ids, *id;
    unsigned int i;

    ids = malloc(sizeof(id) * ((sizeof(oids) / sizeof(oids[0])) + 1));
    if (ids == NULL)
        return ENOMEM;

    for (i = 0; i < (sizeof(oids) / sizeof(oids[0])); i++) {
        id = malloc(sizeof(*id));
        if (id == NULL) {
            free_n_algorithm_identifiers(ids, i - 1);
            return ENOMEM;
        }
        memset(id, 0, sizeof(*id));
        ids[i] = id;
        oid = SECOID_FindOIDByTag(oids[i]);
        if (secitem_to_buf_len(&oid->oid,
                               (unsigned char **)&id->algorithm.data,
                               &id->algorithm.length) != 0) {
            free(ids[i]);
            free_n_algorithm_identifiers(ids, i - 1);
            return ENOMEM;
        }
    }
    ids[i] = NULL;
    *supportedCMSTypes = ids;
    return 0;
}

/* Populate a list of trusted certifiers with the list of the root certificates
 * that we trust. */
static void
free_n_principal_identifiers(krb5_external_principal_identifier **ids, int i)
{
    while (i >= 0) {
        free(ids[i]->subjectKeyIdentifier.data);
        free(ids[i]->issuerAndSerialNumber.data);
        free(ids[i]->subjectName.data);
        free(ids[i]);
        i--;
    }
    free(ids);
}

krb5_error_code
create_krb5_trustedCertifiers(krb5_context context,
                              pkinit_plg_crypto_context plg_cryptoctx,
                              pkinit_req_crypto_context req_cryptoctx,
                              pkinit_identity_crypto_context id_cryptoctx,
                              krb5_external_principal_identifier ***
                              trustedCertifiers)
{
    CERTCertListNode *node;
    krb5_external_principal_identifier **ids, *id;
    unsigned int i, n;

    *trustedCertifiers = NULL;

    /* Count the root certs. */
    n = 0;
    if (!CERT_LIST_EMPTY(id_cryptoctx->ca_certs)) {
        for (n = 0, node = CERT_LIST_HEAD(id_cryptoctx->ca_certs);
             (node != NULL) &&
                 (node->cert != NULL) &&
                 !CERT_LIST_END(node, id_cryptoctx->ca_certs);
             node = CERT_LIST_NEXT(node)) {
            n++;
        }
    }

    /* Build the result list. */
    if (n > 0) {
        ids = malloc((n + 1) * sizeof(id));
        if (ids == NULL)
            return ENOMEM;
        node = CERT_LIST_HEAD(id_cryptoctx->ca_certs);
        for (i = 0; i < n; i++) {
            id = malloc(sizeof(*id));
            if (id == NULL) {
                free_n_principal_identifiers(ids, i - 1);
                return ENOMEM;
            }
            memset(id, 0, sizeof(*id));
            /* Use the certificate's subject key ID iff it's
             * actually in the certificate.  Allocate the memory
             * from the heap because it'll be freed by other parts
             * of the pkinit module. */
            if ((node->cert->keyIDGenerated ?
                 secitem_to_buf_len(&node->cert->derSubject,
                                    (unsigned char **)
                                    &id->subjectName.data,
                                    &id->subjectName.length) :
                 secitem_to_buf_len(&node->cert->subjectKeyID,
                                    (unsigned char **)
                                    &id->subjectKeyIdentifier.data,
                                    &id->subjectKeyIdentifier.length)) != 0) {
                /* Free the earlier items. */
                free(ids[i]);
                free_n_principal_identifiers(ids, i - 1);
                return ENOMEM;
            }
            ids[i] = id;
            node = CERT_LIST_NEXT(node);
        }
        ids[i] = NULL;
        *trustedCertifiers = ids;
    }
    return 0;
}

/* Add a certificate to a list if it isn't already in the list.  Since the list
 * would take ownership of the cert if we added it to the list, if it's already
 * in the list, delete this reference to it. */
static SECStatus
cert_maybe_add_to_list(CERTCertList *list, CERTCertificate *cert)
{
    CERTCertListNode *node;

    for (node = CERT_LIST_HEAD(list);
         (node != NULL) &&
             (node->cert != NULL) &&
             !CERT_LIST_END(node, list);
         node = CERT_LIST_NEXT(node)) {
        if (SECITEM_ItemsAreEqual(&node->cert->derCert, &cert->derCert)) {
            /* Don't add the duplicate. */
            CERT_DestroyCertificate(cert);
            return SECSuccess;
        }
    }
    return CERT_AddCertToListTail(list, cert);
}

/* Load CA certificates from the slot. */
static SECStatus
cert_load_ca_certs_from_slot(krb5_context context,
                             pkinit_identity_crypto_context id,
                             PK11SlotInfo *slot,
                             const char *identity)
{
    CERTCertificate *cert;
    CERTCertList *list;
    CERTCertListNode *node;
    CERTCertTrust trust;
    SECStatus status;

    /* Log in if the slot requires it. */
    PK11_TokenRefresh(slot);
    if (!PK11_IsLoggedIn(slot, crypto_pwcb_prep(id, identity, context)) &&
        PK11_NeedLogin(slot)) {
        pkiDebug("%s: logging in to token \"%s\"\n",
                 __FUNCTION__, PK11_GetTokenName(slot));
        if (PK11_Authenticate(slot, PR_TRUE,
                              crypto_pwcb_prep(id, identity,
                                               context)) != SECSuccess) {
            pkiDebug("%s: error logging into \"%s\": %s, skipping\n",
                     __FUNCTION__, PK11_GetTokenName(slot),
                     PORT_ErrorToName(PORT_GetError()));
            return SECFailure;
        }
    }
    /* Get the list of certs from the slot. */
    list = PK11_ListCertsInSlot(slot);
    if (list == NULL) {
        pkiDebug("%s: nothing found in token \"%s\"\n",
                 __FUNCTION__, PK11_GetTokenName(slot));
        return SECSuccess;
    }
    if (CERT_LIST_EMPTY(list)) {
        CERT_DestroyCertList(list);
        pkiDebug("%s: nothing found in token \"%s\"\n",
                 __FUNCTION__, PK11_GetTokenName(slot));
        return SECSuccess;
    }
    /* Walk the list of certs, and for each one that's a CA, add
     * it to our CA cert list. */
    status = SECSuccess;
    for (node = CERT_LIST_HEAD(list);
         (node != NULL) &&
             (node->cert != NULL) &&
             !CERT_LIST_END(node, list);
         node = CERT_LIST_NEXT(node)) {
#if 0
        /* Skip it if it's not a root. */
        if (!node->cert->isRoot) {
            continue;
        }
#endif
        /* Skip it if we don't trust it to issue certificates. */
        if (CERT_GetCertTrust(node->cert, &trust) != SECSuccess)
            continue;
        if ((SEC_GET_TRUST_FLAGS(&trust, trustSSL) &
             (CERTDB_TRUSTED_CA |
              CERTDB_TRUSTED_CLIENT_CA | CERTDB_NS_TRUSTED_CA)) == 0)
            continue;
        /* DestroyCertList frees all of the certs in the list,
         * so we need to create a copy that we can own. */
        cert = CERT_DupCertificate(node->cert);
        /* Add it to the list. */
        if (cert_maybe_add_to_list(id->ca_certs, cert) != SECSuccess)
            status = SECFailure;
    }
    CERT_DestroyCertList(list);
    return status;
}

/* Load certificates for which we have private keys from the slot. */
static int
cert_load_certs_with_keys_from_slot(krb5_context context,
                                    pkinit_identity_crypto_context
                                    id_cryptoctx,
                                    PK11SlotInfo *slot,
                                    const char *cert_label,
                                    const char *cert_id,
                                    const char *identity)
{
    CERTCertificate *cert;
    CERTCertList *clist;
    CERTCertListNode *cnode;
    SECKEYPrivateKey *key;
    int status;

    /* Log in if the slot requires it. */
    PK11_TokenRefresh(slot);
    if (!PK11_IsLoggedIn(slot, crypto_pwcb_prep(id_cryptoctx, identity,
                                                context)) &&
        PK11_NeedLogin(slot)) {
        pkiDebug("%s: logging in to token \"%s\"\n",
                 __FUNCTION__, PK11_GetTokenName(slot));
        if (PK11_Authenticate(slot, PR_TRUE,
                              crypto_pwcb_prep(id_cryptoctx, identity,
                                               context)) != SECSuccess) {
            pkiDebug("%s: error logging into \"%s\": %s, skipping\n",
                     __FUNCTION__, PK11_GetTokenName(slot),
                     PORT_ErrorToName(PORT_GetError()));
            return id_cryptoctx->defer_id_prompt ? 0 : ENOMEM;
        }
    }
    /* Get the list of certs from the slot. */
    clist = PK11_ListCertsInSlot(slot);
    if (clist == NULL) {
        pkiDebug("%s: nothing found in token \"%s\"\n",
                 __FUNCTION__, PK11_GetTokenName(slot));
        return 0;
    }
    if (CERT_LIST_EMPTY(clist)) {
        CERT_DestroyCertList(clist);
        pkiDebug("%s: nothing found in token \"%s\"\n",
                 __FUNCTION__, PK11_GetTokenName(slot));
        return 0;
    }
    /* Walk the list of certs, and for each one for which we can
     * find the matching private key, add it and the keys to the
     * lists. */
    status = 0;
    for (cnode = CERT_LIST_HEAD(clist);
         (cnode != NULL) &&
             (cnode->cert != NULL) &&
             !CERT_LIST_END(cnode, clist);
         cnode = CERT_LIST_NEXT(cnode)) {
        if (cnode->cert->nickname != NULL) {
            if ((cert_label != NULL) && (cert_id != NULL)) {
                if ((strcmp(cert_id, cnode->cert->nickname) != 0) &&
                    (strcmp(cert_label, cnode->cert->nickname) != 0))
                    continue;
            } else if (cert_label != NULL) {
                if (strcmp(cert_label, cnode->cert->nickname) != 0)
                    continue;
            } else if (cert_id != NULL) {
                if (strcmp(cert_id, cnode->cert->nickname) != 0)
                    continue;
            }
        }
        key = PK11_FindPrivateKeyFromCert(slot, cnode->cert,
                                          crypto_pwcb_prep(id_cryptoctx,
                                                           identity, context));
        if (key == NULL) {
            pkiDebug("%s: no key for \"%s\", skipping it\n",
                     __FUNCTION__,
                     cnode->cert->nickname ?
                     cnode->cert->nickname : "(no name)");
            continue;
        }
        pkiDebug("%s: found \"%s\" and its matching key\n",
                 __FUNCTION__,
                 cnode->cert->nickname ? cnode->cert->nickname : "(no name)");
        /* DestroyCertList frees all of the certs in the list,
         * so we need to create a copy that it can own. */
        cert = CERT_DupCertificate(cnode->cert);
        if (cert_maybe_add_to_list(id_cryptoctx->id_certs,
                                   cert) != SECSuccess)
            status = ENOMEM;
        /* We don't need this reference to the key. */
        SECKEY_DestroyPrivateKey(key);
    }
    CERT_DestroyCertList(clist);
    return status;
}

/*
 * Reassemble the identity as it was supplied by the user or the library
 * configuration.
 */
static char *
reassemble_pkcs11_name(PLArenaPool *pool, pkinit_identity_opts *idopts)
{
    struct k5buf buf;
    int n = 0;
    char *ret;

    k5_buf_init_dynamic(&buf);
    k5_buf_add(&buf, "PKCS11:");
    n = 0;
    if (idopts->p11_module_name != NULL) {
        k5_buf_add_fmt(&buf, "%smodule_name=%s", n++ ? ":" : "",
                       idopts->p11_module_name);
    }
    if (idopts->token_label != NULL) {
        k5_buf_add_fmt(&buf, "%stoken=%s", n++ ? ":" : "",
                       idopts->token_label);
    }
    if (idopts->cert_label != NULL) {
        k5_buf_add_fmt(&buf, "%scertlabel=%s", n++ ? ":" : "",
                       idopts->cert_label);
    }
    if (idopts->cert_id_string != NULL) {
        k5_buf_add_fmt(&buf, "%scertid=%s", n++ ? ":" : "",
                       idopts->cert_id_string);
    }
    if (idopts->slotid != PK_NOSLOT) {
        k5_buf_add_fmt(&buf, "%sslotid=%ld", n++ ? ":" : "",
                       (long)idopts->slotid);
    }
    if (k5_buf_status(&buf) == 0)
        ret = PORT_ArenaStrdup(pool, buf.data);
    else
        ret = NULL;
    k5_buf_free(&buf);
    return ret;
}

/*
 * Assemble an identity string that will distinguish this token from any other
 * that is accessible through the same module, even if the user didn't specify
 * a token name.
 */
static char *
reassemble_pkcs11_identity(PLArenaPool *pool, pkinit_identity_opts *idopts,
                           long slotid, const char *tokenname)
{
    struct k5buf buf;
    int n = 0;
    char *ret;

    k5_buf_init_dynamic(&buf);
    k5_buf_add(&buf, "PKCS11:");
    n = 0;
    if (idopts->p11_module_name != NULL) {
        k5_buf_add_fmt(&buf, "%smodule_name=%s",
                       n++ ? ":" : "",
                       idopts->p11_module_name);
    }

    if (slotid != PK_NOSLOT)
        k5_buf_add_fmt(&buf, "%sslotid=%ld", n++ ? ":" : "", slotid);

    if (tokenname != NULL)
        k5_buf_add_fmt(&buf, "%stoken=%s", n++ ? ":" : "", tokenname);

    if (k5_buf_status(&buf) == 0)
        ret = PORT_ArenaStrdup(pool, buf.data);
    else
        ret = NULL;
    k5_buf_free(&buf);

    return ret;
}

static SECStatus
crypto_load_pkcs11(krb5_context context,
                   pkinit_plg_crypto_context plg_cryptoctx,
                   pkinit_req_crypto_context req_cryptoctx,
                   pkinit_identity_opts *idopts,
                   pkinit_identity_crypto_context id_cryptoctx)
{
    struct _pkinit_identity_crypto_module **id_modules, *module;
    PK11SlotInfo *slot;
    CK_TOKEN_INFO tinfo;
    char *spec, *identity;
    size_t spec_size;
    const char *tokenname;
    SECStatus status;
    int i, j;

    if (idopts == NULL)
        return SECFailure;

    /* If no module is specified, use the default module from pkinit.h. */
    if (idopts->p11_module_name == NULL) {
        idopts->p11_module_name = strdup(PKCS11_MODNAME);
        if (idopts->p11_module_name == NULL)
            return SECFailure;
    }

    /* Build the module spec. */
    spec_size = strlen("library=''") + strlen(idopts->p11_module_name) * 2 + 1;
    spec = PORT_ArenaZAlloc(id_cryptoctx->pool, spec_size);
    if (spec == NULL)
        return SECFailure;
    strlcpy(spec, "library=\"", spec_size);
    j = strlen(spec);
    for (i = 0; idopts->p11_module_name[i] != '\0'; i++) {
        if (strchr("\"", idopts->p11_module_name[i]) != NULL)
            spec[j++] = '\\';
        spec[j++] = idopts->p11_module_name[i];
    }
    spec[j++] = '\0';
    strlcat(spec, "\"", spec_size);

    /* Count the number of modules we've already loaded. */
    if (id_cryptoctx->id_modules != NULL) {
        for (i = 0; id_cryptoctx->id_modules[i] != NULL; i++)
            continue;
    } else {
        i = 0;
    }

    /* Allocate a bigger list. */
    id_modules = PORT_ArenaZAlloc(id_cryptoctx->pool,
                                  sizeof(id_modules[0]) * (i + 2));
    if (id_modules == NULL)
        return SECFailure;
    for (j = 0; j < i; j++)
        id_modules[j] = id_cryptoctx->id_modules[j];

    /* Actually load the module, or just ref an already-loaded copy. */
    module = PORT_ArenaZAlloc(id_cryptoctx->pool, sizeof(*module));
    if (module == NULL)
        return SECFailure;
    module->name = reassemble_pkcs11_name(id_cryptoctx->pool, idopts);
    if (module->name == NULL)
        return SECFailure;
    module->spec = spec;
    for (j = 0; j < i; j++) {
        if (strcmp(module->spec, id_modules[j]->spec) == 0)
            break;
    }
    if (j < i)
        module->module = SECMOD_ReferenceModule(id_modules[j]->module);
    else
        module->module = SECMOD_LoadUserModule(spec, NULL, PR_FALSE);
    if (module->module == NULL) {
        pkiDebug("%s: error loading PKCS11 module \"%s\"",
                 __FUNCTION__, idopts->p11_module_name);
        return SECFailure;
    }
    if (!module->module->loaded) {
        pkiDebug("%s: error really loading PKCS11 module \"%s\"",
                 __FUNCTION__, idopts->p11_module_name);
        SECMOD_DestroyModule(module->module);
        module->module = NULL;
        return SECFailure;
    }
    SECMOD_UpdateSlotList(module->module);
    pkiDebug("%s: loaded PKCS11 module \"%s\"\n", __FUNCTION__,
             idopts->p11_module_name);

    /* Add us to the list and set the new list. */
    id_modules[j++] = module;
    id_modules[j] = NULL;
    id_cryptoctx->id_modules = id_modules;

    /* Walk the list of slots in the module. */
    status = SECFailure;
    for (i = 0;
         (i < module->module->slotCount) &&
         ((slot = module->module->slots[i]) != NULL);
         i++) {
        PK11_TokenRefresh(slot);
        if (idopts->slotid != PK_NOSLOT) {
            if (idopts->slotid != PK11_GetSlotID(slot))
                continue;
        }
        tokenname = PK11_GetTokenName(slot);
        if (tokenname == NULL || strlen(tokenname) == 0)
            continue;
        /* If we're looking for a specific token, and this isn't it, go on. */
        if (idopts->token_label != NULL) {
            if (strcmp(idopts->cert_label, tokenname) != 0)
                continue;
        }
        /* Assemble a useful identity string, in case of an incomplete one. */
        identity = reassemble_pkcs11_identity(id_cryptoctx->pool, idopts,
                                              (long)PK11_GetSlotID(slot),
                                              tokenname);
        /*
         * Skip past all of the loading-certificates-and-keys logic, pick up
         * the token flags, and call it done for now.
         */
        if (id_cryptoctx->defer_id_prompt) {
            if (!PK11_IsLoggedIn(slot, crypto_pwcb_prep(id_cryptoctx, identity,
                                                        context)) &&
                PK11_NeedLogin(slot)) {
                pkiDebug("%s: reading flags for token \"%s\"\n",
                         __FUNCTION__, PK11_GetTokenName(slot));
                if (PK11_GetTokenInfo(slot, &tinfo) == SECSuccess) {
                    pkinit_set_deferred_id(&id_cryptoctx->deferred_ids,
                                           identity, tinfo.flags, NULL);
                }
            }
            return SECSuccess;
        }
        if (!PK11_IsPresent(slot))
            continue;
        /* Load private keys and their certs from this token. */
        if (cert_load_certs_with_keys_from_slot(context, id_cryptoctx,
                                                slot, idopts->cert_label,
                                                idopts->cert_id_string,
                                                identity) == 0)
            status = SECSuccess;
        /* If no label was specified, then we've looked at a token, so we're
         * done. */
        if (idopts->token_label == NULL)
            break;
    }

    return status;
}

/* Return the slot which we'll use for holding PEM items.  Open the module if
 * we need to, first. */
static PK11SlotInfo *
crypto_get_pem_slot(struct _pkinit_identity_crypto_context *id)
{
    PK11SlotInfo *slot;
    char *pem_module_name, *spec;
    size_t spec_size;

    if (id->pem_module == NULL) {
        pem_module_name = PR_GetLibraryName(NULL, PEM_MODULE);
        if (pem_module_name == NULL) {
            pkiDebug("%s: error determining library name for %s\n",
                     __FUNCTION__, PEM_MODULE);
            return NULL;
        }
        spec_size = strlen("library=") + strlen(pem_module_name) + 1;
        spec = malloc(spec_size);
        if (spec == NULL) {
            pkiDebug("%s: out of memory building spec for %s\n",
                     __FUNCTION__, pem_module_name);
            PR_FreeLibraryName(pem_module_name);
            return NULL;
        }
        snprintf(spec, spec_size, "library=%s", pem_module_name);
        id->pem_module = SECMOD_LoadUserModule(spec, NULL, PR_FALSE);
        if (id->pem_module == NULL)
            pkiDebug("%s: error loading %s\n", __FUNCTION__, pem_module_name);
        else if (!id->pem_module->loaded)
            pkiDebug("%s: error really loading %s\n", __FUNCTION__,
                     pem_module_name);
        else
            SECMOD_UpdateSlotList(id->pem_module);
        free(spec);
        PR_FreeLibraryName(pem_module_name);
    }
    if ((id->pem_module != NULL) && id->pem_module->loaded) {
        if (id->pem_module->slotCount != 0)
            slot = id->pem_module->slots[0];
        else
            slot = NULL;
        if (slot == NULL)
            pkiDebug("%s: no slots in %s?\n", __FUNCTION__, PEM_MODULE);
    } else {
        slot = NULL;
    }
    return slot;
}

/* Resolve any ambiguities from having a duplicate nickname in the PKCS12
 * bundle and in the database, or the bag not providing a nickname.  Note: you
 * might expect "arg" to be a wincx, but it's actually a certificate!  (Mozilla
 * bug #321584, fixed in 3.12, documented by #586163, in 3.13.) */
static SECItem *
crypto_nickname_c_cb(SECItem *old_nickname, PRBool *cancel, void *arg)
{
    CERTCertificate *leaf;
    char *old_name, *new_name, *p;
    SECItem *new_nickname, tmp;
    size_t new_name_size;
    int i;

    leaf = arg;
    if (old_nickname != NULL)
        pkiDebug("%s: warning: nickname collision on \"%.*s\", "
                 "generating a new nickname\n", __FUNCTION__,
                 old_nickname->len, old_nickname->data);
    else
        pkiDebug("%s: warning: nickname collision, generating a new "
                 "nickname\n", __FUNCTION__);
    new_nickname = NULL;
    if (old_nickname == NULL) {
        old_name = leaf->subjectName;
        new_name_size = strlen(PKCS12_PREFIX ":  #1") + strlen(old_name) + 1;
        new_name = PR_Malloc(new_name_size);
        if (new_name != NULL) {
            snprintf(new_name, new_name_size, PKCS12_PREFIX ": %s #1",
                     old_name);
            tmp.data = (unsigned char *) new_name;
            tmp.len = strlen(new_name) + 1;
            new_nickname = SECITEM_DupItem(&tmp);
            PR_Free(new_name);
        }
    } else {
        old_name = (char *) old_nickname->data;
        if (strncmp(old_name, PKCS12_PREFIX ": ",
                    strlen(PKCS12_PREFIX) + 2) == 0) {
            p = strrchr(old_name, '#');
            i = (p ? atoi(p + 1) : 0) + 1;
            old_name = leaf->subjectName;
            new_name_size = strlen(PKCS12_PREFIX ":  #") +
                strlen(old_name) + 3 * sizeof(i) + 1;
            new_name = PR_Malloc(new_name_size);
        } else {
            old_name = leaf->subjectName;
            new_name_size = strlen(PKCS12_PREFIX ":  #1") +
                strlen(old_name) + 1;
            new_name = PR_Malloc(new_name_size);
            i = 1;
        }
        if (new_name != NULL) {
            snprintf(new_name, new_name_size, PKCS12_PREFIX ": %s #%d",
                     old_name, i);
            tmp.data = (unsigned char *) new_name;
            tmp.len = strlen(new_name) + 1;
            new_nickname = SECITEM_DupItem(&tmp);
            PR_Free(new_name);
        }
    }
    if (new_nickname == NULL) {
        pkiDebug("%s: warning: unable to generate a new nickname\n",
                 __FUNCTION__);
        *cancel = PR_TRUE;
    } else {
        pkiDebug("%s: generated new nickname \"%.*s\"\n",
                 __FUNCTION__, new_nickname->len, new_nickname->data);
        *cancel = PR_FALSE;
    }
    return new_nickname;
}

static char *
reassemble_pkcs12_name(PLArenaPool *pool, const char *filename)
{
    char *tmp, *ret;

    if (asprintf(&tmp, "PKCS12:%s", filename) < 0)
        return NULL;
    ret = PORT_ArenaStrdup(pool, tmp);
    free(tmp);
    return ret;
}

static SECStatus
crypto_load_pkcs12(krb5_context context,
                   pkinit_plg_crypto_context plg_cryptoctx,
                   pkinit_req_crypto_context req_cryptoctx,
                   const char *name,
                   pkinit_identity_crypto_context id_cryptoctx)
{
    PK11SlotInfo *slot;
    SEC_PKCS12DecoderContext *ctx;
    unsigned char emptypwd[] = { '\0', '\0' };
    SECItem tmp, password;
    PRBool retry;
    int attempt;
    char *identity;

    if ((slot = crypto_get_p12_slot(id_cryptoctx)) == NULL) {
        pkiDebug("%s: skipping identity PKCS12 bundle \"%s\": "
                 "no slot found\n", __FUNCTION__, name);
        return SECFailure;
    }
    if (secitem_from_file(id_cryptoctx->pool, name,
                          secitem_from_file_decode, &tmp) != 0) {
        pkiDebug("%s: skipping identity PKCS12 bundle \"%s\": "
                 "error reading from file\n", __FUNCTION__, name);
        return SECFailure;
    }
    /* There's a chance we'll need these. */
    SEC_PKCS12EnableCipher(PKCS12_RC2_CBC_40, PR_TRUE);
    SEC_PKCS12EnableCipher(PKCS12_RC2_CBC_128, PR_TRUE);
    SEC_PKCS12EnableCipher(PKCS12_RC4_40, PR_TRUE);
    SEC_PKCS12EnableCipher(PKCS12_RC4_128, PR_TRUE);
    SEC_PKCS12EnableCipher(PKCS12_DES_56, PR_TRUE);
    SEC_PKCS12EnableCipher(PKCS12_DES_EDE3_168, PR_TRUE);
    /* Pass in the password. */
    memset(&password, 0, sizeof(password));
    password.data = emptypwd;
    password.len = 2;
    attempt = 0;
    ctx = NULL;
    identity = reassemble_pkcs12_name(id_cryptoctx->pool, name);
    if (identity == NULL)
        return SECFailure;
    id_cryptoctx->id_p12_slot.p12name = identity;
    do {
        retry = PR_FALSE;
        ctx = SEC_PKCS12DecoderStart(&password,
                                     slot,
                                     crypto_pwcb_prep(id_cryptoctx, identity,
                                                      context),
                                     NULL, NULL, NULL, NULL, NULL);
        if (ctx == NULL) {
            pkiDebug("%s: skipping identity PKCS12 bundle \"%s\": "
                     "error setting up decoder\n", __FUNCTION__, name);
            return SECFailure;
        }
        if (SEC_PKCS12DecoderUpdate(ctx, tmp.data, tmp.len) != SECSuccess) {
            pkiDebug("%s: skipping identity PKCS12 bundle \"%s\": "
                     "error passing data to decoder\n", __FUNCTION__, name);
            SEC_PKCS12DecoderFinish(ctx);
            return SECFailure;
        }
        if (SEC_PKCS12DecoderVerify(ctx) != SECSuccess) {
            char *newpass;
            krb5_ucs2 *ucs2;
            unsigned char *ucs2s;
            size_t i, n_ucs2s;
            SECErrorCodes err;

            err = PORT_GetError();
            SEC_PKCS12DecoderFinish(ctx);
            switch (err) {
            case SEC_ERROR_BAD_PASSWORD:
                if (id_cryptoctx->defer_id_prompt) {
                    pkinit_set_deferred_id(&id_cryptoctx->deferred_ids,
                                           identity, 0, NULL);
                    return SECSuccess;
                }
                pkiDebug("%s: prompting for password for %s\n",
                         __FUNCTION__, name);
                newpass = crypto_pwfn(name, PR_FALSE, 0, (attempt > 0),
                                      id_cryptoctx);
                attempt++;
                if (newpass != NULL) {
                    /* convert to 16-bit big-endian */
                    if (krb5int_utf8s_to_ucs2les(newpass,
                                                 &ucs2s, &n_ucs2s) == 0) {
                        PR_Free(newpass);
                        ucs2 = (krb5_ucs2 *) ucs2s;
                        for (i = 0; i < n_ucs2s / 2; i++)
                            ucs2[i] = SWAP16(ucs2[i]);
                        password.data = (void *) ucs2s;
                        password.len = n_ucs2s + 2;
                        PORT_SetError(0);
                        retry = PR_TRUE;
                        continue;
                    }
                    PR_Free(newpass);
                }
                break;
            default:
                break;
            }
            pkiDebug("%s: skipping identity PKCS12 bundle \"%s\": "
                     "error verifying data: %d\n", __FUNCTION__,
                     name, PORT_GetError());
            return SECFailure;
        }
    } while (retry);
    if (SEC_PKCS12DecoderValidateBags(ctx,
                                      crypto_nickname_c_cb) != SECSuccess) {
        pkiDebug("%s: skipping identity PKCS12 bundle \"%s\": "
                 "error validating bags: %d\n", __FUNCTION__, name,
                 PORT_GetError());
        SEC_PKCS12DecoderFinish(ctx);
        if (password.data != emptypwd)
            free(password.data);
        return SECFailure;
    }
    if (SEC_PKCS12DecoderImportBags(ctx) != SECSuccess) {
        pkiDebug("%s: skipping identity PKCS12 bundle \"%s\": "
                 "error importing data: %d\n", __FUNCTION__, name,
                 PORT_GetError());
        SEC_PKCS12DecoderFinish(ctx);
        if (password.data != emptypwd)
            free(password.data);
        return SECFailure;
    }
    pkiDebug("%s: imported PKCS12 bundle \"%s\"\n", __FUNCTION__, name);
    SEC_PKCS12DecoderFinish(ctx);
    if (password.data != emptypwd)
        free(password.data);
    if (cert_load_certs_with_keys_from_slot(context, id_cryptoctx, slot,
                                            NULL, NULL, identity) == 0)
        return SECSuccess;
    else
        return SECFailure;
}

/* Helper to fill out a CK_ATTRIBUTE. */
static void
crypto_set_attributes(CK_ATTRIBUTE *attr,
                      CK_ATTRIBUTE_TYPE type,
                      void *pValue, CK_ULONG ulValueLen)
{
    memset(attr, 0, sizeof(*attr));
    attr->type = type;
    attr->pValue = pValue;
    attr->ulValueLen = ulValueLen;
}

static char *
reassemble_files_name(PLArenaPool *pool, const char *certfile,
                      const char *keyfile)
{
    char *tmp, *ret;

    if (keyfile != NULL) {
        if (asprintf(&tmp, "FILE:%s,%s", certfile, keyfile) < 0)
            return NULL;
    } else {
        if (asprintf(&tmp, "FILE:%s", certfile) < 0)
            return NULL;
    }
    ret = PORT_ArenaStrdup(pool, tmp);
    free(tmp);
    return ret;
}

/* Load keys, certs, and/or CRLs from files. */
static SECStatus
crypto_load_files(krb5_context context,
                  pkinit_plg_crypto_context plg_cryptoctx,
                  pkinit_req_crypto_context req_cryptoctx,
                  const char *certfile,
                  const char *keyfile,
                  const char *crlfile,
                  PRBool cert_self, PRBool cert_mark_trusted,
                  pkinit_identity_crypto_context id_cryptoctx)
{
    PK11SlotInfo *slot;
    struct _pkinit_identity_crypto_file *cobj, *kobj, **id_objects;
    PRBool permanent;
    SECKEYPrivateKey *key;
    CK_ATTRIBUTE attrs[4];
    CK_BBOOL cktrue = CK_TRUE, cktrust;
    CK_OBJECT_CLASS keyclass = CKO_PRIVATE_KEY, certclass = CKO_CERTIFICATE;
    CERTCertificate *cert;
    SECItem tmp, *crl, **crls;
    SECStatus status;
    int i, j, n_attrs, n_objs, n_crls;

    if ((slot = crypto_get_pem_slot(id_cryptoctx)) == NULL) {
        if (certfile != NULL)
            pkiDebug("%s: nsspem module not loaded, not loading file \"%s\"\n",
                     __FUNCTION__, certfile);
        if (keyfile != NULL)
            pkiDebug("%s: nsspem module not loaded, not loading file \"%s\"\n",
                     __FUNCTION__, keyfile);
        if (crlfile != NULL)
            pkiDebug("%s: nsspem module not loaded, not loading file \"%s\"\n",
                     __FUNCTION__, crlfile);
        return SECFailure;
    }
    if ((certfile == NULL) && (crlfile == NULL))
        return SECFailure;

    /* Load the certificate first to work around RHBZ#859535. */
    cobj = NULL;
    if (certfile != NULL) {
        n_attrs = 0;
        crypto_set_attributes(&attrs[n_attrs++], CKA_CLASS,
                              &certclass, sizeof(certclass));
        crypto_set_attributes(&attrs[n_attrs++], CKA_TOKEN,
                              &cktrue, sizeof(cktrue));
        crypto_set_attributes(&attrs[n_attrs++], CKA_LABEL,
                              (char *) certfile, strlen(certfile) + 1);
        cktrust = cert_mark_trusted ? CK_TRUE : CK_FALSE;
        crypto_set_attributes(&attrs[n_attrs++], CKA_TRUST,
                              &cktrust, sizeof(cktrust));
        permanent = PR_FALSE;   /* set lifetime to "session" */
        cobj = PORT_ArenaZAlloc(id_cryptoctx->pool, sizeof(*cobj));
        if (cobj == NULL)
            return SECFailure;
        cobj->name = reassemble_files_name(id_cryptoctx->pool,
                                           certfile, keyfile);
        if (cobj->name == NULL)
            return SECFailure;
        cobj->obj = PK11_CreateGenericObject(slot, attrs, n_attrs, permanent);
        if (cobj->obj == NULL) {
            pkiDebug("%s: error loading %scertificate \"%s\": %s\n",
                     __FUNCTION__, cert_mark_trusted ? "CA " : "", certfile,
                     PORT_ErrorToName(PORT_GetError()));
            status = SECFailure;
        } else {
            pkiDebug("%s: loaded %scertificate \"%s\"\n",
                     __FUNCTION__, cert_mark_trusted ? "CA " : "", certfile);
            status = SECSuccess;
            /* Add it to the list of objects that we're keeping. */
            if (id_cryptoctx->id_objects != NULL)
                for (i = 0; id_cryptoctx->id_objects[i] != NULL; i++)
                    continue;
            else
                i = 0;
            id_objects = PORT_ArenaZAlloc(id_cryptoctx->pool,
                                          sizeof(id_objects[0]) * (i + 2));
            if (id_objects != NULL) {
                n_objs = i;
                for (i = 0; i < n_objs; i++)
                    id_objects[i] = id_cryptoctx->id_objects[i];
                id_objects[i++] = cobj;
                id_objects[i++] = NULL;
                id_cryptoctx->id_objects = id_objects;
            }
            /* Find the certificate that goes with this generic object. */
            memset(&tmp, 0, sizeof(tmp));
            status = PK11_ReadRawAttribute(PK11_TypeGeneric, cobj->obj,
                                           CKA_VALUE, &tmp);
            if (status == SECSuccess) {
                cobj->cert = CERT_FindCertByDERCert(CERT_GetDefaultCertDB(),
                                                    &tmp);
                SECITEM_FreeItem(&tmp, PR_FALSE);
            } else {
                pkiDebug("%s: error locating certificate \"%s\"\n",
                         __FUNCTION__, certfile);
            }
            /* Save a reference to the right list. */
            if (cobj->cert != NULL) {
                cert = CERT_DupCertificate(cobj->cert);
                if (cert == NULL)
                    return SECFailure;
                if (cert_self) {
                    /* Add to the identity list. */
                    if (cert_maybe_add_to_list(id_cryptoctx->id_certs,
                                               cert) != SECSuccess)
                        status = SECFailure;
                } else if (cert_mark_trusted) {
                    /* Add to the CA list. */
                    if (cert_maybe_add_to_list(id_cryptoctx->ca_certs,
                                               cert) != SECSuccess)
                        status = SECFailure;
                } else {
                    /* Don't just lose the reference. */
                    CERT_DestroyCertificate(cert);
                }
            }
        }
    }

    /* Now load what should be the corresponding private key. */
    kobj = NULL;
    if (status == SECSuccess && keyfile != NULL) {
        n_attrs = 0;
        crypto_set_attributes(&attrs[n_attrs++], CKA_CLASS,
                              &keyclass, sizeof(keyclass));
        crypto_set_attributes(&attrs[n_attrs++], CKA_TOKEN,
                              &cktrue, sizeof(cktrue));
        crypto_set_attributes(&attrs[n_attrs++], CKA_LABEL,
                              (char *)keyfile, strlen(keyfile) + 1);
        permanent = PR_FALSE;   /* set lifetime to "session" */
        kobj = PORT_ArenaZAlloc(id_cryptoctx->pool, sizeof(*kobj));
        if (kobj == NULL)
            return SECFailure;
        kobj->obj = PK11_CreateGenericObject(slot, attrs, n_attrs, permanent);
        if (kobj->obj == NULL) {
            pkiDebug("%s: error loading key \"%s\": %s\n", __FUNCTION__,
                     keyfile, PORT_ErrorToName(PORT_GetError()));
            status = SECFailure;
        } else {
            pkiDebug("%s: loaded key \"%s\"\n", __FUNCTION__, keyfile);
            status = SECSuccess;
            /* Add it to the list of objects that we're keeping. */
            if (id_cryptoctx->id_objects != NULL) {
                for (i = 0; id_cryptoctx->id_objects[i] != NULL; i++)
                    continue;
            } else {
                i = 0;
            }
            id_objects = PORT_ArenaZAlloc(id_cryptoctx->pool,
                                          sizeof(id_objects[0]) * (i + 2));
            if (id_objects != NULL) {
                n_objs = i;
                for (i = 0; i < n_objs; i++)
                    id_objects[i] = id_cryptoctx->id_objects[i];
                id_objects[i++] = kobj;
                id_objects[i++] = NULL;
                id_cryptoctx->id_objects = id_objects;
            }
        }

        /* "Log in" (provide an encryption password) if the PEM slot now
         * requires it. */
        PK11_TokenRefresh(slot);

        /*
         * Unlike most tokens, this one won't self-destruct if we throw wrong
         * passwords at it, but it will cause the module to clear the
         * needs-login flag so that we can continue importing PEM items.
         */
        if (!PK11_IsLoggedIn(slot, crypto_pwcb_prep(id_cryptoctx, cobj->name,
                                                    context)) &&
            PK11_NeedLogin(slot)) {
            pkiDebug("%s: logging in to token \"%s\"\n",
                     __FUNCTION__, PK11_GetTokenName(slot));
            if (PK11_Authenticate(slot, PR_TRUE,
                                  crypto_pwcb_prep(id_cryptoctx, cobj->name,
                                                   context)) != SECSuccess) {
                pkiDebug("%s: error logging into \"%s\": %s, skipping\n",
                         __FUNCTION__, PK11_GetTokenName(slot),
                         PORT_ErrorToName(PORT_GetError()));
                status = SECFailure;
                PK11_DestroyGenericObject(kobj->obj);
                kobj->obj = NULL;
            }
        }

        /* If we loaded a key and a certificate, see if they match. */
        if (cobj != NULL && cobj->cert != NULL && kobj->obj != NULL) {
            key = PK11_FindPrivateKeyFromCert(slot, cobj->cert,
                                              crypto_pwcb_prep(id_cryptoctx,
                                                               cobj->name,
                                                               context));
            if (key == NULL) {
                pkiDebug("%s: no private key found for \"%s\"(%s), "
                         "even though we just loaded that key?\n",
                         __FUNCTION__,
                         cobj->cert->nickname ?
                         cobj->cert->nickname : "(no name)",
                         certfile);
                status = SECFailure;
            } else {
                /* We don't need this reference to the key. */
                SECKEY_DestroyPrivateKey(key);
            }
        }
    }

    /* If we succeeded to this point, or more likely didn't do anything
     * yet, cache a CRL. */
    if ((status == SECSuccess) && (crlfile != NULL)) {
        memset(&tmp, 0, sizeof(tmp));
        if (secitem_from_file(id_cryptoctx->pool, crlfile,
                              secitem_from_file_decode, &tmp) == 0) {
            crl = SECITEM_ArenaDupItem(id_cryptoctx->pool, &tmp);
            /* Count the CRLs. */
            if (id_cryptoctx->id_crls != NULL) {
                for (i = 0; id_cryptoctx->id_crls[i] != NULL; i++)
                    continue;
            } else {
                i = 0;
            }
            n_crls = i;
            /* Allocate a bigger list. */
            crls = PORT_ArenaZAlloc(id_cryptoctx->pool,
                                    sizeof(crls[0]) * (n_crls + 2));
            for (j = 0; j < n_crls; j++)
                crls[j] = id_cryptoctx->id_crls[j];
            if (crl != NULL) {
                status = CERT_CacheCRL(CERT_GetDefaultCertDB(), crl);
                if (status == SECSuccess) {
                    crls[j++] = crl;
                    pkiDebug("%s: cached CRL from \"%s\"\n",
                             __FUNCTION__, crlfile);
                } else
                    pkiDebug("%s: error loading CRL from \"%s\": %d\n",
                             __FUNCTION__, crlfile, PORT_GetError());
            }
            crls[j++] = NULL;
            id_cryptoctx->id_crls = crls;
        } else
            status = SECFailure;
    }
    return status;
}

static SECStatus
crypto_load_dir(krb5_context context,
                pkinit_plg_crypto_context plg_cryptoctx,
                pkinit_req_crypto_context req_cryptoctx,
                const char *dirname,
                PRBool cert_self, PRBool cert_mark_trusted, PRBool load_crl,
                pkinit_identity_crypto_context id_cryptoctx)
{
    SECStatus status;
    DIR *dir;
    struct dirent *ent;
    char *key, *certcrl;
    const char *suffix = load_crl ? ".crl" : ".crt";
    int i;

    if (crypto_get_pem_slot(id_cryptoctx) == NULL) {
        pkiDebug("%s: nsspem module not loaded, "
                 "not loading directory \"%s\"\n", __FUNCTION__, dirname);
        return SECFailure;
    }
    if (dirname == NULL)
        return SECFailure;
    dir = opendir(dirname);
    if (dir == NULL) {
        pkiDebug("%s: error loading directory \"%s\": %s\n",
                 __FUNCTION__, dirname, strerror(errno));
        return SECFailure;
    }
    status = SECFailure;
    pkiDebug("%s: scanning directory \"%s\"\n", __FUNCTION__, dirname);
    while ((ent = readdir(dir)) != NULL) {
        i = strlen(ent->d_name);
        /* Skip over anything that isn't named "<something>.crt" or
         * "<something>.crl", whichever we want at the moment. */
        if ((i < 5) || (strcmp(ent->d_name + i - 4, suffix) != 0)) {
            pkiDebug("%s: skipping candidate \"%s/%s\"\n",
                     __FUNCTION__, dirname, ent->d_name);
            continue;
        }
        /* Construct a path to the file. */
        certcrl = NULL;
        if (k5_path_join(dirname, ent->d_name, &certcrl) != 0) {
            pkiDebug("%s: error building pathname \"%s %s\"\n",
                     __FUNCTION__, dirname, ent->d_name);
            continue;
        }
        key = NULL;
        if (!load_crl && cert_self) {   /* No key. */
            /* Construct the matching key name. */
            if (k5_path_join(dirname, ent->d_name, &key) != 0) {
                pkiDebug("%s: error building pathname \"%s %s\"\n",
                         __FUNCTION__, dirname, ent->d_name);
                free(certcrl);
                continue;
            }
            i = strlen(key);
            memcpy(key + i - 4, ".key", 5);
        }
        /* Try loading the key and file as a pair. */
        if (crypto_load_files(context,
                              plg_cryptoctx,
                              req_cryptoctx,
                              load_crl ? NULL : certcrl,
                              key,
                              load_crl ? certcrl : NULL,
                              cert_self, cert_mark_trusted,
                              id_cryptoctx) == SECSuccess)
            status = SECSuccess;
        free(certcrl);
        free(key);
    }
    closedir(dir);
    return status;
}

static char *
reassemble_nssdb_name(PLArenaPool *pool, const char *dbdir)
{
    char *tmp, *ret;

    if (asprintf(&tmp, "NSS:%s", dbdir) < 0)
        return NULL;
    ret = PORT_ArenaStrdup(pool, tmp);
    free(tmp);
    return ret;
}

/* Load up a certificate database. */
static krb5_error_code
crypto_load_nssdb(krb5_context context,
                  pkinit_plg_crypto_context plg_cryptoctx,
                  pkinit_req_crypto_context req_cryptoctx,
                  const char *configdir,
                  pkinit_identity_crypto_context id_cryptoctx)
{
    struct _pkinit_identity_crypto_userdb *userdb, **id_userdbs;
    char *p;
    size_t spec_size;
    int i, j;

    if (configdir == NULL)
        return ENOENT;

    /* Build the spec. */
    spec_size = strlen("configDir='' flags=readOnly") +
        strlen(configdir) * 2 + 1;
    p = PORT_ArenaZAlloc(id_cryptoctx->pool, spec_size);
    if (p == NULL)
        return ENOMEM;
    strlcpy(p, "configDir='", spec_size);
    j = strlen(p);
    for (i = 0; configdir[i] != '\0'; i++) {
        if (configdir[i] == '\'')
            p[j++] = '\\';      /* Is this the right way to do
                                 * escaping? */
        p[j++] = configdir[i];
    }
    p[j++] = '\0';
    strlcat(p, "' flags=readOnly", spec_size);

    /* Count the number of modules we've already loaded. */
    if (id_cryptoctx->id_userdbs != NULL) {
        for (i = 0; id_cryptoctx->id_userdbs[i] != NULL; i++)
            continue;
    } else
        i = 0;

    /* Allocate a bigger list. */
    id_userdbs = PORT_ArenaZAlloc(id_cryptoctx->pool,
                                  sizeof(id_userdbs[0]) * (i + 2));
    for (j = 0; j < i; j++)
        id_userdbs[j] = id_cryptoctx->id_userdbs[j];

    /* Actually load the module. */
    userdb = PORT_ArenaZAlloc(id_cryptoctx->pool, sizeof(*userdb));
    if (userdb == NULL)
        return SECFailure;
    userdb->name = reassemble_nssdb_name(id_cryptoctx->pool, configdir);
    if (userdb->name == NULL)
        return SECFailure;
    userdb->userdb = SECMOD_OpenUserDB(p);
    if (userdb->userdb == NULL) {
        pkiDebug("%s: error loading NSS cert database \"%s\"\n",
                 __FUNCTION__, configdir);
        return ENOENT;
    }
    pkiDebug("%s: opened NSS database \"%s\"\n", __FUNCTION__, configdir);

    /* Add us to the list and set the new list. */
    id_userdbs[i++] = userdb;
    id_userdbs[i++] = NULL;
    id_cryptoctx->id_userdbs = id_userdbs;

    /* Load the CAs from the database. */
    cert_load_ca_certs_from_slot(context, id_cryptoctx, userdb->userdb,
                                 userdb->name);

    /* Load the keys from the database. */
    return cert_load_certs_with_keys_from_slot(context, id_cryptoctx,
                                               userdb->userdb, NULL, NULL,
                                               userdb->name);
}

/* Load up a certificate and associated key. */
krb5_error_code
crypto_load_certs(krb5_context context,
                  pkinit_plg_crypto_context plg_cryptoctx,
                  pkinit_req_crypto_context req_cryptoctx,
                  pkinit_identity_opts *idopts,
                  pkinit_identity_crypto_context id_cryptoctx,
                  krb5_principal princ,
                  krb5_boolean defer_id_prompts)
{
    SECStatus status;

    id_cryptoctx->defer_id_prompt = defer_id_prompts;

    switch (idopts->idtype) {
    case IDTYPE_FILE:
        id_cryptoctx->defer_with_dummy_password = TRUE;
        status = crypto_load_files(context,
                                   plg_cryptoctx,
                                   req_cryptoctx,
                                   idopts->cert_filename,
                                   idopts->key_filename,
                                   NULL, PR_TRUE, PR_FALSE, id_cryptoctx);
        if (status != SECSuccess) {
            pkiDebug("%s: error loading files \"%s\" and \"%s\": %s\n",
                     __FUNCTION__, idopts->cert_filename,
                     idopts->key_filename, PORT_ErrorToName(PORT_GetError()));
            return defer_id_prompts ? 0 : ENOMEM;
        }
        return 0;
        break;
    case IDTYPE_NSS:
        id_cryptoctx->defer_with_dummy_password = FALSE;
        status = crypto_load_nssdb(context,
                                   plg_cryptoctx,
                                   req_cryptoctx,
                                   idopts->cert_filename, id_cryptoctx);
        if (status != SECSuccess) {
            pkiDebug("%s: error loading NSS certdb \"%s\": %s\n",
                     __FUNCTION__, idopts->cert_filename,
                     PORT_ErrorToName(PORT_GetError()));
            return ENOMEM;
        }
        return 0;
        break;
    case IDTYPE_DIR:
        id_cryptoctx->defer_with_dummy_password = TRUE;
        status = crypto_load_dir(context,
                                 plg_cryptoctx,
                                 req_cryptoctx,
                                 idopts->cert_filename,
                                 PR_TRUE, PR_FALSE, PR_FALSE, id_cryptoctx);
        if (status != SECSuccess) {
            pkiDebug("%s: error loading directory \"%s\": %s\n",
                     __FUNCTION__, idopts->cert_filename,
                     PORT_ErrorToName(PORT_GetError()));
            return defer_id_prompts ? 0 : ENOMEM;
        }
        return 0;
        break;
    case IDTYPE_PKCS11:
        id_cryptoctx->defer_with_dummy_password = FALSE;
        status = crypto_load_pkcs11(context,
                                    plg_cryptoctx,
                                    req_cryptoctx, idopts, id_cryptoctx);
        if (status != SECSuccess) {
            pkiDebug("%s: error loading module \"%s\": %s\n",
                     __FUNCTION__, idopts->p11_module_name,
                     PORT_ErrorToName(PORT_GetError()));
            return ENOMEM;
        }
        return 0;
        break;
    case IDTYPE_PKCS12:
        id_cryptoctx->defer_with_dummy_password = FALSE;
        status = crypto_load_pkcs12(context,
                                    plg_cryptoctx,
                                    req_cryptoctx,
                                    idopts->cert_filename, id_cryptoctx);
        if (status != SECSuccess) {
            pkiDebug("%s: error loading PKCS12 bundle \"%s\"\n",
                     __FUNCTION__, idopts->cert_filename);
            return ENOMEM;
        }
        return 0;
        break;
    default:
        return EINVAL;
        break;
    }
}

/* Drop "self" certificate and keys that we didn't select. */
krb5_error_code
crypto_free_cert_info(krb5_context context,
                      pkinit_plg_crypto_context plg_cryptoctx,
                      pkinit_req_crypto_context req_cryptoctx,
                      pkinit_identity_crypto_context id_cryptoctx)
{
    /* Mimic the OpenSSL-based implementation's check first. */
    if (id_cryptoctx == NULL)
        return EINVAL;

    /* Maybe should we nuke the id_certs list here? */
    return 0;
}

/* Count how many candidate "self" certificates and keys we have.  We could as
 * easily count the keys. */
krb5_error_code
crypto_cert_get_count(krb5_context context,
                      pkinit_plg_crypto_context plg_cryptoctx,
                      pkinit_req_crypto_context req_cryptoctx,
                      pkinit_identity_crypto_context id_cryptoctx,
                      int *cert_count)
{
    CERTCertListNode *node;

    *cert_count = 0;
    if (!CERT_LIST_EMPTY(id_cryptoctx->id_certs))
        for (node = CERT_LIST_HEAD(id_cryptoctx->id_certs);
             (node != NULL) &&
                 (node->cert != NULL) &&
                 !CERT_LIST_END(node, id_cryptoctx->id_certs);
             node = CERT_LIST_NEXT(node))
            (*cert_count)++;
    pkiDebug("%s: %d candidate key/certificate pairs found\n",
             __FUNCTION__, *cert_count);
    return 0;
}

/* Start walking the list of "self" certificates and keys. */
krb5_error_code
crypto_cert_iteration_begin(krb5_context context,
                            pkinit_plg_crypto_context plg_cryptoctx,
                            pkinit_req_crypto_context req_cryptoctx,
                            pkinit_identity_crypto_context id_cryptoctx,
                            pkinit_cert_iter_handle *iter_handle)
{
    PLArenaPool *pool;
    struct _pkinit_cert_iter_info *handle;

    if (CERT_LIST_EMPTY(id_cryptoctx->id_certs))
        return ENOENT;
    pool = PORT_NewArena(sizeof(double));
    if (pool == NULL)
        return ENOMEM;
    handle = PORT_ArenaZAlloc(pool, sizeof(*handle));
    if (handle == NULL) {
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
    handle->pool = pool;
    handle->id_cryptoctx = id_cryptoctx;
    handle->node = CERT_LIST_HEAD(handle->id_cryptoctx->id_certs);
    *iter_handle = handle;
    return 0;
}

/* Stop walking the list of "self" certificates and keys. */
krb5_error_code
crypto_cert_iteration_end(krb5_context context,
                          pkinit_cert_iter_handle iter_handle)
{
    PORT_FreeArena(iter_handle->pool, PR_TRUE);
    return 0;
}

/* Walk to the first/next "self" certificate and key.  The cert_handle we
 * produce here has to be useful beyond the life of the iteration handle, so it
 * can't be allocated from the iteration handle's memory pool. */
krb5_error_code
crypto_cert_iteration_next(krb5_context context,
                           pkinit_cert_iter_handle iter_handle,
                           pkinit_cert_handle *cert_handle)
{
    PLArenaPool *pool;

    /* Check if we're at the last node. */
    if (CERT_LIST_END(iter_handle->node,
                      iter_handle->id_cryptoctx->id_certs)) {
        /* No more entries. */
        *cert_handle = NULL;
        return PKINIT_ITER_NO_MORE;
    }
    /* Create a pool to hold info about this certificate. */
    pool = PORT_NewArena(sizeof(double));
    if (pool == NULL)
        return ENOMEM;
    *cert_handle = PORT_ArenaZAlloc(pool, sizeof(**cert_handle));
    if (*cert_handle == NULL) {
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
    (*cert_handle)->pool = pool;
    /* Return a copy of the certificate in this node, and then move on to
     * the next one. */
    (*cert_handle)->id_cryptoctx = iter_handle->id_cryptoctx;
    (*cert_handle)->cert = CERT_DupCertificate(iter_handle->node->cert);
    iter_handle->node = CERT_LIST_NEXT(iter_handle->node);
    return 0;
}

/* Read names, key usage, and extended key usage from the cert. */
static SECItem *
cert_get_ext_by_tag(CERTCertificate *cert, SECOidTag tag)
{
    SECOidData *oid;
    int i;

    oid = SECOID_FindOIDByTag(tag);
    for (i = 0;
         (cert->extensions != NULL) && (cert->extensions[i] != NULL);
         i++)
        if (SECITEM_ItemsAreEqual(&cert->extensions[i]->id, &oid->oid))
            return &cert->extensions[i]->value;
    return NULL;
}

/* Check for the presence of a particular key usage in the cert's keyUsage
 * extension field.  If it's not there, NSS just sets all of the bits, which is
 * consistent with what the OpenSSL version of this does. */
static unsigned int
cert_get_ku_bits(krb5_context context, CERTCertificate *cert)
{
    unsigned int ku = 0;

    if (cert->keyUsage & KU_DIGITAL_SIGNATURE)
        ku |= PKINIT_KU_DIGITALSIGNATURE;
    if (cert->keyUsage & KU_KEY_ENCIPHERMENT)
        ku |= PKINIT_KU_KEYENCIPHERMENT;
    return ku;
}

static unsigned int
cert_get_eku_bits(krb5_context context, CERTCertificate *cert, PRBool kdc)
{
    PLArenaPool *pool;
    SECItem *ext, **oids;
    SECOidData *clientauth, *serverauth, *email;
    int i;
    unsigned int eku;

    /* Pull out the extension. */
    ext = cert_get_ext_by_tag(cert, SEC_OID_X509_EXT_KEY_USAGE);
    if (ext == NULL)
        return 0;

    /* Look up the well-known OIDs. */
    clientauth = SECOID_FindOIDByTag(SEC_OID_EXT_KEY_USAGE_CLIENT_AUTH);
    serverauth = SECOID_FindOIDByTag(SEC_OID_EXT_KEY_USAGE_SERVER_AUTH);
    email = SECOID_FindOIDByTag(SEC_OID_EXT_KEY_USAGE_EMAIL_PROTECT);

    /* Decode the list of OIDs. */
    pool = PORT_NewArena(sizeof(double));
    oids = NULL;
    if (SEC_ASN1DecodeItem(pool, &oids,
                           SEC_ASN1_GET(SEC_SequenceOfObjectIDTemplate),
                           ext) != SECSuccess) {
        PORT_FreeArena(pool, PR_TRUE);
        return 0;
    }
    eku = 0;
    for (i = 0; (oids != NULL) && (oids[i] != NULL); i++) {
        if (SECITEM_ItemsAreEqual(oids[i], &email->oid))
            eku |= PKINIT_EKU_EMAILPROTECTION;
        if (kdc) {
            if (SECITEM_ItemsAreEqual(oids[i], &pkinit_kp_kdc))
                eku |= PKINIT_EKU_PKINIT;
            if (SECITEM_ItemsAreEqual(oids[i], &serverauth->oid))
                eku |= PKINIT_EKU_CLIENTAUTH;
        } else {
            if (SECITEM_ItemsAreEqual(oids[i], &pkinit_kp_client))
                eku |= PKINIT_EKU_PKINIT;
            if (SECITEM_ItemsAreEqual(oids[i], &clientauth->oid))
                eku |= PKINIT_EKU_CLIENTAUTH;
        }
        if (SECITEM_ItemsAreEqual(oids[i], &pkinit_kp_mssclogin))
            eku |= PKINIT_EKU_MSSCLOGIN;
    }
    PORT_FreeArena(pool, PR_TRUE);
    return eku;
}

krb5_error_code
crypto_cert_get_matching_data(krb5_context context,
                              pkinit_cert_handle cert_handle,
                              pkinit_cert_matching_data **ret_data)
{
    pkinit_cert_matching_data *md;

    md = malloc(sizeof(*md));
    if (md == NULL) {
        return ENOMEM;
    }
    md->ch = cert_handle;
    md->subject_dn = strdup(cert_handle->cert->subjectName);
    /* FIXME: string representation varies from OpenSSL's */
    md->issuer_dn = strdup(cert_handle->cert->issuerName);
    /* FIXME: string representation varies from OpenSSL's */
    md->ku_bits = cert_get_ku_bits(context, cert_handle->cert);
    md->eku_bits = cert_get_eku_bits(context, cert_handle->cert, PR_FALSE);
    if (cert_retrieve_cert_sans(context, cert_handle->cert,
                                &md->sans, &md->sans, NULL) != 0)
        md->sans = NULL;
    *ret_data = md;
    return 0;
}

/* Free up the data for this certificate. */
krb5_error_code
crypto_cert_release(krb5_context context, pkinit_cert_handle cert_handle)
{
    CERT_DestroyCertificate(cert_handle->cert);
    PORT_FreeArena(cert_handle->pool, PR_TRUE);
    return 0;
}

/* Free names, key usage, and extended key usage from the cert matching data
 * structure -- everything except the cert_handle it contains, anyway. */
krb5_error_code
crypto_cert_free_matching_data(krb5_context context,
                               pkinit_cert_matching_data *data)
{
    free(data->subject_dn);
    free(data->issuer_dn);
    free(data);
    return 0;
}

/* Mark the cert tracked in the matching data structure as the one we're going
 * to use. */
krb5_error_code
crypto_cert_select(krb5_context context, pkinit_cert_matching_data *data)
{
    CERTCertificate *cert;

    cert = CERT_DupCertificate(data->ch->cert);
    if (data->ch->id_cryptoctx->id_cert != NULL)
        CERT_DestroyCertificate(data->ch->id_cryptoctx->id_cert);
    data->ch->id_cryptoctx->id_cert = cert;
    crypto_update_signer_identity(context, data->ch->id_cryptoctx);
    return 0;
}

/* Try to select the "default" cert, which for now is the only cert, if we only
 * have one. */
krb5_error_code
crypto_cert_select_default(krb5_context context,
                           pkinit_plg_crypto_context plg_cryptoctx,
                           pkinit_req_crypto_context req_cryptoctx,
                           pkinit_identity_crypto_context id_cryptoctx)
{
    CERTCertListNode *node;
    CERTCertificate *cert;
    krb5_principal *sans;
    krb5_data *c;
    krb5_error_code code;
    int result, count, i;

    result = crypto_cert_get_count(context,
                                   plg_cryptoctx,
                                   req_cryptoctx, id_cryptoctx, &count);
    if (result != 0)
        return result;
    if (count == 1)
        /* use the only cert */
        cert = (CERT_LIST_HEAD(id_cryptoctx->id_certs))->cert;
    else {
        pkiDebug("%s: searching for a KDC certificate\n", __FUNCTION__);
        /* look for a cert that includes a TGS principal name */
        cert = NULL;
        for (node = CERT_LIST_HEAD(id_cryptoctx->id_certs);
             (node != NULL) &&
                 (node->cert != NULL) &&
                 !CERT_LIST_END(node, id_cryptoctx->id_certs);
             node = CERT_LIST_NEXT(node)) {
            sans = NULL;
            pkiDebug("%s: checking candidate certificate \"%s\"\n",
                     __FUNCTION__, node->cert->subjectName);
            code = cert_retrieve_cert_sans(context, node->cert,
                                           &sans, NULL, NULL);
            if ((code == 0) && (sans != NULL)) {
                for (i = 0; sans[i] != NULL; i++) {
                    c = krb5_princ_component(context, sans[i], 0);
                    if ((c->length == KRB5_TGS_NAME_SIZE) &&
                        (memcmp(c->data, KRB5_TGS_NAME,
                                KRB5_TGS_NAME_SIZE) == 0)) {
                        cert = node->cert;
                        pkiDebug("%s: selecting %s "
                                 "certificate \"%s\"\n",
                                 __FUNCTION__,
                                 KRB5_TGS_NAME, cert->subjectName);
                    }
                    krb5_free_principal(context, sans[i]);
                }
                free(sans);
                sans = NULL;
            }
            if (cert != NULL)
                break;
        }
        if (cert == NULL)
            return ENOENT;
    }
    if (id_cryptoctx->id_cert != NULL)
        CERT_DestroyCertificate(id_cryptoctx->id_cert);
    id_cryptoctx->id_cert = CERT_DupCertificate(cert);
    crypto_update_signer_identity(context, id_cryptoctx);
    return 0;
}

krb5_error_code
crypto_load_cas_and_crls(krb5_context context,
                         pkinit_plg_crypto_context plg_cryptoctx,
                         pkinit_req_crypto_context req_cryptoctx,
                         pkinit_identity_opts * idopts,
                         pkinit_identity_crypto_context id_cryptoctx,
                         int idtype, int catype, char *id)
{
    SECStatus status;
    PRBool cert_self, cert_mark_trusted, load_crl;

    /* Figure out what we're doing here. */
    switch (catype) {
    case CATYPE_ANCHORS:
        /* Screen out source types we can't use. */
        switch (idtype) {
        case IDTYPE_FILE:
        case IDTYPE_DIR:
        case IDTYPE_NSS:
            /* We only support these sources. */
            break;
        default:
            return EINVAL;
            break;
        }
        /* Mark certs we load as trusted roots. */
        cert_self = PR_FALSE;
        cert_mark_trusted = PR_TRUE;
        load_crl = PR_FALSE;
        break;
    case CATYPE_INTERMEDIATES:
        /* Screen out source types we can't use. */
        switch (idtype) {
        case IDTYPE_FILE:
        case IDTYPE_DIR:
        case IDTYPE_NSS:
            /* We only support these sources. */
            break;
        default:
            return EINVAL;
            break;
        }
        /* Hang on to certs as reference material. */
        cert_self = PR_FALSE;
        cert_mark_trusted = PR_FALSE;
        load_crl = PR_FALSE;
        break;
    case CATYPE_CRLS:
        /* Screen out source types we can't use. */
        switch (idtype) {
        case IDTYPE_FILE:
        case IDTYPE_DIR:
            /* We only support these sources. */
            break;
        default:
            return EINVAL;
            break;
        }
        /* No certs, just CRLs. */
        cert_self = PR_FALSE;
        cert_mark_trusted = PR_FALSE;
        load_crl = PR_TRUE;
        break;
    default:
        return ENOSYS;
        break;
    }

    switch (idtype) {
    case IDTYPE_FILE:
        status = crypto_load_files(context,
                                   plg_cryptoctx,
                                   req_cryptoctx,
                                   load_crl ? NULL : id,
                                   NULL,
                                   load_crl ? id : NULL,
                                   cert_self, cert_mark_trusted, id_cryptoctx);
        if (status != SECSuccess) {
            pkiDebug("%s: error loading file \"%s\"\n", __FUNCTION__, id);
            return ENOMEM;
        }
        return 0;
        break;
    case IDTYPE_NSS:
        status = crypto_load_nssdb(context,
                                   plg_cryptoctx,
                                   req_cryptoctx, id, id_cryptoctx);
        if (status != SECSuccess) {
            pkiDebug("%s: error loading NSS certdb \"%s\"\n",
                     __FUNCTION__, idopts->cert_filename);
            return ENOMEM;
        }
        return 0;
        break;
    case IDTYPE_DIR:
        status = crypto_load_dir(context,
                                 plg_cryptoctx,
                                 req_cryptoctx,
                                 id,
                                 cert_self, cert_mark_trusted, load_crl,
                                 id_cryptoctx);
        if (status != SECSuccess) {
            pkiDebug("%s: error loading directory \"%s\"\n", __FUNCTION__, id);
            return ENOMEM;
        }
        return 0;
        break;
    default:
        return EINVAL;
        break;
    }
}

/* Retrieve the client's copy of the KDC's certificate. */
krb5_error_code
pkinit_get_kdc_cert(krb5_context context,
                    pkinit_plg_crypto_context plg_cryptoctx,
                    pkinit_req_crypto_context req_cryptoctx,
                    pkinit_identity_crypto_context id_cryptoctx,
                    krb5_principal princ)
{
    /* Nothing to do. */
    return 0;
}

/* Create typed-data with sets of acceptable DH parameters. */
krb5_error_code
pkinit_create_td_dh_parameters(krb5_context context,
                               pkinit_plg_crypto_context plg_cryptoctx,
                               pkinit_req_crypto_context req_cryptoctx,
                               pkinit_identity_crypto_context id_cryptoctx,
                               pkinit_plg_opts *opts, krb5_pa_data ***pa_data)
{
    struct domain_parameters *params;
    SECItem tmp, *oid;
    krb5_algorithm_identifier id[sizeof(oakley_groups) /
                                 sizeof(oakley_groups[0])];
    krb5_algorithm_identifier *ids[(sizeof(id) / sizeof(id[0])) + 1];
    unsigned int i, j;
    krb5_data *data;
    krb5_pa_data **typed_data;
    krb5_error_code code;

    *pa_data = NULL;

    /* Fetch the algorithm OID. */
    oid = get_oid_from_tag(SEC_OID_X942_DIFFIE_HELMAN_KEY);
    if (oid == NULL)
        return ENOMEM;
    /* Walk the lists of parameters that we know. */
    for (i = 0, j = 0; i < sizeof(id) / sizeof(id[0]); i++) {
        if (oakley_groups[i].bits < opts->dh_min_bits)
            continue;
        /* Encode these parameters for use as algorithm parameters. */
        if (oakley_parse_group(req_cryptoctx->pool, &oakley_groups[i],
                               &params) != 0)
            continue;
        memset(&params, 0, sizeof(params));
        if (SEC_ASN1EncodeItem(req_cryptoctx->pool, &tmp,
                               params,
                               domain_parameters_template) != SECSuccess)
            continue;
        /* Add it to the list. */
        memset(&id[j], 0, sizeof(id[j]));
        id[j].algorithm.data = (char *)oid->data;
        id[j].algorithm.length = oid->len;
        id[j].parameters.data = (char *)tmp.data;
        id[j].parameters.length = tmp.len;
        ids[j] = &id[j];
        j++;
    }
    if (j == 0)
        return ENOENT;
    ids[j] = NULL;
    /* Pass it back up. */
    data = NULL;
    code = (*k5int_encode_krb5_td_dh_parameters)(ids, &data);
    if (code != 0)
        return code;
    typed_data = malloc(sizeof(*typed_data) * 2);
    if (typed_data == NULL) {
        krb5_free_data(context, data);
        return ENOMEM;
    }
    typed_data[0] = malloc(sizeof(**typed_data));
    if (typed_data[0] == NULL) {
        free(typed_data);
        krb5_free_data(context, data);
        return ENOMEM;
    }
    typed_data[0]->pa_type = TD_DH_PARAMETERS;
    typed_data[0]->length = data->length;
    typed_data[0]->contents = (unsigned char *) data->data;
    typed_data[1] = NULL;
    *pa_data = typed_data;
    free(data);
    return code;
}

/* Parse typed-data with sets of acceptable DH parameters and return the
 * minimum prime size that the KDC will accept. */
krb5_error_code
pkinit_process_td_dh_params(krb5_context context,
                            pkinit_plg_crypto_context plg_cryptoctx,
                            pkinit_req_crypto_context req_cryptoctx,
                            pkinit_identity_crypto_context id_cryptoctx,
                            krb5_algorithm_identifier **algId,
                            int *new_dh_size)
{
    struct domain_parameters params;
    SECItem item;
    int i, size;

    /* Set an initial reasonable guess if we got no hints that we could
     * parse. */
    *new_dh_size = 2048;
    for (i = 0; (algId != NULL) && (algId[i] != NULL); i++) {
        /* Decode the domain parameters. */
        item.len = algId[i]->parameters.length;
        item.data = (unsigned char *)algId[i]->parameters.data;
        memset(&params, 0, sizeof(params));
        if (SEC_ASN1DecodeItem(req_cryptoctx->pool, &params,
                               domain_parameters_template,
                               &item) != SECSuccess)
            continue;
        /* Count the size of the prime by finding the first non-zero
         * byte and working out the size of the integer. */
        size = get_integer_bits(&params.p);
        /* If this is the first parameter set, or the current parameter
         * size is lower than our previous guess, use it. */
        if ((i == 0) || (size < *new_dh_size))
            *new_dh_size = size;
    }
    return 0;
}

/* Create typed-data with the client cert that we didn't like. */
krb5_error_code
pkinit_create_td_invalid_certificate(krb5_context context,
                                     pkinit_plg_crypto_context plg_cryptoctx,
                                     pkinit_req_crypto_context req_cryptoctx,
                                     pkinit_identity_crypto_context
                                     id_cryptoctx, krb5_pa_data ***pa_data)
{
    CERTCertificate *invalid;
    krb5_external_principal_identifier id;
    krb5_external_principal_identifier *ids[2];
    struct issuer_and_serial_number isn;
    krb5_data *data;
    SECItem item;
    krb5_pa_data **typed_data;
    krb5_error_code code;

    *pa_data = NULL;

    /* We didn't trust the peer's certificate.  FIXME: or was it a
     * certificate that was somewhere in its certifying chain? */
    if (req_cryptoctx->peer_cert == NULL)
        return ENOENT;
    invalid = req_cryptoctx->peer_cert;

    /* Fill in the identifier. */
    memset(&id, 0, sizeof(id));
    if (req_cryptoctx->peer_cert->keyIDGenerated) {
        isn.issuer = invalid->derIssuer;
        isn.serial = invalid->serialNumber;
        if (SEC_ASN1EncodeItem(req_cryptoctx->pool, &item, &isn,
                               issuer_and_serial_number_template) != &item)
            return ENOMEM;
        id.issuerAndSerialNumber.data = (char *)item.data;
        id.issuerAndSerialNumber.length = item.len;
    } else {
        item = invalid->subjectKeyID;
        id.subjectKeyIdentifier.data = (char *)item.data;
        id.subjectKeyIdentifier.length = item.len;
    }
    ids[0] = &id;
    ids[1] = NULL;

    /* Pass it back up. */
    data = NULL;
    code = (*k5int_encode_krb5_td_trusted_certifiers)(ids, &data);
    if (code != 0)
        return code;
    typed_data = malloc(sizeof(*typed_data) * 2);
    if (typed_data == NULL) {
        krb5_free_data(context, data);
        return ENOMEM;
    }
    typed_data[0] = malloc(sizeof(**typed_data));
    if (typed_data[0] == NULL) {
        free(typed_data);
        krb5_free_data(context, data);
        return ENOMEM;
    }
    typed_data[0]->pa_type = TD_INVALID_CERTIFICATES;
    typed_data[0]->length = data->length;
    typed_data[0]->contents = (unsigned char *) data->data;
    typed_data[1] = NULL;
    *pa_data = typed_data;
    free(data);
    return code;
}

/* Create typed-data with a list of certifiers that we would accept. */
krb5_error_code
pkinit_create_td_trusted_certifiers(krb5_context context,
                                    pkinit_plg_crypto_context plg_cryptoctx,
                                    pkinit_req_crypto_context req_cryptoctx,
                                    pkinit_identity_crypto_context
                                    id_cryptoctx, krb5_pa_data ***pa_data)
{
    krb5_external_principal_identifier **ids;
    krb5_external_principal_identifier *id;
    struct issuer_and_serial_number isn;
    krb5_data *data;
    SECItem item;
    krb5_pa_data **typed_data;
    krb5_error_code code;
    int i;
    unsigned int trustf;
    SECStatus status;
    PK11SlotList *slist;
    PK11SlotListElement *sle;
    CERTCertificate *cert;
    CERTCertList *sclist, *clist;
    CERTCertListNode *node;

    *pa_data = NULL;

    /* Build the list of trusted roots. */
    clist = CERT_NewCertList();
    if (clist == NULL)
        return ENOMEM;

    /* Get the list of tokens.  All of them. */
    slist = PK11_GetAllTokens(CKM_INVALID_MECHANISM, PR_FALSE,
                              PR_FALSE,
                              crypto_pwcb_prep(id_cryptoctx, NULL, context));
    if (slist == NULL) {
        CERT_DestroyCertList(clist);
        return ENOENT;
    }

    /* Walk the list of tokens. */
    i = 0;
    status = SECSuccess;
    for (sle = slist->head; sle != NULL; sle = sle->next) {
        /* Skip over slots we would still need to log in to before using. */
        if (!PK11_IsLoggedIn(sle->slot,
                             crypto_pwcb_prep(id_cryptoctx, NULL, context)) &&
            PK11_NeedLogin(sle->slot)) {
            pkiDebug("%s: skipping token \"%s\"\n",
                     __FUNCTION__, PK11_GetTokenName(sle->slot));
            continue;
        }
        /* Get the list of certs, and skip the slot if it doesn't have
         * any. */
        sclist = PK11_ListCertsInSlot(sle->slot);
        if (sclist == NULL) {
            pkiDebug("%s: nothing found in token \"%s\"\n",
                     __FUNCTION__, PK11_GetTokenName(sle->slot));
            continue;
        }
        if (CERT_LIST_EMPTY(sclist)) {
            CERT_DestroyCertList(sclist);
            pkiDebug("%s: nothing found in token \"%s\"\n",
                     __FUNCTION__, PK11_GetTokenName(sle->slot));
            continue;
        }
        /* Walk the list of certs, and for each one that's a trusted
         * root, add it to the list. */
        for (node = CERT_LIST_HEAD(sclist);
             (node != NULL) &&
                 (node->cert != NULL) &&
                 !CERT_LIST_END(node, sclist);
             node = CERT_LIST_NEXT(node)) {
            /* If we have no trust for it, we can't trust it. */
            if (node->cert->trust == NULL)
                continue;
            /* We need to trust it to issue client certs. */
            trustf = SEC_GET_TRUST_FLAGS(node->cert->trust, trustSSL);
            if (!(trustf & CERTDB_TRUSTED_CLIENT_CA))
                continue;
            /* DestroyCertList frees all of the certs in the list,
             * so we need to create a copy that it can own. */
            cert = CERT_DupCertificate(node->cert);
            if (cert_maybe_add_to_list(clist, cert) != SECSuccess)
                status = ENOMEM;
            else
                i++;
        }
        CERT_DestroyCertList(sclist);
    }
    PK11_FreeSlotList(slist);
    if (status != SECSuccess) {
        CERT_DestroyCertList(clist);
        return ENOMEM;
    }

    /* Allocate some temporary storage. */
    id = PORT_ArenaZAlloc(req_cryptoctx->pool, sizeof(**ids) * i);
    ids = PORT_ArenaZAlloc(req_cryptoctx->pool, sizeof(*ids) * (i + 1));
    if ((id == NULL) || (ids == NULL)) {
        CERT_DestroyCertList(clist);
        return ENOMEM;
    }

    /* Fill in the identifiers. */
    i = 0;
    for (node = CERT_LIST_HEAD(clist);
         (node != NULL) &&
             (node->cert != NULL) &&
             !CERT_LIST_END(node, clist);
         node = CERT_LIST_NEXT(node)) {
        if (node->cert->keyIDGenerated) {
            isn.issuer = node->cert->derIssuer;
            isn.serial = node->cert->serialNumber;
            if (SEC_ASN1EncodeItem(req_cryptoctx->pool, &item, &isn,
                                   issuer_and_serial_number_template) !=
                &item) {
                CERT_DestroyCertList(clist);
                return ENOMEM;
            }
            id[i].issuerAndSerialNumber.data = (char *)item.data;
            id[i].issuerAndSerialNumber.length = item.len;
        } else {
            item = node->cert->subjectKeyID;
            id[i].subjectKeyIdentifier.data = (char *)item.data;
            id[i].subjectKeyIdentifier.length = item.len;
        }
        ids[i] = &id[i];
        i++;
    }
    ids[i] = NULL;

    /* Pass the list back up. */
    data = NULL;
    code = (*k5int_encode_krb5_td_trusted_certifiers)(ids, &data);
    CERT_DestroyCertList(clist);
    if (code != 0)
        return code;
    typed_data = malloc(sizeof(*typed_data) * 2);
    if (typed_data == NULL) {
        krb5_free_data(context, data);
        return ENOMEM;
    }
    typed_data[0] = malloc(sizeof(**typed_data));
    if (typed_data[0] == NULL) {
        free(typed_data);
        krb5_free_data(context, data);
        return ENOMEM;
    }
    typed_data[0]->pa_type = TD_TRUSTED_CERTIFIERS;
    typed_data[0]->length = data->length;
    typed_data[0]->contents = (unsigned char *) data->data;
    typed_data[1] = NULL;
    *pa_data = typed_data;
    free(data);
    return code;
}

krb5_error_code
pkinit_process_td_trusted_certifiers(krb5_context context,
                                     pkinit_plg_crypto_context plg_cryptoctx,
                                     pkinit_req_crypto_context req_cryptoctx,
                                     pkinit_identity_crypto_context
                                     id_cryptoctx,
                                     krb5_external_principal_identifier **
                                     trustedCertifiers,
                                     int td_type)
{
    /* We should select a different client certificate based on the list of
     * trusted certifiers, but for now we'll just chicken out. */
    return KRB5KDC_ERR_PREAUTH_FAILED;
}

/* Check if the encoded issuer/serial matches our (the KDC's) certificate. */
krb5_error_code
pkinit_check_kdc_pkid(krb5_context context,
                      pkinit_plg_crypto_context plg_cryptoctx,
                      pkinit_req_crypto_context req_cryptoctx,
                      pkinit_identity_crypto_context id_cryptoctx,
                      unsigned char *pkid_buf,
                      unsigned int pkid_len, int *valid_kdcPkId)
{
    PLArenaPool *pool;
    CERTCertificate *cert;
    SECItem pkid;
    struct issuer_and_serial_number isn;

    pool = PORT_NewArena(sizeof(double));
    if (pool == NULL)
        return ENOMEM;

    /* Verify that we have selected a certificate for our (the KDC's) own
     * use. */
    if (id_cryptoctx->id_cert == NULL)
        return ENOENT;
    cert = id_cryptoctx->id_cert;

    /* Decode the pair. */
    pkid.data = pkid_buf;
    pkid.len = pkid_len;
    memset(&isn, 0, sizeof(isn));
    if (SEC_ASN1DecodeItem(pool, &isn, issuer_and_serial_number_template,
                           &pkid) != SECSuccess) {
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }

    /* Compare the issuer and serial number. */
    *valid_kdcPkId = SECITEM_ItemsAreEqual(&isn.issuer,
                                           &cert->derIssuer) &&
        SECITEM_ItemsAreEqual(&isn.serial, &cert->serialNumber);

    /* Clean up. */
    PORT_FreeArena(pool, PR_TRUE);

    return 0;
}

krb5_error_code
pkinit_identity_set_prompter(pkinit_identity_crypto_context id_cryptoctx,
                             krb5_prompter_fct prompter, void *prompter_data)
{
    id_cryptoctx->pwcb_args.prompter = prompter;
    id_cryptoctx->pwcb_args.prompter_data = prompter_data;
    return 0;
}

/* Convert a DH secret and optional data to a keyblock using the specified
 * digest and a big-endian counter of the specified length that starts at the
 * specified value. */
static krb5_error_code
pkinit_octetstring_hkdf(krb5_context context,
                        SECOidTag hash_alg,
                        int counter_start, size_t counter_length,
                        krb5_enctype etype,
                        unsigned char *dh_key, unsigned int dh_key_len,
                        char *other_data, unsigned int other_data_len,
                        krb5_keyblock *krb5key)
{
    PK11Context *ctx;
    unsigned int left, length, rnd_len;
    unsigned char counter[8], buf[512];  /* the longest digest we support */
    int i;
    char *rnd_buf;
    size_t kbyte, klength;
    krb5_data rnd_data;
    krb5_error_code result;
    NSSInitContext *ncontext;

    if (counter_length > sizeof(counter))
        return EINVAL;
    result = krb5_c_keylengths(context, etype, &kbyte, &klength);
    if (result != 0)
        return result;
    rnd_buf = malloc(dh_key_len);
    if (rnd_buf == NULL)
        return ENOMEM;

    memset(counter, 0, sizeof(counter));
    for (i = sizeof(counter) - 1; i >= 0; i--)
        counter[i] = (counter_start >> (8 * (counter_length - 1 - i))) & 0xff;
    rnd_len = kbyte;
    left = rnd_len;
    ncontext = NSS_InitContext(DEFAULT_CONFIGDIR,
                               NULL,
                               NULL,
                               NULL,
                               NULL,
                               NSS_INIT_READONLY |
                               NSS_INIT_NOCERTDB |
                               NSS_INIT_NOMODDB |
                               NSS_INIT_FORCEOPEN |
                               NSS_INIT_NOROOTINIT |
                               NSS_INIT_PK11RELOAD);
    while (left > 0) {
        ctx = PK11_CreateDigestContext(hash_alg);
        if (ctx == NULL) {
            krb5int_zap(buf, sizeof(buf));
            krb5int_zap(rnd_buf, dh_key_len);
            free(rnd_buf);
            return ENOMEM;
        }
        if (PK11_DigestBegin(ctx) != SECSuccess) {
            PK11_DestroyContext(ctx, PR_TRUE);
            krb5int_zap(buf, sizeof(buf));
            krb5int_zap(rnd_buf, dh_key_len);
            free(rnd_buf);
            return ENOMEM;
        }
        if (PK11_DigestOp(ctx, counter, counter_length) != SECSuccess) {
            PK11_DestroyContext(ctx, PR_TRUE);
            krb5int_zap(buf, sizeof(buf));
            krb5int_zap(rnd_buf, dh_key_len);
            free(rnd_buf);
            return ENOMEM;
        }
        if (PK11_DigestOp(ctx, dh_key, dh_key_len) != SECSuccess) {
            PK11_DestroyContext(ctx, PR_TRUE);
            krb5int_zap(buf, sizeof(buf));
            krb5int_zap(rnd_buf, dh_key_len);
            free(rnd_buf);
            return ENOMEM;
        }
        if ((other_data_len > 0) &&
            (PK11_DigestOp(ctx, (const unsigned char *) other_data,
                           other_data_len) != SECSuccess)) {
            PK11_DestroyContext(ctx, PR_TRUE);
            krb5int_zap(buf, sizeof(buf));
            krb5int_zap(rnd_buf, dh_key_len);
            free(rnd_buf);
            return ENOMEM;
        }
        if (PK11_DigestFinal(ctx, buf, &length, sizeof(buf)) != SECSuccess) {
            PK11_DestroyContext(ctx, PR_TRUE);
            krb5int_zap(buf, sizeof(buf));
            krb5int_zap(rnd_buf, dh_key_len);
            free(rnd_buf);
            return ENOMEM;
        }
        PK11_DestroyContext(ctx, PR_TRUE);
        if (left < length) {
            length = left;
        }
        memcpy(rnd_buf + rnd_len - left, buf, length);
        left -= length;
        for (i = counter_length - 1; i >= 0; i--) {
            counter[i] = ((counter[i] + 1) & 0xff);
            if (counter[i] != 0)
                break;
        }
    }

    if (NSS_ShutdownContext(ncontext) != SECSuccess)
        pkiDebug("%s: error shutting down context\n", __FUNCTION__);

    krb5key->contents = malloc(klength);
    if (krb5key->contents == NULL) {
        krb5key->length = 0;
        return ENOMEM;
    }
    krb5key->length = klength;
    krb5key->enctype = etype;

    rnd_data.data = rnd_buf;
    rnd_data.length = rnd_len;
    result = krb5_c_random_to_key(context, etype, &rnd_data, krb5key);

    krb5int_zap(buf, sizeof(buf));
    krb5int_zap(rnd_buf, dh_key_len);
    free(rnd_buf);

    return result;
}

/* Convert a DH secret to a keyblock, RFC4556-style. */
krb5_error_code
pkinit_octetstring2key(krb5_context context,
                       krb5_enctype etype,
                       unsigned char *dh_key,
                       unsigned int dh_key_len, krb5_keyblock *krb5key)
{
    return pkinit_octetstring_hkdf(context,
                                   SEC_OID_SHA1, 0, 1, etype,
                                   dh_key, dh_key_len, NULL, 0,
                                   krb5key);
}

/* Return TRUE if the item and the "algorithm" part of the algorithm identifier
 * are the same. */
static PRBool
data_and_ptr_and_length_equal(const krb5_data *data,
                              const void *ptr, size_t len)
{
    return (data->length == len) && (memcmp(data->data, ptr, len) == 0);
}

/* Encode the other info used by the agility KDF.  Taken almost verbatim from
 * parts of the agility KDF in pkinit_crypto_openssl.c */
static krb5_error_code
encode_agility_kdf_other_info(krb5_context context,
                              krb5_data *alg_oid,
                              krb5_const_principal party_u_info,
                              krb5_const_principal party_v_info,
                              krb5_enctype enctype,
                              krb5_data *as_req,
                              krb5_data *pk_as_rep,
                              krb5_data **other_info)
{
    krb5_error_code retval = 0;
    krb5_sp80056a_other_info other_info_fields;
    krb5_pkinit_supp_pub_info supp_pub_info_fields;
    krb5_data *supp_pub_info = NULL;
    krb5_algorithm_identifier alg_id;

    /* If this is anonymous pkinit, we need to use the anonymous principal for
     * party_u_info */
    if (party_u_info &&
        krb5_principal_compare_any_realm(context, party_u_info,
                                         krb5_anonymous_principal()))
        party_u_info = krb5_anonymous_principal();

    /* Encode the ASN.1 octet string for "SuppPubInfo" */
    supp_pub_info_fields.enctype = enctype;
    supp_pub_info_fields.as_req = *as_req;
    supp_pub_info_fields.pk_as_rep = *pk_as_rep;
    retval = encode_krb5_pkinit_supp_pub_info(&supp_pub_info_fields,
                                              &supp_pub_info);
    if (retval != 0)
        goto cleanup;

    /* Now encode the ASN.1 octet string for "OtherInfo" */
    memset(&alg_id, 0, sizeof alg_id);
    alg_id.algorithm = *alg_oid; /*alias, don't have to free it*/

    other_info_fields.algorithm_identifier = alg_id;
    other_info_fields.party_u_info = (krb5_principal) party_u_info;
    other_info_fields.party_v_info = (krb5_principal) party_v_info;
    other_info_fields.supp_pub_info = *supp_pub_info;
    retval = encode_krb5_sp80056a_other_info(&other_info_fields, other_info);
    if (retval != 0)
        goto cleanup;

cleanup:
    krb5_free_data(context, supp_pub_info);

    return retval;
}

/* Convert a DH secret to a keyblock using the key derivation function
 * identified by the passed-in algorithm identifier.  Return ENOSYS if it's not
 * one that we support. */
krb5_error_code
pkinit_alg_agility_kdf(krb5_context context,
                       krb5_data *secret,
                       krb5_data *alg_oid,
                       krb5_const_principal party_u_info,
                       krb5_const_principal party_v_info,
                       krb5_enctype enctype,
                       krb5_data *as_req,
                       krb5_data *pk_as_rep,
                       krb5_keyblock *key_block)
{
    krb5_data *other_info = NULL;
    krb5_error_code retval = ENOSYS;

    retval = encode_agility_kdf_other_info(context,
                                           alg_oid,
                                           party_u_info,
                                           party_v_info,
                                           enctype, as_req, pk_as_rep,
                                           &other_info);
    if (retval != 0)
        return retval;

    if (data_and_ptr_and_length_equal(alg_oid, krb5_pkinit_sha512_oid,
                                      krb5_pkinit_sha512_oid_len))
        retval = pkinit_octetstring_hkdf(context,
                                         SEC_OID_SHA512, 1, 4, enctype,
                                         (unsigned char *)secret->data,
                                         secret->length, other_info->data,
                                         other_info->length, key_block);
    else if (data_and_ptr_and_length_equal(alg_oid, krb5_pkinit_sha256_oid,
                                           krb5_pkinit_sha256_oid_len))
        retval = pkinit_octetstring_hkdf(context,
                                         SEC_OID_SHA256, 1, 4, enctype,
                                         (unsigned char *)secret->data,
                                         secret->length, other_info->data,
                                         other_info->length, key_block);
    else if (data_and_ptr_and_length_equal(alg_oid, krb5_pkinit_sha1_oid,
                                           krb5_pkinit_sha1_oid_len))
        retval = pkinit_octetstring_hkdf(context,
                                         SEC_OID_SHA1, 1, 4, enctype,
                                         (unsigned char *)secret->data,
                                         secret->length, other_info->data,
                                         other_info->length, key_block);
    else
        retval = KRB5KDC_ERR_NO_ACCEPTABLE_KDF;

    krb5_free_data(context, other_info);

    return retval;
}

static int
cert_add_string(unsigned char ***list, int *count,
                int len, const unsigned char *value)
{
    unsigned char **tmp;

    tmp = malloc(sizeof(tmp[0]) * (*count + 2));
    if (tmp == NULL) {
        return ENOMEM;
    }
    memcpy(tmp, *list, *count * sizeof(tmp[0]));
    tmp[*count] = malloc(len + 1);
    if (tmp[*count] == NULL) {
        free(tmp);
        return ENOMEM;
    }
    memcpy(tmp[*count], value, len);
    tmp[*count][len] = '\0';
    tmp[*count + 1] = NULL;
    if (*count != 0) {
        free(*list);
    }
    *list = tmp;
    (*count)++;
    return 0;
}

static int
cert_add_princ(krb5_context context, krb5_principal princ,
               krb5_principal **sans_inout, int *n_sans_inout)
{
    krb5_principal *tmp;

    tmp = malloc(sizeof(krb5_principal *) * (*n_sans_inout + 2));
    if (tmp == NULL) {
        return ENOMEM;
    }
    memcpy(tmp, *sans_inout, sizeof(tmp[0]) * *n_sans_inout);
    if (krb5_copy_principal(context, princ, &tmp[*n_sans_inout]) != 0) {
        free(tmp);
        return ENOMEM;
    }
    tmp[*n_sans_inout + 1] = NULL;
    if (*n_sans_inout > 0) {
        free(*sans_inout);
    }
    *sans_inout = tmp;
    (*n_sans_inout)++;
    return 0;
}

static int
cert_add_upn(PLArenaPool * pool, krb5_context context, SECItem *name,
             krb5_principal **sans_inout, int *n_sans_inout)
{
    SECItem decoded;
    char *unparsed;
    krb5_principal tmp;
    int i;

    /* Decode the string. */
    memset(&decoded, 0, sizeof(decoded));
    if (SEC_ASN1DecodeItem(pool, &decoded,
                           SEC_ASN1_GET(SEC_UTF8StringTemplate),
                           name) != SECSuccess) {
        return ENOMEM;
    }
    unparsed = malloc(decoded.len + 1);
    if (unparsed == NULL) {
        return ENOMEM;
    }
    memcpy(unparsed, decoded.data, decoded.len);
    unparsed[decoded.len] = '\0';
    /* Parse the string into a principal name. */
    if (krb5_parse_name(context, unparsed, &tmp) != 0) {
        free(unparsed);
        return ENOMEM;
    }
    free(unparsed);
    /* Unparse the name back into a string and make sure it matches what
     * was in the certificate. */
    if (krb5_unparse_name(context, tmp, &unparsed) != 0) {
        krb5_free_principal(context, tmp);
        return ENOMEM;
    }
    if ((strlen(unparsed) != decoded.len) ||
        (memcmp(unparsed, decoded.data, decoded.len) != 0)) {
        krb5_free_unparsed_name(context, unparsed);
        krb5_free_principal(context, tmp);
        return ENOMEM;
    }
    /* Add the principal name to the list. */
    i = cert_add_princ(context, tmp, sans_inout, n_sans_inout);
    krb5_free_unparsed_name(context, unparsed);
    krb5_free_principal(context, tmp);
    return i;
}

static int
cert_add_kpn(PLArenaPool * pool, krb5_context context, SECItem *name,
             krb5_principal** sans_inout, int *n_sans_inout)
{
    struct kerberos_principal_name kname;
    SECItem **names;
    krb5_data *comps;
    krb5_principal_data tmp;
    unsigned long name_type;
    int i, j;

    /* Decode the structure. */
    memset(&kname, 0, sizeof(kname));
    if (SEC_ASN1DecodeItem(pool, &kname,
                           kerberos_principal_name_template,
                           name) != SECSuccess)
        return ENOMEM;

    /* Recover the name type and count the components. */
    if (SEC_ASN1DecodeInteger(&kname.principal_name.name_type,
                              &name_type) != SECSuccess)
        return ENOMEM;
    names = kname.principal_name.name_string;
    for (i = 0; (names != NULL) && (names[i] != NULL); i++)
        continue;
    comps = malloc(sizeof(comps[0]) * i);

    /* Fake up a principal structure. */
    for (j = 0; j < i; j++) {
        comps[j].length = names[j]->len;
        comps[j].data = (char *) names[j]->data;
    }
    memset(&tmp, 0, sizeof(tmp));
    tmp.type = name_type;
    tmp.realm.length = kname.realm.len;
    tmp.realm.data = (char *) kname.realm.data;
    tmp.length = i;
    tmp.data = comps;

    /* Add the principal name to the list. */
    i = cert_add_princ(context, &tmp, sans_inout, n_sans_inout);
    free(comps);
    return i;
}

static const char *
crypto_get_identity_by_slot(krb5_context context,
                            pkinit_identity_crypto_context id_cryptoctx,
                            PK11SlotInfo *slot)
{
    PK11SlotInfo *mslot;
    struct _pkinit_identity_crypto_userdb *userdb;
    struct _pkinit_identity_crypto_module *module;
    int i, j;

    mslot = id_cryptoctx->id_p12_slot.slot;
    if ((mslot != NULL) && (PK11_GetSlotID(mslot) == PK11_GetSlotID(slot)))
        return id_cryptoctx->id_p12_slot.p12name;
    for (i = 0;
         (id_cryptoctx->id_userdbs != NULL) &&
         (id_cryptoctx->id_userdbs[i] != NULL);
         i++) {
        userdb = id_cryptoctx->id_userdbs[i];
        if (PK11_GetSlotID(userdb->userdb) == PK11_GetSlotID(slot))
            return userdb->name;
    }
    for (i = 0;
         (id_cryptoctx->id_modules != NULL) &&
         (id_cryptoctx->id_modules[i] != NULL);
         i++) {
        module = id_cryptoctx->id_modules[i];
        for (j = 0; j < module->module->slotCount; j++) {
            mslot = module->module->slots[j];
            if (PK11_GetSlotID(mslot) == PK11_GetSlotID(slot))
                return module->name;
        }
    }
    return NULL;
}

static void
crypto_update_signer_identity(krb5_context context,
                              pkinit_identity_crypto_context id_cryptoctx)
{
    PK11SlotList *slist;
    PK11SlotListElement *sle;
    CERTCertificate *cert;
    struct _pkinit_identity_crypto_file *obj;
    int i;

    id_cryptoctx->identity = NULL;
    if (id_cryptoctx->id_cert == NULL)
        return;
    cert = id_cryptoctx->id_cert;
    for (i = 0;
         (id_cryptoctx->id_objects != NULL) &&
         (id_cryptoctx->id_objects[i] != NULL);
         i++) {
        obj = id_cryptoctx->id_objects[i];
        if ((obj->cert != NULL) && CERT_CompareCerts(obj->cert, cert)) {
            id_cryptoctx->identity = obj->name;
            return;
        }
    }
    if (cert->slot != NULL) {
        id_cryptoctx->identity = crypto_get_identity_by_slot(context,
                                                             id_cryptoctx,
                                                             cert->slot);
        if (id_cryptoctx->identity != NULL)
            return;
    }
    slist = PK11_GetAllSlotsForCert(cert, NULL);
    if (slist != NULL) {
        for (sle = PK11_GetFirstSafe(slist);
             sle != NULL;
             sle = PK11_GetNextSafe(slist, sle, PR_FALSE)) {
            id_cryptoctx->identity = crypto_get_identity_by_slot(context,
                                                                 id_cryptoctx,
                                                                 sle->slot);
            if (id_cryptoctx->identity != NULL) {
                PK11_FreeSlotList(slist);
                return;
            }
        }
        PK11_FreeSlotList(slist);
    }
}

krb5_error_code
crypto_retrieve_signer_identity(krb5_context context,
                                pkinit_identity_crypto_context id_cryptoctx,
                                const char **identity)
{
    *identity = id_cryptoctx->identity;
    if (*identity == NULL)
        return ENOENT;
    return 0;
}

static krb5_error_code
cert_retrieve_cert_sans(krb5_context context,
                        CERTCertificate *cert,
                        krb5_principal **pkinit_sans_out,
                        krb5_principal **upn_sans_out,
                        unsigned char ***kdc_hostname_out)
{
    PLArenaPool *pool;
    CERTGeneralName name;
    SECItem *ext, **encoded_names;
    int i, n_pkinit_sans, n_upn_sans, n_hostnames;

    /* Pull out the extension. */
    ext = cert_get_ext_by_tag(cert, SEC_OID_X509_SUBJECT_ALT_NAME);
    if (ext == NULL)
        return ENOENT;

    /* Split up the list of names. */
    pool = PORT_NewArena(sizeof(double));
    if (pool == NULL)
        return ENOMEM;
    encoded_names = NULL;
    if (SEC_ASN1DecodeItem(pool, &encoded_names,
                           SEC_ASN1_GET(SEC_SequenceOfAnyTemplate),
                           ext) != SECSuccess) {
        pkiDebug("%s: error decoding subjectAltName extension\n",
                 __FUNCTION__);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }

    /* Check each name in turn. */
    for (i = 0, n_pkinit_sans = 0, n_upn_sans = 0, n_hostnames = 0;
         (encoded_names != NULL) && (encoded_names[i] != NULL);
         i++) {
        memset(&name, 0, sizeof(name));
        if (CERT_DecodeGeneralName(pool, encoded_names[i], &name) != &name) {
            pkiDebug("%s: error decoding GeneralName value, skipping\n",
                     __FUNCTION__);
            continue;
        }
        switch (name.type) {
        case certDNSName:
            /* hostname, easy */
            if ((kdc_hostname_out != NULL) &&
                (cert_add_string(kdc_hostname_out, &n_hostnames,
                                 name.name.other.len,
                                 name.name.other.data) != 0)) {
                PORT_FreeArena(pool, PR_TRUE);
                return ENOMEM;
            }
            break;
        case certOtherName:
            /* possibly a kerberos principal name */
            if (SECITEM_ItemsAreEqual(&name.name.OthName.oid,
                                      &pkinit_nt_principal)) {
                /* Add it to the list. */
                if ((pkinit_sans_out != NULL) &&
                    (cert_add_kpn(pool, context, &name.name.OthName.name,
                                  pkinit_sans_out, &n_pkinit_sans) != 0)) {
                    PORT_FreeArena(pool, PR_TRUE);
                    return ENOMEM;
                }
                /* If both lists are the same, fix the count. */
                if (pkinit_sans_out == upn_sans_out)
                    n_upn_sans = n_pkinit_sans;
            } else
                /* possibly a user principal name */
                if (SECITEM_ItemsAreEqual(&name.name.OthName.oid,
                                          &pkinit_nt_upn)) {
                    /* Add it to the list. */
                    if ((upn_sans_out != NULL) &&
                        (cert_add_upn(pool, context, &name.name.OthName.name,
                                      upn_sans_out, &n_upn_sans) != 0)) {
                        PORT_FreeArena(pool, PR_TRUE);
                        return ENOMEM;
                    }
                    /* If both lists are the same, fix the count. */
                    if (upn_sans_out == pkinit_sans_out)
                        n_pkinit_sans = n_upn_sans;
                }
            break;
        default:
            break;
        }
    }
    PORT_FreeArena(pool, PR_TRUE);

    return 0;
}

krb5_error_code
crypto_retrieve_cert_sans(krb5_context context,
                          pkinit_plg_crypto_context plg_cryptoctx,
                          pkinit_req_crypto_context req_cryptoctx,
                          pkinit_identity_crypto_context id_cryptoctx,
                          krb5_principal **pkinit_sans,
                          krb5_principal **upn_sans,
                          unsigned char ***kdc_hostname)
{
    return cert_retrieve_cert_sans(context,
                                   req_cryptoctx->peer_cert,
                                   pkinit_sans, upn_sans, kdc_hostname);
}

krb5_error_code
crypto_check_cert_eku(krb5_context context,
                      pkinit_plg_crypto_context plg_cryptoctx,
                      pkinit_req_crypto_context req_cryptoctx,
                      pkinit_identity_crypto_context id_cryptoctx,
                      int checking_kdc_cert,
                      int allow_secondary_usage, int *eku_valid)
{
    int ku, eku;

    *eku_valid = 0;

    ku = cert_get_ku_bits(context, req_cryptoctx->peer_cert);
    if (!(ku & PKINIT_KU_DIGITALSIGNATURE)) {
        return 0;
    }

    eku = cert_get_eku_bits(context, req_cryptoctx->peer_cert,
                            checking_kdc_cert ? PR_TRUE : PR_FALSE);
    if (checking_kdc_cert) {
        if (eku & PKINIT_EKU_PKINIT) {
            *eku_valid = 1;
        } else if (allow_secondary_usage && (eku & PKINIT_EKU_CLIENTAUTH)) {
            *eku_valid = 1;
        }
    } else {
        if (eku & PKINIT_EKU_PKINIT) {
            *eku_valid = 1;
        } else if (allow_secondary_usage && (eku & PKINIT_EKU_MSSCLOGIN)) {
            *eku_valid = 1;
        }
    }
    return 0;
}

krb5_error_code
cms_contentinfo_create(krb5_context context,
                       pkinit_plg_crypto_context plg_cryptoctx,
                       pkinit_req_crypto_context req_cryptoctx,
                       pkinit_identity_crypto_context id_cryptoctx,
                       int cms_msg_type,
                       unsigned char *in_data, unsigned int in_length,
                       unsigned char **out_data, unsigned int *out_data_len)
{
    PLArenaPool *pool;
    SECItem *oid, encoded;
    SECOidTag encapsulated_tag;
    struct content_info cinfo;

    switch (cms_msg_type) {
    case CMS_SIGN_DRAFT9:
        encapsulated_tag = get_pkinit_data_auth_data9_tag();
        break;
    case CMS_SIGN_CLIENT:
        encapsulated_tag = get_pkinit_data_auth_data_tag();
        break;
    case CMS_SIGN_SERVER:
        encapsulated_tag = get_pkinit_data_dhkey_data_tag();
        break;
    case CMS_ENVEL_SERVER:
        encapsulated_tag = get_pkinit_data_rkey_data_tag();
        break;
    default:
        return ENOSYS;
        break;
    }

    oid = get_oid_from_tag(encapsulated_tag);
    if (oid == NULL) {
        return ENOMEM;
    }

    pool = PORT_NewArena(sizeof(double));
    if (pool == NULL) {
        return ENOMEM;
    }

    memset(&cinfo, 0, sizeof(cinfo));
    cinfo.content_type = *oid;
    cinfo.content.data = in_data;
    cinfo.content.len = in_length;

    memset(&encoded, 0, sizeof(encoded));
    if (SEC_ASN1EncodeItem(pool, &encoded, &cinfo,
                           content_info_template) != &encoded) {
        PORT_FreeArena(pool, PR_TRUE);
        pkiDebug("%s: error encoding data\n", __FUNCTION__);
        return ENOMEM;
    }

    if (secitem_to_buf_len(&encoded, out_data, out_data_len) != 0) {
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
#ifdef DEBUG_DER
    derdump(*out_data, *out_data_len);
#endif
#ifdef DEBUG_CMS
    cmsdump(*out_data, *out_data_len);
#endif

    PORT_FreeArena(pool, PR_TRUE);

    return 0;
}

/* Create a signed-data content info, add a signature to it, and return it. */
enum sdcc_include_certchain {
    signeddata_common_create_omit_chain,
    signeddata_common_create_with_chain
};
enum sdcc_include_signed_attrs {
    signeddata_common_create_omit_signed_attrs,
    signeddata_common_create_with_signed_attrs
};
static krb5_error_code
crypto_signeddata_common_create(krb5_context context,
                                pkinit_plg_crypto_context plg_cryptoctx,
                                pkinit_req_crypto_context req_cryptoctx,
                                pkinit_identity_crypto_context id_cryptoctx,
                                NSSCMSMessage *msg,
                                SECOidTag digest,
                                enum sdcc_include_certchain certchain_mode,
                                enum sdcc_include_signed_attrs add_signedattrs,
                                NSSCMSSignedData **signed_data_out)
{
    NSSCMSSignedData *sdata;
    NSSCMSSignerInfo *signer;
    NSSCMSCertChainMode chainmode;

    /* Create a signed-data object. */
    sdata = NSS_CMSSignedData_Create(msg);
    if (sdata == NULL)
        return ENOMEM;

    if (id_cryptoctx->id_cert != NULL) {
        /* Create a signer and add it to the signed-data pointer. */
        signer = NSS_CMSSignerInfo_Create(msg, id_cryptoctx->id_cert, digest);
        if (signer == NULL)
            return ENOMEM;
        chainmode = (certchain_mode == signeddata_common_create_with_chain) ?
                    NSSCMSCM_CertChain :
                    NSSCMSCM_CertOnly;
        if (NSS_CMSSignerInfo_IncludeCerts(signer,
                                           chainmode,
                                           certUsageAnyCA) != SECSuccess) {
            pkiDebug("%s: error setting IncludeCerts\n", __FUNCTION__);
            return ENOMEM;
        }
        if (NSS_CMSSignedData_AddSignerInfo(sdata, signer) != SECSuccess)
            return ENOMEM;

        if (add_signedattrs == signeddata_common_create_with_signed_attrs) {
            /* The presence of any signed attribute means the digest
             * becomes a signed attribute, too. */
            if (NSS_CMSSignerInfo_AddSigningTime(signer,
                                                 PR_Now()) != SECSuccess) {
                pkiDebug("%s: error adding signing time\n", __FUNCTION__);
                return ENOMEM;
            }
        }
    }

    *signed_data_out = sdata;
    return 0;
}

/* Create signed-then-enveloped data. */
krb5_error_code
cms_envelopeddata_create(krb5_context context,
                         pkinit_plg_crypto_context plg_cryptoctx,
                         pkinit_req_crypto_context req_cryptoctx,
                         pkinit_identity_crypto_context id_cryptoctx,
                         krb5_preauthtype pa_type,
                         int include_certchain,
                         unsigned char *key_pack,
                         unsigned int key_pack_len,
                         unsigned char **envel_data,
                         unsigned int *envel_data_len)
{
    NSSCMSMessage *msg;
    NSSCMSContentInfo *info;
    NSSCMSEnvelopedData *env;
    NSSCMSRecipientInfo *recipient;
    NSSCMSSignedData *sdata;
    PLArenaPool *pool;
    SECOidTag encapsulated_tag, digest;
    SECItem plain, encoded;
    enum sdcc_include_signed_attrs add_signed_attrs;

    switch (pa_type) {
    case KRB5_PADATA_PK_AS_REQ_OLD:
    case KRB5_PADATA_PK_AS_REP_OLD:
        digest = SEC_OID_MD5;
        add_signed_attrs = signeddata_common_create_omit_signed_attrs;
        encapsulated_tag = get_pkinit_data_rkey_data_tag();
        break;
    case KRB5_PADATA_PK_AS_REQ:
    case KRB5_PADATA_PK_AS_REP:
        digest = SEC_OID_SHA1;
        add_signed_attrs = signeddata_common_create_with_signed_attrs;
        encapsulated_tag = get_pkinit_data_rkey_data_tag();
        break;
    default:
        return ENOSYS;
        break;
    }

    if (id_cryptoctx->id_cert == NULL) {
        pkiDebug("%s: no signer identity\n", __FUNCTION__);
        return ENOENT;
    }

    if (req_cryptoctx->peer_cert == NULL) {
        pkiDebug("%s: no recipient identity\n", __FUNCTION__);
        return ENOENT;
    }

    pool = PORT_NewArena(sizeof(double));
    if (pool == NULL) {
        return ENOMEM;
    }

    /* Create the containing message. */
    msg = NSS_CMSMessage_Create(pool);
    if (msg == NULL) {
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }

    /* Create an enveloped-data pointer and set it as the message's
     * contents. */
    env = NSS_CMSEnvelopedData_Create(msg, SEC_OID_DES_EDE3_CBC, 0);
    if (env == NULL) {
        pkiDebug("%s: error creating enveloped-data\n", __FUNCTION__);
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
    info = NSS_CMSMessage_GetContentInfo(msg);
    if (info == NULL) {
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
    if (NSS_CMSContentInfo_SetContent_EnvelopedData(msg, info,
                                                    env) != SECSuccess) {
        pkiDebug("%s: error setting enveloped-data content\n", __FUNCTION__);
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }

    /* Create a recipient and add it to the enveloped-data pointer. */
    recipient = NSS_CMSRecipientInfo_Create(msg, req_cryptoctx->peer_cert);
    if (recipient == NULL) {
        pkiDebug("%s: error creating recipient-info\n", __FUNCTION__);
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
    if (NSS_CMSEnvelopedData_AddRecipient(env, recipient) != SECSuccess) {
        pkiDebug("%s: error adding recipient\n", __FUNCTION__);
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }

    /* Create a signed-data pointer and set it as the enveloped-data's
     * contents. */
    info = NSS_CMSEnvelopedData_GetContentInfo(env);
    if (info == NULL) {
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
    sdata = NULL;
    if ((crypto_signeddata_common_create(context,
                                         plg_cryptoctx,
                                         req_cryptoctx,
                                         id_cryptoctx,
                                         msg,
                                         digest,
                                         include_certchain ?
                                         signeddata_common_create_with_chain :
                                         signeddata_common_create_omit_chain,
                                         add_signed_attrs,
                                         &sdata) != 0) || (sdata == NULL)) {
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
    if (NSS_CMSContentInfo_SetContent_SignedData(msg, info,
                                                 sdata) != SECSuccess) {
        pkiDebug("%s: error setting signed-data content\n", __FUNCTION__);
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }

    /* Set the raw data as the contents for the signed-data. */
    info = NSS_CMSSignedData_GetContentInfo(sdata);
    if (info == NULL) {
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
    if (NSS_CMSContentInfo_SetContent(msg, info, encapsulated_tag,
                                      NULL) != SECSuccess) {
        pkiDebug("%s: error setting encapsulated content\n", __FUNCTION__);
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }

    /* Encode and export. */
    memset(&plain, 0, sizeof(plain));
    plain.data = key_pack;
    plain.len = key_pack_len;
    memset(&encoded, 0, sizeof(encoded));
    if (NSS_CMSDEREncode(msg, &plain, &encoded, pool) != SECSuccess) {
        pkiDebug("%s: error encoding enveloped-data\n", __FUNCTION__);
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
    if (secitem_to_buf_len(&encoded, envel_data, envel_data_len) != 0) {
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
#ifdef DEBUG_DER
    derdump(*envel_data, *envel_data_len);
#endif
#ifdef DEBUG_CMS
    cmsdump(*envel_data, *envel_data_len);
#endif

    NSS_CMSMessage_Destroy(msg);
    PORT_FreeArena(pool, PR_TRUE);

    return 0;
}

/* Check if this cert is marked as a CA which is trusted to issue certs for
 * the indicated usage.  Return PR_TRUE if it is. */
static PRBool
crypto_is_cert_trusted(CERTCertificate *cert, SECCertUsage usage)
{
    CERTCertTrust trust;
    unsigned int ca_trust;

    if (usage == certUsageSSLClient)
        ca_trust = CERTDB_TRUSTED_CLIENT_CA;
    else if (usage == certUsageSSLServer)
        ca_trust = CERTDB_TRUSTED_CA;
    else {
        pkiDebug("%s: internal error: needed CA trust unknown\n", __FUNCTION__);
        return PR_FALSE;
    }
    memset(&trust, 0, sizeof(trust));
    if (CERT_GetCertTrust(cert, &trust) != SECSuccess) {
        pkiDebug("%s: unable to find trust for \"%s\"\n", __FUNCTION__,
                 cert->subjectName);
        return PR_FALSE;
    }
    if ((SEC_GET_TRUST_FLAGS(&trust, trustSSL) & ca_trust) != ca_trust) {
        pkiDebug("%s: \"%s\" is not a trusted CA\n", __FUNCTION__,
                 cert->subjectName);
        return PR_FALSE;
    }
    return PR_TRUE;
}

/* Check if this cert includes an AuthorityInfoAccess extension which points
 * to an OCSP responder.  Return PR_TRUE if it does. */
static PRBool
crypto_cert_has_ocsp_responder(CERTCertificate *cert)
{
    CERTAuthInfoAccess **aia;
    SECOidData *ocsp;
    SECItem encoded_aia;
    int i;

    /* Look up the OID for "use an OCSP responder". */
    ocsp = SECOID_FindOIDByTag(SEC_OID_PKIX_OCSP);
    if (ocsp == NULL) {
        pkiDebug("%s: internal error: OCSP not known\n", __FUNCTION__);
        return PR_FALSE;
    }
    /* Find the AIA extension. */
    memset(&encoded_aia, 0, sizeof(encoded_aia));
    if (CERT_FindCertExtension(cert, SEC_OID_X509_AUTH_INFO_ACCESS,
                               &encoded_aia) != SECSuccess) {
        pkiDebug("%s: no AuthorityInfoAccess extension for \"%s\"\n",
                 __FUNCTION__, cert->subjectName);
        return PR_FALSE;
    }
    /* Decode the AIA extension. */
    aia = CERT_DecodeAuthInfoAccessExtension(cert->arena, &encoded_aia);
    if (aia == NULL) {
        pkiDebug("%s: error parsing AuthorityInfoAccess for \"%s\"\n",
                 __FUNCTION__, cert->subjectName);
        return PR_FALSE;
    }
    /* We're looking for at least one OCSP responder. */
    for (i = 0; (aia[i] != NULL); i++)
        if (SECITEM_ItemsAreEqual(&(aia[i]->method), &(ocsp->oid))) {
            pkiDebug("%s: found OCSP responder for \"%s\"\n",
                     __FUNCTION__, cert->subjectName);
            return PR_TRUE;
        }
    return PR_FALSE;
}

/* In the original implementation, the assumption has been that we'd use any
 * CRLs, and if we were missing a CRL for the certificate or any point in its
 * issuing chain, we'd raise a failure iff the require_crl_checking flag was
 * set.
 *
 * This is not exactly how NSS does things.  When checking the revocation
 * status of a particular certificate, NSS will consult a cached copy of a CRL
 * issued by the certificate's issuer if one's available.  If the CRL shows
 * that the certificate is revoked, it returns an error.  If it succeeds,
 * however, processing continues, and if the certificate contains an AIA
 * extension which lists an OCSP responder, the library attempts to contact the
 * responder to also give it a chance to tell us that the certificate has been
 * revoked.  We can control what happens if this connection attempt fails by
 * calling CERT_SetOCSPFailureMode().
 *
 * We attempt to compensate for this difference in behavior by walking the
 * issuing chain ourselves, ensuring that for the certificate and all of its
 * issuers, that either we have a CRL on-hand for its issuer, or if OCSP
 * checking is allowed, that the certificate contains the location of an OCSP
 * responder.  We stop only when we reach a trusted CA certificate, as NSS
 * does. */
static int
crypto_check_for_revocation_information(CERTCertificate *cert,
                                        CERTCertDBHandle *certdb,
                                        PRBool allow_ocsp_checking,
                                        SECCertUsage usage)
{
    CERTCertificate *issuer;
    CERTSignedCrl *crl;

    issuer = CERT_FindCertIssuer(cert, PR_Now(), usage);
    while (issuer != NULL) {
        /* Do we have a CRL for this cert's issuer? */
        crl = SEC_FindCrlByName(certdb, &cert->derIssuer, SEC_CRL_TYPE);
        if (crl != NULL) {
            pkiDebug("%s: have CRL for \"%s\"\n", __FUNCTION__,
                     cert->issuerName);
        } else {
            SEC_DestroyCrl(crl);
            if (allow_ocsp_checking) {
                /* Check if the cert points to an OCSP responder. */
                if (!crypto_cert_has_ocsp_responder(cert)) {
                    /* No CRL, no OCSP responder. */
                    pkiDebug("%s: no OCSP responder for \"%s\"\n", __FUNCTION__,
                             cert->subjectName);
                    return -1;
                }
            } else {
                /* No CRL, and OCSP not allowed. */
                pkiDebug("%s: no CRL for issuer \"%s\"\n", __FUNCTION__,
                         cert->issuerName);
                return -1;
            }
        }
        /* Check if this issuer is a trusted CA.  If it is, we're done. */
        if (crypto_is_cert_trusted(issuer, usage)) {
            pkiDebug("%s: \"%s\" is a trusted CA\n", __FUNCTION__,
                     issuer->subjectName);
            CERT_DestroyCertificate(issuer);
            return 0;
        }
        /* Move on to the next link in the chain. */
        cert = issuer;
        issuer = CERT_FindCertIssuer(cert, PR_Now(), usage);
        if (issuer == NULL) {
            pkiDebug("%s: unable to find issuer for \"%s\"\n", __FUNCTION__,
                     cert->subjectName);
            /* Don't leak the reference to the last intermediate. */
            CERT_DestroyCertificate(cert);
            return -1;
        }
        if (SECITEM_ItemsAreEqual(&cert->derCert, &issuer->derCert)) {
            pkiDebug("%s: \"%s\" is self-signed, but not trusted\n",
                     __FUNCTION__, cert->subjectName);
            /* Don't leak the references to the self-signed cert. */
            CERT_DestroyCertificate(issuer);
            CERT_DestroyCertificate(cert);
            return -1;
        }
        /* Don't leak the reference to the just-traversed intermediate. */
        CERT_DestroyCertificate(cert);
        cert = NULL;
    }
    return -1;
}

/* Verify that we have a signed-data content info, that it has one signer, that
 * the signer can be trusted, and then check the type of the encapsulated
 * content and return that content. */
static krb5_error_code
crypto_signeddata_common_verify(krb5_context context,
                                pkinit_plg_crypto_context plg_cryptoctx,
                                pkinit_req_crypto_context req_cryptoctx,
                                pkinit_identity_crypto_context id_cryptoctx,
                                int require_crl_checking,
                                NSSCMSContentInfo *cinfo,
                                CERTCertDBHandle *certdb,
                                SECCertUsage usage,
                                SECOidTag expected_type,
                                SECOidTag expected_type2,
                                PLArenaPool *pool,
                                int cms_msg_type,
                                SECItem **plain_out,
                                int *is_signed_out)
{
    NSSCMSSignedData *sdata;
    NSSCMSSignerInfo *signer;
    NSSCMSMessage *ecmsg;
    NSSCMSContentInfo *ecinfo;
    CERTCertificate *cert;
    SECOidTag encapsulated_tag;
    SEC_OcspFailureMode ocsp_failure_mode;
    SECOidData *expected, *received;
    SECStatus status;
    SECItem *edata;
    int n_signers;
    PRBool allow_ocsp_checking = PR_TRUE;

    *is_signed_out = 0;

    /* Handle cases where we're passed data containing signed-data. */
    if (NSS_CMSContentInfo_GetContentTypeTag(cinfo) == SEC_OID_PKCS7_DATA) {
        /* Look at the payload data. */
        edata = NSS_CMSContentInfo_GetContent(cinfo);
        if (edata == NULL) {
            pkiDebug("%s: no plain-data content\n", __FUNCTION__);
            return ENOMEM;
        }
        /* See if it's content-info. */
        ecmsg = NSS_CMSMessage_CreateFromDER(edata,
                                             NULL, NULL,
                                             crypto_pwcb,
                                             crypto_pwcb_prep(id_cryptoctx,
                                                              NULL, context),
                                             NULL, NULL);
        if (ecmsg == NULL) {
            pkiDebug("%s: plain-data not parsable\n", __FUNCTION__);
            return ENOMEM;
        }
        /* Check if it actually contains signed-data. */
        ecinfo = NSS_CMSMessage_GetContentInfo(ecmsg);
        if (ecinfo == NULL) {
            pkiDebug("%s: plain-data has no cinfo\n", __FUNCTION__);
            NSS_CMSMessage_Destroy(ecmsg);
            return ENOMEM;
        }
        if (NSS_CMSContentInfo_GetContentTypeTag(ecinfo) !=
            SEC_OID_PKCS7_SIGNED_DATA) {
            pkiDebug("%s: plain-data is not sdata\n", __FUNCTION__);
            NSS_CMSMessage_Destroy(ecmsg);
            return EINVAL;
        }
        pkiDebug("%s: parsed plain-data (length=%ld) as signed-data\n",
                 __FUNCTION__, (long) edata->len);
        cinfo = ecinfo;
    } else
        /* Okay, it's a normal signed-data blob. */
        ecmsg = NULL;

    /* Check that we have signed data, that it has exactly one signature,
     * and fish out the signer information. */
    if (NSS_CMSContentInfo_GetContentTypeTag(cinfo) !=
        SEC_OID_PKCS7_SIGNED_DATA) {
        pkiDebug("%s: content type mismatch\n", __FUNCTION__);
        if (ecmsg != NULL)
            NSS_CMSMessage_Destroy(ecmsg);
        return EINVAL;
    }
    sdata = NSS_CMSContentInfo_GetContent(cinfo);
    if (sdata == NULL) {
        pkiDebug("%s: decoding error? content-info was NULL\n", __FUNCTION__);
        if (ecmsg != NULL)
            NSS_CMSMessage_Destroy(ecmsg);
        return ENOENT;
    }
    n_signers = NSS_CMSSignedData_SignerInfoCount(sdata);
    if (n_signers > 1) {
        pkiDebug("%s: wrong number of signers (%d, not 0 or 1)\n",
                 __FUNCTION__, n_signers);
        if (ecmsg != NULL)
            NSS_CMSMessage_Destroy(ecmsg);
        return ENOENT;
    }
    if (n_signers < 1)
        signer = NULL;
    else {
        /* Import the bundle's certs and locate the signerInfo. */
        if (NSS_CMSSignedData_ImportCerts(sdata, certdb, usage,
                                          PR_FALSE) != SECSuccess) {
            pkiDebug("%s: error importing signer certs\n", __FUNCTION__);
            if (ecmsg != NULL)
                NSS_CMSMessage_Destroy(ecmsg);
            return ENOENT;
        }
        signer = NSS_CMSSignedData_GetSignerInfo(sdata, 0);
        if (signer == NULL) {
            pkiDebug("%s: no signers?\n", __FUNCTION__);
            if (ecmsg != NULL)
                NSS_CMSMessage_Destroy(ecmsg);
            return ENOENT;
        }
        if (!NSS_CMSSignedData_HasDigests(sdata)) {
            pkiDebug("%s: no digests?\n", __FUNCTION__);
            if (ecmsg != NULL)
                NSS_CMSMessage_Destroy(ecmsg);
            return ENOENT;
        }
        if (require_crl_checking && (signer->cert != NULL))
            if (crypto_check_for_revocation_information(signer->cert, certdb,
                                                        allow_ocsp_checking,
                                                        usage) != 0) {
                if (ecmsg != NULL)
                    NSS_CMSMessage_Destroy(ecmsg);
                return KRB5KDC_ERR_REVOCATION_STATUS_UNAVAILABLE;
            }
        if (allow_ocsp_checking) {
            status = CERT_EnableOCSPChecking(certdb);
            if (status != SECSuccess) {
                pkiDebug("%s: error enabling OCSP: %s\n", __FUNCTION__,
                         PR_ErrorToString(status == SECFailure ?
                                          PORT_GetError() : status,
                                          PR_LANGUAGE_I_DEFAULT));
                if (ecmsg != NULL)
                    NSS_CMSMessage_Destroy(ecmsg);
                return ENOMEM;
            }
            ocsp_failure_mode = require_crl_checking ?
                ocspMode_FailureIsVerificationFailure :
                ocspMode_FailureIsNotAVerificationFailure;
            status = CERT_SetOCSPFailureMode(ocsp_failure_mode);
            if (status != SECSuccess) {
                pkiDebug("%s: error setting OCSP failure mode: %s\n",
                         __FUNCTION__,
                         PR_ErrorToString(status == SECFailure ?
                                          PORT_GetError() : status,
                                          PR_LANGUAGE_I_DEFAULT));
                if (ecmsg != NULL)
                    NSS_CMSMessage_Destroy(ecmsg);
                return ENOMEM;
            }
        } else {
            status = CERT_DisableOCSPChecking(certdb);
            if ((status != SECSuccess) &&
                (PORT_GetError() != SEC_ERROR_OCSP_NOT_ENABLED)) {
                pkiDebug("%s: error disabling OCSP: %s\n", __FUNCTION__,
                         PR_ErrorToString(status == SECFailure ?
                                          PORT_GetError() : status,
                                          PR_LANGUAGE_I_DEFAULT));
                if (ecmsg != NULL)
                    NSS_CMSMessage_Destroy(ecmsg);
                return ENOMEM;
            }
        }
        status = NSS_CMSSignedData_VerifySignerInfo(sdata, 0, certdb, usage);
        if (status != SECSuccess) {
            pkiDebug("%s: signer verify failed: %s\n", __FUNCTION__,
                     PR_ErrorToString(status == SECFailure ?
                                      PORT_GetError() : status,
                                      PR_LANGUAGE_I_DEFAULT));
            if (ecmsg != NULL)
                NSS_CMSMessage_Destroy(ecmsg);
            switch (cms_msg_type) {
            case CMS_SIGN_DRAFT9:
            case CMS_SIGN_CLIENT:
                switch (PORT_GetError()) {
                case SEC_ERROR_REVOKED_CERTIFICATE:
                    return KRB5KDC_ERR_REVOKED_CERTIFICATE;
                case SEC_ERROR_UNKNOWN_ISSUER:
                    return KRB5KDC_ERR_CANT_VERIFY_CERTIFICATE;
                default:
                    return KRB5KDC_ERR_CLIENT_NOT_TRUSTED;
                }
                break;
            case CMS_SIGN_SERVER:
            case CMS_ENVEL_SERVER:
                switch (PORT_GetError()) {
                case SEC_ERROR_REVOKED_CERTIFICATE:
                    return KRB5KDC_ERR_REVOKED_CERTIFICATE;
                case SEC_ERROR_UNKNOWN_ISSUER:
                    return KRB5KDC_ERR_CANT_VERIFY_CERTIFICATE;
                default:
                    return KRB5KDC_ERR_KDC_NOT_TRUSTED;
                }
                break;
            default:
                return ENOMEM;
            }
        }
        pkiDebug("%s: signer verify passed\n", __FUNCTION__);
        *is_signed_out = 1;
    }
    /* Pull out the payload. */
    ecinfo = NSS_CMSSignedData_GetContentInfo(sdata);
    if (ecinfo == NULL) {
        pkiDebug("%s: error getting encapsulated content\n", __FUNCTION__);
        if (ecmsg != NULL)
            NSS_CMSMessage_Destroy(ecmsg);
        return ENOMEM;
    }
    encapsulated_tag = NSS_CMSContentInfo_GetContentTypeTag(ecinfo);
    if ((encapsulated_tag != expected_type) &&
        ((expected_type2 == SEC_OID_UNKNOWN) ||
         (encapsulated_tag != expected_type2))) {
        pkiDebug("%s: wrong encapsulated content type\n", __FUNCTION__);
        expected = SECOID_FindOIDByTag(expected_type);
        if (encapsulated_tag != SEC_OID_UNKNOWN)
            received = SECOID_FindOIDByTag(encapsulated_tag);
        else
            received = NULL;
        if (expected != NULL) {
            if (received != NULL) {
                pkiDebug("%s: was expecting \"%s\"(%d), but got \"%s\"(%d)\n",
                         __FUNCTION__,
                         expected->desc, expected->offset,
                         received->desc, received->offset);
            } else {
                pkiDebug("%s: was expecting \"%s\"(%d), "
                         "but got unrecognized type (%d)\n",
                         __FUNCTION__,
                         expected->desc, expected->offset, encapsulated_tag);
            }
        }
        if (ecmsg != NULL)
            NSS_CMSMessage_Destroy(ecmsg);
        return EINVAL;
    }
    *plain_out = NSS_CMSContentInfo_GetContent(ecinfo);
    if ((*plain_out != NULL) && ((*plain_out)->len == 0))
        pkiDebug("%s: warning: encapsulated content appears empty\n",
                 __FUNCTION__);
    if (signer != NULL) {
        /* Save the peer cert -- we'll need it later. */
        pkiDebug("%s: saving peer certificate\n", __FUNCTION__);
        if (req_cryptoctx->peer_cert != NULL)
            CERT_DestroyCertificate(req_cryptoctx->peer_cert);
        cert = NSS_CMSSignerInfo_GetSigningCertificate(signer, certdb);
        req_cryptoctx->peer_cert = CERT_DupCertificate(cert);
    }
    if (ecmsg != NULL) {
        *plain_out = SECITEM_ArenaDupItem(pool, *plain_out);
        NSS_CMSMessage_Destroy(ecmsg);
    }
    return 0;
}

/* Verify signed-then-enveloped data, and return the data that was signed. */
krb5_error_code
cms_envelopeddata_verify(krb5_context context,
                         pkinit_plg_crypto_context plg_cryptoctx,
                         pkinit_req_crypto_context req_cryptoctx,
                         pkinit_identity_crypto_context id_cryptoctx,
                         krb5_preauthtype pa_type,
                         int require_crl_checking,
                         unsigned char *envel_data,
                         unsigned int envel_data_len,
                         unsigned char **signed_data,
                         unsigned int *signed_data_len)
{
    NSSCMSMessage *msg;
    NSSCMSContentInfo *info;
    NSSCMSEnvelopedData *env;
    CERTCertDBHandle *certdb;
    PLArenaPool *pool;
    SECItem *plain, encoded;
    SECCertUsage usage;
    SECOidTag expected_tag, expected_tag2;
    int is_signed, ret;

    pool = PORT_NewArena(sizeof(double));
    if (pool == NULL)
        return ENOMEM;
    certdb = CERT_GetDefaultCertDB();

    /* Decode the message. */
#ifdef DEBUG_DER
    derdump(envel_data, envel_data_len);
#endif
    encoded.data = envel_data;
    encoded.len = envel_data_len;
    msg = NSS_CMSMessage_CreateFromDER(&encoded,
                                       NULL, NULL,
                                       crypto_pwcb,
                                       crypto_pwcb_prep(id_cryptoctx,
                                                        NULL, context),
                                       NULL, NULL);
    if (msg == NULL)
        return ENOMEM;

    /* Make sure it's enveloped-data. */
    info = NSS_CMSMessage_GetContentInfo(msg);
    if (info == NULL) {
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
    if (NSS_CMSContentInfo_GetContentTypeTag(info) !=
        SEC_OID_PKCS7_ENVELOPED_DATA) {
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return EINVAL;
    }

    /* Okay, it's enveloped-data. */
    env = NSS_CMSContentInfo_GetContent(info);

    /* Pull out the encapsulated content.  It should be signed-data. */
    info = NSS_CMSEnvelopedData_GetContentInfo(env);
    if (info == NULL) {
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }

    /* Pull out the signed data and verify it. */
    expected_tag = get_pkinit_data_rkey_data_tag();
    expected_tag2 = SEC_OID_PKCS7_DATA;
    usage = certUsageSSLServer;
    plain = NULL;
    is_signed = 0;
    ret = crypto_signeddata_common_verify(context,
                                          plg_cryptoctx,
                                          req_cryptoctx,
                                          id_cryptoctx,
                                          require_crl_checking,
                                          info,
                                          certdb,
                                          usage,
                                          expected_tag,
                                          expected_tag2,
                                          pool,
                                          CMS_ENVEL_SERVER,
                                          &plain,
                                          &is_signed);
    if ((ret != 0) || (plain == NULL) || !is_signed) {
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return ret ? ret : ENOMEM;
    }
    /* Export the payload. */
    if (secitem_to_buf_len(plain, signed_data, signed_data_len) != 0) {
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
    NSS_CMSMessage_Destroy(msg);
    PORT_FreeArena(pool, PR_TRUE);

    return 0;
}

krb5_error_code
cms_signeddata_create(krb5_context context,
                      pkinit_plg_crypto_context plg_cryptoctx,
                      pkinit_req_crypto_context req_cryptoctx,
                      pkinit_identity_crypto_context id_cryptoctx,
                      int cms_msg_type,
                      int include_certchain,
                      unsigned char *payload,
                      unsigned int payload_len,
                      unsigned char **signed_data,
                      unsigned int *signed_data_len)
{
    NSSCMSMessage *msg;
    NSSCMSContentInfo *info;
    NSSCMSSignedData *sdata;
    PLArenaPool *pool;
    SECItem plain, encoded;
    SECOidTag digest, encapsulated_tag;
    enum sdcc_include_signed_attrs add_signed_attrs;

    switch (cms_msg_type) {
    case CMS_SIGN_DRAFT9:
        digest = SEC_OID_MD5;
        add_signed_attrs = signeddata_common_create_omit_signed_attrs;
        encapsulated_tag = get_pkinit_data_auth_data9_tag();
        break;
    case CMS_SIGN_CLIENT:
        digest = SEC_OID_SHA1;
        add_signed_attrs = signeddata_common_create_with_signed_attrs;
        encapsulated_tag = get_pkinit_data_auth_data_tag();
        break;
    case CMS_SIGN_SERVER:
        digest = SEC_OID_SHA1;
        add_signed_attrs = signeddata_common_create_with_signed_attrs;
        encapsulated_tag = get_pkinit_data_dhkey_data_tag();
        break;
    case CMS_ENVEL_SERVER:
    default:
        return ENOSYS;
        break;
    }

    pool = PORT_NewArena(sizeof(double));
    if (pool == NULL)
        return ENOMEM;

    /* Create the containing message. */
    msg = NSS_CMSMessage_Create(pool);
    if (msg == NULL) {
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }

    /* Create a signed-data pointer and set it as the message's
     * contents. */
    info = NSS_CMSMessage_GetContentInfo(msg);
    if (info == NULL) {
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
    sdata = NULL;
    if ((crypto_signeddata_common_create(context,
                                         plg_cryptoctx,
                                         req_cryptoctx,
                                         id_cryptoctx,
                                         msg,
                                         digest,
                                         include_certchain ?
                                         signeddata_common_create_with_chain :
                                         signeddata_common_create_omit_chain,
                                         add_signed_attrs,
                                         &sdata) != 0) || (sdata == NULL)) {
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
    if (NSS_CMSContentInfo_SetContent_SignedData(msg, info,
                                                 sdata) != SECSuccess) {
        pkiDebug("%s: error setting signed-data content\n", __FUNCTION__);
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }

    /* Set the data as the contents of the signed-data. */
    info = NSS_CMSSignedData_GetContentInfo(sdata);
    if (info == NULL) {
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
    if (NSS_CMSContentInfo_SetContent(msg, info, encapsulated_tag,
                                      NULL) != SECSuccess) {
        pkiDebug("%s: error setting encapsulated content type\n",
                 __FUNCTION__);
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }

    /* Encode and export. */
    memset(&plain, 0, sizeof(plain));
    plain.data = payload;
    plain.len = payload_len;
    memset(&encoded, 0, sizeof(encoded));
    if (NSS_CMSDEREncode(msg, &plain, &encoded, pool) != SECSuccess) {
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        pkiDebug("%s: error encoding signed-data: %s\n", __FUNCTION__,
                 PORT_ErrorToName(PORT_GetError()));
        return ENOMEM;
    }
    if (secitem_to_buf_len(&encoded, signed_data, signed_data_len) != 0) {
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
#ifdef DEBUG_DER
    derdump(*signed_data, *signed_data_len);
#endif
#ifdef DEBUG_CMS
    cmsdump(*signed_data, *signed_data_len);
#endif

    NSS_CMSMessage_Destroy(msg);
    PORT_FreeArena(pool, PR_TRUE);

    return 0;
}

krb5_error_code
cms_signeddata_verify(krb5_context context,
                      pkinit_plg_crypto_context plg_cryptoctx,
                      pkinit_req_crypto_context req_cryptoctx,
                      pkinit_identity_crypto_context id_cryptoctx,
                      int cms_msg_type,
                      int require_crl_checking,
                      unsigned char *signed_data,
                      unsigned int signed_data_len,
                      unsigned char **payload,
                      unsigned int *payload_len,
                      unsigned char **authz_data,
                      unsigned int *authz_data_len,
                      int *is_signed)
{
    NSSCMSMessage *msg;
    NSSCMSContentInfo *info;
    CERTCertDBHandle *certdb;
    SECCertUsage usage;
    SECOidTag expected_tag, expected_tag2;
    PLArenaPool *pool;
    SECItem *plain, encoded;
    struct content_info simple_content_info;
    int was_signed, ret;

    switch (cms_msg_type) {
    case CMS_SIGN_DRAFT9:
        usage = certUsageSSLClient;
        expected_tag = get_pkinit_data_auth_data9_tag();
        break;
    case CMS_SIGN_CLIENT:
        usage = certUsageSSLClient;
        expected_tag = get_pkinit_data_auth_data_tag();
        break;
    case CMS_SIGN_SERVER:
        usage = certUsageSSLServer;
        expected_tag = get_pkinit_data_dhkey_data_tag();
        break;
    case CMS_ENVEL_SERVER:
    default:
        return ENOSYS;
        break;
    }
    expected_tag2 = SEC_OID_UNKNOWN;

    pool = PORT_NewArena(sizeof(double));
    if (pool == NULL)
        return ENOMEM;
    certdb = CERT_GetDefaultCertDB();

#ifdef DEBUG_DER
    derdump(signed_data, signed_data_len);
#endif

    memset(&encoded, 0, sizeof(encoded));
    encoded.data = signed_data;
    encoded.len = signed_data_len;

    /* Take a quick look at what it claims to be. */
    memset(&simple_content_info, 0, sizeof(simple_content_info));
    if (SEC_ASN1DecodeItem(pool, &simple_content_info,
                           content_info_template, &encoded) == SECSuccess)
        /* If it's unsigned data of the right type... */
        if (SECOID_FindOIDTag(&simple_content_info.content_type) ==
            expected_tag) {
            /* Pull out the payload -- it's not wrapped in a
             * SignedData. */
            pkiDebug("%s: data is not signed\n", __FUNCTION__);
            if (is_signed != NULL)
                *is_signed = 0;
            if (secitem_to_buf_len(&simple_content_info.content,
                                   payload, payload_len) != 0) {
                PORT_FreeArena(pool, PR_TRUE);
                return ENOMEM;
            }
            return 0;
        }

    /* Decode the message. */
    msg = NSS_CMSMessage_CreateFromDER(&encoded,
                                       NULL, NULL,
                                       crypto_pwcb,
                                       crypto_pwcb_prep(id_cryptoctx,
                                                        NULL, context),
                                       NULL, NULL);
    if (msg == NULL)
        return ENOMEM;

    /* Double-check that it's signed. */
    info = NSS_CMSMessage_GetContentInfo(msg);
    if (info == NULL) {
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
    switch (NSS_CMSContentInfo_GetContentTypeTag(info)) {
    case SEC_OID_PKCS7_SIGNED_DATA:
        /* It's signed: try to verify the signature. */
        pkiDebug("%s: data is probably signed, checking\n", __FUNCTION__);
        plain = NULL;
        was_signed = 0;
        ret = crypto_signeddata_common_verify(context,
                                              plg_cryptoctx,
                                              req_cryptoctx,
                                              id_cryptoctx,
                                              require_crl_checking,
                                              info,
                                              certdb,
                                              usage,
                                              expected_tag,
                                              expected_tag2,
                                              pool,
                                              cms_msg_type,
                                              &plain,
                                              &was_signed);
        if ((ret != 0) || (plain == NULL)) {
            NSS_CMSMessage_Destroy(msg);
            PORT_FreeArena(pool, PR_TRUE);
            return ret ? ret : ENOMEM;
        }
        if (is_signed != NULL)
            *is_signed = was_signed;
        break;
    case SEC_OID_PKCS7_DATA:
        /* It's not signed: try to pull out the payload. */
        pkiDebug("%s: data is not signed\n", __FUNCTION__);
        if (is_signed != NULL)
            *is_signed = 0;
        plain = NSS_CMSContentInfo_GetContent(info);
        break;
    default:
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }

    /* Export the payload. */
    if ((plain == NULL) ||
        (secitem_to_buf_len(plain, payload, payload_len) != 0)) {
        NSS_CMSMessage_Destroy(msg);
        PORT_FreeArena(pool, PR_TRUE);
        return ENOMEM;
    }
    NSS_CMSMessage_Destroy(msg);
    PORT_FreeArena(pool, PR_TRUE);

    return 0;
}

/*
 * Add an item to the pkinit_identity_crypto_context's list of deferred
 * identities.
 */
krb5_error_code
crypto_set_deferred_id(krb5_context context,
                       pkinit_identity_crypto_context id_cryptoctx,
                       const char *identity, const char *password)
{
    unsigned long ck_flags;

    ck_flags = pkinit_get_deferred_id_flags(id_cryptoctx->deferred_ids,
                                            identity);
    return pkinit_set_deferred_id(&id_cryptoctx->deferred_ids,
                                  identity, ck_flags, password);
}

/*
 * Retrieve a read-only copy of the pkinit_identity_crypto_context's list of
 * deferred identities, sure to be valid only until the next time someone calls
 * either pkinit_set_deferred_id() or crypto_set_deferred_id().
 */
const pkinit_deferred_id *
crypto_get_deferred_ids(krb5_context context,
                        pkinit_identity_crypto_context id_cryptoctx)
{
    pkinit_deferred_id *deferred;
    const pkinit_deferred_id *ret;

    deferred = id_cryptoctx->deferred_ids;
    ret = (const pkinit_deferred_id *)deferred;
    return ret;
}
