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

/// \file utils/logging/macros.hpp
/// Convenience macros to simplify usage of the logging library.
///
/// This file <em>must not be included from other header files</em>.

#if !defined(UTILS_LOGGING_MACROS_HPP)
#define UTILS_LOGGING_MACROS_HPP

#include "utils/logging/operations.hpp"


/// Logs a debug message.
///
/// \param message The message to log.
#define LD(message) utils::logging::log(utils::logging::level_debug, \
                                        __FILE__, __LINE__, message)


/// Logs an error message.
///
/// \param message The message to log.
#define LE(message) utils::logging::log(utils::logging::level_error, \
                                        __FILE__, __LINE__, message)


/// Logs an informational message.
///
/// \param message The message to log.
#define LI(message) utils::logging::log(utils::logging::level_info, \
                                        __FILE__, __LINE__, message)


/// Logs a warning message.
///
/// \param message The message to log.
#define LW(message) utils::logging::log(utils::logging::level_warning, \
                                        __FILE__, __LINE__, message)


#endif  // !defined(UTILS_LOGGING_MACROS_HPP)
