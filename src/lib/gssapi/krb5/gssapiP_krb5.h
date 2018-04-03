/* -*- mode: c; indent-tabs-mode: nil -*- */
/*
 * Copyright 2000, 2008 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 */
/*
 * Copyright 1993 by OpenVision Technologies, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OpenVision not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. OpenVision makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * OPENVISION DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL OPENVISION BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _GSSAPIP_KRB5_H_
#define _GSSAPIP_KRB5_H_

#include <k5-int.h>

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif

/* work around sunos braindamage */
#ifdef major
#undef major
#endif
#ifdef minor
#undef minor
#endif

#include "gssapiP_generic.h"

/* The include of gssapi_krb5.h will dtrt with the above #defines in
 * effect.
 */
#include "gssapi_krb5.h"
#include "gssapi_err_krb5.h"
#include "gssapi_ext.h"

/* for debugging */
#undef CFX_EXERCISE

/** constants **/

#define GSS_MECH_KRB5_OID_LENGTH 9
#define GSS_MECH_KRB5_OID "\052\206\110\206\367\022\001\002\002"

#define GSS_MECH_KRB5_OLD_OID_LENGTH 5
#define GSS_MECH_KRB5_OLD_OID "\053\005\001\005\002"

/* Incorrect krb5 mech OID emitted by MS. */
#define GSS_MECH_KRB5_WRONG_OID_LENGTH 9
#define GSS_MECH_KRB5_WRONG_OID "\052\206\110\202\367\022\001\002\002"

/* IAKERB variant */
#define GSS_MECH_IAKERB_OID_LENGTH 6
#define GSS_MECH_IAKERB_OID "\053\006\001\005\002\005"

extern const gss_OID_set kg_all_mechs;

#define CKSUMTYPE_KG_CB         0x8003

#define KG_TOK_CTX_AP_REQ       0x0100
#define KG_TOK_CTX_AP_REP       0x0200
#define KG_TOK_CTX_ERROR        0x0300
#define KG_TOK_SIGN_MSG         0x0101
#define KG_TOK_SEAL_MSG         0x0201
#define KG_TOK_MIC_MSG          0x0101
#define KG_TOK_WRAP_MSG         0x0201
#define KG_TOK_DEL_CTX          0x0102
#define KG2_TOK_MIC_MSG         0x0404
#define KG2_TOK_WRAP_MSG        0x0504
#define KG2_TOK_DEL_CTX         0x0405
#define IAKERB_TOK_PROXY        0x0501

#define KRB5_GSS_FOR_CREDS_OPTION 1

#define KG2_RESP_FLAG_ERROR             0x0001
#define KG2_RESP_FLAG_DELEG_OK          0x0002

/** CFX flags **/
#define FLAG_SENDER_IS_ACCEPTOR 0x01
#define FLAG_WRAP_CONFIDENTIAL  0x02
#define FLAG_ACCEPTOR_SUBKEY    0x04

/* These are to be stored in little-endian order, i.e., des-mac is
   stored as 02 00.  */
enum sgn_alg {
    SGN_ALG_DES_MAC_MD5           = 0x0000,
    SGN_ALG_MD2_5                 = 0x0001,
    SGN_ALG_DES_MAC               = 0x0002,
    SGN_ALG_3                     = 0x0003, /* not published */
    SGN_ALG_HMAC_MD5              = 0x0011, /* microsoft w2k;  */
    SGN_ALG_HMAC_SHA1_DES3_KD     = 0x0004
};
enum seal_alg {
    SEAL_ALG_NONE            = 0xffff,
    SEAL_ALG_DES             = 0x0000,
    SEAL_ALG_1               = 0x0001, /* not published */
    SEAL_ALG_MICROSOFT_RC4   = 0x0010, /* microsoft w2k;  */
    SEAL_ALG_DES3KD          = 0x0002
};

/* for 3DES */
#define KG_USAGE_SEAL 22
#define KG_USAGE_SIGN 23
#define KG_USAGE_SEQ  24

/* for draft-ietf-krb-wg-gssapi-cfx-01 */
#define KG_USAGE_ACCEPTOR_SEAL  22
#define KG_USAGE_ACCEPTOR_SIGN  23
#define KG_USAGE_INITIATOR_SEAL 24
#define KG_USAGE_INITIATOR_SIGN 25

enum qop {
    GSS_KRB5_INTEG_C_QOP_MD5       = 0x0001, /* *partial* MD5 = "MD2.5" */
    GSS_KRB5_INTEG_C_QOP_DES_MD5   = 0x0002,
    GSS_KRB5_INTEG_C_QOP_DES_MAC   = 0x0003,
    GSS_KRB5_INTEG_C_QOP_HMAC_SHA1 = 0x0004,
    GSS_KRB5_INTEG_C_QOP_MASK      = 0x00ff,
    GSS_KRB5_CONF_C_QOP_DES        = 0x0100,
    GSS_KRB5_CONF_C_QOP_DES3_KD    = 0x0200,
    GSS_KRB5_CONF_C_QOP_MASK       = 0xff00
};

/** internal types **/

typedef struct _krb5_gss_name_rec {
    krb5_principal princ;       /* immutable */
    char *service;              /* immutable */
    char *host;                 /* immutable */
    k5_mutex_t lock;            /* protects ad_context only for now */
    krb5_authdata_context ad_context;
} krb5_gss_name_rec, *krb5_gss_name_t;

typedef struct _krb5_gss_cred_id_rec {
    /* protect against simultaneous accesses */
    k5_mutex_t lock;

    /* name/type of credential */
    gss_cred_usage_t usage;
    krb5_gss_name_t name;
    krb5_principal impersonator;
    unsigned int default_identity : 1;
    unsigned int iakerb_mech : 1;
    unsigned int destroy_ccache : 1;
    unsigned int suppress_ci_flags : 1;

    /* keytab (accept) data */
    krb5_keytab keytab;
    krb5_rcache rcache;

    /* ccache (init) data */
    krb5_ccache ccache;
    krb5_keytab client_keytab;
    krb5_boolean have_tgt;
    krb5_timestamp expire;
    krb5_timestamp refresh_time;
    krb5_enctype *req_enctypes;  /* limit negotiated enctypes to this list */
    char *password;
} krb5_gss_cred_id_rec, *krb5_gss_cred_id_t;

typedef struct _krb5_gss_ctx_ext_rec {
    struct {
        krb5_data *conv;
        int verified;
    } iakerb;
} krb5_gss_ctx_ext_rec, *krb5_gss_ctx_ext_t;

