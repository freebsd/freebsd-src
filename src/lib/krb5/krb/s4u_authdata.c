/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2010 by the Massachusetts Institute of Technology.  All
 * Rights Reserved.
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
 *
 */

#include "k5-int.h"
#include "authdata.h"
#include "auth_con.h"
#include "int-proto.h"

/*
 * Authdata backend for processing SignedPath. Presently does not handle
 * the equivalent information in [MS-PAC], as that would require an NDR
 * interpreter.
 */

struct s4u2proxy_context {
    int count;
    krb5_principal *delegated;
    krb5_boolean authenticated;
};

static krb5_error_code
s4u2proxy_init(krb5_context kcontext, void **plugin_context)
{
    *plugin_context = NULL;
    return 0;
}

static void
s4u2proxy_flags(krb5_context kcontext,
                void *plugin_context,
                krb5_authdatatype ad_type,
                krb5_flags *flags)
{
    *flags = AD_USAGE_KDC_ISSUED;
}

static void
s4u2proxy_fini(krb5_context kcontext, void *plugin_context)
{
    return;
}

static krb5_error_code
s4u2proxy_request_init(krb5_context kcontext,
                       krb5_authdata_context context,
                       void *plugin_context,
                       void **request_context)
{
    krb5_error_code code;
    struct s4u2proxy_context *s4uctx;

    s4uctx = k5alloc(sizeof(*s4uctx), &code);
    if (s4uctx == NULL)
        return code;

    s4uctx->count = 0;
    s4uctx->delegated = NULL;
    s4uctx->authenticated = FALSE;

    *request_context = s4uctx;

    return 0;
}

static void
s4u2proxy_free_internal(krb5_context kcontext,
                        krb5_authdata_context context,
                        void *plugin_context,
                        void *request_context,
                        void *ptr)
{
    krb5_principal *delegated = (krb5_principal *)ptr;
    int i;

    if (delegated != NULL) {
        for (i = 0; delegated[i] != NULL; i++)
            krb5_free_principal(kcontext, delegated[i]);
        free(delegated);
    }
}

static krb5_error_code
s4u2proxy_import_authdata(krb5_context kcontext,
                          krb5_authdata_context context,
                          void *plugin_context,
                          void *request_context,
                          krb5_authdata **authdata,
                          krb5_boolean kdc_issued,
                          krb5_const_principal kdc_issuer)
{
    struct s4u2proxy_context *s4uctx = (struct s4u2proxy_context *)request_context;
    krb5_error_code code;
    krb5_ad_signedpath *sp;
    krb5_data enc_sp;

    enc_sp.data = (char *)authdata[0]->contents;
    enc_sp.length = authdata[0]->length;

    code = decode_krb5_ad_signedpath(&enc_sp, &sp);
    if (code != 0)
        return code;

    s4u2proxy_free_internal(kcontext, context,
                            plugin_context, request_context,
                            s4uctx->delegated);

    s4uctx->delegated = sp->delegated;
    sp->delegated = NULL;

    krb5_free_ad_signedpath(kcontext, sp);

    s4uctx->count = 0;

    if (s4uctx->delegated != NULL) {
        for (s4uctx->count = 0; s4uctx->delegated[s4uctx->count];
             s4uctx->count++)
            ;
    }

    s4uctx->authenticated = FALSE;

    return 0;
}

