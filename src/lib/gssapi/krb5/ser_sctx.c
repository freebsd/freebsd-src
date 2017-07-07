/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/gssapi/krb5/ser_sctx.c - [De]serialization of security context */
/*
 * Copyright 1995, 2004, 2008 by the Massachusetts Institute of Technology.
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
#include "gssapiP_krb5.h"

/*
 * This module contains routines to [de]serialize
 *      krb5_gss_enc_desc and krb5_gss_ctx_id_t.
 * XXX This whole serialization abstraction is unnecessary in a
 * non-messaging environment, which krb5 is.  Someday, this should
 * all get redone without the extra level of indirection. I've done
 * some of this work here, since adding new serializers is an internal
 * krb5 interface, and I won't use those.  There is some more
 * deobfuscation (no longer anonymizing pointers, mostly) which could
 * still be done. --marc
 */

static krb5_error_code
kg_oid_externalize(kcontext, arg, buffer, lenremain)
    krb5_context        kcontext;
    krb5_pointer        arg;
    krb5_octet          **buffer;
    size_t              *lenremain;
{
    gss_OID oid = (gss_OID) arg;
    krb5_error_code err;

    err = krb5_ser_pack_int32(KV5M_GSS_OID, buffer, lenremain);
    if (err)
        return err;
    err = krb5_ser_pack_int32((krb5_int32) oid->length,
                              buffer, lenremain);
    if (err)
        return err;
    err = krb5_ser_pack_bytes((krb5_octet *) oid->elements,
                              oid->length, buffer, lenremain);
    if (err)
        return err;
    err = krb5_ser_pack_int32(KV5M_GSS_OID, buffer, lenremain);
    return err;
}

static krb5_error_code
kg_oid_internalize(kcontext, argp, buffer, lenremain)
    krb5_context        kcontext;
    krb5_pointer        *argp;
    krb5_octet          **buffer;
    size_t              *lenremain;
{
    gss_OID oid;
    krb5_int32 ibuf;
    krb5_octet         *bp;
    size_t             remain;

    bp = *buffer;
    remain = *lenremain;

    /* Read in and check our magic number */
    if (krb5_ser_unpack_int32(&ibuf, &bp, &remain))
        return (EINVAL);

    if (ibuf != KV5M_GSS_OID)
        return (EINVAL);

    oid = (gss_OID) malloc(sizeof(gss_OID_desc));
    if (oid == NULL)
        return ENOMEM;
    if (krb5_ser_unpack_int32(&ibuf, &bp, &remain)) {
        free(oid);
        return EINVAL;
    }
    oid->length = ibuf;
    oid->elements = malloc((size_t)ibuf);
    if (oid->elements == 0) {
        free(oid);
        return ENOMEM;
    }
    if (krb5_ser_unpack_bytes((krb5_octet *) oid->elements,
                              oid->length, &bp, &remain)) {
        free(oid->elements);
        free(oid);
        return EINVAL;
    }

    /* Read in and check our trailing magic number */
    if (krb5_ser_unpack_int32(&ibuf, &bp, &remain)) {
        free(oid->elements);
        free(oid);
        return (EINVAL);
    }

    if (ibuf != KV5M_GSS_OID) {
        free(oid->elements);
        free(oid);
        return (EINVAL);
    }

    *buffer = bp;
    *lenremain = remain;
    *argp = (krb5_pointer) oid;
    return 0;
}

static krb5_error_code
kg_oid_size(kcontext, arg, sizep)
    krb5_context        kcontext;
    krb5_pointer        arg;
    size_t              *sizep;
{
    krb5_error_code kret;
    gss_OID oid;
    size_t required;

    kret = EINVAL;
    if ((oid = (gss_OID) arg)) {
        required = 2*sizeof(krb5_int32); /* For the header and trailer */
        required += sizeof(krb5_int32);
        required += oid->length;

        kret = 0;

        *sizep += required;
    }

    return(kret);
}

static krb5_error_code
kg_seqstate_externalize(kcontext, arg, buffer, lenremain)
    krb5_context        kcontext;
    g_seqnum_state      arg;
    krb5_octet          **buffer;
    size_t              *lenremain;
{
    krb5_error_code err;
    err = krb5_ser_pack_int32(KV5M_GSS_QUEUE, buffer, lenremain);
    if (err == 0)
        err = g_seqstate_externalize(arg, buffer, lenremain);
    if (err == 0)
        err = krb5_ser_pack_int32(KV5M_GSS_QUEUE, buffer, lenremain);
    return err;
}