typedef struct _krb5_gss_ctx_id_rec {
    krb5_magic magic;
    unsigned int initiate : 1;   /* nonzero if initiating, zero if accepting */
    unsigned int established : 1;
    unsigned int have_acceptor_subkey : 1;
    unsigned int seed_init : 1;  /* XXX tested but never actually set */
    unsigned int terminated : 1;
    OM_uint32 gss_flags;
    unsigned char seed[16];
    krb5_gss_name_t here;
    krb5_gss_name_t there;
    krb5_key subkey; /* One of two potential keys to use with RFC 4121
                      * packets; this key must always be set. */
    int signalg;
    size_t cksum_size;
    int sealalg;
    krb5_key enc; /* RFC 1964 encryption key; seq xored with a constant
                   * for DES, seq for other RFC 1964 enctypes  */
    krb5_key seq; /* RFC 1964 sequencing key */
    krb5_ticket_times krb_times;
    krb5_flags krb_flags;
    /* XXX these used to be signed.  the old spec is inspecific, and
       the new spec specifies unsigned.  I don't believe that the change
       affects the wire encoding. */
    uint64_t seq_send;
    uint64_t seq_recv;
    g_seqnum_state seqstate;
    krb5_context k5_context;
    krb5_auth_context auth_context;
    gss_OID_desc *mech_used;
    /* Protocol spec revision for sending packets
       0 => RFC 1964 with 3DES and RC4 enhancements
       1 => RFC 4121
       No others defined so far.  It is always permitted to receive
       tokens in RFC 4121 format.  If enc is non-null, receiving RFC
       1964 tokens is permitted.*/
    int proto;
    krb5_cksumtype cksumtype;    /* for "main" subkey */
    krb5_key acceptor_subkey; /* CFX only */
    krb5_cksumtype acceptor_subkey_cksumtype;
    int cred_rcache;             /* did we get rcache from creds? */
    krb5_authdata **authdata;
} krb5_gss_ctx_id_rec, *krb5_gss_ctx_id_t;

extern g_set kg_vdb;

#ifndef LEAN_CLIENT
extern k5_mutex_t gssint_krb5_keytab_lock;
#endif /* LEAN_CLIENT */

/** helper functions **/

OM_uint32 kg_get_defcred
(OM_uint32 *minor_status,
 gss_cred_id_t *cred);

krb5_error_code kg_checksum_channel_bindings
(krb5_context context, gss_channel_bindings_t cb,
 krb5_checksum *cksum);

krb5_error_code kg_make_seq_num (krb5_context context,
                                 krb5_key key,
                                 int direction, krb5_ui_4 seqnum, unsigned char *cksum,
                                 unsigned char *buf);

krb5_error_code kg_get_seq_num (krb5_context context,
                                krb5_key key,
                                unsigned char *cksum, unsigned char *buf, int *direction,
                                krb5_ui_4 *seqnum);

krb5_error_code kg_make_seed (krb5_context context,
                              krb5_key key,
                              unsigned char *seed);

krb5_error_code
kg_setup_keys(krb5_context context,
              krb5_gss_ctx_id_rec *ctx,
              krb5_key subkey,
              krb5_cksumtype *cksumtype);

int kg_confounder_size (krb5_context context, krb5_enctype enctype);

krb5_error_code kg_make_confounder (krb5_context context,
                                    krb5_enctype enctype, unsigned char *buf);

krb5_error_code kg_encrypt (krb5_context context,
                            krb5_key key, int usage,
                            krb5_pointer iv,
                            krb5_const_pointer in,
                            krb5_pointer out,
                            unsigned int length);

/* Encrypt length bytes at ptr in place, with the given key and usage.  If
 * iv is not NULL, use it as the cipher state. */
krb5_error_code kg_encrypt_inplace(krb5_context context, krb5_key key,
                                   int usage, krb5_pointer iv,
                                   krb5_pointer ptr, unsigned int length);

krb5_error_code kg_encrypt_iov (krb5_context context,
                                int proto, int dce_style,
                                size_t ec, size_t rrc,
                                krb5_key key, int usage,
                                krb5_pointer iv,
                                gss_iov_buffer_desc *iov,
                                int iov_count);

krb5_error_code
kg_arcfour_docrypt (const krb5_keyblock *keyblock, int usage,
                    const unsigned char *kd_data, size_t kd_data_len,
                    const unsigned char *input_buf, size_t input_len,
                    unsigned char *output_buf);

krb5_error_code
kg_arcfour_docrypt_iov (krb5_context context,
                        const krb5_keyblock *keyblock, int usage,
                        const unsigned char *kd_data, size_t kd_data_len,
                        gss_iov_buffer_desc *iov,
                        int iov_count);

krb5_error_code kg_decrypt (krb5_context context,
                            krb5_key key,  int usage,
                            krb5_pointer iv,
                            krb5_const_pointer in,
                            krb5_pointer out,
                            unsigned int length);

krb5_error_code kg_decrypt_iov (krb5_context context,
                                int proto, int dce_style,
                                size_t ec, size_t rrc,
                                krb5_key key,  int usage,
                                krb5_pointer iv,
                                gss_iov_buffer_desc *iov,
                                int iov_count);

OM_uint32 kg_seal (OM_uint32 *minor_status,
                   gss_ctx_id_t context_handle,
                   int conf_req_flag,
                   gss_qop_t qop_req,
                   gss_buffer_t input_message_buffer,
                   int *conf_state,
                   gss_buffer_t output_message_buffer,
                   int toktype);

OM_uint32 kg_unseal (OM_uint32 *minor_status,
                     gss_ctx_id_t context_handle,
                     gss_buffer_t input_token_buffer,
                     gss_buffer_t message_buffer,
                     int *conf_state,
                     gss_qop_t *qop_state,
                     int toktype);

OM_uint32 kg_seal_size (OM_uint32 *minor_status,
                        gss_ctx_id_t context_handle,
                        int conf_req_flag,
                        gss_qop_t qop_req,
                        OM_uint32 output_size,
                        OM_uint32 *input_size);

krb5_error_code kg_ctx_size (krb5_context kcontext,
                             krb5_pointer arg,
                             size_t *sizep);

krb5_error_code kg_ctx_externalize (krb5_context kcontext,
                                    krb5_pointer arg,
                                    krb5_octet **buffer,
                                    size_t *lenremain);

krb5_error_code kg_ctx_internalize (krb5_context kcontext,
                                    krb5_pointer *argp,
                                    krb5_octet **buffer,
                                    size_t *lenremain);

OM_uint32 kg_sync_ccache_name (krb5_context context, OM_uint32 *minor_status);

OM_uint32 kg_caller_provided_ccache_name (OM_uint32 *minor_status,
                                          int *out_caller_provided_name);

OM_uint32 kg_get_ccache_name (OM_uint32 *minor_status,
                              const char **out_name);

OM_uint32 kg_set_ccache_name (OM_uint32 *minor_status,
                              const char *name);

/* AEAD */

krb5_error_code gss_krb5int_make_seal_token_v3_iov(krb5_context context,
                           krb5_gss_ctx_id_rec *ctx,
                           int conf_req_flag,
                           int *conf_state,
                           gss_iov_buffer_desc *iov,
                           int iov_count,
                           int toktype);

OM_uint32 gss_krb5int_unseal_v3_iov(krb5_context context,
                          OM_uint32 *minor_status,
                          krb5_gss_ctx_id_rec *ctx,
                          gss_iov_buffer_desc *iov,
                          int iov_count,
                          int *conf_state,
                          gss_qop_t *qop_state,
                          int toktype);

