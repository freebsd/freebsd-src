/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/t_deltat.c */
/*
 * Copyright 1999 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"

int
main (void)
{
    struct {
        char *string;
        krb5_deltat expected;
        int is_error;
#define GOOD(STR,VAL) { STR, VAL, 0 }
#define BAD(STR) { STR, 0, 1 }
#define DAY (24 * 3600)
#define HOUR 3600
#ifdef MIN
#undef MIN
#endif
#define MIN 60
    } values[] = {
        /* d-h-m-s patterns */
        GOOD ("3d", 3*DAY),
        GOOD ("3h", 3*HOUR),
        GOOD ("3m", 3*MIN),
        GOOD ("3s", 3),
        BAD ("3dd"),
        GOOD ("3d4m    42s", 3 * DAY + 4 * MIN + 42),
        GOOD ("3d-1h", 3 * DAY - 1 * HOUR),
        GOOD ("3d -1h", 3 * DAY - HOUR),
        GOOD ("3d4h5m6s", 3 * DAY + 4 * HOUR + 5 * MIN + 6),
        BAD ("3d4m5h"),
        GOOD ("12345s", 12345),
        GOOD ("1m 12345s", MIN + 12345),
        GOOD ("1m12345s", MIN + 12345),
        GOOD ("3d 0m", 3 * DAY),
        GOOD ("3d 0m  ", 3 * DAY),
        GOOD ("3d \n\t 0m  ", 3 * DAY),
        /* colon patterns */
        GOOD ("42-13:42:47", 42 * DAY + 13 * HOUR + 42 * MIN + 47),
        BAD ("3: 4"),
        BAD ("13:0003"),
        GOOD ("12:34", 12 * HOUR + 34 * MIN),
        GOOD ("1:02:03", 1 * HOUR + 2 * MIN + 3),
        BAD ("3:-4"),
        /* XX We might want to require exactly two digits after a colon?  */
        GOOD ("3:4", 3 * HOUR + 4 * MIN),
        /* misc */
        GOOD ("42", 42),
        BAD ("1-2"),
        /* Test overflow limitations */
        GOOD ("2147483647s", 2147483647),
        BAD ("2147483648s"),
        GOOD ("24855d", 24855 * DAY),
        BAD ("24856d"),
        BAD ("24855d 100000000h"),
        GOOD ("24855d 3h", 24855 * DAY + 3 * HOUR),
        BAD ("24855d 4h"),
        GOOD ("24855d 11647s", 24855 * DAY + 11647),
        BAD ("24855d 11648s"),
        GOOD ("24855d 194m 7s", 24855 * DAY + 194 * MIN + 7),
        BAD ("24855d 194m 8s"),
        BAD ("24855d 195m"),
        BAD ("24855d 19500000000m"),
        GOOD ("24855d 3h 14m 7s", 24855 * DAY + 3 * HOUR + 14 * MIN + 7),
        BAD ("24855d 3h 14m 8s"),
        GOOD ("596523h", 596523 * HOUR),
        BAD ("596524h"),
        GOOD ("596523h 847s", 596523 * HOUR + 847),
        BAD ("596523h 848s"),
        GOOD ("596523h 14m 7s", 596523 * HOUR + 14 * MIN + 7),
        BAD ("596523h 14m 8s"),
        GOOD ("35791394m", 35791394 * MIN),
        GOOD ("35791394m7s", 35791394 * MIN + 7),
        BAD ("35791394m8s"),
        /* Test underflow */
        GOOD ("-2147483647s", -2147483647),
        /* This should be valid, but isn't */
        /*BAD ("-2147483648s"),*/
        GOOD ("-24855d", -24855 * DAY),
        BAD ("-24856d"),
        BAD ("-24855d -100000000h"),
        GOOD ("-24855d -3h", -24855 * DAY - 3 * HOUR),
        BAD ("-24855d -4h"),
        GOOD ("-24855d -11647s", -24855 * DAY - 11647),
        BAD ("-24855d -11649s"),
        GOOD ("-24855d -194m -7s", -24855 * DAY - 194 * MIN - 7),
        BAD ("-24855d -194m -9s"),
        BAD ("-24855d -195m"),
        BAD ("-24855d -19500000000m"),
        GOOD ("-24855d -3h -14m -7s", -24855 * DAY - 3 * HOUR - 14 * MIN - 7),
        BAD ("-24855d -3h -14m -9s"),
        GOOD ("-596523h", -596523 * HOUR),
        BAD ("-596524h"),
        GOOD ("-596523h -847s", -596523 * HOUR - 847),
        GOOD ("-596523h -848s", -596523 * HOUR - 848),
        BAD ("-596523h -849s"),
        GOOD ("-596523h -14m -8s", -596523 * HOUR - 14 * MIN - 8),
        BAD ("-596523h -14m -9s"),
        GOOD ("-35791394m", -35791394 * MIN),
        GOOD ("-35791394m7s", -35791394 * MIN + 7),
        BAD ("-35791394m-9s"),

    };
    int fail = 0;
    size_t i;

    for (i = 0; i < sizeof(values)/sizeof(values[0]); i++) {
        krb5_deltat result;
        krb5_error_code code;

        code = krb5_string_to_deltat (values[i].string, &result);
        if (code && !values[i].is_error) {
            fprintf (stderr, "unexpected error for `%s'\n", values[i].string);
            fail++;
        } else if (!code && values[i].is_error) {
            fprintf (stderr, "expected but didn't get error for `%s'\n",
                     values[i].string);
            fail++;
        } else if (code && values[i].is_error) {
            /* do nothing */
        } else if (result != values[i].expected) {
            fprintf (stderr, "got %ld instead of expected %ld for `%s'\n",
                     (long) result, (long) values[i].expected,
                     values[i].string);
            fail++;
        }
    }
    if (fail == 0)
        printf ("Passed all %d tests.\n", (int)i);
    else
        printf ("Failed %d of %d tests.\n", fail, (int)i);
    return fail;
}
