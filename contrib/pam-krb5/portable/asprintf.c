/*
 * Replacement for a missing asprintf and vasprintf.
 *
 * Provides the same functionality as the standard GNU library routines
 * asprintf and vasprintf for those platforms that don't have them.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2006, 2015 Russ Allbery <eagle@eyrie.org>
 * Copyright 2008-2009, 2011, 2013
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

#include <errno.h>

/*
 * If we're running the test suite, rename the functions to avoid conflicts
 * with the system versions.
 */
#if TESTING
#    undef asprintf
#    undef vasprintf
#    define asprintf  test_asprintf
#    define vasprintf test_vasprintf
int test_asprintf(char **, const char *, ...)
    __attribute__((__format__(printf, 2, 3)));
int test_vasprintf(char **, const char *, va_list)
    __attribute__((__format__(printf, 2, 0)));
#endif


int
asprintf(char **strp, const char *fmt, ...)
{
    va_list args;
    int status;

    va_start(args, fmt);
    status = vasprintf(strp, fmt, args);
    va_end(args);
    return status;
}


int
vasprintf(char **strp, const char *fmt, va_list args)
{
    va_list args_copy;
    int status, needed, oerrno;

    va_copy(args_copy, args);
    needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    if (needed < 0) {
        *strp = NULL;
        return needed;
    }
    *strp = malloc(needed + 1);
    if (*strp == NULL)
        return -1;
    status = vsnprintf(*strp, needed + 1, fmt, args);
    if (status >= 0)
        return status;
    else {
        oerrno = errno;
        free(*strp);
        *strp = NULL;
        errno = oerrno;
        return status;
    }
}
