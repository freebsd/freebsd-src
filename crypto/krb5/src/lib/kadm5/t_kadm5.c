/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/kadm5/t_kadm5.c - API tests for libkadm5 */
/*
 * Copyright (C) 2021 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"
#include <kadm5/admin.h>

static uint32_t api;
static krb5_boolean rpc;

static krb5_context context;

/* These must match the creation commands in t_kadm5.py. */
#define ADMIN_PASSWORD "admin"
#define USER_PASSWORD "us3r"

/* This list must match the supported_enctypes setting in t_kadm5.py. */
static krb5_enctype
default_supported_enctypes[] = {
    ENCTYPE_AES256_CTS_HMAC_SHA1_96, ENCTYPE_AES128_CTS_HMAC_SHA1_96,
    ENCTYPE_NULL
};

static void
check(krb5_error_code code)
{
    assert(code == 0);
}

static void
check_fail(krb5_error_code code, krb5_error_code expected)
{
    assert(code == expected);
}

/*
 * Initialize a handle using the global context.  The caller must destroy this
 * handle before initializing another one.  If the client name begins with '$',
 * authenticate to kadmin/changepw; otherwise authenticate to kadmin/admin.  If
 * client is null, return a null handle.
 */
static void *
get_handle(char *client)
{
    void *handle;
    char *service, *pass;

    if (client == NULL)
        return NULL;

    if (*client == '$') {
        service = KADM5_CHANGEPW_SERVICE;
        client++;
    } else {
        service = KADM5_ADMIN_SERVICE;
    }
    pass = (strcmp(client, "user") == 0) ? USER_PASSWORD : ADMIN_PASSWORD;

    check(kadm5_init(context, client, pass, service, NULL,
                     KADM5_STRUCT_VERSION, api, NULL, &handle));
    return handle;
}

static void
free_handle(void *handle)
{
    if (handle != NULL)
        check(kadm5_destroy(handle));
}

static krb5_principal
parse_princ(const char *str)
{
    krb5_principal princ;

    check(krb5_parse_name(context, str, &princ));
    return princ;
}

static void
create_simple_policy(char *name)
{
    void *handle = get_handle("admin");
    kadm5_policy_ent_rec ent;

    memset(&ent, 0, sizeof(ent));
    ent.policy = name;
    check(kadm5_create_policy(handle, &ent, KADM5_POLICY));
    free_handle(handle);
}

static void
delete_policy(char *name)
{
    void *handle = get_handle("admin");

    check(kadm5_delete_policy(handle, name));
    free_handle(handle);
}

static void
compare_policy(kadm5_policy_ent_t x, uint32_t mask)
{
    kadm5_policy_ent_rec g;
    void *handle = get_handle("admin");

    check(kadm5_get_policy(handle, x->policy, &g));

    assert(strcmp(g.policy, x->policy) == 0);
    if (mask & KADM5_PW_MAX_LIFE)
        assert(g.pw_max_life == x->pw_max_life);
    if (mask & KADM5_PW_MIN_LIFE)
        assert(g.pw_min_life == x->pw_min_life);
    if (mask & KADM5_PW_MIN_LENGTH)
        assert(g.pw_min_length == x->pw_min_length);
    if (mask & KADM5_PW_MIN_CLASSES)
        assert(g.pw_min_classes == x->pw_min_classes);
    if (mask & KADM5_PW_HISTORY_NUM)
        assert(g.pw_history_num == x->pw_history_num);
    if (mask & KADM5_PW_MAX_FAILURE)
        assert(g.pw_max_fail == x->pw_max_fail);
    if (mask & KADM5_PW_FAILURE_COUNT_INTERVAL)
        assert(g.pw_failcnt_interval == x->pw_failcnt_interval);
    if (mask & KADM5_PW_LOCKOUT_DURATION)
        assert(g.pw_lockout_duration == x->pw_lockout_duration);

    check(kadm5_free_policy_ent(handle, &g));
    free_handle(handle);
}

static void
create_simple_princ(krb5_principal princ, char *policy)
{
    void *handle = get_handle("admin");
    kadm5_principal_ent_rec ent;
    uint32_t mask = KADM5_PRINCIPAL;

    memset(&ent, 0, sizeof(ent));
    ent.principal = princ;
    ent.policy = policy;
    if (policy != NULL)
        mask |= KADM5_POLICY;
    check(kadm5_create_principal(handle, &ent, mask, "pw"));
    free_handle(handle);
}

static void
delete_princ(krb5_principal princ)
{
    void *handle = get_handle("admin");

    check(kadm5_delete_principal(handle, princ));
    free_handle(handle);
}

static void
compare_key_data(kadm5_principal_ent_t ent, const krb5_enctype *etypes)
{
    int i;

    for (i = 0; etypes[i] != ENCTYPE_NULL; i++) {
        assert(i < ent->n_key_data);
        assert(ent->key_data[i].key_data_ver >= 1);
        assert(ent->key_data[i].key_data_type[0] == etypes[i]);
    }
}

static void
compare_princ(kadm5_principal_ent_t x, uint32_t mask)
{
    void *handle = get_handle("admin");
    kadm5_principal_ent_rec g;
    kadm5_policy_ent_rec pol;

    check(kadm5_get_principal(handle, x->principal, &g,
                              KADM5_PRINCIPAL_NORMAL_MASK));

    assert(krb5_principal_compare(context, g.principal, x->principal));
    if (mask & KADM5_POLICY)
        assert(strcmp(g.policy, x->policy) == 0);
    if (mask & KADM5_PRINC_EXPIRE_TIME)
        assert(g.princ_expire_time == x->princ_expire_time);
    if (mask & KADM5_MAX_LIFE)
        assert(g.max_life == x->max_life);
    if (mask & KADM5_MAX_RLIFE)
        assert(g.max_renewable_life == x->max_renewable_life);
    if (mask & KADM5_FAIL_AUTH_COUNT)
        assert(g.fail_auth_count == x->fail_auth_count);
    if (mask & KADM5_ATTRIBUTES)
        assert(g.attributes == x->attributes);
    if (mask & KADM5_KVNO)
        assert(g.kvno == x->kvno);

    if (mask & KADM5_PW_EXPIRATION) {
        assert(g.pw_expiration == x->pw_expiration);
    } else if ((mask & KADM5_POLICY) &&
               kadm5_get_policy(handle, g.policy, &pol) == 0) {
        /* Check the policy pw_max_life computation. */
        if (pol.pw_max_life != 0) {
            assert(ts_incr(g.last_pwd_change, pol.pw_max_life) ==
                   g.pw_expiration);
        } else {
            assert(g.pw_expiration == 0);
        }
        check(kadm5_free_policy_ent(handle, &pol));
    }

    if (mask & KADM5_POLICY_CLR) {
        assert(g.policy == NULL);
        if (!(mask & KADM5_PW_EXPIRATION))
            assert(g.pw_expiration == 0);
    }

    check(kadm5_free_principal_ent(handle, &g));
    free_handle(handle);
}

