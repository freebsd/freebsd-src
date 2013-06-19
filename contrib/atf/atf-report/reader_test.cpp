//
// Automated Testing Framework (atf)
//
// Copyright (c) 2010 The NetBSD Foundation, Inc.
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

#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "atf-c++/macros.hpp"

#include "atf-c++/detail/parser.hpp"
#include "atf-c++/detail/sanity.hpp"
#include "atf-c++/detail/test_helpers.hpp"
#include "atf-c++/detail/text.hpp"

#include "reader.hpp"

namespace impl = atf::atf_report;

class tps_reader : protected impl::atf_tps_reader {
    void
    got_info(const std::string& what, const std::string& val)
    {
        m_calls.push_back("got_info(" + what + ", " + val + ")");
    }

    void
    got_ntps(size_t ntps)
    {
        m_calls.push_back("got_ntps(" + atf::text::to_string(ntps) + ")");
    }

    void
    got_tp_start(const std::string& tpname, size_t ntcs)
    {
        m_calls.push_back("got_tp_start(" + tpname + ", " +
                          atf::text::to_string(ntcs) + ")");
    }

    void
    got_tp_end(struct timeval* tv ATF_DEFS_ATTRIBUTE_UNUSED,
               const std::string& reason)
    {
        m_calls.push_back("got_tp_end(" + reason + ")");
    }

    void
    got_tc_start(const std::string& tcname)
    {
        m_calls.push_back("got_tc_start(" + tcname + ")");
    }

    void
    got_tc_end(const std::string& state,
               struct timeval* tv ATF_DEFS_ATTRIBUTE_UNUSED,
               const std::string& reason)
    {
        const std::string r = state + (reason.empty() ? "" : ", " + reason);
        m_calls.push_back("got_tc_end(" + r + ")");
    }

    void
    got_tc_stdout_line(const std::string& line)
    {
        m_calls.push_back("got_tc_stdout_line(" + line + ")");
    }

    void
    got_tc_stderr_line(const std::string& line)
    {
        m_calls.push_back("got_tc_stderr_line(" + line + ")");
    }

    void
    got_eof(void)
    {
        m_calls.push_back("got_eof()");
    }

public:
    tps_reader(std::istream& is) :
        impl::atf_tps_reader(is)
    {
    }

    void
    read(void)
    {
        atf_tps_reader::read();
    }

    std::vector< std::string > m_calls;
};

ATF_TEST_CASE_WITHOUT_HEAD(tps_1);
ATF_TEST_CASE_BODY(tps_1)
{
    const char* input =
        "Content-Type: application/X-atf-tps; version=\"3\"\n"
        "\n"
        "tps-count: 0\n"
    ;

    const char* exp_calls[] = {
        "got_ntps(0)",
        "got_eof()",
        NULL
    };

    const char* exp_errors[] = {
        NULL
    };

    do_parser_test< tps_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tps_2);
ATF_TEST_CASE_BODY(tps_2)
{
    const char* input =
        "Content-Type: application/X-atf-tps; version=\"3\"\n"
        "\n"
        "tps-count: 2\n"
        "tp-start: 123.456, first-prog, 0\n"
        "tp-end: 123.567, first-prog\n"
        "tp-start: 123.678, second-prog, 0\n"
        "tp-end: 123.789, second-prog, This program failed\n"
    ;

    const char* exp_calls[] = {
        "got_ntps(2)",
        "got_tp_start(first-prog, 0)",
        "got_tp_end()",
        "got_tp_start(second-prog, 0)",
        "got_tp_end(This program failed)",
        "got_eof()",
        NULL
    };

    const char* exp_errors[] = {
        NULL
    };

    do_parser_test< tps_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tps_3);
