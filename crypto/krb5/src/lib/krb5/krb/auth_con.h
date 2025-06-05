/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#ifndef KRB5_AUTH_CONTEXT
#define KRB5_AUTH_CONTEXT

#include "../rcache/memrcache.h"

struct _krb5_auth_context {
    krb5_magic          magic;
    krb5_address      * remote_addr;
    krb5_address      * remote_port;
    krb5_address      * local_addr;
    krb5_address      * local_port;
    krb5_key            key;
    krb5_key            send_subkey;
    krb5_key            recv_subkey;

    krb5_int32          auth_context_flags;
    krb5_ui_4           remote_seq_number;
    krb5_ui_4           local_seq_number;
    krb5_authenticator *authentp;               /* mk_req, rd_req, mk_rep, ...*/
    krb5_cksumtype      req_cksumtype;          /* mk_safe, ... */
    krb5_cksumtype      safe_cksumtype;         /* mk_safe, ... */
    krb5_data           cstate;                 /* mk_priv, rd_priv only */
    krb5_rcache         rcache;
    k5_memrcache        memrcache;
    krb5_enctype      * permitted_etypes;       /* rd_req */
    krb5_mk_req_checksum_func checksum_func;
    void *checksum_func_data;
    krb5_enctype        negotiated_etype;
    krb5_authdata_context   ad_context;
};


/* Internal auth_context_flags */
#define KRB5_AUTH_CONN_INITIALIZED      0x00010000
#define KRB5_AUTH_CONN_USED_W_MK_REQ    0x00020000
#define KRB5_AUTH_CONN_USED_W_RD_REQ    0x00040000
#define KRB5_AUTH_CONN_SANE_SEQ         0x00080000
#define KRB5_AUTH_CONN_HEIMDAL_SEQ      0x00100000

#endif
