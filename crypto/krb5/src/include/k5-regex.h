/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* include/k5-regex.h - Compatibility glue for std::regex on Windows */

/*
 * Copyright (C) 2024 United States Government as represented by the
 * Secretary of the Navy.
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

/*
 * On POSIX platforms we can use the standardized regcomp()/regexec() function
 * calls.  Windows does not provide POSIX regex functions, but does provide a
 * C++ interface (std::regex) that has the same functionality.
 */

#ifndef _K5_REGEX_H_
#define _K5_REGEX_H_

#ifndef _WIN32

/* On POSIX platforms, just include regex.h. */
#include <regex.h>

#else /* _WIN32 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* On Windows, emulate the POSIX interface using functions from
 * libkrb5support. */

typedef struct {
    size_t re_nsub;             /* Number of subexpressions */
    void *regex;                /* Pointer to std::basic_regex */
    char errmsg[128];           /* Regular expression error message */
} regex_t;

typedef ssize_t regoff_t;

typedef struct {
    regoff_t rm_so;
    regoff_t rm_eo;
} regmatch_t;

/*
 * Flags to k5_regcomp()
 */

#define REG_BASIC       0x00    /* Basic regular expressions */
#define REG_EXTENDED    0x01    /* Extended regular expressions */
#define REG_ICASE       0x02    /* Case-insensitive match */
#define REG_NOSUB       0x04    /* Do not do submatching */

/*
 * Flags to k5_regexec()
 */

#define REG_NOTBOL      0x01    /* First character not at beginning of line */
#define REG_NOTEOL      0x02    /* Last character not at end of line */

/*
 * Error return codes for k5_regcomp()/k5_regexec()
 *
 * We only define REG_NOMATCH and REG_BADPAT, since no Kerberos code looks
 * for anything other than success and REG_NOMATCH.
 */

#define REG_NOMATCH     1
#define REG_BADPAT      2

/*
 * Note that we don't follow the POSIX API exactly because k5_regexec()
 * doesn't declare regex_t as const; that's so we can store an error
 * string.
 */
int k5_regcomp(regex_t *preg, const char *pattern, int flags);
int k5_regexec(regex_t *preg, const char *string, size_t,
               regmatch_t pmatch[], int flags);
size_t k5_regerror(int code, const regex_t *preg, char *errmsg,
                   size_t errmsg_size);
void k5_regfree(regex_t *preg);

#define regcomp k5_regcomp
#define regexec k5_regexec
#define regerror k5_regerror
#define regfree k5_regfree

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _WIN32 */
#endif /* _K5_REGEX_H_ */
