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

#include <fstream>
#include <iostream>

#include "atf-c++/macros.hpp"

#include "atf-c++/detail/parser.hpp"
#include "atf-c++/detail/test_helpers.hpp"
#include "atf-c++/detail/text.hpp"

#include "test-program.hpp"

namespace impl = atf::atf_run;
namespace detail = atf::atf_run::detail;

using atf::tests::vars_map;

// -------------------------------------------------------------------------
// Auxiliary functions.
// -------------------------------------------------------------------------

static
atf::fs::path
get_helper(const atf::tests::tc& tc, const char* name)
{
    return atf::fs::path(tc.get_config_var("srcdir")) / name;
}

static
void
check_property(const vars_map& props, const char* name, const char* value)
{
    const vars_map::const_iterator iter = props.find(name);
    ATF_REQUIRE(iter != props.end());
    ATF_REQUIRE_EQ(value, (*iter).second);
}

static void
check_result(const char* exp_state, const int exp_value, const char* exp_reason,
             const impl::test_case_result& tcr)
{
    ATF_REQUIRE_EQ(exp_state, tcr.state());
    ATF_REQUIRE_EQ(exp_value, tcr.value());
    ATF_REQUIRE_EQ(exp_reason, tcr.reason());
}

static
void
write_test_case_result(const char *results_path, const std::string& contents)
{
    std::ofstream results_file(results_path);
    ATF_REQUIRE(results_file);

    results_file << contents;
}

static
void
print_indented(const std::string& str)
{
    std::vector< std::string > ws = atf::text::split(str, "\n");
    for (std::vector< std::string >::const_iterator iter = ws.begin();
         iter != ws.end(); iter++)
        std::cout << ">>" << *iter << "<<\n";
}

// XXX Should this string handling and verbosity level be part of the
// ATF_REQUIRE_EQ macro?  It may be hard to predict sometimes that a
// string can have newlines in it, and so the error message generated
// at the moment will be bogus if there are some.
static
void
check_match(const atf::tests::tc& tc, const std::string& str,
            const std::string& exp)
{
    if (!atf::text::match(str, exp)) {
        std::cout << "String match check failed.\n"
                  << "Adding >> and << to delimit the string boundaries "
                     "below.\n";
        std::cout << "GOT:\n";
        print_indented(str);
        std::cout << "EXPECTED:\n";
        print_indented(exp);
        tc.fail("Constructed string differs from the expected one");
    }
}

// -------------------------------------------------------------------------
// Tests for the "tp" reader.
// -------------------------------------------------------------------------

class tp_reader : protected detail::atf_tp_reader {
    void
    got_tc(const std::string& ident,
           const std::map< std::string, std::string >& md)
    {
        std::string call = "got_tc(" + ident + ", {";
        for (std::map< std::string, std::string >::const_iterator iter =
             md.begin(); iter != md.end(); iter++) {
            if (iter != md.begin())
                call += ", ";
            call += (*iter).first + '=' + (*iter).second;
        }
        call += "})";
        m_calls.push_back(call);
    }

    void
    got_eof(void)
    {
        m_calls.push_back("got_eof()");
    }

public:
    tp_reader(std::istream& is) :
        detail::atf_tp_reader(is)
    {
    }

    void
    read(void)
    {
        atf_tp_reader::read();
    }

    std::vector< std::string > m_calls;
};

ATF_TEST_CASE_WITHOUT_HEAD(tp_1);
ATF_TEST_CASE_BODY(tp_1)
{
    const char* input =
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "ident: test_case_1\n"
        "\n"
        "ident: test_case_2\n"
        "\n"
        "ident: test_case_3\n"
    ;

    const char* exp_calls[] = {
        "got_tc(test_case_1, {ident=test_case_1})",
        "got_tc(test_case_2, {ident=test_case_2})",
        "got_tc(test_case_3, {ident=test_case_3})",
        "got_eof()",
        NULL
    };

    const char* exp_errors[] = {
        NULL
    };

    do_parser_test< tp_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tp_2);
