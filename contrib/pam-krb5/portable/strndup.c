/*
 * Replacement for a missing strndup.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
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

/*
 * If we're running the test suite, rename the functions to avoid conflicts
 * with the system versions.
 */
#if TESTING
#    undef strndup
#    define strndup test_strndup
char *test_strndup(const char *, size_t);
#endif

char *
strndup(const char *s, size_t n)
{
    const char *p;
    size_t length;
    char *copy;

    if (s == NULL) {
        errno = EINVAL;
        return NULL;
    }

    /* Don't assume that the source string is nul-terminated. */
    for (p = s; (size_t)(p - s) < n && *p != '\0'; p++)
        ;
    length = p - s;
    copy = malloc(length + 1);
    if (copy == NULL)
        return NULL;
    memcpy(copy, s, length);
    copy[length] = '\0';
    return copy;
}
