/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/t_replay.c - tests for replay.c */
/*
 * Copyright (C) 2016 by the Massachusetts Institute of Technology.
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
 * Unit tests for the lookaside cache in replay.c
 */

#ifndef NOCACHE

#include "k5-cmocka.h"

/* For wrapping functions */
#include "k5-int.h"
#include "krb5.h"

/*
 * Wrapper functions
 */

static krb5_error_code
__wrap_krb5_timeofday(krb5_context context, krb5_timestamp *timeret)
{
    *timeret = (krb5_timestamp)mock();
    return (krb5_error_code)mock();
}

#define krb5_timeofday __wrap_krb5_timeofday

#include "replay.c"

#undef krb5_timeofday

#define SEED 0x6F03A219
#define replay_unit_test(fn)                                            \
    cmocka_unit_test_setup_teardown(fn, setup_lookaside, destroy_lookaside)

/*
 * Helper functions
 */

static void
time_return(krb5_timestamp time, krb5_error_code err)
{
    will_return(__wrap_krb5_timeofday, time);
    will_return(__wrap_krb5_timeofday, err);
}

/*
 * setup/teardown functions
 */

static int
global_setup(void **state)
{
    krb5_error_code ret;
    krb5_context context = NULL;

    ret = krb5_init_context(&context);
    if (ret)
        return ret;

    *state = context;
    return 0;
}

static int
global_teardown(void **state)
{
    krb5_free_context(*state);
    return 0;
}

static int
setup_lookaside(void **state)
{
    krb5_error_code ret;
    krb5_context context = *state;

    ret = kdc_init_lookaside(context);
    if (ret)
        return ret;

    /* Ensure some vars are all set to initial values */
    hits = 0;
    calls = 0;
    max_hits_per_entry = 0;
    num_entries = 0;
    total_size = 0;

    return 0;
}

static int
destroy_lookaside(void **state)
{
    kdc_free_lookaside(*state);
    return 0;
}

/*
 * entry_size tests
 */

static void
test_entry_size_no_response(void **state)
{
    size_t result;
    const krb5_data req = string2data("I'm a test request");

    result = entry_size(&req, NULL);
    assert_int_equal(result, sizeof(struct entry) + 18);
}

static void
test_entry_size_w_response(void **state)
{
    size_t result;
    const krb5_data req = string2data("I'm a test request");
    const krb5_data rep = string2data("I'm a test response");

    result = entry_size(&req, &rep);
    assert_int_equal(result, sizeof(struct entry) + 18 + 19);
}

/*
 * insert_entry tests
 */

static void
test_insert_entry(void **state)
{
    struct entry *e;
    krb5_context context = *state;
    krb5_data req = string2data("I'm a test request");
    krb5_data rep = string2data("I'm a test response");

    e = insert_entry(context, &req, &rep, 15);

    assert_ptr_equal(k5_hashtab_get(hash_table, req.data, req.length), e);
    assert_ptr_equal(K5_TAILQ_FIRST(&expiration_queue), e);
    assert_true(data_eq(e->req_packet, req));
    assert_true(data_eq(e->reply_packet, rep));
    assert_int_equal(e->timein, 15);
}

static void
test_insert_entry_no_response(void **state)
{
    struct entry *e;
    krb5_context context = *state;
    krb5_data req = string2data("I'm a test request");

    e = insert_entry(context, &req, NULL, 10);

    assert_ptr_equal(k5_hashtab_get(hash_table, req.data, req.length), e);
    assert_ptr_equal(K5_TAILQ_FIRST(&expiration_queue), e);
    assert_true(data_eq(e->req_packet, req));
    assert_int_equal(e->reply_packet.length, 0);
    assert_int_equal(e->timein, 10);
}

static void
test_insert_entry_multiple(void **state)
{
    struct entry *e1, *e2;
    krb5_context context = *state;
    krb5_data req1 = string2data("I'm a test request");
    krb5_data rep1 = string2data("I'm a test response");
    krb5_data req2 = string2data("I'm a different test request");

    e1 = insert_entry(context, &req1, &rep1, 20);

    assert_ptr_equal(k5_hashtab_get(hash_table, req1.data, req1.length), e1);
    assert_ptr_equal(K5_TAILQ_FIRST(&expiration_queue), e1);
    assert_true(data_eq(e1->req_packet, req1));
    assert_true(data_eq(e1->reply_packet, rep1));
    assert_int_equal(e1->timein, 20);

    e2 = insert_entry(context, &req2, NULL, 30);

    assert_ptr_equal(k5_hashtab_get(hash_table, req2.data, req2.length), e2);
    assert_ptr_equal(K5_TAILQ_LAST(&expiration_queue,entry_queue), e2);
    assert_true(data_eq(e2->req_packet, req2));
    assert_int_equal(e2->reply_packet.length, 0);
    assert_int_equal(e2->timein, 30);
}

