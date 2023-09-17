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

#include "utils/env.hpp"

#include <atf-c++.hpp>

#include "utils/fs/path.hpp"
#include "utils/optional.ipp"

namespace fs = utils::fs;

using utils::optional;


ATF_TEST_CASE_WITHOUT_HEAD(getallenv);
ATF_TEST_CASE_BODY(getallenv)
{
    utils::unsetenv("test-missing");
    utils::setenv("test-empty", "");
    utils::setenv("test-text", "some-value");

    const std::map< std::string, std::string > allenv = utils::getallenv();

    {
        const std::map< std::string, std::string >::const_iterator iter =
            allenv.find("test-missing");
        ATF_REQUIRE(iter == allenv.end());
    }

    {
        const std::map< std::string, std::string >::const_iterator iter =
            allenv.find("test-empty");
        ATF_REQUIRE(iter != allenv.end());
        ATF_REQUIRE((*iter).second.empty());
    }

    {
        const std::map< std::string, std::string >::const_iterator iter =
            allenv.find("test-text");
        ATF_REQUIRE(iter != allenv.end());
        ATF_REQUIRE_EQ("some-value", (*iter).second);
    }

    if (utils::getenv("PATH")) {
        const std::map< std::string, std::string >::const_iterator iter =
            allenv.find("PATH");
        ATF_REQUIRE(iter != allenv.end());
        ATF_REQUIRE_EQ(utils::getenv("PATH").get(), (*iter).second);
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(getenv);
ATF_TEST_CASE_BODY(getenv)
{
    const optional< std::string > path = utils::getenv("PATH");
    ATF_REQUIRE(path);
    ATF_REQUIRE(!path.get().empty());

    ATF_REQUIRE(!utils::getenv("__UNDEFINED_VARIABLE__"));
}


ATF_TEST_CASE_WITHOUT_HEAD(getenv_with_default);
ATF_TEST_CASE_BODY(getenv_with_default)
{
    ATF_REQUIRE("don't use" !=
                utils::getenv_with_default("PATH", "don't use"));

    ATF_REQUIRE_EQ("foo",
                   utils::getenv_with_default("__UNDEFINED_VARIABLE__", "foo"));
}


ATF_TEST_CASE_WITHOUT_HEAD(get_home__ok);
ATF_TEST_CASE_BODY(get_home__ok)
{
    const fs::path home("/foo/bar");
    utils::setenv("HOME", home.str());
    const optional< fs::path > computed = utils::get_home();
    ATF_REQUIRE(computed);
    ATF_REQUIRE_EQ(home, computed.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(get_home__missing);
ATF_TEST_CASE_BODY(get_home__missing)
{
    utils::unsetenv("HOME");
    ATF_REQUIRE(!utils::get_home());
}


ATF_TEST_CASE_WITHOUT_HEAD(get_home__invalid);
ATF_TEST_CASE_BODY(get_home__invalid)
{
    utils::setenv("HOME", "");
    ATF_REQUIRE(!utils::get_home());
}


ATF_TEST_CASE_WITHOUT_HEAD(setenv);
ATF_TEST_CASE_BODY(setenv)
{
    ATF_REQUIRE(utils::getenv("PATH"));
    const std::string oldval = utils::getenv("PATH").get();
    utils::setenv("PATH", "foo-bar");
    ATF_REQUIRE(utils::getenv("PATH").get() != oldval);
    ATF_REQUIRE_EQ("foo-bar", utils::getenv("PATH").get());

    ATF_REQUIRE(!utils::getenv("__UNDEFINED_VARIABLE__"));
    utils::setenv("__UNDEFINED_VARIABLE__", "foo2-bar2");
    ATF_REQUIRE_EQ("foo2-bar2", utils::getenv("__UNDEFINED_VARIABLE__").get());
}


ATF_TEST_CASE_WITHOUT_HEAD(unsetenv);
ATF_TEST_CASE_BODY(unsetenv)
{
    ATF_REQUIRE(utils::getenv("PATH"));
    utils::unsetenv("PATH");
    ATF_REQUIRE(!utils::getenv("PATH"));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, getallenv);

    ATF_ADD_TEST_CASE(tcs, getenv);

    ATF_ADD_TEST_CASE(tcs, getenv_with_default);

    ATF_ADD_TEST_CASE(tcs, get_home__ok);
    ATF_ADD_TEST_CASE(tcs, get_home__missing);
    ATF_ADD_TEST_CASE(tcs, get_home__invalid);

    ATF_ADD_TEST_CASE(tcs, setenv);

    ATF_ADD_TEST_CASE(tcs, unsetenv);
}
