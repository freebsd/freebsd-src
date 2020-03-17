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

#include <map>
#include <string>

#include <atf-c++.hpp>

#include "model/context.hpp"
#include "model/metadata.hpp"
#include "model/test_program.hpp"
#include "store/read_backend.hpp"
#include "store/read_transaction.hpp"
#include "store/write_backend.hpp"
#include "store/write_transaction.hpp"
#include "utils/datetime.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/operations.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/units.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace logging = utils::logging;
namespace units = utils::units;


namespace {


/// Puts and gets a context and validates the results.
///
/// \param exp_context The context to save and restore.
static void
check_get_put_context(const model::context& exp_context)
{
    const fs::path test_db("test.db");

    if (fs::exists(test_db))
        fs::unlink(test_db);

    {
        store::write_backend backend = store::write_backend::open_rw(test_db);
        store::write_transaction tx = backend.start_write();
        tx.put_context(exp_context);
        tx.commit();
    }
    {
        store::read_backend backend = store::read_backend::open_ro(test_db);
        store::read_transaction tx = backend.start_read();
        model::context context = tx.get_context();
        tx.finish();

        ATF_REQUIRE(exp_context == context);
    }
}


}  // anonymous namespace


ATF_TEST_CASE(get_put_context__ok);
ATF_TEST_CASE_HEAD(get_put_context__ok)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_put_context__ok)
{
    std::map< std::string, std::string > env1;
    env1["A1"] = "foo";
    env1["A2"] = "bar";
    std::map< std::string, std::string > env2;
    check_get_put_context(model::context(fs::path("/foo/bar"), env1));
    check_get_put_context(model::context(fs::path("/foo/bar"), env1));
    check_get_put_context(model::context(fs::path("/foo/baz"), env2));
}


ATF_TEST_CASE(get_put_test_case__ok);
ATF_TEST_CASE_HEAD(get_put_test_case__ok)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_put_test_case__ok)
{
    const model::metadata md2 = model::metadata_builder()
        .add_allowed_architecture("powerpc")
        .add_allowed_architecture("x86_64")
        .add_allowed_platform("amd64")
        .add_allowed_platform("macppc")
        .add_custom("user1", "value1")
        .add_custom("user2", "value2")
        .add_required_config("var1")
        .add_required_config("var2")
        .add_required_config("var3")
        .add_required_file(fs::path("/file1/yes"))
        .add_required_file(fs::path("/file2/foo"))
        .add_required_program(fs::path("/bin/ls"))
        .add_required_program(fs::path("cp"))
        .set_description("The description")
        .set_has_cleanup(true)
        .set_required_memory(units::bytes::parse("1k"))
        .set_required_user("root")
        .set_timeout(datetime::delta(520, 0))
        .build();

    const model::test_program test_program = model::test_program_builder(
        "atf", fs::path("the/binary"), fs::path("/some/root"), "the-suite")
        .add_test_case("tc1")
        .add_test_case("tc2", md2)
        .build();

    int64_t test_program_id;
    {
        store::write_backend backend = store::write_backend::open_rw(
            fs::path("test.db"));
        backend.database().exec("PRAGMA foreign_keys = OFF");

        store::write_transaction tx = backend.start_write();
        test_program_id = tx.put_test_program(test_program);
        tx.put_test_case(test_program, "tc1", test_program_id);
        tx.put_test_case(test_program, "tc2", test_program_id);
        tx.commit();
    }

    store::read_backend backend = store::read_backend::open_ro(
        fs::path("test.db"));
    backend.database().exec("PRAGMA foreign_keys = OFF");

    store::read_transaction tx = backend.start_read();
    const model::test_program_ptr loaded_test_program =
        store::detail::get_test_program(backend, test_program_id);
    ATF_REQUIRE(test_program == *loaded_test_program);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, get_put_context__ok);

    ATF_ADD_TEST_CASE(tcs, get_put_test_case__ok);
}
