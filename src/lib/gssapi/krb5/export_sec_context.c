/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/gssapi/krb5/export_sec_context.c - Externalize a security context */
/*
 * Copyright 1995, 2008 by the Massachusetts Institute of Technology.
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

#include "gssapiP_krb5.h"
#ifndef LEAN_CLIENT
OM_uint32 KRB5_CALLCONV
krb5_gss_export_sec_context(minor_status, context_handle, interprocess_token)
    OM_uint32           *minor_status;
    gss_ctx_id_t        *context_handle;
    gss_buffer_t        interprocess_token;
{
    krb5_context        context = NULL;
    krb5_error_code     kret;
    OM_uint32           retval;
    size_t              bufsize, blen;
    krb5_gss_ctx_id_t   ctx;
    krb5_octet          *obuffer, *obp;

    /* Assume a tragic failure */
    obuffer = (krb5_octet *) NULL;
    retval = GSS_S_FAILURE;
    *minor_status = 0;

    ctx = (krb5_gss_ctx_id_t) *context_handle;
    if (ctx->terminated) {
        *minor_status = KG_CTX_INCOMPLETE;
        return (GSS_S_NO_CONTEXT);
    }

    context = ctx->k5_context;
    kret = krb5_gss_ser_init(context);
    if (kret)
        goto error_out;

    /* Determine size needed for externalization of context */
    bufsize = 0;
    if ((kret = kg_ctx_size(context, (krb5_pointer) ctx,
                            &bufsize)))
        goto error_out;

    /* Allocate the buffer */
    if ((obuffer = gssalloc_malloc(bufsize)) == NULL) {
        kret = ENOMEM;
        goto error_out;
    }

    obp = obuffer;
    blen = bufsize;
    /* Externalize the context */
    if ((kret = kg_ctx_externalize(context,
                                   (krb5_pointer) ctx, &obp, &blen)))
        goto error_out;

    /* Success!  Return the buffer */
    interprocess_token->length = bufsize - blen;
    interprocess_token->value = obuffer;
    *minor_status = 0;
    retval = GSS_S_COMPLETE;

    /* Now, clean up the context state */
    (void)krb5_gss_delete_sec_context(minor_status, context_handle, NULL);
    *context_handle = GSS_C_NO_CONTEXT;

    return (GSS_S_COMPLETE);

error_out:
    if (retval != GSS_S_COMPLETE)
        if (kret != 0 && context != 0)
            save_error_info((OM_uint32)kret, context);
    if (obuffer && bufsize) {
        zap(obuffer, bufsize);
        xfree(obuffer);
    }
    if (*minor_status == 0)
        *minor_status = (OM_uint32) kret;
    return(retval);
}
#endif /* LEAN_CLIENT */
