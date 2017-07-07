/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/kdbtest.c - test program to exercise KDB modules */
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

/*
 * This test program uses libkdb5 APIs to exercise as much of the LDAP and DB2
 * back ends.
 */

#include <krb5.h>
#include <kadm5/admin.h>
#include <string.h>

static krb5_context ctx;

#define CHECK(code) check(code, __LINE__)
#define CHECK_COND(val) check_cond(val, __LINE__)

static void
check(krb5_error_code code, int lineno)
{
    const char *errmsg;

    if (code) {
        errmsg = krb5_get_error_message(ctx, code);
        fprintf(stderr, "Unexpected error at line %d: %s\n", lineno, errmsg);
        krb5_free_error_message(ctx, errmsg);
        exit(1);
    }
}

static void
check_cond(int value, int lineno)
{
    if (!value) {
        fprintf(stderr, "Unexpected result at line %d\n", lineno);
        exit(1);
    }
}

static krb5_data princ_data[2] = {
    { KV5M_DATA, 6, "xy*(z)" },
    { KV5M_DATA, 12, "+<> *()\\#\",;" }
};

static krb5_principal_data sample_princ = {
    KV5M_PRINCIPAL,
    { KV5M_DATA, 11, "KRBTEST.COM" },
    princ_data, 2, KRB5_NT_UNKNOWN
};

static krb5_principal_data xrealm_princ = {
    KV5M_PRINCIPAL,
    { KV5M_DATA, 12, "KRBTEST2.COM" },
    princ_data, 2, KRB5_NT_UNKNOWN
};

#define U(x) (unsigned char *)x

/*
 * tl1 through tl4 are normalized to attributes in the LDAP back end.  tl5 is
 * stored as untranslated tl-data.  tl3 contains an encoded osa_princ_ent with
 * a policy reference to "<test*>".
 */
static krb5_tl_data tl5 = { NULL, KRB5_TL_MKVNO, 2, U("\0\1") };
static krb5_tl_data tl4 = { &tl5, KRB5_TL_LAST_ADMIN_UNLOCK, 4,
                            U("\6\0\0\0") };
static krb5_tl_data tl3 = { &tl4, KRB5_TL_KADM_DATA, 32,
                            U("\x12\x34\x5C\x01\x00\x00\x00\x08"
                              "\x3C\x74\x65\x73\x74\x2A\x3E\x00"
                              "\x00\x00\x08\x00\x00\x00\x00\x00"
                              "\x00\x00\x00\x00\x00\x00\x00\x00") };
static krb5_tl_data tl2 = { &tl3, KRB5_TL_MOD_PRINC, 8, U("\5\6\7\0x@Y\0") };
static krb5_tl_data tl1 = { &tl2, KRB5_TL_LAST_PWD_CHANGE, 4, U("\1\2\3\4") };

/* An encoded osa_print_enc with no policy reference. */
static krb5_tl_data tl_no_policy = { NULL, KRB5_TL_KADM_DATA, 24,
                                     U("\x12\x34\x5C\x01\x00\x00\x00\x00"
                                       "\x00\x00\x00\x00\x00\x00\x00\x00"
                                       "\x00\x00\x00\x02\x00\x00\x00\x00") };

static krb5_key_data keys[] = {
    {
        2,                          /* key_data_ver */
        2,                          /* key_data_kvno */
        { ENCTYPE_AES256_CTS_HMAC_SHA1_96, KRB5_KDB_SALTTYPE_SPECIAL },
        { 32, 7 },
        { U("\x17\xF2\x75\xF2\x95\x4F\x2E\xD1"
            "\xF9\x0C\x37\x7B\xA7\xF4\xD6\xA3"
            "\x69\xAA\x01\x36\xE0\xBF\x0C\x92"
            "\x7A\xD6\x13\x3C\x69\x37\x59\xA9"),
          U("expsalt") }
    },
    {
        2,                          /* key_data_ver */
        2,                          /* key_data_kvno */
        { ENCTYPE_AES128_CTS_HMAC_SHA1_96, 0 },
        { 16, 0 },
        { U("\xDC\xEE\xB7\x0B\x3D\xE7\x65\x62"
            "\xE6\x89\x22\x6C\x76\x42\x91\x48"),
          NULL }
    }
};
#undef U

static char polname[] = "<test*>";

