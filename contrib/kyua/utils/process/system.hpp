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

/// \file utils/process/system.hpp
/// Indirection to perform system calls.
///
/// The indirections exposed in this file are provided to allow unit-testing of
/// particular system behaviors (e.g. failures).  The caller of a routine in
/// this library is allowed, for testing purposes only, to explicitly replace
/// the pointers in this file with custom functions to inject a particular
/// behavior into the library code.
///
/// Do not include this header from other header files.
///
/// It may be nice to go one step further and completely abstract the library
/// functions in here to provide exception-based error reporting.

#if !defined(UTILS_PROCESS_SYSTEM_HPP)
#define UTILS_PROCESS_SYSTEM_HPP

extern "C" {
#include <unistd.h>
}

namespace utils {
namespace process {
namespace detail {


extern int (*syscall_dup2)(const int, const int);
extern pid_t (*syscall_fork)(void);
extern int (*syscall_open)(const char*, const int, ...);
extern int (*syscall_pipe)(int[2]);
extern pid_t (*syscall_waitpid)(const pid_t, int*, const int);


}  // namespace detail
}  // namespace process
}  // namespace utils

#endif  // !defined(UTILS_PROCESS_SYSTEM_HPP)
