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

#include "utils/passwd.hpp"

extern "C" {
#include <sys/wait.h>

#include <pwd.h>
#include <unistd.h>
}

#include <cstdlib>
#include <stdexcept>

#include <atf-c++.hpp>

namespace passwd_ns = utils::passwd;


ATF_TEST_CASE_WITHOUT_HEAD(user__public_fields);
ATF_TEST_CASE_BODY(user__public_fields)
{
    const passwd_ns::user user("the-name", 1, 2);
    ATF_REQUIRE_EQ("the-name", user.name);
    ATF_REQUIRE_EQ(1, user.uid);
    ATF_REQUIRE_EQ(2, user.gid);
}


ATF_TEST_CASE_WITHOUT_HEAD(user__is_root__true);
ATF_TEST_CASE_BODY(user__is_root__true)
{
    const passwd_ns::user user("i-am-root", 0, 10);
    ATF_REQUIRE(user.is_root());
}


ATF_TEST_CASE_WITHOUT_HEAD(user__is_root__false);
ATF_TEST_CASE_BODY(user__is_root__false)
{
    const passwd_ns::user user("i-am-not-root", 123, 10);
    ATF_REQUIRE(!user.is_root());
}


ATF_TEST_CASE_WITHOUT_HEAD(current_user);
ATF_TEST_CASE_BODY(current_user)
{
    const passwd_ns::user user = passwd_ns::current_user();
    ATF_REQUIRE_EQ(::getuid(), user.uid);
    ATF_REQUIRE_EQ(::getgid(), user.gid);
}


ATF_TEST_CASE_WITHOUT_HEAD(current_user__fake);
ATF_TEST_CASE_BODY(current_user__fake)
{
    const passwd_ns::user new_user("someone-else", ::getuid() + 1, 0);
    passwd_ns::set_current_user_for_testing(new_user);

    const passwd_ns::user user = passwd_ns::current_user();
    ATF_REQUIRE(::getuid() != user.uid);
    ATF_REQUIRE_EQ(new_user.uid, user.uid);
}


ATF_TEST_CASE_WITHOUT_HEAD(find_user_by_name__ok);
ATF_TEST_CASE_BODY(find_user_by_name__ok)
{
    const struct ::passwd* pw = ::getpwuid(::getuid());
    ATF_REQUIRE(pw != NULL);

    const passwd_ns::user user = passwd_ns::find_user_by_name(pw->pw_name);
    ATF_REQUIRE_EQ(::getuid(), user.uid);
    ATF_REQUIRE_EQ(::getgid(), user.gid);
    ATF_REQUIRE_EQ(pw->pw_name, user.name);
}


ATF_TEST_CASE_WITHOUT_HEAD(find_user_by_name__fail);
ATF_TEST_CASE_BODY(find_user_by_name__fail)
{
    ATF_REQUIRE_THROW_RE(std::runtime_error, "Failed.*user 'i-do-not-exist'",
                         passwd_ns::find_user_by_name("i-do-not-exist"));
}


ATF_TEST_CASE_WITHOUT_HEAD(find_user_by_name__fake);
ATF_TEST_CASE_BODY(find_user_by_name__fake)
{
    std::vector< passwd_ns::user > users;
    users.push_back(passwd_ns::user("myself2", 20, 40));
    users.push_back(passwd_ns::user("myself1", 10, 15));
    users.push_back(passwd_ns::user("myself3", 30, 60));
    passwd_ns::set_mock_users_for_testing(users);

    const passwd_ns::user user = passwd_ns::find_user_by_name("myself1");
    ATF_REQUIRE_EQ(10, user.uid);
    ATF_REQUIRE_EQ(15, user.gid);
    ATF_REQUIRE_EQ("myself1", user.name);

    ATF_REQUIRE_THROW_RE(std::runtime_error, "Failed.*user 'root'",
                         passwd_ns::find_user_by_name("root"));
}


ATF_TEST_CASE_WITHOUT_HEAD(find_user_by_uid__ok);
ATF_TEST_CASE_BODY(find_user_by_uid__ok)
{
    const passwd_ns::user user = passwd_ns::find_user_by_uid(::getuid());
    ATF_REQUIRE_EQ(::getuid(), user.uid);
    ATF_REQUIRE_EQ(::getgid(), user.gid);

    const struct ::passwd* pw = ::getpwuid(::getuid());
    ATF_REQUIRE(pw != NULL);
    ATF_REQUIRE_EQ(pw->pw_name, user.name);
}


ATF_TEST_CASE_WITHOUT_HEAD(find_user_by_uid__fake);
ATF_TEST_CASE_BODY(find_user_by_uid__fake)
{
    std::vector< passwd_ns::user > users;
    users.push_back(passwd_ns::user("myself2", 20, 40));
    users.push_back(passwd_ns::user("myself1", 10, 15));
    users.push_back(passwd_ns::user("myself3", 30, 60));
    passwd_ns::set_mock_users_for_testing(users);

    const passwd_ns::user user = passwd_ns::find_user_by_uid(10);
    ATF_REQUIRE_EQ(10, user.uid);
    ATF_REQUIRE_EQ(15, user.gid);
    ATF_REQUIRE_EQ("myself1", user.name);

    ATF_REQUIRE_THROW_RE(std::runtime_error, "Failed.*user.*UID 0",
                         passwd_ns::find_user_by_uid(0));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, user__public_fields);
    ATF_ADD_TEST_CASE(tcs, user__is_root__true);
    ATF_ADD_TEST_CASE(tcs, user__is_root__false);

    ATF_ADD_TEST_CASE(tcs, current_user);
    ATF_ADD_TEST_CASE(tcs, current_user__fake);

    ATF_ADD_TEST_CASE(tcs, find_user_by_name__ok);
    ATF_ADD_TEST_CASE(tcs, find_user_by_name__fail);
    ATF_ADD_TEST_CASE(tcs, find_user_by_name__fake);
    ATF_ADD_TEST_CASE(tcs, find_user_by_uid__ok);
    ATF_ADD_TEST_CASE(tcs, find_user_by_uid__fake);
}
