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

#ifndef _PKINIT_H
#define _PKINIT_H

#include <k5-platform.h>
#include <krb5/krb5.h>
#include <krb5/preauth_plugin.h>
#include <k5-int-pkinit.h>
#include <profile.h>
#include "pkinit_accessor.h"
#include "pkinit_trace.h"

#ifndef WITHOUT_PKCS11
#include "pkcs11.h"

#define PKCS11_MODNAME "opensc-pkcs11.so"
#define PK_SIGLEN_GUESS 1000
#define PK_NOSLOT 999999
#endif

#define DH_PROTOCOL     1
#define RSA_PROTOCOL    2

#define TD_TRUSTED_CERTIFIERS 104
#define TD_INVALID_CERTIFICATES 105
#define TD_DH_PARAMETERS 109

#define PKINIT_CTX_MAGIC	0x05551212
#define PKINIT_REQ_CTX_MAGIC	0xdeadbeef
#define PKINIT_DEFERRED_ID_MAGIC    0x3ca20d21

#define PKINIT_DEFAULT_DH_MIN_BITS  2048
#define PKINIT_DH_MIN_CONFIG_BITS   1024

#define KRB5_CONF_KDCDEFAULTS                   "kdcdefaults"
#define KRB5_CONF_LIBDEFAULTS                   "libdefaults"
#define KRB5_CONF_REALMS                        "realms"
#define KRB5_CONF_PKINIT_ALLOW_UPN              "pkinit_allow_upn"
#define KRB5_CONF_PKINIT_ANCHORS                "pkinit_anchors"
#define KRB5_CONF_PKINIT_INDICATOR              "pkinit_indicator"
#define KRB5_CONF_PKINIT_CERT_MATCH             "pkinit_cert_match"
#define KRB5_CONF_PKINIT_DH_MIN_BITS            "pkinit_dh_min_bits"
#define KRB5_CONF_PKINIT_EKU_CHECKING           "pkinit_eku_checking"
#define KRB5_CONF_PKINIT_IDENTITIES             "pkinit_identities"
#define KRB5_CONF_PKINIT_IDENTITY               "pkinit_identity"
#define KRB5_CONF_PKINIT_KDC_HOSTNAME           "pkinit_kdc_hostname"
#define KRB5_CONF_PKINIT_KDC_OCSP               "pkinit_kdc_ocsp"
#define KRB5_CONF_PKINIT_POOL                   "pkinit_pool"
#define KRB5_CONF_PKINIT_REQUIRE_CRL_CHECKING   "pkinit_require_crl_checking"
#define KRB5_CONF_PKINIT_REVOKE                 "pkinit_revoke"

/* Make pkiDebug(fmt,...) print, or not.  */
#ifdef DEBUG
#define pkiDebug	printf
#else
/* Still evaluates for side effects.  */
static inline void pkiDebug (const char *fmt, ...) { }
/* This is better if the compiler doesn't inline variadic functions
   well, but gcc will warn about "left-hand operand of comma
   expression has no effect".  Still evaluates for side effects.  */
/* #define pkiDebug	(void) */
#endif

/* Solaris compiler doesn't grok __FUNCTION__
 * hack for now.  Fix all the uses eventually. */
#define __FUNCTION__ __func__

/* Macros to deal with converting between various data types... */
#define PADATA_TO_KRB5DATA(pad, k5d) \
    (k5d)->length = (pad)->length; (k5d)->data = (char *)(pad)->contents;
#define OCTETDATA_TO_KRB5DATA(octd, k5d) \
    (k5d)->length = (octd)->length; (k5d)->data = (char *)(octd)->data;

extern const krb5_data dh_oid;

/*
 * notes about crypto contexts:
 *
 * the basic idea is that there are crypto contexts that live at
 * both the plugin level and request level. the identity context (that
 * keeps info about your own certs and such) is separate because
 * it is needed at different levels for the kdc and and the client.
 * (the kdc's identity is at the plugin level, the client's identity
 * information could change per-request.)
 * the identity context is meant to have the entity's cert,
 * a list of trusted and intermediate cas, a list of crls, and any
 * pkcs11 information.  the req context is meant to have the
 * received certificate and the DH related information. the plugin
 * context is meant to have global crypto information, i.e., OIDs
 * and constant DH parameter information.
 */

/*
 * plugin crypto context should keep plugin common information,
 * eg., OIDs, known DHparams
 */
typedef struct _pkinit_plg_crypto_context *pkinit_plg_crypto_context;

/*
 * request crypto context should keep reqyest common information,
 * eg., received credentials, DH parameters of this request
 */
typedef struct _pkinit_req_crypto_context *pkinit_req_crypto_context;

/*
 * identity context should keep information about credentials
 * for the request, eg., my credentials, trusted ca certs,
 * intermediate ca certs, crls, pkcs11 info
 */