ATF_TEST_CASE_BODY(tp_2)
{
    const char* input =
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "ident: test_case_1\n"
        "descr: This is the description\n"
        "timeout: 300\n"
        "\n"
        "ident: test_case_2\n"
        "\n"
        "ident: test_case_3\n"
        "X-prop1: A custom property\n"
        "descr: Third test case\n"
    ;

    // NO_CHECK_STYLE_BEGIN
    const char* exp_calls[] = {
        "got_tc(test_case_1, {descr=This is the description, ident=test_case_1, timeout=300})",
        "got_tc(test_case_2, {ident=test_case_2})",
        "got_tc(test_case_3, {X-prop1=A custom property, descr=Third test case, ident=test_case_3})",
        "got_eof()",
        NULL
    };
    // NO_CHECK_STYLE_END

    const char* exp_errors[] = {
        NULL
    };

    do_parser_test< tp_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tp_3);
ATF_TEST_CASE_BODY(tp_3)
{
    const char* input =
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "ident: single_test\n"
        "descr: Some description\n"
        "timeout: 300\n"
        "require.arch: thearch\n"
        "require.config: foo-bar\n"
        "require.files: /a/1 /b/2\n"
        "require.machine: themachine\n"
        "require.progs: /bin/cp mv\n"
        "require.user: root\n"
    ;

    // NO_CHECK_STYLE_BEGIN
    const char* exp_calls[] = {
        "got_tc(single_test, {descr=Some description, ident=single_test, require.arch=thearch, require.config=foo-bar, require.files=/a/1 /b/2, require.machine=themachine, require.progs=/bin/cp mv, require.user=root, timeout=300})",
        "got_eof()",
        NULL
    };
    // NO_CHECK_STYLE_END

    const char* exp_errors[] = {
        NULL
    };

    do_parser_test< tp_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tp_4);
ATF_TEST_CASE_BODY(tp_4)
{
    const char* input =
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "ident:   single_test    \n"
        "descr:      Some description	\n"
    ;

    const char* exp_calls[] = {
        "got_tc(single_test, {descr=Some description, ident=single_test})",
        "got_eof()",
        NULL
    };

    const char* exp_errors[] = {
        NULL
    };

    do_parser_test< tp_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tp_50);
ATF_TEST_CASE_BODY(tp_50)
{
    const char* input =
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "3: Unexpected token `<<EOF>>'; expected property name",
        NULL
    };

    do_parser_test< tp_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tp_51);
ATF_TEST_CASE_BODY(tp_51)
{
    const char* input =
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "\n"
        "\n"
        "\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "3: Unexpected token `<<NEWLINE>>'; expected property name",
        NULL
    };

    do_parser_test< tp_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tp_52);
ATF_TEST_CASE_BODY(tp_52)
{
    const char* input =
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "ident: test1\n"
        "ident: test2\n"
    ;

    const char* exp_calls[] = {
        "got_tc(test1, {ident=test1})",
        "got_eof()",
        NULL
    };

    const char* exp_errors[] = {
        NULL
    };

    do_parser_test< tp_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tp_53);
ATF_TEST_CASE_BODY(tp_53)
{
    const char* input =
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "descr: Out of order\n"
        "ident: test1\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "3: First property of a test case must be 'ident'",
        NULL
    };

    do_parser_test< tp_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tp_54);
ATF_TEST_CASE_BODY(tp_54)
{
    const char* input =
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "ident:\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "3: The value for 'ident' cannot be empty",
        NULL
    };

    do_parser_test< tp_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tp_55);
ATF_TEST_CASE_BODY(tp_55)
{
    const char* input =
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "ident: +*,\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "3: The identifier must match ^[_A-Za-z0-9]+$; was '+*,'",
        NULL
    };

    do_parser_test< tp_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tp_56);
ATF_TEST_CASE_BODY(tp_56)
{
    const char* input =
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "ident: test\n"
        "timeout: hello\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "4: The timeout property requires an integer value",
        NULL
    };

    do_parser_test< tp_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tp_57);
ATF_TEST_CASE_BODY(tp_57)
{
    const char* input =
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "ident: test\n"
        "unknown: property\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "4: Unknown property 'unknown'",
        NULL
    };

    do_parser_test< tp_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tp_58);
ATF_TEST_CASE_BODY(tp_58)
{
    const char* input =
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "ident: test\n"
        "X-foo:\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "4: The value for 'X-foo' cannot be empty",
        NULL
    };

    do_parser_test< tp_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tp_59);
ATF_TEST_CASE_BODY(tp_59)
{
    const char* input =
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "\n"
        "ident: test\n"
        "timeout: 300\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "3: Unexpected token `<<NEWLINE>>'; expected property name",
        NULL
    };

    do_parser_test< tp_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(tp_60);
