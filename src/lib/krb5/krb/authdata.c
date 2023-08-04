/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2009 by the Massachusetts Institute of Technology.  All
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

/* Loosely based on preauth2.c */

#define IS_PRIMARY_INSTANCE(_module) ((_module)->client_req_init != NULL)

static const char *objdirs[] = {
#if TARGET_OS_MAC
    KRB5_AUTHDATA_PLUGIN_BUNDLE_DIR,
#endif
    LIBDIR "/krb5/plugins/authdata",
    NULL
}; /* should be a list */

/* Internal authdata systems */
static krb5plugin_authdata_client_ftable_v0 *authdata_systems[] = {
    &k5_mspac_ad_client_ftable,
    &k5_authind_ad_client_ftable,
    NULL
};

static inline int
k5_ad_module_count(krb5plugin_authdata_client_ftable_v0 *table)
{
    int i;

    if (table->ad_type_list == NULL)
        return 0;

    for (i = 0; table->ad_type_list[i]; i++)
        ;

    return i;
}

static krb5_error_code
k5_ad_init_modules(krb5_context kcontext,
                   krb5_authdata_context context,
                   krb5plugin_authdata_client_ftable_v0 *table,
                   int *module_count)
{
    int j, k = *module_count;
    krb5_error_code code;
    void *plugin_context = NULL;
    void **rcpp = NULL;

    if (table->ad_type_list == NULL) {
#ifdef DEBUG
        fprintf(stderr, "warning: module \"%s\" does not advertise "
                "any AD types\n", table->name);
#endif
        return ENOENT;
    }

    if (table->init == NULL)
        return ENOSYS;

    code = (*table->init)(kcontext, &plugin_context);
    if (code != 0) {
#ifdef DEBUG
        fprintf(stderr, "warning: skipping module \"%s\" which "
                "failed to initialize\n", table->name);
#endif
        return code;
    }

    for (j = 0; table->ad_type_list[j] != 0; j++) {
        context->modules[k].ad_type = table->ad_type_list[j];
        context->modules[k].plugin_context = plugin_context;
        if (j == 0)
            context->modules[k].client_fini = table->fini;
        else
            context->modules[k].client_fini = NULL;
        context->modules[k].ftable = table;
        context->modules[k].name = table->name;
        if (table->flags != NULL) {
            (*table->flags)(kcontext, plugin_context,
                            context->modules[k].ad_type,
                            &context->modules[k].flags);
        } else {
            context->modules[k].flags = 0;
        }
        context->modules[k].request_context = NULL;
        if (j == 0) {
            context->modules[k].client_req_init = table->request_init;
            context->modules[k].client_req_fini = table->request_fini;
            rcpp = &context->modules[k].request_context;

            /* For now, single request per context. That may change */
            code = (*table->request_init)(kcontext,
                                          context,
                                          plugin_context,
                                          rcpp);
            if ((code != 0 && code != ENOMEM) &&
                (context->modules[k].flags & AD_INFORMATIONAL))
                code = 0;
            if (code != 0)
                break;
        } else {
            context->modules[k].client_req_init = NULL;
            context->modules[k].client_req_fini = NULL;
        }
        context->modules[k].request_context_pp = rcpp;

#ifdef DEBUG
        fprintf(stderr, "init module \"%s\", ad_type %d, flags %08x\n",
                context->modules[k].name,
                context->modules[k].ad_type,
                context->modules[k].flags);
#endif
        k++;
    }
    *module_count = k;

    return code;
}

/*
 * Determine size of to-be-externalized authdata context, for
 * modules that match given flags mask. Note that this size
 * does not include the magic identifier/trailer.
 */
static krb5_error_code
k5_ad_size(krb5_context kcontext,
           krb5_authdata_context context,
           krb5_flags flags,
           size_t *sizep)
{
    int i;
    krb5_error_code code = 0;

    *sizep += sizeof(krb5_int32); /* count */

    for (i = 0; i < context->n_modules; i++) {
        struct _krb5_authdata_context_module *module = &context->modules[i];
        size_t size;

        if ((module->flags & flags) == 0)
            continue;

        /* externalize request context for the first instance only */
        if (!IS_PRIMARY_INSTANCE(module))
            continue;

        if (module->ftable->size == NULL)
            continue;

        assert(module->ftable->externalize != NULL);

        size = sizeof(krb5_int32) /* namelen */ + strlen(module->name);

        code = (*module->ftable->size)(kcontext,
                                       context,
                                       module->plugin_context,
                                       *(module->request_context_pp),
                                       &size);
        if (code != 0)
            break;

        *sizep += size;
    }

    return code;
}