gss_iov_buffer_t kg_locate_iov (gss_iov_buffer_desc *iov,
              int iov_count,
              OM_uint32 type);

gss_iov_buffer_t kg_locate_header_iov(gss_iov_buffer_desc *iov, int iov_count,
                                      int toktype);

void kg_iov_msglen(gss_iov_buffer_desc *iov,
              int iov_count,
              size_t *data_length,
              size_t *assoc_data_length);

void kg_release_iov(gss_iov_buffer_desc *iov,
               int iov_count);

krb5_error_code kg_make_checksum_iov_v1(krb5_context context,
                krb5_cksumtype type,
                size_t token_cksum_len,
                krb5_key seq,
                krb5_key enc, /* for conf len */
                krb5_keyusage sign_usage,
                gss_iov_buffer_desc *iov,
                int iov_count,
                int toktype,
                krb5_checksum *checksum);

krb5_error_code kg_make_checksum_iov_v3(krb5_context context,
                krb5_cksumtype type,
                size_t rrc,
                krb5_key key,
                krb5_keyusage sign_usage,
                gss_iov_buffer_desc *iov,
                int iov_count,
                int toktype);

krb5_error_code kg_verify_checksum_iov_v3(krb5_context context,
                krb5_cksumtype type,
                size_t rrc,
                krb5_key key,
                krb5_keyusage sign_usage,
                gss_iov_buffer_desc *iov,
                int iov_count,
                int toktype,
                krb5_boolean *valid);

OM_uint32 kg_seal_iov (OM_uint32 *minor_status,
            gss_ctx_id_t context_handle,
            int conf_req_flag,
            gss_qop_t qop_req,
            int *conf_state,
            gss_iov_buffer_desc *iov,
            int iov_count,
            int toktype);

OM_uint32 kg_unseal_iov (OM_uint32 *minor_status,
            gss_ctx_id_t context_handle,
            int *conf_state,
            gss_qop_t *qop_state,
            gss_iov_buffer_desc *iov,
            int iov_count,
            int toktype);

OM_uint32 kg_seal_iov_length(OM_uint32 *minor_status,
           gss_ctx_id_t context_handle,
           int conf_req_flag,
           gss_qop_t qop_req,
           int *conf_state,
           gss_iov_buffer_desc *iov,
           int iov_count,
           int toktype);

krb5_cryptotype kg_translate_flag_iov(OM_uint32 type);

OM_uint32 kg_fixup_padding_iov(OM_uint32 *minor_status,
        gss_iov_buffer_desc *iov,
        int iov_count);

krb5_boolean kg_integ_only_iov(gss_iov_buffer_desc *iov, int iov_count);

krb5_error_code kg_allocate_iov(gss_iov_buffer_t iov, size_t size);

krb5_error_code
krb5_to_gss_cred(krb5_context context,
                 krb5_creds *creds,
                 krb5_gss_cred_id_t *out_cred);

krb5_boolean
kg_cred_time_to_refresh(krb5_context context, krb5_gss_cred_id_rec *cred);

void
kg_cred_set_initial_refresh(krb5_context context, krb5_gss_cred_id_rec *cred,
                            krb5_ticket_times *times);

OM_uint32
kg_cred_resolve(OM_uint32 *minor_status, krb5_context context,
                gss_cred_id_t cred_handle, gss_name_t target_name);

/** declarations of internal name mechanism functions **/

OM_uint32 KRB5_CALLCONV krb5_gss_acquire_cred
(OM_uint32*,       /* minor_status */
 gss_name_t,       /* desired_name */
 OM_uint32,        /* time_req */
 gss_OID_set,      /* desired_mechs */
 gss_cred_usage_t, /* cred_usage */
 gss_cred_id_t*,   /* output_cred_handle */
 gss_OID_set*,     /* actual_mechs */
 OM_uint32*        /* time_rec */
);

OM_uint32 KRB5_CALLCONV iakerb_gss_acquire_cred
(OM_uint32*,       /* minor_status */
 gss_name_t,       /* desired_name */
 OM_uint32,        /* time_req */
 gss_OID_set,      /* desired_mechs */
 gss_cred_usage_t, /* cred_usage */
 gss_cred_id_t*,   /* output_cred_handle */
 gss_OID_set*,     /* actual_mechs */
 OM_uint32*        /* time_rec */
);

OM_uint32 KRB5_CALLCONV
krb5_gss_acquire_cred_with_password(
    OM_uint32 *minor_status,
    const gss_name_t desired_name,
    const gss_buffer_t password,
    OM_uint32 time_req,
    const gss_OID_set desired_mechs,
    int cred_usage,
    gss_cred_id_t *output_cred_handle,
    gss_OID_set *actual_mechs,
    OM_uint32 *time_rec);

OM_uint32 KRB5_CALLCONV
iakerb_gss_acquire_cred_with_password(
    OM_uint32 *minor_status,
    const gss_name_t desired_name,
    const gss_buffer_t password,
    OM_uint32 time_req,
    const gss_OID_set desired_mechs,
    int cred_usage,
    gss_cred_id_t *output_cred_handle,
    gss_OID_set *actual_mechs,
    OM_uint32 *time_rec);

OM_uint32 KRB5_CALLCONV krb5_gss_release_cred
(OM_uint32*,       /* minor_status */
 gss_cred_id_t*    /* cred_handle */
);

OM_uint32 KRB5_CALLCONV krb5_gss_init_sec_context
(OM_uint32*,       /* minor_status */
 gss_cred_id_t,    /* claimant_cred_handle */
 gss_ctx_id_t*,    /* context_handle */
 gss_name_t,       /* target_name */
 gss_OID,          /* mech_type */
 OM_uint32,        /* req_flags */
 OM_uint32,        /* time_req */
 gss_channel_bindings_t,
 /* input_chan_bindings */
 gss_buffer_t,     /* input_token */
 gss_OID*,         /* actual_mech_type */
 gss_buffer_t,     /* output_token */
 OM_uint32*,       /* ret_flags */
 OM_uint32*        /* time_rec */
);

OM_uint32 krb5_gss_init_sec_context_ext
(OM_uint32*,       /* minor_status */
 gss_cred_id_t,    /* claimant_cred_handle */
 gss_ctx_id_t*,    /* context_handle */
 gss_name_t,       /* target_name */
 gss_OID,          /* mech_type */
 OM_uint32,        /* req_flags */
 OM_uint32,        /* time_req */
 gss_channel_bindings_t,
 /* input_chan_bindings */
 gss_buffer_t,     /* input_token */
 gss_OID*,         /* actual_mech_type */
 gss_buffer_t,     /* output_token */
 OM_uint32*,       /* ret_flags */
 OM_uint32*,       /* time_rec */
 krb5_gss_ctx_ext_t /* exts */
);