ATF_TEST_CASE_BODY(tps_3)
{
    const char* input =
        "Content-Type: application/X-atf-tps; version=\"3\"\n"
        "\n"
        "tps-count: 2\n"
        "tp-start: 123.123, first-prog, 3\n"
        "tc-start: 123.234, first-test\n"
        "tc-end: 123.345, first-test, passed\n"
        "tc-start: 123.456, second-test\n"
        "tc-end: 123.567, second-test, skipped, Testing skipped reason\n"
        "tc-start: 123.678, third.test\n"
        "tc-end: 123.789, third.test, failed, Testing failed reason\n"
        "tp-end: 123.890, first-prog\n"
        "tp-start: 124.901, second-prog, 3\n"
        "tc-start: 124.1012, first-test\n"
        "tc-so:first stdout line for 1st test\n"
        "tc-se:first stderr line for 1st test\n"
        "tc-so:second stdout line for 1st test\n"
        "tc-se:second stderr line for 1st test\n"
        "tc-end: 124.1123, first-test, passed\n"
        "tc-start: 124.1234, second-test\n"
        "tc-so:first stdout line for 2nd test\n"
        "tc-se:first stderr line for 2nd test\n"
        "tc-so:second stdout line for 2nd test\n"
        "tc-se:second stderr line for 2nd test\n"
        "tc-end: 124.1345, second-test, skipped, Testing skipped reason\n"
        "tc-start: 124.1456, third.test\n"
        "tc-so:first stdout line for 3rd test\n"
        "tc-se:first stderr line for 3rd test\n"
        "tc-so:second stdout line for 3rd test\n"
        "tc-se:second stderr line for 3rd test\n"
        "tc-end: 124.1567, third.test, failed, Testing failed reason\n"
        "tp-end: 124.1678, second-prog, This program failed\n"
    ;

    const char* exp_calls[] = {
        "got_ntps(2)",
        "got_tp_start(first-prog, 3)",
        "got_tc_start(first-test)",
        "got_tc_end(passed)",
        "got_tc_start(second-test)",
        "got_tc_end(skipped, Testing skipped reason)",
        "got_tc_start(third.test)",
        "got_tc_end(failed, Testing failed reason)",
        "got_tp_end()",
        "got_tp_start(second-prog, 3)",
        "got_tc_start(first-test)",
        "got_tc_stdout_line(first stdout line for 1st test)",
        "got_tc_stderr_line(first stderr line for 1st test)",
        "got_tc_stdout_line(second stdout line for 1st test)",
        "got_tc_stderr_line(second stderr line for 1st test)",
        "got_tc_end(passed)",
        "got_tc_start(second-test)",
        "got_tc_stdout_line(first stdout line for 2nd test)",
        "got_tc_stderr_line(first stderr line for 2nd test)",
        "got_tc_stdout_line(second stdout line for 2nd test)",
        "got_tc_stderr_line(second stderr line for 2nd test)",
        "got_tc_end(skipped, Testing skipped reason)",
        "got_tc_start(third.test)",
        "got_tc_stdout_line(first stdout line for 3rd test)",
        "got_tc_stderr_line(first stderr line for 3rd test)",
        "got_tc_stdout_line(second stdout line for 3rd test)",
        "got_tc_stderr_line(second stderr line for 3rd test)",
        "got_tc_end(failed, Testing failed reason)",
        "got_tp_end(This program failed)",
        "got_eof()",
        NULL
    };

    const char* exp_errors[] = {
        NULL
    };

    do_parser_test< tps_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tps_4);
