/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kadm5_hook/test/main.c */
/*
 * Copyright (C) 2010 by the Massachusetts Institute of Technology.
 * All rights reserved.
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

/**
 * @file plugins/kadm5_hook/test/main.c
 *
 * This is a test kadm5_hook plugin. If enabled, it will print when kadm5_hook
 * calls are made.
 */

#include <krb5/krb5.h>
#include <krb5/kadm5_hook_plugin.h>
#include <stdio.h>
#include <assert.h>

static void
log_call(krb5_context context,
         const char *function,
         int stage,
         krb5_principal princ)
{
    char *unparsed = NULL;
    krb5_error_code ret;
    ret = krb5_unparse_name(context, princ, &unparsed);
    assert(ret == 0);
    printf("%s: stage %s principal %s\n",
           function,
           (stage == KADM5_HOOK_STAGE_PRECOMMIT) ? "precommit" : "postcommit",
           unparsed);
    if (unparsed)
        krb5_free_unparsed_name(context, unparsed);
}

static kadm5_ret_t
chpass(krb5_context context,
       kadm5_hook_modinfo *modinfo,
       int stage,
       krb5_principal princ, krb5_boolean keepold,
       int n_ks_tuple,
       krb5_key_salt_tuple *ks_tuple,
       const char *newpass)
{
    log_call(context, "chpass", stage, princ);
    return 0;
}


static kadm5_ret_t
create(krb5_context context,
       kadm5_hook_modinfo *modinfo,
       int stage,
       kadm5_principal_ent_t princ, long mask,
       int n_ks_tuple,
       krb5_key_salt_tuple *ks_tuple,
       const char *newpass)
{
    log_call(context, "create", stage, princ->principal);
    return 0;
}

static kadm5_ret_t
rename_hook(krb5_context context, kadm5_hook_modinfo *modinfo, int stage,
            krb5_principal oprinc, krb5_principal nprinc)
{
    log_call(context, "rename", stage, oprinc);
    return 0;
}

krb5_error_code
kadm5_hook_test_initvt(krb5_context context, int maj_ver, int min_ver,
                       krb5_plugin_vtable vtable);

krb5_error_code
kadm5_hook_test_initvt(krb5_context context, int maj_ver, int min_ver,
                       krb5_plugin_vtable vtable)
{
    kadm5_hook_vftable_1 *vt = (kadm5_hook_vftable_1 *) vtable;
    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;

    vt->name = "test";
    vt->chpass = chpass;
    vt->create = create;
    vt->rename = rename_hook;
    return 0;
}