ATF_TEST_CASE_BODY(tp_60)
{
    const char* input =
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "ident: test\n"
        "require.memory: 12345D\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "4: The require.memory property requires an integer value representing"
        " an amount of bytes",
        NULL
    };

    do_parser_test< tp_reader >(input, exp_calls, exp_errors);
}

// -------------------------------------------------------------------------
// Tests for the "tps" writer.
// -------------------------------------------------------------------------

ATF_TEST_CASE(atf_tps_writer);
ATF_TEST_CASE_HEAD(atf_tps_writer)
{
    set_md_var("descr", "Verifies the application/X-atf-tps writer");
}
ATF_TEST_CASE_BODY(atf_tps_writer)
{
    std::ostringstream expss;
    std::ostringstream ss;
    const char *ts_regex = "[0-9]+\\.[0-9]{1,6}, ";

#define RESET \
    expss.str(""); \
    ss.str("")

#define CHECK \
    check_match(*this, ss.str(), expss.str())

    {
        RESET;

        impl::atf_tps_writer w(ss);
        expss << "Content-Type: application/X-atf-tps; version=\"3\"\n\n";
        CHECK;
    }

    {
        RESET;

        impl::atf_tps_writer w(ss);
        expss << "Content-Type: application/X-atf-tps; version=\"3\"\n\n";
        CHECK;

        w.info("foo", "bar");
        expss << "info: foo, bar\n";
        CHECK;

        w.info("baz", "second info");
        expss << "info: baz, second info\n";
        CHECK;
    }

    {
        RESET;

        impl::atf_tps_writer w(ss);
        expss << "Content-Type: application/X-atf-tps; version=\"3\"\n\n";
        CHECK;

        w.ntps(0);
        expss << "tps-count: 0\n";
        CHECK;
    }

    {
        RESET;

        impl::atf_tps_writer w(ss);
        expss << "Content-Type: application/X-atf-tps; version=\"3\"\n\n";
        CHECK;

        w.ntps(123);
        expss << "tps-count: 123\n";
        CHECK;
    }

    {
        RESET;

        impl::atf_tps_writer w(ss);
        expss << "Content-Type: application/X-atf-tps; version=\"3\"\n\n";
        CHECK;

        w.ntps(2);
        expss << "tps-count: 2\n";
        CHECK;

        w.start_tp("foo", 0);
        expss << "tp-start: " << ts_regex << "foo, 0\n";
        CHECK;

        w.end_tp("");
        expss << "tp-end: " << ts_regex << "foo\n";
        CHECK;

        w.start_tp("bar", 0);
        expss << "tp-start: " << ts_regex << "bar, 0\n";
        CHECK;

        w.end_tp("failed program");
        expss << "tp-end: " << ts_regex << "bar, failed program\n";
        CHECK;
    }

    {
        RESET;

        impl::atf_tps_writer w(ss);
        expss << "Content-Type: application/X-atf-tps; version=\"3\"\n\n";
        CHECK;

        w.ntps(1);
        expss << "tps-count: 1\n";
        CHECK;

        w.start_tp("foo", 1);
        expss << "tp-start: " << ts_regex << "foo, 1\n";
        CHECK;

        w.start_tc("brokentc");
        expss << "tc-start: " << ts_regex << "brokentc\n";
        CHECK;

        w.end_tp("aborted");
        expss << "tp-end: " << ts_regex << "foo, aborted\n";
        CHECK;
    }

    {
        RESET;

        impl::atf_tps_writer w(ss);
        expss << "Content-Type: application/X-atf-tps; version=\"3\"\n\n";
        CHECK;

        w.ntps(1);
        expss << "tps-count: 1\n";
        CHECK;

        w.start_tp("thetp", 3);
        expss << "tp-start: " << ts_regex << "thetp, 3\n";
        CHECK;

        w.start_tc("passtc");
        expss << "tc-start: " << ts_regex << "passtc\n";
        CHECK;

        w.end_tc("passed", "");
        expss << "tc-end: " << ts_regex << "passtc, passed\n";
        CHECK;

        w.start_tc("failtc");
        expss << "tc-start: " << ts_regex << "failtc\n";
        CHECK;

        w.end_tc("failed", "The reason");
        expss << "tc-end: " << ts_regex << "failtc, failed, The reason\n";
        CHECK;

        w.start_tc("skiptc");
        expss << "tc-start: " << ts_regex << "skiptc\n";
        CHECK;

        w.end_tc("skipped", "The reason");
        expss << "tc-end: " << ts_regex << "skiptc, skipped, The reason\n";
        CHECK;

        w.end_tp("");
        expss << "tp-end: " << ts_regex << "thetp\n";
        CHECK;
    }

    {
        RESET;

        impl::atf_tps_writer w(ss);
        expss << "Content-Type: application/X-atf-tps; version=\"3\"\n\n";
        CHECK;

        w.ntps(1);
        expss << "tps-count: 1\n";
        CHECK;

        w.start_tp("thetp", 1);
        expss << "tp-start: " << ts_regex << "thetp, 1\n";
        CHECK;

        w.start_tc("thetc");
        expss << "tc-start: " << ts_regex << "thetc\n";
        CHECK;

        w.stdout_tc("a line");
        expss << "tc-so:a line\n";
        CHECK;

        w.stdout_tc("another line");
        expss << "tc-so:another line\n";
        CHECK;

        w.stderr_tc("an error message");
        expss << "tc-se:an error message\n";
        CHECK;

        w.end_tc("passed", "");
        expss << "tc-end: " << ts_regex << "thetc, passed\n";
        CHECK;

        w.end_tp("");
        expss << "tp-end: " << ts_regex << "thetp\n";
        CHECK;
    }

    {
        RESET;

        impl::atf_tps_writer w(ss);
        expss << "Content-Type: application/X-atf-tps; version=\"3\"\n\n";
        CHECK;

        w.ntps(1);
        expss << "tps-count: 1\n";
        CHECK;

        w.start_tp("thetp", 0);
        expss << "tp-start: " << ts_regex << "thetp, 0\n";
        CHECK;

        w.end_tp("");
        expss << "tp-end: " << ts_regex << "thetp\n";
        CHECK;

        w.info("foo", "bar");
        expss << "info: foo, bar\n";
        CHECK;

        w.info("baz", "second value");
        expss << "info: baz, second value\n";
        CHECK;
    }

#undef CHECK
#undef RESET
}