/*
 * Externalize authdata context, for modules that match given flags
 * mask. Note that the magic identifier/trailer is not included.
 */
static krb5_error_code
k5_ad_externalize(krb5_context kcontext,
                  krb5_authdata_context context,
                  krb5_flags flags,
                  krb5_octet **buffer,
                  size_t *lenremain)
{
    int i;
    krb5_error_code code;
    krb5_int32 ad_count = 0;
    krb5_octet *bp;
    size_t remain;

    bp = *buffer;
    remain = *lenremain;

    /* placeholder for count */
    code = krb5_ser_pack_int32(0, &bp, &remain);
    if (code != 0)
        return code;

    for (i = 0; i < context->n_modules; i++) {
        struct _krb5_authdata_context_module *module = &context->modules[i];
        size_t namelen;

        if ((module->flags & flags) == 0)
            continue;

        /* externalize request context for the first instance only */
        if (!IS_PRIMARY_INSTANCE(module))
            continue;

        if (module->ftable->externalize == NULL)
            continue;

        /*
         * We use the module name rather than the authdata type, because
         * there may be multiple modules for a particular authdata type.
         */
        namelen = strlen(module->name);

        code = krb5_ser_pack_int32((krb5_int32)namelen, &bp, &remain);
        if (code != 0)
            break;

        code = krb5_ser_pack_bytes((krb5_octet *)module->name,
                                   namelen, &bp, &remain);
        if (code != 0)
            break;

        code = (*module->ftable->externalize)(kcontext,
                                              context,
                                              module->plugin_context,
                                              *(module->request_context_pp),
                                              &bp,
                                              &remain);
        if (code != 0)
            break;

        ad_count++;
    }

    if (code == 0) {
        /* store actual count */
        krb5_ser_pack_int32(ad_count, buffer, lenremain);

        *buffer = bp;
        *lenremain = remain;
    }

    return code;
}

/*
 * Find authdata module for authdata type that matches flag mask
 */
static struct _krb5_authdata_context_module *
k5_ad_find_module(krb5_context kcontext,
                  krb5_authdata_context context,
                  krb5_flags flags,
                  const krb5_data *name)
{
    int i;
    struct _krb5_authdata_context_module *ret = NULL;

    for (i = 0; i < context->n_modules; i++) {
        struct _krb5_authdata_context_module *module = &context->modules[i];

        if ((module->flags & flags) == 0)
            continue;

        /* internalize request context for the first instance only */
        if (!IS_PRIMARY_INSTANCE(module))
            continue;

        /* check for name match */
        if (!data_eq_string(*name, module->name))
            continue;

        ret = module;
        break;
    }

    return ret;
}

/*
 * In-place internalize authdata context, for modules that match given
 * flags mask. The magic identifier/trailer is not expected by this.
 */
static krb5_error_code
k5_ad_internalize(krb5_context kcontext,
                  krb5_authdata_context context,
                  krb5_flags flags,
                  krb5_octet **buffer,
                  size_t *lenremain)
{
    krb5_error_code code = 0;
    krb5_int32 i, count;
    krb5_octet *bp;
    size_t remain;

    bp = *buffer;
    remain = *lenremain;

    code = krb5_ser_unpack_int32(&count, &bp, &remain);
    if (code != 0)
        return code;

    for (i = 0; i < count; i++) {
        struct _krb5_authdata_context_module *module;
        krb5_int32 namelen;
        krb5_data name;

        code = krb5_ser_unpack_int32(&namelen, &bp, &remain);
        if (code != 0)
            break;

        if (remain < (size_t)namelen) {
            code = ENOMEM;
            break;
        }

        name.length = namelen;
        name.data = (char *)bp;

        module = k5_ad_find_module(kcontext, context, flags, &name);
        if (module == NULL || module->ftable->internalize == NULL) {
            code = EINVAL;
            break;
        }

        bp += namelen;
        remain -= namelen;

        code = (*module->ftable->internalize)(kcontext,
                                              context,
                                              module->plugin_context,
                                              *(module->request_context_pp),
                                              &bp,
                                              &remain);
        if (code != 0)
            break;
    }

    if (code == 0) {
        *buffer = bp;
        *lenremain = remain;
    }

    return code;
}

