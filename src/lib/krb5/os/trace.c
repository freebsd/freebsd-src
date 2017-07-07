/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/trace.c - krb5int_trace implementation */
/*
 * Copyright 2009 by the Massachusetts Institute of Technology.
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

/*
 * krb5int_trace is defined in k5-trace.h as a macro or static inline
 * function, and is called like so:
 *
 *   void krb5int_trace(krb5_context context, const char *fmt, ...)
 *
 * Arguments may or may not be evaluated, so don't pass argument
 * expressions with side effects.  Tracing support and calls can be
 * explicitly compiled out with DISABLE_TRACING, but compile-time
 * support is enabled by default.  Tracing calls use a custom
 * formatter supporting the following format specifications:
 */

#include "k5-int.h"
#include "os-proto.h"

#ifndef DISABLE_TRACING

static void subfmt(krb5_context context, struct k5buf *buf,
                   const char *fmt, ...);

static krb5_boolean
buf_is_printable(const char *p, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        if (p[i] < 32 || p[i] > 126)
            break;
    }
    return i == len;
}

static void
buf_add_printable_len(struct k5buf *buf, const char *p, size_t len)
{
    char text[5];
    size_t i;

    if (buf_is_printable(p, len)) {
        k5_buf_add_len(buf, p, len);
    } else {
        for (i = 0; i < len; i++) {
            if (buf_is_printable(p + i, 1)) {
                k5_buf_add_len(buf, p + i, 1);
            } else {
                snprintf(text, sizeof(text), "\\x%02x",
                         (unsigned)(p[i] & 0xff));
                k5_buf_add_len(buf, text, 4);
            }
        }
    }
}

static void
buf_add_printable(struct k5buf *buf, const char *p)
{
    buf_add_printable_len(buf, p, strlen(p));
}

/* Return a four-byte hex string from the first two bytes of a SHA-1 hash of a
 * byte array.  Return NULL on failure. */
static char *
hash_bytes(krb5_context context, const void *ptr, size_t len)
{
    krb5_checksum cksum;
    krb5_data d = make_data((void *) ptr, len);
    char *s = NULL;

    if (krb5_k_make_checksum(context, CKSUMTYPE_NIST_SHA, NULL, 0, &d,
                             &cksum) != 0)
        return NULL;
    if (cksum.length >= 2)
        (void) asprintf(&s, "%02X%02X", cksum.contents[0], cksum.contents[1]);
    krb5_free_checksum_contents(context, &cksum);
    return s;
}

static char *
principal_type_string(krb5_int32 type)
{
    switch (type) {
    case KRB5_NT_UNKNOWN: return "unknown";
    case KRB5_NT_PRINCIPAL: return "principal";
    case KRB5_NT_SRV_INST: return "service instance";
    case KRB5_NT_SRV_HST: return "service with host as instance";
    case KRB5_NT_SRV_XHST: return "service with host as components";
    case KRB5_NT_UID: return "unique ID";
    case KRB5_NT_X500_PRINCIPAL: return "X.509";
    case KRB5_NT_SMTP_NAME: return "SMTP email";
    case KRB5_NT_ENTERPRISE_PRINCIPAL: return "Windows 2000 UPN";
    case KRB5_NT_WELLKNOWN: return "well-known";
    case KRB5_NT_MS_PRINCIPAL: return "Windows 2000 UPN and SID";
    case KRB5_NT_MS_PRINCIPAL_AND_ID: return "NT 4 style name";
    case KRB5_NT_ENT_PRINCIPAL_AND_ID: return "NT 4 style name and SID";
    default: return "?";
    }
}

