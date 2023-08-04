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

#ifndef NO_PASSWORD
#include <pwd.h>
#include <stdio.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

/*
 * errors:
 * GSS_S_BAD_NAMETYPE   if the type is bogus
 * GSS_S_BAD_NAME       if the type is good but the name is bogus
 * GSS_S_FAILURE        if memory allocation fails
 */

/*
 * Import serialized authdata context
 */
static krb5_error_code
import_name_composite(krb5_context context,
                      unsigned char *enc_data, size_t enc_length,
                      krb5_authdata_context *pad_context)
{
    krb5_authdata_context ad_context;
    krb5_error_code code;
    krb5_data data;

    if (enc_length == 0)
        return 0;

    code = krb5_authdata_context_init(context, &ad_context);
    if (code != 0)
        return code;

    data.data = (char *)enc_data;
    data.length = enc_length;

    code = krb5_authdata_import_attributes(context,
                                           ad_context,
                                           AD_USAGE_MASK,
                                           &data);
    if (code != 0) {
        krb5_authdata_context_free(context, ad_context);
        return code;
    }

    *pad_context = ad_context;

    return 0;
}

/* Split a host-based name "service[@host]" into allocated strings
 * placed in *service_out and *host_out (possibly NULL). */
static krb5_error_code
parse_hostbased(const char *str, size_t len,
                char **service_out, char **host_out)
{
    const char *at;
    size_t servicelen, hostlen;
    char *service, *host = NULL;

    *service_out = *host_out = NULL;

    /* Find the bound of the service name and copy it. */
    at = memchr(str, '@', len);
    servicelen = (at == NULL) ? len : (size_t)(at - str);
    service = xmalloc(servicelen + 1);
    if (service == NULL)
        return ENOMEM;
    memcpy(service, str, servicelen);
    service[servicelen] = '\0';

    /* Copy the hostname if present (at least one character after '@'). */
    if (len - servicelen > 1) {
        hostlen = len - servicelen - 1;
        host = malloc(hostlen + 1);
        if (host == NULL) {
            free(service);
            return ENOMEM;
        }
        memcpy(host, at + 1, hostlen);
        host[hostlen] = '\0';
    }

    *service_out = service;
    *host_out = host;
    return 0;
}

