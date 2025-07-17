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

/// \file utils/process/status.hpp
/// Provides the utils::process::status class.

#if !defined(UTILS_PROCESS_STATUS_HPP)
#define UTILS_PROCESS_STATUS_HPP

#include "utils/process/status_fwd.hpp"

#include <ostream>
#include <utility>

#include "utils/optional.ipp"

namespace utils {
namespace process {


/// Representation of the termination status of a process.
class status {
    /// The PID of the process that generated this status.
    ///
    /// Note that the process has exited already and been awaited for, so the
    /// PID cannot be used to address the process.
    int _dead_pid;

    /// The exit status of the process, if it exited cleanly.
    optional< int > _exited;

    /// The signal that terminated the program, if any, and if it dumped core.
    optional< std::pair< int, bool > > _signaled;

    status(const optional< int >&, const optional< std::pair< int, bool > >&);

public:
    status(const int, int);
    static status fake_exited(const int);
    static status fake_signaled(const int, const bool);

    int dead_pid(void) const;

    bool exited(void) const;
    int exitstatus(void) const;

    bool signaled(void) const;
    int termsig(void) const;
    bool coredump(void) const;
};


std::ostream& operator<<(std::ostream&, const status&);


}  // namespace process
}  // namespace utils

#endif  // !defined(UTILS_PROCESS_STATUS_HPP)
