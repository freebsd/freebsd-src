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

#include "utils/cmdline/exceptions.hpp"

#include "utils/format/macros.hpp"
#include "utils/sanity.hpp"

namespace cmdline = utils::cmdline;


#define VALIDATE_OPTION_NAME(option) PRE_MSG( \
    (option.length() == 2 && (option[0] == '-' && option[1] != '-')) || \
    (option.length() > 2 && (option[0] == '-' && option[1] == '-')), \
    F("The option name %s must be fully specified") % option);


/// Constructs a new error with a plain-text message.
///
/// \param message The plain-text error message.
cmdline::error::error(const std::string& message) :
    std::runtime_error(message)
{
}


/// Destructor for the error.
cmdline::error::~error(void) throw()
{
}


/// Constructs a new usage_error.
///
/// \param message The reason behind the usage error.
cmdline::usage_error::usage_error(const std::string& message) :
    error(message)
{
}


/// Destructor for the error.
cmdline::usage_error::~usage_error(void) throw()
{
}


/// Constructs a new missing_option_argument_error.
///
/// \param option_ The option for which no argument was provided.  The option
///     name must be fully specified (with - or -- in front).
cmdline::missing_option_argument_error::missing_option_argument_error(
    const std::string& option_) :
    usage_error(F("Missing required argument for option %s") % option_),
    _option(option_)
{
    VALIDATE_OPTION_NAME(option_);
}


/// Destructor for the error.
cmdline::missing_option_argument_error::~missing_option_argument_error(void)
    throw()
{
}


/// Returns the option name for which no argument was provided.
///
/// \return The option name.
const std::string&
cmdline::missing_option_argument_error::option(void) const
{
    return _option;
}


/// Constructs a new option_argument_value_error.
///
/// \param option_ The option to which an invalid argument was passed.  The
///     option name must be fully specified (with - or -- in front).
/// \param argument_ The invalid argument.
/// \param reason_ The reason describing why the argument is invalid.
cmdline::option_argument_value_error::option_argument_value_error(
    const std::string& option_, const std::string& argument_,
    const std::string& reason_) :
    usage_error(F("Invalid argument '%s' for option %s: %s") % argument_ %
                option_ % reason_),
    _option(option_),
    _argument(argument_),
    _reason(reason_)
{
    VALIDATE_OPTION_NAME(option_);
}


/// Destructor for the error.
cmdline::option_argument_value_error::~option_argument_value_error(void)
    throw()
{
}


/// Returns the option to which the invalid argument was passed.
///
/// \return The option name.
const std::string&
cmdline::option_argument_value_error::option(void) const
{
    return _option;
}


/// Returns the invalid argument value.
///
/// \return The invalid argument.
const std::string&
cmdline::option_argument_value_error::argument(void) const
{
    return _argument;
}


/// Constructs a new unknown_option_error.
///
/// \param option_ The unknown option. The option name must be fully specified
///     (with - or -- in front).
cmdline::unknown_option_error::unknown_option_error(
    const std::string& option_) :
    usage_error(F("Unknown option %s") % option_),
    _option(option_)
{
    VALIDATE_OPTION_NAME(option_);
}


/// Destructor for the error.
cmdline::unknown_option_error::~unknown_option_error(void) throw()
{
}


/// Returns the unknown option name.
///
/// \return The unknown option.
const std::string&
cmdline::unknown_option_error::option(void) const
{
    return _option;
}