static krb5_error_code
s4u2proxy_export_authdata(krb5_context kcontext,
                          krb5_authdata_context context,
                          void *plugin_context,
                          void *request_context,
                          krb5_flags usage,
                          krb5_authdata ***out_authdata)
{
    struct s4u2proxy_context *s4uctx = (struct s4u2proxy_context *)request_context;
    krb5_error_code code;
    krb5_ad_signedpath sp;
    krb5_authdata **authdata;
    krb5_data *data;

    if (s4uctx->count == 0)
        return 0;

    memset(&sp, 0, sizeof(sp));
    sp.delegated = s4uctx->delegated;

    authdata = k5calloc(2, sizeof(krb5_authdata *), &code);
    if (authdata == NULL)
        return code;

    authdata[0] = k5alloc(sizeof(krb5_authdata), &code);
    if (authdata[0] == NULL)
        return code;

    code = encode_krb5_ad_signedpath(&sp, &data);
    if (code != 0) {
        krb5_free_authdata(kcontext, authdata);
        return code;
    }

    authdata[0]->magic = KV5M_AUTHDATA;
    authdata[0]->ad_type = KRB5_AUTHDATA_SIGNTICKET;
    authdata[0]->length = data->length;
    authdata[0]->contents = (krb5_octet *)data->data;

    authdata[1] = NULL;

    free(data);

    *out_authdata = authdata;

    return 0;
}

static krb5_error_code
s4u2proxy_verify(krb5_context kcontext,
                 krb5_authdata_context context,
                 void *plugin_context,
                 void *request_context,
                 const krb5_auth_context *auth_context,
                 const krb5_keyblock *key,
                 const krb5_ap_req *req)
{
    /*
     * XXX there is no way to verify the SignedPath without the TGS
     * key. This means that we can never mark this as authenticated.
     */

    return 0;
}

static void
s4u2proxy_request_fini(krb5_context kcontext,
                       krb5_authdata_context context,
                       void *plugin_context,
                       void *request_context)
{
    struct s4u2proxy_context *s4uctx = (struct s4u2proxy_context *)request_context;

    if (s4uctx == NULL)
        return;

    s4u2proxy_free_internal(kcontext, context,
                            plugin_context, request_context,
                            s4uctx->delegated);
    free(s4uctx);
}

/*
 * Nomenclature defined to be similar to [MS-PAC] 2.9, for future
 * interoperability
 */

static krb5_data s4u2proxy_transited_services_attr = {
    KV5M_DATA,
    sizeof("urn:constrained-delegation:transited-services") - 1,
    "urn:constrained-delegation:transited-services"
};

static krb5_error_code
s4u2proxy_get_attribute_types(krb5_context kcontext,
                              krb5_authdata_context context,
                              void *plugin_context,
                              void *request_context,
                              krb5_data **out_attrs)
{
    struct s4u2proxy_context *s4uctx = (struct s4u2proxy_context *)request_context;
    krb5_error_code code;
    krb5_data *attrs;
    int i = 0;

    if (s4uctx->count == 0)
        return ENOENT;

    attrs = k5calloc(2, sizeof(krb5_data), &code);
    if (attrs == NULL)
        goto cleanup;

    code = krb5int_copy_data_contents(kcontext,
                                      &s4u2proxy_transited_services_attr,
                                      &attrs[i++]);
    if (code != 0)
        goto cleanup;

    attrs[i].data = NULL;
    attrs[i].length = 0;

    *out_attrs = attrs;
    attrs = NULL;

cleanup:
    if (attrs != NULL) {
        for (i = 0; attrs[i].data; i++)
            krb5_free_data_contents(kcontext, &attrs[i]);
        free(attrs);
    }

    return 0;
}

static krb5_error_code
s4u2proxy_get_attribute(krb5_context kcontext,
                        krb5_authdata_context context,
                        void *plugin_context,
                        void *request_context,
                        const krb5_data *attribute,
                        krb5_boolean *authenticated,
                        krb5_boolean *complete,
                        krb5_data *value,
                        krb5_data *display_value,
                        int *more)
{
    struct s4u2proxy_context *s4uctx = (struct s4u2proxy_context *)request_context;
    krb5_error_code code;
    krb5_principal principal;
    int i;

    if (display_value != NULL) {
        display_value->data = NULL;
        display_value->length = 0;
    }

    if (!data_eq(*attribute, s4u2proxy_transited_services_attr))
        return ENOENT;

    i = -(*more) - 1;
    if (i < 0)
        return EINVAL;
    else if (i >= s4uctx->count)
        return ENOENT;

    principal = s4uctx->delegated[i];
    assert(principal != NULL);

    code = krb5_unparse_name_flags(kcontext, principal, 0, &value->data);
    if (code != 0)
        return code;

    value->length = strlen(value->data);

    if (display_value != NULL) {
        code = krb5_unparse_name_flags(kcontext, principal,
                                       KRB5_PRINCIPAL_UNPARSE_DISPLAY,
                                       &display_value->data);
        if (code != 0)
            return code;

        display_value->length = strlen(display_value->data);
    }

    i++;

    if (i == s4uctx->count)
        *more = 0;
    else
        *more = -(i + 1);

    *authenticated = s4uctx->authenticated;
    *complete = TRUE;

    return 0;
}

