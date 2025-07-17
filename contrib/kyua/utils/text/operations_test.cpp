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

#include "utils/text/operations.ipp"

#include <iostream>
#include <set>
#include <string>
#include <vector>

#include <atf-c++.hpp>

#include "utils/text/exceptions.hpp"

namespace text = utils::text;


namespace {


/// Tests text::refill() on an input string with a range of widths.
///
/// \param expected The expected refilled paragraph.
/// \param input The input paragraph to be refilled.
/// \param first_width The first width to validate.
/// \param last_width The last width to validate (inclusive).
static void
refill_test(const char* expected, const char* input,
            const std::size_t first_width, const std::size_t last_width)
{
    for (std::size_t width = first_width; width <= last_width; ++width) {
        const std::vector< std::string > lines = text::split(expected, '\n');
        std::cout << "Breaking at width " << width << '\n';
        ATF_REQUIRE_EQ(expected, text::refill_as_string(input, width));
        ATF_REQUIRE(lines == text::refill(input, width));
    }
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(escape_xml__empty);
ATF_TEST_CASE_BODY(escape_xml__empty)
{
    ATF_REQUIRE_EQ("", text::escape_xml(""));
}


ATF_TEST_CASE_WITHOUT_HEAD(escape_xml__no_escaping);
ATF_TEST_CASE_BODY(escape_xml__no_escaping)
{
    ATF_REQUIRE_EQ("a", text::escape_xml("a"));
    ATF_REQUIRE_EQ("Some text!", text::escape_xml("Some text!"));
    ATF_REQUIRE_EQ("\n\t\r", text::escape_xml("\n\t\r"));
}


ATF_TEST_CASE_WITHOUT_HEAD(escape_xml__some_escaping);
ATF_TEST_CASE_BODY(escape_xml__some_escaping)
{
    ATF_REQUIRE_EQ("&apos;", text::escape_xml("'"));

    ATF_REQUIRE_EQ("foo &quot;bar&amp; &lt;tag&gt; yay&apos; baz",
                   text::escape_xml("foo \"bar& <tag> yay' baz"));

    ATF_REQUIRE_EQ("&quot;&amp;&lt;&gt;&apos;", text::escape_xml("\"&<>'"));
    ATF_REQUIRE_EQ("&amp;&amp;&amp;", text::escape_xml("&&&"));
    ATF_REQUIRE_EQ("&amp;#8;&amp;#11;", text::escape_xml("\b\v"));
    ATF_REQUIRE_EQ("\t&amp;#127;BAR&amp;", text::escape_xml("\t\x7f""BAR&"));
}


ATF_TEST_CASE_WITHOUT_HEAD(quote__empty);
ATF_TEST_CASE_BODY(quote__empty)
{
    ATF_REQUIRE_EQ("''", text::quote("", '\''));
    ATF_REQUIRE_EQ("##", text::quote("", '#'));
}


ATF_TEST_CASE_WITHOUT_HEAD(quote__no_escaping);
ATF_TEST_CASE_BODY(quote__no_escaping)
{
    ATF_REQUIRE_EQ("'Some text\"'", text::quote("Some text\"", '\''));
    ATF_REQUIRE_EQ("#Another'string#", text::quote("Another'string", '#'));
}


ATF_TEST_CASE_WITHOUT_HEAD(quote__some_escaping);
ATF_TEST_CASE_BODY(quote__some_escaping)
{
    ATF_REQUIRE_EQ("'Some\\'text'", text::quote("Some'text", '\''));
    ATF_REQUIRE_EQ("#Some\\#text#", text::quote("Some#text", '#'));

    ATF_REQUIRE_EQ("'More than one\\' quote\\''",
                   text::quote("More than one' quote'", '\''));
    ATF_REQUIRE_EQ("'Multiple quotes \\'\\'\\' together'",
                   text::quote("Multiple quotes ''' together", '\''));

    ATF_REQUIRE_EQ("'\\'escape at the beginning'",
                   text::quote("'escape at the beginning", '\''));
    ATF_REQUIRE_EQ("'escape at the end\\''",
                   text::quote("escape at the end'", '\''));
}


ATF_TEST_CASE_WITHOUT_HEAD(refill__empty);
ATF_TEST_CASE_BODY(refill__empty)
{
    ATF_REQUIRE_EQ(1, text::refill("", 0).size());
    ATF_REQUIRE(text::refill("", 0)[0].empty());
    ATF_REQUIRE_EQ("", text::refill_as_string("", 0));

    ATF_REQUIRE_EQ(1, text::refill("", 10).size());
    ATF_REQUIRE(text::refill("", 10)[0].empty());
    ATF_REQUIRE_EQ("", text::refill_as_string("", 10));
}


ATF_TEST_CASE_WITHOUT_HEAD(refill__no_changes);
ATF_TEST_CASE_BODY(refill__no_changes)
{
    std::vector< std::string > exp_lines;
    exp_lines.push_back("foo bar\nbaz");

    ATF_REQUIRE(exp_lines == text::refill("foo bar\nbaz", 12));
    ATF_REQUIRE_EQ("foo bar\nbaz", text::refill_as_string("foo bar\nbaz", 12));

    ATF_REQUIRE(exp_lines == text::refill("foo bar\nbaz", 18));
    ATF_REQUIRE_EQ("foo bar\nbaz", text::refill_as_string("foo bar\nbaz", 80));
}


ATF_TEST_CASE_WITHOUT_HEAD(refill__break_one);
ATF_TEST_CASE_BODY(refill__break_one)
{
    refill_test("only break the\nfirst line", "only break the first line",
                14, 19);
}


ATF_TEST_CASE_WITHOUT_HEAD(refill__break_one__not_first_word);
ATF_TEST_CASE_BODY(refill__break_one__not_first_word)
{
    refill_test("first-long-word\nother\nwords", "first-long-word other words",
                6, 10);
    refill_test("first-long-word\nother words", "first-long-word other words",
                11, 20);
    refill_test("first-long-word other\nwords", "first-long-word other words",
                21, 26);
    refill_test("first-long-word other words", "first-long-word other words",
                27, 28);
}


ATF_TEST_CASE_WITHOUT_HEAD(refill__break_many);
ATF_TEST_CASE_BODY(refill__break_many)
{
    refill_test("this is a long\nparagraph to be\nsplit into\npieces",
                "this is a long paragraph to be split into pieces",
                15, 15);
}


ATF_TEST_CASE_WITHOUT_HEAD(refill__cannot_break);
ATF_TEST_CASE_BODY(refill__cannot_break)
{
    refill_test("this-is-a-long-string", "this-is-a-long-string", 5, 5);

    refill_test("this is\na-string-with-long-words",
                "this is a-string-with-long-words", 10, 10);
}


ATF_TEST_CASE_WITHOUT_HEAD(refill__preserve_whitespace);
ATF_TEST_CASE_BODY(refill__preserve_whitespace)
{
    refill_test("foo  bar baz  ", "foo  bar baz  ", 80, 80);
    refill_test("foo  \n bar", "foo    bar", 5, 5);

    std::vector< std::string > exp_lines;
    exp_lines.push_back("foo \n");
    exp_lines.push_back(" bar");
    ATF_REQUIRE(exp_lines == text::refill("foo \n  bar", 5));
    ATF_REQUIRE_EQ("foo \n\n bar", text::refill_as_string("foo \n  bar", 5));
}


ATF_TEST_CASE_WITHOUT_HEAD(join__empty);
ATF_TEST_CASE_BODY(join__empty)
{
    std::vector< std::string > lines;
    ATF_REQUIRE_EQ("", text::join(lines, " "));
}


ATF_TEST_CASE_WITHOUT_HEAD(join__one);
ATF_TEST_CASE_BODY(join__one)
{
    std::vector< std::string > lines;
    lines.push_back("first line");
    ATF_REQUIRE_EQ("first line", text::join(lines, "*"));
}


ATF_TEST_CASE_WITHOUT_HEAD(join__several);
ATF_TEST_CASE_BODY(join__several)
{
    std::vector< std::string > lines;
    lines.push_back("first abc");
    lines.push_back("second");
    lines.push_back("and last line");
    ATF_REQUIRE_EQ("first abc second and last line", text::join(lines, " "));
    ATF_REQUIRE_EQ("first abc***second***and last line",
                   text::join(lines, "***"));
}


ATF_TEST_CASE_WITHOUT_HEAD(join__unordered);
ATF_TEST_CASE_BODY(join__unordered)
{
    std::set< std::string > lines;
    lines.insert("first");
    lines.insert("second");
    const std::string joined = text::join(lines, " ");
    ATF_REQUIRE(joined == "first second" || joined == "second first");
}


ATF_TEST_CASE_WITHOUT_HEAD(split__empty);
ATF_TEST_CASE_BODY(split__empty)
{
    std::vector< std::string > words = text::split("", ' ');
    std::vector< std::string > exp_words;
    ATF_REQUIRE(exp_words == words);
}


ATF_TEST_CASE_WITHOUT_HEAD(split__one);
ATF_TEST_CASE_BODY(split__one)
{
    std::vector< std::string > words = text::split("foo", ' ');
    std::vector< std::string > exp_words;
    exp_words.push_back("foo");
    ATF_REQUIRE(exp_words == words);
}


ATF_TEST_CASE_WITHOUT_HEAD(split__several__simple);
ATF_TEST_CASE_BODY(split__several__simple)
{
    std::vector< std::string > words = text::split("foo bar baz", ' ');
    std::vector< std::string > exp_words;
    exp_words.push_back("foo");
    exp_words.push_back("bar");
    exp_words.push_back("baz");
    ATF_REQUIRE(exp_words == words);
}


ATF_TEST_CASE_WITHOUT_HEAD(split__several__delimiters);
ATF_TEST_CASE_BODY(split__several__delimiters)
{
    std::vector< std::string > words = text::split("XfooXXbarXXXbazXX", 'X');
    std::vector< std::string > exp_words;
    exp_words.push_back("");
    exp_words.push_back("foo");
    exp_words.push_back("");
    exp_words.push_back("bar");
    exp_words.push_back("");
    exp_words.push_back("");
    exp_words.push_back("baz");
    exp_words.push_back("");
    exp_words.push_back("");
    ATF_REQUIRE(exp_words == words);
}


ATF_TEST_CASE_WITHOUT_HEAD(replace_all__empty);
ATF_TEST_CASE_BODY(replace_all__empty)
{
    ATF_REQUIRE_EQ("", text::replace_all("", "search", "replacement"));
}


ATF_TEST_CASE_WITHOUT_HEAD(replace_all__none);
ATF_TEST_CASE_BODY(replace_all__none)
{
    ATF_REQUIRE_EQ("string without matches",
                   text::replace_all("string without matches",
                                     "WITHOUT", "replacement"));
}


ATF_TEST_CASE_WITHOUT_HEAD(replace_all__one);
ATF_TEST_CASE_BODY(replace_all__one)
{
    ATF_REQUIRE_EQ("string replacement matches",
                   text::replace_all("string without matches",
                                     "without", "replacement"));
}


ATF_TEST_CASE_WITHOUT_HEAD(replace_all__several);
ATF_TEST_CASE_BODY(replace_all__several)
{
    ATF_REQUIRE_EQ("OO fOO bar OOf baz OO",
                   text::replace_all("oo foo bar oof baz oo",
                                     "oo", "OO"));
}


ATF_TEST_CASE_WITHOUT_HEAD(to_type__ok__bool);
ATF_TEST_CASE_BODY(to_type__ok__bool)
{
    ATF_REQUIRE( text::to_type< bool >("true"));
    ATF_REQUIRE(!text::to_type< bool >("false"));
}


ATF_TEST_CASE_WITHOUT_HEAD(to_type__ok__numerical);
ATF_TEST_CASE_BODY(to_type__ok__numerical)
{
    ATF_REQUIRE_EQ(12, text::to_type< int >("12"));
    ATF_REQUIRE_EQ(18745, text::to_type< int >("18745"));
    ATF_REQUIRE_EQ(-12345, text::to_type< int >("-12345"));

    ATF_REQUIRE_EQ(12.0, text::to_type< double >("12"));
    ATF_REQUIRE_EQ(12.5, text::to_type< double >("12.5"));
}


ATF_TEST_CASE_WITHOUT_HEAD(to_type__ok__string);
ATF_TEST_CASE_BODY(to_type__ok__string)
{
    // While this seems redundant, having this particular specialization that
    // does nothing allows callers to delegate work to to_type without worrying
    // about the particular type being converted.
    ATF_REQUIRE_EQ("", text::to_type< std::string >(""));
    ATF_REQUIRE_EQ("  abcd  ", text::to_type< std::string >("  abcd  "));
}


ATF_TEST_CASE_WITHOUT_HEAD(to_type__empty);
ATF_TEST_CASE_BODY(to_type__empty)
{
    ATF_REQUIRE_THROW(text::value_error, text::to_type< int >(""));
}


ATF_TEST_CASE_WITHOUT_HEAD(to_type__invalid__bool);
ATF_TEST_CASE_BODY(to_type__invalid__bool)
{
    ATF_REQUIRE_THROW(text::value_error, text::to_type< bool >(""));
    ATF_REQUIRE_THROW(text::value_error, text::to_type< bool >("true "));
    ATF_REQUIRE_THROW(text::value_error, text::to_type< bool >("foo"));
}


ATF_TEST_CASE_WITHOUT_HEAD(to_type__invalid__numerical);
ATF_TEST_CASE_BODY(to_type__invalid__numerical)
{
    ATF_REQUIRE_THROW(text::value_error, text::to_type< int >(" 3"));
    ATF_REQUIRE_THROW(text::value_error, text::to_type< int >("3 "));
    ATF_REQUIRE_THROW(text::value_error, text::to_type< int >("3a"));
    ATF_REQUIRE_THROW(text::value_error, text::to_type< int >("a3"));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, escape_xml__empty);
    ATF_ADD_TEST_CASE(tcs, escape_xml__no_escaping);
    ATF_ADD_TEST_CASE(tcs, escape_xml__some_escaping);

    ATF_ADD_TEST_CASE(tcs, quote__empty);
    ATF_ADD_TEST_CASE(tcs, quote__no_escaping);
    ATF_ADD_TEST_CASE(tcs, quote__some_escaping);

    ATF_ADD_TEST_CASE(tcs, refill__empty);
    ATF_ADD_TEST_CASE(tcs, refill__no_changes);
    ATF_ADD_TEST_CASE(tcs, refill__break_one);
    ATF_ADD_TEST_CASE(tcs, refill__break_one__not_first_word);
    ATF_ADD_TEST_CASE(tcs, refill__break_many);
    ATF_ADD_TEST_CASE(tcs, refill__cannot_break);
    ATF_ADD_TEST_CASE(tcs, refill__preserve_whitespace);

    ATF_ADD_TEST_CASE(tcs, join__empty);
    ATF_ADD_TEST_CASE(tcs, join__one);
    ATF_ADD_TEST_CASE(tcs, join__several);
    ATF_ADD_TEST_CASE(tcs, join__unordered);

    ATF_ADD_TEST_CASE(tcs, split__empty);
    ATF_ADD_TEST_CASE(tcs, split__one);
    ATF_ADD_TEST_CASE(tcs, split__several__simple);
    ATF_ADD_TEST_CASE(tcs, split__several__delimiters);

    ATF_ADD_TEST_CASE(tcs, replace_all__empty);
    ATF_ADD_TEST_CASE(tcs, replace_all__none);
    ATF_ADD_TEST_CASE(tcs, replace_all__one);
    ATF_ADD_TEST_CASE(tcs, replace_all__several);

    ATF_ADD_TEST_CASE(tcs, to_type__ok__bool);
    ATF_ADD_TEST_CASE(tcs, to_type__ok__numerical);
    ATF_ADD_TEST_CASE(tcs, to_type__ok__string);
    ATF_ADD_TEST_CASE(tcs, to_type__empty);
    ATF_ADD_TEST_CASE(tcs, to_type__invalid__bool);
    ATF_ADD_TEST_CASE(tcs, to_type__invalid__numerical);
}
