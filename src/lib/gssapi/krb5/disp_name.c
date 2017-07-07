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
krb5_gss_display_name(minor_status, input_name, output_name_buffer,
                      output_name_type)
    OM_uint32 *minor_status;
    gss_name_t input_name;
    gss_buffer_t output_name_buffer;
    gss_OID *output_name_type;
{
    krb5_context context;
    krb5_error_code code;
    char *str;
    krb5_gss_name_t k5name = (krb5_gss_name_t) input_name;
    gss_OID nametype = (gss_OID) gss_nt_krb5_name;

    output_name_buffer->length = 0;
    output_name_buffer->value = NULL;
    if (output_name_type)
        *output_name_type = GSS_C_NO_OID;

    code = krb5_gss_init_context(&context);
    if (code) {
        *minor_status = code;
        return GSS_S_FAILURE;
    }

    if (krb5_princ_type(context, k5name->princ) == KRB5_NT_WELLKNOWN) {
        if (krb5_principal_compare(context, k5name->princ,
                                   krb5_anonymous_principal()))
            nametype = GSS_C_NT_ANONYMOUS;
    }

    if ((code = krb5_unparse_name(context,
                                  ((krb5_gss_name_t) input_name)->princ,
                                  &str))) {
        *minor_status = code;
        save_error_info(*minor_status, context);
        krb5_free_context(context);
        return(GSS_S_FAILURE);
    }

    if (! g_make_string_buffer(str, output_name_buffer)) {
        krb5_free_unparsed_name(context, str);
        krb5_free_context(context);

        *minor_status = (OM_uint32) G_BUFFER_ALLOC;
        return(GSS_S_FAILURE);
    }

    krb5_free_unparsed_name(context, str);
    krb5_free_context(context);

    *minor_status = 0;
    if (output_name_type)
        *output_name_type = (gss_OID) nametype;
    return(GSS_S_COMPLETE);
}
