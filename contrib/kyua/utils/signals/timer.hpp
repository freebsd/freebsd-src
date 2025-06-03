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

/// \file utils/signals/timer.hpp
/// Multiprogrammed support for timers.
///
/// The timer module and class implement a mechanism to program multiple timers
/// concurrently by using a deadline scheduler and leveraging the "single timer"
/// features of the underlying operating system.

#if !defined(UTILS_SIGNALS_TIMER_HPP)
#define UTILS_SIGNALS_TIMER_HPP

#include "utils/signals/timer_fwd.hpp"

#include <memory>

#include "utils/datetime_fwd.hpp"
#include "utils/noncopyable.hpp"

namespace utils {
namespace signals {


namespace detail {
void invoke_do_fired(timer*);
}  // namespace detail


/// Individual timer.
///
/// Asynchronously executes its callback() method, which can be overridden by
/// subclasses, when the timeout given at construction expires.
class timer : noncopyable {
    struct impl;

    /// Pointer to the shared internal implementation.
    std::unique_ptr< impl > _pimpl;

    friend void detail::invoke_do_fired(timer*);
    void do_fired(void);

protected:
    virtual void callback(void);

public:
    timer(const utils::datetime::delta&);
    virtual ~timer(void);

    const utils::datetime::timestamp& when(void) const;

    bool fired(void) const;

    void unprogram(void);
};


} // namespace signals
} // namespace utils

#endif // !defined(UTILS_SIGNALS_TIMER_HPP)