// -------------------------------------------------------------------------
// Tests for the free functions.
// -------------------------------------------------------------------------

ATF_TEST_CASE(get_metadata_bad);
ATF_TEST_CASE_HEAD(get_metadata_bad) {}
ATF_TEST_CASE_BODY(get_metadata_bad) {
    const atf::fs::path executable = get_helper(*this, "bad_metadata_helper");
    ATF_REQUIRE_THROW(atf::parser::parse_errors,
                    impl::get_metadata(executable, vars_map()));
}

ATF_TEST_CASE(get_metadata_zero_tcs);
ATF_TEST_CASE_HEAD(get_metadata_zero_tcs) {}
ATF_TEST_CASE_BODY(get_metadata_zero_tcs) {
    const atf::fs::path executable = get_helper(*this, "zero_tcs_helper");
    ATF_REQUIRE_THROW(atf::parser::parse_errors,
                    impl::get_metadata(executable, vars_map()));
}

ATF_TEST_CASE(get_metadata_several_tcs);
ATF_TEST_CASE_HEAD(get_metadata_several_tcs) {}
ATF_TEST_CASE_BODY(get_metadata_several_tcs) {
    const atf::fs::path executable = get_helper(*this, "several_tcs_helper");
    const impl::metadata md = impl::get_metadata(executable, vars_map());
    ATF_REQUIRE_EQ(3, md.test_cases.size());

    {
        const impl::test_cases_map::const_iterator iter =
            md.test_cases.find("first");
        ATF_REQUIRE(iter != md.test_cases.end());

        ATF_REQUIRE_EQ(4, (*iter).second.size());
        check_property((*iter).second, "descr", "Description 1");
        check_property((*iter).second, "has.cleanup", "false");
        check_property((*iter).second, "ident", "first");
        check_property((*iter).second, "timeout", "300");
    }

    {
        const impl::test_cases_map::const_iterator iter =
            md.test_cases.find("second");
        ATF_REQUIRE(iter != md.test_cases.end());

        ATF_REQUIRE_EQ(5, (*iter).second.size());
        check_property((*iter).second, "descr", "Description 2");
        check_property((*iter).second, "has.cleanup", "true");
        check_property((*iter).second, "ident", "second");
        check_property((*iter).second, "timeout", "500");
        check_property((*iter).second, "X-property", "Custom property");
    }

    {
        const impl::test_cases_map::const_iterator iter =
            md.test_cases.find("third");
        ATF_REQUIRE(iter != md.test_cases.end());

        ATF_REQUIRE_EQ(3, (*iter).second.size());
        check_property((*iter).second, "has.cleanup", "false");
        check_property((*iter).second, "ident", "third");
        check_property((*iter).second, "timeout", "300");
    }
}