#ifndef LEAN_CLIENT
OM_uint32 KRB5_CALLCONV krb5_gss_accept_sec_context
(OM_uint32*,       /* minor_status */
 gss_ctx_id_t*,    /* context_handle */
 gss_cred_id_t,    /* verifier_cred_handle */
 gss_buffer_t,     /* input_token_buffer */
 gss_channel_bindings_t,
 /* input_chan_bindings */
 gss_name_t*,      /* src_name */
 gss_OID*,         /* mech_type */
 gss_buffer_t,     /* output_token */
 OM_uint32*,       /* ret_flags */
 OM_uint32*,       /* time_rec */
 gss_cred_id_t*    /* delegated_cred_handle */
);

OM_uint32 KRB5_CALLCONV krb5_gss_accept_sec_context_ext
(OM_uint32*,       /* minor_status */
 gss_ctx_id_t*,    /* context_handle */
 gss_cred_id_t,    /* verifier_cred_handle */
 gss_buffer_t,     /* input_token_buffer */
 gss_channel_bindings_t,
 /* input_chan_bindings */
 gss_name_t*,      /* src_name */
 gss_OID*,         /* mech_type */
 gss_buffer_t,     /* output_token */
 OM_uint32*,       /* ret_flags */
 OM_uint32*,       /* time_rec */
 gss_cred_id_t*,   /* delegated_cred_handle */
 krb5_gss_ctx_ext_t/*exts */
);
#endif /* LEAN_CLIENT */

OM_uint32 KRB5_CALLCONV krb5_gss_inquire_sec_context_by_oid
(OM_uint32*,       /* minor_status */
 const gss_ctx_id_t,
 /* context_handle */
 const gss_OID,    /* desired_object */
 gss_buffer_set_t* /* data_set */
);

OM_uint32 KRB5_CALLCONV krb5_gss_set_sec_context_option
(OM_uint32*,       /* minor_status */
 gss_ctx_id_t*,    /* context_handle */
 const gss_OID,    /* desired_object */
 const gss_buffer_t/* value */
);

OM_uint32 KRB5_CALLCONV krb5_gss_process_context_token
(OM_uint32*,       /* minor_status */
 gss_ctx_id_t,     /* context_handle */
 gss_buffer_t      /* token_buffer */
);

OM_uint32 KRB5_CALLCONV krb5_gss_delete_sec_context
(OM_uint32*,       /* minor_status */
 gss_ctx_id_t*,    /* context_handle */
 gss_buffer_t      /* output_token */
);

OM_uint32 KRB5_CALLCONV krb5_gss_context_time
(OM_uint32*,       /* minor_status */
 gss_ctx_id_t,     /* context_handle */
 OM_uint32*        /* time_rec */
);

OM_uint32 KRB5_CALLCONV krb5_gss_display_status
(OM_uint32*,       /* minor_status */
 OM_uint32,        /* status_value */
 int,              /* status_type */
 gss_OID,          /* mech_type */
 OM_uint32*,       /* message_context */
 gss_buffer_t      /* status_string */
);

OM_uint32 KRB5_CALLCONV krb5_gss_indicate_mechs
(OM_uint32*,       /* minor_status */
 gss_OID_set*      /* mech_set */
);

OM_uint32 KRB5_CALLCONV krb5_gss_compare_name
(OM_uint32*,       /* minor_status */
 gss_name_t,       /* name1 */
 gss_name_t,       /* name2 */
 int*              /* name_equal */
);

OM_uint32 KRB5_CALLCONV krb5_gss_display_name
(OM_uint32*,      /* minor_status */
 gss_name_t,      /* input_name */
 gss_buffer_t,    /* output_name_buffer */
 gss_OID*         /* output_name_type */
);


OM_uint32 KRB5_CALLCONV krb5_gss_import_name
(OM_uint32*,       /* minor_status */
 gss_buffer_t,     /* input_name_buffer */
 gss_OID,          /* input_name_type */
 gss_name_t*       /* output_name */
);

OM_uint32 KRB5_CALLCONV krb5_gss_release_name
(OM_uint32*,       /* minor_status */
 gss_name_t*       /* input_name */
);

OM_uint32 KRB5_CALLCONV krb5_gss_inquire_cred
(OM_uint32 *,      /* minor_status */
 gss_cred_id_t,    /* cred_handle */
 gss_name_t *,     /* name */
 OM_uint32 *,      /* lifetime */
 gss_cred_usage_t*,/* cred_usage */
 gss_OID_set *     /* mechanisms */
);

OM_uint32 KRB5_CALLCONV krb5_gss_inquire_context
(OM_uint32*,       /* minor_status */
 gss_ctx_id_t,     /* context_handle */
 gss_name_t*,      /* initiator_name */
 gss_name_t*,      /* acceptor_name */
 OM_uint32*,       /* lifetime_rec */
 gss_OID*,         /* mech_type */
 OM_uint32*,       /* ret_flags */
 int*,             /* locally_initiated */
 int*              /* open */
);

/* New V2 entry points */
OM_uint32 KRB5_CALLCONV krb5_gss_get_mic
(OM_uint32 *,           /* minor_status */
 gss_ctx_id_t,               /* context_handle */
 gss_qop_t,                  /* qop_req */
 gss_buffer_t,               /* message_buffer */
 gss_buffer_t                /* message_token */
);

OM_uint32 KRB5_CALLCONV krb5_gss_get_mic_iov
(OM_uint32 *,                /* minor_status */
 gss_ctx_id_t,               /* context_handle */
 gss_qop_t,                  /* qop_req */
 gss_iov_buffer_desc *,      /* iov */
 int                         /* iov_count */
);

OM_uint32 KRB5_CALLCONV krb5_gss_get_mic_iov_length
(OM_uint32 *,                /* minor_status */
 gss_ctx_id_t,               /* context_handle */
 gss_qop_t,                  /* qop_req */
 gss_iov_buffer_desc *,      /* iov */
 int                         /* iov_count */
);

OM_uint32 KRB5_CALLCONV krb5_gss_verify_mic
(OM_uint32 *,           /* minor_status */
 gss_ctx_id_t,               /* context_handle */
 gss_buffer_t,               /* message_buffer */
 gss_buffer_t,               /* message_token */
 gss_qop_t *                 /* qop_state */
);

OM_uint32 KRB5_CALLCONV krb5_gss_verify_mic_iov
(OM_uint32 *,                /* minor_status */
 gss_ctx_id_t,               /* context_handle */
 gss_qop_t *,                /* qop_state */
 gss_iov_buffer_desc *,      /* iov */
 int                         /* iov_count */
);

OM_uint32 KRB5_CALLCONV krb5_gss_wrap
(OM_uint32 *,           /* minor_status */
 gss_ctx_id_t,               /* context_handle */
 int,                        /* conf_req_flag */
 gss_qop_t,                  /* qop_req */
 gss_buffer_t,               /* input_message_buffer */
 int *,                      /* conf_state */
 gss_buffer_t                /* output_message_buffer */
);

