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

/// \file utils/process/isolation.hpp
/// Utilities to isolate a process.
///
/// By "isolation" in this context we mean forcing a process to run in a
/// more-or-less deterministic environment.

#if !defined(UTILS_PROCESS_ISOLATION_HPP)
#define UTILS_PROCESS_ISOLATION_HPP

#include "utils/fs/path_fwd.hpp"
#include "utils/optional_fwd.hpp"
#include "utils/passwd_fwd.hpp"

namespace utils {
namespace process {


extern const int exit_isolation_failure;


void isolate_child(const utils::optional< utils::passwd::user >&,
                   const utils::fs::path&);

void isolate_path(const utils::optional< utils::passwd::user >&,
                  const utils::fs::path&);


}  // namespace process
}  // namespace utils


#endif  // !defined(UTILS_PROCESS_ISOLATION_HPP)
