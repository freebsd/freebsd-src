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

#include "store/metadata.hpp"

#include <atf-c++.hpp>

#include "store/exceptions.hpp"
#include "store/write_backend.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/operations.hpp"
#include "utils/sqlite/database.hpp"

namespace logging = utils::logging;
namespace sqlite = utils::sqlite;


namespace {


/// Creates a test in-memory database.
///
/// When using this function, you must define a 'require.files' property in this
/// case pointing to store::detail::schema_file().
///
/// The database created by this function mimics a real complete database, but
/// without any predefined values.  I.e. for our particular case, the metadata
/// table is empty.
///
/// \return A SQLite database instance.
static sqlite::database
create_database(void)
{
    sqlite::database db = sqlite::database::in_memory();
    store::detail::initialize(db);
    db.exec("DELETE FROM metadata");
    return db;
}


}  // anonymous namespace


ATF_TEST_CASE(fetch_latest__ok);
ATF_TEST_CASE_HEAD(fetch_latest__ok)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(fetch_latest__ok)
{
    sqlite::database db = create_database();
    db.exec("INSERT INTO metadata (schema_version, timestamp) "
            "VALUES (512, 5678)");
    db.exec("INSERT INTO metadata (schema_version, timestamp) "
            "VALUES (256, 1234)");

    const store::metadata metadata = store::metadata::fetch_latest(db);
    ATF_REQUIRE_EQ(5678L, metadata.timestamp());
    ATF_REQUIRE_EQ(512, metadata.schema_version());
}


ATF_TEST_CASE(fetch_latest__empty_metadata);
ATF_TEST_CASE_HEAD(fetch_latest__empty_metadata)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(fetch_latest__empty_metadata)
{
    sqlite::database db = create_database();
    ATF_REQUIRE_THROW_RE(store::integrity_error, "metadata.*empty",
                         store::metadata::fetch_latest(db));
}


ATF_TEST_CASE_WITHOUT_HEAD(fetch_latest__no_timestamp);
ATF_TEST_CASE_BODY(fetch_latest__no_timestamp)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE metadata (schema_version INTEGER)");
    db.exec("INSERT INTO metadata VALUES (3)");

    ATF_REQUIRE_THROW_RE(store::integrity_error,
                         "Invalid metadata.*timestamp",
                         store::metadata::fetch_latest(db));
}


ATF_TEST_CASE_WITHOUT_HEAD(fetch_latest__no_schema_version);
ATF_TEST_CASE_BODY(fetch_latest__no_schema_version)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE metadata (timestamp INTEGER)");
    db.exec("INSERT INTO metadata VALUES (3)");

    ATF_REQUIRE_THROW_RE(store::integrity_error,
                         "Invalid metadata.*schema_version",
                         store::metadata::fetch_latest(db));
}


ATF_TEST_CASE(fetch_latest__invalid_timestamp);
ATF_TEST_CASE_HEAD(fetch_latest__invalid_timestamp)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(fetch_latest__invalid_timestamp)
{
    sqlite::database db = create_database();
    db.exec("INSERT INTO metadata (schema_version, timestamp) "
            "VALUES (3, 'foo')");

    ATF_REQUIRE_THROW_RE(store::integrity_error,
                         "timestamp.*invalid type",
                         store::metadata::fetch_latest(db));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, fetch_latest__ok);
    ATF_ADD_TEST_CASE(tcs, fetch_latest__empty_metadata);
    ATF_ADD_TEST_CASE(tcs, fetch_latest__no_timestamp);
    ATF_ADD_TEST_CASE(tcs, fetch_latest__no_schema_version);
    ATF_ADD_TEST_CASE(tcs, fetch_latest__invalid_timestamp);
}
