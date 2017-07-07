/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/init_ctx.c */
/*
 * Copyright 1994,1999,2000, 2002, 2003, 2007, 2008, 2009  by the Massachusetts Institute of Technology.
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
 * Copyright (C) 1998 by the FundsXpress, INC.
 *
 * All rights reserved.
 *
 * Export of this software from the United States of America may require
 * a specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of FundsXpress. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  FundsXpress makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "k5-int.h"
#include "int-proto.h"
#include "os-proto.h"
#include <ctype.h>
#include "brand.c"
#include "../krb5_libinit.h"

/* The des-mdX entries are last for now, because it's easy to
   configure KDCs to issue TGTs with des-mdX keys and then not accept
   them.  This'll be fixed, but for better compatibility, let's prefer
   des-crc for now.  */
static krb5_enctype default_enctype_list[] = {
    ENCTYPE_AES256_CTS_HMAC_SHA1_96, ENCTYPE_AES128_CTS_HMAC_SHA1_96,
    ENCTYPE_AES256_CTS_HMAC_SHA384_192, ENCTYPE_AES128_CTS_HMAC_SHA256_128,
    ENCTYPE_DES3_CBC_SHA1,
    ENCTYPE_ARCFOUR_HMAC,
    ENCTYPE_CAMELLIA128_CTS_CMAC, ENCTYPE_CAMELLIA256_CTS_CMAC,
    ENCTYPE_DES_CBC_CRC, ENCTYPE_DES_CBC_MD5, ENCTYPE_DES_CBC_MD4,
    0
};

#if (defined(_WIN32))
extern krb5_error_code krb5_vercheck();
extern void krb5_win_ccdll_load(krb5_context context);
#endif

#define DEFAULT_CLOCKSKEW  300 /* 5 min */

static krb5_error_code
get_integer(krb5_context ctx, const char *name, int def_val, int *int_out)
{
    krb5_error_code retval;

    retval = profile_get_integer(ctx->profile, KRB5_CONF_LIBDEFAULTS,
                                 name, NULL, def_val, int_out);
    if (retval)
        TRACE_PROFILE_ERR(ctx, name, KRB5_CONF_LIBDEFAULTS, retval);
    return retval;
}

static krb5_error_code
get_boolean(krb5_context ctx, const char *name, int def_val, int *boolean_out)
{
    krb5_error_code retval;

    retval = profile_get_boolean(ctx->profile, KRB5_CONF_LIBDEFAULTS,
                                 name, NULL, def_val, boolean_out);
    if (retval)
        TRACE_PROFILE_ERR(ctx, name, KRB5_CONF_LIBDEFAULTS, retval);
    return retval;
}

krb5_error_code KRB5_CALLCONV
krb5_init_context(krb5_context *context)
{
    /*
     * This is rather silly, but should improve our chances of
     * retaining the krb5_brand array in the final linked library,
     * better than a static variable that's unreferenced after
     * optimization, or even a non-static symbol that's not exported
     * from the library nor referenced from anywhere else in the
     * library.
     *
     * If someday we grow an API to actually return the string, we can
     * get rid of this silliness.
     */
    int my_zero = (krb5_brand[0] == 0);

    return krb5_init_context_profile(NULL, my_zero, context);
}

krb5_error_code KRB5_CALLCONV
krb5_init_secure_context(krb5_context *context)
{
    return krb5_init_context_profile(NULL, KRB5_INIT_CONTEXT_SECURE, context);
}

krb5_error_code
krb5int_init_context_kdc(krb5_context *context)
{
    return krb5_init_context_profile(NULL, KRB5_INIT_CONTEXT_KDC, context);
}