static krb5_error_code
kg_seqstate_internalize(kcontext, argp, buffer, lenremain)
    krb5_context        kcontext;
    g_seqnum_state      *argp;
    krb5_octet          **buffer;
    size_t              *lenremain;
{
    krb5_int32 ibuf;
    krb5_octet         *bp;
    size_t             remain;
    krb5_error_code    err;

    bp = *buffer;
    remain = *lenremain;

    /* Read in and check our magic number */
    if (krb5_ser_unpack_int32(&ibuf, &bp, &remain))
        return (EINVAL);

    if (ibuf != KV5M_GSS_QUEUE)
        return (EINVAL);

    err = g_seqstate_internalize(argp, &bp, &remain);
    if (err)
        return err;

    /* Read in and check our trailing magic number */
    if (krb5_ser_unpack_int32(&ibuf, &bp, &remain)) {
        g_seqstate_free(*argp);
        return (EINVAL);
    }

    if (ibuf != KV5M_GSS_QUEUE) {
        g_seqstate_free(*argp);
        return (EINVAL);
    }

    *buffer = bp;
    *lenremain = remain;
    return 0;
}

static krb5_error_code
kg_seqstate_size(kcontext, arg, sizep)
    krb5_context        kcontext;
    g_seqnum_state      arg;
    size_t              *sizep;
{
    krb5_error_code kret;
    size_t required;

    kret = EINVAL;
    if (arg) {
        required = 2*sizeof(krb5_int32); /* For the header and trailer */
        g_seqstate_size(arg, &required);

        kret = 0;
        *sizep += required;
    }
    return(kret);
}

/*
 * Determine the size required for this krb5_gss_ctx_id_rec.
 */
krb5_error_code
kg_ctx_size(kcontext, arg, sizep)
    krb5_context        kcontext;
    krb5_pointer        arg;
    size_t              *sizep;
{
    krb5_error_code     kret;
    krb5_gss_ctx_id_rec *ctx;
    size_t              required;

    /*
     * krb5_gss_ctx_id_rec requires:
     *  krb5_int32      for KG_CONTEXT
     *  krb5_int32      for initiate.
     *  krb5_int32      for established.
     *  krb5_int32      for have_acceptor_subkey.
     *  krb5_int32      for seed_init.
     *  krb5_int32      for gss_flags.
     *  sizeof(seed)    for seed
     *  ...             for here
     *  ...             for there
     *  ...             for subkey
     *  krb5_int32      for signalg.
     *  krb5_int32      for cksum_size.
     *  krb5_int32      for sealalg.
     *  ...             for enc
     *  ...             for seq
     *  krb5_int32      for authtime.
     *  krb5_int32      for starttime.
     *  krb5_int32      for endtime.
     *  krb5_int32      for renew_till.
     *  krb5_int32      for flags.
     *  int64_t         for seq_send.
     *  int64_t         for seq_recv.
     *  ...             for seqstate
     *  ...             for auth_context
     *  ...             for mech_used
     *  krb5_int32      for proto
     *  krb5_int32      for cksumtype
     *  ...             for acceptor_subkey
     *  krb5_int32      for acceptor_key_cksumtype
     *  krb5_int32      for cred_rcache
     *  krb5_int32      for number of elements in authdata array
     *  ...             for authdata array
     *  krb5_int32      for trailer.
     */
    kret = EINVAL;
    if ((ctx = (krb5_gss_ctx_id_rec *) arg)) {
        required = 21*sizeof(krb5_int32);
        required += 2*sizeof(int64_t);
        required += sizeof(ctx->seed);

        kret = 0;
        if (!kret && ctx->here)
            kret = krb5_size_opaque(kcontext,
                                    KV5M_PRINCIPAL,
                                    (krb5_pointer) ctx->here->princ,
                                    &required);

        if (!kret && ctx->there)
            kret = krb5_size_opaque(kcontext,
                                    KV5M_PRINCIPAL,
                                    (krb5_pointer) ctx->there->princ,
                                    &required);

        if (!kret && ctx->subkey)
            kret = krb5_size_opaque(kcontext,
                                    KV5M_KEYBLOCK,
                                    (krb5_pointer) &ctx->subkey->keyblock,
                                    &required);

        if (!kret && ctx->enc)
            kret = krb5_size_opaque(kcontext,
                                    KV5M_KEYBLOCK,
                                    (krb5_pointer) &ctx->enc->keyblock,
                                    &required);

        if (!kret && ctx->seq)
            kret = krb5_size_opaque(kcontext,
                                    KV5M_KEYBLOCK,
                                    (krb5_pointer) &ctx->seq->keyblock,
                                    &required);

        if (!kret)
            kret = kg_oid_size(kcontext,
                               (krb5_pointer) ctx->mech_used,
                               &required);

        if (!kret && ctx->seqstate)
            kret = kg_seqstate_size(kcontext, ctx->seqstate, &required);

        if (!kret)
            kret = krb5_size_opaque(kcontext,
                                    KV5M_CONTEXT,
                                    (krb5_pointer) ctx->k5_context,
                                    &required);
        if (!kret)
            kret = krb5_size_opaque(kcontext,
                                    KV5M_AUTH_CONTEXT,
                                    (krb5_pointer) ctx->auth_context,
                                    &required);
        if (!kret && ctx->acceptor_subkey)
            kret = krb5_size_opaque(kcontext,
                                    KV5M_KEYBLOCK, (krb5_pointer)
                                    &ctx->acceptor_subkey->keyblock,
                                    &required);
        if (!kret && ctx->authdata) {
            krb5_int32 i;

            for (i = 0; !kret && ctx->authdata[i]; i++) {
                kret = krb5_size_opaque(kcontext,
                                        KV5M_AUTHDATA,
                                        (krb5_pointer)ctx->authdata[i],
                                        &required);
            }
        }
        if (!kret) {
            krb5_gss_name_t initiator_name;

            initiator_name = ctx->initiate ? ctx->here : ctx->there;

            if (initiator_name && initiator_name->ad_context) {
                kret = krb5_size_opaque(kcontext,
                                        KV5M_AUTHDATA_CONTEXT,
                                        initiator_name->ad_context,
                                        &required);
            }
        }
        *sizep += required;
    }
    return(kret);
}