ATF_TEST_CASE_BODY(tps_4)
{
    const char* input =
        "Content-Type: application/X-atf-tps; version=\"3\"\n"
        "\n"
        "info: a, foo\n"
        "info: b, bar\n"
        "info: c, baz\n"
        "tps-count: 2\n"
        "tp-start: 234.1, first-prog, 3\n"
        "tc-start: 234.12, first-test\n"
        "tc-end: 234.23, first-test, passed\n"
        "tc-start: 234.34, second-test\n"
        "tc-end: 234.45, second-test, skipped, Testing skipped reason\n"
        "tc-start: 234.56, third-test\n"
        "tc-end: 234.67, third-test, failed, Testing failed reason\n"
        "tp-end: 234.78, first-prog\n"
        "tp-start: 234.89, second-prog, 3\n"
        "tc-start: 234.90, first-test\n"
        "tc-so:first stdout line for 1st test\n"
        "tc-se:first stderr line for 1st test\n"
        "tc-so:second stdout line for 1st test\n"
        "tc-se:second stderr line for 1st test\n"
        "tc-end: 234.101, first-test, passed\n"
        "tc-start: 234.112, second-test\n"
        "tc-so:first stdout line for 2nd test\n"
        "tc-se:first stderr line for 2nd test\n"
        "tc-so:second stdout line for 2nd test\n"
        "tc-se:second stderr line for 2nd test\n"
        "tc-end: 234.123, second-test, skipped, Testing skipped reason\n"
        "tc-start: 234.134, third-test\n"
        "tc-so:first stdout line for 3rd test\n"
        "tc-se:first stderr line for 3rd test\n"
        "tc-so:second stdout line for 3rd test\n"
        "tc-se:second stderr line for 3rd test\n"
        "tc-end: 234.145, third-test, failed, Testing failed reason\n"
        "tp-end: 234.156, second-prog, This program failed\n"
        "info: d, foo\n"
        "info: e, bar\n"
        "info: f, baz\n"
    ;

    const char* exp_calls[] = {
        "got_info(a, foo)",
        "got_info(b, bar)",
        "got_info(c, baz)",
        "got_ntps(2)",
        "got_tp_start(first-prog, 3)",
        "got_tc_start(first-test)",
        "got_tc_end(passed)",
        "got_tc_start(second-test)",
        "got_tc_end(skipped, Testing skipped reason)",
        "got_tc_start(third-test)",
        "got_tc_end(failed, Testing failed reason)",
        "got_tp_end()",
        "got_tp_start(second-prog, 3)",
        "got_tc_start(first-test)",
        "got_tc_stdout_line(first stdout line for 1st test)",
        "got_tc_stderr_line(first stderr line for 1st test)",
        "got_tc_stdout_line(second stdout line for 1st test)",
        "got_tc_stderr_line(second stderr line for 1st test)",
        "got_tc_end(passed)",
        "got_tc_start(second-test)",
        "got_tc_stdout_line(first stdout line for 2nd test)",
        "got_tc_stderr_line(first stderr line for 2nd test)",
        "got_tc_stdout_line(second stdout line for 2nd test)",
        "got_tc_stderr_line(second stderr line for 2nd test)",
        "got_tc_end(skipped, Testing skipped reason)",
        "got_tc_start(third-test)",
        "got_tc_stdout_line(first stdout line for 3rd test)",
        "got_tc_stderr_line(first stderr line for 3rd test)",
        "got_tc_stdout_line(second stdout line for 3rd test)",
        "got_tc_stderr_line(second stderr line for 3rd test)",
        "got_tc_end(failed, Testing failed reason)",
        "got_tp_end(This program failed)",
        "got_info(d, foo)",
        "got_info(e, bar)",
        "got_info(f, baz)",
        "got_eof()",
        NULL
    };

    const char* exp_errors[] = {
        NULL
    };

    do_parser_test< tps_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tps_5);
