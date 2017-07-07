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

/*
 * $Id$
 */

#include "gssapiP_krb5.h"

OM_uint32 KRB5_CALLCONV
krb5_gss_compare_name(minor_status, name1, name2, name_equal)
    OM_uint32 *minor_status;
    gss_name_t name1;
    gss_name_t name2;
    int *name_equal;
{
    krb5_context context;
    krb5_error_code code;

    code = krb5_gss_init_context(&context);
    if (code) {
        *minor_status = code;
        return GSS_S_FAILURE;
    }

    *minor_status = 0;
    *name_equal = kg_compare_name(context,
                                  (krb5_gss_name_t)name1,
                                  (krb5_gss_name_t)name2);
    krb5_free_context(context);
    return(GSS_S_COMPLETE);
}