krb5_error_code KRB5_CALLCONV
krb5_authdata_context_init(krb5_context kcontext,
                           krb5_authdata_context *pcontext)
{
    int n_modules, n_tables, i, k;
    void **tables = NULL;
    krb5plugin_authdata_client_ftable_v0 *table;
    krb5_authdata_context context = NULL;
    int internal_count = 0;
    struct plugin_dir_handle plugins;
    krb5_error_code code;

    *pcontext = NULL;
    memset(&plugins, 0, sizeof(plugins));

    n_modules = 0;
    for (n_tables = 0; authdata_systems[n_tables] != NULL; n_tables++) {
        n_modules += k5_ad_module_count(authdata_systems[n_tables]);
    }
    internal_count = n_tables;

    if (PLUGIN_DIR_OPEN(&plugins) == 0 &&
        krb5int_open_plugin_dirs(objdirs, NULL,
                                 &plugins,
                                 &kcontext->err) == 0 &&
        krb5int_get_plugin_dir_data(&plugins,
                                    "authdata_client_0",
                                    &tables,
                                    &kcontext->err) == 0 &&
        tables != NULL)
    {
        for (; tables[n_tables - internal_count] != NULL; n_tables++) {
            table = tables[n_tables - internal_count];
            n_modules += k5_ad_module_count(table);
        }
    }

    context = calloc(1, sizeof(*context));
    if (context == NULL) {
        code = ENOMEM;
        goto cleanup;
    }
    context->magic = KV5M_AUTHDATA_CONTEXT;
    context->modules = calloc(n_modules, sizeof(context->modules[0]));
    if (context->modules == NULL) {
        code = ENOMEM;
        goto cleanup;
    }
    context->n_modules = n_modules;

    /* fill in the structure */
    for (i = 0, k = 0, code = 0; i < n_tables - internal_count; i++) {
        code = k5_ad_init_modules(kcontext, context, tables[i], &k);
        if (code != 0)
            goto cleanup;
    }

    for (i = 0; i < internal_count; i++) {
        code = k5_ad_init_modules(kcontext, context, authdata_systems[i], &k);
        if (code != 0)
            goto cleanup;
    }

    context->plugins = plugins;

cleanup:
    if (tables != NULL)
        krb5int_free_plugin_dir_data(tables);

    if (code != 0) {
        krb5int_close_plugin_dirs(&plugins);
        krb5_authdata_context_free(kcontext, context);
    } else {
        /* plugins is owned by context now */
        *pcontext = context;
    }

    return code;
}

void KRB5_CALLCONV
krb5_authdata_context_free(krb5_context kcontext,
                           krb5_authdata_context context)
{
    int i;

    if (context == NULL)
        return;

    for (i = 0; i < context->n_modules; i++) {
        struct _krb5_authdata_context_module *module = &context->modules[i];

        if (module->client_req_fini != NULL &&
            module->request_context != NULL)
            (*module->client_req_fini)(kcontext,
                                       context,
                                       module->plugin_context,
                                       module->request_context);

        if (module->client_fini != NULL)
            (*module->client_fini)(kcontext, module->plugin_context);

        memset(module, 0, sizeof(*module));
    }

    if (context->modules != NULL) {
        free(context->modules);
        context->modules = NULL;
    }
    krb5int_close_plugin_dirs(&context->plugins);
    zapfree(context, sizeof(*context));
}

krb5_error_code KRB5_CALLCONV
krb5_authdata_import_attributes(krb5_context kcontext,
                                krb5_authdata_context context,
                                krb5_flags usage,
                                const krb5_data *attrs)
{
    krb5_octet *bp;
    size_t remain;

    bp = (krb5_octet *)attrs->data;
    remain = attrs->length;

    return k5_ad_internalize(kcontext, context, usage, &bp, &remain);
}

