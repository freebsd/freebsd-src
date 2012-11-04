/*
 * Automated Testing Framework (atf)
 *
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <atf-c.h>

#include "dynstr.h"
#include "test_helpers.h"

/* ---------------------------------------------------------------------
 * Test cases for the free functions.
 * --------------------------------------------------------------------- */

/* TODO: Add checks for build_check_c_o and the macros defined in the
 * header file. */

ATF_TC(grep_string);
ATF_TC_HEAD(grep_string, tc)
{
    atf_tc_set_md_var(tc, "descr", "Tests the grep_string helper "
                      "function");
}
ATF_TC_BODY(grep_string, tc)
{
    atf_dynstr_t str;

    atf_dynstr_init_fmt(&str, "a string - aaaabbbb");
    ATF_CHECK(grep_string(&str, "a string"));
    ATF_CHECK(grep_string(&str, "^a string"));
    ATF_CHECK(grep_string(&str, "aaaabbbb$"));
    ATF_CHECK(grep_string(&str, "aa.*bb"));
    ATF_CHECK(!grep_string(&str, "foo"));
    ATF_CHECK(!grep_string(&str, "bar"));
    ATF_CHECK(!grep_string(&str, "aaaaa"));

    atf_dynstr_fini(&str);
}


ATF_TC(grep_file);
ATF_TC_HEAD(grep_file, tc)
{
    atf_tc_set_md_var(tc, "descr", "Tests the grep_file helper function");
}
ATF_TC_BODY(grep_file, tc)
{
    FILE *f;

    f = fopen("test.txt", "w");
    ATF_CHECK(f != NULL);
    fprintf(f, "line1\n");
    fprintf(f, "the second line\n");
    fprintf(f, "aaaabbbb\n");
    fclose(f);

    ATF_CHECK(grep_file("test.txt", "line1"));
    ATF_CHECK(grep_file("test.txt", "line%d", 1));
    ATF_CHECK(grep_file("test.txt", "second line"));
    ATF_CHECK(grep_file("test.txt", "aa.*bb"));
    ATF_CHECK(!grep_file("test.txt", "foo"));
    ATF_CHECK(!grep_file("test.txt", "bar"));
    ATF_CHECK(!grep_file("test.txt", "aaaaa"));
}

ATF_TC(read_line);
ATF_TC_HEAD(read_line, tc)
{
    atf_tc_set_md_var(tc, "descr", "Tests the read_line function");
}
ATF_TC_BODY(read_line, tc)
{
    const char *l1 = "First line with % formatting % characters %";
    const char *l2 = "Second line; much longer than the first one";
    const char *l3 = "Last line, without terminator";

    {
        FILE *f;

        f = fopen("test", "w");
        ATF_REQUIRE(f != NULL);
        fclose(f);
    }

    {
        int fd;
        atf_dynstr_t dest;
        bool eof;

        fd = open("test", O_RDONLY);
        ATF_REQUIRE(fd != -1);

        RE(atf_dynstr_init(&dest));
        eof = read_line(fd, &dest);
        ATF_REQUIRE(eof);
        atf_dynstr_fini(&dest);
    }

    {
        FILE *f;

        f = fopen("test", "w");
        ATF_REQUIRE(f != NULL);

        fprintf(f, "%s\n", l1);
        fprintf(f, "%s\n", l2);
        fprintf(f, "%s", l3);

        fclose(f);
    }

    {
        int fd;
        atf_dynstr_t dest;
        bool eof;

        fd = open("test", O_RDONLY);
        ATF_REQUIRE(fd != -1);

        RE(atf_dynstr_init(&dest));
        eof = read_line(fd, &dest);
        ATF_REQUIRE(!eof);
        printf("1st line: >%s<\n", atf_dynstr_cstring(&dest));
        ATF_REQUIRE(atf_equal_dynstr_cstring(&dest, l1));
        atf_dynstr_fini(&dest);

        RE(atf_dynstr_init(&dest));
        eof = read_line(fd, &dest);
        ATF_REQUIRE(!eof);
        printf("2nd line: >%s<\n", atf_dynstr_cstring(&dest));
        ATF_REQUIRE(atf_equal_dynstr_cstring(&dest, l2));
        atf_dynstr_fini(&dest);

        RE(atf_dynstr_init(&dest));
        eof = read_line(fd, &dest);
        ATF_REQUIRE(eof);
        printf("3rd line: >%s<\n", atf_dynstr_cstring(&dest));
        ATF_REQUIRE(atf_equal_dynstr_cstring(&dest, l3));
        atf_dynstr_fini(&dest);

        close(fd);
    }
}

/* ---------------------------------------------------------------------
 * Main.
 * --------------------------------------------------------------------- */

ATF_TP_ADD_TCS(tp)
{
    /* Add the tests for the free functions. */
    ATF_TP_ADD_TC(tp, grep_string);
    ATF_TP_ADD_TC(tp, grep_file);
    ATF_TP_ADD_TC(tp, read_line);

    return atf_no_error();
}