static void
kinit(krb5_ccache cc, const char *user, const char *pass, const char *service)
{
    krb5_get_init_creds_opt *opt;
    krb5_principal client = parse_princ(user);
    krb5_creds creds;

    check(krb5_get_init_creds_opt_alloc(context, &opt));
    check(krb5_get_init_creds_opt_set_out_ccache(context, opt, cc));
    check(krb5_get_init_creds_password(context, &creds, client, pass, NULL,
                                       NULL, 0, service, opt));
    krb5_get_init_creds_opt_free(context, opt);
    krb5_free_cred_contents(context, &creds);
    krb5_free_principal(context, client);
}

static void
cpw_test_fail(char *user, krb5_principal princ, char *pass,
              krb5_error_code code)
{
    void *handle = get_handle(user);

    check_fail(kadm5_chpass_principal(handle, princ, pass), code);
    free_handle(handle);
}

static void
cpw_test_succeed(char *user, krb5_principal princ, char *pass)
{
    cpw_test_fail(user, princ, pass, 0);
}

static void
test_chpass()
{
    krb5_principal princ = parse_princ("chpass-test");
    krb5_principal hist_princ = parse_princ("kadmin/history");
    kadm5_principal_ent_rec ent;
    void *handle;

    /* Specify a policy so that kadmin/history is created. */
    create_simple_princ(princ, "minlife-pol");

    /* Check kvno and enctypes after a password change. */
    handle = get_handle("admin");
    check(kadm5_chpass_principal(handle, princ, "newpassword"));
    check(kadm5_get_principal(handle, princ, &ent, KADM5_KEY_DATA));
    compare_key_data(&ent, default_supported_enctypes);
    assert(ent.key_data[0].key_data_kvno == 2);
    check(kadm5_free_principal_ent(handle, &ent));
    free_handle(handle);

    /* Fails for protected principal. */
    cpw_test_fail("admin", hist_princ, "pw", KADM5_PROTECT_PRINCIPAL);

    /* Fails over RPC if "change" ACL is not granted, or if we authenticated to
     * kadmin/changepw and are changing another principal's password. */
    if (rpc) {
        cpw_test_succeed("admin/modify", princ, "pw2");
        cpw_test_fail("admin/none", princ, "pw3", KADM5_AUTH_CHANGEPW);
        cpw_test_fail("$admin", princ, "pw3", KADM5_AUTH_CHANGEPW);
    }

    /* Fails with null handle or principal name. */
    cpw_test_fail(NULL, princ, "pw", KADM5_BAD_SERVER_HANDLE);
    cpw_test_fail("admin", NULL, "pw", EINVAL);

    delete_princ(princ);
    krb5_free_principal(context, princ);
    krb5_free_principal(context, hist_princ);
}

static void
cpol_test_fail(char *user, kadm5_policy_ent_t ent, uint32_t mask,
               krb5_error_code code)
{
    void *handle = get_handle(user);

    check_fail(kadm5_create_policy(handle, ent, mask | KADM5_POLICY), code);
    free_handle(handle);
}

static void
cpol_test_compare(char *user, kadm5_policy_ent_t ent, uint32_t mask)
{
    cpol_test_fail(user, ent, mask, 0);
    compare_policy(ent, mask);
    delete_policy(ent->policy);
}

static void
test_create_policy()
{
    void *handle;
    kadm5_policy_ent_rec ent;

    memset(&ent, 0, sizeof(ent));

    /* Fails with undefined mask bit. */
    ent.policy = "create-policy-test";
    cpol_test_fail("admin", &ent, 0x10000000, KADM5_BAD_MASK);

    /* Fails without KADM5_POLICY mask bit. */
    handle = get_handle("admin");
    check_fail(kadm5_create_policy(handle, &ent, 0), KADM5_BAD_MASK);
    free_handle(handle);

    /* pw_min_life = 0 and pw_min_life != 0 */
    cpol_test_compare("admin", &ent, KADM5_PW_MIN_LIFE);
    ent.pw_min_life = 32;
    cpol_test_compare("admin", &ent, KADM5_PW_MIN_LIFE);

    /* pw_max_life = 0 and pw_max_life != 0 */
    cpol_test_compare("admin", &ent, KADM5_PW_MAX_LIFE);
    ent.pw_max_life = 32;
    cpol_test_compare("admin", &ent, KADM5_PW_MAX_LIFE);

    /* pw_min_length = 0 (rejected) and pw_min_length != 0 */
    cpol_test_fail("admin", &ent, KADM5_PW_MIN_LENGTH, KADM5_BAD_LENGTH);
    ent.pw_min_length = 32;
    cpol_test_compare("admin", &ent, KADM5_PW_MIN_LENGTH);

    /* pw_min_classes = 0 (rejected), 1, 5, 6 (rejected) */
    cpol_test_fail("admin", &ent, KADM5_PW_MIN_CLASSES, KADM5_BAD_CLASS);
    ent.pw_min_classes = 1;
    cpol_test_compare("admin", &ent, KADM5_PW_MIN_CLASSES);
    ent.pw_min_classes = 5;
    cpol_test_compare("admin", &ent, KADM5_PW_MIN_CLASSES);
    ent.pw_min_classes = 6;
    cpol_test_fail("admin", &ent, KADM5_PW_MIN_CLASSES, KADM5_BAD_CLASS);

    /* pw_history_num = 0 (rejected), 1, 10 */
    cpol_test_fail("admin", &ent, KADM5_PW_HISTORY_NUM, KADM5_BAD_HISTORY);
    ent.pw_history_num = 1;
    cpol_test_compare("admin", &ent, KADM5_PW_HISTORY_NUM);
    ent.pw_history_num = 10;
    cpol_test_compare("admin", &ent, KADM5_PW_HISTORY_NUM);

    if (api >= KADM5_API_VERSION_3) {
        ent.pw_max_fail = 2;
        cpol_test_compare("admin", &ent, KADM5_PW_MAX_FAILURE);
        ent.pw_failcnt_interval = 90;
        cpol_test_compare("admin", &ent,
                          KADM5_PW_FAILURE_COUNT_INTERVAL);
        ent.pw_lockout_duration = 180;
        cpol_test_compare("admin", &ent, KADM5_PW_LOCKOUT_DURATION);
    }

    /* Fails over RPC if "add" ACL is not granted, or if we authenticated to
     * kadmin/changepw. */
    if (rpc) {
        cpol_test_fail("$admin", &ent, 0, KADM5_AUTH_ADD);
        cpol_test_fail("admin/none", &ent, 0, KADM5_AUTH_ADD);
        cpol_test_fail("admin/get", &ent, 0, KADM5_AUTH_ADD);
        cpol_test_fail("admin/modify", &ent, 0, KADM5_AUTH_ADD);
        cpol_test_fail("admin/delete", &ent, 0, KADM5_AUTH_ADD);
        cpol_test_compare("admin/add", &ent, 0);
    }

    /* Fails with existing policy name. */
    ent.policy = "test-pol";
    cpol_test_fail("admin", &ent, 0, KADM5_DUP);

    /* Fails with null or empty policy name, or invalid character in name. */
    ent.policy = NULL;
    cpol_test_fail("admin", &ent, 0, EINVAL);
    ent.policy = "";
    cpol_test_fail("admin", &ent, 0, KADM5_BAD_POLICY);
    ent.policy = "pol\7";
    cpol_test_fail("admin", &ent, 0, KADM5_BAD_POLICY);

    /* Fails with null handle or policy ent. */
    cpol_test_fail(NULL, &ent, 0, KADM5_BAD_SERVER_HANDLE);
    cpol_test_fail("admin", NULL, 0, EINVAL);
}

