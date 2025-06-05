/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/gssapi/krb5/lucid_context.c */
/*
 * Copyright 2004, 2008 by the Massachusetts Institute of Technology.
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

/* Externalize a "lucid" security context from a krb5_gss_ctx_id_rec
 * structure. */
#include "gssapiP_krb5.h"
#include "gssapi_krb5.h"

/*
 * Local routine prototypes
 */
static void
free_external_lucid_ctx_v1(
    gss_krb5_lucid_context_v1_t *ctx);

static void
free_lucid_key_data(
    gss_krb5_lucid_key_t *key);

static krb5_error_code
copy_keyblock_to_lucid_key(
    krb5_keyblock *k5key,
    gss_krb5_lucid_key_t *lkey);

static krb5_error_code
make_external_lucid_ctx_v1(
    krb5_gss_ctx_id_rec * gctx,
    int version,
    void **out_ptr);


/*
 * Exported routines
 */

OM_uint32
gss_krb5int_export_lucid_sec_context(
    OM_uint32           *minor_status,
    const gss_ctx_id_t  context_handle,
    const gss_OID       desired_object,
    gss_buffer_set_t    *data_set)
{
    krb5_error_code     kret = 0;
    OM_uint32           retval;
    krb5_gss_ctx_id_t   ctx = (krb5_gss_ctx_id_t)context_handle;
    void                *lctx = NULL;
    int                 version = 0;
    gss_buffer_desc     rep;

    /* Assume failure */
    retval = GSS_S_FAILURE;
    *minor_status = 0;
    *data_set = GSS_C_NO_BUFFER_SET;

    if (ctx->terminated || !ctx->established) {
        *minor_status = KG_CTX_INCOMPLETE;
        return GSS_S_NO_CONTEXT;
    }

    retval = generic_gss_oid_decompose(minor_status,
                                       GSS_KRB5_EXPORT_LUCID_SEC_CONTEXT_OID,
                                       GSS_KRB5_EXPORT_LUCID_SEC_CONTEXT_OID_LENGTH,
                                       desired_object,
                                       &version);
    if (GSS_ERROR(retval))
        return retval;

    /* Externalize a structure of the right version */
    switch (version) {
    case 1:
        kret = make_external_lucid_ctx_v1((krb5_pointer)ctx,
                                          version, &lctx);
        break;
    default:
        kret = (OM_uint32) KG_LUCID_VERSION;
        break;
    }

    if (kret)
        goto error_out;

    rep.value = &lctx;
    rep.length = sizeof(lctx);

    retval = generic_gss_add_buffer_set_member(minor_status, &rep, data_set);
    if (GSS_ERROR(retval))
        goto error_out;

error_out:
    if (*minor_status == 0)
        *minor_status = (OM_uint32) kret;
    return(retval);
}

/*
 * Frees the storage associated with an
 * exported lucid context structure.
 */
OM_uint32
gss_krb5int_free_lucid_sec_context(
    OM_uint32 *minor_status,
    const gss_OID desired_mech,
    const gss_OID desired_object,
    gss_buffer_t value)
{
    OM_uint32           retval;
    krb5_error_code     kret = 0;
    int                 version;
    void                *kctx;

    /* Assume failure */
    retval = GSS_S_FAILURE;
    *minor_status = 0;

    kctx = value->value;
    if (!kctx) {
        kret = EINVAL;
        goto error_out;
    }

    /* Determine version and call correct free routine */
    version = ((gss_krb5_lucid_context_version_t *)kctx)->version;
    switch (version) {
    case 1:
        free_external_lucid_ctx_v1((gss_krb5_lucid_context_v1_t*) kctx);
        break;
    default:
        kret = EINVAL;
        break;
    }

    if (kret)
        goto error_out;

    /* Success! */
    *minor_status = 0;
    retval = GSS_S_COMPLETE;

    return (retval);

error_out:
    if (*minor_status == 0)
        *minor_status = (OM_uint32) kret;
    return(retval);
}

/*
 * Local routines
 */