OM_uint32 KRB5_CALLCONV krb5_gss_wrap_iov
(OM_uint32 *,           /* minor_status */
 gss_ctx_id_t,              /* context_handle */
 int,                       /* conf_req_flag */
 gss_qop_t,                 /* qop_req */
 int *,                     /* conf_state */
 gss_iov_buffer_desc *,     /* iov */
 int                        /* iov_count */
);

OM_uint32 KRB5_CALLCONV krb5_gss_wrap_iov_length
(OM_uint32 *,           /* minor_status */
 gss_ctx_id_t,              /* context_handle */
 int,                       /* conf_req_flag */
 gss_qop_t,                 /* qop_req */
 int *,                     /* conf_state */
 gss_iov_buffer_desc *,     /* iov */
 int                        /* iov_count */
);

OM_uint32 KRB5_CALLCONV krb5_gss_unwrap
(OM_uint32 *,           /* minor_status */
 gss_ctx_id_t,               /* context_handle */
 gss_buffer_t,               /* input_message_buffer */
 gss_buffer_t,               /* output_message_buffer */
 int *,                      /* conf_state */
 gss_qop_t *                 /* qop_state */
);

OM_uint32 KRB5_CALLCONV krb5_gss_unwrap_iov
(OM_uint32 *,           /* minor_status */
 gss_ctx_id_t,              /* context_handle */
 int *,                     /* conf_state */
 gss_qop_t *,               /* qop_state */
 gss_iov_buffer_desc *,     /* iov */
 int                        /* iov_count */
);

OM_uint32 KRB5_CALLCONV krb5_gss_wrap_size_limit
(OM_uint32 *,           /* minor_status */
 gss_ctx_id_t,               /* context_handle */
 int,                        /* conf_req_flag */
 gss_qop_t,                  /* qop_req */
 OM_uint32,                  /* req_output_size */
 OM_uint32 *                 /* max_input_size */
);

OM_uint32 KRB5_CALLCONV krb5_gss_import_name_object
(OM_uint32 *,           /* minor_status */
 void *,                     /* input_name */
 gss_OID,                    /* input_name_type */
 gss_name_t *                /* output_name */
);

OM_uint32 KRB5_CALLCONV krb5_gss_export_name_object
(OM_uint32 *,           /* minor_status */
 gss_name_t,                 /* input_name */
 gss_OID,                    /* desired_name_type */
 void * *                    /* output_name */
);

OM_uint32 KRB5_CALLCONV krb5_gss_inquire_cred_by_mech
(OM_uint32  *,          /* minor_status */
 gss_cred_id_t,              /* cred_handle */
 gss_OID,                    /* mech_type */
 gss_name_t *,               /* name */
 OM_uint32 *,                /* initiator_lifetime */
 OM_uint32 *,                /* acceptor_lifetime */
 gss_cred_usage_t *          /* cred_usage */
);
#ifndef LEAN_CLIENT
OM_uint32 KRB5_CALLCONV krb5_gss_export_sec_context
(OM_uint32 *,           /* minor_status */
 gss_ctx_id_t *,             /* context_handle */
 gss_buffer_t                /* interprocess_token */
);

OM_uint32 KRB5_CALLCONV krb5_gss_import_sec_context
(OM_uint32 *,           /* minor_status */
 gss_buffer_t,               /* interprocess_token */
 gss_ctx_id_t *              /* context_handle */
);
#endif /* LEAN_CLIENT */

krb5_error_code krb5_gss_ser_init(krb5_context);

OM_uint32 krb5_gss_release_oid
(OM_uint32 *,           /* minor_status */
 gss_OID *                   /* oid */
);

OM_uint32 KRB5_CALLCONV krb5_gss_internal_release_oid
(OM_uint32 *,           /* minor_status */
 gss_OID *                   /* oid */
);

OM_uint32 KRB5_CALLCONV krb5_gss_inquire_names_for_mech
(OM_uint32 *,           /* minor_status */
 gss_OID,                    /* mechanism */
 gss_OID_set *               /* name_types */
);

OM_uint32 krb5_gss_canonicalize_name
(OM_uint32  *,          /* minor_status */
 const gss_name_t,           /* input_name */
 const gss_OID,              /* mech_type */
 gss_name_t *                /* output_name */
);

OM_uint32 KRB5_CALLCONV krb5_gss_export_name
(OM_uint32  *,          /* minor_status */
 const gss_name_t,           /* input_name */
 gss_buffer_t                /* exported_name */
);

OM_uint32 KRB5_CALLCONV krb5_gss_duplicate_name
(OM_uint32  *,          /* minor_status */
 const gss_name_t,           /* input_name */
 gss_name_t *                /* dest_name */
);

OM_uint32 krb5_gss_validate_cred
(OM_uint32 *,           /* minor_status */
 gss_cred_id_t               /* cred */
);

OM_uint32 KRB5_CALLCONV krb5_gss_acquire_cred_impersonate_name(
    OM_uint32 *,            /* minor_status */
    const gss_cred_id_t,    /* impersonator_cred_handle */
    const gss_name_t,       /* desired_name */
    OM_uint32,              /* time_req */
    const gss_OID_set,      /* desired_mechs */
    gss_cred_usage_t,       /* cred_usage */
    gss_cred_id_t *,        /* output_cred_handle */
    gss_OID_set *,          /* actual_mechs */
    OM_uint32 *);           /* time_rec */

OM_uint32
krb5_gss_validate_cred_1(OM_uint32 * /* minor_status */,
                         gss_cred_id_t /* cred_handle */,
                         krb5_context /* context */);

gss_OID krb5_gss_convert_static_mech_oid(gss_OID oid);

krb5_error_code gss_krb5int_make_seal_token_v3(krb5_context,
                                               krb5_gss_ctx_id_rec *,
                                               const gss_buffer_desc *,
                                               gss_buffer_t,
                                               int, int);

OM_uint32 gss_krb5int_unseal_token_v3(krb5_context *contextptr,
                                      OM_uint32 *minor_status,
                                      krb5_gss_ctx_id_rec *ctx,
                                      unsigned char *ptr,
                                      unsigned int bodysize,
                                      gss_buffer_t message_buffer,
                                      int *conf_state, gss_qop_t *qop_state,
                                      int toktype);

int gss_krb5int_rotate_left (void *ptr, size_t bufsiz, size_t rc);

/* naming_exts.c */
#define KG_INIT_NAME_NO_COPY 0x1

krb5_error_code
kg_init_name(krb5_context context, krb5_principal principal,
             char *service, char *host, krb5_authdata_context ad_context,
             krb5_flags flags, krb5_gss_name_t *name);

krb5_error_code
kg_release_name(krb5_context context, krb5_gss_name_t *name);

krb5_error_code
kg_duplicate_name(krb5_context context, const krb5_gss_name_t src,
                  krb5_gss_name_t *dst);

krb5_boolean
kg_compare_name(krb5_context context,
                krb5_gss_name_t name1,
                krb5_gss_name_t name2);

