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

/// \file utils/signals/interrupts.hpp
/// Handling of interrupts.

#if !defined(UTILS_SIGNALS_INTERRUPTS_HPP)
#define UTILS_SIGNALS_INTERRUPTS_HPP

#include "utils/signals/interrupts_fwd.hpp"

#include <unistd.h>

#include "utils/noncopyable.hpp"

namespace utils {
namespace signals {


/// Provides a scope in which interrupts can be detected and handled.
///
/// This RAII-modeled object installs signal handler when instantiated and
/// removes them upon destruction.  While this object is active, the
/// check_interrupt() free function can be used to determine if an interrupt has
/// happened.
class interrupts_handler : noncopyable {
    /// Whether the interrupts are still programmed or not.
    ///
    /// Used by the destructor to prevent double-unprogramming when unprogram()
    /// is explicitly called by the user.
    bool _programmed;

public:
    interrupts_handler(void);
    ~interrupts_handler(void);

    void unprogram(void);
};


/// Disables interrupts while the object is alive.
class interrupts_inhibiter : noncopyable {
public:
    interrupts_inhibiter(void);
    ~interrupts_inhibiter(void);
};


void check_interrupt(void);

void add_pid_to_kill(const pid_t);
void remove_pid_to_kill(const pid_t);


} // namespace signals
} // namespace utils

#endif // !defined(UTILS_SIGNALS_INTERRUPTS_HPP)
