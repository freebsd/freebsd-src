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

#include "store/read_transaction.hpp"

extern "C" {
#include <stdint.h>
}

#include <map>
#include <utility>

#include "model/context.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "store/dbtypes.hpp"
#include "store/exceptions.hpp"
#include "store/read_backend.hpp"
#include "utils/datetime.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/noncopyable.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/exceptions.hpp"
#include "utils/sqlite/statement.ipp"
#include "utils/sqlite/transaction.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace sqlite = utils::sqlite;

using utils::optional;


namespace {


/// Retrieves the environment variables of the context.
///
/// \param db The SQLite database.
///
/// \return The environment variables of the specified context.
///
/// \throw sqlite::error If there is a problem loading the variables.
static std::map< std::string, std::string >
get_env_vars(sqlite::database& db)
{
    std::map< std::string, std::string > env;

    sqlite::statement stmt = db.create_statement(
        "SELECT var_name, var_value FROM env_vars");

    while (stmt.step()) {
        const std::string name = stmt.safe_column_text("var_name");
        const std::string value = stmt.safe_column_text("var_value");
        env[name] = value;
    }

    return env;
}


/// Retrieves a metadata object.
///
/// \param db The SQLite database.
/// \param metadata_id The identifier of the metadata.
///
/// \return A new metadata object.
static model::metadata
get_metadata(sqlite::database& db, const int64_t metadata_id)
{
    model::metadata_builder builder;

    sqlite::statement stmt = db.create_statement(
        "SELECT * FROM metadatas WHERE metadata_id == :metadata_id");
    stmt.bind(":metadata_id", metadata_id);
    while (stmt.step()) {
        const std::string name = stmt.safe_column_text("property_name");
        const std::string value = stmt.safe_column_text("property_value");
        builder.set_string(name, value);
    }

    return builder.build();
}


/// Gets a file from the database.
///
/// \param db The database to query the file from.
/// \param file_id The identifier of the file to be queried.
///
/// \return A textual representation of the file contents.
///
/// \throw integrity_error If there is any problem in the loaded data or if the
///     file cannot be found.
static std::string
get_file(sqlite::database& db, const int64_t file_id)
{
    sqlite::statement stmt = db.create_statement(
        "SELECT contents FROM files WHERE file_id == :file_id");
    stmt.bind(":file_id", file_id);
    if (!stmt.step())
        throw store::integrity_error(F("Cannot find referenced file %s") %
                                     file_id);

    try {
        const sqlite::blob raw_contents = stmt.safe_column_blob("contents");
        const std::string contents(
            static_cast< const char *>(raw_contents.memory), raw_contents.size);

        const bool more = stmt.step();
        INV(!more);

        return contents;
    } catch (const sqlite::error& e) {
        throw store::integrity_error(e.what());
    }
}


/// Gets all the test cases within a particular test program.
///
/// \param db The database to query the information from.
/// \param test_program_id The identifier of the test program whose test cases
///     to query.
///
/// \return The collection of loaded test cases.
///
/// \throw integrity_error If there is any problem in the loaded data.
static model::test_cases_map
get_test_cases(sqlite::database& db, const int64_t test_program_id)
{
    model::test_cases_map_builder test_cases;

    sqlite::statement stmt = db.create_statement(
        "SELECT name, metadata_id "
        "FROM test_cases WHERE test_program_id == :test_program_id");
    stmt.bind(":test_program_id", test_program_id);
    while (stmt.step()) {
        const std::string name = stmt.safe_column_text("name");
        const int64_t metadata_id = stmt.safe_column_int64("metadata_id");

        const model::metadata metadata = get_metadata(db, metadata_id);
        LD(F("Loaded test case '%s'") % name);
        test_cases.add(name, metadata);
    }

    return test_cases.build();
}


/// Retrieves a result from the database.
///
/// \param stmt The statement with the data for the result to load.
/// \param type_column The name of the column containing the type of the result.
/// \param reason_column The name of the column containing the reason for the
///     result, if any.
///
/// \return The loaded result.
///
/// \throw integrity_error If the data in the database is invalid.
static model::test_result
parse_result(sqlite::statement& stmt, const char* type_column,
             const char* reason_column)
{
    try {
        const model::test_result_type type =
            store::column_test_result_type(stmt, type_column);
        if (type == model::test_result_passed) {
            if (stmt.column_type(stmt.column_id(reason_column)) !=
                sqlite::type_null)
                throw store::integrity_error("Result of type 'passed' has a "
                                             "non-NULL reason");
            return model::test_result(type);
        } else {
            return model::test_result(type,
                                      stmt.safe_column_text(reason_column));
        }
    } catch (const sqlite::error& e) {
        throw store::integrity_error(e.what());
    }
}


}  // anonymous namespace