static char *
trace_format(krb5_context context, const char *fmt, va_list ap)
{
    struct k5buf buf;
    krb5_error_code kerr;
    size_t len, i;
    int err;
    struct remote_address *ra;
    const krb5_data *d;
    krb5_data data;
    char addrbuf[NI_MAXHOST], portbuf[NI_MAXSERV], tmpbuf[200], *str;
    const char *p;
    krb5_const_principal princ;
    const krb5_keyblock *keyblock;
    krb5_key key;
    const krb5_checksum *cksum;
    krb5_pa_data **padata;
    krb5_ccache ccache;
    krb5_keytab keytab;
    krb5_creds *creds;
    krb5_enctype *etypes, etype;

    k5_buf_init_dynamic(&buf);
    while (TRUE) {
        /* Advance to the next word in braces. */
        len = strcspn(fmt, "{");
        k5_buf_add_len(&buf, fmt, len);
        if (fmt[len] == '\0')
            break;
        fmt += len + 1;
        len = strcspn(fmt, "}");
        if (fmt[len] == '\0' || len > sizeof(tmpbuf) - 1)
            break;
        memcpy(tmpbuf, fmt, len);
        tmpbuf[len] = '\0';
        fmt += len + 1;

        /* Process the format word. */
        if (strcmp(tmpbuf, "int") == 0) {
            k5_buf_add_fmt(&buf, "%d", va_arg(ap, int));
        } else if (strcmp(tmpbuf, "long") == 0) {
            k5_buf_add_fmt(&buf, "%ld", va_arg(ap, long));
        } else if (strcmp(tmpbuf, "str") == 0) {
            p = va_arg(ap, const char *);
            buf_add_printable(&buf, (p == NULL) ? "(null)" : p);
        } else if (strcmp(tmpbuf, "lenstr") == 0) {
            len = va_arg(ap, size_t);
            p = va_arg(ap, const char *);
            if (p == NULL && len != 0)
                k5_buf_add(&buf, "(null)");
            else
                buf_add_printable_len(&buf, p, len);
        } else if (strcmp(tmpbuf, "hexlenstr") == 0) {
            len = va_arg(ap, size_t);
            p = va_arg(ap, const char *);
            if (p == NULL && len != 0)
                k5_buf_add(&buf, "(null)");
            else {
                for (i = 0; i < len; i++)
                    k5_buf_add_fmt(&buf, "%02X", (unsigned char)p[i]);
            }
        } else if (strcmp(tmpbuf, "hashlenstr") == 0) {
            len = va_arg(ap, size_t);
            p = va_arg(ap, const char *);
            if (p == NULL && len != 0)
                k5_buf_add(&buf, "(null)");
            else {
                str = hash_bytes(context, p, len);
                if (str != NULL)
                    k5_buf_add(&buf, str);
                free(str);
            }
        } else if (strcmp(tmpbuf, "raddr") == 0) {
            ra = va_arg(ap, struct remote_address *);
            if (ra->transport == UDP)
                k5_buf_add(&buf, "dgram");
            else if (ra->transport == TCP)
                k5_buf_add(&buf, "stream");
            else if (ra->transport == HTTPS)
                k5_buf_add(&buf, "https");
            else
                k5_buf_add_fmt(&buf, "transport%d", ra->transport);

            if (getnameinfo((struct sockaddr *)&ra->saddr, ra->len,
                            addrbuf, sizeof(addrbuf), portbuf, sizeof(portbuf),
                            NI_NUMERICHOST|NI_NUMERICSERV) != 0) {
                if (ra->family == AF_UNSPEC)
                    k5_buf_add(&buf, " AF_UNSPEC");
                else
                    k5_buf_add_fmt(&buf, " af%d", ra->family);
            } else
                k5_buf_add_fmt(&buf, " %s:%s", addrbuf, portbuf);
        } else if (strcmp(tmpbuf, "data") == 0) {
            d = va_arg(ap, krb5_data *);
            if (d == NULL || (d->length != 0 && d->data == NULL))
                k5_buf_add(&buf, "(null)");
            else
                buf_add_printable_len(&buf, d->data, d->length);
        } else if (strcmp(tmpbuf, "hexdata") == 0) {
            d = va_arg(ap, krb5_data *);
            if (d == NULL)
                k5_buf_add(&buf, "(null)");
            else
                subfmt(context, &buf, "{hexlenstr}", d->length, d->data);
        } else if (strcmp(tmpbuf, "errno") == 0) {
            err = va_arg(ap, int);
            k5_buf_add_fmt(&buf, "%d", err);
            if (strerror_r(err, tmpbuf, sizeof(tmpbuf)) == 0)
                k5_buf_add_fmt(&buf, "/%s", tmpbuf);
        } else if (strcmp(tmpbuf, "kerr") == 0) {
            kerr = va_arg(ap, krb5_error_code);
            p = krb5_get_error_message(context, kerr);
            k5_buf_add_fmt(&buf, "%ld/%s", (long)kerr, kerr ? p : "Success");
            krb5_free_error_message(context, p);
        } else if (strcmp(tmpbuf, "keyblock") == 0) {
            keyblock = va_arg(ap, const krb5_keyblock *);
            if (keyblock == NULL)
                k5_buf_add(&buf, "(null)");
            else {
                subfmt(context, &buf, "{etype}/{hashlenstr}",
                       keyblock->enctype, keyblock->length,
                       keyblock->contents);
            }
        } else if (strcmp(tmpbuf, "key") == 0) {
            key = va_arg(ap, krb5_key);
            if (key == NULL)
                k5_buf_add(&buf, "(null)");
            else
                subfmt(context, &buf, "{keyblock}", &key->keyblock);
        } else if (strcmp(tmpbuf, "cksum") == 0) {
            cksum = va_arg(ap, const krb5_checksum *);
            data = make_data(cksum->contents, cksum->length);
            subfmt(context, &buf, "{int}/{hexdata}",
                   (int) cksum->checksum_type, &data);
        } else if (strcmp(tmpbuf, "princ") == 0) {
            princ = va_arg(ap, krb5_principal);
            if (krb5_unparse_name(context, princ, &str) == 0) {
                k5_buf_add(&buf, str);
                krb5_free_unparsed_name(context, str);
            }
        } else if (strcmp(tmpbuf, "ptype") == 0) {
            p = principal_type_string(va_arg(ap, krb5_int32));
            k5_buf_add(&buf, p);
        } else if (strcmp(tmpbuf, "patypes") == 0) {
            padata = va_arg(ap, krb5_pa_data **);
            if (padata == NULL || *padata == NULL)
                k5_buf_add(&buf, "(empty)");
            for (; padata != NULL && *padata != NULL; padata++) {
                k5_buf_add_fmt(&buf, "%d", (int)(*padata)->pa_type);
                if (*(padata + 1) != NULL)
                    k5_buf_add(&buf, ", ");
            }
        } else if (strcmp(tmpbuf, "etype") == 0) {
            etype = va_arg(ap, krb5_enctype);
            if (krb5_enctype_to_name(etype, TRUE, tmpbuf, sizeof(tmpbuf)) == 0)
                k5_buf_add(&buf, tmpbuf);
            else
                k5_buf_add_fmt(&buf, "%d", (int)etype);
        } else if (strcmp(tmpbuf, "etypes") == 0) {
            etypes = va_arg(ap, krb5_enctype *);
            if (etypes == NULL || *etypes == 0)
                k5_buf_add(&buf, "(empty)");
            for (; etypes != NULL && *etypes != 0; etypes++) {
                subfmt(context, &buf, "{etype}", *etypes);
                if (*(etypes + 1) != 0)
                    k5_buf_add(&buf, ", ");
            }
        } else if (strcmp(tmpbuf, "ccache") == 0) {
            ccache = va_arg(ap, krb5_ccache);
            k5_buf_add(&buf, krb5_cc_get_type(context, ccache));
            k5_buf_add(&buf, ":");
            k5_buf_add(&buf, krb5_cc_get_name(context, ccache));
        } else if (strcmp(tmpbuf, "keytab") == 0) {
            keytab = va_arg(ap, krb5_keytab);
            if (krb5_kt_get_name(context, keytab, tmpbuf, sizeof(tmpbuf)) == 0)
                k5_buf_add(&buf, tmpbuf);
        } else if (strcmp(tmpbuf, "creds") == 0) {
            creds = va_arg(ap, krb5_creds *);
            subfmt(context, &buf, "{princ} -> {princ}",
                   creds->client, creds->server);
        }
    }
    return buf.data;
}