OM_uint32 KRB5_CALLCONV
krb5_gss_import_name(minor_status, input_name_buffer,
                     input_name_type, output_name)
    OM_uint32 *minor_status;
    gss_buffer_t input_name_buffer;
    gss_OID input_name_type;
    gss_name_t *output_name;
{
    krb5_context context;
    krb5_principal princ = NULL;
    krb5_error_code code;
    unsigned char *cp, *end;
    char *tmp = NULL, *tmp2 = NULL, *service = NULL, *host = NULL, *stringrep;
    ssize_t    length;
#ifndef NO_PASSWORD
    struct passwd *pw;
#endif
    int is_composite = 0, is_cert = 0;
    krb5_authdata_context ad_context = NULL;
    OM_uint32 status = GSS_S_FAILURE;
    krb5_gss_name_t name;
    int flags = 0;

    *output_name = NULL;
    *minor_status = 0;

    code = krb5_gss_init_context(&context);
    if (code)
        goto cleanup;

    if ((input_name_type != GSS_C_NULL_OID) &&
        (g_OID_equal(input_name_type, gss_nt_service_name) ||
         g_OID_equal(input_name_type, gss_nt_service_name_v2))) {
        /* Split the name into service and host (or NULL). */
        code = parse_hostbased(input_name_buffer->value,
                               input_name_buffer->length, &service, &host);
        if (code)
            goto cleanup;

        /*
         * Compute the initiator target name.  In some cases this is a waste of
         * getaddrinfo/getnameinfo queries, but computing the name when we need
         * it would require a lot of code changes.
         */
        code = krb5_sname_to_principal(context, host, service, KRB5_NT_SRV_HST,
                                       &princ);
        if (code)
            goto cleanup;
    } else if ((input_name_type != GSS_C_NULL_OID) &&
               (g_OID_equal(input_name_type, gss_nt_krb5_principal))) {
        krb5_principal input;

        if (input_name_buffer->length != sizeof(krb5_principal)) {
            code = G_WRONG_SIZE;
            status = GSS_S_BAD_NAME;
            goto cleanup;
        }

        input = *((krb5_principal *) input_name_buffer->value);

        code = krb5_copy_principal(context, input, &princ);
        if (code)
            goto cleanup;
    } else if ((input_name_type != NULL) &&
               g_OID_equal(input_name_type, GSS_C_NT_ANONYMOUS)) {
        code = krb5_copy_principal(context, krb5_anonymous_principal(),
                                   &princ);
        if (code)
            goto cleanup;
    } else if ((input_name_type != NULL) &&
               g_OID_equal(input_name_type, GSS_KRB5_NT_X509_CERT)) {
        code = krb5_build_principal_ext(context, &princ, 0, NULL,
                                        input_name_buffer->length,
                                        input_name_buffer->value, 0);
        if (code)
            goto cleanup;
        is_cert = 1;
    } else {
#ifndef NO_PASSWORD
        uid_t uid;
        struct passwd pwx;
        char pwbuf[BUFSIZ];
#endif

        stringrep = NULL;

        tmp = k5memdup0(input_name_buffer->value, input_name_buffer->length,
                        &code);
        if (tmp == NULL)
            goto cleanup;
        tmp2 = NULL;

        /* Find the appropriate string rep to pass into parse_name. */
        if ((input_name_type == GSS_C_NULL_OID) ||
            g_OID_equal(input_name_type, gss_nt_krb5_name) ||
            g_OID_equal(input_name_type, gss_nt_user_name)) {
            stringrep = tmp;
        } else if (g_OID_equal(input_name_type, GSS_KRB5_NT_ENTERPRISE_NAME)) {
            stringrep = tmp;
            flags |= KRB5_PRINCIPAL_PARSE_ENTERPRISE;
#ifndef NO_PASSWORD
        } else if (g_OID_equal(input_name_type, gss_nt_machine_uid_name)) {
            uid = *(uid_t *) input_name_buffer->value;
        do_getpwuid:
            if (k5_getpwuid_r(uid, &pwx, pwbuf, sizeof(pwbuf), &pw) == 0)
                stringrep = pw->pw_name;
            else
                code = G_NOUSER;
        } else if (g_OID_equal(input_name_type, gss_nt_string_uid_name)) {
            uid = atoi(tmp);
            goto do_getpwuid;
#endif
        } else if (g_OID_equal(input_name_type, gss_nt_exported_name) ||
                   g_OID_equal(input_name_type, GSS_C_NT_COMPOSITE_EXPORT)) {
#define BOUNDS_CHECK(cp, end, n)                                        \
            do { if ((end) - (cp) < (n)) goto fail_name; } while (0)
            cp = (unsigned char *)tmp;
            end = cp + input_name_buffer->length;

            BOUNDS_CHECK(cp, end, 2);
            if (*cp++ != 0x04)
                goto fail_name;
            switch (*cp++) {
            case 0x01:
                break;
            case 0x02:
                is_composite++;
                break;
            default:
                goto fail_name;
            }

            BOUNDS_CHECK(cp, end, 2);
            if (*cp++ != 0x00)
                goto fail_name;
            length = *cp++;
            if (length != (ssize_t)gss_mech_krb5->length+2)
                goto fail_name;

            BOUNDS_CHECK(cp, end, 2);
            if (*cp++ != 0x06)
                goto fail_name;
            length = *cp++;
            if (length != (ssize_t)gss_mech_krb5->length)
                goto fail_name;

            BOUNDS_CHECK(cp, end, length);
            if (memcmp(cp, gss_mech_krb5->elements, length) != 0)
                goto fail_name;
            cp += length;

            BOUNDS_CHECK(cp, end, 4);
            length = *cp++;
            length = (length << 8) | *cp++;
            length = (length << 8) | *cp++;
            length = (length << 8) | *cp++;

            BOUNDS_CHECK(cp, end, length);
            tmp2 = k5alloc(length + 1, &code);
            if (tmp2 == NULL)
                goto cleanup;
            strncpy(tmp2, (char *)cp, length);
            tmp2[length] = 0;
            stringrep = tmp2;
            cp += length;

            if (is_composite) {
                BOUNDS_CHECK(cp, end, 4);
                length = *cp++;
                length = (length << 8) | *cp++;
                length = (length << 8) | *cp++;
                length = (length << 8) | *cp++;

                BOUNDS_CHECK(cp, end, length);
                code = import_name_composite(context,
                                             cp, length,
                                             &ad_context);
                if (code != 0)
                    goto fail_name;
                cp += length;
            }
            assert(cp == end);
        } else {
            status = GSS_S_BAD_NAMETYPE;
            goto cleanup;
        }

        /* At this point, stringrep is set, or if not, code is. */
        if (stringrep) {
            code = krb5_parse_name_flags(context, stringrep, flags, &princ);
            if (code)
                goto cleanup;
        } else {
        fail_name:
            status = GSS_S_BAD_NAME;
            goto cleanup;
        }
    }

    /* Create a name and save it in the validation database. */
    code = kg_init_name(context, princ, service, host, ad_context,
                        KG_INIT_NAME_NO_COPY, &name);
    if (code)
        goto cleanup;
    name->is_cert = is_cert;

    princ = NULL;
    ad_context = NULL;
    service = host = NULL;
    *output_name = (gss_name_t)name;
    status = GSS_S_COMPLETE;

cleanup:
    *minor_status = (OM_uint32)code;
    if (*minor_status)
        save_error_info(*minor_status, context);
    krb5_free_principal(context, princ);
    krb5_authdata_context_free(context, ad_context);
    krb5_free_context(context);
    free(tmp);
    free(tmp2);
    free(service);
    free(host);
    return status;
}
