/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/misc/test_getpw.c */
/*
 * Copyright (C) 2012 by the Red Hat Inc.
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

#include "autoconf.h"
#include "krb5.h"

#include <sys/types.h>
#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static krb5_data result_utf8 = {
    0, 23, "This is a valid string.",
};

static krb5_data result_invalid_utf8 = {
    0, 19, "\0This is not valid.",
};

static krb5_data result_ad_complex = {
    0, 30,
    "\0\0"             /* zero bytes */
    "\0\0\0\0"         /* min length */
    "\0\0\0\0"         /* history */
    "\0\0\0\1"         /* properties, complex */
    "\0\0\0\0\0\0\0\0" /* expire */
    "\0\0\0\0\0\0\0\0" /* min age */
};

static krb5_data result_ad_length = {
    0, 30,
    "\0\0"             /* zero bytes */
    "\0\0\0\x0d"       /* min length, 13 charaters */
    "\0\0\0\0"         /* history */
    "\0\0\0\0"         /* properties */
    "\0\0\0\0\0\0\0\0" /* expire */
    "\0\0\0\0\0\0\0\0" /* min age */
};

static krb5_data result_ad_history = {
    0, 30,
    "\0\0"             /* zero bytes */
    "\0\0\0\0"         /* min length */
    "\0\0\0\x09"       /* history, 9 passwords */
    "\0\0\0\0"         /* properties */
    "\0\0\0\0\0\0\0\0" /* expire */
    "\0\0\0\0\0\0\0\0" /* min age */
};

static krb5_data result_ad_age = {
    0, 30,
    "\0\0"                        /* zero bytes */
    "\0\0\0\0"                    /* min length */
    "\0\0\0\0"                    /* history, 9 passwords */
    "\0\0\0\0"                    /* properties */
    "\0\0\0\0\0\0\0\0"            /* expire */
    "\0\0\x01\x92\x54\xd3\x80\0"  /* min age, 2 days */
};

static krb5_data result_ad_all = {
    0, 30,
    "\0\0"                      /* zero bytes */
    "\0\0\0\x05"                /* min length, 5 characters */
    "\0\0\0\x0D"                /* history, 13 passwords */
    "\0\0\0\x01"                /* properties, complex */
    "\0\0\0\0\0\0\0\0"          /* expire */
    "\0\0\0\xc9\x2a\x69\xc0\0"  /* min age, 1 day */
};

static void
check(krb5_error_code code)
{
    if (code != 0) {
        com_err("t_vfy_increds", code, "");
        abort();
    }
}

static void
check_msg(const char *real, const char *expected)
{
    if (strstr(real, expected) == NULL) {
        fprintf(stderr, "Expected to see: %s\n", expected);
        abort();
    }
}

int
main(void)
{
    krb5_context context;
    char *msg;

    setlocale(LC_ALL, "C");

    check(krb5_init_context(&context));

    /* Valid utf-8 data in the result should be returned as is */
    check(krb5_chpw_message(context, &result_utf8, &msg));
    printf("  UTF8 valid:   %s\n", msg);
    check_msg(msg, "This is a valid string.");
    free(msg);

    /* Invalid data should have a generic message. */
    check(krb5_chpw_message(context, &result_invalid_utf8, &msg));
    printf("  UTF8 invalid: %s\n", msg);
    check_msg(msg, "contact your administrator");
    free(msg);

    /* AD data with complex data requirement */
    check(krb5_chpw_message(context, &result_ad_complex, &msg));
    printf("  AD complex:   %s\n", msg);
    check_msg(msg, "The password must include numbers or symbols.");
    check_msg(msg, "Don't include any part of your name in the password.");
    free(msg);

    /* AD data with min password length */
    check(krb5_chpw_message(context, &result_ad_length, &msg));
    printf("  AD length:    %s\n", msg);
    check_msg(msg, "The password must contain at least 13 characters.");
    free(msg);

    /* AD data with history requirements */
    check(krb5_chpw_message(context, &result_ad_history, &msg));
    printf("  AD history:   %s\n", msg);
    check_msg(msg, "The password must be different from the previous 9 "
              "passwords.");
    free(msg);

    /* AD data with minimum age */
    check(krb5_chpw_message(context, &result_ad_age, &msg));
    printf("  AD min age:   %s\n", msg);
    check_msg(msg, "The password can only be changed every 2 days.");
    free(msg);

    /* AD data with all */
    check(krb5_chpw_message(context, &result_ad_all, &msg));
    printf("  AD all:       %s\n", msg);
    check_msg(msg, "The password can only be changed once a day.");
    check_msg(msg, "The password must be different from the previous 13 "
              "passwords.");
    check_msg(msg, "The password must contain at least 5 characters.");
    check_msg(msg, "The password must include numbers or symbols.");
    check_msg(msg, "Don't include any part of your name in the password.");
    free(msg);

    krb5_free_context(context);
    exit(0);
}
