// Copyright 2014 The Kyua Authors.
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

/// \file utils/process/operations.hpp
/// Collection of utilities for process management.

#if !defined(UTILS_PROCESS_OPERATIONS_HPP)
#define UTILS_PROCESS_OPERATIONS_HPP

#include "utils/process/operations_fwd.hpp"

#include "utils/defs.hpp"
#include "utils/fs/path_fwd.hpp"
#include "utils/process/status_fwd.hpp"

namespace utils {
namespace process {


void exec(const utils::fs::path&, const args_vector&) throw() UTILS_NORETURN;
void exec_unsafe(const utils::fs::path&, const args_vector&) UTILS_NORETURN;
void terminate_group(const int);
void terminate_self_with(const status&) UTILS_NORETURN;
status wait(const int);
status wait_any(void);


}  // namespace process
}  // namespace utils

#endif  // !defined(UTILS_PROCESS_OPERATIONS_HPP)
