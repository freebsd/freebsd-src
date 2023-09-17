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

#include "store/write_backend.hpp"

#include <atf-c++.hpp>

#include "store/exceptions.hpp"
#include "store/metadata.hpp"
#include "utils/datetime.hpp"
#include "utils/env.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/operations.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/exceptions.hpp"
#include "utils/sqlite/statement.ipp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace logging = utils::logging;
namespace sqlite = utils::sqlite;


ATF_TEST_CASE(detail__initialize__ok);
ATF_TEST_CASE_HEAD(detail__initialize__ok)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(detail__initialize__ok)
{
    sqlite::database db = sqlite::database::in_memory();
    const datetime::timestamp before = datetime::timestamp::now();
    const store::metadata md = store::detail::initialize(db);
    const datetime::timestamp after = datetime::timestamp::now();

    ATF_REQUIRE(md.timestamp() >= before.to_seconds());
    ATF_REQUIRE(md.timestamp() <= after.to_microseconds());
    ATF_REQUIRE_EQ(store::detail::current_schema_version, md.schema_version());

    // Query some known tables to ensure they were created.
    db.exec("SELECT * FROM metadata");

    // And now query some know values.
    sqlite::statement stmt = db.create_statement(
        "SELECT COUNT(*) FROM metadata");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(1, stmt.column_int(0));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(detail__initialize__missing_schema);
ATF_TEST_CASE_BODY(detail__initialize__missing_schema)
{
    utils::setenv("KYUA_STOREDIR", "/non-existent");
    store::detail::current_schema_version = 712;

    sqlite::database db = sqlite::database::in_memory();
    ATF_REQUIRE_THROW_RE(store::error,
                         "Cannot read.*'/non-existent/schema_v712.sql'",
                         store::detail::initialize(db));
}


ATF_TEST_CASE_WITHOUT_HEAD(detail__initialize__sqlite_error);
ATF_TEST_CASE_BODY(detail__initialize__sqlite_error)
{
    utils::setenv("KYUA_STOREDIR", ".");
    store::detail::current_schema_version = 712;

    atf::utils::create_file("schema_v712.sql", "foo_bar_baz;\n");

    sqlite::database db = sqlite::database::in_memory();
    ATF_REQUIRE_THROW_RE(store::error, "Failed to initialize.*:.*foo_bar_baz",
                         store::detail::initialize(db));
}


ATF_TEST_CASE_WITHOUT_HEAD(detail__schema_file__builtin);
ATF_TEST_CASE_BODY(detail__schema_file__builtin)
{
    utils::unsetenv("KYUA_STOREDIR");
    ATF_REQUIRE_EQ(fs::path(KYUA_STOREDIR) / "schema_v3.sql",
                   store::detail::schema_file());
}


ATF_TEST_CASE_WITHOUT_HEAD(detail__schema_file__overriden);
ATF_TEST_CASE_BODY(detail__schema_file__overriden)
{
    utils::setenv("KYUA_STOREDIR", "/tmp/test");
    store::detail::current_schema_version = 123;
    ATF_REQUIRE_EQ(fs::path("/tmp/test/schema_v123.sql"),
                   store::detail::schema_file());
}


ATF_TEST_CASE(write_backend__open_rw__ok_if_empty);
ATF_TEST_CASE_HEAD(write_backend__open_rw__ok_if_empty)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(write_backend__open_rw__ok_if_empty)
{
    {
        sqlite::database db = sqlite::database::open(
            fs::path("test.db"), sqlite::open_readwrite | sqlite::open_create);
    }
    store::write_backend backend = store::write_backend::open_rw(
        fs::path("test.db"));
    backend.database().exec("SELECT * FROM metadata");
}


ATF_TEST_CASE(write_backend__open_rw__error_if_not_empty);
ATF_TEST_CASE_HEAD(write_backend__open_rw__error_if_not_empty)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(write_backend__open_rw__error_if_not_empty)
{
    {
        sqlite::database db = sqlite::database::open(
            fs::path("test.db"), sqlite::open_readwrite | sqlite::open_create);
        store::detail::initialize(db);
    }
    ATF_REQUIRE_THROW_RE(store::error, "test.db already exists",
                         store::write_backend::open_rw(fs::path("test.db")));
}


ATF_TEST_CASE(write_backend__open_rw__create_missing);
ATF_TEST_CASE_HEAD(write_backend__open_rw__create_missing)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(write_backend__open_rw__create_missing)
{
    store::write_backend backend = store::write_backend::open_rw(
        fs::path("test.db"));
    backend.database().exec("SELECT * FROM metadata");
}


ATF_TEST_CASE(write_backend__close);
ATF_TEST_CASE_HEAD(write_backend__close)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(write_backend__close)
{
    store::write_backend backend = store::write_backend::open_rw(
        fs::path("test.db"));
    backend.database().exec("SELECT * FROM metadata");
    backend.close();
    ATF_REQUIRE_THROW(utils::sqlite::error,
                      backend.database().exec("SELECT * FROM metadata"));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, detail__initialize__ok);
    ATF_ADD_TEST_CASE(tcs, detail__initialize__missing_schema);
    ATF_ADD_TEST_CASE(tcs, detail__initialize__sqlite_error);

    ATF_ADD_TEST_CASE(tcs, detail__schema_file__builtin);
    ATF_ADD_TEST_CASE(tcs, detail__schema_file__overriden);

    ATF_ADD_TEST_CASE(tcs, write_backend__open_rw__ok_if_empty);
    ATF_ADD_TEST_CASE(tcs, write_backend__open_rw__error_if_not_empty);
    ATF_ADD_TEST_CASE(tcs, write_backend__open_rw__create_missing);
    ATF_ADD_TEST_CASE(tcs, write_backend__close);
}
