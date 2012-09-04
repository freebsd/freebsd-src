//
// Automated Testing Framework (atf)
//
// Copyright (c) 2009 The NetBSD Foundation, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
// CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
// IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include <cstring>
#include <iostream>

#include "../macros.hpp"

#include "env.hpp"
#include "ui.hpp"

// ------------------------------------------------------------------------
// Test cases for the free functions.
// ------------------------------------------------------------------------

struct test {
    const char *tc;
    const char *tag;
    bool repeat;
    size_t col;
    const char *fmt;
    const char *result;
} tests[] = {
    //
    // wo_tag
    //

    {
        "wo_tag",
        "",
        false,
        0,
        "12345",
        "12345",
    },

    {
        "wo_tag",
        "",
        false,
        0,
        "12345  ",
        "12345",
    },

    {
        "wo_tag",
        "",
        false,
        0,
        "12345 7890",
        "12345 7890",
    },

    {
        "wo_tag",
        "",
        false,
        0,
        "12345 789012 45",
        "12345 789012 45",
    },

    {
        "wo_tag",
        "",
        false,
        0,
        "12345 789012 456",
        "12345 789012\n456",
    },

    {
        "wo_tag",
        "",
        false,
        0,
        "1234567890123456",
        "1234567890123456",
    },

    // TODO(jmmv): Fix the code to pass this test...
//    {
//        "wo_tag",
//        "",
//        false,
//        0,
//        " 2345678901234567",
//        "\n2345678901234567",
//    },

    {
        "wo_tag",
        "",
        false,
        0,
        "12345 789012345 78",
        "12345 789012345\n78",
    },

    //
    // wo_tag_col
    //

    {
        "wo_tag_col",
        "",
        false,
        10,
        "12345",
        "          12345",
    },

    {
        "wo_tag_col",
        "",
        false,
        10,
        "12345 7890",
        "          12345\n"
        "          7890",
    },

    {
        "wo_tag_col",
        "",
        false,
        10,
        "1 3 5 7 9",
        "          1 3 5\n"
        "          7 9",
    },

    //
    // w_tag_no_repeat
    //

    {
        "w_tag_no_repeat",
        "1234: ",
        false,
        0,
        "789012345",
        "1234: 789012345",
    },

    {
        "w_tag_no_repeat",
        "1234: ",
        false,
        0,
        "789 1234 56789",
        "1234: 789 1234\n"
        "      56789",
    },

    {
        "w_tag_no_repeat",
        "1234: ",
        false,
        0,
        "789012345",
        "1234: 789012345",
    },

    {
        "w_tag_no_repeat",
        "1234: ",
        false,
        0,
        "789012345 7890",
        "1234: 789012345\n"
        "      7890",
    },

    //
    // w_tag_repeat
    //

    {
        "w_tag_repeat",
        "1234: ",
        true,
        0,
        "789012345",
        "1234: 789012345",
    },

    {
        "w_tag_repeat",
        "1234: ",
        true,
        0,
        "789 1234 56789",
        "1234: 789 1234\n"
        "1234: 56789",
    },

    {
        "w_tag_repeat",
        "1234: ",
        true,
        0,
        "789012345",
        "1234: 789012345",
    },

    {
        "w_tag_no_repeat",
        "1234: ",
        true,
        0,
        "789012345 7890",
        "1234: 789012345\n"
        "1234: 7890",
    },

    //
    // w_tag_col
    //

    {
        "w_tag_col",
        "1234:",
        false,
        10,
        "1 3 5",
        "1234:     1 3 5",
    },

    {
        "w_tag_col",
        "1234:",
        false,
        10,
        "1 3 5 7 9",
        "1234:     1 3 5\n"
        "          7 9",
    },

    {
        "w_tag_col",
        "1234:",
        true,
        10,
        "1 3 5 7 9",
        "1234:     1 3 5\n"
        "1234:     7 9",
    },

    //
    // paragraphs
    //

    {
        "paragraphs",
        "",
        false,
        0,
        "1 3 5\n\n",
        "1 3 5"
    },

    {
        "paragraphs",
        "",
        false,
        0,
        "1 3 5\n2 4 6",
        "1 3 5\n\n2 4 6"
    },

    {
        "paragraphs",
        "",
        false,
        0,
        "1234 6789 123456\n2 4 6",
        "1234 6789\n123456\n\n2 4 6"
    },

    {
        "paragraphs",
        "12: ",
        false,
        0,
        "56789 123456\n2 4 6",
        "12: 56789\n    123456\n\n    2 4 6"
    },

    {
        "paragraphs",
        "12: ",
        true,
        0,
        "56789 123456\n2 4 6",
        "12: 56789\n12: 123456\n12: \n12: 2 4 6"
    },

    {
        "paragraphs",
        "12:",
        false,
        4,
        "56789 123456\n2 4 6",
        "12: 56789\n    123456\n\n    2 4 6"
    },

    {
        "paragraphs",
        "12:",
        true,
        4,
        "56789 123456\n2 4 6",
        "12: 56789\n12: 123456\n12:\n12: 2 4 6"
    },

    //
    // end
    //

    {
        NULL,
        NULL,
        false,
        0,
        NULL,
        NULL,
    },
};

