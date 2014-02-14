//
// Automated Testing Framework (atf)
//
// Copyright (c) 2007 The NetBSD Foundation, Inc.
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

#include <sstream>

#include <atf-c++.hpp>

#include "parser.hpp"
#include "test_helpers.hpp"

// ------------------------------------------------------------------------
// Tests for the "parse_error" class.
// ------------------------------------------------------------------------

ATF_TEST_CASE(parse_error_to_string);
ATF_TEST_CASE_HEAD(parse_error_to_string)
{
    set_md_var("descr", "Tests the parse_error conversion to strings");
}
ATF_TEST_CASE_BODY(parse_error_to_string)
{
    using tools::parser::parse_error;

    const parse_error e(123, "This is the message");
    ATF_REQUIRE_EQ("123: This is the message", std::string(e));
}

// ------------------------------------------------------------------------
// Tests for the "parse_errors" class.
// ------------------------------------------------------------------------

ATF_TEST_CASE(parse_errors_what);
ATF_TEST_CASE_HEAD(parse_errors_what)
{
    set_md_var("descr", "Tests the parse_errors description");
}
ATF_TEST_CASE_BODY(parse_errors_what)
{
    using tools::parser::parse_error;
    using tools::parser::parse_errors;

    parse_errors es;
    es.push_back(parse_error(2, "Second error"));
    es.push_back(parse_error(1, "First error"));

    ATF_REQUIRE_EQ("2: Second error\n1: First error", std::string(es.what()));
}

// ------------------------------------------------------------------------
// Tests for the "token" class.
// ------------------------------------------------------------------------

ATF_TEST_CASE(token_getters);
ATF_TEST_CASE_HEAD(token_getters)
{
    set_md_var("descr", "Tests the token getters");
}
ATF_TEST_CASE_BODY(token_getters)
{
    using tools::parser::token;

    {
        token t(10, 0);
        ATF_REQUIRE_EQ(t.lineno(), 10);
        ATF_REQUIRE_EQ(t.type(), 0);
        ATF_REQUIRE(t.text().empty());
    }

    {
        token t(10, 0, "foo");
        ATF_REQUIRE_EQ(t.lineno(), 10);
        ATF_REQUIRE_EQ(t.type(), 0);
        ATF_REQUIRE_EQ(t.text(), "foo");
    }

    {
        token t(20, 1);
        ATF_REQUIRE_EQ(t.lineno(), 20);
        ATF_REQUIRE_EQ(t.type(), 1);
        ATF_REQUIRE(t.text().empty());
    }

    {
        token t(20, 1, "bar");
        ATF_REQUIRE_EQ(t.lineno(), 20);
        ATF_REQUIRE_EQ(t.type(), 1);
        ATF_REQUIRE_EQ(t.text(), "bar");
    }
}

// ------------------------------------------------------------------------
// Tests for the "tokenizer" class.
// ------------------------------------------------------------------------

#define EXPECT(tkz, ttype, ttext) \
    do { \
        tools::parser::token t = tkz.next(); \
        ATF_REQUIRE(t.type() == ttype); \
        ATF_REQUIRE_EQ(t.text(), ttext); \
    } while (false);

namespace minimal {

    static const tools::parser::token_type eof_type = 0;
    static const tools::parser::token_type nl_type = 1;
    static const tools::parser::token_type word_type = 2;

    class tokenizer : public tools::parser::tokenizer< std::istream > {
    public:
        tokenizer(std::istream& is, bool skipws) :
            tools::parser::tokenizer< std::istream >
                (is, skipws, eof_type, nl_type, word_type)
        {
        }
    };

}

namespace delims {

    static const tools::parser::token_type eof_type = 0;
    static const tools::parser::token_type nl_type = 1;
    static const tools::parser::token_type word_type = 2;
    static const tools::parser::token_type plus_type = 3;
    static const tools::parser::token_type minus_type = 4;
    static const tools::parser::token_type equal_type = 5;

    class tokenizer : public tools::parser::tokenizer< std::istream > {
    public:
        tokenizer(std::istream& is, bool skipws) :
            tools::parser::tokenizer< std::istream >
                (is, skipws, eof_type, nl_type, word_type)
        {
            add_delim('+', plus_type);
            add_delim('-', minus_type);
            add_delim('=', equal_type);
        }
    };

}

namespace keywords {

    static const tools::parser::token_type eof_type = 0;
    static const tools::parser::token_type nl_type = 1;
    static const tools::parser::token_type word_type = 2;
    static const tools::parser::token_type var_type = 3;
    static const tools::parser::token_type loop_type = 4;
    static const tools::parser::token_type endloop_type = 5;

