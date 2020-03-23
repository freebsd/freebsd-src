// Copyright 2011 The Kyua Authors.
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

#include "model/context.hpp"

#include <map>
#include <sstream>
#include <string>

#include <atf-c++.hpp>

#include "utils/fs/path.hpp"

namespace fs = utils::fs;


ATF_TEST_CASE_WITHOUT_HEAD(ctor_and_getters);
ATF_TEST_CASE_BODY(ctor_and_getters)
{
    std::map< std::string, std::string > env;
    env["foo"] = "first";
    env["bar"] = "second";
    const model::context context(fs::path("/foo/bar"), env);
    ATF_REQUIRE_EQ(fs::path("/foo/bar"), context.cwd());
    ATF_REQUIRE(env == context.env());
}


ATF_TEST_CASE_WITHOUT_HEAD(operators_eq_and_ne);
ATF_TEST_CASE_BODY(operators_eq_and_ne)
{
    std::map< std::string, std::string > env;
    env["foo"] = "first";
    const model::context context1(fs::path("/foo/bar"), env);
    const model::context context2(fs::path("/foo/bar"), env);
    const model::context context3(fs::path("/foo/baz"), env);
    env["bar"] = "second";
    const model::context context4(fs::path("/foo/bar"), env);
    ATF_REQUIRE(  context1 == context2);
    ATF_REQUIRE(!(context1 != context2));
    ATF_REQUIRE(!(context1 == context3));
    ATF_REQUIRE(  context1 != context3);
    ATF_REQUIRE(!(context1 == context4));
    ATF_REQUIRE(  context1 != context4);
}


ATF_TEST_CASE_WITHOUT_HEAD(output__empty_env);
ATF_TEST_CASE_BODY(output__empty_env)
{
    const std::map< std::string, std::string > env;
    const model::context context(fs::path("/foo/bar"), env);

    std::ostringstream str;
    str << context;
    ATF_REQUIRE_EQ("context{cwd='/foo/bar', env=[]}", str.str());
}


ATF_TEST_CASE_WITHOUT_HEAD(output__some_env);
ATF_TEST_CASE_BODY(output__some_env)
{
    std::map< std::string, std::string > env;
    env["foo"] = "first";
    env["bar"] = "second' var";
    const model::context context(fs::path("/foo/bar"), env);

    std::ostringstream str;
    str << context;
    ATF_REQUIRE_EQ("context{cwd='/foo/bar', env=[bar='second\\' var', "
        "foo='first']}", str.str());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, ctor_and_getters);
    ATF_ADD_TEST_CASE(tcs, operators_eq_and_ne);
    ATF_ADD_TEST_CASE(tcs, output__empty_env);
    ATF_ADD_TEST_CASE(tcs, output__some_env);
}
