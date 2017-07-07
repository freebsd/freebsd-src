/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
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
/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved
 */

#include    "k5-int.h"
#include    <kdb.h>
#include    <ctype.h>
#include    <pwd.h>
#include    <syslog.h>
#include    "server_internal.h"
#include    <adm_proto.h>

kadm5_ret_t
init_pwqual(kadm5_server_handle_t handle)
{
    krb5_error_code ret;
    pwqual_handle *list;
    const char *dict_file = NULL;

    /* Register the built-in password quality modules. */
    ret = k5_plugin_register(handle->context, PLUGIN_INTERFACE_PWQUAL,
                             "dict", pwqual_dict_initvt);
    if (ret != 0)
        return ret;
    ret = k5_plugin_register(handle->context, PLUGIN_INTERFACE_PWQUAL,
                             "empty", pwqual_empty_initvt);
    if (ret != 0)
        return ret;
    ret = k5_plugin_register(handle->context, PLUGIN_INTERFACE_PWQUAL,
                             "hesiod", pwqual_hesiod_initvt);
    if (ret != 0)
        return ret;
    ret = k5_plugin_register(handle->context, PLUGIN_INTERFACE_PWQUAL,
                             "princ", pwqual_princ_initvt);
    if (ret != 0)
        return ret;

    /* Load all available password quality modules. */
    if (handle->params.mask & KADM5_CONFIG_DICT_FILE)
        dict_file = handle->params.dict_file;
    ret = k5_pwqual_load(handle->context, dict_file, &list);
    if (ret != 0)
        return ret;

    handle->qual_handles = list;
    return 0;
}

/* Check that a password meets the quality constraints given in pol. */
static kadm5_ret_t
check_against_policy(kadm5_server_handle_t handle, const char *password,
                     kadm5_policy_ent_t pol)
{
    int hasupper = 0, haslower = 0, hasdigit = 0, haspunct = 0, hasspec = 0;
    int c, nclasses;

    /* Check against the policy's minimum length. */
    if (strlen(password) < (size_t)pol->pw_min_length)
        return KADM5_PASS_Q_TOOSHORT;

    /* Check against the policy's minimum number of character classes. */
    while ((c = (unsigned char)*password++) != '\0') {
        if (islower(c))
            haslower = 1;
        else if (isupper(c))
            hasupper = 1;
        else if (isdigit(c))
            hasdigit = 1;
        else if (ispunct(c))
            haspunct = 1;
        else
            hasspec = 1;
    }
    nclasses = hasupper + haslower + hasdigit + haspunct + hasspec;
    if (nclasses < pol->pw_min_classes)
        return KADM5_PASS_Q_CLASS;
    return KADM5_OK;
}

/* Check a password against all available password quality plugin modules
 * and against policy. */
kadm5_ret_t
passwd_check(kadm5_server_handle_t handle, const char *password,
             kadm5_policy_ent_t policy, krb5_principal princ)
{
    krb5_error_code ret;
    pwqual_handle *h;
    const char *polname = (policy == NULL) ? NULL : policy->policy;

    if (policy != NULL) {
        ret = check_against_policy(handle, password, policy);
        if (ret != 0)
            return ret;
    }
    for (h = handle->qual_handles; *h != NULL; h++) {
        ret = k5_pwqual_check(handle->context, *h, password, polname, princ);
        if (ret != 0) {
            const char *e = krb5_get_error_message(handle->context, ret);
            const char *modname = k5_pwqual_name(handle->context, *h);
            char *princname;
            if (krb5_unparse_name(handle->context, princ, &princname) != 0)
                princname = NULL;
            krb5_klog_syslog(LOG_ERR,
                             _("password quality module %s rejected password "
                               "for %s: %s"), modname,
                             princname ? princname : "(can't unparse)", e);
            krb5_free_error_message(handle->context, e);
            free(princname);
            return ret;
        }
    }
    return 0;
}

void
destroy_pwqual(kadm5_server_handle_t handle)
{
    k5_pwqual_free_handles(handle->context, handle->qual_handles);
    handle->qual_handles = NULL;
}