krb5_error_code KRB5_CALLCONV
krb5_init_context_profile(profile_t profile, krb5_flags flags,
                          krb5_context *context_out)
{
    krb5_context ctx = 0;
    krb5_error_code retval;
    struct {
        krb5_int32 now, now_usec;
        long pid;
    } seed_data;
    krb5_data seed;
    int tmp;

    /* Verify some assumptions.  If the assumptions hold and the
       compiler is optimizing, this should result in no code being
       executed.  If we're guessing "unsigned long long" instead
       of using uint64_t, the possibility does exist that we're
       wrong.  */
    {
        uint64_t i64;
        assert(sizeof(i64) == 8);
        i64 = 0, i64--, i64 >>= 62;
        assert(i64 == 3);
        i64 = 1, i64 <<= 31, i64 <<= 31, i64 <<= 1;
        assert(i64 != 0);
        i64 <<= 1;
        assert(i64 == 0);
    }

    retval = krb5int_initialize_library();
    if (retval)
        return retval;

#if (defined(_WIN32))
    /*
     * Load the krbcc32.dll if necessary.  We do this here so that
     * we know to use API: later on during initialization.
     * The context being NULL is ok.
     */
    krb5_win_ccdll_load(ctx);

    /*
     * krb5_vercheck() is defined in win_glue.c, and this is
     * where we handle the timebomb and version server checks.
     */
    retval = krb5_vercheck();
    if (retval)
        return retval;
#endif

    *context_out = NULL;

    ctx = calloc(1, sizeof(struct _krb5_context));
    if (!ctx)
        return ENOMEM;
    ctx->magic = KV5M_CONTEXT;

    ctx->profile_secure = (flags & KRB5_INIT_CONTEXT_SECURE) != 0;

    retval = k5_os_init_context(ctx, profile, flags);
    if (retval)
        goto cleanup;

    ctx->trace_callback = NULL;
#ifndef DISABLE_TRACING
    if (!ctx->profile_secure)
        k5_init_trace(ctx);
#endif

    retval = get_boolean(ctx, KRB5_CONF_ALLOW_WEAK_CRYPTO, 0, &tmp);
    if (retval)
        goto cleanup;
    ctx->allow_weak_crypto = tmp;

    retval = get_boolean(ctx, KRB5_CONF_IGNORE_ACCEPTOR_HOSTNAME, 0, &tmp);
    if (retval)
        goto cleanup;
    ctx->ignore_acceptor_hostname = tmp;

    retval = get_boolean(ctx, KRB5_CONF_DNS_CANONICALIZE_HOSTNAME, 1, &tmp);
    if (retval)
        goto cleanup;
    ctx->dns_canonicalize_hostname = tmp;

    /* initialize the prng (not well, but passable) */
    if ((retval = krb5_c_random_os_entropy( ctx, 0, NULL)) !=0)
        goto cleanup;
    if ((retval = krb5_crypto_us_timeofday(&seed_data.now, &seed_data.now_usec)))
        goto cleanup;
    seed_data.pid = getpid ();
    seed.length = sizeof(seed_data);
    seed.data = (char *) &seed_data;
    if ((retval = krb5_c_random_add_entropy(ctx, KRB5_C_RANDSOURCE_TIMING, &seed)))
        goto cleanup;

    ctx->default_realm = 0;
    get_integer(ctx, KRB5_CONF_CLOCKSKEW, DEFAULT_CLOCKSKEW, &tmp);
    ctx->clockskew = tmp;

#if 0
    /* Default ticket lifetime is currently not supported */
    profile_get_integer(ctx->profile, KRB5_CONF_LIBDEFAULTS, "tkt_lifetime",
                        0, 10 * 60 * 60, &tmp);
    ctx->tkt_lifetime = tmp;
#endif

    /* DCE 1.1 and below only support CKSUMTYPE_RSA_MD4 (2)  */
    /* DCE add kdc_req_checksum_type = 2 to krb5.conf */
    get_integer(ctx, KRB5_CONF_KDC_REQ_CHECKSUM_TYPE, CKSUMTYPE_RSA_MD5,
                &tmp);
    ctx->kdc_req_sumtype = tmp;

    get_integer(ctx, KRB5_CONF_AP_REQ_CHECKSUM_TYPE, 0, &tmp);
    ctx->default_ap_req_sumtype = tmp;

    get_integer(ctx, KRB5_CONF_SAFE_CHECKSUM_TYPE, CKSUMTYPE_RSA_MD5_DES,
                &tmp);
    ctx->default_safe_sumtype = tmp;

    get_integer(ctx, KRB5_CONF_KDC_DEFAULT_OPTIONS, KDC_OPT_RENEWABLE_OK,
                &tmp);
    ctx->kdc_default_options = tmp;
#define DEFAULT_KDC_TIMESYNC 1
    get_integer(ctx, KRB5_CONF_KDC_TIMESYNC, DEFAULT_KDC_TIMESYNC, &tmp);
    ctx->library_options = tmp ? KRB5_LIBOPT_SYNC_KDCTIME : 0;

    retval = profile_get_string(ctx->profile, KRB5_CONF_LIBDEFAULTS,
                                KRB5_CONF_PLUGIN_BASE_DIR, 0,
                                DEFAULT_PLUGIN_BASE_DIR,
                                &ctx->plugin_base_dir);
    if (retval) {
        TRACE_PROFILE_ERR(ctx, KRB5_CONF_PLUGIN_BASE_DIR,
                          KRB5_CONF_LIBDEFAULTS, retval);
        goto cleanup;
    }

    /*
     * We use a default file credentials cache of 3.  See
     * lib/krb5/krb/ccache/file/fcc.h for a description of the
     * credentials cache types.
     *
     * Note: DCE 1.0.3a only supports a cache type of 1
     *      DCE 1.1 supports a cache type of 2.
     */
#define DEFAULT_CCACHE_TYPE 4
    get_integer(ctx, KRB5_CONF_CCACHE_TYPE, DEFAULT_CCACHE_TYPE, &tmp);
    ctx->fcc_default_format = tmp + 0x0500;
    ctx->prompt_types = 0;
    ctx->use_conf_ktypes = 0;
    ctx->udp_pref_limit = -1;

    /* It's OK if this fails */
    (void)profile_get_string(ctx->profile, KRB5_CONF_LIBDEFAULTS,
                             KRB5_CONF_ERR_FMT, NULL, NULL, &ctx->err_fmt);
    *context_out = ctx;
    return 0;

cleanup:
    krb5_free_context(ctx);
    return retval;
}

