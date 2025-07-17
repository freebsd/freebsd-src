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
#include "com_err.h"

/* XXXX internationalization!! */

static inline int
compare_OM_uint32 (OM_uint32 a, OM_uint32 b)
{
    if (a < b)
        return -1;
    else if (a == b)
        return 0;
    else
        return 1;
}
static inline void
free_string (char *s)
{
    free(s);
}
#include "error_map.h"
#include <stdio.h>
char *get_error_message(OM_uint32 minor_code)
{
    gsserrmap *p = k5_getspecific(K5_KEY_GSS_KRB5_ERROR_MESSAGE);
    char *msg = NULL;
#ifdef DEBUG
    fprintf(stderr, "%s(%lu, p=%p)", __FUNCTION__, (unsigned long) minor_code,
            (void *) p);
#endif
    if (p) {
        char **v = gsserrmap_find(p, minor_code);
        if (v) {
            msg = *v;
#ifdef DEBUG
            fprintf(stderr, " FOUND!");
#endif
        }
    }
    if (msg == 0)
        msg = (char *)error_message((krb5_error_code)minor_code);
#ifdef DEBUG
    fprintf(stderr, " -> %p/%s\n", (void *) msg, msg);
#endif
    return msg;
}
#define save_error_string_nocopy gss_krb5_save_error_string_nocopy
static int save_error_string_nocopy(OM_uint32 minor_code, char *msg)
{
    gsserrmap *p;
    int ret;

#ifdef DEBUG
    fprintf(stderr, "%s(%lu, %s)", __FUNCTION__, (unsigned long) minor_code, msg);
#endif
    p = k5_getspecific(K5_KEY_GSS_KRB5_ERROR_MESSAGE);
    if (!p) {
        p = malloc(sizeof(*p));
        if (p == NULL) {
            ret = 1;
            goto fail;
        }
        if (gsserrmap_init(p) != 0) {
            free(p);
            p = NULL;
            ret = 1;
            goto fail;
        }
        if (k5_setspecific(K5_KEY_GSS_KRB5_ERROR_MESSAGE, p) != 0) {
            gsserrmap_destroy(p);
            free(p);
            p = NULL;
            ret = 1;
            goto fail;
        }
    }
    ret = gsserrmap_replace_or_insert(p, minor_code, msg);
fail:
#ifdef DEBUG
    fprintf(stderr, " p=%p %s\n", (void *)p, ret ? "FAIL" : "SUCCESS");
#endif
    return ret;
}
void save_error_string(OM_uint32 minor_code, char *msg)
{
    char *s = strdup(msg);
    if (s) {
        if (save_error_string_nocopy(minor_code, s) != 0)
            free(s);
    }
}
void save_error_message(OM_uint32 minor_code, const char *format, ...)
{
    char *s;
    int n;
    va_list ap;

    va_start(ap, format);
    n = vasprintf(&s, format, ap);
    va_end(ap);
    if (n >= 0) {
        if (save_error_string_nocopy(minor_code, s) != 0)
            free(s);
    }
}
void krb5_gss_save_error_info(OM_uint32 minor_code, krb5_context ctx)
{
    char *s;

#ifdef DEBUG
    fprintf(stderr, "%s(%lu, ctx=%p)\n", __FUNCTION__,
            (unsigned long) minor_code, (void *)ctx);
#endif
    s = (char *)krb5_get_error_message(ctx, (krb5_error_code)minor_code);
#ifdef DEBUG
    fprintf(stderr, "%s(%lu, ctx=%p) saving: %s\n", __FUNCTION__,
            (unsigned long) minor_code, (void *)ctx, s);
#endif
    save_error_string(minor_code, s);
    /* The get_error_message call above resets the error message in
       ctx.  Put it back, in case we make this call again *sigh*.  */
    k5_setmsg(ctx, (krb5_error_code)minor_code, "%s", s);
    krb5_free_error_message(ctx, s);
}
void krb5_gss_delete_error_info(void *p)
{
    gsserrmap_destroy(p);
    free(p);
}

/**/

OM_uint32 KRB5_CALLCONV
krb5_gss_display_status(minor_status, status_value, status_type,
                        mech_type, message_context, status_string)
    OM_uint32 *minor_status;
    OM_uint32 status_value;
    int status_type;
    gss_OID mech_type;
    OM_uint32 *message_context;
    gss_buffer_t status_string;
{
    status_string->length = 0;
    status_string->value = NULL;

    if ((mech_type != GSS_C_NULL_OID) &&
        !g_OID_equal(gss_mech_krb5, mech_type) &&
        !g_OID_equal(gss_mech_krb5_old, mech_type) &&
        !g_OID_equal(gss_mech_iakerb, mech_type)) {
        *minor_status = 0;
        return(GSS_S_BAD_MECH);
    }

    if (status_type == GSS_C_GSS_CODE) {
        return(g_display_major_status(minor_status, status_value,
                                      message_context, status_string));
    } else if (status_type == GSS_C_MECH_CODE) {
        (void) gss_krb5int_initialize_library();

        if (*message_context) {
            *minor_status = (OM_uint32) G_BAD_MSG_CTX;
            return(GSS_S_FAILURE);
        }

        /* If this fails, there's not much we can do...  */
        if (!g_make_string_buffer(krb5_gss_get_error_message(status_value),
                                  status_string)) {
            *minor_status = ENOMEM;
            return(GSS_S_FAILURE);
        }
        *minor_status = 0;
        return(GSS_S_COMPLETE);
    } else {
        *minor_status = 0;
        return(GSS_S_BAD_STATUS);
    }
}