/// Loads a specific test program from the database.
///
/// \param backend_ The store backend we are dealing with.
/// \param id The identifier of the test program to load.
///
/// \return The instantiated test program.
///
/// \throw integrity_error If the data read from the database cannot be properly
///     interpreted.
model::test_program_ptr
store::detail::get_test_program(read_backend& backend_, const int64_t id)
{
    sqlite::database& db = backend_.database();

    model::test_program_ptr test_program;
    sqlite::statement stmt = db.create_statement(
        "SELECT * FROM test_programs WHERE test_program_id == :id");
    stmt.bind(":id", id);
    stmt.step();
    const std::string interface = stmt.safe_column_text("interface");
    test_program.reset(new model::test_program(
        interface,
        fs::path(stmt.safe_column_text("relative_path")),
        fs::path(stmt.safe_column_text("root")),
        stmt.safe_column_text("test_suite_name"),
        get_metadata(db, stmt.safe_column_int64("metadata_id")),
        get_test_cases(db, id)));
    const bool more = stmt.step();
    INV(!more);

    LD(F("Loaded test program '%s'") % test_program->relative_path());
    return test_program;
}


/// Internal implementation for a results iterator.
struct store::results_iterator::impl : utils::noncopyable {
    /// The store backend we are dealing with.
    store::read_backend _backend;

    /// The statement to iterate on.
    sqlite::statement _stmt;

    /// A cache for the last loaded test program.
    optional< std::pair< int64_t, model::test_program_ptr > >
        _last_test_program;

    /// Whether the iterator is still valid or not.
    bool _valid;

    /// Constructor.
    ///
    /// \param backend_ The store backend implementation.
    impl(store::read_backend& backend_) :
        _backend(backend_),
        _stmt(backend_.database().create_statement(
            "SELECT test_programs.test_program_id, "
            "    test_programs.interface, "
            "    test_cases.test_case_id, test_cases.name, "
            "    test_results.result_type, test_results.result_reason, "
            "    test_results.start_time, test_results.end_time "
            "FROM test_programs "
            "    JOIN test_cases "
            "    ON test_programs.test_program_id = test_cases.test_program_id "
            "    JOIN test_results "
            "    ON test_cases.test_case_id = test_results.test_case_id "
            "ORDER BY test_programs.absolute_path, test_cases.name"))
    {
        _valid = _stmt.step();
    }
};


/// Constructor.
///
/// \param pimpl_ The internal implementation details of the iterator.
store::results_iterator::results_iterator(
    std::shared_ptr< impl > pimpl_) :
    _pimpl(pimpl_)
{
}


/// Destructor.
store::results_iterator::~results_iterator(void)
{
}


/// Moves the iterator forward by one result.
///
/// \return The iterator itself.
store::results_iterator&
store::results_iterator::operator++(void)
{
    _pimpl->_valid = _pimpl->_stmt.step();
    return *this;
}


/// Checks whether the iterator is still valid.
///
/// \return True if there is more elements to iterate on, false otherwise.
store::results_iterator::operator bool(void) const
{
    return _pimpl->_valid;
}


/// Gets the test program this result belongs to.
///
/// \return The representation of a test program.
const model::test_program_ptr
store::results_iterator::test_program(void) const
{
    const int64_t id = _pimpl->_stmt.safe_column_int64("test_program_id");
    if (!_pimpl->_last_test_program ||
        _pimpl->_last_test_program.get().first != id)
    {
        const model::test_program_ptr tp = detail::get_test_program(
            _pimpl->_backend, id);
        _pimpl->_last_test_program = std::make_pair(id, tp);
    }
    return _pimpl->_last_test_program.get().second;
}


/// Gets the name of the test case pointed by the iterator.
///
/// The caller can look up the test case data by using the find() method on the
/// test program returned by test_program().
///
/// \return A test case name, unique within the test program.
std::string
store::results_iterator::test_case_name(void) const
{
    return _pimpl->_stmt.safe_column_text("name");
}


