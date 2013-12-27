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

extern "C" {
#include <sys/ioctl.h>

#include <termios.h>
#include <unistd.h>
}

#include <sstream>

#include "env.hpp"
#include "text.hpp"
#include "sanity.hpp"
#include "text.hpp"
#include "ui.hpp"

namespace impl = atf::ui;
#define IMPL_NAME "atf::ui"

static
size_t
terminal_width(void)
{
    static bool done = false;
    static size_t width = 0;

    if (!done) {
        if (atf::env::has("COLUMNS")) {
            const std::string cols = atf::env::get("COLUMNS");
            if (cols.length() > 0) {
                width = atf::text::to_type< size_t >(cols);
            }
        } else {
            struct winsize ws;
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1)
                width = ws.ws_col;
        }

        if (width >= 80)
            width -= 5;

        done = true;
    }

    return width;
}

static
std::string
format_paragraph(const std::string& text,
                 const std::string& tag,
                 const bool first,
                 const bool repeat,
                 const size_t col)
{
    PRE(text.find('\n') == std::string::npos);

    const std::string pad(col - tag.length(), ' ');
    const std::string fullpad(col, ' ');

    std::string formatted;
    if (first || repeat)
        formatted = tag + pad;
    else
        formatted = fullpad;
    INV(formatted.length() == col);
    size_t curcol = col;

    const size_t maxcol = terminal_width();

    std::vector< std::string > words = atf::text::split(text, " ");
    for (std::vector< std::string >::const_iterator iter = words.begin();
         iter != words.end(); iter++) {
        const std::string& word = *iter;

        if (iter != words.begin() && maxcol > 0 &&
            curcol + word.length() + 1 > maxcol) {
            if (repeat)
                formatted += '\n' + tag + pad;
            else
                formatted += '\n' + fullpad;
            curcol = col;
        } else if (iter != words.begin()) {
            formatted += ' ';
            curcol++;
        }

        formatted += word;
        curcol += word.length();
    }

    return formatted;
}

std::string
impl::format_error(const std::string& prog_name, const std::string& error)
{
    return format_text_with_tag("ERROR: " + error, prog_name + ": ", true);
}

std::string
impl::format_info(const std::string& prog_name, const std::string& msg)
{
    return format_text_with_tag(msg, prog_name + ": ", true);
}

std::string
impl::format_text(const std::string& text)
{
    return format_text_with_tag(text, "", false, 0);
}

std::string
impl::format_text_with_tag(const std::string& text, const std::string& tag,
                           bool repeat, size_t col)
{
    PRE(col == 0 || col >= tag.length());
    if (col == 0)
        col = tag.length();

    std::string formatted;

    std::vector< std::string > lines = atf::text::split(text, "\n");
    for (std::vector< std::string >::const_iterator iter = lines.begin();
         iter != lines.end(); iter++) {
        const std::string& line = *iter;

        formatted += format_paragraph(line, tag, iter == lines.begin(),
                                      repeat, col);
        if (iter + 1 != lines.end()) {
            if (repeat)
                formatted += "\n" + tag + "\n";
            else
                formatted += "\n\n";
        }
    }

    return formatted;
}

std::string
impl::format_warning(const std::string& prog_name, const std::string& error)
{
    return format_text_with_tag("WARNING: " + error, prog_name + ": ", true);
}