void KRB5_CALLCONV
krb5_free_context(krb5_context ctx)
{
    if (ctx == NULL)
        return;
    k5_os_free_context(ctx);

    free(ctx->in_tkt_etypes);
    ctx->in_tkt_etypes = NULL;
    free(ctx->tgs_etypes);
    ctx->tgs_etypes = NULL;
    free(ctx->default_realm);
    ctx->default_realm = 0;
    if (ctx->ser_ctx_count && ctx->ser_ctx) {
        free(ctx->ser_ctx);
        ctx->ser_ctx = 0;
    }

    krb5_clear_error_message(ctx);
    free(ctx->err_fmt);

#ifndef DISABLE_TRACING
    if (ctx->trace_callback)
        ctx->trace_callback(ctx, NULL, ctx->trace_callback_data);
#endif

    k5_ccselect_free_context(ctx);
    k5_hostrealm_free_context(ctx);
    k5_localauth_free_context(ctx);
    k5_plugin_free_context(ctx);
    free(ctx->plugin_base_dir);
    free(ctx->tls);

    ctx->magic = 0;
    free(ctx);
}

/*
 * Set the desired default ktypes, making sure they are valid.
 */
static krb5_error_code
set_default_etype_var(krb5_context context, const krb5_enctype *etypes,
                      krb5_enctype **var)
{
    krb5_error_code code;
    krb5_enctype *list;
    size_t src, dst;

    if (etypes) {
        /* Empty list passed in. */
        if (etypes[0] == 0)
            return EINVAL;
        code = k5_copy_etypes(etypes, &list);
        if (code)
            return code;

        /* Filter list in place to exclude invalid and (optionally) weak
         * enctypes. */
        for (src = dst = 0; list[src]; src++) {
            if (!krb5_c_valid_enctype(list[src]))
                continue;
            if (!context->allow_weak_crypto
                && krb5int_c_weak_enctype(list[src]))
                continue;
            list[dst++] = list[src];
        }
        list[dst] = 0;          /* Zero-terminate. */
        if (dst == 0) {
            free(list);
            return KRB5_CONFIG_ETYPE_NOSUPP;
        }
    } else {
        list = NULL;
    }

    free(*var);
    *var = list;
    return 0;
}

krb5_error_code
krb5_set_default_in_tkt_ktypes(krb5_context context,
                               const krb5_enctype *etypes)
{
    return set_default_etype_var(context, etypes, &context->in_tkt_etypes);
}