/* Allows trace_format formatters to be represented in terms of other
 * formatters. */
static void
subfmt(krb5_context context, struct k5buf *buf, const char *fmt, ...)
{
    va_list ap;
    char *str;

    va_start(ap, fmt);
    str = trace_format(context, fmt, ap);
    if (str != NULL)
        k5_buf_add(buf, str);
    free(str);
    va_end(ap);
}

void
k5_init_trace(krb5_context context)
{
    const char *filename;

    filename = getenv("KRB5_TRACE");
    if (filename)
        (void) krb5_set_trace_filename(context, filename);
}

void
krb5int_trace(krb5_context context, const char *fmt, ...)
{
    va_list ap;
    krb5_trace_info info;
    char *str = NULL, *msg = NULL;
    krb5_int32 sec, usec;

    if (context == NULL || context->trace_callback == NULL)
        return;
    va_start(ap, fmt);
    str = trace_format(context, fmt, ap);
    if (str == NULL)
        goto cleanup;
    if (krb5_crypto_us_timeofday(&sec, &usec) != 0)
        goto cleanup;
    if (asprintf(&msg, "[%d] %d.%d: %s\n", (int) getpid(), (int) sec,
                 (int) usec, str) < 0)
        goto cleanup;
    info.message = msg;
    context->trace_callback(context, &info, context->trace_callback_data);
cleanup:
    free(str);
    free(msg);
    va_end(ap);
}

krb5_error_code KRB5_CALLCONV
krb5_set_trace_callback(krb5_context context, krb5_trace_callback fn,
                        void *cb_data)
{
    /* Allow the old callback to destroy its data if necessary. */
    if (context->trace_callback != NULL)
        context->trace_callback(context, NULL, context->trace_callback_data);
    context->trace_callback = fn;
    context->trace_callback_data = cb_data;
    return 0;
}

static void KRB5_CALLCONV
file_trace_cb(krb5_context context, const krb5_trace_info *info, void *data)
{
    int *fd = data;

    if (info == NULL) {
        /* Null info means destroy the callback data. */
        close(*fd);
        free(fd);
        return;
    }

    (void) write(*fd, info->message, strlen(info->message));
}

krb5_error_code KRB5_CALLCONV
krb5_set_trace_filename(krb5_context context, const char *filename)
{
    int *fd;

    /* Create callback data containing a file descriptor. */
    fd = malloc(sizeof(*fd));
    if (fd == NULL)
        return ENOMEM;
    *fd = open(filename, O_WRONLY|O_CREAT|O_APPEND, 0600);
    if (*fd == -1) {
        free(fd);
        return errno;
    }

    return krb5_set_trace_callback(context, file_trace_cb, fd);
}

#else /* DISABLE_TRACING */

krb5_error_code KRB5_CALLCONV
krb5_set_trace_callback(krb5_context context, krb5_trace_callback fn,
                        void *cb_data)
{
    if (fn == NULL)
        return 0;
    return KRB5_TRACE_NOSUPP;
}

krb5_error_code KRB5_CALLCONV
krb5_set_trace_filename(krb5_context context, const char *filename)
{
    return KRB5_TRACE_NOSUPP;
}

#endif /* DISABLE_TRACING */
