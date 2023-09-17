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

#include "store/write_transaction.hpp"

extern "C" {
#include <stdint.h>
}

#include <fstream>
#include <map>

#include "model/context.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "model/types.hpp"
#include "store/dbtypes.hpp"
#include "store/exceptions.hpp"
#include "store/write_backend.hpp"
#include "utils/datetime.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/noncopyable.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"
#include "utils/stream.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/exceptions.hpp"
#include "utils/sqlite/statement.ipp"
#include "utils/sqlite/transaction.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace sqlite = utils::sqlite;

using utils::none;
using utils::optional;


namespace {


/// Stores the environment variables of a context.
///
/// \param db The SQLite database.
/// \param env The environment variables to store.
///
/// \throw sqlite::error If there is a problem storing the variables.
static void
put_env_vars(sqlite::database& db,
             const std::map< std::string, std::string >& env)
{
    sqlite::statement stmt = db.create_statement(
        "INSERT INTO env_vars (var_name, var_value) "
        "VALUES (:var_name, :var_value)");
    for (std::map< std::string, std::string >::const_iterator iter =
             env.begin(); iter != env.end(); iter++) {
        stmt.bind(":var_name", (*iter).first);
        stmt.bind(":var_value", (*iter).second);
        stmt.step_without_results();
        stmt.reset();
    }
}


/// Calculates the last rowid of a table.
///
/// \param db The SQLite database.
/// \param table Name of the table.
///
/// \return The last rowid; 0 if the table is empty.
static int64_t
last_rowid(sqlite::database& db, const std::string& table)
{
    sqlite::statement stmt = db.create_statement(
        F("SELECT MAX(ROWID) AS max_rowid FROM %s") % table);
    stmt.step();
    if (stmt.column_type(0) == sqlite::type_null) {
        return 0;
    } else {
        INV(stmt.column_type(0) == sqlite::type_integer);
        return stmt.column_int64(0);
    }
}


/// Stores a metadata object.
///
/// \param db The database into which to store the information.
/// \param md The metadata to store.
///
/// \return The identifier of the new metadata object.
static int64_t
put_metadata(sqlite::database& db, const model::metadata& md)
{
    const model::properties_map props = md.to_properties();

    const int64_t metadata_id = last_rowid(db, "metadatas");

    sqlite::statement stmt = db.create_statement(
        "INSERT INTO metadatas (metadata_id, property_name, property_value) "
        "VALUES (:metadata_id, :property_name, :property_value)");
    stmt.bind(":metadata_id", metadata_id);

    for (model::properties_map::const_iterator iter = props.begin();
         iter != props.end(); ++iter) {
        stmt.bind(":property_name", (*iter).first);
        stmt.bind(":property_value", (*iter).second);
        stmt.step_without_results();
        stmt.reset();
    }

    return metadata_id;
}


/// Stores an arbitrary file into the database as a BLOB.
///
/// \param db The database into which to store the file.
/// \param path Path to the file to be stored.
///
/// \return The identifier of the stored file, or none if the file was empty.
///
/// \throw sqlite::error If there are problems writing to the database.
static optional< int64_t >
put_file(sqlite::database& db, const fs::path& path)
{
    std::ifstream input(path.c_str());
    if (!input)
        throw store::error(F("Cannot open file %s") % path);

    try {
        if (utils::stream_length(input) == 0)
            return none;
    } catch (const std::runtime_error& e) {
        // Skipping empty files is an optimization.  If we fail to calculate the
        // size of the file, just ignore the problem.  If there are real issues
        // with the file, the read below will fail anyway.
        LD(F("Cannot determine if file is empty: %s") % e.what());
    }

    // TODO(jmmv): This will probably cause an unreasonable amount of memory
    // consumption if we decide to store arbitrary files in the database (other
    // than stdout or stderr).  Should this happen, we need to investigate a
    // better way to feel blobs into SQLite.
    const std::string contents = utils::read_stream(input);

    sqlite::statement stmt = db.create_statement(
        "INSERT INTO files (contents) VALUES (:contents)");
    stmt.bind(":contents", sqlite::blob(contents.c_str(), contents.length()));
    stmt.step_without_results();

    return optional< int64_t >(db.last_insert_rowid());
}


}  // anonymous namespace


/// Internal implementation for a store write-only transaction.
struct store::write_transaction::impl : utils::noncopyable {
    /// The backend instance.
    store::write_backend& _backend;

    /// The SQLite database this transaction deals with.
    sqlite::database _db;

    /// The backing SQLite transaction.
    sqlite::transaction _tx;

    /// Opens a transaction.
    ///
    /// \param backend_ The backend this transaction is connected to.
    impl(write_backend& backend_) :
        _backend(backend_),
        _db(backend_.database()),
        _tx(backend_.database().begin_transaction())
    {
    }
};


/// Creates a new write-only transaction.
///
/// \param backend_ The backend this transaction belongs to.
store::write_transaction::write_transaction(write_backend& backend_) :
    _pimpl(new impl(backend_))
{
}


/// Destructor.
store::write_transaction::~write_transaction(void)
{
}


