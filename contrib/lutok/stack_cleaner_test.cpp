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

#include "stack_cleaner.hpp"

#include <atf-c++.hpp>


ATF_TEST_CASE_WITHOUT_HEAD(empty);
ATF_TEST_CASE_BODY(empty)
{
    lutok::state state;
    {
        lutok::stack_cleaner cleaner(state);
        ATF_REQUIRE_EQ(0, state.get_top());
    }
    ATF_REQUIRE_EQ(0, state.get_top());
}


ATF_TEST_CASE_WITHOUT_HEAD(some);
ATF_TEST_CASE_BODY(some)
{
    lutok::state state;
    {
        lutok::stack_cleaner cleaner(state);
        state.push_integer(15);
        ATF_REQUIRE_EQ(1, state.get_top());
        state.push_integer(30);
        ATF_REQUIRE_EQ(2, state.get_top());
    }
    ATF_REQUIRE_EQ(0, state.get_top());
}


ATF_TEST_CASE_WITHOUT_HEAD(nested);
ATF_TEST_CASE_BODY(nested)
{
    lutok::state state;
    {
        lutok::stack_cleaner cleaner1(state);
        state.push_integer(10);
        ATF_REQUIRE_EQ(1, state.get_top());
        ATF_REQUIRE_EQ(10, state.to_integer(-1));
        {
            lutok::stack_cleaner cleaner2(state);
            state.push_integer(20);
            ATF_REQUIRE_EQ(2, state.get_top());
            ATF_REQUIRE_EQ(20, state.to_integer(-1));
            ATF_REQUIRE_EQ(10, state.to_integer(-2));
        }
        ATF_REQUIRE_EQ(1, state.get_top());
        ATF_REQUIRE_EQ(10, state.to_integer(-1));
    }
    ATF_REQUIRE_EQ(0, state.get_top());
}


ATF_TEST_CASE_WITHOUT_HEAD(forget);
ATF_TEST_CASE_BODY(forget)
{
    lutok::state state;
    {
        lutok::stack_cleaner cleaner(state);
        state.push_integer(15);
        state.push_integer(30);
        cleaner.forget();
        state.push_integer(60);
        ATF_REQUIRE_EQ(3, state.get_top());
    }
    ATF_REQUIRE_EQ(2, state.get_top());
    ATF_REQUIRE_EQ(30, state.to_integer(-1));
    state.pop(2);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, empty);
    ATF_ADD_TEST_CASE(tcs, some);
    ATF_ADD_TEST_CASE(tcs, nested);
    ATF_ADD_TEST_CASE(tcs, forget);
}
