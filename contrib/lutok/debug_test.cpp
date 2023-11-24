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

#include "debug.hpp"

#include <atf-c++.hpp>
#include <lua.hpp>

#include "state.ipp"
#include "test_utils.hpp"


ATF_TEST_CASE_WITHOUT_HEAD(get_info);
ATF_TEST_CASE_BODY(get_info)
{
    lutok::state state;
    ATF_REQUIRE(luaL_dostring(raw(state), "\n\nfunction hello() end\n"
                              "return hello") == 0);
    lutok::debug debug;
    debug.get_info(state, ">S");
    ATF_REQUIRE_EQ(3, debug.line_defined());
}


ATF_TEST_CASE_WITHOUT_HEAD(get_stack);
ATF_TEST_CASE_BODY(get_stack)
{
    lutok::state state;
    ATF_REQUIRE(luaL_dostring(raw(state), "error('Hello')") == 1);
    lutok::debug debug;
    debug.get_stack(state, 0);
    lua_pop(raw(state), 1);
    // Not sure if we can actually validate anything here, other than we did not
    // crash... (because get_stack only is supposed to update internal values of
    // the debug structure).
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, get_info);
    ATF_ADD_TEST_CASE(tcs, get_stack);
}
