/*
 * Replacement for a missing reallocarray.
 *
 * Provides the same functionality as the OpenBSD library function
 * reallocarray for those systems that don't have it.  This function is the
 * same as realloc, but takes the size arguments in the same form as calloc
 * and checks for overflow so that the caller doesn't need to.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2017 Russ Allbery <eagle@eyrie.org>
 * Copyright 2014
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
 * If we're running the test suite, rename reallocarray to avoid conflicts
 * with the system version.  #undef it first because some systems may define
 * it to another name.
 */
#if TESTING
#    undef reallocarray
#    define reallocarray test_reallocarray
void *test_reallocarray(void *, size_t, size_t);
#endif

/*
 * nmemb * size cannot overflow if both are smaller than sqrt(SIZE_MAX).  We
 * can calculate that value statically by using 2^(sizeof(size_t) * 8) as the
 * value of SIZE_MAX and then taking the square root, which gives
 * 2^(sizeof(size_t) * 4).  Compute the exponentiation with shift.
 */
#define CHECK_THRESHOLD (1UL << (sizeof(size_t) * 4))

void *
reallocarray(void *ptr, size_t nmemb, size_t size)
{
    if (nmemb >= CHECK_THRESHOLD || size >= CHECK_THRESHOLD)
        if (nmemb > 0 && SIZE_MAX / nmemb <= size) {
            errno = ENOMEM;
            return NULL;
        }

    /* Avoid a zero-size allocation. */
    if (nmemb == 0 || size == 0) {
        nmemb = 1;
        size = 1;
    }
    return realloc(ptr, nmemb * size);
}
