/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/t_copy_context.C - Test program for krb5_copy_context */
/*
 * Copyright (C) 2013 by the Massachusetts Institute of Technology.
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

#include <k5-int.h>

static void
trace(krb5_context ctx, const krb5_trace_info *info, void *data)
{
}

static void
check(int cond)
{
    if (!cond)
        abort();
}

static void
compare_string(const char *str1, const char *str2)
{
    check((str1 == NULL) == (str2 == NULL));
    if (str1 != NULL)
        check(strcmp(str1, str2) == 0);
}

static void
compare_etypes(krb5_enctype *list1, krb5_enctype *list2)
{
    check((list1 == NULL) == (list2 == NULL));
    if (list1 == NULL)
        return;
    while (*list1 != ENCTYPE_NULL && *list1 == *list2)
        list1++, list2++;
    check(*list1 == *list2);
}

/* Check that the context c is a valid copy of the reference context r. */
static void
check_context(krb5_context c, krb5_context r)
{
    int i;

    /* Check fields which should have been propagated from r. */
    compare_etypes(c->in_tkt_etypes, r->in_tkt_etypes);
    compare_etypes(c->tgs_etypes, r->tgs_etypes);
    check(c->os_context.time_offset == r->os_context.time_offset);
    check(c->os_context.usec_offset == r->os_context.usec_offset);
    check(c->os_context.os_flags == r->os_context.os_flags);
    compare_string(c->os_context.default_ccname, r->os_context.default_ccname);
    check(c->clockskew == r->clockskew);
    check(c->kdc_req_sumtype == r->kdc_req_sumtype);
    check(c->default_ap_req_sumtype == r->default_ap_req_sumtype);
    check(c->default_safe_sumtype == r->default_safe_sumtype);
    check(c->kdc_default_options == r->kdc_default_options);
    check(c->library_options == r->library_options);
    check(c->profile_secure == r->profile_secure);
    check(c->fcc_default_format == r->fcc_default_format);
    check(c->udp_pref_limit == r->udp_pref_limit);
    check(c->use_conf_ktypes == r->use_conf_ktypes);
    check(c->allow_weak_crypto == r->allow_weak_crypto);
    check(c->ignore_acceptor_hostname == r->ignore_acceptor_hostname);
    check(c->dns_canonicalize_hostname == r->dns_canonicalize_hostname);
    compare_string(c->plugin_base_dir, r->plugin_base_dir);

    /* Check fields which don't propagate. */
    check(c->dal_handle == NULL);
    check(c->ser_ctx_count == 0);
    check(c->ser_ctx == NULL);
    check(c->prompt_types == NULL);
    check(c->libkrb5_plugins.files == NULL);
    check(c->preauth_context == NULL);
    check(c->ccselect_handles == NULL);
    check(c->localauth_handles == NULL);
    check(c->hostrealm_handles == NULL);
    check(c->err.code == 0);
    check(c->err.msg == NULL);
    check(c->kdblog_context == NULL);
    check(c->trace_callback == NULL);
    check(c->trace_callback_data == NULL);
    for (i = 0; i < PLUGIN_NUM_INTERFACES; i++) {
        check(c->plugins[i].modules == NULL);
        check(!c->plugins[i].configured);
    }
}

int
main(int argc, char **argv)
{
    krb5_context ctx, ctx2;
    krb5_plugin_initvt_fn *mods;
    const krb5_enctype etypes1[] = { ENCTYPE_DES3_CBC_SHA1, 0 };
    const krb5_enctype etypes2[] = { ENCTYPE_AES128_CTS_HMAC_SHA1_96,
                                     ENCTYPE_AES256_CTS_HMAC_SHA1_96, 0 };
    krb5_prompt_type ptypes[] = { KRB5_PROMPT_TYPE_PASSWORD };

    /* Copy a default context and verify the result. */
    check(krb5_init_context(&ctx) == 0);
    check(krb5_copy_context(ctx, &ctx2) == 0);
    check_context(ctx2, ctx);
    krb5_free_context(ctx2);

    /* Set non-default values for all of the propagated fields in ctx. */
    ctx->allow_weak_crypto = TRUE;
    check(krb5_set_default_in_tkt_ktypes(ctx, etypes1) == 0);
    check(krb5_set_default_tgs_enctypes(ctx, etypes2) == 0);
    check(krb5_set_debugging_time(ctx, 1234, 5678) == 0);
    check(krb5_cc_set_default_name(ctx, "defccname") == 0);
    check(krb5_set_default_realm(ctx, "defrealm") == 0);
    ctx->clockskew = 18;
    ctx->kdc_req_sumtype = CKSUMTYPE_NIST_SHA;
    ctx->default_ap_req_sumtype = CKSUMTYPE_HMAC_SHA1_96_AES128;
    ctx->default_safe_sumtype = CKSUMTYPE_HMAC_SHA1_96_AES256;
    ctx->kdc_default_options = KDC_OPT_FORWARDABLE;
    ctx->library_options = 0;
    ctx->profile_secure = TRUE;
    ctx->udp_pref_limit = 2345;
    ctx->use_conf_ktypes = TRUE;
    ctx->ignore_acceptor_hostname = TRUE;
    ctx->dns_canonicalize_hostname = FALSE;
    free(ctx->plugin_base_dir);
    check((ctx->plugin_base_dir = strdup("/a/b/c/d")) != NULL);

    /* Also set some of the non-propagated fields. */
    ctx->prompt_types = ptypes;
    check(k5_plugin_load_all(ctx, PLUGIN_INTERFACE_PWQUAL, &mods) == 0);
    k5_plugin_free_modules(ctx, mods);
    k5_setmsg(ctx, ENOMEM, "nooooooooo");
    krb5_set_trace_callback(ctx, trace, ctx);

    /* Copy the intentionally messy context and verify the result. */
    check(krb5_copy_context(ctx, &ctx2) == 0);
    check_context(ctx2, ctx);
    krb5_free_context(ctx2);

    krb5_free_context(ctx);
    return 0;
}