krb5_boolean
kg_acceptor_princ(krb5_context context, krb5_gss_name_t name,
                  krb5_principal *princ_out);

OM_uint32 KRB5_CALLCONV
krb5_gss_display_name_ext(OM_uint32 *minor_status,
                          gss_name_t name,
                          gss_OID display_as_name_type,
                          gss_buffer_t display_name);

OM_uint32 KRB5_CALLCONV
krb5_gss_inquire_name(OM_uint32 *minor_status,
                      gss_name_t name,
                      int *name_is_MN,
                      gss_OID *MN_mech,
                      gss_buffer_set_t *attrs);

OM_uint32 KRB5_CALLCONV
krb5_gss_get_name_attribute(OM_uint32 *minor_status,
                            gss_name_t name,
                            gss_buffer_t attr,
                            int *authenticated,
                            int *complete,
                            gss_buffer_t value,
                            gss_buffer_t display_value,
                            int *more);

OM_uint32 KRB5_CALLCONV
krb5_gss_set_name_attribute(OM_uint32 *minor_status,
                            gss_name_t name,
                            int complete,
                            gss_buffer_t attr,
                            gss_buffer_t value);

OM_uint32 KRB5_CALLCONV
krb5_gss_delete_name_attribute(OM_uint32 *minor_status,
                               gss_name_t name,
                               gss_buffer_t attr);

OM_uint32 KRB5_CALLCONV
krb5_gss_export_name_composite(OM_uint32 *minor_status,
                               gss_name_t name,
                               gss_buffer_t exp_composite_name);

OM_uint32 KRB5_CALLCONV
krb5_gss_map_name_to_any(OM_uint32 *minor_status,
                         gss_name_t name,
                         int authenticated,
                         gss_buffer_t type_id,
                         gss_any_t *output);

OM_uint32 KRB5_CALLCONV
krb5_gss_release_any_name_mapping(OM_uint32 *minor_status,
                                  gss_name_t name,
                                  gss_buffer_t type_id,
                                  gss_any_t *input);

OM_uint32 KRB5_CALLCONV
krb5_gss_pseudo_random(OM_uint32 *minor_status,
                       gss_ctx_id_t context,
                       int prf_key,
                       const gss_buffer_t prf_in,
                       ssize_t desired_output_len,
                       gss_buffer_t prf_out);

OM_uint32 KRB5_CALLCONV
krb5_gss_store_cred(OM_uint32 *minor_status,
                    gss_cred_id_t input_cred_handle,
                    gss_cred_usage_t cred_usage,
                    const gss_OID desired_mech,
                    OM_uint32 overwrite_cred,
                    OM_uint32 default_cred,
                    gss_OID_set *elements_stored,
                    gss_cred_usage_t *cred_usage_stored);

/* s4u_gss_glue.c */
OM_uint32
kg_compose_deleg_cred(OM_uint32 *minor_status,
                      krb5_gss_cred_id_t impersonator_cred,
                      krb5_creds *subject_creds,
                      OM_uint32 time_req,
                      krb5_gss_cred_id_t *output_cred,
                      OM_uint32 *time_rec,
                      krb5_context context);

/*
 * These take unglued krb5-mech-specific contexts.
 */

#define GSS_KRB5_GET_TKT_FLAGS_OID_LENGTH 11
#define GSS_KRB5_GET_TKT_FLAGS_OID "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x05\x01"

OM_uint32 gss_krb5int_get_tkt_flags
(OM_uint32 *minor_status,
 const gss_ctx_id_t context_handle,
 const gss_OID desired_object,
 gss_buffer_set_t *data_set);

#define GSS_KRB5_COPY_CCACHE_OID_LENGTH 11
#define GSS_KRB5_COPY_CCACHE_OID "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x05\x02"

OM_uint32 gss_krb5int_copy_ccache
(OM_uint32 *minor_status,
 gss_cred_id_t *cred_handle,
 const gss_OID desired_oid,
 const gss_buffer_t value);

#define GSS_KRB5_CCACHE_NAME_OID_LENGTH 11
#define GSS_KRB5_CCACHE_NAME_OID "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x05\x03"

struct krb5_gss_ccache_name_req {
    const char *name;
    const char **out_name;
};

OM_uint32
gss_krb5int_ccache_name(OM_uint32 *minor_status, const gss_OID, const gss_OID,
                        const gss_buffer_t);

#define GSS_KRB5_INQ_SSPI_SESSION_KEY_OID_LENGTH 11
#define GSS_KRB5_INQ_SSPI_SESSION_KEY_OID "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x05\x05"

OM_uint32
gss_krb5int_inq_session_key(OM_uint32 *, const gss_ctx_id_t, const gss_OID, gss_buffer_set_t *);

#define GSS_KRB5_SET_ALLOWABLE_ENCTYPES_OID_LENGTH 11
#define GSS_KRB5_SET_ALLOWABLE_ENCTYPES_OID "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x05\x04"

struct krb5_gss_set_allowable_enctypes_req {
    OM_uint32 num_ktypes;
    krb5_enctype *ktypes;
};

OM_uint32
gss_krb5int_set_allowable_enctypes(OM_uint32 *minor_status,
                                   gss_cred_id_t *cred,
                                   const gss_OID desired_oid,
                                   const gss_buffer_t value);

#define GSS_KRB5_EXPORT_LUCID_SEC_CONTEXT_OID_LENGTH 11
#define GSS_KRB5_EXPORT_LUCID_SEC_CONTEXT_OID "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x05\x06"

OM_uint32
gss_krb5int_export_lucid_sec_context(OM_uint32 *minor_status,
                                     const gss_ctx_id_t context_handle,
                                     const gss_OID desired_object,
                                     gss_buffer_set_t *data_set);

#define GSS_KRB5_FREE_LUCID_SEC_CONTEXT_OID_LENGTH 11
#define GSS_KRB5_FREE_LUCID_SEC_CONTEXT_OID "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x05\x07"

OM_uint32
gss_krb5int_free_lucid_sec_context(OM_uint32 *, const gss_OID,
                                   const gss_OID, gss_buffer_t);

extern k5_mutex_t kg_kdc_flag_mutex;
krb5_error_code krb5_gss_init_context (krb5_context *ctxp);

#define GSS_KRB5_USE_KDC_CONTEXT_OID_LENGTH 11
#define GSS_KRB5_USE_KDC_CONTEXT_OID "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x05\x08"

OM_uint32 krb5int_gss_use_kdc_context(OM_uint32 *, const gss_OID,
                                      const gss_OID, gss_buffer_t);

krb5_error_code krb5_gss_use_kdc_context(void);

#define GSS_KRB5_REGISTER_ACCEPTOR_IDENTITY_OID_LENGTH 11
#define GSS_KRB5_REGISTER_ACCEPTOR_IDENTITY_OID "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x05\x09"

OM_uint32
gss_krb5int_register_acceptor_identity(OM_uint32 *, const gss_OID, const gss_OID, gss_buffer_t);

