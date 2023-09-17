// Copyright 2012 The Kyua Authors.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "utils/text/templates.hpp"

#include <fstream>
#include <sstream>

#include <atf-c++.hpp>

#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/text/exceptions.hpp"

namespace fs = utils::fs;
namespace text = utils::text;


namespace {


/// Applies a set of templates to an input string and validates the output.
///
/// This fails the test case if exp_output does not match the document generated
/// by the application of the templates.
///
/// \param templates The templates to apply.
/// \param input_str The input document to which to apply the templates.
/// \param exp_output The expected output document.
static void
do_test_ok(const text::templates_def& templates, const std::string& input_str,
           const std::string& exp_output)
{
    std::istringstream input(input_str);
    std::ostringstream output;

    text::instantiate(templates, input, output);
    ATF_REQUIRE_EQ(exp_output, output.str());
}


/// Applies a set of templates to an input string and checks for an error.
///
/// This fails the test case if the exception raised by the template processing
/// does not match the expected message.
///
/// \param templates The templates to apply.
/// \param input_str The input document to which to apply the templates.
/// \param exp_message The expected error message in the raised exception.
static void
do_test_fail(const text::templates_def& templates, const std::string& input_str,
             const std::string& exp_message)
{
    std::istringstream input(input_str);
    std::ostringstream output;

    ATF_REQUIRE_THROW_RE(text::syntax_error, exp_message,
                         text::instantiate(templates, input, output));
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(templates_def__add_variable__first);
ATF_TEST_CASE_BODY(templates_def__add_variable__first)
{
    text::templates_def templates;
    templates.add_variable("the-name", "first-value");
    ATF_REQUIRE_EQ("first-value", templates.get_variable("the-name"));
}


ATF_TEST_CASE_WITHOUT_HEAD(templates_def__add_variable__replace);
ATF_TEST_CASE_BODY(templates_def__add_variable__replace)
{
    text::templates_def templates;
    templates.add_variable("the-name", "first-value");
    templates.add_variable("the-name", "second-value");
    ATF_REQUIRE_EQ("second-value", templates.get_variable("the-name"));
}


ATF_TEST_CASE_WITHOUT_HEAD(templates_def__remove_variable);
ATF_TEST_CASE_BODY(templates_def__remove_variable)
{
    text::templates_def templates;
    templates.add_variable("the-name", "the-value");
    templates.get_variable("the-name");  // Should not throw.
    templates.remove_variable("the-name");
    ATF_REQUIRE_THROW(text::syntax_error, templates.get_variable("the-name"));
}


ATF_TEST_CASE_WITHOUT_HEAD(templates_def__add_vector__first);
ATF_TEST_CASE_BODY(templates_def__add_vector__first)
{
    text::templates_def templates;
    templates.add_vector("the-name");
    ATF_REQUIRE(templates.get_vector("the-name").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(templates_def__add_vector__replace);
ATF_TEST_CASE_BODY(templates_def__add_vector__replace)
{
    text::templates_def templates;
    templates.add_vector("the-name");
    templates.add_to_vector("the-name", "foo");
    ATF_REQUIRE(!templates.get_vector("the-name").empty());
    templates.add_vector("the-name");
    ATF_REQUIRE(templates.get_vector("the-name").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(templates_def__add_to_vector);
ATF_TEST_CASE_BODY(templates_def__add_to_vector)
{
    text::templates_def templates;
    templates.add_vector("the-name");
    ATF_REQUIRE_EQ(0, templates.get_vector("the-name").size());
    templates.add_to_vector("the-name", "first");
    ATF_REQUIRE_EQ(1, templates.get_vector("the-name").size());
    templates.add_to_vector("the-name", "second");
    ATF_REQUIRE_EQ(2, templates.get_vector("the-name").size());
    templates.add_to_vector("the-name", "third");
    ATF_REQUIRE_EQ(3, templates.get_vector("the-name").size());

    std::vector< std::string > expected;
    expected.push_back("first");
    expected.push_back("second");
    expected.push_back("third");
    ATF_REQUIRE(expected == templates.get_vector("the-name"));
}


ATF_TEST_CASE_WITHOUT_HEAD(templates_def__exists__variable);
ATF_TEST_CASE_BODY(templates_def__exists__variable)
{
    text::templates_def templates;
    ATF_REQUIRE(!templates.exists("some-name"));
    templates.add_variable("some-name ", "foo");
    ATF_REQUIRE(!templates.exists("some-name"));
    templates.add_variable("some-name", "foo");
    ATF_REQUIRE(templates.exists("some-name"));
}


ATF_TEST_CASE_WITHOUT_HEAD(templates_def__exists__vector);
ATF_TEST_CASE_BODY(templates_def__exists__vector)
{
    text::templates_def templates;
    ATF_REQUIRE(!templates.exists("some-name"));
    templates.add_vector("some-name ");
    ATF_REQUIRE(!templates.exists("some-name"));
    templates.add_vector("some-name");
    ATF_REQUIRE(templates.exists("some-name"));
}


ATF_TEST_CASE_WITHOUT_HEAD(templates_def__get_variable__ok);
ATF_TEST_CASE_BODY(templates_def__get_variable__ok)
{
    text::templates_def templates;
    templates.add_variable("foo", "");
    templates.add_variable("bar", "    baz  ");
    ATF_REQUIRE_EQ("", templates.get_variable("foo"));
    ATF_REQUIRE_EQ("    baz  ", templates.get_variable("bar"));
}


ATF_TEST_CASE_WITHOUT_HEAD(templates_def__get_variable__unknown);
ATF_TEST_CASE_BODY(templates_def__get_variable__unknown)
{
    text::templates_def templates;
    templates.add_variable("foo", "");
    ATF_REQUIRE_THROW_RE(text::syntax_error, "Unknown variable 'foo '",
                         templates.get_variable("foo "));
}


ATF_TEST_CASE_WITHOUT_HEAD(templates_def__get_vector__ok);
ATF_TEST_CASE_BODY(templates_def__get_vector__ok)
{
    text::templates_def templates;
    templates.add_vector("foo");
    templates.add_vector("bar");
    templates.add_to_vector("bar", "baz");
    ATF_REQUIRE_EQ(0, templates.get_vector("foo").size());
    ATF_REQUIRE_EQ(1, templates.get_vector("bar").size());
}


ATF_TEST_CASE_WITHOUT_HEAD(templates_def__get_vector__unknown);
ATF_TEST_CASE_BODY(templates_def__get_vector__unknown)
{
    text::templates_def templates;
    templates.add_vector("foo");
    ATF_REQUIRE_THROW_RE(text::syntax_error, "Unknown vector 'foo '",
                         templates.get_vector("foo "));
}


ATF_TEST_CASE_WITHOUT_HEAD(templates_def__evaluate__variable__ok);
ATF_TEST_CASE_BODY(templates_def__evaluate__variable__ok)
{
    text::templates_def templates;
    templates.add_variable("foo", "");
    templates.add_variable("bar", "    baz  ");
    ATF_REQUIRE_EQ("", templates.evaluate("foo"));
    ATF_REQUIRE_EQ("    baz  ", templates.evaluate("bar"));
}


ATF_TEST_CASE_WITHOUT_HEAD(templates_def__evaluate__variable__unknown);
ATF_TEST_CASE_BODY(templates_def__evaluate__variable__unknown)
{
    text::templates_def templates;
    templates.add_variable("foo", "");
    ATF_REQUIRE_THROW_RE(text::syntax_error, "Unknown variable 'foo1'",
                         templates.evaluate("foo1"));
}


ATF_TEST_CASE_WITHOUT_HEAD(templates_def__evaluate__vector__ok);
ATF_TEST_CASE_BODY(templates_def__evaluate__vector__ok)
{
    text::templates_def templates;
    templates.add_vector("v");
    templates.add_to_vector("v", "foo");
    templates.add_to_vector("v", "bar");
    templates.add_to_vector("v", "baz");

    templates.add_variable("index", "0");
    ATF_REQUIRE_EQ("foo", templates.evaluate("v(index)"));
    templates.add_variable("index", "1");
    ATF_REQUIRE_EQ("bar", templates.evaluate("v(index)"));
    templates.add_variable("index", "2");
    ATF_REQUIRE_EQ("baz", templates.evaluate("v(index)"));
}


ATF_TEST_CASE_WITHOUT_HEAD(templates_def__evaluate__vector__unknown_vector);
ATF_TEST_CASE_BODY(templates_def__evaluate__vector__unknown_vector)
{
    text::templates_def templates;
    templates.add_vector("v");
    templates.add_to_vector("v", "foo");
    templates.add_variable("index", "0");
    ATF_REQUIRE_THROW_RE(text::syntax_error, "Unknown vector 'fooz'",
                         templates.evaluate("fooz(index)"));
}


ATF_TEST_CASE_WITHOUT_HEAD(templates_def__evaluate__vector__unknown_index);
ATF_TEST_CASE_BODY(templates_def__evaluate__vector__unknown_index)
{
    text::templates_def templates;
    templates.add_vector("v");
    templates.add_to_vector("v", "foo");
    templates.add_variable("index", "0");
    ATF_REQUIRE_THROW_RE(text::syntax_error, "Unknown variable 'indexz'",
                         templates.evaluate("v(indexz)"));
}


ATF_TEST_CASE_WITHOUT_HEAD(templates_def__evaluate__vector__out_of_range);
ATF_TEST_CASE_BODY(templates_def__evaluate__vector__out_of_range)
{
    text::templates_def templates;
    templates.add_vector("v");
    templates.add_to_vector("v", "foo");
    templates.add_variable("index", "1");
    ATF_REQUIRE_THROW_RE(text::syntax_error, "Index 'index' out of range "
                         "at position '1'", templates.evaluate("v(index)"));
}


ATF_TEST_CASE_WITHOUT_HEAD(templates_def__evaluate__defined);
ATF_TEST_CASE_BODY(templates_def__evaluate__defined)
{
    text::templates_def templates;
    templates.add_vector("the-variable");
    templates.add_vector("the-vector");
    ATF_REQUIRE_EQ("false", templates.evaluate("defined(the-variabl)"));
    ATF_REQUIRE_EQ("false", templates.evaluate("defined(the-vecto)"));
    ATF_REQUIRE_EQ("true", templates.evaluate("defined(the-variable)"));
    ATF_REQUIRE_EQ("true", templates.evaluate("defined(the-vector)"));
}


ATF_TEST_CASE_WITHOUT_HEAD(templates_def__evaluate__length__ok);
ATF_TEST_CASE_BODY(templates_def__evaluate__length__ok)
{
    text::templates_def templates;
    templates.add_vector("v");
    ATF_REQUIRE_EQ("0", templates.evaluate("length(v)"));
    templates.add_to_vector("v", "foo");
    ATF_REQUIRE_EQ("1", templates.evaluate("length(v)"));
    templates.add_to_vector("v", "bar");
    ATF_REQUIRE_EQ("2", templates.evaluate("length(v)"));
    templates.add_to_vector("v", "baz");
    ATF_REQUIRE_EQ("3", templates.evaluate("length(v)"));
}


ATF_TEST_CASE_WITHOUT_HEAD(templates_def__evaluate__length__unknown_vector);
ATF_TEST_CASE_BODY(templates_def__evaluate__length__unknown_vector)
{
    text::templates_def templates;
    templates.add_vector("foo1");
    ATF_REQUIRE_THROW_RE(text::syntax_error, "Unknown vector 'foo'",
                         templates.evaluate("length(foo)"));
}


ATF_TEST_CASE_WITHOUT_HEAD(templates_def__evaluate__parenthesis_error);
ATF_TEST_CASE_BODY(templates_def__evaluate__parenthesis_error)
{
    text::templates_def templates;
    ATF_REQUIRE_THROW_RE(text::syntax_error,
                         "Expected '\\)' in.*'foo\\(abc'",
                         templates.evaluate("foo(abc"));
    ATF_REQUIRE_THROW_RE(text::syntax_error,
                         "Unexpected text.*'\\)' in.*'a\\(b\\)c'",
                         templates.evaluate("a(b)c"));
}


ATF_TEST_CASE_WITHOUT_HEAD(instantiate__empty_input);
ATF_TEST_CASE_BODY(instantiate__empty_input)
{
    const text::templates_def templates;
    do_test_ok(templates, "", "");
}


ATF_TEST_CASE_WITHOUT_HEAD(instantiate__value__ok);
ATF_TEST_CASE_BODY(instantiate__value__ok)
{
    const std::string input =
        "first line\n"
        "%%testvar1%%\n"
        "third line\n"
        "%%testvar2%% %%testvar3%%%%testvar4%%\n"
        "fifth line\n";

    const std::string exp_output =
        "first line\n"
        "second line\n"
        "third line\n"
        "fourth line.\n"
        "fifth line\n";

    text::templates_def templates;
    templates.add_variable("testvar1", "second line");
    templates.add_variable("testvar2", "fourth");
    templates.add_variable("testvar3", "line");
    templates.add_variable("testvar4", ".");

    do_test_ok(templates, input, exp_output);
}


ATF_TEST_CASE_WITHOUT_HEAD(instantiate__value__unknown_variable);
ATF_TEST_CASE_BODY(instantiate__value__unknown_variable)
{
    const std::string input =
        "%%testvar1%%\n";

    text::templates_def templates;
    templates.add_variable("testvar2", "fourth line");

    do_test_fail(templates, input, "Unknown variable 'testvar1'");
}


ATF_TEST_CASE_WITHOUT_HEAD(instantiate__vector_length__ok);
ATF_TEST_CASE_BODY(instantiate__vector_length__ok)
{
    const std::string input =
        "%%length(testvector1)%%\n"
        "%%length(testvector2)%% - %%length(testvector3)%%\n";

    const std::string exp_output =
        "4\n"
        "0 - 1\n";

    text::templates_def templates;
    templates.add_vector("testvector1");
    templates.add_to_vector("testvector1", "000");
    templates.add_to_vector("testvector1", "111");
    templates.add_to_vector("testvector1", "543");
    templates.add_to_vector("testvector1", "999");
    templates.add_vector("testvector2");
    templates.add_vector("testvector3");
    templates.add_to_vector("testvector3", "123");

    do_test_ok(templates, input, exp_output);
}


ATF_TEST_CASE_WITHOUT_HEAD(instantiate__vector_length__unknown_vector);
ATF_TEST_CASE_BODY(instantiate__vector_length__unknown_vector)
{
    const std::string input =
        "%%length(testvector)%%\n";

    text::templates_def templates;
    templates.add_vector("testvector2");

    do_test_fail(templates, input, "Unknown vector 'testvector'");
}


ATF_TEST_CASE_WITHOUT_HEAD(instantiate__vector_value__ok);
ATF_TEST_CASE_BODY(instantiate__vector_value__ok)
{
    const std::string input =
        "first line\n"
        "%%testvector1(i)%%\n"
        "third line\n"
        "%%testvector2(j)%%\n"
        "fifth line\n";

    const std::string exp_output =
        "first line\n"
        "543\n"
        "third line\n"
        "123\n"
        "fifth line\n";

    text::templates_def templates;
    templates.add_variable("i", "2");
    templates.add_variable("j", "0");
    templates.add_vector("testvector1");
    templates.add_to_vector("testvector1", "000");
    templates.add_to_vector("testvector1", "111");
    templates.add_to_vector("testvector1", "543");
    templates.add_to_vector("testvector1", "999");
    templates.add_vector("testvector2");
    templates.add_to_vector("testvector2", "123");

    do_test_ok(templates, input, exp_output);
}


ATF_TEST_CASE_WITHOUT_HEAD(instantiate__vector_value__unknown_vector);
ATF_TEST_CASE_BODY(instantiate__vector_value__unknown_vector)
{
    const std::string input =
        "%%testvector(j)%%\n";

    text::templates_def templates;
    templates.add_vector("testvector2");

    do_test_fail(templates, input, "Unknown vector 'testvector'");
}


ATF_TEST_CASE_WITHOUT_HEAD(instantiate__vector_value__out_of_range__empty);
ATF_TEST_CASE_BODY(instantiate__vector_value__out_of_range__empty)
{
    const std::string input =
        "%%testvector(j)%%\n";

    text::templates_def templates;
    templates.add_vector("testvector");
    templates.add_variable("j", "0");

    do_test_fail(templates, input, "Index 'j' out of range at position '0'");
}


ATF_TEST_CASE_WITHOUT_HEAD(instantiate__vector_value__out_of_range__not_empty);
ATF_TEST_CASE_BODY(instantiate__vector_value__out_of_range__not_empty)
{
    const std::string input =
        "%%testvector(j)%%\n";

    text::templates_def templates;
    templates.add_vector("testvector");
    templates.add_to_vector("testvector", "a");
    templates.add_to_vector("testvector", "b");
    templates.add_variable("j", "2");

    do_test_fail(templates, input, "Index 'j' out of range at position '2'");
}


ATF_TEST_CASE_WITHOUT_HEAD(instantiate__if__one_level__taken);
ATF_TEST_CASE_BODY(instantiate__if__one_level__taken)
{
    const std::string input =
        "first line\n"
        "%if defined(some_var)\n"
        "hello from within the variable conditional\n"
        "%endif\n"
        "%if defined(some_vector)\n"
        "hello from within the vector conditional\n"
        "%else\n"
        "bye from within the vector conditional\n"
        "%endif\n"
        "some more\n";

    const std::string exp_output =
        "first line\n"
        "hello from within the variable conditional\n"
        "hello from within the vector conditional\n"
        "some more\n";

    text::templates_def templates;
    templates.add_variable("some_var", "zzz");
    templates.add_vector("some_vector");

    do_test_ok(templates, input, exp_output);
}


ATF_TEST_CASE_WITHOUT_HEAD(instantiate__if__one_level__not_taken);
ATF_TEST_CASE_BODY(instantiate__if__one_level__not_taken)
{
    const std::string input =
        "first line\n"
        "%if defined(some_var)\n"
        "hello from within the variable conditional\n"
        "%endif\n"
        "%if defined(some_vector)\n"
        "hello from within the vector conditional\n"
        "%else\n"
        "bye from within the vector conditional\n"
        "%endif\n"
        "some more\n";

    const std::string exp_output =
        "first line\n"
        "bye from within the vector conditional\n"
        "some more\n";

    text::templates_def templates;

    do_test_ok(templates, input, exp_output);
}


ATF_TEST_CASE_WITHOUT_HEAD(instantiate__if__multiple_levels__taken);
ATF_TEST_CASE_BODY(instantiate__if__multiple_levels__taken)
{
    const std::string input =
        "first line\n"
        "%if defined(var1)\n"
        "first before\n"
        "%if length(var2)\n"
        "second before\n"
        "%if defined(var3)\n"
        "third before\n"
        "hello from within the conditional\n"
        "third after\n"
        "%endif\n"
        "second after\n"
        "%else\n"
        "second after not shown\n"
        "%endif\n"
        "first after\n"
        "%endif\n"
        "some more\n";

    const std::string exp_output =
        "first line\n"
        "first before\n"
        "second before\n"
        "third before\n"
        "hello from within the conditional\n"
        "third after\n"
        "second after\n"
        "first after\n"
        "some more\n";

    text::templates_def templates;
    templates.add_variable("var1", "false");
    templates.add_vector("var2");
    templates.add_to_vector("var2", "not-empty");
    templates.add_variable("var3", "foobar");

    do_test_ok(templates, input, exp_output);
}


ATF_TEST_CASE_WITHOUT_HEAD(instantiate__if__multiple_levels__not_taken);
ATF_TEST_CASE_BODY(instantiate__if__multiple_levels__not_taken)
{
    const std::string input =
        "first line\n"
        "%if defined(var1)\n"
        "first before\n"
        "%if length(var2)\n"
        "second before\n"
        "%if defined(var3)\n"
        "third before\n"
        "hello from within the conditional\n"
        "third after\n"
        "%else\n"
        "will not be shown either\n"
        "%endif\n"
        "second after\n"
        "%else\n"
        "second after shown\n"
        "%endif\n"
        "first after\n"
        "%endif\n"
        "some more\n";

    const std::string exp_output =
        "first line\n"
        "first before\n"
        "second after shown\n"
        "first after\n"
        "some more\n";

    text::templates_def templates;
    templates.add_variable("var1", "false");
    templates.add_vector("var2");
    templates.add_vector("var3");

    do_test_ok(templates, input, exp_output);
}


ATF_TEST_CASE_WITHOUT_HEAD(instantiate__loop__no_iterations);
ATF_TEST_CASE_BODY(instantiate__loop__no_iterations)
{
    const std::string input =
        "first line\n"
        "%loop table1 i\n"
        "hello\n"
        "value in vector: %%table1(i)%%\n"
        "%if defined(var1)\n" "some other text\n" "%endif\n"
        "%endloop\n"
        "some more\n";

    const std::string exp_output =
        "first line\n"
        "some more\n";

    text::templates_def templates;
    templates.add_variable("var1", "defined");
    templates.add_vector("table1");

    do_test_ok(templates, input, exp_output);
}


ATF_TEST_CASE_WITHOUT_HEAD(instantiate__loop__multiple_iterations);
ATF_TEST_CASE_BODY(instantiate__loop__multiple_iterations)
{
    const std::string input =
        "first line\n"
        "%loop table1 i\n"
        "hello %%table1(i)%% %%table2(i)%%\n"
        "%endloop\n"
        "some more\n";

    const std::string exp_output =
        "first line\n"
        "hello foo1 foo2\n"
        "hello bar1 bar2\n"
        "some more\n";

    text::templates_def templates;
    templates.add_vector("table1");
    templates.add_to_vector("table1", "foo1");
    templates.add_to_vector("table1", "bar1");
    templates.add_vector("table2");
    templates.add_to_vector("table2", "foo2");
    templates.add_to_vector("table2", "bar2");

    do_test_ok(templates, input, exp_output);
}


ATF_TEST_CASE_WITHOUT_HEAD(instantiate__loop__nested__no_iterations);
ATF_TEST_CASE_BODY(instantiate__loop__nested__no_iterations)
{
    const std::string input =
        "first line\n"
        "%loop table1 i\n"
        "before: %%table1(i)%%\n"
        "%loop table2 j\n"
        "before: %%table2(j)%%\n"
        "%loop table3 k\n"
        "%%table3(k)%%\n"
        "%endloop\n"
        "after: %%table2(i)%%\n"
        "%endloop\n"
        "after: %%table1(i)%%\n"
        "%endloop\n"
        "some more\n";

    const std::string exp_output =
        "first line\n"
        "before: a\n"
        "after: a\n"
        "before: b\n"
        "after: b\n"
        "some more\n";

    text::templates_def templates;
    templates.add_vector("table1");
    templates.add_to_vector("table1", "a");
    templates.add_to_vector("table1", "b");
    templates.add_vector("table2");
    templates.add_vector("table3");
    templates.add_to_vector("table3", "1");

    do_test_ok(templates, input, exp_output);
}


ATF_TEST_CASE_WITHOUT_HEAD(instantiate__loop__nested__multiple_iterations);
ATF_TEST_CASE_BODY(instantiate__loop__nested__multiple_iterations)
{
    const std::string input =
        "first line\n"
        "%loop table1 i\n"
        "%loop table2 j\n"
        "%%table1(i)%% %%table2(j)%%\n"
        "%endloop\n"
        "%endloop\n"
        "some more\n";

    const std::string exp_output =
        "first line\n"
        "a 1\n"
        "a 2\n"
        "a 3\n"
        "b 1\n"
        "b 2\n"
        "b 3\n"
        "some more\n";

    text::templates_def templates;
    templates.add_vector("table1");
    templates.add_to_vector("table1", "a");
    templates.add_to_vector("table1", "b");
    templates.add_vector("table2");
    templates.add_to_vector("table2", "1");
    templates.add_to_vector("table2", "2");
    templates.add_to_vector("table2", "3");

    do_test_ok(templates, input, exp_output);
}


ATF_TEST_CASE_WITHOUT_HEAD(instantiate__loop__sequential);
ATF_TEST_CASE_BODY(instantiate__loop__sequential)
{
    const std::string input =
        "first line\n"
        "%loop table1 iter\n"
        "1: %%table1(iter)%%\n"
        "%endloop\n"
        "divider\n"
        "%loop table2 iter\n"
        "2: %%table2(iter)%%\n"
        "%endloop\n"
        "divider\n"
        "%loop table3 iter\n"
        "3: %%table3(iter)%%\n"
        "%endloop\n"
        "divider\n"
        "%loop table4 iter\n"
        "4: %%table4(iter)%%\n"
        "%endloop\n"
        "some more\n";

    const std::string exp_output =
        "first line\n"
        "1: a\n"
        "1: b\n"
        "divider\n"
        "divider\n"
        "divider\n"
        "4: 1\n"
        "4: 2\n"
        "4: 3\n"
        "some more\n";

    text::templates_def templates;
    templates.add_vector("table1");
    templates.add_to_vector("table1", "a");
    templates.add_to_vector("table1", "b");
    templates.add_vector("table2");
    templates.add_vector("table3");
    templates.add_vector("table4");
    templates.add_to_vector("table4", "1");
    templates.add_to_vector("table4", "2");
    templates.add_to_vector("table4", "3");

    do_test_ok(templates, input, exp_output);
}


ATF_TEST_CASE_WITHOUT_HEAD(instantiate__loop__scoping);
ATF_TEST_CASE_BODY(instantiate__loop__scoping)
{
    const std::string input =
        "%loop table1 i\n"
        "%if defined(i)\n" "i defined inside scope 1\n" "%endif\n"
        "%loop table2 j\n"
        "%if defined(i)\n" "i defined inside scope 2\n" "%endif\n"
        "%if defined(j)\n" "j defined inside scope 2\n" "%endif\n"
        "%endloop\n"
        "%if defined(j)\n" "j defined inside scope 1\n" "%endif\n"
        "%endloop\n"
        "%if defined(i)\n" "i defined outside\n" "%endif\n"
        "%if defined(j)\n" "j defined outside\n" "%endif\n";

    const std::string exp_output =
        "i defined inside scope 1\n"
        "i defined inside scope 2\n"
        "j defined inside scope 2\n"
        "i defined inside scope 1\n"
        "i defined inside scope 2\n"
        "j defined inside scope 2\n";

    text::templates_def templates;
    templates.add_vector("table1");
    templates.add_to_vector("table1", "first");
    templates.add_to_vector("table1", "second");
    templates.add_vector("table2");
    templates.add_to_vector("table2", "first");

    do_test_ok(templates, input, exp_output);
}


ATF_TEST_CASE_WITHOUT_HEAD(instantiate__mismatched_delimiters);
ATF_TEST_CASE_BODY(instantiate__mismatched_delimiters)
{
    const std::string input =
        "this is some %% text\n"
        "and this is %%var%% text%%\n";

    const std::string exp_output =
        "this is some %% text\n"
        "and this is some more text%%\n";

    text::templates_def templates;
    templates.add_variable("var", "some more");

    do_test_ok(templates, input, exp_output);
}


ATF_TEST_CASE_WITHOUT_HEAD(instantiate__empty_statement);
ATF_TEST_CASE_BODY(instantiate__empty_statement)
{
    do_test_fail(text::templates_def(), "%\n", "Empty statement");
}


ATF_TEST_CASE_WITHOUT_HEAD(instantiate__unknown_statement);
ATF_TEST_CASE_BODY(instantiate__unknown_statement)
{
    do_test_fail(text::templates_def(), "%if2\n", "Unknown statement 'if2'");
}


ATF_TEST_CASE_WITHOUT_HEAD(instantiate__invalid_narguments);
ATF_TEST_CASE_BODY(instantiate__invalid_narguments)
{
    do_test_fail(text::templates_def(), "%if a b\n",
                 "Invalid number of arguments for statement 'if'");
}


ATF_TEST_CASE_WITHOUT_HEAD(instantiate__files__ok);
ATF_TEST_CASE_BODY(instantiate__files__ok)
{
    text::templates_def templates;
    templates.add_variable("string", "Hello, world!");

    atf::utils::create_file("input.txt", "The string is: %%string%%\n");

    text::instantiate(templates, fs::path("input.txt"), fs::path("output.txt"));

    std::ifstream output("output.txt");
    std::string line;
    ATF_REQUIRE(std::getline(output, line).good());
    ATF_REQUIRE_EQ(line, "The string is: Hello, world!");
    ATF_REQUIRE(std::getline(output, line).eof());
}


ATF_TEST_CASE_WITHOUT_HEAD(instantiate__files__input_error);
ATF_TEST_CASE_BODY(instantiate__files__input_error)
{
    text::templates_def templates;
    ATF_REQUIRE_THROW_RE(text::error, "Failed to open input.txt for read",
                         text::instantiate(templates, fs::path("input.txt"),
                                           fs::path("output.txt")));
}


ATF_TEST_CASE(instantiate__files__output_error);
ATF_TEST_CASE_HEAD(instantiate__files__output_error)
{
    set_md_var("require.user", "unprivileged");
}
ATF_TEST_CASE_BODY(instantiate__files__output_error)
{
    text::templates_def templates;

    atf::utils::create_file("input.txt", "");

    fs::mkdir(fs::path("dir"), 0444);

    ATF_REQUIRE_THROW_RE(text::error, "Failed to open dir/output.txt for write",
                         text::instantiate(templates, fs::path("input.txt"),
                                           fs::path("dir/output.txt")));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, templates_def__add_variable__first);
    ATF_ADD_TEST_CASE(tcs, templates_def__add_variable__replace);
    ATF_ADD_TEST_CASE(tcs, templates_def__remove_variable);
    ATF_ADD_TEST_CASE(tcs, templates_def__add_vector__first);
    ATF_ADD_TEST_CASE(tcs, templates_def__add_vector__replace);
    ATF_ADD_TEST_CASE(tcs, templates_def__add_to_vector);
    ATF_ADD_TEST_CASE(tcs, templates_def__exists__variable);
    ATF_ADD_TEST_CASE(tcs, templates_def__exists__vector);
    ATF_ADD_TEST_CASE(tcs, templates_def__get_variable__ok);
    ATF_ADD_TEST_CASE(tcs, templates_def__get_variable__unknown);
    ATF_ADD_TEST_CASE(tcs, templates_def__get_vector__ok);
    ATF_ADD_TEST_CASE(tcs, templates_def__get_vector__unknown);
    ATF_ADD_TEST_CASE(tcs, templates_def__evaluate__variable__ok);
    ATF_ADD_TEST_CASE(tcs, templates_def__evaluate__variable__unknown);
    ATF_ADD_TEST_CASE(tcs, templates_def__evaluate__vector__ok);
    ATF_ADD_TEST_CASE(tcs, templates_def__evaluate__vector__unknown_vector);
    ATF_ADD_TEST_CASE(tcs, templates_def__evaluate__vector__unknown_index);
    ATF_ADD_TEST_CASE(tcs, templates_def__evaluate__vector__out_of_range);
    ATF_ADD_TEST_CASE(tcs, templates_def__evaluate__defined);
    ATF_ADD_TEST_CASE(tcs, templates_def__evaluate__length__ok);
    ATF_ADD_TEST_CASE(tcs, templates_def__evaluate__length__unknown_vector);
    ATF_ADD_TEST_CASE(tcs, templates_def__evaluate__parenthesis_error);

    ATF_ADD_TEST_CASE(tcs, instantiate__empty_input);
    ATF_ADD_TEST_CASE(tcs, instantiate__value__ok);
    ATF_ADD_TEST_CASE(tcs, instantiate__value__unknown_variable);
    ATF_ADD_TEST_CASE(tcs, instantiate__vector_length__ok);
    ATF_ADD_TEST_CASE(tcs, instantiate__vector_length__unknown_vector);
    ATF_ADD_TEST_CASE(tcs, instantiate__vector_value__ok);
    ATF_ADD_TEST_CASE(tcs, instantiate__vector_value__unknown_vector);
    ATF_ADD_TEST_CASE(tcs, instantiate__vector_value__out_of_range__empty);
    ATF_ADD_TEST_CASE(tcs, instantiate__vector_value__out_of_range__not_empty);
    ATF_ADD_TEST_CASE(tcs, instantiate__if__one_level__taken);
    ATF_ADD_TEST_CASE(tcs, instantiate__if__one_level__not_taken);
    ATF_ADD_TEST_CASE(tcs, instantiate__if__multiple_levels__taken);
    ATF_ADD_TEST_CASE(tcs, instantiate__if__multiple_levels__not_taken);
    ATF_ADD_TEST_CASE(tcs, instantiate__loop__no_iterations);
    ATF_ADD_TEST_CASE(tcs, instantiate__loop__multiple_iterations);
    ATF_ADD_TEST_CASE(tcs, instantiate__loop__nested__no_iterations);
    ATF_ADD_TEST_CASE(tcs, instantiate__loop__nested__multiple_iterations);
    ATF_ADD_TEST_CASE(tcs, instantiate__loop__sequential);
    ATF_ADD_TEST_CASE(tcs, instantiate__loop__scoping);
    ATF_ADD_TEST_CASE(tcs, instantiate__mismatched_delimiters);
    ATF_ADD_TEST_CASE(tcs, instantiate__empty_statement);
    ATF_ADD_TEST_CASE(tcs, instantiate__unknown_statement);
    ATF_ADD_TEST_CASE(tcs, instantiate__invalid_narguments);

    ATF_ADD_TEST_CASE(tcs, instantiate__files__ok);
    ATF_ADD_TEST_CASE(tcs, instantiate__files__input_error);
    ATF_ADD_TEST_CASE(tcs, instantiate__files__output_error);
}