static void
cprinc_test_fail(char *user, kadm5_principal_ent_t ent, uint32_t mask,
                 char *pass, krb5_error_code code)
{
    void *handle = get_handle(user);

    check_fail(kadm5_create_principal(handle, ent, mask | KADM5_PRINCIPAL,
                                      pass), code);
    free_handle(handle);
}

static void
cprinc_test_compare(char *user, kadm5_principal_ent_t ent, uint32_t mask,
                    char *pass)
{
    cprinc_test_fail(user, ent, mask, pass, 0);
    compare_princ(ent, mask);
    delete_princ(ent->principal);
}

static void
test_create_principal()
{
    void *handle;
    kadm5_principal_ent_rec ent;
    krb5_principal princ = parse_princ("create-principal-test");
    krb5_principal user_princ = parse_princ("user");

    memset(&ent, 0, sizeof(ent));
    ent.principal = princ;

    /* Fails with undefined or prohibited mask bit. */
    cprinc_test_fail("admin", &ent, 0x100000, "", KADM5_BAD_MASK);
    cprinc_test_fail("admin", &ent, KADM5_LAST_PWD_CHANGE, "pw",
                     KADM5_BAD_MASK);
    cprinc_test_fail("admin", &ent, KADM5_MOD_TIME, "pw", KADM5_BAD_MASK);
    cprinc_test_fail("admin", &ent, KADM5_MOD_NAME, "pw", KADM5_BAD_MASK);
    cprinc_test_fail("admin", &ent, KADM5_MKVNO, "pw", KADM5_BAD_MASK);
    cprinc_test_fail("admin", &ent, KADM5_AUX_ATTRIBUTES, "pw",
                     KADM5_BAD_MASK);

    /* Fails without KADM5_PRINCIPAL mask bit. */
    handle = get_handle("admin");
    check_fail(kadm5_create_principal(handle, &ent, 0, "pw"), KADM5_BAD_MASK);
    free_handle(handle);

    /* Fails with empty password or password prohibited by policy. */
    cprinc_test_fail("admin", &ent, 0, "", KADM5_PASS_Q_TOOSHORT);
    ent.policy = "test-pol";
    cprinc_test_fail("admin", &ent, KADM5_POLICY, "tP", KADM5_PASS_Q_TOOSHORT);
    cprinc_test_fail("admin", &ent, KADM5_POLICY, "testpassword",
                     KADM5_PASS_Q_CLASS);
    cprinc_test_fail("admin", &ent, KADM5_POLICY, "Abyssinia",
                     KADM5_PASS_Q_DICT);

    cprinc_test_compare("admin", &ent, 0, "pw");
    ent.policy = "nonexistent-pol";
    cprinc_test_compare("admin", &ent, KADM5_POLICY, "pw");
    cprinc_test_compare("admin/rename", &ent, KADM5_POLICY, "pw");

    /* Test pw_expiration explicit specifications vs. policy pw_max_life. */
    ent.policy = "test-pol";
    cprinc_test_compare("admin", &ent, KADM5_POLICY, "NotinTheDictionary");
    cprinc_test_compare("admin", &ent, KADM5_PRINC_EXPIRE_TIME, "pw");
    cprinc_test_compare("admin", &ent, KADM5_PW_EXPIRATION, "pw");
    cprinc_test_compare("admin", &ent, KADM5_POLICY | KADM5_PW_EXPIRATION,
                        "NotinTheDictionary");
    ent.pw_expiration = 1234;
    cprinc_test_compare("admin", &ent, KADM5_PW_EXPIRATION, "pw");
    cprinc_test_compare("admin", &ent, KADM5_POLICY | KADM5_PW_EXPIRATION,
                        "NotinTheDictionary");
    ent.pw_expiration = 999999999;
    cprinc_test_compare("admin", &ent, KADM5_POLICY | KADM5_PW_EXPIRATION,
                        "NotinTheDictionary");
    ent.policy = "dict-only-pol";
    cprinc_test_compare("admin", &ent, KADM5_POLICY | KADM5_PW_EXPIRATION,
                        "pw");

    /* Fails over RPC if "add" ACL is not granted, or if we authenticated to
     * kadmin/changepw. */
    if (rpc) {
        cprinc_test_fail("$admin", &ent, 0, "pw", KADM5_AUTH_ADD);
        cprinc_test_fail("admin/none", &ent, 0, "pw", KADM5_AUTH_ADD);
        cprinc_test_fail("admin/get", &ent, 0, "pw", KADM5_AUTH_ADD);
        cprinc_test_fail("admin/modify", &ent, 0, "pw", KADM5_AUTH_ADD);
        cprinc_test_fail("admin/delete", &ent, 0, "pw", KADM5_AUTH_ADD);
    }

    /* Fails with existing policy name. */
    ent.principal = user_princ;
    cprinc_test_fail("admin", &ent, 0, "pw", KADM5_DUP);

    /* Fails with null handle or principal ent. */
    cprinc_test_fail(NULL, &ent, 0, "pw", KADM5_BAD_SERVER_HANDLE);
    cprinc_test_fail("admin", NULL, 0, "pw", EINVAL);

    krb5_free_principal(context, princ);
    krb5_free_principal(context, user_princ);
}

static void
dpol_test_fail(char *user, char *name, krb5_error_code code)
{
    void *handle = get_handle(user);

    check_fail(kadm5_delete_policy(handle, name), code);
    free_handle(handle);
}

static void
dpol_test_succeed(char *user, char *name)
{
    dpol_test_fail(user, name, 0);
}

static void
test_delete_policy()
{
    krb5_principal princ = parse_princ("delete-policy-test-princ");

    /* Fails with unknown policy. */
    dpol_test_fail("admin", "delete-policy-test", KADM5_UNK_POLICY);

    /* Fails with empty policy name. */
    dpol_test_fail("admin", "", KADM5_BAD_POLICY);

    /* Succeeds with "delete" ACL (or local authentication). */
    create_simple_policy("delete-policy-test");
    dpol_test_succeed("admin/delete", "delete-policy-test");

    /* Succeeds even if a principal references the policy, since we now allow
     * principals to reference nonexistent policies. */
    create_simple_policy("delete-policy-test");
    create_simple_princ(princ, "delete-policy-test");
    dpol_test_succeed("admin", "delete-policy-test");
    delete_princ(princ);

    /* Fails over RPC if "delete" ACL is not granted, or if we authenticated to
     * kadmin/changepw. */
    if (rpc) {
        dpol_test_fail("$admin", "test-pol", KADM5_AUTH_DELETE);
        dpol_test_fail("admin/none", "test-pol", KADM5_AUTH_DELETE);
        dpol_test_fail("admin/add", "test-pol", KADM5_AUTH_DELETE);
    }

    /* Fails with null handle or principal ent. */
    dpol_test_fail(NULL, "test-pol", KADM5_BAD_SERVER_HANDLE);
    dpol_test_fail("admin", NULL, EINVAL);

    krb5_free_principal(context, princ);
}

