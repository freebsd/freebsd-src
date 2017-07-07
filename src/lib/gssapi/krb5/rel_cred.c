/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
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

#include "gssapiP_krb5.h"

OM_uint32 KRB5_CALLCONV
krb5_gss_release_cred(minor_status, cred_handle)
    OM_uint32 *minor_status;
    gss_cred_id_t *cred_handle;
{
    krb5_context context;
    krb5_gss_cred_id_t cred;
    krb5_error_code code1, code2, code3;

    code1 = krb5_gss_init_context(&context);
    if (code1) {
        *minor_status = code1;
        return GSS_S_FAILURE;
    }

    if (*cred_handle == GSS_C_NO_CREDENTIAL) {
        *minor_status = 0;
        krb5_free_context(context);
        return(GSS_S_COMPLETE);
    }

    cred = (krb5_gss_cred_id_t)*cred_handle;

    k5_mutex_destroy(&cred->lock);
    /* ignore error destroying mutex */

    if (cred->ccache) {
        if (cred->destroy_ccache)
            code1 = krb5_cc_destroy(context, cred->ccache);
        else
            code1 = krb5_cc_close(context, cred->ccache);
    } else
        code1 = 0;

    if (cred->client_keytab)
        krb5_kt_close(context, cred->client_keytab);

#ifndef LEAN_CLIENT
    if (cred->keytab)
        code2 = krb5_kt_close(context, cred->keytab);
    else
#endif /* LEAN_CLIENT */
        code2 = 0;

    if (cred->rcache)
        code3 = krb5_rc_close(context, cred->rcache);
    else
        code3 = 0;
    if (cred->name)
        kg_release_name(context, &cred->name);

    krb5_free_principal(context, cred->impersonator);

    if (cred->req_enctypes)
        free(cred->req_enctypes);

    if (cred->password != NULL)
        zapfree(cred->password, strlen(cred->password));

    xfree(cred);

    *cred_handle = NULL;

    *minor_status = 0;
    if (code1)
        *minor_status = code1;
    if (code2)
        *minor_status = code2;
    if (code3)
        *minor_status = code3;

    if (*minor_status)
        save_error_info(*minor_status, context);
    krb5_free_context(context);
    return(*minor_status?GSS_S_FAILURE:GSS_S_COMPLETE);
}