ATF_TEST_CASE_BODY(tps_5)
{
    const char* input =
        "Content-Type: application/X-atf-tps; version=\"3\"\n"
        "\n"
        "tps-count: 1\n"
        "tp-start: 345.123, the-prog, 1\n"
        "tc-start: 345.134, the-test\n"
        "tc-so:--- a	2007-11-04 14:00:41.000000000 +0100\n"
        "tc-so:+++ b	2007-11-04 14:00:48.000000000 +0100\n"
        "tc-so:@@ -1,7 +1,7 @@\n"
        "tc-so: This test is meant to simulate a diff.\n"
        "tc-so: Blank space at beginning of context lines must be preserved.\n"
        "tc-so: \n"
        "tc-so:-First original line.\n"
        "tc-so:-Second original line.\n"
        "tc-so:+First modified line.\n"
        "tc-so:+Second modified line.\n"
        "tc-so: \n"
        "tc-so: EOF\n"
        "tc-end: 345.145, the-test, passed\n"
        "tp-end: 345.156, the-prog\n"
    ;

    // NO_CHECK_STYLE_BEGIN
    const char* exp_calls[] = {
        "got_ntps(1)",
        "got_tp_start(the-prog, 1)",
        "got_tc_start(the-test)",
        "got_tc_stdout_line(--- a	2007-11-04 14:00:41.000000000 +0100)",
        "got_tc_stdout_line(+++ b	2007-11-04 14:00:48.000000000 +0100)",
        "got_tc_stdout_line(@@ -1,7 +1,7 @@)",
        "got_tc_stdout_line( This test is meant to simulate a diff.)",
        "got_tc_stdout_line( Blank space at beginning of context lines must be preserved.)",
        "got_tc_stdout_line( )",
        "got_tc_stdout_line(-First original line.)",
        "got_tc_stdout_line(-Second original line.)",
        "got_tc_stdout_line(+First modified line.)",
        "got_tc_stdout_line(+Second modified line.)",
        "got_tc_stdout_line( )",
        "got_tc_stdout_line( EOF)",
        "got_tc_end(passed)",
        "got_tp_end()",
        "got_eof()",
        NULL
    };
    // NO_CHECK_STYLE_END

    const char* exp_errors[] = {
        NULL
    };

    do_parser_test< tps_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tps_6);
ATF_TEST_CASE_BODY(tps_6)
{
    const char* input =
        "Content-Type: application/X-atf-tps; version=\"3\"\n"
        "\n"
        "tps-count: 1\n"
        "tp-start: 321.1, the-prog, 8\n"
        "tc-start: 321.12, one\n"
        "tc-end: 321.23, one, expected_death, The reason\n"
        "tc-start: 321.34, two\n"
        "tc-end: 321.45, two, expected_exit, This would be an exit\n"
        "tc-start: 321.56, three\n"
        "tc-end: 321.67, three, expected_failure, And this a failure\n"
        "tc-start: 321.78, four\n"
        "tc-end: 321.89, four, expected_signal, And this a signal\n"
        "tc-start: 321.90, five\n"
        "tc-end: 321.101, five, failed, Another reason\n"
        "tc-start: 321.112, six\n"
        "tc-end: 321.123, six, passed\n"
        "tc-start: 321.134, seven\n"
        "tc-end: 321.145, seven, skipped, Skipping it\n"
        "tc-start: 321.156, eight\n"
        "tc-end: 321.167, eight, expected_timeout, Some hang reason\n"
        "tp-end: 321.178, the-prog\n"
    ;

    // NO_CHECK_STYLE_BEGIN
    const char* exp_calls[] = {
        "got_ntps(1)",
        "got_tp_start(the-prog, 8)",
        "got_tc_start(one)",
        "got_tc_end(expected_death, The reason)",
        "got_tc_start(two)",
        "got_tc_end(expected_exit, This would be an exit)",
        "got_tc_start(three)",
        "got_tc_end(expected_failure, And this a failure)",
        "got_tc_start(four)",
        "got_tc_end(expected_signal, And this a signal)",
        "got_tc_start(five)",
        "got_tc_end(failed, Another reason)",
        "got_tc_start(six)",
        "got_tc_end(passed)",
        "got_tc_start(seven)",
        "got_tc_end(skipped, Skipping it)",
        "got_tc_start(eight)",
        "got_tc_end(expected_timeout, Some hang reason)",
        "got_tp_end()",
        "got_eof()",
        NULL
    };
    // NO_CHECK_STYLE_END

    const char* exp_errors[] = {
        NULL
    };

    do_parser_test< tps_reader >(input, exp_calls, exp_errors);
}


