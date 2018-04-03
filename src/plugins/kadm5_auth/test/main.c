/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kadm5_auth/test/main.c - test modules for kadm5_auth interface */
/*
 * Copyright (C) 2017 by the Massachusetts Institute of Technology.
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

/*
 * This file implements two testing kadm5_auth modules, the welcomer and the
 * bouncer.  The welcomer implements permissive behavior, while the bouncer
 * implements restrictive behavior.
 *
 * Module data objects and restrictions are adequately tested by the acl
 * module, so we do not test them here.  Focus instead on the ability to
 * examine principal and policy objects and to perform DB operations.
 */

#include "k5-int.h"
#include <kadm5/admin.h>
#include <krb5/kadm5_auth_plugin.h>

krb5_error_code
kadm5_auth_welcomer_initvt(krb5_context context, int maj_ver, int min_ver,
                           krb5_plugin_vtable vtable);
krb5_error_code
kadm5_auth_bouncer_initvt(krb5_context context, int maj_ver, int min_ver,
                          krb5_plugin_vtable vtable);

/* The welcomer authorizes all getprinc operations, since kadmin uses them as a
 * precursor to modprinc. */
static krb5_error_code
welcomer_getprinc(krb5_context context, kadm5_auth_moddata data,
                  krb5_const_principal client, krb5_const_principal target)
{
    return 0;
}

/* The welcomer authorizes addprinc operations which set a policy "VIP". */
static krb5_error_code
welcomer_addprinc(krb5_context context, kadm5_auth_moddata data,
                  krb5_const_principal client, krb5_const_principal target,
                  const struct _kadm5_principal_ent_t *ent, long mask,
                  struct kadm5_auth_restrictions **rs_out)
{
    if ((mask & KADM5_POLICY) && strcmp(ent->policy, "VIP") == 0)
        return 0;
    return KRB5_PLUGIN_NO_HANDLE;
}

/* The bouncer denies addprinc operations which include a maximum lifetime. */
static krb5_error_code
bouncer_addprinc(krb5_context context, kadm5_auth_moddata data,
                 krb5_const_principal client, krb5_const_principal target,
                 const struct _kadm5_principal_ent_t *ent, long mask,
                 struct kadm5_auth_restrictions **rs_out)
{
    return (mask & KADM5_MAX_LIFE) ? EPERM : KRB5_PLUGIN_NO_HANDLE;
}

/* The welcomer authorizes modprinc operations which only set maxrenewlife. */
static krb5_error_code
welcomer_modprinc(krb5_context context, kadm5_auth_moddata data,
                  krb5_const_principal client, krb5_const_principal target,
                  const struct _kadm5_principal_ent_t *ent, long mask,
                  struct kadm5_auth_restrictions **rs_out)
{
    return (mask == KADM5_MAX_RLIFE) ? 0 : KRB5_PLUGIN_NO_HANDLE;
}

/* The bouncer denies modprinc operations if the target principal has an even
 * number of components. */
static krb5_error_code
bouncer_modprinc(krb5_context context, kadm5_auth_moddata data,
                 krb5_const_principal client, krb5_const_principal target,
                 const struct _kadm5_principal_ent_t *ent, long mask,
                 struct kadm5_auth_restrictions **rs_out)
{
    return (target->length % 2 == 0) ? EPERM : KRB5_PLUGIN_NO_HANDLE;
}

/* The welcomer authorizes setstr operations for the attribute "note". */
static krb5_error_code
welcomer_setstr(krb5_context context, kadm5_auth_moddata data,
                krb5_const_principal client, krb5_const_principal target,
                const char *key, const char *value)
{
    return (strcmp(key, "note") == 0) ? 0 : KRB5_PLUGIN_NO_HANDLE;
}

/* The bouncer denies setstr operations if the value is more than 10 bytes. */
static krb5_error_code
bouncer_setstr(krb5_context context, kadm5_auth_moddata data,
               krb5_const_principal client, krb5_const_principal target,
               const char *key, const char *value)
{
    return (strlen(value) > 10) ? EPERM : KRB5_PLUGIN_NO_HANDLE;
}

/* The welcomer authorizes delprinc operations if the target principal starts
 * with "d". */
static krb5_error_code
welcomer_delprinc(krb5_context context, kadm5_auth_moddata data,
                  krb5_const_principal client, krb5_const_principal target)
{
    if (target->length > 0 && target->data[0].length > 0 &&
        *target->data[0].data == 'd')
        return 0;
    return KRB5_PLUGIN_NO_HANDLE;
}

/* The bouncer denies delprinc operations if the target principal has the
 * "nodelete" string attribute. */
static krb5_error_code
bouncer_delprinc(krb5_context context, kadm5_auth_moddata data,
                 krb5_const_principal client, krb5_const_principal target)
{
    krb5_error_code ret;
    krb5_db_entry *ent;
    char *val = NULL;

    if (krb5_db_get_principal(context, target, 0, &ent) != 0)
        return EPERM;
    ret = krb5_dbe_get_string(context, ent, "nodelete", &val);
    krb5_db_free_principal(context, ent);
    ret = (ret != 0 || val != NULL) ? EPERM : KRB5_PLUGIN_NO_HANDLE;
    krb5_dbe_free_string(context, val);
    return ret;
}

/* The welcomer authorizes rename operations if the first components of the
 * principals have the same length. */
static krb5_error_code
welcomer_renprinc(krb5_context context, kadm5_auth_moddata data,
                  krb5_const_principal client, krb5_const_principal src,
                  krb5_const_principal dest)
{
    if (src->length > 0 && dest->length > 0 &&
        src->data[0].length == dest->data[0].length)
        return 0;
    return KRB5_PLUGIN_NO_HANDLE;
}