/*
 * discard_entry tests
 */

static void
test_discard_entry(void **state)
{
    struct entry *e;
    krb5_context context = *state;
    krb5_data req = string2data("I'm a test request");
    krb5_data rep = string2data("I'm a test response");

    e = insert_entry(context, &req, &rep, 0);
    discard_entry(context, e);

    assert_null(k5_hashtab_get(hash_table, req.data, req.length));
    assert_int_equal(num_entries, 0);
    assert_int_equal(total_size, 0);
}

static void
test_discard_entry_no_response(void **state)
{
    struct entry *e;
    krb5_context context = *state;
    krb5_data req = string2data("I'm a test request");

    e = insert_entry(context, &req, NULL, 0);
    discard_entry(context, e);

    assert_null(k5_hashtab_get(hash_table, req.data, req.length));
    assert_int_equal(num_entries, 0);
    assert_int_equal(total_size, 0);
}

/*
 * kdc_remove_lookaside tests
 */

static void
test_kdc_remove_lookaside(void **state)
{
    krb5_context context = *state;
    krb5_data req = string2data("I'm a test request");
    krb5_data rep = string2data("I'm a test response");

    insert_entry(context, &req, &rep, 0);
    kdc_remove_lookaside(context, &req);

    assert_null(k5_hashtab_get(hash_table, req.data, req.length));
    assert_int_equal(num_entries, 0);
    assert_int_equal(total_size, 0);
}

static void
test_kdc_remove_lookaside_empty_cache(void **state)
{
    krb5_context context = *state;
    krb5_data req = string2data("I'm a test request");

    assert_int_equal(num_entries, 0);
    kdc_remove_lookaside(context, &req);

    assert_int_equal(num_entries, 0);
    assert_int_equal(total_size, 0);
}

static void
test_kdc_remove_lookaside_unknown(void **state)
{
    struct entry *e;
    krb5_context context = *state;
    krb5_data req1 = string2data("I'm a test request");
    krb5_data rep1 = string2data("I'm a test response");
    krb5_data req2 = string2data("I'm a different test request");

    e = insert_entry(context, &req1, &rep1, 0);
    kdc_remove_lookaside(context, &req2);

    assert_ptr_equal(k5_hashtab_get(hash_table, req1.data, req1.length), e);
    assert_int_equal(num_entries, 1);
    assert_int_equal(total_size, entry_size(&req1, &rep1));
}

static void
test_kdc_remove_lookaside_multiple(void **state)
{
    struct entry *e1;
    krb5_context context = *state;
    krb5_data req1 = string2data("I'm a test request");
    krb5_data rep1 = string2data("I'm a test response");
    krb5_data req2 = string2data("I'm a different test request");

    e1 = insert_entry(context, &req1, &rep1, 0);
    insert_entry(context, &req2, NULL, 0);

    kdc_remove_lookaside(context, &req2);

    assert_null(k5_hashtab_get(hash_table, req2.data, req2.length));
    assert_ptr_equal(k5_hashtab_get(hash_table, req1.data, req1.length), e1);
    assert_int_equal(num_entries, 1);
    assert_int_equal(total_size, entry_size(&req1, &rep1));

    kdc_remove_lookaside(context, &req1);

    assert_null(k5_hashtab_get(hash_table, req1.data, req1.length));
    assert_int_equal(num_entries, 0);
    assert_int_equal(total_size, 0);
}

/*
 * kdc_check_lookaside tests
 */

static void
test_kdc_check_lookaside_hit(void **state)
{
    struct entry *e;
    krb5_boolean result;
    krb5_data *result_data;
    krb5_context context = *state;
    krb5_data req = string2data("I'm a test request");
    krb5_data rep = string2data("I'm a test response");

    e = insert_entry(context, &req, &rep, 0);

    result = kdc_check_lookaside(context, &req, &result_data);

    assert_true(result);
    assert_true(data_eq(rep, *result_data));
    assert_int_equal(hits, 1);
    assert_int_equal(e->num_hits, 1);

    krb5_free_data(context, result_data);
}

static void
test_kdc_check_lookaside_no_hit(void **state)
{
    krb5_boolean result;
    krb5_data *result_data;
    krb5_context context = *state;
    krb5_data req = string2data("I'm a test request");

    result = kdc_check_lookaside(context, &req, &result_data);

    assert_false(result);
    assert_null(result_data);
    assert_int_equal(hits, 0);
}

static void
test_kdc_check_lookaside_empty(void **state)
{
    krb5_boolean result;
    krb5_data *result_data;
    krb5_context context = *state;
    krb5_data req = string2data("I'm a test request");

    /* Set result_data so we can verify that it is reset to NULL. */
    result_data = &req;
    result = kdc_check_lookaside(context, &req, &result_data);

    assert_false(result);
    assert_null(result_data);
    assert_int_equal(hits, 0);
}