    class tokenizer : public tools::parser::tokenizer< std::istream > {
    public:
        tokenizer(std::istream& is, bool skipws) :
            tools::parser::tokenizer< std::istream >
                (is, skipws, eof_type, nl_type, word_type)
        {
            add_keyword("var", var_type);
            add_keyword("loop", loop_type);
            add_keyword("endloop", endloop_type);
        }
    };

}

namespace quotes {

    static const tools::parser::token_type eof_type = 0;
    static const tools::parser::token_type nl_type = 1;
    static const tools::parser::token_type word_type = 2;
    static const tools::parser::token_type dblquote_type = 3;

    class tokenizer : public tools::parser::tokenizer< std::istream > {
    public:
        tokenizer(std::istream& is, bool skipws) :
            tools::parser::tokenizer< std::istream >
                (is, skipws, eof_type, nl_type, word_type)
        {
            add_quote('"', dblquote_type);
        }
    };

}

ATF_TEST_CASE(tokenizer_minimal_nows);
ATF_TEST_CASE_HEAD(tokenizer_minimal_nows)
{
    set_md_var("descr", "Tests the tokenizer class using a minimal parser "
               "and not skipping whitespace");
}
ATF_TEST_CASE_BODY(tokenizer_minimal_nows)
{
    using namespace minimal;

    {
        std::istringstream iss("");
        tokenizer mt(iss, false);

        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("\n");
        tokenizer mt(iss, false);

        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("\n\n\n");
        tokenizer mt(iss, false);

        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("line 1");
        tokenizer mt(iss, false);

        EXPECT(mt, word_type, "line 1");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("line 1\n");
        tokenizer mt(iss, false);

        EXPECT(mt, word_type, "line 1");
        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("line 1\nline 2");
        tokenizer mt(iss, false);

        EXPECT(mt, word_type, "line 1");
        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, word_type, "line 2");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("line 1\nline 2\nline 3\n");
        tokenizer mt(iss, false);

        EXPECT(mt, word_type, "line 1");
        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, word_type, "line 2");
        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, word_type, "line 3");
        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }
}

ATF_TEST_CASE(tokenizer_minimal_ws);
ATF_TEST_CASE_HEAD(tokenizer_minimal_ws)
{
    set_md_var("descr", "Tests the tokenizer class using a minimal parser "
               "and skipping whitespace");
}
ATF_TEST_CASE_BODY(tokenizer_minimal_ws)
{
    using namespace minimal;

    {
        std::istringstream iss("");
        minimal::tokenizer mt(iss, true);

        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss(" \t ");
        tokenizer mt(iss, true);

        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("\n");
        tokenizer mt(iss, true);

        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss(" \t \n \t ");
        tokenizer mt(iss, true);

        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("\n\n\n");
        tokenizer mt(iss, true);

        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("line 1");
        tokenizer mt(iss, true);

        EXPECT(mt, word_type, "line");
        EXPECT(mt, word_type, "1");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("   \tline\t   1\t");
        tokenizer mt(iss, true);

        EXPECT(mt, word_type, "line");
        EXPECT(mt, word_type, "1");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("line 1\n");
        tokenizer mt(iss, true);

        EXPECT(mt, word_type, "line");
        EXPECT(mt, word_type, "1");
        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("line 1\nline 2");
        tokenizer mt(iss, true);

        EXPECT(mt, word_type, "line");
        EXPECT(mt, word_type, "1");
        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, word_type, "line");
        EXPECT(mt, word_type, "2");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("line 1\nline 2\nline 3\n");
        tokenizer mt(iss, true);

        EXPECT(mt, word_type, "line");
        EXPECT(mt, word_type, "1");
        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, word_type, "line");
        EXPECT(mt, word_type, "2");
        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, word_type, "line");
        EXPECT(mt, word_type, "3");
        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss(" \t line \t 1\n\tline\t2\n line 3 \n");
        tokenizer mt(iss, true);

        EXPECT(mt, word_type, "line");
        EXPECT(mt, word_type, "1");
        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, word_type, "line");
        EXPECT(mt, word_type, "2");
        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, word_type, "line");
        EXPECT(mt, word_type, "3");
        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }
}

