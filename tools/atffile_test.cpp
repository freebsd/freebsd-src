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

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
}

#include <algorithm>
#include <fstream>
#include <memory>

#include <atf-c++.hpp>

#include "atffile.hpp"
#include "exceptions.hpp"
#include "test_helpers.hpp"

namespace detail = tools::detail;

// ------------------------------------------------------------------------
// Auxiliary functions.
// ------------------------------------------------------------------------

namespace {

typedef std::map< std::string, std::string > vars_map;

static
std::auto_ptr< std::ofstream >
new_atffile(void)
{
    std::auto_ptr< std::ofstream > os(new std::ofstream("Atffile"));
    ATF_REQUIRE(*os);

    (*os) << "Content-Type: application/X-atf-atffile; version=\"1\"\n\n";
    return os;
}

static
void
touch_exec(const char* name)
{
    std::ofstream os(name);
    ATF_REQUIRE(os);
    os.close();
    ATF_REQUIRE(::chmod(name, S_IRWXU) != -1);
}

static inline
bool
is_in(const std::string& value, const std::vector< std::string >& v)
{
    return std::find(v.begin(), v.end(), value) != v.end();
}

} // anonymous namespace

// ------------------------------------------------------------------------
// Tests cases for the "atffile" parser.
// ------------------------------------------------------------------------

class atffile_reader : protected detail::atf_atffile_reader {
    void
    got_conf(const std::string& name, const std::string& val)
    {
        m_calls.push_back("got_conf(" + name + ", " + val + ")");
    }

    void
    got_prop(const std::string& name, const std::string& val)
    {
        m_calls.push_back("got_prop(" + name + ", " + val + ")");
    }

    void
    got_tp(const std::string& name, bool isglob)
    {
        m_calls.push_back("got_tp(" + name + ", " + (isglob ? "true" : "false")
                  + ")");
    }

    void
    got_eof(void)
    {
        m_calls.push_back("got_eof()");
    }

public:
    atffile_reader(std::istream& is) :
        detail::atf_atffile_reader(is)
    {
    }

    void
    read(void)
    {
        atf_atffile_reader::read();
    }

    std::vector< std::string > m_calls;
};