static void
test_kdc_check_lookaside_no_response(void **state)
{
    struct entry *e;
    krb5_boolean result;
    krb5_data *result_data;
    krb5_context context = *state;
    krb5_data req = string2data("I'm a test request");

    e = insert_entry(context, &req, NULL, 0);

    /* Set result_data so we can verify that it is reset to NULL. */
    result_data = &req;
    result = kdc_check_lookaside(context, &req, &result_data);

    assert_true(result);
    assert_null(result_data);
    assert_int_equal(hits, 1);
    assert_int_equal(e->num_hits, 1);
}

static void
test_kdc_check_lookaside_hit_multiple(void **state)
{
    struct entry *e1, *e2;
    krb5_boolean result;
    krb5_data *result_data;
    krb5_context context = *state;
    krb5_data req1 = string2data("I'm a test request");
    krb5_data rep1 = string2data("I'm a test response");
    krb5_data req2 = string2data("I'm a different test request");

    e1 = insert_entry(context, &req1, &rep1, 0);
    e2 = insert_entry(context, &req2, NULL, 0);

    result = kdc_check_lookaside(context, &req1, &result_data);

    assert_true(result);
    assert_true(data_eq(rep1, *result_data));
    assert_int_equal(hits, 1);
    assert_int_equal(e1->num_hits, 1);
    assert_int_equal(e2->num_hits, 0);

    krb5_free_data(context, result_data);

    /* Set result_data so we can verify that it is reset to NULL. */
    result_data = &req1;
    result = kdc_check_lookaside(context, &req2, &result_data);

    assert_true(result);
    assert_null(result_data);
    assert_int_equal(hits, 2);
    assert_int_equal(e1->num_hits, 1);
    assert_int_equal(e2->num_hits, 1);
}

/*
 * kdc_insert_lookaside tests
 */

static void
test_kdc_insert_lookaside_single(void **state)
{
    krb5_context context = *state;
    krb5_data req = string2data("I'm a test request");
    krb5_data rep = string2data("I'm a test response");
    struct entry *hash_ent, *exp_ent;

    time_return(0, 0);
    kdc_insert_lookaside(context, &req, &rep);

    hash_ent = k5_hashtab_get(hash_table, req.data, req.length);
    assert_non_null(hash_ent);
    assert_true(data_eq(hash_ent->req_packet, req));
    assert_true(data_eq(hash_ent->reply_packet, rep));
    exp_ent = K5_TAILQ_FIRST(&expiration_queue);
    assert_true(data_eq(exp_ent->req_packet, req));
    assert_true(data_eq(exp_ent->reply_packet, rep));
    assert_int_equal(num_entries, 1);
    assert_int_equal(total_size, entry_size(&req, &rep));
}

static void
test_kdc_insert_lookaside_no_reply(void **state)
{
    krb5_context context = *state;
    krb5_data req = string2data("I'm a test request");
    struct entry *hash_ent, *exp_ent;

    time_return(0, 0);
    kdc_insert_lookaside(context, &req, NULL);

    hash_ent = k5_hashtab_get(hash_table, req.data, req.length);
    assert_non_null(hash_ent);
    assert_true(data_eq(hash_ent->req_packet, req));
    assert_int_equal(hash_ent->reply_packet.length, 0);
    exp_ent = K5_TAILQ_FIRST(&expiration_queue);
    assert_true(data_eq(exp_ent->req_packet, req));
    assert_int_equal(exp_ent->reply_packet.length, 0);
    assert_int_equal(num_entries, 1);
    assert_int_equal(total_size, entry_size(&req, NULL));
}

static void
test_kdc_insert_lookaside_multiple(void **state)
{
    krb5_context context = *state;
    krb5_data req1 = string2data("I'm a test request");
    krb5_data rep1 = string2data("I'm a test response");
    size_t e1_size = entry_size(&req1, &rep1);
    krb5_data req2 = string2data("I'm a different test request");
    size_t e2_size = entry_size(&req2, NULL);
    struct entry *hash1_ent, *hash2_ent, *exp_first, *exp_last;

    time_return(0, 0);
    kdc_insert_lookaside(context, &req1, &rep1);

    hash1_ent = k5_hashtab_get(hash_table, req1.data, req1.length);
    assert_non_null(hash1_ent);
    assert_true(data_eq(hash1_ent->req_packet, req1));
    assert_true(data_eq(hash1_ent->reply_packet, rep1));
    exp_first = K5_TAILQ_FIRST(&expiration_queue);
    assert_true(data_eq(exp_first->req_packet, req1));
    assert_true(data_eq(exp_first->reply_packet, rep1));
    assert_int_equal(num_entries, 1);
    assert_int_equal(total_size, e1_size);

    time_return(0, 0);
    kdc_insert_lookaside(context, &req2, NULL);

    hash2_ent = k5_hashtab_get(hash_table, req2.data, req2.length);
    assert_non_null(hash2_ent);
    assert_true(data_eq(hash2_ent->req_packet, req2));
    assert_int_equal(hash2_ent->reply_packet.length, 0);
    exp_last = K5_TAILQ_LAST(&expiration_queue, entry_queue);
    assert_true(data_eq(exp_last->req_packet, req2));
    assert_int_equal(exp_last->reply_packet.length, 0);
    assert_int_equal(num_entries, 2);
    assert_int_equal(total_size, e1_size + e2_size);
}