/// Gets the result of the test case pointed by the iterator.
///
/// \return A test case result.
model::test_result
store::results_iterator::result(void) const
{
    return parse_result(_pimpl->_stmt, "result_type", "result_reason");
}


/// Gets the start time of the test case execution.
///
/// \return The time when the test started execution.
datetime::timestamp
store::results_iterator::start_time(void) const
{
    return column_timestamp(_pimpl->_stmt, "start_time");
}


/// Gets the end time of the test case execution.
///
/// \return The time when the test finished execution.
datetime::timestamp
store::results_iterator::end_time(void) const
{
    return column_timestamp(_pimpl->_stmt, "end_time");
}


/// Gets a file from a test case.
///
/// \param db The database to query the file from.
/// \param test_case_id The identifier of the test case.
/// \param filename The name of the file to be retrieved.
///
/// \return A textual representation of the file contents.
///
/// \throw integrity_error If there is any problem in the loaded data or if the
///     file cannot be found.
static std::string
get_test_case_file(sqlite::database& db, const int64_t test_case_id,
                   const char* filename)
{
    sqlite::statement stmt = db.create_statement(
        "SELECT file_id FROM test_case_files "
        "WHERE test_case_id == :test_case_id AND file_name == :file_name");
    stmt.bind(":test_case_id", test_case_id);
    stmt.bind(":file_name", filename);
    if (stmt.step())
        return get_file(db, stmt.safe_column_int64("file_id"));
    else
        return "";
}


/// Gets the contents of stdout of a test case.
///
/// \return A textual representation of the stdout contents of the test case.
/// This may of course be empty if the test case didn't print anything.
std::string
store::results_iterator::stdout_contents(void) const
{
    return get_test_case_file(_pimpl->_backend.database(),
                              _pimpl->_stmt.safe_column_int64("test_case_id"),
                              "__STDOUT__");
}


/// Gets the contents of stderr of a test case.
///
/// \return A textual representation of the stderr contents of the test case.
/// This may of course be empty if the test case didn't print anything.
std::string
store::results_iterator::stderr_contents(void) const
{
    return get_test_case_file(_pimpl->_backend.database(),
                              _pimpl->_stmt.safe_column_int64("test_case_id"),
                              "__STDERR__");
}


/// Internal implementation for a store read-only transaction.
struct store::read_transaction::impl : utils::noncopyable {
    /// The backend instance.
    store::read_backend& _backend;

    /// The SQLite database this transaction deals with.
    sqlite::database _db;

    /// The backing SQLite transaction.
    sqlite::transaction _tx;

    /// Opens a transaction.
    ///
    /// \param backend_ The backend this transaction is connected to.
    impl(read_backend& backend_) :
        _backend(backend_),
        _db(backend_.database()),
        _tx(backend_.database().begin_transaction())
    {
    }
};


/// Creates a new read-only transaction.
///
/// \param backend_ The backend this transaction belongs to.
store::read_transaction::read_transaction(read_backend& backend_) :
    _pimpl(new impl(backend_))
{
}


/// Destructor.
store::read_transaction::~read_transaction(void)
{
}


/// Finishes the transaction.
///
/// This actually commits the result of the transaction, but because the
/// transaction is read-only, we use a different term to denote that there is no
/// distinction between commit and rollback.
///
/// \throw error If there is any problem when talking to the database.
void
store::read_transaction::finish(void)
{
    try {
        _pimpl->_tx.commit();
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}


/// Retrieves an context from the database.
///
/// \return The retrieved context.
///
/// \throw error If there is a problem loading the context.
model::context
store::read_transaction::get_context(void)
{
    try {
        sqlite::statement stmt = _pimpl->_db.create_statement(
            "SELECT cwd FROM contexts");
        if (!stmt.step())
            throw error("Error loading context: no data");

        return model::context(fs::path(stmt.safe_column_text("cwd")),
                              get_env_vars(_pimpl->_db));
    } catch (const sqlite::error& e) {
        throw error(F("Error loading context: %s") % e.what());
    }
}


/// Creates a new iterator to scan tests results.
///
/// \return The constructed iterator.
///
/// \throw error If there is any problem constructing the iterator.
store::results_iterator
store::read_transaction::get_results(void)
{
    try {
        return results_iterator(std::shared_ptr< results_iterator::impl >(
           new results_iterator::impl(_pimpl->_backend)));
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}
