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

#include <cassert>
#include <cstring>

#include "stack_cleaner.hpp"
#include "state.ipp"


/// Internal implementation for lutok::stack_cleaner.
struct lutok::stack_cleaner::impl {
    /// Reference to the Lua state this stack_cleaner refers to.
    state& state_ref;

    /// The depth of the Lua stack to be restored.
    unsigned int original_depth;

    /// Constructor.
    ///
    /// \param state_ref_ Reference to the Lua state.
    /// \param original_depth_ The depth of the Lua stack.
    impl(state& state_ref_, const unsigned int original_depth_) :
        state_ref(state_ref_),
        original_depth(original_depth_)
    {
    }
};


/// Creates a new stack cleaner.
///
/// This gathers the current height of the stack so that extra elements can be
/// popped during destruction.
///
/// \param state_ The Lua state.
lutok::stack_cleaner::stack_cleaner(state& state_) :
    _pimpl(new impl(state_, state_.get_top()))
{
}


/// Pops any values from the stack not known at construction time.
///
/// \pre The current height of the stack must be equal or greater to the height
/// of the stack when this object was instantiated.
lutok::stack_cleaner::~stack_cleaner(void)
{
    const unsigned int current_depth = _pimpl->state_ref.get_top();
    assert(current_depth >= _pimpl->original_depth);
    const unsigned int diff = current_depth - _pimpl->original_depth;
    if (diff > 0)
        _pimpl->state_ref.pop(diff);
}


/// Forgets about any elements currently in the stack.
///
/// This allows a function to return values on the stack because all the
/// elements that are currently in the stack will not be touched during
/// destruction when the function is called.
void
lutok::stack_cleaner::forget(void)
{
    _pimpl->original_depth = _pimpl->state_ref.get_top();
}