ATF_TEST_CASE_WITHOUT_HEAD(tps_50);
ATF_TEST_CASE_BODY(tps_50)
{
    const char* input =
        "Content-Type: application/X-atf-tps; version=\"3\"\n"
        "\n"
        "foo\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "3: Unexpected token `foo'; expected tps-count or info field",
        NULL
    };

    do_parser_test< tps_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tps_51);
ATF_TEST_CASE_BODY(tps_51)
{
    const char* input =
        "Content-Type: application/X-atf-tps; version=\"3\"\n"
        "\n"
        "tps-count\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "3: Unexpected token `<<NEWLINE>>'; expected `:'",
        NULL
    };

    do_parser_test< tps_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tps_52);
ATF_TEST_CASE_BODY(tps_52)
{
    const char* input =
        "Content-Type: application/X-atf-tps; version=\"3\"\n"
        "\n"
        "tps-count:\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "3: Unexpected token `<<NEWLINE>>'; expected number of test programs",
        NULL
    };

    do_parser_test< tps_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tps_53);
ATF_TEST_CASE_BODY(tps_53)
{
    const char* input =
        "Content-Type: application/X-atf-tps; version=\"3\"\n"
        "\n"
        "tps-count: 1\n"
        "foo\n"
    ;

    const char* exp_calls[] = {
        "got_ntps(1)",
        NULL
    };

    const char* exp_errors[] = {
        "4: Unexpected token `foo'; expected start of test program",
        NULL
    };

    do_parser_test< tps_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tps_54);
ATF_TEST_CASE_BODY(tps_54)
{
    const char* input =
        "Content-Type: application/X-atf-tps; version=\"3\"\n"
        "\n"
        "tps-count: 1\n"
        "foo\n"
        "tp-start\n"
        "tp-start:\n"
        "tp-start: 123\n"
        "tp-start: 123.\n"
        "tp-start: 123.456\n"
        "tp-start: 123.456,\n"
        "tp-start: 123.456, foo\n"
        "tp-start: 123.456, foo,\n"
        "tp-start: 123.456, foo, 0\n"
        "bar\n"
        "tp-start: 456.789, foo, 0\n"
        "tp-end\n"
        "tp-start: 777.777, foo, 0\n"
        "tp-end:\n"
        "tp-start: 777.777, foo, 0\n"
        "tp-end: 777\n"
        "tp-start: 777.777, foo, 0\n"
        "tp-end: 777.\n"
        "tp-start: 777.777, foo, 0\n"
        "tp-end: 777.888\n"
        "tp-start: 777.777, foo, 0\n"
        "tp-end: 777.888, \n"
        "tp-start: 777.777, foo, 0\n"
        "tp-end: 777.888, bar\n"
        "tp-start: 777.777, foo, 0\n"
        "tp-end: 777.888, foo,\n"
    ;

    const char* exp_calls[] = {
        "got_ntps(1)",
        NULL
    };

    const char* exp_errors[] = {
        "4: Unexpected token `foo'; expected start of test program",
        "5: Unexpected token `<<NEWLINE>>'; expected `:'",
        "6: Unexpected token `<<NEWLINE>>'; expected timestamp",
        "7: Malformed timestamp value 123",
        "8: Malformed timestamp value 123.",
        "9: Unexpected token `<<NEWLINE>>'; expected `,'",
        "10: Unexpected token `<<NEWLINE>>'; expected test program name",
        "11: Unexpected token `<<NEWLINE>>'; expected `,'",
        "12: Unexpected token `<<NEWLINE>>'; expected number of test programs",
        "14: Unexpected token `bar'; expected end of test program",
        "16: Unexpected token `<<NEWLINE>>'; expected `:'",
        "18: Unexpected token `<<NEWLINE>>'; expected timestamp",
        "20: Malformed timestamp value 777",
        "22: Malformed timestamp value 777.",
        "24: Unexpected token `<<NEWLINE>>'; expected `,'",

        "26: Unexpected token `<<NEWLINE>>'; expected test program name",
        "28: Test program name used in terminator does not match opening",
        "30: Empty reason for failed test program",
        NULL
    };

    do_parser_test< tps_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tps_55);