static void
dprinc_test_fail(char *user, krb5_principal princ, krb5_error_code code)
{
    void *handle = get_handle(user);

    check_fail(kadm5_delete_principal(handle, princ), code);
    free_handle(handle);
}

static void
dprinc_test_succeed(char *user, krb5_principal princ)
{
    dprinc_test_fail(user, princ, 0);
}

static void
test_delete_principal()
{
    krb5_principal princ = parse_princ("delete-principal-test");

    /* Fails with unknown principal. */
    dprinc_test_fail("admin", princ, KADM5_UNK_PRINC);

    /* Succeeds with "delete" ACL (or local authentication). */
    create_simple_princ(princ, NULL);
    dprinc_test_succeed("admin/delete", princ);

    /* Fails over RPC if "delete" ACL is not granted, or if we authenticated to
     * kadmin/changepw. */
    if (rpc) {
        dprinc_test_fail("$admin", princ, KADM5_AUTH_DELETE);
        dprinc_test_fail("admin/add", princ, KADM5_AUTH_DELETE);
        dprinc_test_fail("admin/modify", princ, KADM5_AUTH_DELETE);
        dprinc_test_fail("admin/get", princ, KADM5_AUTH_DELETE);
        dprinc_test_fail("admin/none", princ, KADM5_AUTH_DELETE);
    }

    /* Fails with null handle or principal ent. */
    dprinc_test_fail(NULL, princ, KADM5_BAD_SERVER_HANDLE);
    dprinc_test_fail("admin", NULL, EINVAL);

    krb5_free_principal(context, princ);
}

static void
gpol_test_succeed(char *user, char *name)
{
    void *handle = get_handle(user);
    kadm5_policy_ent_rec ent;

    check(kadm5_get_policy(handle, name, &ent));
    assert(strcmp(ent.policy, name) == 0);
    check(kadm5_free_policy_ent(handle, &ent));
    free_handle(handle);
}

static void
gpol_test_fail(char *user, char *name, krb5_error_code code)
{
    void *handle = get_handle(user);
    kadm5_policy_ent_rec ent;

    check_fail(kadm5_get_policy(handle, name, &ent), code);
    free_handle(handle);
}

static void
test_get_policy()
{
    /* Fails with unknown policy. */
    dpol_test_fail("admin", "unknown-policy", KADM5_UNK_POLICY);

    /* Fails with empty or null policy name or a null handle. */
    gpol_test_fail("admin", "", KADM5_BAD_POLICY);
    gpol_test_fail("admin", NULL, EINVAL);
    gpol_test_fail(NULL, "", KADM5_BAD_SERVER_HANDLE);

    /* Fails over RPC unless "get" ACL is granted or the principal's own policy
     * is retrieved. */
    if (rpc) {
        gpol_test_fail("admin/none", "test-pol", KADM5_AUTH_GET);
        gpol_test_fail("admin/add", "test-pol", KADM5_AUTH_GET);
        gpol_test_succeed("admin/get", "test-pol");
        gpol_test_succeed("user", "minlife-pol");
        gpol_test_succeed("$user", "minlife-pol");
    }
}

static void
gprinc_test_succeed(char *user, krb5_principal princ)
{
    void *handle = get_handle(user);
    kadm5_principal_ent_rec ent;

    check(kadm5_get_principal(handle, princ, &ent,
                              KADM5_PRINCIPAL_NORMAL_MASK));
    assert(krb5_principal_compare(context, ent.principal, princ));
    check(kadm5_free_principal_ent(handle, &ent));
    free_handle(handle);
}

static void
gprinc_test_fail(char *user, krb5_principal princ, krb5_error_code code)
{
    void *handle = get_handle(user);
    kadm5_principal_ent_rec ent;

    check_fail(kadm5_get_principal(handle, princ, &ent,
                                   KADM5_PRINCIPAL_NORMAL_MASK), code);
    free_handle(handle);
}

static void
test_get_principal()
{
    void *handle;
    kadm5_principal_ent_rec ent;
    krb5_principal princ = parse_princ("get-principal-test");
    krb5_principal admin_princ = parse_princ("admin");
    krb5_principal admin_none_princ = parse_princ("admin/none");
    int i;

    /* Fails with unknown principal. */
    gprinc_test_fail("admin", princ, KADM5_UNK_PRINC);

    create_simple_princ(princ, NULL);

    /* Succeeds with "get" ACL (or local authentication), or operating on
     * self. */
    gprinc_test_succeed("admin/none", admin_none_princ);
    gprinc_test_succeed("$admin", admin_princ);
    gprinc_test_succeed("admin/get", princ);

    /* Fails over RPC if "get" ACL is not granted, or if we authenticated to
     * kadmin/changepw and getting another principal entry. */
    if (rpc) {
        gprinc_test_fail("$admin", princ, KADM5_AUTH_GET);
        gprinc_test_fail("admin/none", princ, KADM5_AUTH_GET);
        gprinc_test_fail("admin/add", princ, KADM5_AUTH_GET);
        gprinc_test_fail("admin/modify", princ, KADM5_AUTH_GET);
        gprinc_test_fail("admin/delete", princ, KADM5_AUTH_GET);
    }

    /* Entry contains no key data or tl-data unless asked for. */
    handle = get_handle("admin");
    check(kadm5_get_principal(handle, princ, &ent,
                              KADM5_PRINCIPAL_NORMAL_MASK));
    assert(ent.n_tl_data == 0);
    assert(ent.n_key_data == 0);
    assert(ent.tl_data == NULL);
    check(kadm5_free_principal_ent(handle, &ent));

    /* Key data (without the actual keys over RPC) is provided if asked for. */
    check(kadm5_get_principal(handle, princ, &ent,
                              KADM5_PRINCIPAL_NORMAL_MASK | KADM5_KEY_DATA));
    assert(ent.n_key_data == 2);
    for (i = 0; i < ent.n_key_data; i++)
        assert(rpc == (ent.key_data[i].key_data_length[0] == 0));
    check(kadm5_free_principal_ent(handle, &ent));
    free_handle(handle);

    /* Fails with null handle or principal. */
    gprinc_test_fail(NULL, princ, KADM5_BAD_SERVER_HANDLE);
    gprinc_test_fail("admin", NULL, EINVAL);

    delete_princ(princ);
    krb5_free_principal(context, princ);
    krb5_free_principal(context, admin_princ);
    krb5_free_principal(context, admin_none_princ);
}