#define GSS_KRB5_EXTRACT_AUTHZ_DATA_FROM_SEC_CONTEXT_OID_LENGTH 11
#define GSS_KRB5_EXTRACT_AUTHZ_DATA_FROM_SEC_CONTEXT_OID "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x05\x0a"

OM_uint32
gss_krb5int_extract_authz_data_from_sec_context(OM_uint32 *minor_status,
                                                const gss_ctx_id_t context_handle,
                                                const gss_OID desired_object,
                                                gss_buffer_set_t *ad_data);

#define GSS_KRB5_SET_CRED_RCACHE_OID_LENGTH 11
#define GSS_KRB5_SET_CRED_RCACHE_OID "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x05\x0b"

OM_uint32
gss_krb5int_set_cred_rcache(OM_uint32 *, gss_cred_id_t *, const gss_OID, const gss_buffer_t);

#define GSS_KRB5_EXTRACT_AUTHTIME_FROM_SEC_CONTEXT_OID_LENGTH 11
#define GSS_KRB5_EXTRACT_AUTHTIME_FROM_SEC_CONTEXT_OID "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x05\x0c"

OM_uint32
gss_krb5int_extract_authtime_from_sec_context(OM_uint32 *,
                                              const gss_ctx_id_t,
                                              const gss_OID,
                                              gss_buffer_set_t *);

#define GET_SEC_CONTEXT_SASL_SSF_OID_LENGTH 11
#define GET_SEC_CONTEXT_SASL_SSF_OID "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x05\x0f"
OM_uint32
gss_krb5int_sec_context_sasl_ssf(OM_uint32 *, const gss_ctx_id_t,
                                 const gss_OID, gss_buffer_set_t *);

#define GSS_KRB5_IMPORT_CRED_OID_LENGTH 11
#define GSS_KRB5_IMPORT_CRED_OID "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x05\x0d"

struct krb5_gss_import_cred_req {
    krb5_ccache id;
    krb5_principal keytab_principal;
    krb5_keytab keytab;
};

OM_uint32
gss_krb5int_import_cred(OM_uint32 *minor_status,
                        gss_cred_id_t *cred,
                        const gss_OID desired_oid,
                        const gss_buffer_t value);

#ifdef _GSS_STATIC_LINK
int gss_krb5int_lib_init(void);
void gss_krb5int_lib_fini(void);
#endif /* _GSS_STATIC_LINK */

OM_uint32 gss_krb5int_initialize_library(void);
void gss_krb5int_cleanup_library(void);

/* For error message handling.  */
/* Returns a shared string, not a private copy!  */
extern char *
krb5_gss_get_error_message(OM_uint32 minor_code);
extern void
krb5_gss_save_error_string(OM_uint32 minor_code, char *msg);
extern void
krb5_gss_save_error_message(OM_uint32 minor_code, const char *format, ...)
#if !defined(__cplusplus) && (__GNUC__ > 2)
    __attribute__((__format__(__printf__, 2, 3)))
#endif
    ;
    extern void
    krb5_gss_save_error_info(OM_uint32 minor_code, krb5_context ctx);
#define get_error_message krb5_gss_get_error_message
#define save_error_string krb5_gss_save_error_string
#define save_error_message krb5_gss_save_error_message
#ifdef KRB5_KERNEL
/* Error messages aren't needed in the kernel, so reduce dependencies. */
#define save_error_info(x,y)
#else
#define save_error_info krb5_gss_save_error_info
#endif
extern void krb5_gss_delete_error_info(void *p);

/* Prefix concatenated with Kerberos encryption type */
#define GSS_KRB5_SESSION_KEY_ENCTYPE_OID_LENGTH 10
#define GSS_KRB5_SESSION_KEY_ENCTYPE_OID  "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x04"

/* IAKERB */

OM_uint32 KRB5_CALLCONV
iakerb_gss_init_sec_context(OM_uint32 *minor_status,
                            gss_cred_id_t claimant_cred_handle,
                            gss_ctx_id_t *context_handle,
                            gss_name_t target_name,
                            gss_OID mech_type,
                            OM_uint32 req_flags,
                            OM_uint32 time_req,
                            gss_channel_bindings_t input_chan_bindings,
                            gss_buffer_t input_token,
                            gss_OID *actual_mech_type,
                            gss_buffer_t output_token,
                            OM_uint32 *ret_flags,
                            OM_uint32 *time_rec);

OM_uint32 KRB5_CALLCONV
iakerb_gss_accept_sec_context(OM_uint32 *minor_status,
                              gss_ctx_id_t *context_handler,
                              gss_cred_id_t verifier_cred_handle,
                              gss_buffer_t input_token,
                              gss_channel_bindings_t input_chan_bindings,
                              gss_name_t *src_name,
                              gss_OID *mech_type,
                              gss_buffer_t output_token,
                              OM_uint32 *ret_flags,
                              OM_uint32 *time_rec,
                              gss_cred_id_t *delegated_cred_handle);

OM_uint32 KRB5_CALLCONV
iakerb_gss_delete_sec_context(OM_uint32 *minor_status,
                              gss_ctx_id_t *context_handle,
                              gss_buffer_t output_token);

krb5_error_code
iakerb_make_finished(krb5_context context,
                     krb5_key key,
                     const krb5_data *conv,
                     krb5_data **finished);

krb5_error_code
iakerb_verify_finished(krb5_context context,
                       krb5_key key,
                       const krb5_data *conv,
                       const krb5_data *finished);

/*
 * Transfer contents of a krb5_data to a gss_buffer and invalidate the source
 * On unix, this is a simple pointer copy
 * On windows, memory is reallocated and copied.
 */
static inline krb5_error_code
data_to_gss(krb5_data *input_k5data, gss_buffer_t output_buffer)
{
    krb5_error_code code = 0;
    output_buffer->length = input_k5data->length;
#if defined(_WIN32) || defined(DEBUG_GSSALLOC)
    if (output_buffer->length > 0) {
        output_buffer->value = gssalloc_malloc(output_buffer->length);
        if (output_buffer->value)
            memcpy(output_buffer->value, input_k5data->data, output_buffer->length);
        else
            code = ENOMEM;
    } else {
        output_buffer->value = NULL;
    }
    free(input_k5data->data);
#else
    output_buffer->value = input_k5data->data;
#endif
    *input_k5data = empty_data();
    return code;
}

#define KRB5_GSS_EXTS_IAKERB_FINISHED 1


/* Credential store extensions */

#define KRB5_CS_CLI_KEYTAB_URN "client_keytab"
#define KRB5_CS_KEYTAB_URN "keytab"
#define KRB5_CS_CCACHE_URN "ccache"
#define KRB5_CS_RCACHE_URN "rcache"

OM_uint32
kg_value_from_cred_store(gss_const_key_value_set_t cred_store,
                         const char *type, const char **value);

