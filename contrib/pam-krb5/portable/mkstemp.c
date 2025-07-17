/*
 * Replacement for a missing mkstemp.
 *
 * Provides the same functionality as the library function mkstemp for those
 * systems that don't have it.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2009, 2011, 2014
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
#include <fcntl.h>
#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif
#include <time.h>

/*
 * If we're running the test suite, rename mkstemp to avoid conflicts with the
 * system version.  #undef it first because some systems may define it to
 * another name.
 */
#if TESTING
#    undef mkstemp
#    define mkstemp test_mkstemp
int test_mkstemp(char *);
#endif

/* Pick the longest available integer type. */
#if HAVE_LONG_LONG_INT
typedef unsigned long long long_int_type;
#else
typedef unsigned long long_int_type;
#endif

int
mkstemp(char *template)
{
    static const char letters[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    size_t length;
    char *XXXXXX;
    struct timeval tv;
    long_int_type randnum, working;
    int i, tries, fd;

    /*
     * Make sure we have a valid template and initialize p to point at the
     * beginning of the template portion of the string.
     */
    length = strlen(template);
    if (length < 6) {
        errno = EINVAL;
        return -1;
    }
    XXXXXX = template + length - 6;
    if (strcmp(XXXXXX, "XXXXXX") != 0) {
        errno = EINVAL;
        return -1;
    }

    /* Get some more-or-less random information. */
    gettimeofday(&tv, NULL);
    randnum = ((long_int_type) tv.tv_usec << 16) ^ tv.tv_sec ^ getpid();

    /*
     * Now, try to find a working file name.  We try no more than TMP_MAX file
     * names.
     */
    for (tries = 0; tries < TMP_MAX; tries++) {
        for (working = randnum, i = 0; i < 6; i++) {
            XXXXXX[i] = letters[working % 62];
            working /= 62;
        }
        fd = open(template, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd >= 0 || (errno != EEXIST && errno != EISDIR))
            return fd;

        /*
         * This is a relatively random increment.  Cut off the tail end of
         * tv_usec since it's often predictable.
         */
        randnum += (tv.tv_usec >> 10) & 0xfff;
    }
    errno = EEXIST;
    return -1;
}