krb5_error_code KRB5_CALLCONV
krb5_set_default_tgs_enctypes(krb5_context context, const krb5_enctype *etypes)
{
    return set_default_etype_var(context, etypes, &context->tgs_etypes);
}

/* Old name for above function. */
krb5_error_code
krb5_set_default_tgs_ktypes(krb5_context context, const krb5_enctype *etypes)
{
    return set_default_etype_var(context, etypes, &context->tgs_etypes);
}

/*
 * Add etype to, or remove etype from, the zero-terminated list *list_ptr,
 * reallocating if the list size changes.  Filter out weak enctypes if
 * allow_weak is false.  If memory allocation fails, set *list_ptr to NULL and
 * do nothing for subsequent operations.
 */
static void
mod_list(krb5_enctype etype, krb5_boolean add, krb5_boolean allow_weak,
         krb5_enctype **list_ptr)
{
    size_t i;
    krb5_enctype *list = *list_ptr;

    /* Stop now if a previous allocation failed or the enctype is filtered. */
    if (list == NULL || (!allow_weak && krb5int_c_weak_enctype(etype)))
        return;
    if (add) {
        /* Count entries; do nothing if etype is a duplicate. */
        for (i = 0; list[i] != 0; i++) {
            if (list[i] == etype)
                return;
        }
        /* Make room for the new entry and add it. */
        list = realloc(list, (i + 2) * sizeof(krb5_enctype));
        if (list != NULL) {
            list[i] = etype;
            list[i + 1] = 0;
        }
    } else {
        /* Look for etype in the list. */
        for (i = 0; list[i] != 0; i++) {
            if (list[i] != etype)
                continue;
            /* Perform removal. */
            for (; list[i + 1] != 0; i++)
                list[i] = list[i + 1];
            list[i] = 0;
            list = realloc(list, (i + 1) * sizeof(krb5_enctype));
            break;
        }
    }
    /* Update *list_ptr, freeing the old value if realloc failed. */
    if (list == NULL)
        free(*list_ptr);
    *list_ptr = list;
}

/*
 * Set *result to a zero-terminated list of enctypes resulting from
 * parsing profstr.  profstr may be modified during parsing.
 */
krb5_error_code
krb5int_parse_enctype_list(krb5_context context, const char *profkey,
                           char *profstr, krb5_enctype *default_list,
                           krb5_enctype **result)
{
    char *token, *delim = " \t\r\n,", *save = NULL;
    krb5_boolean sel, weak = context->allow_weak_crypto;
    krb5_enctype etype, *list;
    unsigned int i;

    *result = NULL;

    /* Set up an empty list.  Allocation failure is detected at the end. */
    list = malloc(sizeof(krb5_enctype));
    if (list != NULL)
        list[0] = 0;

    /* Walk through the words in profstr. */
    for (token = strtok_r(profstr, delim, &save); token;
         token = strtok_r(NULL, delim, &save)) {
        /* Determine if we are adding or removing enctypes. */
        sel = TRUE;
        if (*token == '+' || *token == '-')
            sel = (*token++ == '+');

        if (strcasecmp(token, "DEFAULT") == 0) {
            /* Set all enctypes in the default list. */
            for (i = 0; default_list[i]; i++)
                mod_list(default_list[i], sel, weak, &list);
        } else if (strcasecmp(token, "des") == 0) {
            mod_list(ENCTYPE_DES_CBC_CRC, sel, weak, &list);
            mod_list(ENCTYPE_DES_CBC_MD5, sel, weak, &list);
            mod_list(ENCTYPE_DES_CBC_MD4, sel, weak, &list);
        } else if (strcasecmp(token, "des3") == 0) {
            mod_list(ENCTYPE_DES3_CBC_SHA1, sel, weak, &list);
        } else if (strcasecmp(token, "aes") == 0) {
            mod_list(ENCTYPE_AES256_CTS_HMAC_SHA1_96, sel, weak, &list);
            mod_list(ENCTYPE_AES128_CTS_HMAC_SHA1_96, sel, weak, &list);
            mod_list(ENCTYPE_AES256_CTS_HMAC_SHA384_192, sel, weak, &list);
            mod_list(ENCTYPE_AES128_CTS_HMAC_SHA256_128, sel, weak, &list);
        } else if (strcasecmp(token, "rc4") == 0) {
            mod_list(ENCTYPE_ARCFOUR_HMAC, sel, weak, &list);
        } else if (strcasecmp(token, "camellia") == 0) {
            mod_list(ENCTYPE_CAMELLIA256_CTS_CMAC, sel, weak, &list);
            mod_list(ENCTYPE_CAMELLIA128_CTS_CMAC, sel, weak, &list);
        } else if (krb5_string_to_enctype(token, &etype) == 0) {
            /* Set a specific enctype. */
            mod_list(etype, sel, weak, &list);
        } else {
            TRACE_ENCTYPE_LIST_UNKNOWN(context, profkey, token);
        }
    }

    if (list == NULL)
        return ENOMEM;
    *result = list;
    return 0;
}