/* The bouncer denies rename operations if the source principal starts with
 * "a". */
static krb5_error_code
bouncer_renprinc(krb5_context context, kadm5_auth_moddata data,
                 krb5_const_principal client, krb5_const_principal src,
                 krb5_const_principal dest)
{
    if (src->length > 0 && src->data[0].length > 0 &&
        *src->data[0].data == 'a')
        return EPERM;
    return KRB5_PLUGIN_NO_HANDLE;
}

/* The welcomer authorizes addpol operations which set a minlength of 3. */
static krb5_error_code
welcomer_addpol(krb5_context context, kadm5_auth_moddata data,
                krb5_const_principal client, const char *policy,
                const struct _kadm5_policy_ent_t *ent, long mask)
{
    if ((mask & KADM5_PW_MIN_LENGTH) && ent->pw_min_length == 3)
        return 0;
    return KRB5_PLUGIN_NO_HANDLE;
}

/* The bouncer denies addpol operations if the name is 3 bytes or less. */
static krb5_error_code
bouncer_addpol(krb5_context context, kadm5_auth_moddata data,
               krb5_const_principal client, const char *policy,
               const struct _kadm5_policy_ent_t *ent, long mask)
{
    return (strlen(policy) <= 3) ? EPERM : KRB5_PLUGIN_NO_HANDLE;
}

/* The welcomer authorizes modpol operations which only change min_life. */
static krb5_error_code
welcomer_modpol(krb5_context context, kadm5_auth_moddata data,
                krb5_const_principal client, const char *policy,
                const struct _kadm5_policy_ent_t *ent, long mask)
{
    return (mask == KADM5_PW_MIN_LIFE) ? 0 : KRB5_PLUGIN_NO_HANDLE;
}

/* The bouncer denies modpol operations which set pw_min_life above 10. */
static krb5_error_code
bouncer_modpol(krb5_context context, kadm5_auth_moddata data,
               krb5_const_principal client, const char *policy,
               const struct _kadm5_policy_ent_t *ent, long mask)
{
    if ((mask & KADM5_PW_MIN_LIFE) && ent->pw_min_life > 10)
        return EPERM;
    return KRB5_PLUGIN_NO_HANDLE;
}

/* The welcomer authorizes getpol operations if the policy and client principal
 * policy have the same length. */
static krb5_error_code
welcomer_getpol(krb5_context context, kadm5_auth_moddata data,
                krb5_const_principal client, const char *policy,
                const char *client_policy)
{
    if (client_policy != NULL && strlen(policy) == strlen(client_policy))
        return 0;
    return KRB5_PLUGIN_NO_HANDLE;
}

/* The bouncer denies getpol operations if the policy name begins with 'x'. */
static krb5_error_code
bouncer_getpol(krb5_context context, kadm5_auth_moddata data,
               krb5_const_principal client, const char *policy,
               const char *client_policy)
{
    return (*policy == 'x') ? EPERM : KRB5_PLUGIN_NO_HANDLE;
}

/* The welcomer counts end calls by incrementing the "ends" string attribute on
 * the "opcount" principal, if it exists. */
static void
welcomer_end(krb5_context context, kadm5_auth_moddata data)
{
    krb5_principal princ = NULL;
    krb5_db_entry *ent = NULL;
    char *val = NULL, buf[10];

    if (krb5_parse_name(context, "opcount", &princ) != 0)
        goto cleanup;
    if (krb5_db_get_principal(context, princ, 0, &ent) != 0)
        goto cleanup;
    if (krb5_dbe_get_string(context, ent, "ends", &val) != 0 || val == NULL)
        goto cleanup;
    snprintf(buf, sizeof(buf), "%d", atoi(val) + 1);
    if (krb5_dbe_set_string(context, ent, "ends", buf) != 0)
        goto cleanup;
    ent->mask = KADM5_TL_DATA;
    krb5_db_put_principal(context, ent);

cleanup:
    krb5_dbe_free_string(context, val);
    krb5_db_free_principal(context, ent);
    krb5_free_principal(context, princ);
}

krb5_error_code
kadm5_auth_welcomer_initvt(krb5_context context, int maj_ver, int min_ver,
                           krb5_plugin_vtable vtable)
{
    kadm5_auth_vtable vt = (kadm5_auth_vtable)vtable;

    vt->name = "welcomer";
    vt->addprinc = welcomer_addprinc;
    vt->modprinc = welcomer_modprinc;
    vt->setstr = welcomer_setstr;
    vt->delprinc = welcomer_delprinc;
    vt->renprinc = welcomer_renprinc;
    vt->getprinc = welcomer_getprinc;
    vt->addpol = welcomer_addpol;
    vt->modpol = welcomer_modpol;
    vt->getpol = welcomer_getpol;
    vt->end = welcomer_end;
    return 0;
}

krb5_error_code
kadm5_auth_bouncer_initvt(krb5_context context, int maj_ver, int min_ver,
                          krb5_plugin_vtable vtable)
{
    kadm5_auth_vtable vt = (kadm5_auth_vtable)vtable;

    vt->name = "bouncer";
    vt->addprinc = bouncer_addprinc;
    vt->modprinc = bouncer_modprinc;
    vt->setstr = bouncer_setstr;
    vt->delprinc = bouncer_delprinc;
    vt->renprinc = bouncer_renprinc;
    vt->addpol = bouncer_addpol;
    vt->modpol = bouncer_modpol;
    vt->getpol = bouncer_getpol;
    return 0;
}
