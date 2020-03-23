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

#include "utils/text/table.hpp"

#include <algorithm>

#include <atf-c++.hpp>

#include "utils/text/operations.ipp"

namespace text = utils::text;


/// Performs a check on text::table_formatter.
///
/// This is provided for test simplicity's sake.  Having to match the result of
/// the formatting on a line by line basis would result in too verbose tests
/// (maybe not with C++11, but not using this yet).
///
/// Because of the flattening of the formatted table into a string, we risk
/// misdetecting problems when the algorithm bundles newlines into the lines of
/// a table.  This should not happen, and not accounting for this little detail
/// makes testing so much easier.
///
/// \param expected Textual representation of the table, as a collection of
///     lines separated by newline characters.
/// \param formatter The formatter to use.
/// \param table The table to format.
static void
table_formatter_check(const std::string& expected,
                      const text::table_formatter& formatter,
                      const text::table& table)
{
    ATF_REQUIRE_EQ(expected, text::join(formatter.format(table), "\n") + "\n");
}



ATF_TEST_CASE_WITHOUT_HEAD(table__ncolumns);
ATF_TEST_CASE_BODY(table__ncolumns)
{
    ATF_REQUIRE_EQ(5, text::table(5).ncolumns());
    ATF_REQUIRE_EQ(10, text::table(10).ncolumns());
}


ATF_TEST_CASE_WITHOUT_HEAD(table__column_width);
ATF_TEST_CASE_BODY(table__column_width)
{
    text::table_row row1;
    row1.push_back("1234");
    row1.push_back("123456");
    text::table_row row2;
    row2.push_back("12");
    row2.push_back("12345678");

    text::table table(2);
    table.add_row(row1);
    table.add_row(row2);

    ATF_REQUIRE_EQ(4, table.column_width(0));
    ATF_REQUIRE_EQ(8, table.column_width(1));
}


ATF_TEST_CASE_WITHOUT_HEAD(table__column_widths);
ATF_TEST_CASE_BODY(table__column_widths)
{
    text::table_row row1;
    row1.push_back("1234");
    row1.push_back("123456");
    text::table_row row2;
    row2.push_back("12");
    row2.push_back("12345678");

    text::table table(2);
    table.add_row(row1);
    table.add_row(row2);

    ATF_REQUIRE_EQ(4, table.column_widths()[0]);
    ATF_REQUIRE_EQ(8, table.column_widths()[1]);
}


