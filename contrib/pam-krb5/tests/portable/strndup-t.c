/*
 * strndup test suite.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2018 Russ Allbery <eagle@eyrie.org>
 * Copyright 2011-2012
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
#include <portable/system.h>

#include <errno.h>

#include <tests/tap/basic.h>

char *test_strndup(const char *, size_t);


int
main(void)
{
    char buffer[3];
    char *result;

    plan(7);

    result = test_strndup("foo", 8);
    is_string("foo", result, "strndup longer than string");
    free(result);
    result = test_strndup("foo", 2);
    is_string("fo", result, "strndup shorter than string");
    free(result);
    result = test_strndup("foo", 3);
    is_string("foo", result, "strndup same size as string");
    free(result);
    result = test_strndup("foo", 0);
    is_string("", result, "strndup of size 0");
    free(result);
    memcpy(buffer, "foo", 3);
    result = test_strndup(buffer, 3);
    is_string("foo", result, "strndup of non-nul-terminated string");
    free(result);
    errno = 0;
    result = test_strndup(NULL, 0);
    is_string(NULL, result, "strndup of NULL");
    is_int(errno, EINVAL, "...and returns EINVAL");

    return 0;
}