/* Return 0 with *kdc_issued_authdata == NULL on verification failure. */
static krb5_error_code
k5_get_kdc_issued_authdata(krb5_context kcontext,
                           const krb5_ap_req *ap_req,
                           krb5_principal *kdc_issuer,
                           krb5_authdata ***kdc_issued_authdata)
{
    krb5_error_code code;
    krb5_authdata **authdata;
    krb5_authdata **ticket_authdata;

    *kdc_issuer = NULL;
    *kdc_issued_authdata = NULL;

    ticket_authdata = ap_req->ticket->enc_part2->authorization_data;

    code = krb5_find_authdata(kcontext, ticket_authdata, NULL,
                              KRB5_AUTHDATA_KDC_ISSUED, &authdata);
    if (code != 0 || authdata == NULL)
        return code;

    /*
     * Note: a module must still implement a verify_authdata
     * method, even it is a NOOP that simply records the value
     * of the kdc_issued_flag.
     */
    code = krb5_verify_authdata_kdc_issued(kcontext,
                                           ap_req->ticket->enc_part2->session,
                                           authdata[0],
                                           kdc_issuer,
                                           kdc_issued_authdata);

    if (code == KRB5KRB_AP_ERR_BAD_INTEGRITY ||
        code == KRB5KRB_AP_ERR_INAPP_CKSUM ||
        code == KRB5_BAD_ENCTYPE || code == KRB5_BAD_MSIZE)
        code = 0;

    krb5_free_authdata(kcontext, authdata);

    return code;
}

/* Decode and verify each CAMMAC and collect the resulting authdata,
 * ignoring those that failed verification. */
static krb5_error_code
extract_cammacs(krb5_context kcontext, krb5_authdata **cammacs,
                const krb5_keyblock *key, krb5_authdata ***ad_out)
{
    krb5_error_code ret = 0;
    krb5_authdata **list = NULL, **elements = NULL, **new_list;
    size_t i, n_elements, count = 0;

    *ad_out = NULL;

    for (i = 0; cammacs != NULL && cammacs[i] != NULL; i++) {
        ret = k5_unwrap_cammac_svc(kcontext, cammacs[i], key, &elements);
        if (ret && ret != KRB5KRB_AP_ERR_BAD_INTEGRITY)
            goto cleanup;
        ret = 0;
        if (elements == NULL)
            continue;

        /* Add the verified elements to list and free the container array. */
        for (n_elements = 0; elements[n_elements] != NULL; n_elements++);
        new_list = realloc(list, (count + n_elements + 1) * sizeof(*list));
        if (new_list == NULL) {
            ret = ENOMEM;
            goto cleanup;
        }
        list = new_list;
        memcpy(list + count, elements, n_elements * sizeof(*list));
        count += n_elements;
        list[count] = NULL;
        free(elements);
        elements = NULL;
    }

    *ad_out = list;
    list = NULL;

cleanup:
    krb5_free_authdata(kcontext, list);
    krb5_free_authdata(kcontext, elements);
    return ret;
}

/* Retrieve verified CAMMAC contained elements. */
static krb5_error_code
get_cammac_authdata(krb5_context kcontext, const krb5_ap_req *ap_req,
                    const krb5_keyblock *key, krb5_authdata ***elems_out)
{
    krb5_error_code ret = 0;
    krb5_authdata **ticket_authdata, **cammacs, **elements;

    *elems_out = NULL;

    ticket_authdata = ap_req->ticket->enc_part2->authorization_data;
    ret = krb5_find_authdata(kcontext, ticket_authdata, NULL,
                             KRB5_AUTHDATA_CAMMAC, &cammacs);
    if (ret || cammacs == NULL)
        return ret;

    ret = extract_cammacs(kcontext, cammacs, key, &elements);
    if (!ret)
        *elems_out = elements;

    krb5_free_authdata(kcontext, cammacs);
    return ret;
}