ATF_TEST_CASE_WITHOUT_HEAD(table__empty);
ATF_TEST_CASE_BODY(table__empty)
{
    text::table table(2);
    ATF_REQUIRE(table.empty());
    table.add_row(text::table_row(2));
    ATF_REQUIRE(!table.empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(table__iterate);
ATF_TEST_CASE_BODY(table__iterate)
{
    text::table_row row1;
    row1.push_back("foo");
    text::table_row row2;
    row2.push_back("bar");

    text::table table(1);
    table.add_row(row1);
    table.add_row(row2);

    text::table::const_iterator iter = table.begin();
    ATF_REQUIRE(iter != table.end());
    ATF_REQUIRE(row1 == *iter);
    ++iter;
    ATF_REQUIRE(iter != table.end());
    ATF_REQUIRE(row2 == *iter);
    ++iter;
    ATF_REQUIRE(iter == table.end());
}


ATF_TEST_CASE_WITHOUT_HEAD(table_formatter__empty);
ATF_TEST_CASE_BODY(table_formatter__empty)
{
    ATF_REQUIRE(text::table_formatter().set_separator(" ")
                .format(text::table(1)).empty());
    ATF_REQUIRE(text::table_formatter().set_separator(" ")
                .format(text::table(10)).empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(table_formatter__defaults);
ATF_TEST_CASE_BODY(table_formatter__defaults)
{
    text::table table(3);
    {
        text::table_row row;
        row.push_back("First");
        row.push_back("Second");
        row.push_back("Third");
        table.add_row(row);
    }
    {
        text::table_row row;
        row.push_back("Fourth with some text");
        row.push_back("Fifth with some more text");
        row.push_back("Sixth foo");
        table.add_row(row);
    }

    table_formatter_check(
        "First                Second                   Third\n"
        "Fourth with some textFifth with some more textSixth foo\n",
        text::table_formatter(), table);
}


ATF_TEST_CASE_WITHOUT_HEAD(table_formatter__one_column__no_max_width);
ATF_TEST_CASE_BODY(table_formatter__one_column__no_max_width)
{
    text::table table(1);
    {
        text::table_row row;
        row.push_back("First row with some words");
        table.add_row(row);
    }
    {
        text::table_row row;
        row.push_back("Second row with some words");
        table.add_row(row);
    }

    table_formatter_check(
        "First row with some words\n"
        "Second row with some words\n",
        text::table_formatter().set_separator(" | "), table);
}


ATF_TEST_CASE_WITHOUT_HEAD(table_formatter__one_column__explicit_width);
ATF_TEST_CASE_BODY(table_formatter__one_column__explicit_width)
{
    text::table table(1);
    {
        text::table_row row;
        row.push_back("First row with some words");
        table.add_row(row);
    }
    {
        text::table_row row;
        row.push_back("Second row with some words");
        table.add_row(row);
    }

    table_formatter_check(
        "First row with some words\n"
        "Second row with some words\n",
        text::table_formatter().set_separator(" | ").set_column_width(0, 1024),
        table);
}


ATF_TEST_CASE_WITHOUT_HEAD(table_formatter__one_column__max_width);
ATF_TEST_CASE_BODY(table_formatter__one_column__max_width)
{
    text::table table(1);
    {
        text::table_row row;
        row.push_back("First row with some words");
        table.add_row(row);
    }
    {
        text::table_row row;
        row.push_back("Second row with some words");
        table.add_row(row);
    }

    table_formatter_check(
        "First row\nwith some\nwords\n"
        "Second row\nwith some\nwords\n",
        text::table_formatter().set_separator(" | ").set_table_width(11),
        table);
}


ATF_TEST_CASE_WITHOUT_HEAD(table_formatter__many_columns__no_max_width);
ATF_TEST_CASE_BODY(table_formatter__many_columns__no_max_width)
{
    text::table table(3);
    {
        text::table_row row;
        row.push_back("First");
        row.push_back("Second");
        row.push_back("Third");
        table.add_row(row);
    }
    {
        text::table_row row;
        row.push_back("Fourth with some text");
        row.push_back("Fifth with some more text");
        row.push_back("Sixth foo");
        table.add_row(row);
    }

    table_formatter_check(
        "First                 | Second                    | Third\n"
        "Fourth with some text | Fifth with some more text | Sixth foo\n",
        text::table_formatter().set_separator(" | "), table);
}


ATF_TEST_CASE_WITHOUT_HEAD(table_formatter__many_columns__explicit_width);
ATF_TEST_CASE_BODY(table_formatter__many_columns__explicit_width)
{
    text::table table(3);
    {
        text::table_row row;
        row.push_back("First");
        row.push_back("Second");
        row.push_back("Third");
        table.add_row(row);
    }
    {
        text::table_row row;
        row.push_back("Fourth with some text");
        row.push_back("Fifth with some more text");
        row.push_back("Sixth foo");
        table.add_row(row);
    }

    table_formatter_check(
        "First                   | Second                       | Third\n"
        "Fourth with some text   | Fifth with some more text    | Sixth foo\n",
        text::table_formatter().set_separator(" | ").set_column_width(0, 23)
        .set_column_width(1, 28), table);
}


ATF_TEST_CASE_WITHOUT_HEAD(table_formatter__many_columns__max_width);
ATF_TEST_CASE_BODY(table_formatter__many_columns__max_width)
{
    text::table table(3);
    {
        text::table_row row;
        row.push_back("First");
        row.push_back("Second");
        row.push_back("Third");
        table.add_row(row);
    }
    {
        text::table_row row;
        row.push_back("Fourth with some text");
        row.push_back("Fifth with some more text");
        row.push_back("Sixth foo");
        table.add_row(row);
    }

    table_formatter_check(
        "First                 | Second     | Third\n"
        "Fourth with some text | Fifth with | Sixth foo\n"
        "                      | some more  | \n"
        "                      | text       | \n",
        text::table_formatter().set_separator(" | ").set_table_width(46)
        .set_column_width(1, text::table_formatter::width_refill)
        .set_column_width(0, text::table_formatter::width_auto), table);

    table_formatter_check(
        "First                   | Second     | Third\n"
        "Fourth with some text   | Fifth with | Sixth foo\n"
        "                        | some more  | \n"
        "                        | text       | \n",
        text::table_formatter().set_separator(" | ").set_table_width(48)
        .set_column_width(1, text::table_formatter::width_refill)
        .set_column_width(0, 23), table);
}


ATF_TEST_CASE_WITHOUT_HEAD(table_formatter__use_case__cli_help);
ATF_TEST_CASE_BODY(table_formatter__use_case__cli_help)
{
    text::table options_table(2);
    {
        text::table_row row;
        row.push_back("-a a_value");
        row.push_back("This is the description of the first flag");
        options_table.add_row(row);
    }
    {
        text::table_row row;
        row.push_back("-b");
        row.push_back("And this is the text for the second flag");
        options_table.add_row(row);
    }

    text::table commands_table(2);
    {
        text::table_row row;
        row.push_back("first");
        row.push_back("This is the first command");
        commands_table.add_row(row);
    }
    {
        text::table_row row;
        row.push_back("second");
        row.push_back("And this is the second command");
        commands_table.add_row(row);
    }

    const text::widths_vector::value_type first_width =
        std::max(options_table.column_width(0), commands_table.column_width(0));

    table_formatter_check(
        "-a a_value  This is the description\n"
        "            of the first flag\n"
        "-b          And this is the text for\n"
        "            the second flag\n",
        text::table_formatter().set_separator("  ").set_table_width(36)
        .set_column_width(0, first_width)
        .set_column_width(1, text::table_formatter::width_refill),
        options_table);

    table_formatter_check(
        "first       This is the first\n"
        "            command\n"
        "second      And this is the second\n"
        "            command\n",
        text::table_formatter().set_separator("  ").set_table_width(36)
        .set_column_width(0, first_width)
        .set_column_width(1, text::table_formatter::width_refill),
        commands_table);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, table__ncolumns);
    ATF_ADD_TEST_CASE(tcs, table__column_width);
    ATF_ADD_TEST_CASE(tcs, table__column_widths);
    ATF_ADD_TEST_CASE(tcs, table__empty);
    ATF_ADD_TEST_CASE(tcs, table__iterate);

    ATF_ADD_TEST_CASE(tcs, table_formatter__empty);
    ATF_ADD_TEST_CASE(tcs, table_formatter__defaults);
    ATF_ADD_TEST_CASE(tcs, table_formatter__one_column__no_max_width);
    ATF_ADD_TEST_CASE(tcs, table_formatter__one_column__explicit_width);
    ATF_ADD_TEST_CASE(tcs, table_formatter__one_column__max_width);
    ATF_ADD_TEST_CASE(tcs, table_formatter__many_columns__no_max_width);
    ATF_ADD_TEST_CASE(tcs, table_formatter__many_columns__explicit_width);
    ATF_ADD_TEST_CASE(tcs, table_formatter__many_columns__max_width);
    ATF_ADD_TEST_CASE(tcs, table_formatter__use_case__cli_help);
}