ATF_TEST_CASE(tokenizer_delims_nows);
ATF_TEST_CASE_HEAD(tokenizer_delims_nows)
{
    set_md_var("descr", "Tests the tokenizer class using a parser with some "
               "additional delimiters and not skipping whitespace");
}
ATF_TEST_CASE_BODY(tokenizer_delims_nows)
{
    using namespace delims;

    {
        std::istringstream iss("+-=");
        tokenizer mt(iss, false);

        EXPECT(mt, plus_type, "+");
        EXPECT(mt, minus_type, "-");
        EXPECT(mt, equal_type, "=");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("+++");
        tokenizer mt(iss, false);

        EXPECT(mt, plus_type, "+");
        EXPECT(mt, plus_type, "+");
        EXPECT(mt, plus_type, "+");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("\n+\n++\n");
        tokenizer mt(iss, false);

        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, plus_type, "+");
        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, plus_type, "+");
        EXPECT(mt, plus_type, "+");
        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("foo+bar=baz");
        tokenizer mt(iss, false);

        EXPECT(mt, word_type, "foo");
        EXPECT(mt, plus_type, "+");
        EXPECT(mt, word_type, "bar");
        EXPECT(mt, equal_type, "=");
        EXPECT(mt, word_type, "baz");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss(" foo\t+\tbar = baz ");
        tokenizer mt(iss, false);

        EXPECT(mt, word_type, " foo\t");
        EXPECT(mt, plus_type, "+");
        EXPECT(mt, word_type, "\tbar ");
        EXPECT(mt, equal_type, "=");
        EXPECT(mt, word_type, " baz ");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }
}

ATF_TEST_CASE(tokenizer_delims_ws);
ATF_TEST_CASE_HEAD(tokenizer_delims_ws)
{
    set_md_var("descr", "Tests the tokenizer class using a parser with some "
               "additional delimiters and skipping whitespace");
}
ATF_TEST_CASE_BODY(tokenizer_delims_ws)
{
    using namespace delims;

    {
        std::istringstream iss(" foo\t+\tbar = baz ");
        tokenizer mt(iss, true);

        EXPECT(mt, word_type, "foo");
        EXPECT(mt, plus_type, "+");
        EXPECT(mt, word_type, "bar");
        EXPECT(mt, equal_type, "=");
        EXPECT(mt, word_type, "baz");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }
}

ATF_TEST_CASE(tokenizer_keywords_nows);
ATF_TEST_CASE_HEAD(tokenizer_keywords_nows)
{
    set_md_var("descr", "Tests the tokenizer class using a parser with some "
               "additional keywords and not skipping whitespace");
}
ATF_TEST_CASE_BODY(tokenizer_keywords_nows)
{
    using namespace keywords;

    {
        std::istringstream iss("var");
        tokenizer mt(iss, false);

        EXPECT(mt, var_type, "var");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("va");
        tokenizer mt(iss, false);

        EXPECT(mt, word_type, "va");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("vara");
        tokenizer mt(iss, false);

        EXPECT(mt, word_type, "vara");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("var ");
        tokenizer mt(iss, false);

        EXPECT(mt, word_type, "var ");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("var\nloop\nendloop");
        tokenizer mt(iss, false);

        EXPECT(mt, var_type, "var");
        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, loop_type, "loop");
        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, endloop_type, "endloop");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }
}

ATF_TEST_CASE(tokenizer_keywords_ws);
ATF_TEST_CASE_HEAD(tokenizer_keywords_ws)
{
    set_md_var("descr", "Tests the tokenizer class using a parser with some "
               "additional keywords and not skipping whitespace");
}
ATF_TEST_CASE_BODY(tokenizer_keywords_ws)
{
    using namespace keywords;

    {
        std::istringstream iss("var ");
        tokenizer mt(iss, true);

        EXPECT(mt, var_type, "var");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss(" var \n\tloop\t\n \tendloop \t");
        tokenizer mt(iss, true);

        EXPECT(mt, var_type, "var");
        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, loop_type, "loop");
        EXPECT(mt, nl_type, "<<NEWLINE>>");
        EXPECT(mt, endloop_type, "endloop");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("var loop endloop");
        tokenizer mt(iss, true);

        EXPECT(mt, var_type, "var");
        EXPECT(mt, loop_type, "loop");
        EXPECT(mt, endloop_type, "endloop");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }
}

