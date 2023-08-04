/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/auth_con.c */
/*
 * Copyright 2010 by the Massachusetts Institute of Technology.
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
 */

#include "k5-int.h"
#include "int-proto.h"
#include "auth_con.h"

krb5_error_code KRB5_CALLCONV
krb5_auth_con_init(krb5_context context, krb5_auth_context *auth_context)
{
    *auth_context =
        (krb5_auth_context)calloc(1, sizeof(struct _krb5_auth_context));
    if (!*auth_context)
        return ENOMEM;

    /* Default flags, do time not seq */
    (*auth_context)->auth_context_flags =
        KRB5_AUTH_CONTEXT_DO_TIME |  KRB5_AUTH_CONN_INITIALIZED;

    (*auth_context)->checksum_func = NULL;
    (*auth_context)->checksum_func_data = NULL;
    (*auth_context)->negotiated_etype = ENCTYPE_NULL;
    (*auth_context)->magic = KV5M_AUTH_CONTEXT;
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_auth_con_free(krb5_context context, krb5_auth_context auth_context)
{
    if (auth_context == NULL)
        return 0;
    if (auth_context->local_addr)
        krb5_free_address(context, auth_context->local_addr);
    if (auth_context->remote_addr)
        krb5_free_address(context, auth_context->remote_addr);
    if (auth_context->local_port)
        krb5_free_address(context, auth_context->local_port);
    if (auth_context->remote_port)
        krb5_free_address(context, auth_context->remote_port);
    if (auth_context->authentp)
        krb5_free_authenticator(context, auth_context->authentp);
    if (auth_context->key)
        krb5_k_free_key(context, auth_context->key);
    if (auth_context->send_subkey)
        krb5_k_free_key(context, auth_context->send_subkey);
    if (auth_context->recv_subkey)
        krb5_k_free_key(context, auth_context->recv_subkey);
    zapfree(auth_context->cstate.data, auth_context->cstate.length);
    if (auth_context->rcache)
        k5_rc_close(context, auth_context->rcache);
    if (auth_context->permitted_etypes)
        free(auth_context->permitted_etypes);
    if (auth_context->ad_context)
        krb5_authdata_context_free(context, auth_context->ad_context);
    k5_memrcache_free(context, auth_context->memrcache);
    free(auth_context);
    return 0;
}

krb5_error_code
krb5_auth_con_setaddrs(krb5_context context, krb5_auth_context auth_context, krb5_address *local_addr, krb5_address *remote_addr)
{
    krb5_error_code     retval;

    /* Free old addresses */
    if (auth_context->local_addr)
        (void) krb5_free_address(context, auth_context->local_addr);
    if (auth_context->remote_addr)
        (void) krb5_free_address(context, auth_context->remote_addr);

    retval = 0;
    if (local_addr)
        retval = krb5_copy_addr(context,
                                local_addr,
                                &auth_context->local_addr);
    else
        auth_context->local_addr = NULL;

    if (!retval && remote_addr)
        retval = krb5_copy_addr(context,
                                remote_addr,
                                &auth_context->remote_addr);
    else
        auth_context->remote_addr = NULL;

    return retval;
}

krb5_error_code KRB5_CALLCONV
krb5_auth_con_getaddrs(krb5_context context, krb5_auth_context auth_context, krb5_address **local_addr, krb5_address **remote_addr)
{
    krb5_error_code     retval;

    retval = 0;
    if (local_addr && auth_context->local_addr) {
        retval = krb5_copy_addr(context,
                                auth_context->local_addr,
                                local_addr);
    }
    if (!retval && (remote_addr) && auth_context->remote_addr) {
        retval = krb5_copy_addr(context,
                                auth_context->remote_addr,
                                remote_addr);
    }
    return retval;
}

krb5_error_code KRB5_CALLCONV
krb5_auth_con_setports(krb5_context context, krb5_auth_context auth_context, krb5_address *local_port, krb5_address *remote_port)
{
    krb5_error_code     retval;

    /* Free old addresses */
    if (auth_context->local_port)
        (void) krb5_free_address(context, auth_context->local_port);
    if (auth_context->remote_port)
        (void) krb5_free_address(context, auth_context->remote_port);

    retval = 0;
    if (local_port)
        retval = krb5_copy_addr(context,
                                local_port,
                                &auth_context->local_port);
    else
        auth_context->local_port = NULL;

    if (!retval && remote_port)
        retval = krb5_copy_addr(context,
                                remote_port,
                                &auth_context->remote_port);
    else
        auth_context->remote_port = NULL;

    return retval;
}


/*
 * This function overloads the keyblock field. It is only useful prior to
 * a krb5_rd_req_decode() call for user to user authentication where the
 * server has the key and needs to use it to decrypt the incoming request.
 * Once decrypted this key is no longer necessary and is then overwritten
 * with the session key sent by the client.
 */
krb5_error_code KRB5_CALLCONV
krb5_auth_con_setuseruserkey(krb5_context context, krb5_auth_context auth_context, krb5_keyblock *keyblock)
{
    if (auth_context->key)
        krb5_k_free_key(context, auth_context->key);
    return(krb5_k_create_key(context, keyblock, &(auth_context->key)));
}

krb5_error_code KRB5_CALLCONV
krb5_auth_con_getkey(krb5_context context, krb5_auth_context auth_context, krb5_keyblock **keyblock)
{
    if (auth_context->key)
        return krb5_k_key_keyblock(context, auth_context->key, keyblock);
    *keyblock = NULL;
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_auth_con_getkey_k(krb5_context context, krb5_auth_context auth_context,
                       krb5_key *key)
{
    krb5_k_reference_key(context, auth_context->key);
    *key = auth_context->key;
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_auth_con_getlocalsubkey(krb5_context context, krb5_auth_context auth_context, krb5_keyblock **keyblock)
{
    return krb5_auth_con_getsendsubkey(context, auth_context, keyblock);
}

krb5_error_code KRB5_CALLCONV
krb5_auth_con_getremotesubkey(krb5_context context, krb5_auth_context auth_context, krb5_keyblock **keyblock)
{
    return krb5_auth_con_getrecvsubkey(context, auth_context, keyblock);
}

krb5_error_code KRB5_CALLCONV
krb5_auth_con_setsendsubkey(krb5_context ctx, krb5_auth_context ac, krb5_keyblock *keyblock)
{
    if (ac->send_subkey != NULL)
        krb5_k_free_key(ctx, ac->send_subkey);
    ac->send_subkey = NULL;
    if (keyblock !=NULL)
        return krb5_k_create_key(ctx, keyblock, &ac->send_subkey);
    else
        return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_auth_con_setsendsubkey_k(krb5_context ctx, krb5_auth_context ac,
                              krb5_key key)
{
    krb5_k_free_key(ctx, ac->send_subkey);
    ac->send_subkey = key;
    krb5_k_reference_key(ctx, key);
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_auth_con_setrecvsubkey(krb5_context ctx, krb5_auth_context ac, krb5_keyblock *keyblock)
{
    if (ac->recv_subkey != NULL)
        krb5_k_free_key(ctx, ac->recv_subkey);
    ac->recv_subkey = NULL;
    if (keyblock != NULL)
        return krb5_k_create_key(ctx, keyblock, &ac->recv_subkey);
    else
        return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_auth_con_setrecvsubkey_k(krb5_context ctx, krb5_auth_context ac,
                              krb5_key key)
{
    krb5_k_free_key(ctx, ac->recv_subkey);
    ac->recv_subkey = key;
    krb5_k_reference_key(ctx, key);
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_auth_con_getsendsubkey(krb5_context ctx, krb5_auth_context ac, krb5_keyblock **keyblock)
{
    if (ac->send_subkey != NULL)
        return krb5_k_key_keyblock(ctx, ac->send_subkey, keyblock);
    *keyblock = NULL;
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_auth_con_getsendsubkey_k(krb5_context ctx, krb5_auth_context ac,
                              krb5_key *key)
{
    krb5_k_reference_key(ctx, ac->send_subkey);
    *key = ac->send_subkey;
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_auth_con_getrecvsubkey(krb5_context ctx, krb5_auth_context ac, krb5_keyblock **keyblock)
{
    if (ac->recv_subkey != NULL)
        return krb5_k_key_keyblock(ctx, ac->recv_subkey, keyblock);
    *keyblock = NULL;
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_auth_con_getrecvsubkey_k(krb5_context ctx, krb5_auth_context ac,
                              krb5_key *key)
{
    krb5_k_reference_key(ctx, ac->recv_subkey);
    *key = ac->recv_subkey;
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_auth_con_set_req_cksumtype(krb5_context context, krb5_auth_context auth_context, krb5_cksumtype cksumtype)
{
    auth_context->req_cksumtype = cksumtype;
    return 0;
}

krb5_error_code
krb5_auth_con_set_safe_cksumtype(krb5_context context, krb5_auth_context auth_context, krb5_cksumtype cksumtype)
{
    auth_context->safe_cksumtype = cksumtype;
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_auth_con_getlocalseqnumber(krb5_context context, krb5_auth_context auth_context, krb5_int32 *seqnumber)
{
    *seqnumber = auth_context->local_seq_number;
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_auth_con_getremoteseqnumber(krb5_context context, krb5_auth_context auth_context, krb5_int32 *seqnumber)
{
    *seqnumber = auth_context->remote_seq_number;
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_auth_con_initivector(krb5_context context, krb5_auth_context auth_context)
{
    if (auth_context->key == NULL)
        return EINVAL;
    return krb5_c_init_state(context, &auth_context->key->keyblock,
                             KRB5_KEYUSAGE_KRB_PRIV_ENCPART,
                             &auth_context->cstate);
}

krb5_error_code
krb5_auth_con_setivector(krb5_context context, krb5_auth_context auth_context, krb5_pointer ivector)
{
    /*
     * This function was part of the pre-1.2.2 API.  Because it aliased the
     * caller's memory into auth_context, and doesn't provide the size of the
     * cipher state, it's inconvenient to support now, so return an error.
     */
    return EINVAL;
}

krb5_error_code
krb5_auth_con_getivector(krb5_context context, krb5_auth_context auth_context, krb5_pointer *ivector)
{
    *ivector = auth_context->cstate.data;
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_auth_con_setflags(krb5_context context, krb5_auth_context auth_context, krb5_int32 flags)
{
    auth_context->auth_context_flags = flags;
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_auth_con_getflags(krb5_context context, krb5_auth_context auth_context, krb5_int32 *flags)
{
    *flags = auth_context->auth_context_flags;
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_auth_con_setrcache(krb5_context context, krb5_auth_context auth_context, krb5_rcache rcache)
{
    auth_context->rcache = rcache;
    return 0;
}

krb5_error_code
krb5_auth_con_getrcache(krb5_context context, krb5_auth_context auth_context, krb5_rcache *rcache)
{
    *rcache = auth_context->rcache;
    return 0;
}

krb5_error_code
krb5_auth_con_setpermetypes(krb5_context context,
                            krb5_auth_context auth_context,
                            const krb5_enctype *permetypes)
{
    krb5_enctype *newpe;
    krb5_error_code ret;

    ret = k5_copy_etypes(permetypes, &newpe);
    if (ret != 0)
        return ret;

    free(auth_context->permitted_etypes);
    auth_context->permitted_etypes = newpe;
    return 0;
}

krb5_error_code
krb5_auth_con_getpermetypes(krb5_context context,
                            krb5_auth_context auth_context,
                            krb5_enctype **permetypes)
{
    *permetypes = NULL;
    if (auth_context->permitted_etypes == NULL)
        return 0;
    return k5_copy_etypes(auth_context->permitted_etypes, permetypes);
}

krb5_error_code KRB5_CALLCONV
krb5_auth_con_set_checksum_func( krb5_context context,
                                 krb5_auth_context  auth_context,
                                 krb5_mk_req_checksum_func func,
                                 void *data)
{
    auth_context->checksum_func = func;
    auth_context->checksum_func_data = data;
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_auth_con_get_checksum_func( krb5_context context,
                                 krb5_auth_context auth_context,
                                 krb5_mk_req_checksum_func *func,
                                 void **data)
{
    *func = auth_context->checksum_func;
    *data = auth_context->checksum_func_data;
    return 0;
}

krb5_error_code
krb5_auth_con_get_subkey_enctype(krb5_context context,
                                 krb5_auth_context auth_context,
                                 krb5_enctype *etype)
{
    *etype = auth_context->negotiated_etype;
    return 0;
}

krb5_error_code
krb5_auth_con_get_authdata_context(krb5_context context,
                                   krb5_auth_context auth_context,
                                   krb5_authdata_context *ad_context)
{
    *ad_context = auth_context->ad_context;
    return 0;
}

krb5_error_code
krb5_auth_con_set_authdata_context(krb5_context context,
                                   krb5_auth_context auth_context,
                                   krb5_authdata_context ad_context)
{
    auth_context->ad_context = ad_context;
    return 0;
}