ATF_TEST_CASE_WITHOUT_HEAD(parse_test_case_result_expected_death);
ATF_TEST_CASE_BODY(parse_test_case_result_expected_death) {
    check_result("expected_death", -1, "foo bar",
                 detail::parse_test_case_result("expected_death: foo bar"));

    ATF_REQUIRE_THROW(std::runtime_error,
                    detail::parse_test_case_result("expected_death"));
    ATF_REQUIRE_THROW(std::runtime_error,
                    detail::parse_test_case_result("expected_death(3): foo"));
}

ATF_TEST_CASE_WITHOUT_HEAD(parse_test_case_result_expected_exit);
ATF_TEST_CASE_BODY(parse_test_case_result_expected_exit) {
    check_result("expected_exit", -1, "foo bar",
                 detail::parse_test_case_result("expected_exit: foo bar"));
    check_result("expected_exit", -1, "foo bar",
                 detail::parse_test_case_result("expected_exit(): foo bar"));
    check_result("expected_exit", 5, "foo bar",
                 detail::parse_test_case_result("expected_exit(5): foo bar"));

    ATF_REQUIRE_THROW(std::runtime_error,
                    detail::parse_test_case_result("expected_exit"));
    ATF_REQUIRE_THROW(std::runtime_error,
                    detail::parse_test_case_result("expected_exit("));
}

ATF_TEST_CASE_WITHOUT_HEAD(parse_test_case_result_expected_failure);
ATF_TEST_CASE_BODY(parse_test_case_result_expected_failure) {
    check_result("expected_failure", -1, "foo bar",
                 detail::parse_test_case_result("expected_failure: foo bar"));

    ATF_REQUIRE_THROW(std::runtime_error,
                    detail::parse_test_case_result("expected_failure"));
    ATF_REQUIRE_THROW(std::runtime_error,
                    detail::parse_test_case_result("expected_failure(3): foo"));
}

ATF_TEST_CASE_WITHOUT_HEAD(parse_test_case_result_expected_signal);
ATF_TEST_CASE_BODY(parse_test_case_result_expected_signal) {
    check_result("expected_signal", -1, "foo bar",
                 detail::parse_test_case_result("expected_signal: foo bar"));
    check_result("expected_signal", -1, "foo bar",
                 detail::parse_test_case_result("expected_signal(): foo bar"));
    check_result("expected_signal", 5, "foo bar",
                 detail::parse_test_case_result("expected_signal(5): foo bar"));

    ATF_REQUIRE_THROW(std::runtime_error,
                    detail::parse_test_case_result("expected_signal"));
    ATF_REQUIRE_THROW(std::runtime_error,
                    detail::parse_test_case_result("expected_signal("));
}

