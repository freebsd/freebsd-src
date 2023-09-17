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

#if defined(HAVE_TERMIOS_H)
#   include <termios.h>
#endif
#include <unistd.h>
}

#include <iostream>

#include "utils/cmdline/globals.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/optional.ipp"
#include "utils/text/operations.ipp"
#include "utils/text/table.hpp"

namespace cmdline = utils::cmdline;
namespace text = utils::text;

using utils::none;
using utils::optional;


/// Destructor for the class.
cmdline::ui::~ui(void)
{
}


/// Writes a single line to stderr.
///
/// The written line is printed as is, without being wrapped to fit within the
/// screen width.  If the caller wants to print more than one line, it shall
/// invoke this function once per line.
///
/// \param message The line to print.  Should not include a trailing newline
///     character.
/// \param newline Whether to append a newline to the message or not.
void
cmdline::ui::err(const std::string& message, const bool newline)
{
    LI(F("stderr: %s") % message);
    if (newline)
        std::cerr << message << "\n";
    else {
        std::cerr << message;
        std::cerr.flush();
    }
}


/// Writes a single line to stdout.
///
/// The written line is printed as is, without being wrapped to fit within the
/// screen width.  If the caller wants to print more than one line, it shall
/// invoke this function once per line.
///
/// \param message The line to print.  Should not include a trailing newline
///     character.
/// \param newline Whether to append a newline to the message or not.
void
cmdline::ui::out(const std::string& message, const bool newline)
{
    LI(F("stdout: %s") % message);
    if (newline)
        std::cout << message << "\n";
    else {
        std::cout << message;
        std::cout.flush();
    }
}


/// Queries the width of the screen.
///
/// This information comes first from the COLUMNS environment variable.  If not
/// present or invalid, and if the stdout of the current process is connected to
/// a terminal the width is deduced from the terminal itself.  Ultimately, if
/// all fails, none is returned.  This function shall not raise any errors.
///
/// Be aware that the results of this query are cached during execution.
/// Subsequent calls to this function will always return the same value even if
/// the terminal size has actually changed.
///
/// \todo Install a signal handler for SIGWINCH so that we can readjust our
/// knowledge of the terminal width when the user resizes the window.
///
/// \return The width of the screen if it was possible to determine it, or none
/// otherwise.
optional< std::size_t >
cmdline::ui::screen_width(void) const
{
    static bool done = false;
    static optional< std::size_t > width = none;

    if (!done) {
        const optional< std::string > columns = utils::getenv("COLUMNS");
        if (columns) {
            if (columns.get().length() > 0) {
                try {
                    width = utils::make_optional(
                        utils::text::to_type< std::size_t >(columns.get()));
                } catch (const utils::text::value_error& e) {
                    LD(F("Ignoring invalid value in COLUMNS variable: %s") %
                       e.what());
                }
            }
        }
        if (!width) {
            struct ::winsize ws;
            if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1)
                width = optional< std::size_t >(ws.ws_col);
        }

        if (width && width.get() >= 80)
            width.get() -= 5;

        done = true;
    }

    return width;
}


/// Writes a line to stdout.
///
/// The line is wrapped to fit on screen.
///
/// \param message The line to print, without the trailing newline character.
void
cmdline::ui::out_wrap(const std::string& message)
{
    const optional< std::size_t > max_width = screen_width();
    if (max_width) {
        const std::vector< std::string > lines = text::refill(
            message, max_width.get());
        for (std::vector< std::string >::const_iterator iter = lines.begin();
             iter != lines.end(); iter++)
            out(*iter);
    } else
        out(message);
}


/// Writes a line to stdout with a leading tag.
///
/// If the line does not fit on the current screen width, the line is broken
/// into pieces and the tag is repeated on every line.
///
/// \param tag The leading line tag.
/// \param message The message to be printed, without the trailing newline
///     character.
/// \param repeat If true, print the tag on every line; otherwise, indent the
///     text of all lines to match the width of the tag on the first line.
void
cmdline::ui::out_tag_wrap(const std::string& tag, const std::string& message,
                          const bool repeat)
{
    const optional< std::size_t > max_width = screen_width();
    if (max_width && max_width.get() > tag.length()) {
        const std::vector< std::string > lines = text::refill(
            message, max_width.get() - tag.length());
        for (std::vector< std::string >::const_iterator iter = lines.begin();
             iter != lines.end(); iter++) {
            if (repeat || iter == lines.begin())
                out(F("%s%s") % tag % *iter);
            else
                out(F("%s%s") % std::string(tag.length(), ' ') % *iter);
        }
    } else {
        out(F("%s%s") % tag % message);
    }
}


/// Writes a table to stdout.
///
/// \param table The table to write.
/// \param formatter The table formatter to use to convert the table to a
///     console representation.
/// \param prefix Text to prepend to all the lines of the output table.
void
cmdline::ui::out_table(const text::table& table,
                       text::table_formatter formatter,
                       const std::string& prefix)
{
    if (table.empty())
        return;

    const optional< std::size_t > max_width = screen_width();
    if (max_width)
        formatter.set_table_width(max_width.get() - prefix.length());

    const std::vector< std::string > lines = formatter.format(table);
    for (std::vector< std::string >::const_iterator iter = lines.begin();
         iter != lines.end(); ++iter)
        out(prefix + *iter);
}


/// Formats and prints an error message.
///
/// \param ui_ The user interface object used to print the message.
/// \param message The message to print.  Should not end with a newline
///     character.
void
cmdline::print_error(ui* ui_, const std::string& message)
{
    LE(message);
    ui_->err(F("%s: E: %s") % cmdline::progname() % message);
}


/// Formats and prints an informational message.
///
/// \param ui_ The user interface object used to print the message.
/// \param message The message to print.  Should not end with a newline
///     character.
void
cmdline::print_info(ui* ui_, const std::string& message)
{
    LI(message);
    ui_->err(F("%s: I: %s") % cmdline::progname() % message);
}


/// Formats and prints a warning message.
///
/// \param ui_ The user interface object used to print the message.
/// \param message The message to print.  Should not end with a newline
///     character.
void
cmdline::print_warning(ui* ui_, const std::string& message)
{
    LW(message);
    ui_->err(F("%s: W: %s") % cmdline::progname() % message);
}