typedef struct _pkinit_identity_crypto_context *pkinit_identity_crypto_context;

/*
 * this structure keeps information about the config options
 */
typedef struct _pkinit_plg_opts {
    int require_eku;	    /* require EKU checking (default is true) */
    int accept_secondary_eku;/* accept secondary EKU (default is false) */
    int allow_upn;	    /* allow UPN-SAN instead of pkinit-SAN */
    int dh_or_rsa;	    /* selects DH or RSA based pkinit */
    int require_crl_checking; /* require CRL for a CA (default is false) */
    int dh_min_bits;	    /* minimum DH modulus size allowed */
} pkinit_plg_opts;

/*
 * this structure keeps options used for a given request
 */
typedef struct _pkinit_req_opts {
    int require_eku;
    int accept_secondary_eku;
    int allow_upn;
    int dh_or_rsa;
    int require_crl_checking;
    int dh_size;	    /* initial request DH modulus size (default=1024) */
    int require_hostname_match;
} pkinit_req_opts;

/*
 * information about identity from config file or command line
 */

typedef struct _pkinit_identity_opts {
    char *identity;
    char **identity_alt;
    char **anchors;
    char **intermediates;
    char **crls;
    char *ocsp;
    int  idtype;
    char *cert_filename;
    char *key_filename;
#ifndef WITHOUT_PKCS11
    char *p11_module_name;
    CK_SLOT_ID slotid;
    char *token_label;
    char *cert_id_string;
    char *cert_label;
#endif
} pkinit_identity_opts;


/*
 * Client's plugin context
 */
struct _pkinit_context {
    int magic;
    pkinit_plg_crypto_context cryptoctx;
    pkinit_plg_opts *opts;
    pkinit_identity_opts *idopts;
};
typedef struct _pkinit_context *pkinit_context;

/*
 * Client's per-request context
 */
struct _pkinit_req_context {
    unsigned int magic;
    pkinit_req_crypto_context cryptoctx;
    pkinit_req_opts *opts;
    pkinit_identity_crypto_context idctx;
    pkinit_identity_opts *idopts;
    int do_identity_matching;
    krb5_preauthtype pa_type;
    int rfc6112_kdc;
    int identity_initialized;
    int identity_prompted;
    krb5_error_code identity_prompt_retval;
};
typedef struct _pkinit_req_context *pkinit_req_context;

/*
 * KDC's (per-realm) plugin context
 */
struct _pkinit_kdc_context {
    int magic;
    pkinit_plg_crypto_context cryptoctx;
    pkinit_plg_opts *opts;
    pkinit_identity_crypto_context idctx;
    pkinit_identity_opts *idopts;
    char *realmname;
    unsigned int realmname_len;
    char **auth_indicators;
};
typedef struct _pkinit_kdc_context *pkinit_kdc_context;

/*
 * KDC's per-request context
 */
struct _pkinit_kdc_req_context {
    int magic;
    pkinit_req_crypto_context cryptoctx;
    krb5_auth_pack *rcv_auth_pack;
    krb5_auth_pack_draft9 *rcv_auth_pack9;
    krb5_preauthtype pa_type;
};
typedef struct _pkinit_kdc_req_context *pkinit_kdc_req_context;

/*
 * Functions in pkinit_lib.c
 */

krb5_error_code pkinit_init_req_opts(pkinit_req_opts **);
void pkinit_fini_req_opts(pkinit_req_opts *);

krb5_error_code pkinit_init_plg_opts(pkinit_plg_opts **);
void pkinit_fini_plg_opts(pkinit_plg_opts *);

krb5_error_code pkinit_init_identity_opts(pkinit_identity_opts **idopts);
void pkinit_fini_identity_opts(pkinit_identity_opts *idopts);
krb5_error_code pkinit_dup_identity_opts(pkinit_identity_opts *src_opts,
					 pkinit_identity_opts **dest_opts);

/*
 * Functions in pkinit_identity.c
 */
char * idtype2string(int idtype);
char * catype2string(int catype);

krb5_error_code pkinit_identity_initialize
	(krb5_context context,				/* IN */
	 pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	 pkinit_req_crypto_context req_cryptoctx,	/* IN */
	 pkinit_identity_opts *idopts,			/* IN */
	 pkinit_identity_crypto_context id_cryptoctx,	/* IN/OUT */
	 krb5_clpreauth_callbacks cb,			/* IN/OUT */
	 krb5_clpreauth_rock rock,			/* IN/OUT */
	 krb5_principal princ);				/* IN (optional) */

