/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/gssapi/common.c - Common utility functions for GSSAPI test programs */
/*
 * Copyright (C) 2012 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include "common.h"

gss_OID_desc mech_krb5 = { 9, "\052\206\110\206\367\022\001\002\002" };
gss_OID_desc mech_spnego = { 6, "\053\006\001\005\005\002" };
gss_OID_desc mech_iakerb = { 6, "\053\006\001\005\002\005" };
gss_OID_set_desc mechset_krb5 = { 1, &mech_krb5 };
gss_OID_set_desc mechset_spnego = { 1, &mech_spnego };
gss_OID_set_desc mechset_iakerb = { 1, &mech_iakerb };

static void
display_status(const char *msg, OM_uint32 code, int type)
{
    OM_uint32 min_stat, msg_ctx = 0;
    gss_buffer_desc buf;

    do {
        (void)gss_display_status(&min_stat, code, type, GSS_C_NULL_OID,
                                 &msg_ctx, &buf);
        fprintf(stderr, "%s: %.*s\n", msg, (int)buf.length, (char *)buf.value);
        (void)gss_release_buffer(&min_stat, &buf);
    } while (msg_ctx != 0);
}

void
check_gsserr(const char *msg, OM_uint32 major, OM_uint32 minor)
{
    if (GSS_ERROR(major)) {
        display_status(msg, major, GSS_C_GSS_CODE);
        display_status(msg, minor, GSS_C_MECH_CODE);
        exit(1);
    }
}

void
check_k5err(krb5_context context, const char *msg, krb5_error_code code)
{
    const char *errmsg;

    if (code) {
        errmsg = krb5_get_error_message(context, code);
        printf("%s: %s\n", msg, errmsg);
        krb5_free_error_message(context, errmsg);
        exit(1);
    }
}

void
errout(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

gss_name_t
import_name(const char *str)
{
    OM_uint32 major, minor;
    gss_name_t name;
    gss_buffer_desc buf;
    gss_OID nametype = NULL;

    if (*str == 'u')
        nametype = GSS_C_NT_USER_NAME;
    else if (*str == 'p')
        nametype = (gss_OID)GSS_KRB5_NT_PRINCIPAL_NAME;
    else if (*str == 'e')
        nametype = (gss_OID)GSS_KRB5_NT_ENTERPRISE_NAME;
    else if (*str == 'c')
        nametype = (gss_OID)GSS_KRB5_NT_X509_CERT;
    else if (*str == 'h')
        nametype = GSS_C_NT_HOSTBASED_SERVICE;
    if (nametype == NULL || str[1] != ':')
        errout("names must begin with u: or p: or e: or c: or h:");
    buf.value = (char *)str + 2;
    buf.length = strlen(str) - 2;
    major = gss_import_name(&minor, &buf, nametype, &name);
    check_gsserr("gss_import_name", major, minor);
    return name;
}

void
establish_contexts(gss_OID imech, gss_cred_id_t icred, gss_cred_id_t acred,
                   gss_name_t tname, OM_uint32 flags, gss_ctx_id_t *ictx,
                   gss_ctx_id_t *actx, gss_name_t *src_name, gss_OID *amech,
                   gss_cred_id_t *deleg_cred)
{
    establish_contexts_ex(imech, icred, acred, tname, flags, ictx, actx,
                          GSS_C_NO_CHANNEL_BINDINGS, GSS_C_NO_CHANNEL_BINDINGS,
                          NULL, src_name, amech, deleg_cred);
}

void
establish_contexts_ex(gss_OID imech, gss_cred_id_t icred, gss_cred_id_t acred,
                      gss_name_t tname, OM_uint32 flags, gss_ctx_id_t *ictx,
                      gss_ctx_id_t *actx, gss_channel_bindings_t icb,
                      gss_channel_bindings_t acb, OM_uint32 *aret_flags,
                      gss_name_t *src_name, gss_OID *amech,
                      gss_cred_id_t *deleg_cred)
{
    OM_uint32 minor, imaj, amaj;
    gss_buffer_desc itok, atok;

    *ictx = *actx = GSS_C_NO_CONTEXT;
    imaj = amaj = GSS_S_CONTINUE_NEEDED;
    itok.value = atok.value = NULL;
    itok.length = atok.length = 0;
    for (;;) {
        (void)gss_release_buffer(&minor, &itok);
        imaj = gss_init_sec_context(&minor, icred, ictx, tname, imech, flags,
                                    GSS_C_INDEFINITE, icb, &atok, NULL, &itok,
                                    NULL, NULL);
        check_gsserr("gss_init_sec_context", imaj, minor);
        if (amaj == GSS_S_COMPLETE)
            break;

        (void)gss_release_buffer(&minor, &atok);
        amaj = gss_accept_sec_context(&minor, actx, acred, &itok, acb,
                                      src_name, amech, &atok, aret_flags, NULL,
                                      deleg_cred);
        check_gsserr("gss_accept_sec_context", amaj, minor);
        (void)gss_release_buffer(&minor, &itok);
        if (imaj == GSS_S_COMPLETE)
            break;
    }

    if (imaj != GSS_S_COMPLETE || amaj != GSS_S_COMPLETE)
        errout("One side wants to continue after the other is done");

    (void)gss_release_buffer(&minor, &itok);
    (void)gss_release_buffer(&minor, &atok);
}

void
export_import_cred(gss_cred_id_t *cred)
{
    OM_uint32 major, minor;
    gss_buffer_desc buf;

    major = gss_export_cred(&minor, *cred, &buf);
    check_gsserr("gss_export_cred", major, minor);
    (void)gss_release_cred(&minor, cred);
    major = gss_import_cred(&minor, &buf, cred);
    check_gsserr("gss_import_cred", major, minor);
    (void)gss_release_buffer(&minor, &buf);
}

void
display_canon_name(const char *tag, gss_name_t name, gss_OID mech)
{
    gss_name_t canon;
    OM_uint32 major, minor;
    gss_buffer_desc buf;

    major = gss_canonicalize_name(&minor, name, mech, &canon);
    check_gsserr("gss_canonicalize_name", major, minor);

    major = gss_display_name(&minor, canon, &buf, NULL);
    check_gsserr("gss_display_name", major, minor);

    printf("%s:\t%.*s\n", tag, (int)buf.length, (char *)buf.value);

    (void)gss_release_name(&minor, &canon);
    (void)gss_release_buffer(&minor, &buf);
}

void
display_oid(const char *tag, gss_OID oid)
{
    OM_uint32 major, minor;
    gss_buffer_desc buf;

    major = gss_oid_to_str(&minor, oid, &buf);
    check_gsserr("gss_oid_to_str", major, minor);
    if (tag != NULL)
        printf("%s:\t", tag);
    printf("%.*s\n", (int)buf.length, (char *)buf.value);
    (void)gss_release_buffer(&minor, &buf);
}

static void
dump_attribute(gss_name_t name, gss_buffer_t attribute, int noisy)
{
    OM_uint32 major, minor;
    gss_buffer_desc value;
    gss_buffer_desc display_value;
    int authenticated = 0;
    int complete = 0;
    int more = -1;
    unsigned int i;

    while (more != 0) {
        value.value = NULL;
        display_value.value = NULL;

        major = gss_get_name_attribute(&minor, name, attribute, &authenticated,
                                       &complete, &value, &display_value,
                                       &more);
        check_gsserr("gss_get_name_attribute", major, minor);

        printf("Attribute %.*s %s %s\n\n%.*s\n",
               (int)attribute->length, (char *)attribute->value,
               authenticated ? "Authenticated" : "",
               complete ? "Complete" : "",
               (int)display_value.length, (char *)display_value.value);

        if (noisy) {
            for (i = 0; i < value.length; i++) {
                if ((i % 32) == 0)
                    printf("\n");
                printf("%02x", ((char *)value.value)[i] & 0xFF);
            }
            printf("\n\n");
        }

        (void)gss_release_buffer(&minor, &value);
        (void)gss_release_buffer(&minor, &display_value);
    }
}

void
enumerate_attributes(gss_name_t name, int noisy)
{
    OM_uint32 major, minor;
    int is_mechname;
    gss_buffer_set_t attrs = GSS_C_NO_BUFFER_SET;
    size_t i;

    major = gss_inquire_name(&minor, name, &is_mechname, NULL, &attrs);
    check_gsserr("gss_inquire_name", major, minor);

    if (attrs != GSS_C_NO_BUFFER_SET) {
        for (i = 0; i < attrs->count; i++)
            dump_attribute(name, &attrs->elements[i], noisy);
    }

    (void)gss_release_buffer_set(&minor, &attrs);
}

void
print_hex(FILE *fp, gss_buffer_t buf)
{
    size_t i;
    const unsigned char *bytes = buf->value;

    for (i = 0; i < buf->length; i++)
        printf("%02X", bytes[i]);
    printf("\n");
}
