/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* regex.cpp - Glue routines to std::regex functions */

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
 * These functions provide a mostly-complete POSIX regex(3)
 * implementation using C++ std::regex.  Deficiencies are noted below.
 */

#include "k5-platform.h"
#include "k5-regex.h"

#include <regex>

/*
 * Our implementation of regcomp() which calls into std::regex.  We implement
 * the standard flags, but not the non-portable extensions present on some
 * platforms.
 */
extern "C" int
k5_regcomp(regex_t *preg, const char *pattern, int cflags)
{
    std::regex *r;
    std::regex_constants::syntax_option_type flags;

    memset(preg, 0, sizeof(*preg));

    flags = (cflags & REG_EXTENDED) ? std::regex::extended : std::regex::basic;
    if (cflags & REG_ICASE)
        flags |= std::regex::icase;
    if (cflags & REG_NOSUB)
        flags |= std::regex::nosubs;

    try {
        r = new std::regex(pattern, flags);
        preg->regex = r;
        preg->re_nsub = r->mark_count();
    } catch (std::regex_error& e) {
        /* Save the error message in errmsg.  We don't actually use the
         * error code for anything; return REG_BADPAT for everything. */
        strlcpy(preg->errmsg, e.what(), sizeof(preg->errmsg));
        return REG_BADPAT;
    }

    return 0;
}

extern "C" int
k5_regexec(regex_t *preg, const char *string, size_t nmatch,
           regmatch_t pmatch[], int eflags)
{
    size_t i;
    std::cmatch cm;
    std::regex_constants::match_flag_type flags;
    std::regex *r = static_cast<std::regex *>(preg->regex);

    flags = std::regex_constants::match_default;
    if (eflags & REG_NOTBOL)
        flags |= std::regex_constants::match_not_bol;
    if (eflags & REG_NOTEOL)
        flags |= std::regex_constants::match_not_eol;

    try {
        if (!std::regex_search(string, cm, *r, flags))
            return REG_NOMATCH;

        /*
         * If given, fill in pmatch with the full match string and any
         * sub-matches.  If we set nosub previously we shouldn't have any
         * submatches, but should still have the first element which refers to
         * the whole match string.
         */

        for (i = 0; i < nmatch; i++) {
            /*
             * If we're past the end of the match list (cm.size()) or
             * this sub-match didn't match (!cm[i].matched()) then
             * return -1 for those array members.
             */
            if (i >= cm.size() || !cm[i].matched) {
                pmatch[i].rm_so = pmatch[i].rm_eo = -1;
            } else {
                pmatch[i].rm_so = cm.position(i);
                pmatch[i].rm_eo = cm.position(i) + cm.length(i);
            }
        }
    } catch (std::regex_error& e) {
        /* See above. */
        strlcpy(preg->errmsg, e.what(), sizeof(preg->errmsg));
        return REG_BADPAT;
    }

    return 0;
}

/*
 * Report back an error string.  We don't use the errcode for anything, just
 * the error string stored in regex_t.  If we don't have an error string,
 * return an "unknown error" message.
 */
extern "C" size_t
k5_regerror(int errcode, const regex_t *preg, char *errbuf, size_t errbuf_size)
{
    const char *err;
    size_t errlen;

    err = preg->errmsg;
    if (*err == '\0')
        err = "Unknown regular expression error";

    if (errbuf != NULL && errbuf_size > 0)
        strlcpy(errbuf, err, errbuf_size);
    return strlen(err);
}

extern "C" void
k5_regfree(regex_t *preg)
{
    if (preg->regex == NULL)
        return;
    delete static_cast<std::regex *>(preg->regex);
    memset(preg, 0, sizeof(*preg));
}
