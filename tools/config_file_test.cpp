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

#include <atf-c++.hpp>

#include "config.hpp"
#include "config_file.hpp"
#include "env.hpp"
#include "test_helpers.hpp"

namespace impl = tools::config_file;
namespace detail = tools::config_file::detail;

namespace {

typedef std::map< std::string, std::string > vars_map;

} // anonymous namespace

namespace atf {
namespace config {

void __reinit(void);

}  // namespace config
}  // namespace atf

// -------------------------------------------------------------------------
// Tests for the "config" parser.
// -------------------------------------------------------------------------

class config_reader : protected detail::atf_config_reader {
    void
    got_var(const std::string& name, const std::string& val)
    {
        m_calls.push_back("got_var(" + name + ", " + val + ")");
    }

    void
    got_eof(void)
    {
        m_calls.push_back("got_eof()");
    }

public:
    config_reader(std::istream& is) :
        detail::atf_config_reader(is)
    {
    }

    void
    read(void)
    {
        atf_config_reader::read();
    }

    std::vector< std::string > m_calls;
};

ATF_TEST_CASE_WITHOUT_HEAD(config_1);
ATF_TEST_CASE_BODY(config_1)
{
    const char* input =
        "Content-Type: application/X-atf-config; version=\"1\"\n"
        "\n"
    ;

    const char* exp_calls[] = {
        "got_eof()",
        NULL
    };

    const char* exp_errors[] = {
        NULL
    };

    do_parser_test< config_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(config_2);
ATF_TEST_CASE_BODY(config_2)
{
    const char* input =
        "Content-Type: application/X-atf-config; version=\"1\"\n"
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

    do_parser_test< config_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(config_3);
ATF_TEST_CASE_BODY(config_3)
{
    const char* input =
        "Content-Type: application/X-atf-config; version=\"1\"\n"
        "\n"
        "var1=value1\n"
        "var2 = value2\n"
        "var3	=	value3\n"
        "var4	    =	    value4\n"
        "\n"
        "var5=value5\n"
        "    var6=value6\n"
        "\n"
        "var7 = \"This is a long value.\"\n"
        "var8 = \"Single-word\"\n"
        "var9 = \"    Single-word	\"\n"
        "var10 = Single-word\n"
    ;

    const char* exp_calls[] = {
        "got_var(var1, value1)",
        "got_var(var2, value2)",
        "got_var(var3, value3)",
        "got_var(var4, value4)",
        "got_var(var5, value5)",
        "got_var(var6, value6)",
        "got_var(var7, This is a long value.)",
        "got_var(var8, Single-word)",
        "got_var(var9,     Single-word	)",
        "got_var(var10, Single-word)",
        "got_eof()",
        NULL
    };

    const char* exp_errors[] = {
        NULL
    };

    do_parser_test< config_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(config_4);
ATF_TEST_CASE_BODY(config_4)
{
    const char* input =
        "Content-Type: application/X-atf-config; version=\"1\"\n"
        "\n"
        "foo = bar # A comment.\n"
    ;

    const char* exp_calls[] = {
        "got_var(foo, bar)",
        "got_eof()",
        NULL
    };

    const char* exp_errors[] = {
        NULL
    };

    do_parser_test< config_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(config_50);
ATF_TEST_CASE_BODY(config_50)
{
    const char* input =
        "Content-Type: application/X-atf-config; version=\"1\"\n"
        "\n"
        "foo\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "3: Unexpected token `<<NEWLINE>>'; expected equal sign",
        NULL
    };

    do_parser_test< config_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(config_51);
ATF_TEST_CASE_BODY(config_51)
{
    const char* input =
        "Content-Type: application/X-atf-config; version=\"1\"\n"
        "\n"
        "foo bar\n"
        "baz\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "3: Unexpected token `bar'; expected equal sign",
        "4: Unexpected token `<<NEWLINE>>'; expected equal sign",
        NULL
    };

    do_parser_test< config_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(config_52);
ATF_TEST_CASE_BODY(config_52)
{
    const char* input =
        "Content-Type: application/X-atf-config; version=\"1\"\n"
        "\n"
        "foo =\n"
        "bar = # A comment.\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "3: Unexpected token `<<NEWLINE>>'; expected word or quoted string",
        "4: Unexpected token `#'; expected word or quoted string",
        NULL
    };

    do_parser_test< config_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(config_53);
ATF_TEST_CASE_BODY(config_53)
{
    const char* input =
        "Content-Type: application/X-atf-config; version=\"1\"\n"
        "\n"
        "foo = \"Correct value\" # With comment.\n"
        "\n"
        "bar = # A comment.\n"
        "\n"
        "baz = \"Last variable\"\n"
        "\n"
        "# End of file.\n"
    ;

    const char* exp_calls[] = {
        "got_var(foo, Correct value)",
        NULL
    };

    const char* exp_errors[] = {
        "5: Unexpected token `#'; expected word or quoted string",
        NULL
    };

    do_parser_test< config_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(config_54);
ATF_TEST_CASE_BODY(config_54)
{
    const char* input =
        "Content-Type: application/X-atf-config; version=\"1\"\n"
        "\n"
        "foo = \"\n"
        "bar = \"text\n"
        "baz = \"te\\\"xt\n"
        "last = \"\\\"\n"
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

    do_parser_test< config_reader >(input, exp_calls, exp_errors);
}

// -------------------------------------------------------------------------
// Tests for the free functions.
// -------------------------------------------------------------------------

ATF_TEST_CASE(merge_configs_both_empty);
ATF_TEST_CASE_HEAD(merge_configs_both_empty) {}
ATF_TEST_CASE_BODY(merge_configs_both_empty) {
    vars_map lower, upper;

    ATF_REQUIRE(impl::merge_configs(lower, upper).empty());
}

ATF_TEST_CASE(merge_configs_lower_empty);
ATF_TEST_CASE_HEAD(merge_configs_lower_empty) {}
ATF_TEST_CASE_BODY(merge_configs_lower_empty) {
    vars_map lower, upper;
    upper["var"] = "value";

    vars_map merged = impl::merge_configs(lower, upper);
    ATF_REQUIRE_EQ("value", merged["var"]);
}

ATF_TEST_CASE(merge_configs_upper_empty);
ATF_TEST_CASE_HEAD(merge_configs_upper_empty) {}
ATF_TEST_CASE_BODY(merge_configs_upper_empty) {
    vars_map lower, upper;
    lower["var"] = "value";

    vars_map merged = impl::merge_configs(lower, upper);
    ATF_REQUIRE_EQ("value", merged["var"]);
}

ATF_TEST_CASE(merge_configs_mixed);
ATF_TEST_CASE_HEAD(merge_configs_mixed) {}
ATF_TEST_CASE_BODY(merge_configs_mixed) {
    vars_map lower, upper;
    lower["var1"] = "value1";
    lower["var2"] = "value2-l";
    upper["var2"] = "value2-u";
    upper["var3"] = "value3";

    vars_map merged = impl::merge_configs(lower, upper);
    ATF_REQUIRE_EQ("value1", merged["var1"]);
    ATF_REQUIRE_EQ("value2-u", merged["var2"]);
    ATF_REQUIRE_EQ("value3", merged["var3"]);
}

ATF_TEST_CASE(read_config_files_none);
ATF_TEST_CASE_HEAD(read_config_files_none) {}
ATF_TEST_CASE_BODY(read_config_files_none) {
    tools::env::set("ATF_CONFDIR", ".");
    atf::config::__reinit();
    ATF_REQUIRE(vars_map() == impl::read_config_files("test-suite"));
}

// -------------------------------------------------------------------------
// Main.
// -------------------------------------------------------------------------

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, config_1);
    ATF_ADD_TEST_CASE(tcs, config_2);
    ATF_ADD_TEST_CASE(tcs, config_3);
    ATF_ADD_TEST_CASE(tcs, config_4);
    ATF_ADD_TEST_CASE(tcs, config_50);
    ATF_ADD_TEST_CASE(tcs, config_51);
    ATF_ADD_TEST_CASE(tcs, config_52);
    ATF_ADD_TEST_CASE(tcs, config_53);
    ATF_ADD_TEST_CASE(tcs, config_54);

    ATF_ADD_TEST_CASE(tcs, merge_configs_both_empty);
    ATF_ADD_TEST_CASE(tcs, merge_configs_lower_empty);
    ATF_ADD_TEST_CASE(tcs, merge_configs_upper_empty);
    ATF_ADD_TEST_CASE(tcs, merge_configs_mixed);

    ATF_ADD_TEST_CASE(tcs, read_config_files_none);
}