krb5_error_code
krb5int_authdata_verify(krb5_context kcontext,
                        krb5_authdata_context context,
                        krb5_flags usage,
                        const krb5_auth_context *auth_context,
                        const krb5_keyblock *key,
                        const krb5_ap_req *ap_req)
{
    int i;
    krb5_error_code code = 0;
    krb5_authdata **authen_authdata;
    krb5_authdata **ticket_authdata;
    krb5_principal kdc_issuer = NULL;
    krb5_authdata **kdc_issued_authdata = NULL;
    krb5_authdata **cammac_authdata = NULL;

    authen_authdata = (*auth_context)->authentp->authorization_data;
    ticket_authdata = ap_req->ticket->enc_part2->authorization_data;

    code = k5_get_kdc_issued_authdata(kcontext, ap_req, &kdc_issuer,
                                      &kdc_issued_authdata);
    if (code)
        goto cleanup;

    code = get_cammac_authdata(kcontext, ap_req, key, &cammac_authdata);
    if (code)
        goto cleanup;

    for (i = 0; i < context->n_modules; i++) {
        struct _krb5_authdata_context_module *module = &context->modules[i];
        krb5_authdata **authdata = NULL;
        krb5_boolean kdc_issued_flag = FALSE;

        if ((module->flags & usage) == 0)
            continue;

        if (module->ftable->import_authdata == NULL)
            continue;

        if (kdc_issued_authdata != NULL &&
            (module->flags & AD_USAGE_KDC_ISSUED)) {
            code = krb5_find_authdata(kcontext, kdc_issued_authdata, NULL,
                                      module->ad_type, &authdata);
            if (code != 0)
                break;

            kdc_issued_flag = TRUE;
        }

        if (cammac_authdata != NULL && (module->flags & AD_CAMMAC_PROTECTED)) {
            code = krb5_find_authdata(kcontext, cammac_authdata, NULL,
                                      module->ad_type, &authdata);
            if (code)
                break;

            kdc_issued_flag = TRUE;
        }

        if (authdata == NULL) {
            krb5_boolean ticket_usage = FALSE;
            krb5_boolean authen_usage = FALSE;

            /*
             * Determine which authdata sources to interrogate based on the
             * module's usage. This is important if the authdata is signed
             * by the KDC with the TGT key (as the user can forge that in
             * the AP-REQ).
             */
            if (module->flags & (AD_USAGE_AS_REQ | AD_USAGE_TGS_REQ))
                ticket_usage = TRUE;
            if (module->flags & AD_USAGE_AP_REQ)
                authen_usage = TRUE;

            code = krb5_find_authdata(kcontext,
                                      ticket_usage ? ticket_authdata : NULL,
                                      authen_usage ? authen_authdata : NULL,
                                      module->ad_type, &authdata);
            if (code != 0)
                break;
        }

        if (authdata == NULL)
            continue;

        assert(authdata[0] != NULL);

        code = (*module->ftable->import_authdata)(kcontext,
                                                  context,
                                                  module->plugin_context,
                                                  *(module->request_context_pp),
                                                  authdata,
                                                  kdc_issued_flag,
                                                  kdc_issuer);
        if (code == 0 && module->ftable->verify != NULL) {
            code = (*module->ftable->verify)(kcontext,
                                             context,
                                             module->plugin_context,
                                             *(module->request_context_pp),
                                             auth_context,
                                             key,
                                             ap_req);
        }
        if (code != 0 && (module->flags & AD_INFORMATIONAL))
            code = 0;
        krb5_free_authdata(kcontext, authdata);
        if (code != 0)
            break;
    }

cleanup:
    krb5_free_principal(kcontext, kdc_issuer);
    krb5_free_authdata(kcontext, kdc_issued_authdata);
    krb5_free_authdata(kcontext, cammac_authdata);

    return code;
}