ATF_TEST_CASE(tokenizer_quotes_nows);
ATF_TEST_CASE_HEAD(tokenizer_quotes_nows)
{
    set_md_var("descr", "Tests the tokenizer class using a parser with "
               "quoted strings and not skipping whitespace");
}
ATF_TEST_CASE_BODY(tokenizer_quotes_nows)
{
    using namespace quotes;

    {
        std::istringstream iss("var");
        tokenizer mt(iss, false);

        EXPECT(mt, word_type, "var");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("\"var\"");
        tokenizer mt(iss, false);

        EXPECT(mt, word_type, "var");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("var1\"var2\"");
        tokenizer mt(iss, false);

        EXPECT(mt, word_type, "var1");
        EXPECT(mt, word_type, "var2");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("var1\"  var2  \"");
        tokenizer mt(iss, false);

        EXPECT(mt, word_type, "var1");
        EXPECT(mt, word_type, "  var2  ");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }
}

ATF_TEST_CASE(tokenizer_quotes_ws);
ATF_TEST_CASE_HEAD(tokenizer_quotes_ws)
{
    set_md_var("descr", "Tests the tokenizer class using a parser with "
               "quoted strings and skipping whitespace");
}
ATF_TEST_CASE_BODY(tokenizer_quotes_ws)
{
    using namespace quotes;

    {
        std::istringstream iss("  var  ");
        tokenizer mt(iss, true);

        EXPECT(mt, word_type, "var");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("  \"var\"  ");
        tokenizer mt(iss, true);

        EXPECT(mt, word_type, "var");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("  var1  \"var2\"  ");
        tokenizer mt(iss, true);

        EXPECT(mt, word_type, "var1");
        EXPECT(mt, word_type, "var2");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }

    {
        std::istringstream iss("  var1  \"  var2  \"  ");
        tokenizer mt(iss, true);

        EXPECT(mt, word_type, "var1");
        EXPECT(mt, word_type, "  var2  ");
        EXPECT(mt, eof_type, "<<EOF>>");
        EXPECT(mt, eof_type, "<<EOF>>");
    }
}

// ------------------------------------------------------------------------
// Tests for the headers parser.
// ------------------------------------------------------------------------

class header_reader {
    std::istream& m_is;

public:
    header_reader(std::istream& is) :
        m_is(is)
    {
    }

    void
    read(void)
    {
        std::pair< size_t, tools::parser::headers_map > hml =
            tools::parser::read_headers(m_is, 1);
        tools::parser::validate_content_type(hml.second,
            "application/X-atf-headers-test", 1234);
    }

    std::vector< std::string > m_calls;
};

ATF_TEST_CASE_WITHOUT_HEAD(headers_1);
ATF_TEST_CASE_BODY(headers_1)
{
    const char* input =
        ""
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "1: Unexpected token `<<EOF>>'; expected a header name",
        NULL
    };

    do_parser_test< header_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(headers_2);
ATF_TEST_CASE_BODY(headers_2)
{
    const char* input =
        "Content-Type\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "1: Unexpected token `<<NEWLINE>>'; expected `:'",
        NULL
    };

    do_parser_test< header_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(headers_3);
ATF_TEST_CASE_BODY(headers_3)
{
    const char* input =
        "Content-Type:\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "1: Unexpected token `<<NEWLINE>>'; expected a textual value",
        NULL
    };

    do_parser_test< header_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(headers_4);
ATF_TEST_CASE_BODY(headers_4)
{
    const char* input =
        "Content-Type: application/X-atf-headers-test\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "2: Unexpected token `<<EOF>>'; expected a header name",
        NULL
    };

    do_parser_test< header_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(headers_5);
ATF_TEST_CASE_BODY(headers_5)
{
    const char* input =
        "Content-Type: application/X-atf-headers-test;\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "1: Unexpected token `<<NEWLINE>>'; expected an attribute name",
        NULL
    };

    do_parser_test< header_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(headers_6);
ATF_TEST_CASE_BODY(headers_6)
{
    const char* input =
        "Content-Type: application/X-atf-headers-test; version\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "1: Unexpected token `<<NEWLINE>>'; expected `='",
        NULL
    };

    do_parser_test< header_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(headers_7);
ATF_TEST_CASE_BODY(headers_7)
{
    const char* input =
        "Content-Type: application/X-atf-headers-test; version=\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "1: Unexpected token `<<NEWLINE>>'; expected word or quoted string",
        NULL
    };

    do_parser_test< header_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(headers_8);
ATF_TEST_CASE_BODY(headers_8)
{
    const char* input =
        "Content-Type: application/X-atf-headers-test; version=\"1234\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "1: Missing double quotes before end of line",
        NULL
    };

    do_parser_test< header_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(headers_9);
ATF_TEST_CASE_BODY(headers_9)
{
    const char* input =
        "Content-Type: application/X-atf-headers-test; version=1234\"\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "1: Missing double quotes before end of line",
        NULL
    };

    do_parser_test< header_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(headers_10);
ATF_TEST_CASE_BODY(headers_10)
{
    const char* input =
        "Content-Type: application/X-atf-headers-test; version=1234\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "2: Unexpected token `<<EOF>>'; expected a header name",
        NULL
    };

    do_parser_test< header_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(headers_11);