static void
test_init_destroy()
{
    krb5_context ctx;
    kadm5_ret_t ret;
    kadm5_config_params params;
    kadm5_principal_ent_rec ent, gent;
    krb5_principal princ = parse_princ("init-test");
    krb5_ccache cc;
    void *handle;
    char hostname[MAXHOSTNAMELEN];
    int r;

    memset(&params, 0, sizeof(params));
    memset(&ent, 0, sizeof(ent));
    ent.principal = princ;

    r = gethostname(hostname, sizeof(hostname));
    assert(r == 0);

    /* Destroy fails with no server handle. */
    check_fail(kadm5_destroy(NULL), KADM5_BAD_SERVER_HANDLE);

    /* Fails with bad structure version mask. */
    check_fail(kadm5_init(context, "admin", "admin", KADM5_ADMIN_SERVICE, NULL,
                          0x65432101, api, NULL, &handle),
               KADM5_BAD_STRUCT_VERSION);
    check_fail(kadm5_init(context, "admin", "admin", KADM5_ADMIN_SERVICE, NULL,
                          1, api, NULL, &handle), KADM5_BAD_STRUCT_VERSION);

    /* Fails with too-old or too-new structure version. */
    check_fail(kadm5_init(context, "admin", "admin", KADM5_ADMIN_SERVICE, NULL,
                          KADM5_STRUCT_VERSION_MASK, api, NULL, &handle),
               KADM5_OLD_STRUCT_VERSION);
    check_fail(kadm5_init(context, "admin", "admin", KADM5_ADMIN_SERVICE, NULL,
                          KADM5_STRUCT_VERSION_MASK | 0xca, api, NULL,
                          &handle), KADM5_NEW_STRUCT_VERSION);

    /* Fails with bad API version mask. */
    check_fail(kadm5_init(context, "admin", "admin", KADM5_ADMIN_SERVICE, NULL,
                          KADM5_STRUCT_VERSION, 0x65432100, NULL, &handle),
               KADM5_BAD_API_VERSION);
    check_fail(kadm5_init(context, "admin", "admin", KADM5_ADMIN_SERVICE, NULL,
                          KADM5_STRUCT_VERSION, 4, NULL, &handle),
               KADM5_BAD_API_VERSION);

    /* Fails with too-old or too-new API version.*/
    ret = kadm5_init(context, "admin", "admin", KADM5_ADMIN_SERVICE, NULL,
                     KADM5_STRUCT_VERSION, KADM5_API_VERSION_MASK, NULL,
                     &handle);
    assert(ret == (rpc ? KADM5_OLD_LIB_API_VERSION :
                   KADM5_OLD_SERVER_API_VERSION));
    ret = kadm5_init(context, "admin", "admin", KADM5_ADMIN_SERVICE, NULL,
                     KADM5_STRUCT_VERSION, KADM5_API_VERSION_MASK | 0xca, NULL,
                     &handle);
    assert(ret == (rpc ? KADM5_NEW_LIB_API_VERSION :
                   KADM5_NEW_SERVER_API_VERSION));

    /* Fails with structure and API version reversed. */
    check_fail(kadm5_init(context, "admin", "admin", KADM5_ADMIN_SERVICE, NULL,
                          api, KADM5_STRUCT_VERSION, NULL, &handle),
               KADM5_BAD_STRUCT_VERSION);

    /* Hardcoded default max lifetime is used when no handle or krb5.conf
     * setting is given. */
    handle = get_handle("admin");
    check(kadm5_create_principal(handle, &ent, KADM5_PRINCIPAL, "pw"));
    check(kadm5_get_principal(handle, princ, &gent,
                              KADM5_PRINCIPAL_NORMAL_MASK));
    assert(gent.max_life == KRB5_KDB_MAX_LIFE);
    check(kadm5_delete_principal(handle, princ));
    check(kadm5_free_principal_ent(handle, &gent));
    free_handle(handle);

    /* Fails with configured unknown realm.  Do these tests in separate krb5
     * contexts since the realm setting sticks to the context. */
    check(kadm5_init_krb5_context(&ctx));
    params.realm = "";
    params.mask = KADM5_CONFIG_REALM;
    ret = kadm5_init(ctx, "admin", "admin", KADM5_ADMIN_SERVICE, &params,
                     KADM5_STRUCT_VERSION, api, NULL, &handle);
    assert(ret == (rpc ? KADM5_MISSING_KRB5_CONF_PARAMS : ENOENT));
    krb5_free_context(ctx);

    check(kadm5_init_krb5_context(&ctx));
    params.realm = "@";
    ret = kadm5_init(ctx, "admin", "admin", KADM5_ADMIN_SERVICE, &params,
                     KADM5_STRUCT_VERSION, api, NULL, &handle);
    assert(ret == (rpc ? KADM5_MISSING_KRB5_CONF_PARAMS : ENOENT));
    krb5_free_context(ctx);

    check(kadm5_init_krb5_context(&ctx));
    params.realm = "BAD.REALM";
    ret = kadm5_init(ctx, "admin", "admin", KADM5_ADMIN_SERVICE, &params,
                     KADM5_STRUCT_VERSION, api, NULL, &handle);
    assert(ret == (rpc ? KADM5_MISSING_KRB5_CONF_PARAMS : ENOENT));
    krb5_free_context(ctx);

    /* Succeeds with explicit client realm and configured realm. */
    check(kadm5_init_krb5_context(&ctx));
    params.realm = "KRBTEST.COM";
    check(kadm5_init(ctx, "admin@KRBTEST.COM", "admin", KADM5_ADMIN_SERVICE,
                     &params, KADM5_STRUCT_VERSION, api, NULL, &handle));
    check(kadm5_destroy(handle));
    krb5_free_context(ctx);

    /* Succeeds with explicit client realm. */
    check(kadm5_init(context, "admin@KRBTEST.COM", "admin",
                     KADM5_ADMIN_SERVICE, NULL, KADM5_STRUCT_VERSION, api,
                     NULL, &handle));
    check(kadm5_destroy(handle));


    if (rpc) {
        check(krb5_cc_default(context, &cc));

        /* Succeeds with configured host and port. */
        params.admin_server = hostname;
        params.kadmind_port = 61001;
        params.mask = KADM5_CONFIG_ADMIN_SERVER | KADM5_CONFIG_KADMIND_PORT;
        check(kadm5_init(context, "admin", "admin", KADM5_ADMIN_SERVICE,
                         &params, KADM5_STRUCT_VERSION, api, NULL, &handle));
        check(kadm5_destroy(handle));

        /* Fails with wrong configured port. */
        params.kadmind_port = 4;
        check_fail(kadm5_init(context, "admin", "admin", KADM5_ADMIN_SERVICE,
                              &params, KADM5_STRUCT_VERSION, api, NULL,
                              &handle), KADM5_RPC_ERROR);

        /* Fails with non-resolving hostname. */
        params.admin_server = "does.not.exist";
        params.mask = KADM5_CONFIG_ADMIN_SERVER;
        check_fail(kadm5_init(context, "admin", "admin", KADM5_ADMIN_SERVICE,
                              &params, KADM5_STRUCT_VERSION, api, NULL,
                              &handle), KADM5_CANT_RESOLVE);

        /* Fails with uninitialized cache. */
        check_fail(kadm5_init_with_creds(context, "admin", cc,
                                         KADM5_ADMIN_SERVICE, NULL,
                                         KADM5_STRUCT_VERSION, api, NULL,
                                         &handle), KRB5_FCC_NOFILE);

        /* Succeeds with cache containing kadmin/admin cred. */
        kinit(cc, "admin", "admin", KADM5_ADMIN_SERVICE);
        check(kadm5_init_with_creds(context, "admin", cc, KADM5_ADMIN_SERVICE,
                                    NULL, KADM5_STRUCT_VERSION, api, NULL,
                                    &handle));
        check(kadm5_destroy(handle));

        /* Succeeds with cache containing kadmin/changepw cred. */
        kinit(cc, "admin", "admin", KADM5_CHANGEPW_SERVICE);
        check(kadm5_init_with_creds(context, "admin", cc,
                                    KADM5_CHANGEPW_SERVICE, NULL,
                                    KADM5_STRUCT_VERSION, api, NULL, &handle));
        check(kadm5_destroy(handle));

        /* Fails with cache containing only a TGT. */
        kinit(cc, "admin", "admin", NULL);
        check_fail(kadm5_init_with_creds(context, "admin", cc,
                                         KADM5_ADMIN_SERVICE, NULL,
                                         KADM5_STRUCT_VERSION, api, NULL,
                                         &handle), KRB5_CC_NOTFOUND);

        /* Fails authenticating to non-kadmin princ. */
        check_fail(kadm5_init(context, "admin", "admin", "user", NULL,
                              KADM5_STRUCT_VERSION, api, NULL, &handle),
                   KADM5_RPC_ERROR);

        /* Fails authenticating to nonexistent princ. */
        check_fail(kadm5_init(context, "admin", "admin", "noexist", NULL,
                              KADM5_STRUCT_VERSION, api, NULL, &handle),
                   KADM5_SECURE_PRINC_MISSING);

        /* Fails authenticating to client princ (which is non-kadmin). */
        check_fail(kadm5_init(context, "admin", "admin", "admin", NULL,
                              KADM5_STRUCT_VERSION, api, NULL, &handle),
                   KADM5_RPC_ERROR);

        /* Fails with wrong password. */
        check_fail(kadm5_init(context, "admin", "wrong", KADM5_ADMIN_SERVICE,
                              NULL, KADM5_STRUCT_VERSION, api, NULL, &handle),
                   KADM5_BAD_PASSWORD);

        /* Fails with null client name. */
        check_fail(kadm5_init(context, NULL, "admin", KADM5_ADMIN_SERVICE,
                              NULL, KADM5_STRUCT_VERSION, api, NULL, &handle),
                   EINVAL);

        /* Fails with nonexistent client name. */
        check_fail(kadm5_init(context, "noexist", "admin", KADM5_ADMIN_SERVICE,
                              NULL, KADM5_STRUCT_VERSION, api, NULL, &handle),
                   KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN);

        /* Fails with nonexistent client name with explicit realm. */
        check_fail(kadm5_init(context, "noexist@KRBTEST.COM", "admin",
                              KADM5_ADMIN_SERVICE, NULL, KADM5_STRUCT_VERSION,
                              api, NULL, &handle),
                   KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN);

        /* Fails with nonexistent client name with unknown realm. */
        check_fail(kadm5_init(context, "noexist@BAD.REALM", "admin",
                              KADM5_ADMIN_SERVICE, NULL, KADM5_STRUCT_VERSION,
                              api, NULL, &handle), KRB5_REALM_UNKNOWN);

        /* Fails with known name but unknown realm. */
        check_fail(kadm5_init(context, "admin@BAD.REALM", "admin",
                              KADM5_ADMIN_SERVICE, NULL, KADM5_STRUCT_VERSION,
                              api, NULL, &handle), KRB5_REALM_UNKNOWN);

        check(krb5_cc_destroy(context, cc));
    } else {
        /* Fails with nonexistent stash file. */
        params.stash_file = "does/not/exist";
        params.mask = KADM5_CONFIG_STASH_FILE;
        check_fail(kadm5_init(context, "admin", "admin", KADM5_ADMIN_SERVICE,
                              &params, KADM5_STRUCT_VERSION, api, NULL,
                              &handle), KRB5_KDB_CANTREAD_STORED);

        /* Uses configured defaults for principal creation. */
        params.max_life = 10;
        params.max_rlife = 20;
        params.expiration = 30;
        params.num_keysalts = 0;
        params.mask = KADM5_CONFIG_MAX_LIFE | KADM5_CONFIG_MAX_RLIFE |
            KADM5_CONFIG_EXPIRATION | KADM5_CONFIG_ENCTYPES;
        check(kadm5_init(context, "admin", "admin", KADM5_ADMIN_SERVICE,
                         &params, KADM5_STRUCT_VERSION, api, NULL, &handle));
        check(kadm5_create_principal(handle, &ent, KADM5_PRINCIPAL, "pw"));
        check(kadm5_get_principal(handle, princ, &gent,
                                  KADM5_PRINCIPAL_NORMAL_MASK |
                                  KADM5_KEY_DATA));
        assert(gent.max_life == 10);
        assert(gent.max_renewable_life == 20);
        assert(gent.princ_expire_time == 30);
        assert(gent.n_key_data == 0);
        check(kadm5_delete_principal(handle, princ));
        check(kadm5_free_principal_ent(handle, &gent));
        check(kadm5_destroy(handle));

        /* Succeeds with incorrect password using local auth. */
        check(kadm5_init(context, "admin", "wrong", KADM5_ADMIN_SERVICE, NULL,
                         KADM5_STRUCT_VERSION, api, NULL, &handle));
        check(kadm5_destroy(handle));

        /* Succeeds with null service using local auth. */
        check(kadm5_init(context, "admin", "admin", NULL, NULL,
                         KADM5_STRUCT_VERSION, api, NULL, &handle));
        check(kadm5_destroy(handle));

        /* Succeeds with nonexistent, non-kadmin service using local auth. */
        check(kadm5_init(context, "admin", "admin", "foobar", NULL,
                         KADM5_STRUCT_VERSION, api, NULL, &handle));
        check(kadm5_destroy(handle));
    }

    krb5_free_principal(context, princ);
}

