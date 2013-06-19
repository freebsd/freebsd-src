//
// Automated Testing Framework (atf)
//
// Copyright (c) 2007 The NetBSD Foundation, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
// CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
// IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

extern "C" {
#include <sys/param.h>
#include <sys/types.h>
#include <limits.h>
#include <unistd.h>
}

#include <iostream>
#include <set>

#include "../atf-c++/macros.hpp"

#include "user.hpp"

// ------------------------------------------------------------------------
// Test cases for the free functions.
// ------------------------------------------------------------------------

ATF_TEST_CASE(euid);
ATF_TEST_CASE_HEAD(euid)
{
    set_md_var("descr", "Tests the euid function");
}
ATF_TEST_CASE_BODY(euid)
{
    using atf::atf_run::euid;

    ATF_REQUIRE_EQ(euid(), ::geteuid());
}

ATF_TEST_CASE(is_member_of_group);
ATF_TEST_CASE_HEAD(is_member_of_group)
{
    set_md_var("descr", "Tests the is_member_of_group function");
}
ATF_TEST_CASE_BODY(is_member_of_group)
{
    using atf::atf_run::is_member_of_group;

    std::set< gid_t > groups;
    gid_t maxgid = 0;
    {
        gid_t gids[NGROUPS_MAX];
        int ngids = ::getgroups(NGROUPS_MAX, gids);
        if (ngids == -1)
            ATF_FAIL("Call to ::getgroups failed");
        for (int i = 0; i < ngids; i++) {
            groups.insert(gids[i]);
            if (gids[i] > maxgid)
                maxgid = gids[i];
        }
        std::cout << "User belongs to " << ngids << " groups\n";
        std::cout << "Last GID is " << maxgid << "\n";
    }

    const gid_t maxgid_limit = 1 << 16;
    if (maxgid > maxgid_limit) {
        std::cout << "Test truncated from " << maxgid << " groups to "
                  << maxgid_limit << " to keep the run time reasonable "
            "enough\n";
        maxgid = maxgid_limit;
    }

    for (gid_t g = 0; g <= maxgid; g++) {
        if (groups.find(g) == groups.end()) {
            std::cout << "Checking if user does not belong to group "
                      << g << "\n";
            ATF_REQUIRE(!is_member_of_group(g));
        } else {
            std::cout << "Checking if user belongs to group " << g << "\n";
            ATF_REQUIRE(is_member_of_group(g));
        }
    }
}

ATF_TEST_CASE(is_root);
ATF_TEST_CASE_HEAD(is_root)
{
    set_md_var("descr", "Tests the is_root function");
}
ATF_TEST_CASE_BODY(is_root)
{
    using atf::atf_run::is_root;

    if (::geteuid() == 0) {
        ATF_REQUIRE(is_root());
    } else {
        ATF_REQUIRE(!is_root());
    }
}

ATF_TEST_CASE(is_unprivileged);
ATF_TEST_CASE_HEAD(is_unprivileged)
{
    set_md_var("descr", "Tests the is_unprivileged function");
}
ATF_TEST_CASE_BODY(is_unprivileged)
{
    using atf::atf_run::is_unprivileged;

    if (::geteuid() != 0) {
        ATF_REQUIRE(is_unprivileged());
    } else {
        ATF_REQUIRE(!is_unprivileged());
    }
}

// ------------------------------------------------------------------------
// Main.
// ------------------------------------------------------------------------

ATF_INIT_TEST_CASES(tcs)
{
    // Add the tests for the free functions.
    ATF_ADD_TEST_CASE(tcs, euid);
    ATF_ADD_TEST_CASE(tcs, is_member_of_group);
    ATF_ADD_TEST_CASE(tcs, is_root);
    ATF_ADD_TEST_CASE(tcs, is_unprivileged);
}
