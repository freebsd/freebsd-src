/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/t_response_set.c - Test krb5_response_set */
/*
 * Copyright 2012 Red Hat, Inc.
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
 * the name of Red Hat not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original Red Hat software.
 * Red Hat makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include <k5-int.h>

#include "int-proto.h"

#define TEST_STR1 "test1"
#define TEST_STR2 "test2"

static void
check_pred(int predicate)
{
    if (!predicate)
        abort();
}

static void
check(krb5_error_code code)
{
    if (code != 0) {
        com_err("t_response_items", code, NULL);
        abort();
    }
}

static int
nstrcmp(const char *a, const char *b)
{
    if (a == NULL && b == NULL)
        return 0;
    else if (a == NULL)
        return -1;
    else if (b == NULL)
        return 1;

    return strcmp(a, b);
}

int
main()
{
    k5_response_items *ri;

    check(k5_response_items_new(&ri));
    check_pred(k5_response_items_empty(ri));

    check(k5_response_items_ask_question(ri, TEST_STR1, TEST_STR1));
    check(k5_response_items_ask_question(ri, TEST_STR2, NULL));
    check_pred(nstrcmp(k5_response_items_get_challenge(ri, TEST_STR1),
                       TEST_STR1) == 0);
    check_pred(nstrcmp(k5_response_items_get_challenge(ri, TEST_STR2),
                       NULL) == 0);
    check_pred(!k5_response_items_empty(ri));

    k5_response_items_reset(ri);
    check_pred(k5_response_items_empty(ri));
    check_pred(k5_response_items_get_challenge(ri, TEST_STR1) == NULL);
    check_pred(k5_response_items_get_challenge(ri, TEST_STR2) == NULL);

    check(k5_response_items_ask_question(ri, TEST_STR1, TEST_STR1));
    check_pred(nstrcmp(k5_response_items_get_challenge(ri, TEST_STR1),
                       TEST_STR1) == 0);
    check(k5_response_items_set_answer(ri, TEST_STR1, TEST_STR1));
    check_pred(nstrcmp(k5_response_items_get_answer(ri, TEST_STR1),
                       TEST_STR1) == 0);

    k5_response_items_free(ri);

    return 0;
}