/*
 * Externalize this krb5_gss_ctx_id_ret.
 */
krb5_error_code
kg_ctx_externalize(kcontext, arg, buffer, lenremain)
    krb5_context        kcontext;
    krb5_pointer        arg;
    krb5_octet          **buffer;
    size_t              *lenremain;
{
    krb5_error_code     kret;
    krb5_gss_ctx_id_rec *ctx;
    size_t              required;
    krb5_octet          *bp;
    size_t              remain;
    krb5int_access kaccess;

    kret = krb5int_accessor (&kaccess, KRB5INT_ACCESS_VERSION);
    if (kret)
        return(kret);

    required = 0;
    bp = *buffer;
    remain = *lenremain;
    kret = EINVAL;
    if ((ctx = (krb5_gss_ctx_id_rec *) arg)) {
        kret = ENOMEM;
        if (!kg_ctx_size(kcontext, arg, &required) &&
            (required <= remain)) {
            /* Our identifier */
            (void) krb5_ser_pack_int32(KG_CONTEXT, &bp, &remain);

            /* Now static data */
            (void) krb5_ser_pack_int32((krb5_int32) ctx->initiate,
                                       &bp, &remain);
            (void) krb5_ser_pack_int32((krb5_int32) ctx->established,
                                       &bp, &remain);
            (void) krb5_ser_pack_int32((krb5_int32) ctx->have_acceptor_subkey,
                                       &bp, &remain);
            (void) krb5_ser_pack_int32((krb5_int32) ctx->seed_init,
                                       &bp, &remain);
            (void) krb5_ser_pack_int32((krb5_int32) ctx->gss_flags,
                                       &bp, &remain);
            (void) krb5_ser_pack_bytes((krb5_octet *) ctx->seed,
                                       sizeof(ctx->seed),
                                       &bp, &remain);
            (void) krb5_ser_pack_int32((krb5_int32) ctx->signalg,
                                       &bp, &remain);
            (void) krb5_ser_pack_int32((krb5_int32) ctx->cksum_size,
                                       &bp, &remain);
            (void) krb5_ser_pack_int32((krb5_int32) ctx->sealalg,
                                       &bp, &remain);
            (void) krb5_ser_pack_int32((krb5_int32) ctx->krb_times.authtime,
                                       &bp, &remain);
            (void) krb5_ser_pack_int32((krb5_int32) ctx->krb_times.starttime,
                                       &bp, &remain);
            (void) krb5_ser_pack_int32((krb5_int32) ctx->krb_times.endtime,
                                       &bp, &remain);
            (void) krb5_ser_pack_int32((krb5_int32) ctx->krb_times.renew_till,
                                       &bp, &remain);
            (void) krb5_ser_pack_int32((krb5_int32) ctx->krb_flags,
                                       &bp, &remain);
            (void) (*kaccess.ser_pack_int64)((int64_t) ctx->seq_send,
                                             &bp, &remain);
            (void) (*kaccess.ser_pack_int64)((int64_t) ctx->seq_recv,
                                             &bp, &remain);

            /* Now dynamic data */
            kret = 0;

            if (!kret && ctx->mech_used)
                kret = kg_oid_externalize(kcontext, ctx->mech_used,
                                          &bp, &remain);

            if (!kret && ctx->here)
                kret = krb5_externalize_opaque(kcontext,
                                               KV5M_PRINCIPAL,
                                               (krb5_pointer) ctx->here->princ,
                                               &bp, &remain);

            if (!kret && ctx->there)
                kret = krb5_externalize_opaque(kcontext,
                                               KV5M_PRINCIPAL,
                                               (krb5_pointer) ctx->there->princ,
                                               &bp, &remain);

            if (!kret && ctx->subkey)
                kret = krb5_externalize_opaque(kcontext,
                                               KV5M_KEYBLOCK, (krb5_pointer)
                                               &ctx->subkey->keyblock,
                                               &bp, &remain);

            if (!kret && ctx->enc)
                kret = krb5_externalize_opaque(kcontext,
                                               KV5M_KEYBLOCK, (krb5_pointer)
                                               &ctx->enc->keyblock,
                                               &bp, &remain);

            if (!kret && ctx->seq)
                kret = krb5_externalize_opaque(kcontext,
                                               KV5M_KEYBLOCK, (krb5_pointer)
                                               &ctx->seq->keyblock,
                                               &bp, &remain);

            if (!kret && ctx->seqstate)
                kret = kg_seqstate_externalize(kcontext,
                                               ctx->seqstate, &bp, &remain);

            if (!kret)
                kret = krb5_externalize_opaque(kcontext,
                                               KV5M_CONTEXT,
                                               (krb5_pointer) ctx->k5_context,
                                               &bp, &remain);

            if (!kret)
                kret = krb5_externalize_opaque(kcontext,
                                               KV5M_AUTH_CONTEXT,
                                               (krb5_pointer) ctx->auth_context,
                                               &bp, &remain);

            if (!kret)
                kret = krb5_ser_pack_int32((krb5_int32) ctx->proto,
                                           &bp, &remain);
            if (!kret)
                kret = krb5_ser_pack_int32((krb5_int32) ctx->cksumtype,
                                           &bp, &remain);
            if (!kret && ctx->acceptor_subkey)
                kret = krb5_externalize_opaque(kcontext,
                                               KV5M_KEYBLOCK, (krb5_pointer)
                                               &ctx->acceptor_subkey->keyblock,
                                               &bp, &remain);
            if (!kret)
                kret = krb5_ser_pack_int32((krb5_int32) ctx->acceptor_subkey_cksumtype,
                                           &bp, &remain);

            if (!kret)
                kret = krb5_ser_pack_int32((krb5_int32) ctx->cred_rcache,
                                           &bp, &remain);
            if (!kret) {
                krb5_int32 i = 0;

                if (ctx->authdata) {
                    for (; ctx->authdata[i]; i++)
                        ;
                }
                /* authdata count */
                kret = krb5_ser_pack_int32(i, &bp, &remain);
                if (!kret && ctx->authdata) {
                    /* authdata */
                    for (i = 0; !kret && ctx->authdata[i]; i++)
                        kret = krb5_externalize_opaque(kcontext,
                                                       KV5M_AUTHDATA,
                                                       ctx->authdata[i],
                                                       &bp,
                                                       &remain);
                }
            }
            /* authdata context */
            if (!kret) {
                krb5_gss_name_t initiator_name;

                initiator_name = ctx->initiate ? ctx->here : ctx->there;

                if (initiator_name && initiator_name->ad_context) {
                    kret = krb5_externalize_opaque(kcontext,
                                                   KV5M_AUTHDATA_CONTEXT,
                                                   initiator_name->ad_context,
                                                   &bp,
                                                   &remain);
                }
            }
            /* trailer */
            if (!kret)
                kret = krb5_ser_pack_int32(KG_CONTEXT, &bp, &remain);
            if (!kret) {
                *buffer = bp;
                *lenremain = remain;
            }
        }
    }
    return(kret);
}