static void
mpol_test_fail(char *user, kadm5_policy_ent_t ent, uint32_t mask,
               krb5_error_code code)
{
    void *handle = get_handle(user);

    check_fail(kadm5_modify_policy(handle, ent, mask), code);
    free_handle(handle);
}

static void
mpol_test_compare(void *handle, kadm5_policy_ent_t ent, uint32_t mask)
{
    mpol_test_fail(handle, ent, mask, 0);
    compare_policy(ent, mask);
}

static void
test_modify_policy()
{
    kadm5_policy_ent_rec ent;

    memset(&ent, 0, sizeof(ent));
    ent.policy = "modify-policy-test";
    create_simple_policy(ent.policy);

    /* pw_min_life = 0 and pw_min_life != 0 */
    mpol_test_compare("admin", &ent, KADM5_PW_MIN_LIFE);
    ent.pw_min_life = 32;
    mpol_test_compare("admin", &ent, KADM5_PW_MIN_LIFE);

    /* pw_max_life = 0 and pw_max_life != 0 */
    mpol_test_compare("admin", &ent, KADM5_PW_MAX_LIFE);
    ent.pw_max_life = 32;
    mpol_test_compare("admin", &ent, KADM5_PW_MAX_LIFE);

    /* pw_min_length = 0 (rejected) and pw_min_length != 0 */
    mpol_test_fail("admin", &ent, KADM5_PW_MIN_LENGTH, KADM5_BAD_LENGTH);
    ent.pw_min_length = 8;
    mpol_test_compare("admin", &ent, KADM5_PW_MIN_LENGTH);

    /* pw_min_classes = 0 (rejected), 1, 5, 6 (rejected) */
    mpol_test_fail("admin", &ent, KADM5_PW_MIN_CLASSES, KADM5_BAD_CLASS);
    ent.pw_min_classes = 1;
    mpol_test_compare("admin", &ent, KADM5_PW_MIN_CLASSES);
    ent.pw_min_classes = 5;
    mpol_test_compare("admin", &ent, KADM5_PW_MIN_CLASSES);
    ent.pw_min_classes = 6;
    mpol_test_fail("admin", &ent, KADM5_PW_MIN_CLASSES, KADM5_BAD_CLASS);

    /* pw_history_num = 0 (rejected), 1, 10 */
    mpol_test_fail("admin", &ent, KADM5_PW_HISTORY_NUM, KADM5_BAD_HISTORY);
    ent.pw_history_num = 1;
    mpol_test_compare("admin", &ent, KADM5_PW_HISTORY_NUM);
    ent.pw_history_num = 10;
    mpol_test_compare("admin", &ent, KADM5_PW_HISTORY_NUM);

    if (api >= KADM5_API_VERSION_3) {
        ent.pw_max_fail = 2;
        mpol_test_compare("admin", &ent, KADM5_PW_MAX_FAILURE);
        ent.pw_failcnt_interval = 90;
        mpol_test_compare("admin", &ent, KADM5_PW_FAILURE_COUNT_INTERVAL);
        ent.pw_lockout_duration = 180;
        mpol_test_compare("admin", &ent, KADM5_PW_LOCKOUT_DURATION);
    }

    /* Fails over RPC if "modify" ACL is not granted, or if we authenticated to
     * kadmin/changepw. */
    if (rpc) {
        mpol_test_fail("$admin", &ent, KADM5_PW_MAX_LIFE, KADM5_AUTH_MODIFY);
        mpol_test_fail("admin/none", &ent, KADM5_PW_MAX_LIFE,
                       KADM5_AUTH_MODIFY);
        mpol_test_fail("admin/get", &ent, KADM5_PW_MAX_LIFE,
                       KADM5_AUTH_MODIFY);
        mpol_test_compare("admin/modify", &ent, KADM5_PW_MAX_LIFE);
    }

    delete_policy(ent.policy);

    /* Fails with empty or null policy name. */
    ent.policy = NULL;
    mpol_test_fail("admin", &ent, KADM5_PW_MAX_LIFE, EINVAL);
    ent.policy = "";
    mpol_test_fail("admin", &ent, KADM5_PW_MAX_LIFE, KADM5_BAD_POLICY);

    /* Fails with null handle or policy ent. */
    mpol_test_fail(NULL, &ent, KADM5_PW_MAX_LIFE, KADM5_BAD_SERVER_HANDLE);
    mpol_test_fail("admin", NULL, KADM5_PW_MAX_LIFE, EINVAL);
}