/*
 * Set *etypes_ptr to a zero-terminated list of enctypes.  ctx_list
 * (containing application-specified enctypes) is used if non-NULL;
 * otherwise the libdefaults profile string specified by profkey is
 * used.  default_list is the default enctype list to be used while
 * parsing profile strings, and is also used if the profile string is
 * not set.
 */
static krb5_error_code
get_profile_etype_list(krb5_context context, krb5_enctype **etypes_ptr,
                       char *profkey, krb5_enctype *ctx_list,
                       krb5_enctype *default_list)
{
    krb5_enctype *etypes;
    krb5_error_code code;
    char *profstr;

    *etypes_ptr = NULL;

    if (ctx_list) {
        /* Use application defaults. */
        code = k5_copy_etypes(ctx_list, &etypes);
        if (code)
            return code;
    } else {
        /* Parse profile setting, or "DEFAULT" if not specified. */
        code = profile_get_string(context->profile, KRB5_CONF_LIBDEFAULTS,
                                  profkey, NULL, "DEFAULT", &profstr);
        if (code)
            return code;
        code = krb5int_parse_enctype_list(context, profkey, profstr,
                                          default_list, &etypes);
        profile_release_string(profstr);
        if (code)
            return code;
    }

    if (etypes[0] == 0) {
        free(etypes);
        return KRB5_CONFIG_ETYPE_NOSUPP;
    }

    *etypes_ptr = etypes;
    return 0;
}

krb5_error_code
krb5_get_default_in_tkt_ktypes(krb5_context context, krb5_enctype **ktypes)
{
    return get_profile_etype_list(context, ktypes,
                                  KRB5_CONF_DEFAULT_TKT_ENCTYPES,
                                  context->in_tkt_etypes,
                                  default_enctype_list);
}

void
KRB5_CALLCONV
krb5_free_enctypes(krb5_context context, krb5_enctype *val)
{
    free (val);
}

krb5_error_code
KRB5_CALLCONV
krb5_get_tgs_ktypes(krb5_context context, krb5_const_principal princ, krb5_enctype **ktypes)
{
    if (context->use_conf_ktypes)
        /* This one is set *only* by reading the config file; it's not
           set by the application.  */
        return get_profile_etype_list(context, ktypes,
                                      KRB5_CONF_DEFAULT_TGS_ENCTYPES, NULL,
                                      default_enctype_list);
    else
        return get_profile_etype_list(context, ktypes,
                                      KRB5_CONF_DEFAULT_TGS_ENCTYPES,
                                      context->tgs_etypes,
                                      default_enctype_list);
}

krb5_error_code KRB5_CALLCONV
krb5_get_permitted_enctypes(krb5_context context, krb5_enctype **ktypes)
{
    return get_profile_etype_list(context, ktypes,
                                  KRB5_CONF_PERMITTED_ENCTYPES,
                                  context->tgs_etypes, default_enctype_list);
}

krb5_boolean
krb5_is_permitted_enctype(krb5_context context, krb5_enctype etype)
{
    krb5_enctype *list;
    krb5_boolean ret;

    if (krb5_get_permitted_enctypes(context, &list))
        return FALSE;
    ret = k5_etypes_contains(list, etype);
    krb5_free_enctypes(context, list);
    return ret;
}
