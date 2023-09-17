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

/// \file utils/cmdline/exceptions.hpp
/// Exception types raised by the cmdline module.

#if !defined(UTILS_CMDLINE_EXCEPTIONS_HPP)
#define UTILS_CMDLINE_EXCEPTIONS_HPP

#include <stdexcept>
#include <string>

namespace utils {
namespace cmdline {


/// Base exception for cmdline errors.
class error : public std::runtime_error {
public:
    explicit error(const std::string&);
    ~error(void) throw();
};


/// Generic error to describe problems caused by the user.
class usage_error : public error {
public:
    explicit usage_error(const std::string&);
    ~usage_error(void) throw();
};


/// Error denoting that no argument was provided to an option that required one.
class missing_option_argument_error : public usage_error {
    /// Name of the option for which no required argument was specified.
    std::string _option;

public:
    explicit missing_option_argument_error(const std::string&);
    ~missing_option_argument_error(void) throw();

    const std::string& option(void) const;
};


/// Error denoting that the argument provided to an option is invalid.
class option_argument_value_error : public usage_error {
    /// Name of the option for which the argument was invalid.
    std::string _option;

    /// Raw value of the invalid user-provided argument.
    std::string _argument;

    /// Reason describing why the argument is invalid.
    std::string _reason;

public:
    explicit option_argument_value_error(const std::string&, const std::string&,
                                         const std::string&);
    ~option_argument_value_error(void) throw();

    const std::string& option(void) const;
    const std::string& argument(void) const;
};


/// Error denoting that the user specified an unknown option.
class unknown_option_error : public usage_error {
    /// Name of the option that was not known.
    std::string _option;

public:
    explicit unknown_option_error(const std::string&);
    ~unknown_option_error(void) throw();

    const std::string& option(void) const;
};


}  // namespace cmdline
}  // namespace utils


#endif  // !defined(UTILS_CMDLINE_EXCEPTIONS_HPP)
