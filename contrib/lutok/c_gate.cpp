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

#include "c_gate.hpp"
#include "state.ipp"


/// Creates a new gateway to an existing C++ Lua state.
///
/// \param state_ The state to connect to.  This object must remain alive while
///     the newly-constructed state_c_gate is alive.
lutok::state_c_gate::state_c_gate(state& state_) :
    _state(state_)
{
}


/// Destructor.
///
/// Destroying this object has no implications on the life cycle of the Lua
/// state.  Only the corresponding state object controls when the Lua state is
/// closed.
lutok::state_c_gate::~state_c_gate(void)
{
}


/// Creates a C++ state for a C Lua state.
///
/// \warning The created state object does NOT own the C state.  You must take
/// care to properly destroy the input lua_State when you are done with it to
/// not leak resources.
///
/// \param raw_state The raw state to wrap temporarily.
///
/// \return The wrapped state without strong ownership on the input state.
lutok::state
lutok::state_c_gate::connect(lua_State* raw_state)
{
    return state(static_cast< void* >(raw_state));
}


/// Returns the C native Lua state.
///
/// \return A native lua_State object holding the Lua C API state.
lua_State*
lutok::state_c_gate::c_state(void)
{
    return static_cast< lua_State* >(_state.raw_state());
}