ATF_TEST_CASE_BODY(headers_11)
{
    const char* input =
        "Content-Type: application/X-atf-headers-test; version=\"1234\"\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "2: Unexpected token `<<EOF>>'; expected a header name",
        NULL
    };

    do_parser_test< header_reader >(input, exp_calls, exp_errors);
}

ATF_TEST_CASE_WITHOUT_HEAD(headers_12);
ATF_TEST_CASE_BODY(headers_12)
{
    const char* input =
        "Content-Type: application/X-atf-headers-test; version=\"1234\"\n"
        "a b\n"
        "a-b:\n"
        "a-b: foo;\n"
        "a-b: foo; var\n"
        "a-b: foo; var=\n"
        "a-b: foo; var=\"a\n"
        "a-b: foo; var=a\"\n"
        "a-b: foo; var=\"a\";\n"
        "a-b: foo; var=\"a\"; second\n"
        "a-b: foo; var=\"a\"; second=\n"
        "a-b: foo; var=\"a\"; second=\"b\n"
        "a-b: foo; var=\"a\"; second=b\"\n"
        "a-b: foo; var=\"a\"; second=\"b\"\n"
    ;

    const char* exp_calls[] = {
        NULL
    };

    const char* exp_errors[] = {
        "2: Unexpected token `b'; expected `:'",
        "3: Unexpected token `<<NEWLINE>>'; expected a textual value",
        "4: Unexpected token `<<NEWLINE>>'; expected an attribute name",
        "5: Unexpected token `<<NEWLINE>>'; expected `='",
        "6: Unexpected token `<<NEWLINE>>'; expected word or quoted string",
        "7: Missing double quotes before end of line",
        "8: Missing double quotes before end of line",
        "9: Unexpected token `<<NEWLINE>>'; expected an attribute name",
        "10: Unexpected token `<<NEWLINE>>'; expected `='",
        "11: Unexpected token `<<NEWLINE>>'; expected word or quoted string",
        "12: Missing double quotes before end of line",
        "13: Missing double quotes before end of line",
        NULL
    };

    do_parser_test< header_reader >(input, exp_calls, exp_errors);
}

// ------------------------------------------------------------------------
// Main.
// ------------------------------------------------------------------------

ATF_INIT_TEST_CASES(tcs)
{
    // Add test cases for the "parse_error" class.
    ATF_ADD_TEST_CASE(tcs, parse_error_to_string);

    // Add test cases for the "parse_errors" class.
    ATF_ADD_TEST_CASE(tcs, parse_errors_what);

    // Add test cases for the "token" class.
    ATF_ADD_TEST_CASE(tcs, token_getters);

    // Add test cases for the "tokenizer" class.
    ATF_ADD_TEST_CASE(tcs, tokenizer_minimal_nows);
    ATF_ADD_TEST_CASE(tcs, tokenizer_minimal_ws);
    ATF_ADD_TEST_CASE(tcs, tokenizer_delims_nows);
    ATF_ADD_TEST_CASE(tcs, tokenizer_delims_ws);
    ATF_ADD_TEST_CASE(tcs, tokenizer_keywords_nows);
    ATF_ADD_TEST_CASE(tcs, tokenizer_keywords_ws);
    ATF_ADD_TEST_CASE(tcs, tokenizer_quotes_nows);
    ATF_ADD_TEST_CASE(tcs, tokenizer_quotes_ws);

    // Add the tests for the headers parser.

    // Add the test cases for the header file.
    ATF_ADD_TEST_CASE(tcs, headers_1);
    ATF_ADD_TEST_CASE(tcs, headers_2);
    ATF_ADD_TEST_CASE(tcs, headers_3);
    ATF_ADD_TEST_CASE(tcs, headers_4);
    ATF_ADD_TEST_CASE(tcs, headers_5);
    ATF_ADD_TEST_CASE(tcs, headers_6);
    ATF_ADD_TEST_CASE(tcs, headers_7);
    ATF_ADD_TEST_CASE(tcs, headers_8);
    ATF_ADD_TEST_CASE(tcs, headers_9);
    ATF_ADD_TEST_CASE(tcs, headers_10);
    ATF_ADD_TEST_CASE(tcs, headers_11);
    ATF_ADD_TEST_CASE(tcs, headers_12);
}
