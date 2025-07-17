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

#include "store/migrate.hpp"

extern "C" {
#include <sys/stat.h>
}

#include <atf-c++.hpp>

#include "store/exceptions.hpp"
#include "utils/env.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"

namespace fs = utils::fs;


ATF_TEST_CASE_WITHOUT_HEAD(detail__backup_database__ok);
ATF_TEST_CASE_BODY(detail__backup_database__ok)
{
    atf::utils::create_file("test.db", "The DB\n");
    store::detail::backup_database(fs::path("test.db"), 13);
    ATF_REQUIRE(fs::exists(fs::path("test.db")));
    ATF_REQUIRE(fs::exists(fs::path("test.db.v13.backup")));
    ATF_REQUIRE(atf::utils::compare_file("test.db.v13.backup", "The DB\n"));
}


ATF_TEST_CASE_WITHOUT_HEAD(detail__backup_database__ok_overwrite);
ATF_TEST_CASE_BODY(detail__backup_database__ok_overwrite)
{
    atf::utils::create_file("test.db", "Original contents");
    atf::utils::create_file("test.db.v1.backup", "Overwrite me");
    store::detail::backup_database(fs::path("test.db"), 1);
    ATF_REQUIRE(fs::exists(fs::path("test.db")));
    ATF_REQUIRE(fs::exists(fs::path("test.db.v1.backup")));
    ATF_REQUIRE(atf::utils::compare_file("test.db.v1.backup",
                                         "Original contents"));
}


ATF_TEST_CASE_WITHOUT_HEAD(detail__backup_database__fail_open);
ATF_TEST_CASE_BODY(detail__backup_database__fail_open)
{
    ATF_REQUIRE_THROW_RE(store::error, "Cannot open.*foo.db",
                         store::detail::backup_database(fs::path("foo.db"), 5));
}


ATF_TEST_CASE_WITH_CLEANUP(detail__backup_database__fail_create);
ATF_TEST_CASE_HEAD(detail__backup_database__fail_create)
{
    set_md_var("require.user", "unprivileged");
}
ATF_TEST_CASE_BODY(detail__backup_database__fail_create)
{
    ATF_REQUIRE(::mkdir("dir", 0755) != -1);
    atf::utils::create_file("dir/test.db", "Does not need to be valid");
    ATF_REQUIRE(::chmod("dir", 0111) != -1);
    ATF_REQUIRE_THROW_RE(
        store::error, "Cannot create.*dir/test.db.v13.backup",
        store::detail::backup_database(fs::path("dir/test.db"), 13));
}
ATF_TEST_CASE_CLEANUP(detail__backup_database__fail_create)
{
    if (::chmod("dir", 0755) == -1) {
        // If we cannot restore the original permissions, we cannot do much
        // more.  However, leaving an unwritable directory behind will cause the
        // runtime engine to report us as broken.
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(detail__migration_file__builtin);
ATF_TEST_CASE_BODY(detail__migration_file__builtin)
{
    utils::unsetenv("KYUA_STOREDIR");
    ATF_REQUIRE_EQ(fs::path(KYUA_STOREDIR) / "migrate_v5_v9.sql",
                   store::detail::migration_file(5, 9));
}


ATF_TEST_CASE_WITHOUT_HEAD(detail__migration_file__overriden);
ATF_TEST_CASE_BODY(detail__migration_file__overriden)
{
    utils::setenv("KYUA_STOREDIR", "/tmp/test");
    ATF_REQUIRE_EQ(fs::path("/tmp/test/migrate_v5_v9.sql"),
                   store::detail::migration_file(5, 9));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, detail__backup_database__ok);
    ATF_ADD_TEST_CASE(tcs, detail__backup_database__ok_overwrite);
    ATF_ADD_TEST_CASE(tcs, detail__backup_database__fail_open);
    ATF_ADD_TEST_CASE(tcs, detail__backup_database__fail_create);

    ATF_ADD_TEST_CASE(tcs, detail__migration_file__builtin);
    ATF_ADD_TEST_CASE(tcs, detail__migration_file__overriden);

    // Tests for migrate_schema are in schema_inttest.  This is because, for
    // such tests to be meaningful, they need to be integration tests and don't
    // really fit the goal of this unit-test module.
}
