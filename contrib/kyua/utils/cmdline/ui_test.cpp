// Copyright 2011 The Kyua Authors.
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

#include "utils/cmdline/ui.hpp"

#if defined(HAVE_CONFIG_H)
#   include "config.h"
#endif

extern "C" {
#include <sys/param.h>
#include <sys/ioctl.h>

#include <fcntl.h>
#if defined(HAVE_TERMIOS_H)
#   include <termios.h>
#endif
#include <unistd.h>
}

#include <cerrno>
#include <cstring>

#include <atf-c++.hpp>

#include "utils/cmdline/globals.hpp"
#include "utils/cmdline/ui_mock.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/optional.ipp"
#include "utils/text/table.hpp"

namespace cmdline = utils::cmdline;
namespace text = utils::text;

using utils::none;
using utils::optional;


namespace {


/// Reopens stdout as a tty and returns its width.
///
/// \return The width of the tty in columns.  If the width is wider than 80, the
/// result is 5 columns narrower to match the screen_width() algorithm.
static std::size_t
reopen_stdout(void)
{
    const int fd = ::open("/dev/tty", O_WRONLY);
    if (fd == -1)
        ATF_SKIP(F("Cannot open tty for test: %s") % ::strerror(errno));
    struct ::winsize ws;
    if (::ioctl(fd, TIOCGWINSZ, &ws) == -1)
        ATF_SKIP(F("Cannot determine size of tty: %s") % ::strerror(errno));

    if (fd != STDOUT_FILENO) {
        if (::dup2(fd, STDOUT_FILENO) == -1)
            ATF_SKIP(F("Failed to redirect stdout: %s") % ::strerror(errno));
        ::close(fd);
    }

    return ws.ws_col >= 80 ? ws.ws_col - 5 : ws.ws_col;
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(ui__screen_width__columns_set__no_tty);
ATF_TEST_CASE_BODY(ui__screen_width__columns_set__no_tty)
{
    utils::setenv("COLUMNS", "4321");
    ::close(STDOUT_FILENO);

    cmdline::ui ui;
    ATF_REQUIRE_EQ(4321 - 5, ui.screen_width().get());
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__screen_width__columns_set__tty);
ATF_TEST_CASE_BODY(ui__screen_width__columns_set__tty)
{
    utils::setenv("COLUMNS", "4321");
    (void)reopen_stdout();

    cmdline::ui ui;
    ATF_REQUIRE_EQ(4321 - 5, ui.screen_width().get());
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__screen_width__columns_empty__no_tty);
ATF_TEST_CASE_BODY(ui__screen_width__columns_empty__no_tty)
{
    utils::setenv("COLUMNS", "");
    ::close(STDOUT_FILENO);

    cmdline::ui ui;
    ATF_REQUIRE(!ui.screen_width());
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__screen_width__columns_empty__tty);
ATF_TEST_CASE_BODY(ui__screen_width__columns_empty__tty)
{
    utils::setenv("COLUMNS", "");
    const std::size_t columns = reopen_stdout();

    cmdline::ui ui;
    ATF_REQUIRE_EQ(columns, ui.screen_width().get());
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__screen_width__columns_invalid__no_tty);
ATF_TEST_CASE_BODY(ui__screen_width__columns_invalid__no_tty)
{
    utils::setenv("COLUMNS", "foo bar");
    ::close(STDOUT_FILENO);

    cmdline::ui ui;
    ATF_REQUIRE(!ui.screen_width());
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__screen_width__columns_invalid__tty);
ATF_TEST_CASE_BODY(ui__screen_width__columns_invalid__tty)
{
    utils::setenv("COLUMNS", "foo bar");
    const std::size_t columns = reopen_stdout();

    cmdline::ui ui;
    ATF_REQUIRE_EQ(columns, ui.screen_width().get());
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__screen_width__tty_is_file);
ATF_TEST_CASE_BODY(ui__screen_width__tty_is_file)
{
    utils::unsetenv("COLUMNS");
    const int fd = ::open("test.txt", O_WRONLY | O_CREAT | O_TRUNC, 0755);
    ATF_REQUIRE(fd != -1);
    if (fd != STDOUT_FILENO) {
        ATF_REQUIRE(::dup2(fd, STDOUT_FILENO) != -1);
        ::close(fd);
    }

    cmdline::ui ui;
    ATF_REQUIRE(!ui.screen_width());
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__screen_width__cached);
ATF_TEST_CASE_BODY(ui__screen_width__cached)
{
    cmdline::ui ui;

    utils::setenv("COLUMNS", "100");
    ATF_REQUIRE_EQ(100 - 5, ui.screen_width().get());

    utils::setenv("COLUMNS", "80");
    ATF_REQUIRE_EQ(100 - 5, ui.screen_width().get());

    utils::unsetenv("COLUMNS");
    ATF_REQUIRE_EQ(100 - 5, ui.screen_width().get());
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__err);
ATF_TEST_CASE_BODY(ui__err)
{
    cmdline::ui_mock ui(10);  // Keep shorter than message.
    ui.err("This is a short message");
    ATF_REQUIRE_EQ(1, ui.err_log().size());
    ATF_REQUIRE_EQ("This is a short message", ui.err_log()[0]);
    ATF_REQUIRE(ui.out_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__err__tolerates_newline);
ATF_TEST_CASE_BODY(ui__err__tolerates_newline)
{
    cmdline::ui_mock ui(10);  // Keep shorter than message.
    ui.err("This is a short message\n");
    ATF_REQUIRE_EQ(1, ui.err_log().size());
    ATF_REQUIRE_EQ("This is a short message\n", ui.err_log()[0]);
    ATF_REQUIRE(ui.out_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__out);
ATF_TEST_CASE_BODY(ui__out)
{
    cmdline::ui_mock ui(10);  // Keep shorter than message.
    ui.out("This is a short message");
    ATF_REQUIRE(ui.err_log().empty());
    ATF_REQUIRE_EQ(1, ui.out_log().size());
    ATF_REQUIRE_EQ("This is a short message", ui.out_log()[0]);
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__out__tolerates_newline);
ATF_TEST_CASE_BODY(ui__out__tolerates_newline)
{
    cmdline::ui_mock ui(10);  // Keep shorter than message.
    ui.out("This is a short message\n");
    ATF_REQUIRE(ui.err_log().empty());
    ATF_REQUIRE_EQ(1, ui.out_log().size());
    ATF_REQUIRE_EQ("This is a short message\n", ui.out_log()[0]);
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__out_wrap__no_refill);
ATF_TEST_CASE_BODY(ui__out_wrap__no_refill)
{
    cmdline::ui_mock ui(100);
    ui.out_wrap("This is a short message");
    ATF_REQUIRE(ui.err_log().empty());
    ATF_REQUIRE_EQ(1, ui.out_log().size());
    ATF_REQUIRE_EQ("This is a short message", ui.out_log()[0]);
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__out_wrap__refill);
ATF_TEST_CASE_BODY(ui__out_wrap__refill)
{
    cmdline::ui_mock ui(16);
    ui.out_wrap("This is a short message");
    ATF_REQUIRE(ui.err_log().empty());
    ATF_REQUIRE_EQ(2, ui.out_log().size());
    ATF_REQUIRE_EQ("This is a short", ui.out_log()[0]);
    ATF_REQUIRE_EQ("message", ui.out_log()[1]);
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__out_tag_wrap__no_refill);
ATF_TEST_CASE_BODY(ui__out_tag_wrap__no_refill)
{
    cmdline::ui_mock ui(100);
    ui.out_tag_wrap("Some long tag: ", "This is a short message");
    ATF_REQUIRE(ui.err_log().empty());
    ATF_REQUIRE_EQ(1, ui.out_log().size());
    ATF_REQUIRE_EQ("Some long tag: This is a short message", ui.out_log()[0]);
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__out_tag_wrap__refill__repeat);
ATF_TEST_CASE_BODY(ui__out_tag_wrap__refill__repeat)
{
    cmdline::ui_mock ui(32);
    ui.out_tag_wrap("Some long tag: ", "This is a short message");
    ATF_REQUIRE(ui.err_log().empty());
    ATF_REQUIRE_EQ(2, ui.out_log().size());
    ATF_REQUIRE_EQ("Some long tag: This is a short", ui.out_log()[0]);
    ATF_REQUIRE_EQ("Some long tag: message", ui.out_log()[1]);
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__out_tag_wrap__refill__no_repeat);
ATF_TEST_CASE_BODY(ui__out_tag_wrap__refill__no_repeat)
{
    cmdline::ui_mock ui(32);
    ui.out_tag_wrap("Some long tag: ", "This is a short message", false);
    ATF_REQUIRE(ui.err_log().empty());
    ATF_REQUIRE_EQ(2, ui.out_log().size());
    ATF_REQUIRE_EQ("Some long tag: This is a short", ui.out_log()[0]);
    ATF_REQUIRE_EQ("               message", ui.out_log()[1]);
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__out_tag_wrap__tag_too_long);
ATF_TEST_CASE_BODY(ui__out_tag_wrap__tag_too_long)
{
    cmdline::ui_mock ui(5);
    ui.out_tag_wrap("Some long tag: ", "This is a short message");
    ATF_REQUIRE(ui.err_log().empty());
    ATF_REQUIRE_EQ(1, ui.out_log().size());
    ATF_REQUIRE_EQ("Some long tag: This is a short message", ui.out_log()[0]);
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__out_table__empty);
ATF_TEST_CASE_BODY(ui__out_table__empty)
{
    const text::table table(3);

    text::table_formatter formatter;
    formatter.set_separator(" | ");
    formatter.set_column_width(0, 23);
    formatter.set_column_width(1, text::table_formatter::width_refill);

    cmdline::ui_mock ui(52);
    ui.out_table(table, formatter, "    ");
    ATF_REQUIRE(ui.out_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__out_table__not_empty);
ATF_TEST_CASE_BODY(ui__out_table__not_empty)
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

    text::table_formatter formatter;
    formatter.set_separator(" | ");
    formatter.set_column_width(0, 23);
    formatter.set_column_width(1, text::table_formatter::width_refill);

    cmdline::ui_mock ui(52);
    ui.out_table(table, formatter, "    ");
    ATF_REQUIRE_EQ(4, ui.out_log().size());
    ATF_REQUIRE_EQ("    First                   | Second     | Third",
                   ui.out_log()[0]);
    ATF_REQUIRE_EQ("    Fourth with some text   | Fifth with | Sixth foo",
                   ui.out_log()[1]);
    ATF_REQUIRE_EQ("                            | some more  | ",
                   ui.out_log()[2]);
    ATF_REQUIRE_EQ("                            | text       | ",
                   ui.out_log()[3]);
}


ATF_TEST_CASE_WITHOUT_HEAD(print_error);
ATF_TEST_CASE_BODY(print_error)
{
    cmdline::init("error-program");
    cmdline::ui_mock ui;
    cmdline::print_error(&ui, "The error.");
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE_EQ(1, ui.err_log().size());
    ATF_REQUIRE_EQ("error-program: E: The error.", ui.err_log()[0]);
}


ATF_TEST_CASE_WITHOUT_HEAD(print_info);
ATF_TEST_CASE_BODY(print_info)
{
    cmdline::init("info-program");
    cmdline::ui_mock ui;
    cmdline::print_info(&ui, "The info.");
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE_EQ(1, ui.err_log().size());
    ATF_REQUIRE_EQ("info-program: I: The info.", ui.err_log()[0]);
}


ATF_TEST_CASE_WITHOUT_HEAD(print_warning);
ATF_TEST_CASE_BODY(print_warning)
{
    cmdline::init("warning-program");
    cmdline::ui_mock ui;
    cmdline::print_warning(&ui, "The warning.");
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE_EQ(1, ui.err_log().size());
    ATF_REQUIRE_EQ("warning-program: W: The warning.", ui.err_log()[0]);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, ui__screen_width__columns_set__no_tty);
    ATF_ADD_TEST_CASE(tcs, ui__screen_width__columns_set__tty);
    ATF_ADD_TEST_CASE(tcs, ui__screen_width__columns_empty__no_tty);
    ATF_ADD_TEST_CASE(tcs, ui__screen_width__columns_empty__tty);
    ATF_ADD_TEST_CASE(tcs, ui__screen_width__columns_invalid__no_tty);
    ATF_ADD_TEST_CASE(tcs, ui__screen_width__columns_invalid__tty);
    ATF_ADD_TEST_CASE(tcs, ui__screen_width__tty_is_file);
    ATF_ADD_TEST_CASE(tcs, ui__screen_width__cached);

    ATF_ADD_TEST_CASE(tcs, ui__err);
    ATF_ADD_TEST_CASE(tcs, ui__err__tolerates_newline);
    ATF_ADD_TEST_CASE(tcs, ui__out);
    ATF_ADD_TEST_CASE(tcs, ui__out__tolerates_newline);

    ATF_ADD_TEST_CASE(tcs, ui__out_wrap__no_refill);
    ATF_ADD_TEST_CASE(tcs, ui__out_wrap__refill);
    ATF_ADD_TEST_CASE(tcs, ui__out_tag_wrap__no_refill);
    ATF_ADD_TEST_CASE(tcs, ui__out_tag_wrap__refill__repeat);
    ATF_ADD_TEST_CASE(tcs, ui__out_tag_wrap__refill__no_repeat);
    ATF_ADD_TEST_CASE(tcs, ui__out_tag_wrap__tag_too_long);
    ATF_ADD_TEST_CASE(tcs, ui__out_table__empty);
    ATF_ADD_TEST_CASE(tcs, ui__out_table__not_empty);

    ATF_ADD_TEST_CASE(tcs, print_error);
    ATF_ADD_TEST_CASE(tcs, print_info);
    ATF_ADD_TEST_CASE(tcs, print_warning);
}
