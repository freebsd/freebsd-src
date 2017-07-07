/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/kerrs.c - Error message functions */
/*
 * Copyright 2006 Massachusetts Institute of Technology.
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

#ifdef DEBUG
static int error_message_debug = 0;
#ifndef ERROR_MESSAGE_DEBUG
#define ERROR_MESSAGE_DEBUG() (error_message_debug != 0)
#endif
#endif

void KRB5_CALLCONV_C
krb5_set_error_message(krb5_context ctx, krb5_error_code code,
                       const char *fmt, ...)
{
    va_list args;

    if (ctx == NULL)
        return;
    va_start(args, fmt);
#ifdef DEBUG
    if (ERROR_MESSAGE_DEBUG()) {
        fprintf(stderr,
                "krb5_set_error_message(ctx=%p/err=%p, code=%ld, ...)\n",
                ctx, &ctx->err, (long)code);
    }
#endif
    k5_vset_error(&ctx->err, code, fmt, args);
#ifdef DEBUG
    if (ERROR_MESSAGE_DEBUG())
        fprintf(stderr, "->%s\n", ctx->err.msg);
#endif
    va_end(args);
}

void KRB5_CALLCONV
krb5_vset_error_message(krb5_context ctx, krb5_error_code code,
                        const char *fmt, va_list args)
{
#ifdef DEBUG
    if (ERROR_MESSAGE_DEBUG()) {
        fprintf(stderr, "krb5_vset_error_message(ctx=%p, code=%ld, ...)\n",
                ctx, (long)code);
    }
#endif
    if (ctx == NULL)
        return;
    k5_vset_error(&ctx->err, code, fmt, args);
#ifdef DEBUG
    if (ERROR_MESSAGE_DEBUG())
        fprintf(stderr, "->%s\n", ctx->err.msg);
#endif
}

void KRB5_CALLCONV
krb5_prepend_error_message(krb5_context ctx, krb5_error_code code,
                           const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    krb5_vwrap_error_message(ctx, code, code, fmt, ap);
    va_end(ap);
}

void KRB5_CALLCONV
krb5_vprepend_error_message(krb5_context ctx, krb5_error_code code,
                            const char *fmt, va_list ap)
{
    krb5_wrap_error_message(ctx, code, code, fmt, ap);
}

void KRB5_CALLCONV
krb5_wrap_error_message(krb5_context ctx, krb5_error_code old_code,
                        krb5_error_code code, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    krb5_vwrap_error_message(ctx, old_code, code, fmt, ap);
    va_end(ap);
}

void KRB5_CALLCONV
krb5_vwrap_error_message(krb5_context ctx, krb5_error_code old_code,
                         krb5_error_code code, const char *fmt, va_list ap)
{
    const char *prev_msg;
    char *prepend;

    if (ctx == NULL || vasprintf(&prepend, fmt, ap) < 0)
        return;
    prev_msg = k5_get_error(&ctx->err, old_code);
    k5_set_error(&ctx->err, code, "%s: %s", prepend, prev_msg);
    k5_free_error(&ctx->err, prev_msg);
    free(prepend);
}

/* Set the error message state of dest_ctx to that of src_ctx. */
void KRB5_CALLCONV
krb5_copy_error_message(krb5_context dest_ctx, krb5_context src_ctx)
{
    if (dest_ctx == src_ctx)
        return;
    if (src_ctx->err.msg != NULL) {
        k5_set_error(&dest_ctx->err, src_ctx->err.code, "%s",
                     src_ctx->err.msg);
    } else {
        k5_clear_error(&dest_ctx->err);
    }
}

/* Re-format msg using the format string err_fmt.  Return an allocated result,
 * or NULL if err_fmt is NULL or on allocation failure. */
static char *
err_fmt_fmt(const char *err_fmt, long code, const char *msg)
{
    struct k5buf buf;
    const char *p, *s;

    if (err_fmt == NULL)
        return NULL;

    k5_buf_init_dynamic(&buf);

    s = err_fmt;
    while ((p = strchr(s, '%')) != NULL) {
        k5_buf_add_len(&buf, s, p - s);
        s = p;
        if (p[1] == '\0')
            break;
        else if (p[1] == 'M')
            k5_buf_add(&buf, msg);
        else if (p[1] == 'C')
            k5_buf_add_fmt(&buf, "%ld", code);
        else if (p[1] == '%')
            k5_buf_add(&buf, "%");
        else
            k5_buf_add_fmt(&buf, "%%%c", p[1]);
        s += 2;
    }
    k5_buf_add(&buf, s);        /* Remainder after last token */
    return buf.data;
}

const char * KRB5_CALLCONV
krb5_get_error_message(krb5_context ctx, krb5_error_code code)
{
    const char *std, *custom;

#ifdef DEBUG
    if (ERROR_MESSAGE_DEBUG())
        fprintf(stderr, "krb5_get_error_message(%p, %ld)\n", ctx, (long)code);
#endif
    if (ctx == NULL)
        return error_message(code);

    std = k5_get_error(&ctx->err, code);
    custom = err_fmt_fmt(ctx->err_fmt, code, std);
    if (custom != NULL) {
        free((char *)std);
        return custom;
    }
    return std;
}

void KRB5_CALLCONV
krb5_free_error_message(krb5_context ctx, const char *msg)
{
#ifdef DEBUG
    if (ERROR_MESSAGE_DEBUG())
        fprintf(stderr, "krb5_free_error_message(%p, %p)\n", ctx, msg);
#endif
    if (ctx == NULL)
        return;
    k5_free_error(&ctx->err, msg);
}

void KRB5_CALLCONV
krb5_clear_error_message(krb5_context ctx)
{
#ifdef DEBUG
    if (ERROR_MESSAGE_DEBUG())
        fprintf(stderr, "krb5_clear_error_message(%p)\n", ctx);
#endif
    if (ctx == NULL)
        return;
    k5_clear_error(&ctx->err);
}

void
k5_save_ctx_error(krb5_context ctx, krb5_error_code code, struct errinfo *out)
{
    out->code = code;
    out->msg = NULL;
    if (ctx != NULL && ctx->err.code == code) {
        out->msg = ctx->err.msg;
        ctx->err.code = 0;
        ctx->err.msg = NULL;
    }
}

krb5_error_code
k5_restore_ctx_error(krb5_context ctx, struct errinfo *in)
{
    krb5_error_code code = in->code;

    if (ctx != NULL) {
        k5_clear_error(&ctx->err);
        ctx->err.code = in->code;
        ctx->err.msg = in->msg;
        in->msg = NULL;
    } else {
        k5_clear_error(in);
    }
    return code;
}

/* If ctx contains an extended error message for oldcode, change it to be an
 * extended error message for newcode. */
void
k5_change_error_message_code(krb5_context ctx, krb5_error_code oldcode,
                             krb5_error_code newcode)
{
    if (ctx != NULL && ctx->err.msg != NULL && ctx->err.code == oldcode)
        ctx->err.code = newcode;
}
