// Copyright 2011 Google Inc.
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

/// \file stack_cleaner.hpp
/// Provides the stack_cleaner class.

#if !defined(LUTOK_STACK_CLEANER_HPP)
#define LUTOK_STACK_CLEANER_HPP

#include <memory>

#include <lutok/state.hpp>

namespace lutok {


/// A RAII model for values on the Lua stack.
///
/// At creation time, the object records the current depth of the Lua stack and,
/// during destruction, restores the recorded depth by popping as many stack
/// entries as required.  As a corollary, the stack can only grow during the
/// lifetime of a stack_cleaner object (or shrink, but cannot become shorter
/// than the depth recorded at creation time).
///
/// Use this class as follows:
///
/// state s;
/// {
///     stack_cleaner cleaner1(s);
///     s.push_integer(3);
///     s.push_integer(5);
///     ... do stuff here ...
///     for (...) {
///         stack_cleaner cleaner2(s);
///         s.load_string("...");
///         s.pcall(0, 1, 0);
///         ... do stuff here ...
///     }
///     // cleaner2 destroyed; the result of pcall is gone.
/// }
/// // cleaner1 destroyed; the integers 3 and 5 are gone.
///
/// You must give a name to the instantiated objects even if they cannot be
/// accessed later.  Otherwise, the instance will be destroyed right away and
/// will not have the desired effect.
class stack_cleaner {
    struct impl;

    /// Pointer to the shared internal implementation.
    std::auto_ptr< impl > _pimpl;

    /// Disallow copies.
    stack_cleaner(const stack_cleaner&);

    /// Disallow assignment.
    stack_cleaner& operator=(const stack_cleaner&);

public:
    stack_cleaner(state&);
    ~stack_cleaner(void);

    void forget(void);
};


}  // namespace lutok

#endif  // !defined(LUTOK_STACK_CLEANER_HPP)
