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

#include <atf-c++.hpp>
#include <lua.hpp>

#include "state.ipp"
#include "test_utils.hpp"


ATF_TEST_CASE_WITHOUT_HEAD(connect);
ATF_TEST_CASE_BODY(connect)
{
    lua_State* raw_state = luaL_newstate();
    ATF_REQUIRE(raw_state != NULL);

    {
        lutok::state state = lutok::state_c_gate::connect(raw_state);
        lua_pushinteger(raw(state), 123);
    }
    // If the wrapper object had closed the Lua state, we could very well crash
    // here.
    ATF_REQUIRE_EQ(123, lua_tointeger(raw_state, -1));

    lua_close(raw_state);
}


ATF_TEST_CASE_WITHOUT_HEAD(c_state);
ATF_TEST_CASE_BODY(c_state)
{
    lutok::state state;
    state.push_integer(5);
    {
        lutok::state_c_gate gate(state);
        lua_State* raw_state = gate.c_state();
        ATF_REQUIRE_EQ(5, lua_tointeger(raw_state, -1));
    }
    state.pop(1);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, c_state);
    ATF_ADD_TEST_CASE(tcs, connect);
}