ATF_TEST_CASE_BODY(tps_55)
{
    const char* input =
        "Content-Type: application/X-atf-tps; version=\"3\"\n"
        "\n"
        "tps-count: 1\n"
        "tp-start: 100.200, foo, 1\n"
        "foo\n"
        "tc-start\n"
        "tc-start:\n"
        "tc-start: 111\n"
        "tc-start: 111.\n"
        "tc-start: 111.222\n"
        "tc-start: 111.222,\n"
        "tc-start: 111.222, foo\n"
        "bar\n"
        "tc-start: 111.333, foo\n"
        "tc-end\n"
        "tc-start: 111.444, foo\n"
        "tc-end:\n"
        "tc-start: 111.444, foo\n"
        "tc-end: 111\n"
        "tc-start: 111.444, foo\n"
        "tc-end: 111.\n"
        "tc-start: 111.444, foo\n"
        "tc-end: 111.555\n"
        "tc-start: 111.444, foo\n"
        "tc-end: 111.555, \n"
        "tc-start: 111.444, foo\n"
        "tc-end: 111.555, bar\n"
        "tc-start: 111.444, foo\n"
        "tc-end: 111.555, foo\n"
        "tc-start: 111.444, foo\n"
        "tc-end: 111.555, foo,\n"
        "tp-end: 111.666, foo\n"
    ;

    const char* exp_calls[] = {
        "got_ntps(1)",
        "got_tp_start(foo, 1)",
        NULL
    };

    // NO_CHECK_STYLE_BEGIN
    const char* exp_errors[] = {
        "5: Unexpected token `foo'; expected start of test case",
        "6: Unexpected token `<<NEWLINE>>'; expected `:'",
        "7: Unexpected token `<<NEWLINE>>'; expected timestamp",
        "8: Malformed timestamp value 111",
        "9: Malformed timestamp value 111.",
        "10: Unexpected token `<<NEWLINE>>'; expected `,'",
        "11: Unexpected token `<<NEWLINE>>'; expected test case name",
        "13: Unexpected token `bar'; expected end of test case or test case's stdout/stderr line",
        "15: Unexpected token `<<NEWLINE>>'; expected `:'",
        "17: Unexpected token `<<NEWLINE>>'; expected timestamp",
        "19: Malformed timestamp value 111",
        "21: Malformed timestamp value 111.",
        "23: Unexpected token `<<NEWLINE>>'; expected `,'",
        "25: Unexpected token `<<NEWLINE>>'; expected test case name",
        "27: Test case name used in terminator does not match opening",
        "29: Unexpected token `<<NEWLINE>>'; expected `,'",
        "31: Unexpected token `<<NEWLINE>>'; expected expected_{death,exit,failure,signal,timeout}, failed, passed or skipped",
        "32: Unexpected token `tp-end'; expected start of test case",
        NULL
    };
    // NO_CHECK_STYLE_END

    do_parser_test< tps_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tps_56);
