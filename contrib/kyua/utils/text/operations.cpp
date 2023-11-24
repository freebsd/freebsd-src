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

#include <sstream>

#include "utils/format/macros.hpp"
#include "utils/sanity.hpp"

namespace text = utils::text;


/// Replaces XML special characters from an input string.
///
/// The list of XML special characters is specified here:
///     http://www.w3.org/TR/xml11/#charsets
///
/// \param in The input to quote.
///
/// \return A quoted string without any XML special characters.
std::string
text::escape_xml(const std::string& in)
{
    std::ostringstream quoted;

    for (std::string::const_iterator it = in.begin();
         it != in.end(); ++it) {
        unsigned char c = (unsigned char)*it;
        if (c == '"') {
            quoted << "&quot;";
        } else if (c == '&') {
            quoted << "&amp;";
        } else if (c == '<') {
            quoted << "&lt;";
        } else if (c == '>') {
            quoted << "&gt;";
        } else if (c == '\'') {
            quoted << "&apos;";
        } else if ((c >= 0x01 && c <= 0x08) ||
                   (c >= 0x0B && c <= 0x0C) ||
                   (c >= 0x0E && c <= 0x1F) ||
                   (c >= 0x7F && c <= 0x84) ||
                   (c >= 0x86 && c <= 0x9F)) {
            // for RestrictedChar characters, escape them
            // as '&amp;#[decimal ASCII value];'
            // so that in the XML file we will see the escaped
            // character.
            quoted << "&amp;#" << static_cast< std::string::size_type >(*it)
                   << ";";
        } else {
            quoted << *it;
        }
    }
    return quoted.str();
}


/// Surrounds a string with quotes, escaping the quote itself if needed.
///
/// \param text The string to quote.
/// \param quote The quote character to use.
///
/// \return The quoted string.
std::string
text::quote(const std::string& text, const char quote)
{
    std::ostringstream quoted;
    quoted << quote;

    std::string::size_type start_pos = 0;
    std::string::size_type last_pos = text.find(quote);
    while (last_pos != std::string::npos) {
        quoted << text.substr(start_pos, last_pos - start_pos) << '\\';
        start_pos = last_pos;
        last_pos = text.find(quote, start_pos + 1);
    }
    quoted << text.substr(start_pos);

    quoted << quote;
    return quoted.str();
}


/// Fills a paragraph to the specified length.
///
/// This preserves any sequence of spaces in the input and any possible
/// newlines.  Sequences of spaces may be split in half (and thus one space is
/// lost), but the rest of the spaces will be preserved as either trailing or
/// leading spaces.
///
/// \param input The string to refill.
/// \param target_width The width to refill the paragraph to.
///
/// \return The refilled paragraph as a sequence of independent lines.
std::vector< std::string >
text::refill(const std::string& input, const std::size_t target_width)
{
    std::vector< std::string > output;

    std::string::size_type start = 0;
    while (start < input.length()) {
        std::string::size_type width;
        if (start + target_width >= input.length())
            width = input.length() - start;
        else {
            if (input[start + target_width] == ' ') {
                width = target_width;
            } else {
                const std::string::size_type pos = input.find_last_of(
                    " ", start + target_width - 1);
                if (pos == std::string::npos || pos < start + 1) {
                    width = input.find_first_of(" ", start + target_width);
                    if (width == std::string::npos)
                        width = input.length() - start;
                    else
                        width -= start;
                } else {
                    width = pos - start;
                }
            }
        }
        INV(width != std::string::npos);
        INV(start + width <= input.length());
        INV(input[start + width] == ' ' || input[start + width] == '\0');
        output.push_back(input.substr(start, width));

        start += width + 1;
    }

    if (input.empty()) {
        INV(output.empty());
        output.push_back("");
    }

    return output;
}


/// Fills a paragraph to the specified length.
///
/// See the documentation for refill() for additional details.
///
/// \param input The string to refill.
/// \param target_width The width to refill the paragraph to.
///
/// \return The refilled paragraph as a string with embedded newlines.
std::string
text::refill_as_string(const std::string& input, const std::size_t target_width)
{
    return join(refill(input, target_width), "\n");
}


/// Replaces all occurrences of a substring in a string.
///
/// \param input The string in which to perform the replacement.
/// \param search The pattern to be replaced.
/// \param replacement The substring to replace search with.
///
/// \return A copy of input with the replacements performed.
std::string
text::replace_all(const std::string& input, const std::string& search,
                  const std::string& replacement)
{
    std::string output;

    std::string::size_type pos, lastpos = 0;
    while ((pos = input.find(search, lastpos)) != std::string::npos) {
        output += input.substr(lastpos, pos - lastpos);
        output += replacement;
        lastpos = pos + search.length();
    }
    output += input.substr(lastpos);

    return output;
}


/// Splits a string into different components.
///
/// \param str The string to split.
/// \param delimiter The separator to use to split the words.
///
/// \return The different words in the input string as split by the provided
/// delimiter.
std::vector< std::string >
text::split(const std::string& str, const char delimiter)
{
    std::vector< std::string > words;
    if (!str.empty()) {
        std::string::size_type pos = str.find(delimiter);
        words.push_back(str.substr(0, pos));
        while (pos != std::string::npos) {
            ++pos;
            const std::string::size_type next = str.find(delimiter, pos);
            words.push_back(str.substr(pos, next - pos));
            pos = next;
        }
    }
    return words;
}


/// Converts a string to a boolean.
///
/// \param str The string to convert.
///
/// \return The converted string, if the input string was valid.
///
/// \throw std::value_error If the input string does not represent a valid
///     boolean value.
template<>
bool
text::to_type(const std::string& str)
{
    if (str == "true")
        return true;
    else if (str == "false")
        return false;
    else
        throw value_error(F("Invalid boolean value '%s'") % str);
}


/// Identity function for to_type, for genericity purposes.
///
/// \param str The string to convert.
///
/// \return The input string.
template<>
std::string
text::to_type(const std::string& str)
{
    return str;
}