static krb5_error_code
s4u2proxy_set_attribute(krb5_context kcontext,
                        krb5_authdata_context context,
                        void *plugin_context,
                        void *request_context,
                        krb5_boolean complete,
                        const krb5_data *attribute,
                        const krb5_data *value)
{
    /* Only the KDC can set this attribute. */
    if (!data_eq(*attribute, s4u2proxy_transited_services_attr))
        return ENOENT;

    return EPERM;
}

static krb5_error_code
s4u2proxy_export_internal(krb5_context kcontext,
                          krb5_authdata_context context,
                          void *plugin_context,
                          void *request_context,
                          krb5_boolean restrict_authenticated,
                          void **ptr)
{
    struct s4u2proxy_context *s4uctx = (struct s4u2proxy_context *)request_context;
    krb5_error_code code;
    int i;
    krb5_principal *delegated;

    *ptr = NULL;

    if (s4uctx->count == 0)
        return ENOENT;

    if (restrict_authenticated)
        return ENOENT;

    delegated = k5calloc(s4uctx->count + 1, sizeof(krb5_principal), &code);
    if (delegated == NULL)
        return code;

    for (i = 0; i < s4uctx->count; i++) {
        code = krb5_copy_principal(kcontext, s4uctx->delegated[i],
                                   &delegated[i]);
        if (code != 0)
            goto cleanup;
    }

    delegated[i] = NULL;

    *ptr = delegated;
    delegated = NULL;

cleanup:
    s4u2proxy_free_internal(kcontext, context,
                            plugin_context, request_context,
                            delegated);

    return code;
}

static krb5_error_code
s4u2proxy_size(krb5_context kcontext,
               krb5_authdata_context context,
               void *plugin_context,
               void *request_context,
               size_t *sizep)
{
    struct s4u2proxy_context *s4uctx = (struct s4u2proxy_context *)request_context;
    krb5_error_code code = 0;
    int i;

    *sizep += sizeof(krb5_int32); /* version */
    *sizep += sizeof(krb5_int32); /* princ count */

    for (i = 0; i < s4uctx->count; i++) {
        code = krb5_size_opaque(kcontext, KV5M_PRINCIPAL,
                                (krb5_pointer)s4uctx->delegated[i], sizep);
        if (code != 0)
            return code;
    }

    *sizep += sizeof(krb5_int32); /* authenticated flag */

    return code;
}

static krb5_error_code
s4u2proxy_externalize(krb5_context kcontext,
                      krb5_authdata_context context,
                      void *plugin_context,
                      void *request_context,
                      krb5_octet **buffer,
                      size_t *lenremain)
{
    struct s4u2proxy_context *s4uctx = (struct s4u2proxy_context *)request_context;
    krb5_error_code code = 0;
    size_t required = 0;
    krb5_octet *bp;
    size_t remain;
    int i = 0;

    bp = *buffer;
    remain = *lenremain;

    s4u2proxy_size(kcontext, context, plugin_context,
                   request_context, &required);

    if (required > remain)
        return ENOMEM;

    krb5_ser_pack_int32(1, &bp, &remain); /* version */
    krb5_ser_pack_int32(s4uctx->count, &bp, &remain); /* princ count */

    for (i = 0; i < s4uctx->count; i++) {
        code = krb5_externalize_opaque(kcontext, KV5M_PRINCIPAL,
                                       (krb5_pointer)s4uctx->delegated[i],
                                       &bp, &remain);
        if (code != 0)
            return code;
    }

    krb5_ser_pack_int32(s4uctx->authenticated, &bp, &remain); /* authenticated */

    *buffer = bp;
    *lenremain = remain;

    return 0;
}

