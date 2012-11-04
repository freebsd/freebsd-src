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

#if !defined(_ATF_CXX_CONFIG_HPP_)
#define _ATF_CXX_CONFIG_HPP_

#include <map>
#include <string>

namespace atf {

namespace config {

//!
//! \brief Gets a build-time configuration variable's value.
//!
//! Given the name of a build-time configuration variable, returns its
//! textual value.  The user is free to override these by setting their
//! corresponding environment variables.  Therefore always use this
//! interface to get the value of these variables.
//!
//! \pre The variable must exist.
//!
const std::string& get(const std::string&);

//!
//! \brief Returns all the build-time configuration variables.
//!
//! Returns a name to value map containing all build-time configuration
//! variables.
//!
const std::map< std::string, std::string >& get_all(void);

//!
//! \brief Checks whether a build-time configuration variable exists.
//!
//! Given the name of a build-time configuration variable, checks
//! whether it is defined and returns a boolean indicating this
//! condition.  The program only has to use this function to sanity-check
//! a variable name provided by the user.  Otherwise it can assume that
//! the variables are defined.
//!
bool has(const std::string&);

} // namespace config

} // namespace atf

#endif // !defined(_ATF_CXX_CONFIG_HPP_)