ATF_TEST_CASE_WITHOUT_HEAD(parse_test_case_result_expected_timeout);
ATF_TEST_CASE_BODY(parse_test_case_result_expected_timeout) {
    check_result("expected_timeout", -1, "foo bar",
                 detail::parse_test_case_result("expected_timeout: foo bar"));

    ATF_REQUIRE_THROW(std::runtime_error,
                    detail::parse_test_case_result("expected_timeout"));
    ATF_REQUIRE_THROW(std::runtime_error,
                    detail::parse_test_case_result("expected_timeout(3): foo"));
}

ATF_TEST_CASE_WITHOUT_HEAD(parse_test_case_result_failed);
ATF_TEST_CASE_BODY(parse_test_case_result_failed) {
    check_result("failed", -1, "foo bar",
                 detail::parse_test_case_result("failed: foo bar"));

    ATF_REQUIRE_THROW(std::runtime_error,
                    detail::parse_test_case_result("failed"));
    ATF_REQUIRE_THROW(std::runtime_error,
                    detail::parse_test_case_result("failed(3): foo"));
}

ATF_TEST_CASE_WITHOUT_HEAD(parse_test_case_result_passed);
ATF_TEST_CASE_BODY(parse_test_case_result_passed) {
    check_result("passed", -1, "",
                 detail::parse_test_case_result("passed"));

    ATF_REQUIRE_THROW(std::runtime_error,
                    detail::parse_test_case_result("passed: foo"));
    ATF_REQUIRE_THROW(std::runtime_error,
                    detail::parse_test_case_result("passed(3): foo"));
}

ATF_TEST_CASE_WITHOUT_HEAD(parse_test_case_result_skipped);
ATF_TEST_CASE_BODY(parse_test_case_result_skipped) {
    check_result("skipped", -1, "foo bar",
                 detail::parse_test_case_result("skipped: foo bar"));

    ATF_REQUIRE_THROW(std::runtime_error,
                    detail::parse_test_case_result("skipped"));
    ATF_REQUIRE_THROW(std::runtime_error,
                    detail::parse_test_case_result("skipped(3): foo"));
}

ATF_TEST_CASE_WITHOUT_HEAD(parse_test_case_result_unknown);
ATF_TEST_CASE_BODY(parse_test_case_result_unknown) {
    ATF_REQUIRE_THROW(std::runtime_error,
                    detail::parse_test_case_result("foo"));
    ATF_REQUIRE_THROW(std::runtime_error,
                    detail::parse_test_case_result("bar: foo"));
    ATF_REQUIRE_THROW(std::runtime_error,
                    detail::parse_test_case_result("baz: foo"));
}

ATF_TEST_CASE_WITHOUT_HEAD(read_test_case_result_failed);
ATF_TEST_CASE_BODY(read_test_case_result_failed) {
    write_test_case_result("resfile", "failed: foo bar\n");
    const impl::test_case_result tcr = impl::read_test_case_result(
        atf::fs::path("resfile"));
    ATF_REQUIRE_EQ("failed", tcr.state());
    ATF_REQUIRE_EQ("foo bar", tcr.reason());
}

ATF_TEST_CASE_WITHOUT_HEAD(read_test_case_result_skipped);
ATF_TEST_CASE_BODY(read_test_case_result_skipped) {
    write_test_case_result("resfile", "skipped: baz bar\n");
    const impl::test_case_result tcr = impl::read_test_case_result(
        atf::fs::path("resfile"));
    ATF_REQUIRE_EQ("skipped", tcr.state());
    ATF_REQUIRE_EQ("baz bar", tcr.reason());
}


ATF_TEST_CASE(read_test_case_result_no_file);
ATF_TEST_CASE_HEAD(read_test_case_result_no_file) {}
ATF_TEST_CASE_BODY(read_test_case_result_no_file) {
    ATF_REQUIRE_THROW(std::runtime_error,
                    impl::read_test_case_result(atf::fs::path("resfile")));
}

