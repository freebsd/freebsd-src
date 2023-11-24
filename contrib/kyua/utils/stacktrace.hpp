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

/// \file utils/stacktrace.hpp
/// Utilities to gather a stacktrace of a crashing binary.

#if !defined(ENGINE_STACKTRACE_HPP)
#define ENGINE_STACKTRACE_HPP

#include <ostream>

#include "utils/datetime_fwd.hpp"
#include "utils/fs/path_fwd.hpp"
#include "utils/optional_fwd.hpp"
#include "utils/process/executor_fwd.hpp"
#include "utils/process/status_fwd.hpp"

namespace utils {


extern const char* builtin_gdb;
extern utils::datetime::delta gdb_timeout;

utils::optional< utils::fs::path > find_gdb(void);

utils::optional< utils::fs::path > find_core(const utils::fs::path&,
                                             const utils::process::status&,
                                             const utils::fs::path&);

bool unlimit_core_size(void);

void dump_stacktrace(const utils::fs::path&,
                     utils::process::executor::executor_handle&,
                     const utils::process::executor::exit_handle&);

void dump_stacktrace_if_available(const utils::fs::path&,
                                  utils::process::executor::executor_handle&,
                                  const utils::process::executor::exit_handle&);


}  // namespace utils

#endif  // !defined(ENGINE_STACKTRACE_HPP)