static void
test_kdc_insert_lookaside_cache_expire(void **state)
{
    struct entry *e;
    krb5_context context = *state;
    krb5_data req1 = string2data("I'm a test request");
    krb5_data rep1 = string2data("I'm a test response");
    size_t e1_size = entry_size(&req1, &rep1);
    krb5_data req2 = string2data("I'm a different test request");
    size_t e2_size = entry_size(&req2, NULL);
    struct entry *hash1_ent, *hash2_ent, *exp_ent;

    time_return(0, 0);
    kdc_insert_lookaside(context, &req1, &rep1);

    hash1_ent = k5_hashtab_get(hash_table, req1.data, req1.length);
    assert_non_null(hash1_ent);
    assert_true(data_eq(hash1_ent->req_packet, req1));
    assert_true(data_eq(hash1_ent->reply_packet, rep1));
    exp_ent = K5_TAILQ_FIRST(&expiration_queue);
    assert_true(data_eq(exp_ent->req_packet, req1));
    assert_true(data_eq(exp_ent->reply_packet, rep1));
    assert_int_equal(num_entries, 1);
    assert_int_equal(total_size, e1_size);

    /* Increase hits on entry */
    e = k5_hashtab_get(hash_table, req1.data, req1.length);
    assert_non_null(e);
    e->num_hits = 5;

    time_return(STALE_TIME + 1, 0);
    kdc_insert_lookaside(context, &req2, NULL);

    assert_null(k5_hashtab_get(hash_table, req1.data, req1.length));
    assert_int_equal(max_hits_per_entry, 5);

    hash2_ent = k5_hashtab_get(hash_table, req2.data, req2.length);
    assert_non_null(hash2_ent);
    assert_true(data_eq(hash2_ent->req_packet, req2));
    assert_int_equal(hash2_ent-> reply_packet.length, 0);
    exp_ent = K5_TAILQ_FIRST(&expiration_queue);
    assert_true(data_eq(exp_ent->req_packet, req2));
    assert_int_equal(exp_ent->reply_packet.length, 0);
    assert_int_equal(num_entries, 1);
    assert_int_equal(total_size, e2_size);
}

int main()
{
    int ret;

    const struct CMUnitTest replay_tests[] = {
        /* entry_size tests */
        replay_unit_test(test_entry_size_no_response),
        replay_unit_test(test_entry_size_w_response),
        /* insert_entry tests */
        replay_unit_test(test_insert_entry),
        replay_unit_test(test_insert_entry_no_response),
        replay_unit_test(test_insert_entry_multiple),
        /* discard_entry tests */
        replay_unit_test(test_discard_entry),
        replay_unit_test(test_discard_entry_no_response),
        /* kdc_remove_lookaside tests */
        replay_unit_test(test_kdc_remove_lookaside),
        replay_unit_test(test_kdc_remove_lookaside_empty_cache),
        replay_unit_test(test_kdc_remove_lookaside_unknown),
        replay_unit_test(test_kdc_remove_lookaside_multiple),
        /* kdc_check_lookaside tests */
        replay_unit_test(test_kdc_check_lookaside_hit),
        replay_unit_test(test_kdc_check_lookaside_no_hit),
        replay_unit_test(test_kdc_check_lookaside_empty),
        replay_unit_test(test_kdc_check_lookaside_no_response),
        replay_unit_test(test_kdc_check_lookaside_hit_multiple),
        /* kdc_insert_lookaside tests */
        replay_unit_test(test_kdc_insert_lookaside_single),
        replay_unit_test(test_kdc_insert_lookaside_no_reply),
        replay_unit_test(test_kdc_insert_lookaside_multiple),
        replay_unit_test(test_kdc_insert_lookaside_cache_expire)
    };

    ret = cmocka_run_group_tests_name("replay_lookaside", replay_tests,
                                      global_setup, global_teardown);

    return ret;
}

#else /* NOCACHE */

int main()
{
    return 0;
}

#endif /* NOCACHE */