static
void
run_tests(const char *tc)
{
    struct test *t;

    std::cout << "Running tests for " << tc << "\n";

    atf::env::set("COLUMNS", "15");

    for (t = &tests[0]; t->tc != NULL; t++) {
        if (std::strcmp(t->tc, tc) == 0) {
            std::cout << "\n";
            std::cout << "Testing with tag '" << t->tag << "', '"
                << (t->repeat ? "repeat" : "no repeat") << "', col "
                << t->col << "\n";
            std::cout << "Input: >>>" << t->fmt << "<<<\n";
            std::cout << "Expected output: >>>" << t->result << "<<<\n";

            std::string result = atf::ui::format_text_with_tag(t->fmt, t->tag,
                t->repeat, t->col);
            std::cout << "Output         : >>>" << result << "<<<\n";
            ATF_REQUIRE_EQ(t->result, result);
        }
    }
}

ATF_TEST_CASE(wo_tag);
ATF_TEST_CASE_HEAD(wo_tag)
{
    set_md_var("descr", "Checks formatting without tags");
}
ATF_TEST_CASE_BODY(wo_tag)
{
    run_tests("wo_tag");
}

ATF_TEST_CASE(wo_tag_col);
ATF_TEST_CASE_HEAD(wo_tag_col)
{
    set_md_var("descr", "Checks formatting without tags and with a non-zero "
        "starting column");
}
ATF_TEST_CASE_BODY(wo_tag_col)
{
    run_tests("wo_tag_col");
}

ATF_TEST_CASE(w_tag_no_repeat);
ATF_TEST_CASE_HEAD(w_tag_no_repeat)
{
    set_md_var("descr", "Checks formatting with a tag");
}
ATF_TEST_CASE_BODY(w_tag_no_repeat)
{
    run_tests("w_tag_no_repeat");
}

ATF_TEST_CASE(w_tag_repeat);
ATF_TEST_CASE_HEAD(w_tag_repeat)
{
    set_md_var("descr", "Checks formatting with a tag and repeating it on "
        "each line");
}
ATF_TEST_CASE_BODY(w_tag_repeat)
{
    run_tests("w_tag_repeat");
}

ATF_TEST_CASE(w_tag_col);
ATF_TEST_CASE_HEAD(w_tag_col)
{
    set_md_var("descr", "Checks formatting with a tag and starting at a "
        "column greater than its length");
}
ATF_TEST_CASE_BODY(w_tag_col)
{
    run_tests("w_tag_col");
}

ATF_TEST_CASE(paragraphs);
ATF_TEST_CASE_HEAD(paragraphs)
{
    set_md_var("descr", "Checks formatting a string that contains multiple "
        "paragraphs");
}
ATF_TEST_CASE_BODY(paragraphs)
{
    run_tests("paragraphs");
}

// ------------------------------------------------------------------------
// Main.
// ------------------------------------------------------------------------

ATF_INIT_TEST_CASES(tcs)
{
    // Add the test cases for the free functions.
    ATF_ADD_TEST_CASE(tcs, wo_tag);
    ATF_ADD_TEST_CASE(tcs, wo_tag_col);
    ATF_ADD_TEST_CASE(tcs, w_tag_no_repeat);
    ATF_ADD_TEST_CASE(tcs, w_tag_repeat);
    ATF_ADD_TEST_CASE(tcs, w_tag_col);
    ATF_ADD_TEST_CASE(tcs, paragraphs);
}