static void
mprinc_test_fail(char *user, kadm5_principal_ent_t ent, uint32_t mask,
                 krb5_error_code code)
{
    void *handle = get_handle(user);

    check_fail(kadm5_modify_principal(handle, ent, mask), code);
    free_handle(handle);
}

static void
mprinc_test_compare(char *user, kadm5_principal_ent_t ent, uint32_t mask)
{
    mprinc_test_fail(user, ent, mask, 0);
    compare_princ(ent, mask);
}

static void
test_modify_principal()
{
    void *handle;
    krb5_principal princ = parse_princ("modify-principal-test");
    kadm5_principal_ent_rec ent;
    krb5_tl_data tl = { NULL, 1, 1, (uint8_t *)"x" };
    krb5_tl_data tl2 = { NULL, 999, 6, (uint8_t *)"foobar" };

    memset(&ent, 0, sizeof(ent));
    ent.principal = princ;

    /* Fails with unknown principal. */
    mprinc_test_fail("admin", &ent, KADM5_KVNO, KADM5_UNK_PRINC);

    create_simple_princ(princ, NULL);

    /* Fails with prohibited mask bit or tl-data type. */
    mprinc_test_fail("admin", &ent, KADM5_AUX_ATTRIBUTES, KADM5_BAD_MASK);
    mprinc_test_fail("admin", &ent, KADM5_KEY_DATA, KADM5_BAD_MASK);
    mprinc_test_fail("admin", &ent, KADM5_LAST_FAILED, KADM5_BAD_MASK);
    mprinc_test_fail("admin", &ent, KADM5_LAST_SUCCESS, KADM5_BAD_MASK);
    mprinc_test_fail("admin", &ent, KADM5_LAST_PWD_CHANGE, KADM5_BAD_MASK);
    mprinc_test_fail("admin", &ent, KADM5_MKVNO, KADM5_BAD_MASK);
    mprinc_test_fail("admin", &ent, KADM5_MOD_NAME, KADM5_BAD_MASK);
    mprinc_test_fail("admin", &ent, KADM5_MOD_TIME, KADM5_BAD_MASK);
    mprinc_test_fail("admin", &ent, KADM5_PRINCIPAL, KADM5_BAD_MASK);

    /* Fails with tl-data type below 256. */
    ent.n_tl_data = 1;
    ent.tl_data = &tl;
    mprinc_test_fail("admin", &ent, KADM5_TL_DATA, KADM5_BAD_TL_TYPE);

    /* Fails with fail_auth_count other than zero. */
    ent.fail_auth_count = 1234;
    mprinc_test_fail("admin", &ent, KADM5_FAIL_AUTH_COUNT,
                     KADM5_BAD_SERVER_PARAMS);
    ent.fail_auth_count = 0;

    /* Succeeds with zero values of various fields. */
    mprinc_test_compare("admin", &ent, KADM5_PW_EXPIRATION);
    mprinc_test_compare("admin", &ent, KADM5_MAX_LIFE);
    mprinc_test_compare("admin", &ent, KADM5_MAX_RLIFE);
    mprinc_test_compare("admin", &ent, KADM5_FAIL_AUTH_COUNT);
    mprinc_test_compare("admin/modify", &ent, KADM5_PRINC_EXPIRE_TIME);
    mprinc_test_compare("admin", &ent, KADM5_POLICY_CLR);

    /* Setting a policy causes a pw_expiration computation.  Explicit
     * PW_EXPIRATION overrides the policy. */
    ent.pw_expiration = 1234;
    mprinc_test_compare("admin", &ent, KADM5_PW_EXPIRATION);
    ent.policy = "dict-only-pol";
    mprinc_test_compare("admin", &ent, KADM5_POLICY);
    ent.policy = "test-pol";
    mprinc_test_compare("admin", &ent, KADM5_POLICY);
    ent.pw_expiration = 999999999;
    mprinc_test_compare("admin", &ent, KADM5_PW_EXPIRATION);
    mprinc_test_compare("admin", &ent, KADM5_POLICY_CLR);

    /* Succeeds with non-zero values of various fields. */
    ent.princ_expire_time = 1234;
    mprinc_test_compare("admin", &ent, KADM5_PRINC_EXPIRE_TIME);
    ent.attributes = KRB5_KDB_DISALLOW_ALL_TIX;
    mprinc_test_compare("admin", &ent, KADM5_ATTRIBUTES);
    ent.attributes = KRB5_KDB_REQUIRES_PWCHANGE;
    mprinc_test_compare("admin", &ent, KADM5_ATTRIBUTES);
    ent.attributes = KRB5_KDB_DISALLOW_TGT_BASED;
    mprinc_test_compare("admin", &ent, KADM5_ATTRIBUTES);
    ent.max_life = 3456;
    mprinc_test_compare("admin", &ent, KADM5_MAX_LIFE);
    ent.kvno = 7;
    mprinc_test_compare("admin", &ent, KADM5_KVNO);

    /* Fails over RPC if "modify" ACL is not granted, or if we authenticated to
     * kadmin/changepw. */
    if (rpc) {
        mprinc_test_fail("$admin", &ent, KADM5_KVNO, KADM5_AUTH_MODIFY);
        mprinc_test_fail("admin/none", &ent, KADM5_KVNO, KADM5_AUTH_MODIFY);
        mprinc_test_fail("admin/get", &ent, KADM5_KVNO, KADM5_AUTH_MODIFY);
        mprinc_test_fail("admin/add", &ent, KADM5_KVNO, KADM5_AUTH_MODIFY);
        mprinc_test_fail("admin/delete", &ent, KADM5_KVNO, KADM5_AUTH_MODIFY);
    }

    /* tl-data of type > 255 is accepted. */
    handle = get_handle("admin");
    ent.max_renewable_life = 88;
    ent.tl_data = &tl2;
    check(kadm5_modify_principal(handle, &ent,
                                 KADM5_MAX_RLIFE | KADM5_TL_DATA));
    memset(&ent, 0, sizeof(ent));
    check(kadm5_get_principal(handle, princ, &ent,
                              KADM5_PRINCIPAL_NORMAL_MASK | KADM5_TL_DATA));
    assert(ent.max_renewable_life == 88);
    assert(ent.n_tl_data == 1);
    assert(ent.tl_data->tl_data_type == tl2.tl_data_type);
    assert(ent.tl_data->tl_data_length == tl2.tl_data_length);
    assert(memcmp(ent.tl_data->tl_data_contents, tl2.tl_data_contents,
                  tl2.tl_data_length) == 0);
    check(kadm5_free_principal_ent(handle, &ent));
    free_handle(handle);

    /* Fails with null handle or principal ent. */
    mprinc_test_fail(NULL, &ent, KADM5_KVNO, KADM5_BAD_SERVER_HANDLE);
    mprinc_test_fail("admin", NULL, KADM5_KVNO, EINVAL);

    delete_princ(princ);
    krb5_free_principal(context, princ);
}