static krb5_db_entry sample_entry = {
    0,
    KRB5_KDB_V1_BASE_LENGTH,
    /* mask */
    KADM5_PRINCIPAL | KADM5_PRINC_EXPIRE_TIME | KADM5_PW_EXPIRATION |
    KADM5_ATTRIBUTES | KADM5_MAX_LIFE | KADM5_POLICY | KADM5_MAX_RLIFE |
    KADM5_LAST_SUCCESS | KADM5_LAST_FAILED | KADM5_FAIL_AUTH_COUNT |
    KADM5_KEY_DATA | KADM5_TL_DATA,
    /* attributes */
    KRB5_KDB_REQUIRES_PRE_AUTH | KRB5_KDB_REQUIRES_HW_AUTH |
    KRB5_KDB_DISALLOW_SVR,
    1234,                       /* max_life */
    5678,                       /* max_renewable_life */
    9012,                       /* expiration */
    3456,                       /* pw_expiration */
    1,                          /* last_success */
    5,                          /* last_failed */
    2,                          /* fail_auth_count */
    5,                          /* n_tl_data */
    2,                          /* n_key_data */
    0, NULL,                    /* e_length, e_data */
    &sample_princ,
    &tl1,
    keys
};

static osa_policy_ent_rec sample_policy = {
    0,                          /* version */
    polname,                    /* name */
    1357,                       /* pw_min_life */
    100,                        /* pw_max_life */
    6,                          /* pw_min_length */
    2,                          /* pw_min_classes */
    3,                          /* pw_history_num */
    0,                          /* policy_refcnt */
    2,                          /* pw_max_fail */
    60,                         /* pw_failcnt_interval */
    120,                        /* pw_lockout_duration */
    0,                          /* attributes */
    2468,                       /* max_life */
    3579,                       /* max_renewable_life */
    "aes",                      /* allowed_keysalts */
    0, NULL                     /* n_tl_data, tl_data */
};

/* Compare pol against sample_policy. */
static void
check_policy(osa_policy_ent_t pol)
{
    CHECK_COND(strcmp(pol->name, sample_policy.name) == 0);
    CHECK_COND(pol->pw_min_life == sample_policy.pw_min_life);
    CHECK_COND(pol->pw_max_life == sample_policy.pw_max_life);
    CHECK_COND(pol->pw_min_length == sample_policy.pw_min_length);
    CHECK_COND(pol->pw_min_classes == sample_policy.pw_min_classes);
    CHECK_COND(pol->pw_history_num == sample_policy.pw_history_num);
    CHECK_COND(pol->pw_max_life == sample_policy.pw_max_life);
    CHECK_COND(pol->pw_failcnt_interval == sample_policy.pw_failcnt_interval);
    CHECK_COND(pol->pw_lockout_duration == sample_policy.pw_lockout_duration);
    CHECK_COND(pol->attributes == sample_policy.attributes);
    CHECK_COND(pol->max_life == sample_policy.max_life);
    CHECK_COND(pol->max_renewable_life == sample_policy.max_renewable_life);
    CHECK_COND(strcmp(pol->allowed_keysalts,
                      sample_policy.allowed_keysalts) == 0);
}

/* Compare ent against sample_entry. */
static void
check_entry(krb5_db_entry *ent)
{
    krb5_int16 i, j;
    krb5_key_data *k1, *k2;
    krb5_tl_data *tl, etl;

    CHECK_COND(ent->attributes == sample_entry.attributes);
    CHECK_COND(ent->max_life == sample_entry.max_life);
    CHECK_COND(ent->max_renewable_life == sample_entry.max_renewable_life);
    CHECK_COND(ent->expiration == sample_entry.expiration);
    CHECK_COND(ent->pw_expiration == sample_entry.pw_expiration);
    CHECK_COND(ent->last_success == sample_entry.last_success);
    CHECK_COND(ent->last_failed == sample_entry.last_failed);
    CHECK_COND(ent->fail_auth_count == sample_entry.fail_auth_count);
    CHECK_COND(krb5_principal_compare(ctx, ent->princ, sample_entry.princ));
    CHECK_COND(ent->n_key_data == sample_entry.n_key_data);
    for (i = 0; i < ent->n_key_data; i++) {
        k1 = &ent->key_data[i];
        k2 = &sample_entry.key_data[i];
        CHECK_COND(k1->key_data_ver == k2->key_data_ver);
        CHECK_COND(k1->key_data_kvno == k2->key_data_kvno);
        for (j = 0; j < k1->key_data_ver; j++) {
            CHECK_COND(k1->key_data_type[j] == k2->key_data_type[j]);
            CHECK_COND(k1->key_data_length[j] == k2->key_data_length[j]);
            CHECK_COND(memcmp(k1->key_data_contents[j],
                              k2->key_data_contents[j],
                              k1->key_data_length[j]) == 0);
        }
    }
    for (tl = sample_entry.tl_data; tl != NULL; tl = tl->tl_data_next) {
        etl.tl_data_type = tl->tl_data_type;
        CHECK(krb5_dbe_lookup_tl_data(ctx, ent, &etl));
        CHECK_COND(tl->tl_data_length == etl.tl_data_length);
        CHECK_COND(memcmp(tl->tl_data_contents, etl.tl_data_contents,
                          tl->tl_data_length) == 0);
    }
}