ATF_TEST_CASE_BODY(tps_56)
{
    const char* input =
        "Content-Type: application/X-atf-tps; version=\"3\"\n"
        "\n"
        "tps-count: 1\n"
        "tp-start: 111.222, foo, 1\n"
        "tc-start: 111.333, foo\n"
        "tc-end: 111.444, foo, passe\n"
        "tc-start: 111.333, foo\n"
        "tc-end: 111.444, foo, passed,\n"
        "tc-start: 111.555, bar\n"
        "tc-end: 111.666, bar, failed\n"
        "tc-start: 111.555, bar\n"
        "tc-end: 111.666, bar, failed,\n"
        "tc-start: 111.555, baz\n"
        "tc-end: 111.666, baz, skipped\n"
        "tc-start: 111.555, baz\n"
        "tc-end: 111.666, baz, skipped,\n"
        "tp-end: 111.777, foo\n"
    ;

    const char* exp_calls[] = {
        "got_ntps(1)",
        "got_tp_start(foo, 1)",
        "got_tc_start(foo)",
        NULL
    };

    // NO_CHECK_STYLE_BEGIN
    const char* exp_errors[] = {
        "6: Unexpected token `passe'; expected expected_{death,exit,failure,signal,timeout}, failed, passed or skipped",
        "8: Unexpected token `,'; expected new line",
        "10: Unexpected token `<<NEWLINE>>'; expected `,'",
        "12: Empty reason for failed test case result",
        "14: Unexpected token `<<NEWLINE>>'; expected `,'",
        "16: Empty reason for skipped test case result",
        "17: Unexpected token `tp-end'; expected start of test case",
        NULL
    };
    // NO_CHECK_STYLE_END

    do_parser_test< tps_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tps_57);
ATF_TEST_CASE_BODY(tps_57)
{
    const char* input =
        "Content-Type: application/X-atf-tps; version=\"3\"\n"
        "\n"
        "tps-count: 2\n"
        "tp-start: 111.222, foo, 0\n"
        "tp-end: 111.333, foo\n"
    ;

    const char* exp_calls[] = {
        "got_ntps(2)",
        "got_tp_start(foo, 0)",
        "got_tp_end()",
        NULL
    };

    const char* exp_errors[] = {
        "6: Unexpected token `<<EOF>>'; expected start of test program",
        NULL
    };

    do_parser_test< tps_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tps_58);
ATF_TEST_CASE_BODY(tps_58)
{
    const char* input =
        "Content-Type: application/X-atf-tps; version=\"3\"\n"
        "\n"
        "tps-count: 1\n"
        "tp-start: 111.222, foo, 0\n"
        "tp-end: 111.333, foo\n"
        "tp-start: 111.444, bar, 0\n"
        "tp-end: 111.555, bar\n"
    ;

    const char* exp_calls[] = {
        "got_ntps(1)",
        "got_tp_start(foo, 0)",
        "got_tp_end()",
        NULL
    };

    const char* exp_errors[] = {
        "6: Unexpected token `tp-start'; expected end of stream or info field",
        NULL
    };

    do_parser_test< tps_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tps_59);
ATF_TEST_CASE_BODY(tps_59)
{
    const char* input =
        "Content-Type: application/X-atf-tps; version=\"3\"\n"
        "\n"
        "info\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "3: Unexpected token `<<NEWLINE>>'; expected `:'",
        NULL
    };

    do_parser_test< tps_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tps_60);
ATF_TEST_CASE_BODY(tps_60)
{
    const char* input =
        "Content-Type: application/X-atf-tps; version=\"3\"\n"
        "\n"
        "info:\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "3: Unexpected token `<<NEWLINE>>'; expected info property name",
        NULL
    };

    do_parser_test< tps_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tps_61);
ATF_TEST_CASE_BODY(tps_61)
{
    const char* input =
        "Content-Type: application/X-atf-tps; version=\"3\"\n"
        "\n"
        "info: a\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "3: Unexpected token `<<NEWLINE>>'; expected `,'",
        NULL
    };

    do_parser_test< tps_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tps_62);