krb5_error_code pkinit_identity_prompt
	(krb5_context context,				/* IN */
	 pkinit_plg_crypto_context plg_cryptoctx,	/* IN */
	 pkinit_req_crypto_context req_cryptoctx,	/* IN */
	 pkinit_identity_opts *idopts,			/* IN */
	 pkinit_identity_crypto_context id_cryptoctx,	/* IN/OUT */
	 krb5_clpreauth_callbacks cb,			/* IN/OUT */
	 krb5_clpreauth_rock rock,			/* IN/OUT */
	 int do_matching,				/* IN */
	 krb5_principal princ);				/* IN (optional) */

krb5_error_code pkinit_cert_matching
	(krb5_context context,
	pkinit_plg_crypto_context plg_cryptoctx,
	pkinit_req_crypto_context req_cryptoctx,
	pkinit_identity_crypto_context id_cryptoctx,
	krb5_principal princ);

/*
 * Client's list of identities for which it needs PINs or passwords
 */
struct _pkinit_deferred_id {
    int magic;
    char *identity;
    unsigned long ck_flags;
    char *password;
};
typedef struct _pkinit_deferred_id *pkinit_deferred_id;

krb5_error_code pkinit_set_deferred_id
	(pkinit_deferred_id **identities, const char *identity,
	 unsigned long ck_flags, const char *password);
const char * pkinit_find_deferred_id
	(pkinit_deferred_id *identities, const char *identity);
unsigned long pkinit_get_deferred_id_flags
	(pkinit_deferred_id *identities, const char *identity);
void pkinit_free_deferred_ids(pkinit_deferred_id *identities);

/*
 * initialization and free functions
 */
void init_krb5_pa_pk_as_req(krb5_pa_pk_as_req **in);
void init_krb5_pa_pk_as_req_draft9(krb5_pa_pk_as_req_draft9 **in);
void init_krb5_reply_key_pack(krb5_reply_key_pack **in);
void init_krb5_reply_key_pack_draft9(krb5_reply_key_pack_draft9 **in);

void init_krb5_pa_pk_as_rep(krb5_pa_pk_as_rep **in);
void init_krb5_pa_pk_as_rep_draft9(krb5_pa_pk_as_rep_draft9 **in);
void init_krb5_subject_pk_info(krb5_subject_pk_info **in);

void free_krb5_pa_pk_as_req(krb5_pa_pk_as_req **in);
void free_krb5_pa_pk_as_req_draft9(krb5_pa_pk_as_req_draft9 **in);
void free_krb5_reply_key_pack(krb5_reply_key_pack **in);
void free_krb5_reply_key_pack_draft9(krb5_reply_key_pack_draft9 **in);
void free_krb5_auth_pack(krb5_auth_pack **in);
void free_krb5_auth_pack_draft9(krb5_context, krb5_auth_pack_draft9 **in);
void free_krb5_pa_pk_as_rep(krb5_pa_pk_as_rep **in);
void free_krb5_pa_pk_as_rep_draft9(krb5_pa_pk_as_rep_draft9 **in);
void free_krb5_external_principal_identifier(krb5_external_principal_identifier ***in);
void free_krb5_algorithm_identifiers(krb5_algorithm_identifier ***in);
void free_krb5_algorithm_identifier(krb5_algorithm_identifier *in);
void free_krb5_kdc_dh_key_info(krb5_kdc_dh_key_info **in);
void free_krb5_subject_pk_info(krb5_subject_pk_info **in);
krb5_error_code pkinit_copy_krb5_data(krb5_data *dst, const krb5_data *src);


/*
 * Functions in pkinit_profile.c
 */
krb5_error_code pkinit_kdcdefault_strings
	(krb5_context context, const char *realmname, const char *option,
	 char ***ret_value);
krb5_error_code pkinit_kdcdefault_string
	(krb5_context context, const char *realmname, const char *option,
	 char **ret_value);
krb5_error_code pkinit_kdcdefault_boolean
	(krb5_context context, const char *realmname, const char *option,
	 int default_value, int *ret_value);
krb5_error_code pkinit_kdcdefault_integer
	(krb5_context context, const char *realmname, const char *option,
	 int default_value, int *ret_value);


krb5_error_code pkinit_libdefault_strings
	(krb5_context context, const krb5_data *realm,
	 const char *option, char ***ret_value);
krb5_error_code pkinit_libdefault_string
	(krb5_context context, const krb5_data *realm,
	 const char *option, char **ret_value);
krb5_error_code pkinit_libdefault_boolean
	(krb5_context context, const krb5_data *realm, const char *option,
	 int default_value, int *ret_value);
krb5_error_code pkinit_libdefault_integer
	(krb5_context context, const krb5_data *realm, const char *option,
	 int default_value, int *ret_value);

/*
 * debugging functions
 */
void print_buffer(const unsigned char *, unsigned int);
void print_buffer_bin(unsigned char *, unsigned int, char *);

/*
 * Now get crypto function declarations
 */
#include "pkinit_crypto.h"

#endif	/* _PKINIT_H */
