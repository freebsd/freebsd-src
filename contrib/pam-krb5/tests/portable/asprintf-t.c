/*
 * asprintf and vasprintf test suite.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2014, 2018 Russ Allbery <eagle@eyrie.org>
 * Copyright 2006-2009, 2011
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Copying and distribution of this file, with or without modification, are
 * permitted in any medium without royalty provided the copyright notice and
 * this notice are preserved.  This file is offered as-is, without any
 * warranty.
 *
 * SPDX-License-Identifier: FSFAP
 */

#include <config.h>
#include <portable/macros.h>
#include <portable/system.h>

#include <tests/tap/basic.h>

int test_asprintf(char **, const char *, ...)
    __attribute__((__format__(printf, 2, 3)));
int test_vasprintf(char **, const char *, va_list)
    __attribute__((__format__(printf, 2, 0)));

static int __attribute__((__format__(printf, 2, 3)))
vatest(char **result, const char *format, ...)
{
    va_list args;
    int status;

    va_start(args, format);
    status = test_vasprintf(result, format, args);
    va_end(args);
    return status;
}

int
main(void)
{
    char *result = NULL;

    plan(12);

    is_int(7, test_asprintf(&result, "%s", "testing"), "asprintf length");
    is_string("testing", result, "asprintf result");
    free(result);
    ok(3, "free asprintf");
    is_int(0, test_asprintf(&result, "%s", ""), "asprintf empty length");
    is_string("", result, "asprintf empty string");
    free(result);
    ok(6, "free asprintf of empty string");

    is_int(6, vatest(&result, "%d %s", 2, "test"), "vasprintf length");
    is_string("2 test", result, "vasprintf result");
    free(result);
    ok(9, "free vasprintf");
    is_int(0, vatest(&result, "%s", ""), "vasprintf empty length");
    is_string("", result, "vasprintf empty string");
    free(result);
    ok(12, "free vasprintf of empty string");

    return 0;
}