/// Commits the transaction.
///
/// \throw error If there is any problem when talking to the database.
void
store::write_transaction::commit(void)
{
    try {
        _pimpl->_tx.commit();
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}


/// Rolls the transaction back.
///
/// \throw error If there is any problem when talking to the database.
void
store::write_transaction::rollback(void)
{
    try {
        _pimpl->_tx.rollback();
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}


/// Puts a context into the database.
///
/// \pre The context has not been put yet.
/// \post The context is stored into the database with a new identifier.
///
/// \param context The context to put.
///
/// \throw error If there is any problem when talking to the database.
void
store::write_transaction::put_context(const model::context& context)
{
    try {
        sqlite::statement stmt = _pimpl->_db.create_statement(
            "INSERT INTO contexts (cwd) VALUES (:cwd)");
        stmt.bind(":cwd", context.cwd().str());
        stmt.step_without_results();

        put_env_vars(_pimpl->_db, context.env());
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}


/// Puts a test program into the database.
///
/// \pre The test program has not been put yet.
/// \post The test program is stored into the database with a new identifier.
///
/// \param test_program The test program to put.
///
/// \return The identifier of the inserted test program.
///
/// \throw error If there is any problem when talking to the database.
int64_t
store::write_transaction::put_test_program(
    const model::test_program& test_program)
{
    try {
        const int64_t metadata_id = put_metadata(
            _pimpl->_db, test_program.get_metadata());

        sqlite::statement stmt = _pimpl->_db.create_statement(
            "INSERT INTO test_programs (absolute_path, "
            "                           root, relative_path, test_suite_name, "
            "                           metadata_id, interface) "
            "VALUES (:absolute_path, :root, :relative_path, "
            "        :test_suite_name, :metadata_id, :interface)");
        stmt.bind(":absolute_path", test_program.absolute_path().str());
        // TODO(jmmv): The root is not necessarily absolute.  We need to ensure
        // that we can recover the absolute path of the test program.  Maybe we
        // need to change the test_program to always ensure root is absolute?
        stmt.bind(":root", test_program.root().str());
        stmt.bind(":relative_path", test_program.relative_path().str());
        stmt.bind(":test_suite_name", test_program.test_suite_name());
        stmt.bind(":metadata_id", metadata_id);
        stmt.bind(":interface", test_program.interface_name());
        stmt.step_without_results();
        return _pimpl->_db.last_insert_rowid();
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}


/// Puts a test case into the database.
///
/// \pre The test case has not been put yet.
/// \post The test case is stored into the database with a new identifier.
///
/// \param test_program The program containing the test case to be stored.
/// \param test_case_name The name of the test case to put.
/// \param test_program_id The test program this test case belongs to.
///
/// \return The identifier of the inserted test case.
///
/// \throw error If there is any problem when talking to the database.
int64_t
store::write_transaction::put_test_case(const model::test_program& test_program,
                                        const std::string& test_case_name,
                                        const int64_t test_program_id)
{
    const model::test_case& test_case = test_program.find(test_case_name);

    try {
        const int64_t metadata_id = put_metadata(
            _pimpl->_db, test_case.get_raw_metadata());

        sqlite::statement stmt = _pimpl->_db.create_statement(
            "INSERT INTO test_cases (test_program_id, name, metadata_id) "
            "VALUES (:test_program_id, :name, :metadata_id)");
        stmt.bind(":test_program_id", test_program_id);
        stmt.bind(":name", test_case.name());
        stmt.bind(":metadata_id", metadata_id);
        stmt.step_without_results();
        return _pimpl->_db.last_insert_rowid();
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}


/// Stores a file generated by a test case into the database as a BLOB.
///
/// \param name The name of the file to store in the database.  This needs to be
///     unique per test case.  The caller is free to decide what names to use
///     for which files.  For example, it might make sense to always call
///     __STDOUT__ the stdout of the test case so that it is easy to locate.
/// \param path The path to the file to be stored.
/// \param test_case_id The identifier of the test case this file belongs to.
///
/// \return The identifier of the stored file, or none if the file was empty.
///
/// \throw store::error If there are problems writing to the database.
optional< int64_t >
store::write_transaction::put_test_case_file(const std::string& name,
                                             const fs::path& path,
                                             const int64_t test_case_id)
{
    LD(F("Storing %s (%s) of test case %s") % name % path % test_case_id);
    try {
        const optional< int64_t > file_id = put_file(_pimpl->_db, path);
        if (!file_id) {
            LD("Not storing empty file");
            return none;
        }

        sqlite::statement stmt = _pimpl->_db.create_statement(
            "INSERT INTO test_case_files (test_case_id, file_name, file_id) "
            "VALUES (:test_case_id, :file_name, :file_id)");
        stmt.bind(":test_case_id", test_case_id);
        stmt.bind(":file_name", name);
        stmt.bind(":file_id", file_id.get());
        stmt.step_without_results();

        return optional< int64_t >(_pimpl->_db.last_insert_rowid());
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}


/// Puts a result into the database.
///
/// \pre The result has not been put yet.
/// \post The result is stored into the database with a new identifier.
///
/// \param result The result to put.
/// \param test_case_id The test case this result corresponds to.
/// \param start_time The time when the test started to run.
/// \param end_time The time when the test finished running.
///
/// \return The identifier of the inserted result.
///
/// \throw error If there is any problem when talking to the database.
int64_t
store::write_transaction::put_result(const model::test_result& result,
                                     const int64_t test_case_id,
                                     const datetime::timestamp& start_time,
                                     const datetime::timestamp& end_time)
{
    try {
        sqlite::statement stmt = _pimpl->_db.create_statement(
            "INSERT INTO test_results (test_case_id, result_type, "
            "                          result_reason, start_time, "
            "                          end_time) "
            "VALUES (:test_case_id, :result_type, :result_reason, "
            "        :start_time, :end_time)");
        stmt.bind(":test_case_id", test_case_id);

        store::bind_test_result_type(stmt, ":result_type", result.type());
        if (result.reason().empty())
            stmt.bind(":result_reason", sqlite::null());
        else
            stmt.bind(":result_reason", result.reason());

        store::bind_timestamp(stmt, ":start_time", start_time);
        store::bind_timestamp(stmt, ":end_time", end_time);

        stmt.step_without_results();
        const int64_t result_id = _pimpl->_db.last_insert_rowid();

        return result_id;
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}
