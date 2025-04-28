// Copyright 2010 The Kyua Authors.
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

#include "utils/format/formatter.hpp"

#include <memory>
#include <string>
#include <utility>

#include "utils/format/exceptions.hpp"
#include "utils/sanity.hpp"
#include "utils/text/exceptions.hpp"
#include "utils/text/operations.ipp"

namespace format = utils::format;
namespace text = utils::text;


namespace {


/// Finds the next placeholder in a string.
///
/// \param format The original format string provided by the user; needed for
///     error reporting purposes only.
/// \param expansion The string containing the placeholder to look for.  Any
///     '%%' in the string will be skipped, and they must be stripped later by
///     strip_double_percent().
/// \param begin The position from which to start looking for the next
///     placeholder.
///
/// \return The position in the string in which the placeholder is located and
/// the placeholder itself.  If there are no placeholders left, this returns
/// the length of the string and an empty string.
///
/// \throw bad_format_error If the input string contains a trailing formatting
///     character.  We cannot detect any other kind of invalid formatter because
///     we do not implement a full parser for them.
static std::pair< std::string::size_type, std::string >
find_next_placeholder(const std::string& format,
                      const std::string& expansion,
                      std::string::size_type begin)
{
    begin = expansion.find('%', begin);
    while (begin != std::string::npos && expansion[begin + 1] == '%')
        begin = expansion.find('%', begin + 2);
    if (begin == std::string::npos)
        return std::make_pair(expansion.length(), "");
    if (begin == expansion.length() - 1)
        throw format::bad_format_error(format, "Trailing %");

    std::string::size_type end = begin + 1;
    while (end < expansion.length() && expansion[end] != 's')
        end++;
    const std::string placeholder = expansion.substr(begin, end - begin + 1);
    if (end == expansion.length() ||
        placeholder.find('%', 1) != std::string::npos)
        throw format::bad_format_error(format, "Unterminated placeholder '" +
                                       placeholder + "'");
    return std::make_pair(begin, placeholder);
}


/// Converts a string to an integer.
///
/// \param format The format string; for error reporting purposes only.
/// \param str The string to conver.
/// \param what The name of the field this integer belongs to; for error
///     reporting purposes only.
///
/// \return An integer representing the input string.
inline int
to_int(const std::string& format, const std::string& str, const char* what)
{
    try {
        return text::to_type< int >(str);
    } catch (const text::value_error& e) {
        throw format::bad_format_error(format, "Invalid " + std::string(what) +
                                       "specifier");
    }
}


/// Constructs an std::ostringstream based on a formatting placeholder.
///
/// \param format The format placeholder; may be empty.
///
/// \return A new std::ostringstream that is prepared to format a single
/// object in the manner specified by the format placeholder.
///
/// \throw bad_format_error If the format string is bad.  We do minimal
///     validation on this string though.
static std::ostringstream*
new_ostringstream(const std::string& format)
{
    std::unique_ptr< std::ostringstream > output(new std::ostringstream());

    if (format.length() <= 2) {
        // If the format is empty, we create a new stream so that we don't have
        // to check for NULLs later on.  We rarely should hit this condition
        // (and when we do it's a bug in the caller), so this is not a big deal.
        //
        // Otherwise, if the format is a regular '%s', then we don't have to do
        // any processing for additional formatters.  So this is just a "fast
        // path".
    } else {
        std::string partial = format.substr(1, format.length() - 2);
        if (partial[0] == '0') {
            output->fill('0');
            partial.erase(0, 1);
        }
        if (!partial.empty()) {
            const std::string::size_type dot = partial.find('.');
            if (dot != 0)
                output->width(to_int(format, partial.substr(0, dot), "width"));
            if (dot != std::string::npos) {
                output->setf(std::ios::fixed, std::ios::floatfield);
                output->precision(to_int(format, partial.substr(dot + 1),
                                         "precision"));
            }
        }
    }

    return output.release();
}


/// Replaces '%%' by '%' in a given string range.
///
/// \param in The input string to be rewritten.
/// \param begin The position at which to start the replacement.
/// \param end The position at which to end the replacement.
///
/// \return The modified string and the amount of characters removed.
static std::pair< std::string, int >
strip_double_percent(const std::string& in, const std::string::size_type begin,
                     std::string::size_type end)
{
    std::string part = in.substr(begin, end - begin);

    int removed = 0;
    std::string::size_type pos = part.find("%%");
    while (pos != std::string::npos) {
        part.erase(pos, 1);
        ++removed;
        pos = part.find("%%", pos + 1);
    }

    return std::make_pair(in.substr(0, begin) + part + in.substr(end), removed);
}


}  // anonymous namespace


/// Performs internal initialization of the formatter.
///
/// This is separate from the constructor just because it is shared by different
/// overloaded constructors.
void
format::formatter::init(void)
{
    const std::pair< std::string::size_type, std::string > placeholder =
        find_next_placeholder(_format, _expansion, _last_pos);
    const std::pair< std::string, int > no_percents =
        strip_double_percent(_expansion, _last_pos, placeholder.first);

    _oss = new_ostringstream(placeholder.second);

    _expansion = no_percents.first;
    _placeholder_pos = placeholder.first - no_percents.second;
    _placeholder = placeholder.second;
}


/// Constructs a new formatter object (internal).
///
/// \param format The format string.
/// \param expansion The format string with any replacements performed so far.
/// \param last_pos The position from which to start looking for formatting
///     placeholders.  This must be maintained in case one of the replacements
///     introduced a new placeholder, which must be ignored.  Think, for
///     example, replacing a "%s" string with "foo %s".
format::formatter::formatter(const std::string& format,
                             const std::string& expansion,
                             const std::string::size_type last_pos) :
    _format(format),
    _expansion(expansion),
    _last_pos(last_pos),
    _oss(NULL)
{
    init();
}


/// Constructs a new formatter object.
///
/// \param format The format string.  The formatters in the string are not
///     validated during construction, but will cause errors when used later if
///     they are invalid.
format::formatter::formatter(const std::string& format) :
    _format(format),
    _expansion(format),
    _last_pos(0),
    _oss(NULL)
{
    init();
}


format::formatter::~formatter(void)
{
    delete _oss;
}


/// Returns the formatted string.
///
/// \return A string representation of the formatted string.
const std::string&
format::formatter::str(void) const
{
    return _expansion;
}


/// Automatic conversion of formatter objects to strings.
///
/// This is provided to allow painless injection of formatter objects into
/// streams, without having to manually call the str() method.
format::formatter::operator const std::string&(void) const
{
    return _expansion;
}


/// Specialization of operator% for booleans.
///
/// \param value The boolean to inject into the format string.
///
/// \return A new formatter that has one less format placeholder.
format::formatter
format::formatter::operator%(const bool& value) const
{
    (*_oss) << (value ? "true" : "false");
    return replace(_oss->str());
}


/// Replaces the first formatting placeholder with a value.
///
/// \param arg The replacement string.
///
/// \return A new formatter in which the first formatting placeholder has been
///     replaced by arg and is ready to replace the next item.
///
/// \throw utils::format::extra_args_error If there are no more formatting
///     placeholders in the input string, or if the placeholder is invalid.
format::formatter
format::formatter::replace(const std::string& arg) const
{
    if (_placeholder_pos == _expansion.length())
        throw format::extra_args_error(_format, arg);

    const std::string expansion = _expansion.substr(0, _placeholder_pos)
        + arg + _expansion.substr(_placeholder_pos + _placeholder.length());
    return formatter(_format, expansion, _placeholder_pos + arg.length());
}
