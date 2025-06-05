/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/support/strerror_r.c - strerror_r compatibility shim */
/*
 * Copyright (C) 2014 by the Massachusetts Institute of Technology.
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

#include <k5-platform.h>
#undef strerror_r

#if defined(_WIN32)

/* Implement strerror_r in terms of strerror_s. */
int
k5_strerror_r(int errnum, char *buf, size_t buflen)
{
    int st;

    st = strerror_s(buf, buflen, errnum);
    if (st != 0) {
        errno = st;
        return -1;
    }
    return 0;
}

#elif !defined(HAVE_STRERROR_R)

/* Implement strerror_r in terms of strerror (not thread-safe). */
int
k5_strerror_r(int errnum, char *buf, size_t buflen)
{
    if (strlcpy(buf, strerror(errnum), buflen) >= buflen) {
        errno = ERANGE;
        return -1;
    }
    return 0;
}

#elif defined(STRERROR_R_CHAR_P)

/*
 * Implement the POSIX strerror_r API in terms of the GNU strerror_r, which
 * returns a pointer to either the caller buffer or a constant string.  This is
 * the default version on glibc systems when _GNU_SOURCE is defined.
 */
int
k5_strerror_r(int errnum, char *buf, size_t buflen)
{
    const char *str;

    str = strerror_r(errnum, buf, buflen);
    if (str != buf) {
        if (strlcpy(buf, str, buflen) >= buflen) {
            errno = ERANGE;
            return -1;
        }
    }
    return 0;
}

#else

/* Define a stub in terms of the real strerror_r, just to simplify the library
 * export list.  This shouldn't get used. */
int
k5_strerror_r(int errnum, char *buf, size_t buflen)
{
    return strerror_r(errnum, buf, buflen);
}

#endif