/* Internalize a keyblock and convert it to a key. */
static krb5_error_code
intern_key(krb5_context ctx, krb5_key *key, krb5_octet **bp, size_t *sp)
{
    krb5_keyblock *keyblock;
    krb5_error_code ret;

    ret = krb5_internalize_opaque(ctx, KV5M_KEYBLOCK,
                                  (krb5_pointer *) &keyblock, bp, sp);
    if (ret != 0)
        return ret;
    ret = krb5_k_create_key(ctx, keyblock, key);
    krb5_free_keyblock(ctx, keyblock);
    return ret;
}

/*
 * Internalize this krb5_gss_ctx_id_t.
 */
krb5_error_code
kg_ctx_internalize(kcontext, argp, buffer, lenremain)
    krb5_context        kcontext;
    krb5_pointer        *argp;
    krb5_octet          **buffer;
    size_t              *lenremain;
{
    krb5_error_code     kret;
    krb5_gss_ctx_id_rec *ctx;
    krb5_int32          ibuf;
    krb5_octet          *bp;
    size_t              remain;
    krb5int_access kaccess;
    krb5_principal        princ;

    kret = krb5int_accessor (&kaccess, KRB5INT_ACCESS_VERSION);
    if (kret)
        return(kret);

    bp = *buffer;
    remain = *lenremain;
    kret = EINVAL;
    princ = NULL;
    /* Read our magic number */
    if (krb5_ser_unpack_int32(&ibuf, &bp, &remain))
        ibuf = 0;
    if (ibuf == KG_CONTEXT) {
        kret = ENOMEM;

        /* Get a context */
        if ((remain >= (17*sizeof(krb5_int32)
                        + 2*sizeof(int64_t)
                        + sizeof(ctx->seed))) &&
            (ctx = (krb5_gss_ctx_id_rec *)
             xmalloc(sizeof(krb5_gss_ctx_id_rec)))) {
            memset(ctx, 0, sizeof(krb5_gss_ctx_id_rec));

            ctx->magic = ibuf;
            ctx->k5_context = kcontext;

            /* Get static data */
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            ctx->initiate = (int) ibuf;
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            ctx->established = (int) ibuf;
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            ctx->have_acceptor_subkey = (int) ibuf;
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            ctx->seed_init = (int) ibuf;
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            ctx->gss_flags = (int) ibuf;
            (void) krb5_ser_unpack_bytes((krb5_octet *) ctx->seed,
                                         sizeof(ctx->seed),
                                         &bp, &remain);
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            ctx->signalg = (int) ibuf;
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            ctx->cksum_size = (int) ibuf;
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            ctx->sealalg = (int) ibuf;
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            ctx->krb_times.authtime = (krb5_timestamp) ibuf;
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            ctx->krb_times.starttime = (krb5_timestamp) ibuf;
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            ctx->krb_times.endtime = (krb5_timestamp) ibuf;
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            ctx->krb_times.renew_till = (krb5_timestamp) ibuf;
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            ctx->krb_flags = (krb5_flags) ibuf;
            (void) (*kaccess.ser_unpack_int64)((int64_t *)&ctx->seq_send,
                                               &bp, &remain);
            kret = (*kaccess.ser_unpack_int64)((int64_t *)&ctx->seq_recv,
                                               &bp, &remain);
            if (kret) {
                free(ctx);
                return kret;
            }

            {
                krb5_pointer tmp;
                kret = kg_oid_internalize(kcontext, &tmp, &bp,
                                          &remain);
                if (kret == 0)
                    ctx->mech_used = tmp;
                else if (kret == EINVAL)
                    kret = 0;
            }
            /* Now get substructure data */
            kret = krb5_internalize_opaque(kcontext,
                                           KV5M_PRINCIPAL,
                                           (krb5_pointer *) &princ,
                                           &bp, &remain);
            if (kret == 0) {
                kret = kg_init_name(kcontext, princ, NULL, NULL, NULL,
                                    KG_INIT_NAME_NO_COPY, &ctx->here);
                if (kret)
                    krb5_free_principal(kcontext, princ);
            } else if (kret == EINVAL)
                kret = 0;
            if (!kret) {
                kret = krb5_internalize_opaque(kcontext,
                                               KV5M_PRINCIPAL,
                                               (krb5_pointer *) &princ,
                                               &bp, &remain);
                if (kret == 0) {
                    kret = kg_init_name(kcontext, princ, NULL, NULL, NULL,
                                        KG_INIT_NAME_NO_COPY, &ctx->there);
                    if (kret)
                        krb5_free_principal(kcontext, princ);
                } else if (kret == EINVAL)
                    kret = 0;
            }
            if (!kret &&
                (kret = intern_key(kcontext, &ctx->subkey, &bp, &remain))) {
                if (kret == EINVAL)
                    kret = 0;
            }
            if (!kret &&
                (kret = intern_key(kcontext, &ctx->enc, &bp, &remain))) {
                if (kret == EINVAL)
                    kret = 0;
            }
            if (!kret &&
                (kret = intern_key(kcontext, &ctx->seq, &bp, &remain))) {
                if (kret == EINVAL)
                    kret = 0;
            }

            if (!kret) {
                kret = kg_seqstate_internalize(kcontext, &ctx->seqstate,
                                               &bp, &remain);
                if (kret == EINVAL)
                    kret = 0;
            }

            if (!kret)
                kret = krb5_internalize_opaque(kcontext,
                                               KV5M_CONTEXT,
                                               (krb5_pointer *) &ctx->k5_context,
                                               &bp, &remain);

            if (!kret)
                kret = krb5_internalize_opaque(kcontext,
                                               KV5M_AUTH_CONTEXT,
                                               (krb5_pointer *) &ctx->auth_context,
                                               &bp, &remain);

            if (!kret)
                kret = krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            ctx->proto = ibuf;
            if (!kret)
                kret = krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            ctx->cksumtype = ibuf;
            if (!kret &&
                (kret = intern_key(kcontext, &ctx->acceptor_subkey,
                                   &bp, &remain))) {
                if (kret == EINVAL)
                    kret = 0;
            }
            if (!kret)
                kret = krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            ctx->acceptor_subkey_cksumtype = ibuf;
            if (!kret)
                kret = krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            ctx->cred_rcache = ibuf;
            /* authdata */
            if (!kret)
                kret = krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            if (!kret) {
                krb5_int32 nadata = ibuf, i;

                if (nadata > 0) {
                    ctx->authdata = (krb5_authdata **)calloc((size_t)nadata + 1,
                                                             sizeof(krb5_authdata *));
                    if (ctx->authdata == NULL) {
                        kret = ENOMEM;
                    } else {
                        for (i = 0; !kret && i < nadata; i++)
                            kret = krb5_internalize_opaque(kcontext,
                                                           KV5M_AUTHDATA,
                                                           (krb5_pointer *)&ctx->authdata[i],
                                                           &bp,
                                                           &remain);
                    }
                }
            }
            /* authdata context */
            if (!kret) {
                krb5_gss_name_t initiator_name;

                initiator_name = ctx->initiate ? ctx->here : ctx->there;
                if (initiator_name == NULL) {
                    kret = EINVAL;
                } else {
                    kret = krb5_internalize_opaque(kcontext,
                                                   KV5M_AUTHDATA_CONTEXT,
                                                   (krb5_pointer *)&initiator_name->ad_context,
                                                   &bp,
                                                   &remain);
                    if (kret == EINVAL)
                        kret = 0;
                }
            }
            /* Get trailer */
            if (!kret)
                kret = krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            if (!kret && ibuf != KG_CONTEXT)
                kret = EINVAL;

            if (!kret) {
                *buffer = bp;
                *lenremain = remain;
                *argp = (krb5_pointer) ctx;
            } else {
                if (ctx->seq)
                    krb5_k_free_key(kcontext, ctx->seq);
                if (ctx->enc)
                    krb5_k_free_key(kcontext, ctx->enc);
                if (ctx->subkey)
                    krb5_k_free_key(kcontext, ctx->subkey);
                if (ctx->there)
                    kg_release_name(kcontext, &ctx->there);
                if (ctx->here)
                    kg_release_name(kcontext, &ctx->here);
                xfree(ctx);
            }
        }
    }
    return(kret);
}