static krb5_error_code
k5_merge_data_list(krb5_data **dst, krb5_data *src, unsigned int *len)
{
    unsigned int i;
    krb5_data *d;

    if (src == NULL)
        return 0;

    for (i = 0; src[i].data != NULL; i++)
        ;

    d = realloc(*dst, (*len + i + 1) * sizeof(krb5_data));
    if (d == NULL)
        return ENOMEM;

    memcpy(&d[*len], src, i * sizeof(krb5_data));

    *len += i;

    d[*len].data = NULL;
    d[*len].length = 0;

    *dst = d;

    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_authdata_get_attribute_types(krb5_context kcontext,
                                  krb5_authdata_context context,
                                  krb5_data **out_attrs)
{
    int i;
    krb5_error_code code = 0;
    krb5_data *attrs = NULL;
    unsigned int attrs_len = 0;

    for (i = 0; i < context->n_modules; i++) {
        struct _krb5_authdata_context_module *module = &context->modules[i];
        krb5_data *attrs2 = NULL;

        if (module->ftable->get_attribute_types == NULL)
            continue;

        if ((*module->ftable->get_attribute_types)(kcontext,
                                                   context,
                                                   module->plugin_context,
                                                   *(module->request_context_pp),
                                                   &attrs2))
            continue;

        code = k5_merge_data_list(&attrs, attrs2, &attrs_len);
        if (code != 0) {
            krb5int_free_data_list(kcontext, attrs2);
            break;
        }
        if (attrs2 != NULL)
            free(attrs2);
    }

    if (code != 0) {
        krb5int_free_data_list(kcontext, attrs);
        attrs = NULL;
    }

    *out_attrs = attrs;

    return code;
}

krb5_error_code KRB5_CALLCONV
krb5_authdata_get_attribute(krb5_context kcontext,
                            krb5_authdata_context context,
                            const krb5_data *attribute,
                            krb5_boolean *authenticated,
                            krb5_boolean *complete,
                            krb5_data *value,
                            krb5_data *display_value,
                            int *more)
{
    int i;
    krb5_error_code code = ENOENT;

    *authenticated = FALSE;
    *complete = FALSE;

    value->data = NULL;
    value->length = 0;

    display_value->data = NULL;
    display_value->length = 0;

    /*
     * NB at present a module is presumed to be authoritative for
     * an attribute; not sure how to federate "more" across module
     * yet
     */
    for (i = 0; i < context->n_modules; i++) {
        struct _krb5_authdata_context_module *module = &context->modules[i];

        if (module->ftable->get_attribute == NULL)
            continue;

        code = (*module->ftable->get_attribute)(kcontext,
                                                context,
                                                module->plugin_context,
                                                *(module->request_context_pp),
                                                attribute,
                                                authenticated,
                                                complete,
                                                value,
                                                display_value,
                                                more);
        if (code == 0)
            break;
    }

    if (code != 0)
        *more = 0;

    return code;
}

krb5_error_code KRB5_CALLCONV
krb5_authdata_set_attribute(krb5_context kcontext,
                            krb5_authdata_context context,
                            krb5_boolean complete,
                            const krb5_data *attribute,
                            const krb5_data *value)
{
    int i;
    krb5_error_code code = 0;
    int found = 0;

    for (i = 0; i < context->n_modules; i++) {
        struct _krb5_authdata_context_module *module = &context->modules[i];

        if (module->ftable->set_attribute == NULL)
            continue;

        code = (*module->ftable->set_attribute)(kcontext,
                                                context,
                                                module->plugin_context,
                                                *(module->request_context_pp),
                                                complete,
                                                attribute,
                                                value);
        if (code == ENOENT)
            code = 0;
        else if (code == 0)
            found++;
        else
            break;
    }

    if (code == 0 && found == 0)
        code = ENOENT;

    return code;
}

krb5_error_code KRB5_CALLCONV
krb5_authdata_delete_attribute(krb5_context kcontext,
                               krb5_authdata_context context,
                               const krb5_data *attribute)
{
    int i;
    krb5_error_code code = ENOENT;
    int found = 0;

    for (i = 0; i < context->n_modules; i++) {
        struct _krb5_authdata_context_module *module = &context->modules[i];

        if (module->ftable->delete_attribute == NULL)
            continue;

        code = (*module->ftable->delete_attribute)(kcontext,
                                                   context,
                                                   module->plugin_context,
                                                   *(module->request_context_pp),
                                                   attribute);
        if (code == ENOENT)
            code = 0;
        else if (code == 0)
            found++;
        else
            break;
    }

    if (code == 0 && found == 0)
        code = ENOENT;

    return code;
}

krb5_error_code KRB5_CALLCONV
krb5_authdata_export_attributes(krb5_context kcontext,
                                krb5_authdata_context context,
                                krb5_flags flags,
                                krb5_data **attrsp)
{
    krb5_error_code code;
    size_t required = 0;
    krb5_octet *bp;
    size_t remain;
    krb5_data *attrs;

    code = k5_ad_size(kcontext, context, AD_USAGE_MASK, &required);
    if (code != 0)
        return code;

    attrs = malloc(sizeof(*attrs));
    if (attrs == NULL)
        return ENOMEM;

    attrs->magic = KV5M_DATA;
    attrs->length = 0;
    attrs->data = malloc(required);
    if (attrs->data == NULL) {
        free(attrs);
        return ENOMEM;
    }

    bp = (krb5_octet *)attrs->data;
    remain = required;

    code = k5_ad_externalize(kcontext, context, AD_USAGE_MASK, &bp, &remain);
    if (code != 0) {
        krb5_free_data(kcontext, attrs);
        return code;
    }

    attrs->length = (bp - (krb5_octet *)attrs->data);

    *attrsp = attrs;

    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_authdata_export_internal(krb5_context kcontext,
                              krb5_authdata_context context,
                              krb5_boolean restrict_authenticated,
                              const char *module_name,
                              void **ptr)
{
    krb5_error_code code;
    krb5_data name;
    struct _krb5_authdata_context_module *module;

    *ptr = NULL;

    name = make_data((char *)module_name, strlen(module_name));
    module = k5_ad_find_module(kcontext, context, AD_USAGE_MASK, &name);
    if (module == NULL)
        return ENOENT;

    if (module->ftable->export_internal == NULL)
        return ENOENT;

    code = (*module->ftable->export_internal)(kcontext,
                                              context,
                                              module->plugin_context,
                                              *(module->request_context_pp),
                                              restrict_authenticated,
                                              ptr);

    return code;
}

krb5_error_code KRB5_CALLCONV
krb5_authdata_free_internal(krb5_context kcontext,
                            krb5_authdata_context context,
                            const char *module_name,
                            void *ptr)
{
    krb5_data name;
    struct _krb5_authdata_context_module *module;

    name = make_data((char *)module_name, strlen(module_name));
    module = k5_ad_find_module(kcontext, context, AD_USAGE_MASK, &name);
    if (module == NULL)
        return ENOENT;

    if (module->ftable->free_internal == NULL)
        return ENOENT;

    (*module->ftable->free_internal)(kcontext,
                                     context,
                                     module->plugin_context,
                                     *(module->request_context_pp),
                                     ptr);

    return 0;
}

static krb5_error_code
k5_copy_ad_module_data(krb5_context kcontext,
                       krb5_authdata_context context,
                       struct _krb5_authdata_context_module *src_module,
                       krb5_authdata_context dst)
{
    int i;
    krb5_error_code code;
    struct _krb5_authdata_context_module *dst_module = NULL;

    for (i = 0; i < dst->n_modules; i++) {
        struct _krb5_authdata_context_module *module = &dst->modules[i];

        if (module->ftable == src_module->ftable) {
            /* XXX is this safe to assume these pointers are interned? */
            dst_module = module;
            break;
        }
    }

    if (dst_module == NULL)
        return ENOENT;

    /* copy request context for the first instance only */
    if (!IS_PRIMARY_INSTANCE(dst_module))
        return 0;

    assert(strcmp(dst_module->name, src_module->name) == 0);

    /* If copy is unimplemented, externalize/internalize */
    if (src_module->ftable->copy == NULL) {
        size_t size = 0, remain;
        krb5_octet *contents, *bp;

        assert(src_module->ftable->size != NULL);
        assert(src_module->ftable->externalize != NULL);
        assert(dst_module->ftable->internalize != NULL);

        code = (*src_module->ftable->size)(kcontext,
                                           context,
                                           src_module->plugin_context,
                                           src_module->request_context,
                                           &size);
        if (code != 0)
            return code;

        contents = malloc(size);
        if (contents == NULL)
            return ENOMEM;

        bp = contents;
        remain = size;

        code = (*src_module->ftable->externalize)(kcontext,
                                                  context,
                                                  src_module->plugin_context,
                                                  *(src_module->request_context_pp),
                                                  &bp,
                                                  &remain);
        if (code != 0) {
            free(contents);
            return code;
        }

        remain = (bp - contents);
        bp = contents;

        code = (*dst_module->ftable->internalize)(kcontext,
                                                  context,
                                                  dst_module->plugin_context,
                                                  *(dst_module->request_context_pp),
                                                  &bp,
                                                  &remain);
        if (code != 0) {
            free(contents);
            return code;
        }

        free(contents);
    } else {
        assert(src_module->request_context_pp == &src_module->request_context);
        assert(dst_module->request_context_pp == &dst_module->request_context);

        code = (*src_module->ftable->copy)(kcontext,
                                           context,
                                           src_module->plugin_context,
                                           src_module->request_context,
                                           dst_module->plugin_context,
                                           dst_module->request_context);
    }

    return code;
}

krb5_error_code KRB5_CALLCONV
krb5_authdata_context_copy(krb5_context kcontext,
                           krb5_authdata_context src,
                           krb5_authdata_context *pdst)
{
    int i;
    krb5_error_code code;
    krb5_authdata_context dst;

    /* XXX we need to init a new context because we can't copy plugins */
    code = krb5_authdata_context_init(kcontext, &dst);
    if (code != 0)
        return code;

    for (i = 0; i < src->n_modules; i++) {
        struct _krb5_authdata_context_module *module = &src->modules[i];

        code = k5_copy_ad_module_data(kcontext, src, module, dst);
        if (code != 0)
            break;
    }

    if (code != 0) {
        krb5_authdata_context_free(kcontext, dst);
        return code;
    }

    *pdst = dst;

    return 0;
}

/*
 * Calculate size of to-be-externalized authdata context.
 */
krb5_error_code
k5_size_authdata_context(krb5_context kcontext, krb5_authdata_context context,
                         size_t *sizep)
{
    krb5_error_code code;

    code = k5_ad_size(kcontext, context, AD_USAGE_MASK, sizep);
    if (code != 0)
        return code;

    *sizep += 2 * sizeof(krb5_int32); /* identifier/trailer */

    return 0;
}

/*
 * Externalize an authdata context.
 */
krb5_error_code
k5_externalize_authdata_context(krb5_context kcontext,
                                krb5_authdata_context context,
                                krb5_octet **buffer, size_t *lenremain)
{
    krb5_error_code code;
    krb5_octet *bp;
    size_t remain;

    bp = *buffer;
    remain = *lenremain;

    /* Our identifier */
    code = krb5_ser_pack_int32(KV5M_AUTHDATA_CONTEXT, &bp, &remain);
    if (code != 0)
        return code;

    /* The actual context data */
    code = k5_ad_externalize(kcontext, context, AD_USAGE_MASK,
                             &bp, &remain);
    if (code != 0)
        return code;

    /* Our trailer */
    code = krb5_ser_pack_int32(KV5M_AUTHDATA_CONTEXT, &bp, &remain);
    if (code != 0)
        return code;

    *buffer = bp;
    *lenremain = remain;

    return 0;
}

/*
 * Internalize an authdata context.
 */
krb5_error_code
k5_internalize_authdata_context(krb5_context kcontext,
                                krb5_authdata_context *ptr,
                                krb5_octet **buffer, size_t *lenremain)
{
    krb5_error_code code;
    krb5_authdata_context context;
    krb5_int32 ibuf;
    krb5_octet *bp;
    size_t remain;

    bp = *buffer;
    remain = *lenremain;

    code = krb5_ser_unpack_int32(&ibuf, &bp, &remain);
    if (code != 0)
        return code;

    if (ibuf != KV5M_AUTHDATA_CONTEXT)
        return EINVAL;

    code = krb5_authdata_context_init(kcontext, &context);
    if (code != 0)
        return code;

    code = k5_ad_internalize(kcontext, context, AD_USAGE_MASK,
                             &bp, &remain);
    if (code != 0) {
        krb5_authdata_context_free(kcontext, context);
        return code;
    }

    code = krb5_ser_unpack_int32(&ibuf, &bp, &remain);
    if (code != 0)
        return code;

    if (ibuf != KV5M_AUTHDATA_CONTEXT) {
        krb5_authdata_context_free(kcontext, context);
        return EINVAL;
    }

    *buffer = bp;
    *lenremain = remain;
    *ptr = context;

    return 0;
}

krb5_error_code
krb5int_copy_authdatum(krb5_context context,
                       const krb5_authdata *inad, krb5_authdata **outad)
{
    krb5_authdata *tmpad;

    if (!(tmpad = (krb5_authdata *)malloc(sizeof(*tmpad))))
        return ENOMEM;
    *tmpad = *inad;
    if (!(tmpad->contents = (krb5_octet *)malloc(inad->length))) {
        free(tmpad);
        return ENOMEM;
    }
    memcpy(tmpad->contents, inad->contents, inad->length);
    *outad = tmpad;
    return 0;
}

void KRB5_CALLCONV
krb5_free_authdata(krb5_context context, krb5_authdata **val)
{
    krb5_authdata **temp;

    if (val == NULL)
        return;
    for (temp = val; *temp; temp++) {
        free((*temp)->contents);
        free(*temp);
    }
    free(val);
}
