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

#include "engine/config.hpp"

#if defined(HAVE_CONFIG_H)
#   include "config.h"
#endif

#include <stdexcept>
#include <vector>

#include <atf-c++.hpp>

#include "engine/exceptions.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/parser.hpp"
#include "utils/config/tree.ipp"
#include "utils/passwd.hpp"

namespace config = utils::config;
namespace fs = utils::fs;
namespace passwd = utils::passwd;

using utils::none;
using utils::optional;


namespace {


/// Replaces the system user database with a fake one for testing purposes.
static void
set_mock_users(void)
{
    std::vector< passwd::user > users;
    users.push_back(passwd::user("user1", 100, 150));
    users.push_back(passwd::user("user2", 200, 250));
    passwd::set_mock_users_for_testing(users);
}


/// Checks that the default values of a config object match our expectations.
///
/// This fails the test case if any field of the input config object is not
/// what we expect.
///
/// \param config The configuration to validate.
static void
validate_defaults(const config::tree& config)
{
    ATF_REQUIRE_EQ(
        KYUA_ARCHITECTURE,
        config.lookup< config::string_node >("architecture"));

    ATF_REQUIRE_EQ(
        1,
        config.lookup< config::positive_int_node >("parallelism"));

    ATF_REQUIRE_EQ(
        KYUA_PLATFORM,
        config.lookup< config::string_node >("platform"));

    ATF_REQUIRE(!config.is_set("unprivileged_user"));

    ATF_REQUIRE(config.all_properties("test_suites").empty());
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(config__defaults);
ATF_TEST_CASE_BODY(config__defaults)
{
    const config::tree user_config = engine::default_config();
    validate_defaults(user_config);
}


ATF_TEST_CASE_WITHOUT_HEAD(config__set__parallelism);
ATF_TEST_CASE_BODY(config__set__parallelism)
{
    config::tree user_config = engine::default_config();
    user_config.set_string("parallelism", "8");
    ATF_REQUIRE_THROW_RE(
        config::error, "parallelism.*Must be a positive integer",
        user_config.set_string("parallelism", "0"));
    ATF_REQUIRE_THROW_RE(
        config::error, "parallelism.*Must be a positive integer",
        user_config.set_string("parallelism", "-1"));
}


ATF_TEST_CASE_WITHOUT_HEAD(config__load__defaults);
ATF_TEST_CASE_BODY(config__load__defaults)
{
    atf::utils::create_file("config", "syntax(2)\n");

    const config::tree user_config = engine::load_config(fs::path("config"));
    validate_defaults(user_config);
}


ATF_TEST_CASE_WITHOUT_HEAD(config__load__overrides);
ATF_TEST_CASE_BODY(config__load__overrides)
{
    set_mock_users();

    atf::utils::create_file(
        "config",
        "syntax(2)\n"
        "architecture = 'test-architecture'\n"
        "parallelism = 16\n"
        "platform = 'test-platform'\n"
        "unprivileged_user = 'user2'\n"
        "test_suites.mysuite.myvar = 'myvalue'\n");

    const config::tree user_config = engine::load_config(fs::path("config"));

    ATF_REQUIRE_EQ("test-architecture",
                   user_config.lookup_string("architecture"));
    ATF_REQUIRE_EQ("16",
                   user_config.lookup_string("parallelism"));
    ATF_REQUIRE_EQ("test-platform",
                   user_config.lookup_string("platform"));

    const passwd::user& user = user_config.lookup< engine::user_node >(
        "unprivileged_user");
    ATF_REQUIRE_EQ("user2", user.name);
    ATF_REQUIRE_EQ(200, user.uid);

    config::properties_map exp_test_suites;
    exp_test_suites["test_suites.mysuite.myvar"] = "myvalue";

    ATF_REQUIRE(exp_test_suites == user_config.all_properties("test_suites"));
}


ATF_TEST_CASE_WITHOUT_HEAD(config__load__lua_error);
ATF_TEST_CASE_BODY(config__load__lua_error)
{
    atf::utils::create_file("config", "this syntax is invalid\n");

    ATF_REQUIRE_THROW(engine::load_error, engine::load_config(
        fs::path("config")));
}


ATF_TEST_CASE_WITHOUT_HEAD(config__load__bad_syntax__version);
ATF_TEST_CASE_BODY(config__load__bad_syntax__version)
{
    atf::utils::create_file("config", "syntax(123)\n");

    ATF_REQUIRE_THROW_RE(engine::load_error,
                         "Unsupported config version 123",
                         engine::load_config(fs::path("config")));
}


ATF_TEST_CASE_WITHOUT_HEAD(config__load__missing_file);
ATF_TEST_CASE_BODY(config__load__missing_file)
{
    ATF_REQUIRE_THROW_RE(engine::load_error, "Load of 'missing' failed",
                         engine::load_config(fs::path("missing")));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, config__defaults);
    ATF_ADD_TEST_CASE(tcs, config__set__parallelism);
    ATF_ADD_TEST_CASE(tcs, config__load__defaults);
    ATF_ADD_TEST_CASE(tcs, config__load__overrides);
    ATF_ADD_TEST_CASE(tcs, config__load__lua_error);
    ATF_ADD_TEST_CASE(tcs, config__load__bad_syntax__version);
    ATF_ADD_TEST_CASE(tcs, config__load__missing_file);
}
