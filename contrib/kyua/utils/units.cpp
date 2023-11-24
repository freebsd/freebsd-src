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

#include "utils/units.hpp"

extern "C" {
#include <stdint.h>
}

#include <stdexcept>

#include "utils/format/macros.hpp"
#include "utils/text/exceptions.hpp"
#include "utils/text/operations.ipp"

namespace units = utils::units;


/// Constructs a zero bytes quantity.
units::bytes::bytes(void) :
    _count(0)
{
}


/// Constructs an arbitrary bytes quantity.
///
/// \param count_ The amount of bytes in the quantity.
units::bytes::bytes(const uint64_t count_) :
    _count(count_)
{
}


/// Parses a string into a bytes quantity.
///
/// \param in_str The user-provided string to be converted.
///
/// \return The converted bytes quantity.
///
/// \throw std::runtime_error If the input string is empty or invalid.
units::bytes
units::bytes::parse(const std::string& in_str)
{
    if (in_str.empty())
        throw std::runtime_error("Bytes quantity cannot be empty");

    uint64_t multiplier;
    std::string str = in_str;
    {
        const char unit = str[str.length() - 1];
        switch (unit) {
        case 'T': case 't': multiplier = TB; break;
        case 'G': case 'g': multiplier = GB; break;
        case 'M': case 'm': multiplier = MB; break;
        case 'K': case 'k': multiplier = KB; break;
        default: multiplier = 1;
        }
        if (multiplier != 1)
            str.erase(str.length() - 1);
    }

    if (str.empty())
        throw std::runtime_error("Bytes quantity cannot be empty");
    if (str[0] == '.' || str[str.length() - 1] == '.') {
        // The standard parser for float values accepts things like ".3" and
        // "3.", which means that we would interpret ".3K" and "3.K" as valid
        // quantities.  I think this is ugly and should not be allowed, so
        // special-case this condition and just error out.
        throw std::runtime_error(F("Invalid bytes quantity '%s'") % in_str);
    }

    double count;
    try {
        count = text::to_type< double >(str);
    } catch (const text::value_error& e) {
        throw std::runtime_error(F("Invalid bytes quantity '%s'") % in_str);
    }

    return bytes(uint64_t(count * multiplier));
}


/// Formats a bytes quantity for user consumption.
///
/// \return A textual representation of the bytes quantiy.
std::string
units::bytes::format(void) const
{
    if (_count >= TB) {
        return F("%.2sT") % (static_cast< float >(_count) / TB);
    } else if (_count >= GB) {
        return F("%.2sG") % (static_cast< float >(_count) / GB);
    } else if (_count >= MB) {
        return F("%.2sM") % (static_cast< float >(_count) / MB);
    } else if (_count >= KB) {
        return F("%.2sK") % (static_cast< float >(_count) / KB);
    } else {
        return F("%s") % _count;
    }
}


/// Implicit conversion to an integral representation.
units::bytes::operator uint64_t(void) const
{
    return _count;
}


/// Extracts a bytes quantity from a stream.
///
/// \param input The stream from which to read a single word representing the
///     bytes quantity.
/// \param rhs The variable into which to store the parsed value.
///
/// \return The input stream.
///
/// \post The bad bit of input is set to 1 if the parsing failed.
std::istream&
units::operator>>(std::istream& input, bytes& rhs)
{
    std::string word;
    input >> word;
    if (input.good() || input.eof()) {
        try {
            rhs = bytes::parse(word);
        } catch (const std::runtime_error& e) {
            input.setstate(std::ios::badbit);
        }
    }
    return input;
}


/// Injects a bytes quantity into a stream.
///
/// \param output The stream into which to inject the bytes quantity as a
///     user-readable string.
/// \param rhs The bytes quantity to format.
///
/// \return The output stream.
std::ostream&
units::operator<<(std::ostream& output, const bytes& rhs)
{
    return (output << rhs.format());
}
