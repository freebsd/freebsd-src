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

/// \file c_gate.hpp
/// Provides direct access to the C state of the Lua wrappers.

#if !defined(LUTOK_C_GATE_HPP)
#define LUTOK_C_GATE_HPP

#include <lua.hpp>

namespace lutok {


class state;


/// Gateway to the raw C state of Lua.
///
/// This class provides a mechanism to muck with the internals of the state
/// wrapper class.  Client code may wish to do so if Lutok is missing some
/// features of the performance of Lutok in a particular situation is not
/// reasonable.
///
/// \warning The use of this class is discouraged.  By using this class, you are
/// entering the world of unsafety.  Anything you do through the objects exposed
/// through this class will not be controlled by RAII patterns not validated in
/// any other way, so you can end up corrupting the Lua state and later get
/// crashes on otherwise perfectly-valid C++ code.
class state_c_gate {
    /// The C++ state that this class wraps.
    state& _state;

public:
    state_c_gate(state&);
    ~state_c_gate(void);

    static state connect(lua_State*);

    lua_State* c_state(void);
};


}  // namespace lutok

#endif  // !defined(LUTOK_C_GATE_HPP)