static krb5_error_code
s4u2proxy_internalize(krb5_context kcontext,
                      krb5_authdata_context context,
                      void *plugin_context,
                      void *request_context,
                      krb5_octet **buffer,
                      size_t *lenremain)
{
    struct s4u2proxy_context *s4uctx = (struct s4u2proxy_context *)request_context;
    krb5_error_code code;
    krb5_int32 ibuf;
    krb5_octet *bp;
    size_t remain;
    int count;
    krb5_principal *delegated = NULL;

    bp = *buffer;
    remain = *lenremain;

    /* version */
    code = krb5_ser_unpack_int32(&ibuf, &bp, &remain);
    if (code != 0)
        goto cleanup;

    if (ibuf != 1) {
        code = EINVAL;
        goto cleanup;
    }

    /* count */
    code = krb5_ser_unpack_int32(&count, &bp, &remain);
    if (code != 0)
        goto cleanup;

    if (count > 65535)
        return ERANGE; /* let's set some reasonable limits here */
    else if (count > 0) {
        int i;

        delegated = k5calloc(count + 1, sizeof(krb5_principal), &code);
        if (delegated == NULL)
            goto cleanup;

        for (i = 0; i < count; i++) {
            code = krb5_internalize_opaque(kcontext, KV5M_PRINCIPAL,
                                           (krb5_pointer *)&delegated[i],
                                           &bp, &remain);
            if (code != 0)
                goto cleanup;
        }

        delegated[i] = NULL;
    }

    code = krb5_ser_unpack_int32(&ibuf, &bp, &remain);
    if (code != 0)
        goto cleanup;

    s4u2proxy_free_internal(kcontext, context,
                            plugin_context, request_context,
                            s4uctx->delegated);

    s4uctx->count = count;
    s4uctx->delegated = delegated;
    s4uctx->authenticated = (ibuf != 0);

    delegated = NULL;

    *buffer = bp;
    *lenremain = remain;

cleanup:
    s4u2proxy_free_internal(kcontext, context,
                            plugin_context, request_context,
                            delegated);

    return code;
}

static krb5_error_code
s4u2proxy_copy(krb5_context kcontext,
               krb5_authdata_context context,
               void *plugin_context,
               void *request_context,
               void *dst_plugin_context,
               void *dst_request_context)
{
    struct s4u2proxy_context *srcctx = (struct s4u2proxy_context *)request_context;
    struct s4u2proxy_context *dstctx = (struct s4u2proxy_context *)dst_request_context;
    krb5_error_code code;

    code = s4u2proxy_export_internal(kcontext, context,
                                     plugin_context, request_context,
                                     FALSE, (void **)&dstctx->delegated);
    if (code != 0 && code != ENOENT)
        return code;

    dstctx->count = srcctx->count;
    dstctx->authenticated = srcctx->authenticated;

    return 0;
}

static krb5_authdatatype s4u2proxy_ad_types[] = { KRB5_AUTHDATA_SIGNTICKET, 0 };

krb5plugin_authdata_client_ftable_v0 k5_s4u2proxy_ad_client_ftable = {
    "constrained-delegation",
    s4u2proxy_ad_types,
    s4u2proxy_init,
    s4u2proxy_fini,
    s4u2proxy_flags,
    s4u2proxy_request_init,
    s4u2proxy_request_fini,
    s4u2proxy_get_attribute_types,
    s4u2proxy_get_attribute,
    s4u2proxy_set_attribute,
    NULL, /* delete_attribute_proc */
    s4u2proxy_export_authdata,
    s4u2proxy_import_authdata,
    s4u2proxy_export_internal,
    s4u2proxy_free_internal,
    s4u2proxy_verify,
    s4u2proxy_size,
    s4u2proxy_externalize,
    s4u2proxy_internalize,
    s4u2proxy_copy
};