static void
rnd_test_fail(char *user, krb5_principal princ, krb5_error_code code)
{
    void *handle = get_handle(user);

    check_fail(kadm5_randkey_principal(handle, princ, NULL, NULL), code);
    free_handle(handle);
}

static void
rnd_test_succeed(char *user, krb5_principal princ)
{
    rnd_test_fail(user, princ, 0);
}

static void
test_randkey()
{
    void *handle;
    krb5_principal princ = parse_princ("randkey-principal-test");
    krb5_principal user_princ = parse_princ("user");
    krb5_principal admin_princ = parse_princ("admin");
    kadm5_principal_ent_rec ent;
    krb5_keyblock *keys;
    int n_keys, i;

    create_simple_princ(princ, NULL);

    /* Check kvno and enctypes after randkey. */
    handle = get_handle("admin");
    check(kadm5_randkey_principal(handle, princ, &keys, &n_keys));
    check(kadm5_get_principal(handle, princ, &ent, KADM5_KEY_DATA));
    compare_key_data(&ent, default_supported_enctypes);
    assert(ent.key_data[0].key_data_kvno == 2);
    assert(n_keys == ent.n_key_data);
    for (i = 0; i < n_keys; i++)
        krb5_free_keyblock_contents(context, &keys[i]);
    free(keys);
    check(kadm5_free_principal_ent(handle, &ent));
    free_handle(handle);

    /*
     * Fails over RPC if "change" ACL is not granted, or if we authenticated to
     * kadmin/changepw and are changing another principal's password, or for
     * self-service if the policy minimum life has not elapsed since the last
     * key change.
     */
    if (rpc) {
        rnd_test_fail("$admin", user_princ, KADM5_AUTH_CHANGEPW);
        rnd_test_fail("admin/none", user_princ, KADM5_AUTH_CHANGEPW);
        rnd_test_fail("admin/delete", user_princ, KADM5_AUTH_CHANGEPW);
        rnd_test_succeed("admin/modify", user_princ);
        cpw_test_succeed("admin", user_princ, USER_PASSWORD);
        rnd_test_fail("user", user_princ, KADM5_PASS_TOOSOON);
        rnd_test_fail("$user", user_princ, KADM5_PASS_TOOSOON);
    }

    /* Succeeds with change privilege in spite of policy minimum life. */
    rnd_test_succeed("admin/modify", user_princ);
    cpw_test_succeed("admin", user_princ, USER_PASSWORD);

    /* Succeeds for self-service when authenticating to kadmin/changepw. */
    handle = get_handle("$admin");
    check(kadm5_randkey_principal(handle, admin_princ, NULL, NULL));
    check(kadm5_chpass_principal(handle, admin_princ, ADMIN_PASSWORD));
    free_handle(handle);

    /* Fails with null handle or principal name. */
    rnd_test_fail(NULL, princ, KADM5_BAD_SERVER_HANDLE);
    rnd_test_fail("admin", NULL, EINVAL);

    delete_princ(princ);
    krb5_free_principal(context, princ);
    krb5_free_principal(context, user_princ);
    krb5_free_principal(context, admin_princ);
}

int
main(int argc, char **argv)
{
    assert(argc == 2);
    rpc = (strcmp(argv[1], "clnt") == 0);

    check(kadm5_init_krb5_context(&context));

    api = KADM5_API_VERSION_2;
    test_create_policy();
    test_get_policy();
    test_modify_policy();

    api = KADM5_API_VERSION_4;
    test_chpass();
    test_create_policy();
    test_create_principal();
    test_delete_policy();
    test_delete_principal();
    test_get_policy();
    test_get_principal();
    test_init_destroy();
    test_modify_policy();
    test_modify_principal();
    test_randkey();

    krb5_free_context(context);

    return 0;
}