static krb5_error_code
make_external_lucid_ctx_v1(
    krb5_gss_ctx_id_rec * gctx,
    int version,
    void **out_ptr)
{
    gss_krb5_lucid_context_v1_t *lctx = NULL;
    unsigned int bufsize = sizeof(gss_krb5_lucid_context_v1_t);
    krb5_error_code retval;

    /* Allocate the structure */
    if ((lctx = xmalloc(bufsize)) == NULL) {
        retval = ENOMEM;
        goto error_out;
    }

    memset(lctx, 0, bufsize);

    lctx->version = 1;
    lctx->initiate = gctx->initiate ? 1 : 0;
    lctx->endtime = gctx->krb_times.endtime;
    lctx->send_seq = gctx->seq_send;
    lctx->recv_seq = gctx->seq_recv;
    lctx->protocol = gctx->proto;
    /* gctx->proto == 0 ==> rfc1964-style key information
       gctx->proto == 1 ==> cfx-style (draft-ietf-krb-wg-gssapi-cfx-07) keys */
    if (gctx->proto == 0) {
        lctx->rfc1964_kd.sign_alg = gctx->signalg;
        lctx->rfc1964_kd.seal_alg = gctx->sealalg;
        /* Copy key */
        if ((retval = copy_keyblock_to_lucid_key(&gctx->seq->keyblock,
                                                 &lctx->rfc1964_kd.ctx_key)))
            goto error_out;
    }
    else if (gctx->proto == 1) {
        /* Copy keys */
        /* (subkey is always present, either a copy of the kerberos
           session key or a subkey) */
        if ((retval = copy_keyblock_to_lucid_key(&gctx->subkey->keyblock,
                                                 &lctx->cfx_kd.ctx_key)))
            goto error_out;
        if (gctx->have_acceptor_subkey) {
            if ((retval = copy_keyblock_to_lucid_key(&gctx->acceptor_subkey->keyblock,
                                                     &lctx->cfx_kd.acceptor_subkey)))
                goto error_out;
            lctx->cfx_kd.have_acceptor_subkey = 1;
        }
    }
    else {
        xfree(lctx);
        return EINVAL;  /* XXX better error code? */
    }

    /* Success! */
    *out_ptr = lctx;
    return 0;

error_out:
    if (lctx) {
        free_external_lucid_ctx_v1(lctx);
    }
    return retval;

}

/* Copy the contents of a krb5_keyblock to a gss_krb5_lucid_key_t structure */
static krb5_error_code
copy_keyblock_to_lucid_key(
    krb5_keyblock *k5key,
    gss_krb5_lucid_key_t *lkey)
{
    if (!k5key || !k5key->contents || k5key->length == 0)
        return EINVAL;

    memset(lkey, 0, sizeof(gss_krb5_lucid_key_t));

    /* Allocate storage for the key data */
    if ((lkey->data = xmalloc(k5key->length)) == NULL) {
        return ENOMEM;
    }
    memcpy(lkey->data, k5key->contents, k5key->length);
    lkey->length = k5key->length;
    lkey->type = k5key->enctype;

    return 0;
}


/* Free any storage associated with a gss_krb5_lucid_key_t structure */
static void
free_lucid_key_data(
    gss_krb5_lucid_key_t *key)
{
    if (key) {
        if (key->data && key->length) {
            zap(key->data, key->length);
            xfree(key->data);
            zap(key, sizeof(gss_krb5_lucid_key_t));
        }
    }
}
/* Free any storage associated with a gss_krb5_lucid_context_v1 structure */
static void
free_external_lucid_ctx_v1(
    gss_krb5_lucid_context_v1_t *ctx)
{
    if (ctx) {
        if (ctx->protocol == 0) {
            free_lucid_key_data(&ctx->rfc1964_kd.ctx_key);
        }
        if (ctx->protocol == 1) {
            free_lucid_key_data(&ctx->cfx_kd.ctx_key);
            if (ctx->cfx_kd.have_acceptor_subkey)
                free_lucid_key_data(&ctx->cfx_kd.acceptor_subkey);
        }
        xfree(ctx);
        ctx = NULL;
    }
}
