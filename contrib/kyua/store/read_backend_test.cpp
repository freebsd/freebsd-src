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

#include "store/read_backend.hpp"

#include <atf-c++.hpp>

#include "store/exceptions.hpp"
#include "store/metadata.hpp"
#include "store/write_backend.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/operations.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/exceptions.hpp"

namespace fs = utils::fs;
namespace logging = utils::logging;
namespace sqlite = utils::sqlite;


ATF_TEST_CASE_WITHOUT_HEAD(detail__open_and_setup__ok);
ATF_TEST_CASE_BODY(detail__open_and_setup__ok)
{
    {
        sqlite::database db = sqlite::database::open(
            fs::path("test.db"), sqlite::open_readwrite | sqlite::open_create);
        db.exec("CREATE TABLE one (foo INTEGER PRIMARY KEY AUTOINCREMENT);");
        db.exec("CREATE TABLE two (foo INTEGER REFERENCES one);");
        db.close();
    }

    sqlite::database db = store::detail::open_and_setup(
        fs::path("test.db"), sqlite::open_readwrite);
    db.exec("INSERT INTO one (foo) VALUES (12);");
    // Ensure foreign keys have been enabled.
    db.exec("INSERT INTO two (foo) VALUES (12);");
    ATF_REQUIRE_THROW(sqlite::error,
                      db.exec("INSERT INTO two (foo) VALUES (34);"));
}


ATF_TEST_CASE_WITHOUT_HEAD(detail__open_and_setup__missing_file);
ATF_TEST_CASE_BODY(detail__open_and_setup__missing_file)
{
    ATF_REQUIRE_THROW_RE(store::error, "Cannot open 'missing.db': ",
                         store::detail::open_and_setup(fs::path("missing.db"),
                                                       sqlite::open_readonly));
    ATF_REQUIRE(!fs::exists(fs::path("missing.db")));
}


ATF_TEST_CASE(read_backend__open_ro__ok);
ATF_TEST_CASE_HEAD(read_backend__open_ro__ok)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(read_backend__open_ro__ok)
{
    {
        sqlite::database db = sqlite::database::open(
            fs::path("test.db"), sqlite::open_readwrite | sqlite::open_create);
        store::detail::initialize(db);
    }
    store::read_backend backend = store::read_backend::open_ro(
        fs::path("test.db"));
    backend.database().exec("SELECT * FROM metadata");
}


ATF_TEST_CASE_WITHOUT_HEAD(read_backend__open_ro__missing_file);
ATF_TEST_CASE_BODY(read_backend__open_ro__missing_file)
{
    ATF_REQUIRE_THROW_RE(store::error, "Cannot open 'missing.db': ",
                         store::read_backend::open_ro(fs::path("missing.db")));
    ATF_REQUIRE(!fs::exists(fs::path("missing.db")));
}


ATF_TEST_CASE(read_backend__open_ro__integrity_error);
ATF_TEST_CASE_HEAD(read_backend__open_ro__integrity_error)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(read_backend__open_ro__integrity_error)
{
    {
        sqlite::database db = sqlite::database::open(
            fs::path("test.db"), sqlite::open_readwrite | sqlite::open_create);
        store::detail::initialize(db);
        db.exec("DELETE FROM metadata");
    }
    ATF_REQUIRE_THROW_RE(store::integrity_error, "metadata.*empty",
                         store::read_backend::open_ro(fs::path("test.db")));
}


ATF_TEST_CASE(read_backend__close);
ATF_TEST_CASE_HEAD(read_backend__close)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(read_backend__close)
{
    store::write_backend::open_rw(fs::path("test.db"));  // Create database.
    store::read_backend backend = store::read_backend::open_ro(
        fs::path("test.db"));
    backend.database().exec("SELECT * FROM metadata");
    backend.close();
    ATF_REQUIRE_THROW(utils::sqlite::error,
                      backend.database().exec("SELECT * FROM metadata"));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, detail__open_and_setup__ok);
    ATF_ADD_TEST_CASE(tcs, detail__open_and_setup__missing_file);

    ATF_ADD_TEST_CASE(tcs, read_backend__open_ro__ok);
    ATF_ADD_TEST_CASE(tcs, read_backend__open_ro__missing_file);
    ATF_ADD_TEST_CASE(tcs, read_backend__open_ro__integrity_error);
    ATF_ADD_TEST_CASE(tcs, read_backend__close);
}