ATF_TEST_CASE_BODY(tps_62)
{
    const char* input =
        "Content-Type: application/X-atf-tps; version=\"3\"\n"
        "\n"
        "info: a,\n"
    ;

    const char* exp_calls[] = {
        "got_info(a, )",
        NULL
    };

    const char* exp_errors[] = {
        "4: Unexpected token `<<EOF>>'; expected tps-count or info field",
        NULL
    };

    do_parser_test< tps_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tps_63);
ATF_TEST_CASE_BODY(tps_63)
{
    const char* input =
        "Content-Type: application/X-atf-tps; version=\"3\"\n"
        "\n"
        "info: a, b\n"
    ;

    const char* exp_calls[] = {
        "got_info(a, b)",
        NULL
    };

    const char* exp_errors[] = {
        "4: Unexpected token `<<EOF>>'; expected tps-count or info field",
        NULL
    };

    do_parser_test< tps_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tps_64);
ATF_TEST_CASE_BODY(tps_64)
{
    const char* input =
        "Content-Type: application/X-atf-tps; version=\"3\"\n"
        "\n"
        "info: a, b\n"
        "info: a.b.c.def, g\n"
        "tps-count\n"
    ;

    const char* exp_calls[] = {
        "got_info(a, b)",
        "got_info(a.b.c.def, g)",
        NULL
    };

    const char* exp_errors[] = {
        "5: Unexpected token `<<NEWLINE>>'; expected `:'",
        NULL
    };

    do_parser_test< tps_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tps_65);
ATF_TEST_CASE_BODY(tps_65)
{
    const char* input =
        "Content-Type: application/X-atf-tps; version=\"3\"\n"
        "\n"
        "info: a, b\n"
        "tps-count:\n"
    ;

    const char* exp_calls[] = {
        "got_info(a, b)",
        NULL
    };

    const char* exp_errors[] = {
        "4: Unexpected token `<<NEWLINE>>'; expected number of test programs",
        NULL
    };

    do_parser_test< tps_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tps_66);
ATF_TEST_CASE_BODY(tps_66)
{
    const char* input =
        "Content-Type: application/X-atf-tps; version=\"3\"\n"
        "\n"
        "info: a, b\n"
        "tps-count: 0\n"
        "info\n"
    ;

    const char* exp_calls[] = {
        "got_info(a, b)",
        "got_ntps(0)",
        NULL
    };

    const char* exp_errors[] = {
        "5: Unexpected token `<<NEWLINE>>'; expected `:'",
        NULL
    };

    do_parser_test< tps_reader >(input, exp_calls, exp_errors);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, tps_1);
    ATF_ADD_TEST_CASE(tcs, tps_2);
    ATF_ADD_TEST_CASE(tcs, tps_3);
    ATF_ADD_TEST_CASE(tcs, tps_4);
    ATF_ADD_TEST_CASE(tcs, tps_5);
    ATF_ADD_TEST_CASE(tcs, tps_6);
    ATF_ADD_TEST_CASE(tcs, tps_50);
    ATF_ADD_TEST_CASE(tcs, tps_51);
    ATF_ADD_TEST_CASE(tcs, tps_52);
    ATF_ADD_TEST_CASE(tcs, tps_53);
    ATF_ADD_TEST_CASE(tcs, tps_54);
    ATF_ADD_TEST_CASE(tcs, tps_55);
    ATF_ADD_TEST_CASE(tcs, tps_56);
    ATF_ADD_TEST_CASE(tcs, tps_57);
    ATF_ADD_TEST_CASE(tcs, tps_58);
    ATF_ADD_TEST_CASE(tcs, tps_59);
    ATF_ADD_TEST_CASE(tcs, tps_60);
    ATF_ADD_TEST_CASE(tcs, tps_61);
    ATF_ADD_TEST_CASE(tcs, tps_62);
    ATF_ADD_TEST_CASE(tcs, tps_63);
    ATF_ADD_TEST_CASE(tcs, tps_64);
    ATF_ADD_TEST_CASE(tcs, tps_65);
    ATF_ADD_TEST_CASE(tcs, tps_66);
}

