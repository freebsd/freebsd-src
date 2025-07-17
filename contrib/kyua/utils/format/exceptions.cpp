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

#include "utils/format/exceptions.hpp"

using utils::format::bad_format_error;
using utils::format::error;
using utils::format::extra_args_error;


/// Constructs a new error with a plain-text message.
///
/// \param message The plain-text error message.
error::error(const std::string& message) :
    std::runtime_error(message)
{
}


/// Destructor for the error.
error::~error(void) throw()
{
}


/// Constructs a new bad_format_error.
///
/// \param format_ The invalid format string.
/// \param message Description of the error in the format string.
bad_format_error::bad_format_error(const std::string& format_,
                                   const std::string& message) :
    error("Invalid formatting string '" + format_ + "': " + message),
    _format(format_)
{
}


/// Destructor for the error.
bad_format_error::~bad_format_error(void) throw()
{
}


/// \return The format string that caused the error.
const std::string&
bad_format_error::format(void) const
{
    return _format;
}


/// Constructs a new extra_args_error.
///
/// \param format_ The format string.
/// \param arg_ The first extra argument passed to the format string.
extra_args_error::extra_args_error(const std::string& format_,
                                   const std::string& arg_) :
    error("Not enough fields in formatting string '" + format_ + "' to place "
          "argument '" + arg_ + "'"),
    _format(format_),
    _arg(arg_)
{
}


/// Destructor for the error.
extra_args_error::~extra_args_error(void) throw()
{
}


/// \return The format string that was passed too many arguments.
const std::string&
extra_args_error::format(void) const
{
    return _format;
}


/// \return The first argument that caused the error.
const std::string&
extra_args_error::arg(void) const
{
    return _arg;
}