ATF_TEST_CASE_WITHOUT_HEAD(read_test_case_result_empty_file);
ATF_TEST_CASE_BODY(read_test_case_result_empty_file) {
    write_test_case_result("resfile", "");
    ATF_REQUIRE_THROW(std::runtime_error,
                    impl::read_test_case_result(atf::fs::path("resfile")));
}

ATF_TEST_CASE_WITHOUT_HEAD(read_test_case_result_invalid);
ATF_TEST_CASE_BODY(read_test_case_result_invalid) {
    write_test_case_result("resfile", "passed: hello\n");
    ATF_REQUIRE_THROW(std::runtime_error,
                    impl::read_test_case_result(atf::fs::path("resfile")));
}

ATF_TEST_CASE_WITHOUT_HEAD(read_test_case_result_multiline);
ATF_TEST_CASE_BODY(read_test_case_result_multiline) {
    write_test_case_result("resfile", "skipped: foo\nbar\n");
    const impl::test_case_result tcr = impl::read_test_case_result(
        atf::fs::path("resfile"));
    ATF_REQUIRE_EQ("skipped", tcr.state());
    ATF_REQUIRE_EQ("foo<<NEWLINE UNEXPECTED>>bar", tcr.reason());
}

// -------------------------------------------------------------------------
// Main.
// -------------------------------------------------------------------------

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, tp_1);
    ATF_ADD_TEST_CASE(tcs, tp_2);
    ATF_ADD_TEST_CASE(tcs, tp_3);
    ATF_ADD_TEST_CASE(tcs, tp_4);
    ATF_ADD_TEST_CASE(tcs, tp_50);
    ATF_ADD_TEST_CASE(tcs, tp_51);
    ATF_ADD_TEST_CASE(tcs, tp_52);
    ATF_ADD_TEST_CASE(tcs, tp_53);
    ATF_ADD_TEST_CASE(tcs, tp_54);
    ATF_ADD_TEST_CASE(tcs, tp_55);
    ATF_ADD_TEST_CASE(tcs, tp_56);
    ATF_ADD_TEST_CASE(tcs, tp_57);
    ATF_ADD_TEST_CASE(tcs, tp_58);
    ATF_ADD_TEST_CASE(tcs, tp_59);
    ATF_ADD_TEST_CASE(tcs, tp_60);

    ATF_ADD_TEST_CASE(tcs, atf_tps_writer);

    ATF_ADD_TEST_CASE(tcs, get_metadata_bad);
    ATF_ADD_TEST_CASE(tcs, get_metadata_zero_tcs);
    ATF_ADD_TEST_CASE(tcs, get_metadata_several_tcs);

    ATF_ADD_TEST_CASE(tcs, parse_test_case_result_expected_death);
    ATF_ADD_TEST_CASE(tcs, parse_test_case_result_expected_exit);
    ATF_ADD_TEST_CASE(tcs, parse_test_case_result_expected_failure);
    ATF_ADD_TEST_CASE(tcs, parse_test_case_result_expected_signal);
    ATF_ADD_TEST_CASE(tcs, parse_test_case_result_expected_timeout);
    ATF_ADD_TEST_CASE(tcs, parse_test_case_result_failed);
    ATF_ADD_TEST_CASE(tcs, parse_test_case_result_passed);
    ATF_ADD_TEST_CASE(tcs, parse_test_case_result_skipped);
    ATF_ADD_TEST_CASE(tcs, parse_test_case_result_unknown);

    ATF_ADD_TEST_CASE(tcs, read_test_case_result_failed);
    ATF_ADD_TEST_CASE(tcs, read_test_case_result_skipped);
    ATF_ADD_TEST_CASE(tcs, read_test_case_result_no_file);
    ATF_ADD_TEST_CASE(tcs, read_test_case_result_empty_file);
    ATF_ADD_TEST_CASE(tcs, read_test_case_result_multiline);
    ATF_ADD_TEST_CASE(tcs, read_test_case_result_invalid);

    // TODO: Add tests for run_test_case once all the missing functionality
    // is implemented.
}