ATF_TEST_CASE_WITHOUT_HEAD(atffile_1);
ATF_TEST_CASE_BODY(atffile_1)
{
    const char* input =
        "Content-Type: application/X-atf-atffile; version=\"1\"\n"
        "\n"
    ;

    const char* exp_calls[] = {
        "got_eof()",
        NULL
    };

    const char* exp_errors[] = {
        NULL
    };

    do_parser_test< atffile_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(atffile_2);
ATF_TEST_CASE_BODY(atffile_2)
{
    const char* input =
        "Content-Type: application/X-atf-atffile; version=\"1\"\n"
        "\n"
        "# This is a comment on a line of its own.\n"
        "# And this is another one.\n"
        "\n"
        "	    # Another after some whitespace.\n"
        "\n"
        "# The last one after an empty line.\n"
    ;

    const char* exp_calls[] = {
        "got_eof()",
        NULL
    };

    const char* exp_errors[] = {
        NULL
    };

    do_parser_test< atffile_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(atffile_3);
ATF_TEST_CASE_BODY(atffile_3)
{
    const char* input =
        "Content-Type: application/X-atf-atffile; version=\"1\"\n"
        "\n"
        "conf: var1=value1\n"
        "conf: var2 = value2\n"
        "conf: var3	=	value3\n"
        "conf: var4	    =	    value4\n"
        "\n"
        "conf:var5=value5\n"
        "    conf:var6=value6\n"
        "\n"
        "conf: var7 = \"This is a long value.\"\n"
        "conf: var8 = \"Single-word\"\n"
        "conf: var9 = \"    Single-word	\"\n"
        "conf: var10 = Single-word\n"
    ;

    const char* exp_calls[] = {
        "got_conf(var1, value1)",
        "got_conf(var2, value2)",
        "got_conf(var3, value3)",
        "got_conf(var4, value4)",
        "got_conf(var5, value5)",
        "got_conf(var6, value6)",
        "got_conf(var7, This is a long value.)",
        "got_conf(var8, Single-word)",
        "got_conf(var9,     Single-word	)",
        "got_conf(var10, Single-word)",
        "got_eof()",
        NULL
    };

    const char* exp_errors[] = {
        NULL
    };

    do_parser_test< atffile_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(atffile_4);
ATF_TEST_CASE_BODY(atffile_4)
{
    const char* input =
        "Content-Type: application/X-atf-atffile; version=\"1\"\n"
        "\n"
        "prop: var1=value1\n"
        "prop: var2 = value2\n"
        "prop: var3	=	value3\n"
        "prop: var4	    =	    value4\n"
        "\n"
        "prop:var5=value5\n"
        "    prop:var6=value6\n"
        "\n"
        "prop: var7 = \"This is a long value.\"\n"
        "prop: var8 = \"Single-word\"\n"
        "prop: var9 = \"    Single-word	\"\n"
        "prop: var10 = Single-word\n"
    ;

    const char* exp_calls[] = {
        "got_prop(var1, value1)",
        "got_prop(var2, value2)",
        "got_prop(var3, value3)",
        "got_prop(var4, value4)",
        "got_prop(var5, value5)",
        "got_prop(var6, value6)",
        "got_prop(var7, This is a long value.)",
        "got_prop(var8, Single-word)",
        "got_prop(var9,     Single-word	)",
        "got_prop(var10, Single-word)",
        "got_eof()",
        NULL
    };

    const char* exp_errors[] = {
        NULL
    };

    do_parser_test< atffile_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(atffile_5);
ATF_TEST_CASE_BODY(atffile_5)
{
    const char* input =
        "Content-Type: application/X-atf-atffile; version=\"1\"\n"
        "\n"
        "tp:foo\n"
        "tp: foo\n"
        "tp:  foo\n"
        "tp:	foo\n"
        "tp:	    foo\n"
        "tp: \"name with spaces\"\n"
        "tp: \"single-word\"\n"
        "tp: single-word\n"
        "\n"
        "tp-glob:foo*?bar\n"
        "tp-glob: foo*?bar\n"
        "tp-glob:  foo*?bar\n"
        "tp-glob:	foo*?bar\n"
        "tp-glob:	    foo*?bar\n"
        "tp-glob: \"glob * with ? spaces\"\n"
        "tp-glob: \"single-*-word\"\n"
        "tp-glob: single-*-word\n"
    ;

    const char* exp_calls[] = {
        "got_tp(foo, false)",
        "got_tp(foo, false)",
        "got_tp(foo, false)",
        "got_tp(foo, false)",
        "got_tp(foo, false)",
        "got_tp(name with spaces, false)",
        "got_tp(single-word, false)",
        "got_tp(single-word, false)",
        "got_tp(foo*?bar, true)",
        "got_tp(foo*?bar, true)",
        "got_tp(foo*?bar, true)",
        "got_tp(foo*?bar, true)",
        "got_tp(foo*?bar, true)",
        "got_tp(glob * with ? spaces, true)",
        "got_tp(single-*-word, true)",
        "got_tp(single-*-word, true)",
        "got_eof()",
        NULL
    };

    const char* exp_errors[] = {
        NULL
    };

    do_parser_test< atffile_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(atffile_6);
ATF_TEST_CASE_BODY(atffile_6)
{
    const char* input =
        "Content-Type: application/X-atf-atffile; version=\"1\"\n"
        "\n"
        "prop: foo = bar # A comment.\n"
        "conf: foo = bar # A comment.\n"
        "tp: foo # A comment.\n"
        "tp-glob: foo # A comment.\n"
    ;

    const char* exp_calls[] = {
        "got_prop(foo, bar)",
        "got_conf(foo, bar)",
        "got_tp(foo, false)",
        "got_tp(foo, true)",
        "got_eof()",
        NULL
    };

    const char* exp_errors[] = {
        NULL
    };

    do_parser_test< atffile_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(atffile_50);
ATF_TEST_CASE_BODY(atffile_50)
{
    const char* input =
        "Content-Type: application/X-atf-atffile; version=\"1\"\n"
        "\n"
        "foo\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    // NO_CHECK_STYLE_BEGIN
    const char* exp_errors[] = {
        "3: Unexpected token `foo'; expected conf, #, prop, tp, tp-glob, a new line or eof",
        NULL
    };
    // NO_CHECK_STYLE_END

    do_parser_test< atffile_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(atffile_51);
ATF_TEST_CASE_BODY(atffile_51)
{
    const char* input =
        "Content-Type: application/X-atf-atffile; version=\"1\"\n"
        "\n"
        "foo bar\n"
        "baz\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    // NO_CHECK_STYLE_BEGIN
    const char* exp_errors[] = {
        "3: Unexpected token `foo'; expected conf, #, prop, tp, tp-glob, a new line or eof",
        "4: Unexpected token `baz'; expected conf, #, prop, tp, tp-glob, a new line or eof",
        NULL
    };
    // NO_CHECK_STYLE_END

    do_parser_test< atffile_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(atffile_52);
ATF_TEST_CASE_BODY(atffile_52)
{
    const char* input =
        "Content-Type: application/X-atf-atffile; version=\"1\"\n"
        "\n"
        "conf\n"
        "conf:\n"
        "conf: foo =\n"
        "conf: bar = # A comment.\n"
        "\n"
        "prop\n"
        "prop:\n"
        "prop: foo =\n"
        "prop: bar = # A comment.\n"
        "\n"
        "tp\n"
        "tp:\n"
        "tp: # A comment.\n"
        "\n"
        "tp-glob\n"
        "tp-glob:\n"
        "tp-glob: # A comment.\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "3: Unexpected token `<<NEWLINE>>'; expected `:'",
        "4: Unexpected token `<<NEWLINE>>'; expected variable name",
        "5: Unexpected token `<<NEWLINE>>'; expected word or quoted string",
        "6: Unexpected token `#'; expected word or quoted string",
        "8: Unexpected token `<<NEWLINE>>'; expected `:'",
        "9: Unexpected token `<<NEWLINE>>'; expected property name",
        "10: Unexpected token `<<NEWLINE>>'; expected word or quoted string",
        "11: Unexpected token `#'; expected word or quoted string",
        "13: Unexpected token `<<NEWLINE>>'; expected `:'",
        "14: Unexpected token `<<NEWLINE>>'; expected word or quoted string",
        "15: Unexpected token `#'; expected word or quoted string",
        "17: Unexpected token `<<NEWLINE>>'; expected `:'",
        "18: Unexpected token `<<NEWLINE>>'; expected word or quoted string",
        "19: Unexpected token `#'; expected word or quoted string",
        NULL
    };

    do_parser_test< atffile_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(atffile_53);
ATF_TEST_CASE_BODY(atffile_53)
{
    const char* input =
        "Content-Type: application/X-atf-atffile; version=\"1\"\n"
        "\n"
        "prop: foo = \"Correct value\" # With comment.\n"
        "\n"
        "prop: bar = # A comment.\n"
        "\n"
        "prop: baz = \"Last variable\"\n"
        "\n"
        "# End of file.\n"
    ;

    const char* exp_calls[] = {
        "got_prop(foo, Correct value)",
        NULL
    };

    const char* exp_errors[] = {
        "5: Unexpected token `#'; expected word or quoted string",
        NULL
    };

    do_parser_test< atffile_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(atffile_54);
ATF_TEST_CASE_BODY(atffile_54)
{
    const char* input =
        "Content-Type: application/X-atf-atffile; version=\"1\"\n"
        "\n"
        "prop: foo = \"\n"
        "prop: bar = \"text\n"
        "prop: baz = \"te\\\"xt\n"
        "prop: last = \"\\\"\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "3: Missing double quotes before end of line",
        "4: Missing double quotes before end of line",
        "5: Missing double quotes before end of line",
        "6: Missing double quotes before end of line",
        NULL
    };

    do_parser_test< atffile_reader >(input, exp_calls, exp_errors);
}

// ------------------------------------------------------------------------
// Tests cases for the "atffile" class.
// ------------------------------------------------------------------------

ATF_TEST_CASE(atffile_getters);
ATF_TEST_CASE_HEAD(atffile_getters) {}
ATF_TEST_CASE_BODY(atffile_getters) {
    vars_map config_vars;
    config_vars["config-var-1"] = "value 1";

    std::vector< std::string > test_program_names;
    test_program_names.push_back("test-program-1");

    vars_map properties;
    properties["test-suite"] = "a test name";

    const tools::atffile atffile(config_vars, test_program_names, properties);
    ATF_REQUIRE(config_vars == atffile.conf());
    ATF_REQUIRE(test_program_names == atffile.tps());
    ATF_REQUIRE(properties == atffile.props());
}

// ------------------------------------------------------------------------
// Tests cases for the free functions.
// ------------------------------------------------------------------------

ATF_TEST_CASE_WITHOUT_HEAD(read_ok_simple);
ATF_TEST_CASE_BODY(read_ok_simple) {
    std::auto_ptr< std::ofstream > os = new_atffile();
    (*os) << "prop: test-suite = foo\n";
    (*os) << "tp: tp-1\n";
    (*os) << "conf: var1 = value1\n";
    (*os) << "tp: tp-2\n";
    (*os) << "tp: tp-3\n";
    (*os) << "prop: prop1 = propvalue1\n";
    (*os) << "conf: var2 = value2\n";
    (*os).close();

    touch_exec("tp-1");
    touch_exec("tp-2");
    touch_exec("tp-3");

    const tools::atffile atffile = tools::read_atffile(
        tools::fs::path("Atffile"));
    ATF_REQUIRE_EQ(2, atffile.conf().size());
    ATF_REQUIRE_EQ("value1", atffile.conf().find("var1")->second);
    ATF_REQUIRE_EQ("value2", atffile.conf().find("var2")->second);
    ATF_REQUIRE_EQ(3, atffile.tps().size());
    ATF_REQUIRE(is_in("tp-1", atffile.tps()));
    ATF_REQUIRE(is_in("tp-2", atffile.tps()));
    ATF_REQUIRE(is_in("tp-3", atffile.tps()));
    ATF_REQUIRE_EQ(2, atffile.props().size());
    ATF_REQUIRE_EQ("foo", atffile.props().find("test-suite")->second);
    ATF_REQUIRE_EQ("propvalue1", atffile.props().find("prop1")->second);
}

ATF_TEST_CASE_WITHOUT_HEAD(read_ok_some_globs);
ATF_TEST_CASE_BODY(read_ok_some_globs) {
    std::auto_ptr< std::ofstream > os = new_atffile();
    (*os) << "prop: test-suite = foo\n";
    (*os) << "tp: foo\n";
    (*os) << "tp-glob: *K*\n";
    (*os) << "tp: bar\n";
    (*os) << "tp-glob: t_*\n";
    (*os).close();

    touch_exec("foo");
    touch_exec("bar");
    touch_exec("aK");
    touch_exec("KKKKK");
    touch_exec("t_hello");
    touch_exec("zzzt_hello");

    const tools::atffile atffile = tools::read_atffile(
        tools::fs::path("Atffile"));
    ATF_REQUIRE_EQ(5, atffile.tps().size());
    ATF_REQUIRE(is_in("foo", atffile.tps()));
    ATF_REQUIRE(is_in("bar", atffile.tps()));
    ATF_REQUIRE(is_in("aK", atffile.tps()));
    ATF_REQUIRE(is_in("KKKKK", atffile.tps()));
    ATF_REQUIRE(is_in("t_hello", atffile.tps()));
}

ATF_TEST_CASE_WITHOUT_HEAD(read_missing_test_suite);
ATF_TEST_CASE_BODY(read_missing_test_suite) {
    std::auto_ptr< std::ofstream > os = new_atffile();
    (*os).close();

    try {
        (void)tools::read_atffile(tools::fs::path("Atffile"));
        ATF_FAIL("Missing property 'test-suite' did not raise an error");
    } catch (const tools::not_found_error< std::string >& e) {
        ATF_REQUIRE_EQ("test-suite", e.get_value());
    }
}

ATF_TEST_CASE_WITHOUT_HEAD(read_missing_test_program);
ATF_TEST_CASE_BODY(read_missing_test_program) {
    std::auto_ptr< std::ofstream > os = new_atffile();
    (*os) << "tp: foo\n";
    (*os) << "tp: bar\n";
    (*os) << "tp: baz\n";
    (*os).close();

    touch_exec("foo");
    touch_exec("baz");

    try {
        (void)tools::read_atffile(tools::fs::path("Atffile"));
        ATF_FAIL("Missing file 'bar' did not raise an error");
    } catch (const tools::not_found_error< tools::fs::path >& e) {
        ATF_REQUIRE_EQ("bar", e.get_value().str());
    }
}

// ------------------------------------------------------------------------
// Main.
// ------------------------------------------------------------------------

ATF_INIT_TEST_CASES(tcs)
{
    // Add the test cases for the parser class.
    ATF_ADD_TEST_CASE(tcs, atffile_1);
    ATF_ADD_TEST_CASE(tcs, atffile_2);
    ATF_ADD_TEST_CASE(tcs, atffile_3);
    ATF_ADD_TEST_CASE(tcs, atffile_4);
    ATF_ADD_TEST_CASE(tcs, atffile_5);
    ATF_ADD_TEST_CASE(tcs, atffile_6);
    ATF_ADD_TEST_CASE(tcs, atffile_50);
    ATF_ADD_TEST_CASE(tcs, atffile_51);
    ATF_ADD_TEST_CASE(tcs, atffile_52);
    ATF_ADD_TEST_CASE(tcs, atffile_53);
    ATF_ADD_TEST_CASE(tcs, atffile_54);

    // Add the test cases for the atffile class.
    ATF_ADD_TEST_CASE(tcs, atffile_getters);

    // Add the test cases for the free functions.
    ATF_ADD_TEST_CASE(tcs, read_ok_simple);
    ATF_ADD_TEST_CASE(tcs, read_ok_some_globs);
    ATF_ADD_TEST_CASE(tcs, read_missing_test_suite);
    ATF_ADD_TEST_CASE(tcs, read_missing_test_program);
}
