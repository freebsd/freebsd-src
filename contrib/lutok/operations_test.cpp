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

#include "operations.hpp"

#include <fstream>

#include <atf-c++.hpp>

#include "exceptions.hpp"
#include "state.ipp"
#include "test_utils.hpp"


namespace {


/// Addition function for injection into Lua.
///
/// \pre stack(-2) The first summand.
/// \pre stack(-1) The second summand.
/// \post stack(-1) The result of the sum.
///
/// \param state The Lua state.
///
/// \return The number of results (1).
static int
hook_add(lutok::state& state)
{
    state.push_integer(state.to_integer(-1) + state.to_integer(-2));
    return 1;
}


/// Multiplication function for injection into Lua.
///
/// \pre stack(-2) The first factor.
/// \pre stack(-1) The second factor.
/// \post stack(-1) The product.
///
/// \param state The Lua state.
///
/// \return The number of results (1).
static int
hook_multiply(lutok::state& state)
{
    state.push_integer(state.to_integer(-1) * state.to_integer(-2));
    return 1;
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(create_module__empty);
ATF_TEST_CASE_BODY(create_module__empty)
{
    lutok::state state;
    std::map< std::string, lutok::cxx_function > members;
    lutok::create_module(state, "my_math", members);

    state.open_base();
    lutok::do_string(state, "return next(my_math) == nil", 0, 1, 0);
    ATF_REQUIRE(state.to_boolean(-1));
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(create_module__one);
ATF_TEST_CASE_BODY(create_module__one)
{
    lutok::state state;
    std::map< std::string, lutok::cxx_function > members;
    members["add"] = hook_add;
    lutok::create_module(state, "my_math", members);

    lutok::do_string(state, "return my_math.add(10, 20)", 0, 1, 0);
    ATF_REQUIRE_EQ(30, state.to_integer(-1));
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(create_module__many);
ATF_TEST_CASE_BODY(create_module__many)
{
    lutok::state state;
    std::map< std::string, lutok::cxx_function > members;
    members["add"] = hook_add;
    members["multiply"] = hook_multiply;
    members["add2"] = hook_add;
    lutok::create_module(state, "my_math", members);

    lutok::do_string(state, "return my_math.add(10, 20)", 0, 1, 0);
    ATF_REQUIRE_EQ(30, state.to_integer(-1));
    lutok::do_string(state, "return my_math.multiply(10, 20)", 0, 1, 0);
    ATF_REQUIRE_EQ(200, state.to_integer(-1));
    lutok::do_string(state, "return my_math.add2(20, 30)", 0, 1, 0);
    ATF_REQUIRE_EQ(50, state.to_integer(-1));
    state.pop(3);
}


ATF_TEST_CASE_WITHOUT_HEAD(do_file__some_args);
ATF_TEST_CASE_BODY(do_file__some_args)
{
    std::ofstream output("test.lua");
    output << "local a1, a2 = ...\nreturn a1 * 2, a2 * 2\n";
    output.close();

    lutok::state state;
    state.push_integer(456);
    state.push_integer(3);
    state.push_integer(5);
    state.push_integer(123);
    ATF_REQUIRE_EQ(2, lutok::do_file(state, "test.lua", 3, -1, 0));
    ATF_REQUIRE_EQ(3, state.get_top());
    ATF_REQUIRE_EQ(456, state.to_integer(-3));
    ATF_REQUIRE_EQ(6, state.to_integer(-2));
    ATF_REQUIRE_EQ(10, state.to_integer(-1));
    state.pop(3);
}


ATF_TEST_CASE_WITHOUT_HEAD(do_file__any_results);
ATF_TEST_CASE_BODY(do_file__any_results)
{
    std::ofstream output("test.lua");
    output << "return 10, 20, 30\n";
    output.close();

    lutok::state state;
    ATF_REQUIRE_EQ(3, lutok::do_file(state, "test.lua", 0, -1, 0));
    ATF_REQUIRE_EQ(3, state.get_top());
    ATF_REQUIRE_EQ(10, state.to_integer(-3));
    ATF_REQUIRE_EQ(20, state.to_integer(-2));
    ATF_REQUIRE_EQ(30, state.to_integer(-1));
    state.pop(3);
}


ATF_TEST_CASE_WITHOUT_HEAD(do_file__no_results);
ATF_TEST_CASE_BODY(do_file__no_results)
{
    std::ofstream output("test.lua");
    output << "return 10, 20, 30\n";
    output.close();

    lutok::state state;
    ATF_REQUIRE_EQ(0, lutok::do_file(state, "test.lua", 0, 0, 0));
    ATF_REQUIRE_EQ(0, state.get_top());
}


ATF_TEST_CASE_WITHOUT_HEAD(do_file__many_results);
ATF_TEST_CASE_BODY(do_file__many_results)
{
    std::ofstream output("test.lua");
    output << "return 10, 20, 30\n";
    output.close();

    lutok::state state;
    ATF_REQUIRE_EQ(2, lutok::do_file(state, "test.lua", 0, 2, 0));
    ATF_REQUIRE_EQ(2, state.get_top());
    ATF_REQUIRE_EQ(10, state.to_integer(-2));
    ATF_REQUIRE_EQ(20, state.to_integer(-1));
    state.pop(2);
}


ATF_TEST_CASE_WITHOUT_HEAD(do_file__not_found);
ATF_TEST_CASE_BODY(do_file__not_found)
{
    lutok::state state;
    stack_balance_checker checker(state);
    ATF_REQUIRE_THROW_RE(lutok::file_not_found_error, "missing.lua",
                         lutok::do_file(state, "missing.lua", 0, 0, 0));
}


ATF_TEST_CASE_WITHOUT_HEAD(do_file__error);
ATF_TEST_CASE_BODY(do_file__error)
{
    std::ofstream output("test.lua");
    output << "a b c\n";
    output.close();

    lutok::state state;
    stack_balance_checker checker(state);
    ATF_REQUIRE_THROW_RE(lutok::error, "Failed to load Lua file 'test.lua'",
                         lutok::do_file(state, "test.lua", 0, 0, 0));
}


ATF_TEST_CASE_WITHOUT_HEAD(do_file__error_with_errfunc);
ATF_TEST_CASE_BODY(do_file__error_with_errfunc)
{
    std::ofstream output("test.lua");
    output << "unknown_function()\n";
    output.close();

    lutok::state state;
    lutok::eval(state, "function(message) return 'This is an error!' end", 1);
    {
        stack_balance_checker checker(state);
        ATF_REQUIRE_THROW_RE(lutok::error, "This is an error!",
                             lutok::do_file(state, "test.lua", 0, 0, -2));
    }
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(do_string__some_args);
ATF_TEST_CASE_BODY(do_string__some_args)
{
    lutok::state state;
    state.push_integer(456);
    state.push_integer(3);
    state.push_integer(5);
    state.push_integer(123);
    ATF_REQUIRE_EQ(2, lutok::do_string(
        state, "local a1, a2 = ...\nreturn a1 * 2, a2 * 2\n", 3, -1, 0));
    ATF_REQUIRE_EQ(3, state.get_top());
    ATF_REQUIRE_EQ(456, state.to_integer(-3));
    ATF_REQUIRE_EQ(6, state.to_integer(-2));
    ATF_REQUIRE_EQ(10, state.to_integer(-1));
    state.pop(3);
}


ATF_TEST_CASE_WITHOUT_HEAD(do_string__any_results);
ATF_TEST_CASE_BODY(do_string__any_results)
{
    lutok::state state;
    ATF_REQUIRE_EQ(3, lutok::do_string(state, "return 10, 20, 30", 0, -1, 0));
    ATF_REQUIRE_EQ(3, state.get_top());
    ATF_REQUIRE_EQ(10, state.to_integer(-3));
    ATF_REQUIRE_EQ(20, state.to_integer(-2));
    ATF_REQUIRE_EQ(30, state.to_integer(-1));
    state.pop(3);
}


ATF_TEST_CASE_WITHOUT_HEAD(do_string__no_results);
ATF_TEST_CASE_BODY(do_string__no_results)
{
    lutok::state state;
    ATF_REQUIRE_EQ(0, lutok::do_string(state, "return 10, 20, 30", 0, 0, 0));
    ATF_REQUIRE_EQ(0, state.get_top());
}


ATF_TEST_CASE_WITHOUT_HEAD(do_string__many_results);
ATF_TEST_CASE_BODY(do_string__many_results)
{
    lutok::state state;
    ATF_REQUIRE_EQ(2, lutok::do_string(state, "return 10, 20, 30", 0, 2, 0));
    ATF_REQUIRE_EQ(2, state.get_top());
    ATF_REQUIRE_EQ(10, state.to_integer(-2));
    ATF_REQUIRE_EQ(20, state.to_integer(-1));
    state.pop(2);
}


ATF_TEST_CASE_WITHOUT_HEAD(do_string__error);
ATF_TEST_CASE_BODY(do_string__error)
{
    lutok::state state;
    stack_balance_checker checker(state);
    ATF_REQUIRE_THROW_RE(lutok::error, "Failed to process Lua string 'a b c'",
                         lutok::do_string(state, "a b c", 0, 0, 0));
}


ATF_TEST_CASE_WITHOUT_HEAD(do_string__error_with_errfunc);
ATF_TEST_CASE_BODY(do_string__error_with_errfunc)
{
    lutok::state state;
    lutok::eval(state, "function(message) return 'This is an error!' end", 1);
    {
        stack_balance_checker checker(state);
        ATF_REQUIRE_THROW_RE(lutok::error, "This is an error!",
                             lutok::do_string(state, "unknown_function()",
                                              0, 0, -2));
    }
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(eval__one_result);
ATF_TEST_CASE_BODY(eval__one_result)
{
    lutok::state state;
    stack_balance_checker checker(state);
    lutok::eval(state, "3 + 10", 1);
    ATF_REQUIRE_EQ(13, state.to_integer(-1));
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(eval__many_results);
ATF_TEST_CASE_BODY(eval__many_results)
{
    lutok::state state;
    stack_balance_checker checker(state);
    lutok::eval(state, "5, 8, 10", 3);
    ATF_REQUIRE_EQ(5, state.to_integer(-3));
    ATF_REQUIRE_EQ(8, state.to_integer(-2));
    ATF_REQUIRE_EQ(10, state.to_integer(-1));
    state.pop(3);
}


ATF_TEST_CASE_WITHOUT_HEAD(eval__error);
ATF_TEST_CASE_BODY(eval__error)
{
    lutok::state state;
    stack_balance_checker checker(state);
    ATF_REQUIRE_THROW(lutok::error,
                      lutok::eval(state, "non_existent.method()", 1));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, create_module__empty);
    ATF_ADD_TEST_CASE(tcs, create_module__one);
    ATF_ADD_TEST_CASE(tcs, create_module__many);

    ATF_ADD_TEST_CASE(tcs, do_file__some_args);
    ATF_ADD_TEST_CASE(tcs, do_file__any_results);
    ATF_ADD_TEST_CASE(tcs, do_file__no_results);
    ATF_ADD_TEST_CASE(tcs, do_file__many_results);
    ATF_ADD_TEST_CASE(tcs, do_file__not_found);
    ATF_ADD_TEST_CASE(tcs, do_file__error);
    ATF_ADD_TEST_CASE(tcs, do_file__error_with_errfunc);

    ATF_ADD_TEST_CASE(tcs, do_string__some_args);
    ATF_ADD_TEST_CASE(tcs, do_string__any_results);
    ATF_ADD_TEST_CASE(tcs, do_string__no_results);
    ATF_ADD_TEST_CASE(tcs, do_string__many_results);
    ATF_ADD_TEST_CASE(tcs, do_string__error);
    ATF_ADD_TEST_CASE(tcs, do_string__error_with_errfunc);

    ATF_ADD_TEST_CASE(tcs, eval__one_result);
    ATF_ADD_TEST_CASE(tcs, eval__many_results);
    ATF_ADD_TEST_CASE(tcs, eval__error);
}