/* Audit a successful or failed preauth attempt for *entp.  Then reload *entp
 * (by fetching sample_princ) so we can see the effect. */
static void
sim_preauth(krb5_timestamp authtime, krb5_boolean ok, krb5_db_entry **entp)
{
    /* Both back ends ignore the request parameter for now. */
    krb5_db_audit_as_req(ctx, NULL, *entp, *entp, authtime,
                         ok ? 0 : KRB5KDC_ERR_PREAUTH_FAILED);
    krb5_db_free_principal(ctx, *entp);
    CHECK(krb5_db_get_principal(ctx, &sample_princ, 0, entp));
}

static krb5_error_code
iter_princ_handler(void *data, krb5_db_entry *ent)
{
    int *count = data;

    CHECK_COND(krb5_principal_compare(ctx, ent->princ, sample_entry.princ));
    (*count)++;
    return 0;
}

static void
iter_pol_handler(void *data, osa_policy_ent_t pol)
{
    int *count = data;

    CHECK_COND(strcmp(pol->name, sample_policy.name) == 0);
    (*count)++;
}

int
main()
{
    krb5_db_entry *ent;
    osa_policy_ent_t pol;
    krb5_pa_data **e_data;
    const char *status;
    int count;

    CHECK(krb5_init_context_profile(NULL, KRB5_INIT_CONTEXT_KDC, &ctx));

    /* If we can, revert to requiring all entries match sample_princ in
     * iter_princ_handler */
    CHECK_COND(krb5_db_inited(ctx) != 0);
    CHECK(krb5_db_create(ctx, NULL));
    CHECK(krb5_db_inited(ctx));
    CHECK(krb5_db_fini(ctx));
    CHECK_COND(krb5_db_inited(ctx) != 0);

    CHECK_COND(krb5_db_inited(ctx) != 0);
    CHECK(krb5_db_open(ctx, NULL, KRB5_KDB_OPEN_RW | KRB5_KDB_SRV_TYPE_ADMIN));
    CHECK(krb5_db_inited(ctx));

    /* Manipulate a policy, leaving it in place at the end. */
    CHECK_COND(krb5_db_put_policy(ctx, &sample_policy) != 0);
    CHECK_COND(krb5_db_delete_policy(ctx, polname) != 0);
    CHECK_COND(krb5_db_get_policy(ctx, polname, &pol) == KRB5_KDB_NOENTRY);
    CHECK(krb5_db_create_policy(ctx, &sample_policy));
    CHECK_COND(krb5_db_create_policy(ctx, &sample_policy) != 0);
    CHECK(krb5_db_get_policy(ctx, polname, &pol));
    check_policy(pol);
    pol->pw_min_length--;
    CHECK(krb5_db_put_policy(ctx, pol));
    krb5_db_free_policy(ctx, pol);
    CHECK(krb5_db_get_policy(ctx, polname, &pol));
    CHECK_COND(pol->pw_min_length == sample_policy.pw_min_length - 1);
    krb5_db_free_policy(ctx, pol);
    CHECK(krb5_db_delete_policy(ctx, polname));
    CHECK_COND(krb5_db_put_policy(ctx, &sample_policy) != 0);
    CHECK_COND(krb5_db_delete_policy(ctx, polname) != 0);
    CHECK_COND(krb5_db_get_policy(ctx, polname, &pol) == KRB5_KDB_NOENTRY);
    CHECK(krb5_db_create_policy(ctx, &sample_policy));
    count = 0;
    CHECK(krb5_db_iter_policy(ctx, NULL, iter_pol_handler, &count));
    CHECK_COND(count == 1);

    /* Create a principal. */
    CHECK_COND(krb5_db_delete_principal(ctx, &sample_princ) ==
               KRB5_KDB_NOENTRY);
    CHECK_COND(krb5_db_get_principal(ctx, &xrealm_princ, 0, &ent) ==
               KRB5_KDB_NOENTRY);
    CHECK(krb5_db_put_principal(ctx, &sample_entry));
    /* Putting again will fail with LDAP (due to KADM5_PRINCIPAL in mask)
     * but succeed with DB2, so don't check the result. */
    (void)krb5_db_put_principal(ctx, &sample_entry);
    /* But it should succeed in both back ends with KADM5_LOAD in mask. */
    sample_entry.mask |= KADM5_LOAD;
    CHECK(krb5_db_put_principal(ctx, &sample_entry));
    sample_entry.mask &= ~KADM5_LOAD;
    /* Fetch and compare the added principal. */
    CHECK(krb5_db_get_principal(ctx, &sample_princ, 0, &ent));
    check_entry(ent);

    /* We can't set up a successful allowed-to-delegate check through existing
     * APIs yet, but we can make a failed check. */
    CHECK_COND(krb5_db_check_allowed_to_delegate(ctx, &sample_princ, ent,
                                                 &sample_princ) != 0);

    /* Exercise lockout code. */
    /* Policy params: max_fail 2, failcnt_interval 60, lockout_duration 120 */
    /* Initial state: last_success 1, last_failed 5, fail_auth_count 2,
     * last admin unlock 6 */
    /* Check succeeds due to last admin unlock. */
    CHECK(krb5_db_check_policy_as(ctx, NULL, ent, ent, 7, &status, &e_data));
    /* Failure count resets to 1 due to last admin unlock. */
    sim_preauth(8, FALSE, &ent);
    CHECK_COND(ent->fail_auth_count == 1 && ent->last_failed == 8);
    /* Failure count resets to 1 due to failcnt_interval */
    sim_preauth(70, FALSE, &ent);
    CHECK_COND(ent->fail_auth_count == 1 && ent->last_failed == 70);
    /* Failure count resets to 0 due to successful preauth. */
    sim_preauth(75, TRUE, &ent);
    CHECK_COND(ent->fail_auth_count == 0 && ent->last_success == 75);
    /* Failure count increments to 2 and stops incrementing. */
    sim_preauth(80, FALSE, &ent);
    CHECK_COND(ent->fail_auth_count == 1 && ent->last_failed == 80);
    sim_preauth(100, FALSE, &ent);
    CHECK_COND(ent->fail_auth_count == 2 && ent->last_failed == 100);
    sim_preauth(110, FALSE, &ent);
    CHECK_COND(ent->fail_auth_count == 2 && ent->last_failed == 100);
    /* Check fails due to reaching maximum failure count. */
    CHECK_COND(krb5_db_check_policy_as(ctx, NULL, ent, ent, 170, &status,
                                       &e_data) == KRB5KDC_ERR_CLIENT_REVOKED);
    /* Check succeeds after lockout_duration has passed. */
    CHECK(krb5_db_check_policy_as(ctx, NULL, ent, ent, 230, &status, &e_data));
    /* Failure count resets to 1 on next failure. */
    sim_preauth(240, FALSE, &ent);
    CHECK_COND(ent->fail_auth_count == 1 && ent->last_failed == 240);

    /* Exercise LDAP code to clear a policy reference and to set the key
     * data on an existing principal. */
    CHECK(krb5_dbe_update_tl_data(ctx, ent, &tl_no_policy));
    ent->mask = KADM5_POLICY_CLR | KADM5_KEY_DATA;
    CHECK(krb5_db_put_principal(ctx, ent));
    CHECK(krb5_db_delete_policy(ctx, polname));

    /* Put the modified entry again (with KDB_TL_USER_INFO tl-data for LDAP) as
     * from a load operation. */
    ent->mask = (sample_entry.mask & ~KADM5_POLICY) | KADM5_LOAD;
    CHECK(krb5_db_put_principal(ctx, ent));

    /* Exercise LDAP code to create a new principal at a DN from
     * KDB_TL_USER_INFO tl-data. */
    CHECK(krb5_db_delete_principal(ctx, &sample_princ));
    CHECK(krb5_db_put_principal(ctx, ent));
    krb5_db_free_principal(ctx, ent);

    /* Exercise principal iteration code. */
    count = 0;
    CHECK(krb5_db_iterate(ctx, "xy*", iter_princ_handler, &count, 0));
    CHECK_COND(count == 1);

    CHECK(krb5_db_fini(ctx));
    CHECK_COND(krb5_db_inited(ctx) != 0);

    /* It might be nice to exercise krb5_db_destroy here, but the LDAP module
     * doesn't support it. */

    krb5_free_context(ctx);
    return 0;
}