OM_uint32 KRB5_CALLCONV
krb5_gss_acquire_cred_from(
    OM_uint32 *,               /* minor_status */
    const gss_name_t,          /* desired_name */
    OM_uint32,                 /* time_req */
    const gss_OID_set,         /* desired_mechs */
    gss_cred_usage_t,          /* cred_usage */
    gss_const_key_value_set_t, /* cred_store */
    gss_cred_id_t *,           /* output_cred_handle */
    gss_OID_set *,             /* actual_mechs */
    OM_uint32 *);              /* time_rec */

OM_uint32 KRB5_CALLCONV
krb5_gss_store_cred_into(
    OM_uint32 *,               /* minor_status */
    gss_cred_id_t,             /* input_cred_handle */
    gss_cred_usage_t,          /* input_usage */
    const gss_OID,             /* desired_mech */
    OM_uint32,                 /* overwrite_cred */
    OM_uint32,                 /* default_cred */
    gss_const_key_value_set_t, /* cred_store */
    gss_OID_set *,             /* elements_stored */
    gss_cred_usage_t *);       /* cred_usage_stored */

OM_uint32 KRB5_CALLCONV
krb5_gss_export_cred(OM_uint32 *minor_status, gss_cred_id_t cred_handle,
                     gss_buffer_t token);

OM_uint32 KRB5_CALLCONV
krb5_gss_import_cred(OM_uint32 *minor_status, gss_buffer_t token,
                     gss_cred_id_t *cred_handle);

OM_uint32 KRB5_CALLCONV
iakerb_gss_process_context_token(OM_uint32 *minor_status,
                                 const gss_ctx_id_t context_handle,
                                 const gss_buffer_t token_buffer);

OM_uint32 KRB5_CALLCONV
iakerb_gss_context_time(OM_uint32 *minor_status, gss_ctx_id_t context_handle,
                        OM_uint32 *time_rec);

OM_uint32 KRB5_CALLCONV
iakerb_gss_inquire_context(OM_uint32 *minor_status,
                           gss_ctx_id_t context_handle, gss_name_t *src_name,
                           gss_name_t *targ_name, OM_uint32 *lifetime_rec,
                           gss_OID *mech_type, OM_uint32 *ctx_flags,
                           int *locally_initiated, int *opened);

OM_uint32 KRB5_CALLCONV
iakerb_gss_get_mic(OM_uint32 *minor_status, gss_ctx_id_t context_handle,
                   gss_qop_t qop_req, gss_buffer_t message_buffer,
                   gss_buffer_t message_token);

OM_uint32 KRB5_CALLCONV
iakerb_gss_get_mic_iov(OM_uint32 *minor_status, gss_ctx_id_t context_handle,
                       gss_qop_t qop_req, gss_iov_buffer_desc *iov,
                       int iov_count);

OM_uint32 KRB5_CALLCONV
iakerb_gss_get_mic_iov_length(OM_uint32 *minor_status,
                              gss_ctx_id_t context_handle, gss_qop_t qop_req,
                              gss_iov_buffer_desc *iov, int iov_count);

OM_uint32 KRB5_CALLCONV
iakerb_gss_verify_mic(OM_uint32 *minor_status, gss_ctx_id_t context_handle,
                      gss_buffer_t msg_buffer, gss_buffer_t token_buffer,
                      gss_qop_t *qop_state);

OM_uint32 KRB5_CALLCONV
iakerb_gss_verify_mic_iov(OM_uint32 *minor_status, gss_ctx_id_t context_handle,
                          gss_qop_t *qop_state, gss_iov_buffer_desc *iov,
                          int iov_count);

OM_uint32 KRB5_CALLCONV
iakerb_gss_wrap(OM_uint32 *minor_status, gss_ctx_id_t context_handle,
                int conf_req_flag, gss_qop_t qop_req,
                gss_buffer_t input_message_buffer, int *conf_state,
                gss_buffer_t output_message_buffer);

OM_uint32 KRB5_CALLCONV
iakerb_gss_wrap_iov(OM_uint32 *minor_status, gss_ctx_id_t context_handle,
                    int conf_req_flag, gss_qop_t qop_req, int *conf_state,
                    gss_iov_buffer_desc *iov, int iov_count);

OM_uint32 KRB5_CALLCONV
iakerb_gss_wrap_iov_length(OM_uint32 *minor_status,
                           gss_ctx_id_t context_handle, int conf_req_flag,
                           gss_qop_t qop_req, int *conf_state,
                           gss_iov_buffer_desc *iov, int iov_count);

OM_uint32 KRB5_CALLCONV
iakerb_gss_unwrap(OM_uint32 *minor_status, gss_ctx_id_t context_handle,
                  gss_buffer_t input_message_buffer,
                  gss_buffer_t output_message_buffer, int *conf_state,
                  gss_qop_t *qop_state);

OM_uint32 KRB5_CALLCONV
iakerb_gss_unwrap_iov(OM_uint32 *minor_status, gss_ctx_id_t context_handle,
                      int *conf_state, gss_qop_t *qop_state,
                      gss_iov_buffer_desc *iov, int iov_count);

OM_uint32 KRB5_CALLCONV
iakerb_gss_wrap_size_limit(OM_uint32 *minor_status,
                           gss_ctx_id_t context_handle, int conf_req_flag,
                           gss_qop_t qop_req, OM_uint32 req_output_size,
                           OM_uint32 *max_input_size);

#ifndef LEAN_CLIENT
OM_uint32 KRB5_CALLCONV
iakerb_gss_export_sec_context(OM_uint32 *minor_status,
                              gss_ctx_id_t *context_handle,
                              gss_buffer_t interprocess_token);

OM_uint32 KRB5_CALLCONV
iakerb_gss_import_sec_context(OM_uint32 *minor_status,
                              const gss_buffer_t interprocess_token,
                              gss_ctx_id_t *context_handle);
#endif /* LEAN_CLIENT */

OM_uint32 KRB5_CALLCONV
iakerb_gss_inquire_sec_context_by_oid(OM_uint32 *minor_status,
                                      const gss_ctx_id_t context_handle,
                                      const gss_OID desired_object,
                                      gss_buffer_set_t *data_set);

OM_uint32 KRB5_CALLCONV
iakerb_gss_set_sec_context_option(OM_uint32 *minor_status,
                                  gss_ctx_id_t *context_handle,
                                  const gss_OID desired_object,
                                  const gss_buffer_t value);

OM_uint32 KRB5_CALLCONV
iakerb_gss_pseudo_random(OM_uint32 *minor_status, gss_ctx_id_t context_handle,
                         int prf_key, const gss_buffer_t prf_in,
                         ssize_t desired_output_len, gss_buffer_t prf_out);

/* Magic string to identify exported krb5 GSS credentials.  Increment this if
 * the format changes. */
#define CRED_EXPORT_MAGIC "K5C1"

OM_uint32
gss_krb5int_get_cred_impersonator(OM_uint32 *minor_status,
                                  const gss_cred_id_t cred_handle,
                                  const gss_OID desired_object,
                                  gss_buffer_set_t *data_set);

#endif /* _GSSAPIP_KRB5_H_ */
