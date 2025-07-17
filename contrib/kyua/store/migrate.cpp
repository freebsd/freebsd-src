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

#include <stdexcept>

#include "store/dbtypes.hpp"
#include "store/exceptions.hpp"
#include "store/layout.hpp"
#include "store/metadata.hpp"
#include "store/read_backend.hpp"
#include "store/write_backend.hpp"
#include "utils/datetime.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"
#include "utils/stream.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/exceptions.hpp"
#include "utils/sqlite/statement.ipp"
#include "utils/text/operations.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace sqlite = utils::sqlite;
namespace text = utils::text;

using utils::none;
using utils::optional;


namespace {


/// Schema version at which we switched to results files.
const int first_chunked_schema_version = 3;


/// Queries the schema version of the given database.
///
/// \param file The database from which to query the schema version.
///
/// \return The schema version number.
static int
get_schema_version(const fs::path& file)
{
    sqlite::database db = store::detail::open_and_setup(
        file, sqlite::open_readonly);
    return store::metadata::fetch_latest(db).schema_version();
}


/// Performs a single migration step.
///
/// Both action_id and old_database are little hacks to support the migration
/// from the historical database to chunked files.  We'd use a more generic
/// "replacements" map, but it's not worth it.
///
/// \param file Database on which to apply the migration step.
/// \param version_from Current schema version in the database.
/// \param version_to Schema version to migrate to.
/// \param action_id If not none, replace ACTION_ID in the migration file with
///     this value.
/// \param old_database If not none, replace OLD_DATABASE in the migration
///     file with this value.
///
/// \throw error If there is a problem applying the migration.
static void
migrate_schema_step(const fs::path& file,
                    const int version_from,
                    const int version_to,
                    const optional< int64_t > action_id = none,
                    const optional< fs::path > old_database = none)
{
    LI(F("Migrating schema of %s from version %s to %s") % file % version_from
       % version_to);

    PRE(version_to == version_from + 1);

    sqlite::database db = store::detail::open_and_setup(
        file, sqlite::open_readwrite);

    const fs::path migration = store::detail::migration_file(version_from,
                                                             version_to);

    std::string migration_string;
    try {
        migration_string = utils::read_file(migration);
    } catch (const std::runtime_error& unused_e) {
        throw store::error(F("Cannot read migration file '%s'") % migration);
    }
    if (action_id) {
        migration_string = text::replace_all(migration_string, "@ACTION_ID@",
                                             F("%s") % action_id.get());
    }
    if (old_database) {
        migration_string = text::replace_all(migration_string, "@OLD_DATABASE@",
                                             old_database.get().str());
    }
    try {
        db.exec(migration_string);
    } catch (const sqlite::error& e) {
        throw store::error(F("Schema migration failed: %s") % e.what());
    }
}


/// Given a historical database, chunks it up into results files.
///
/// The given database is DELETED on success given that it will have been
/// split up into various different files.
///
/// \param old_file Path to the old database.
static void
chunk_database(const fs::path& old_file)
{
    PRE(get_schema_version(old_file) == first_chunked_schema_version - 1);

    LI(F("Need to split %s into per-action files") % old_file);

    sqlite::database old_db = store::detail::open_and_setup(
        old_file, sqlite::open_readonly);

    sqlite::statement actions_stmt = old_db.create_statement(
        "SELECT action_id, cwd FROM actions NATURAL JOIN contexts");

    sqlite::statement start_time_stmt = old_db.create_statement(
        "SELECT test_results.start_time AS start_time "
        "FROM test_programs "
        "    JOIN test_cases "
        "        ON test_programs.test_program_id == test_cases.test_program_id"
        "    JOIN test_results "
        "        ON test_cases.test_case_id == test_results.test_case_id "
        "WHERE test_programs.action_id == :action_id "
        "ORDER BY start_time LIMIT 1");

    while (actions_stmt.step()) {
        const int64_t action_id = actions_stmt.safe_column_int64("action_id");
        const fs::path cwd(actions_stmt.safe_column_text("cwd"));

        LI(F("Extracting action %s") % action_id);

        start_time_stmt.reset();
        start_time_stmt.bind(":action_id", action_id);
        if (!start_time_stmt.step()) {
            LI(F("Skipping empty action %s") % action_id);
            continue;
        }
        const datetime::timestamp start_time = store::column_timestamp(
            start_time_stmt, "start_time");
        start_time_stmt.step_without_results();

        const fs::path new_file = store::layout::new_db_for_migration(
            cwd, start_time);
        if (fs::exists(new_file)) {
            LI(F("Skipping action because %s already exists") % new_file);
            continue;
        }

        LI(F("Creating %s for previous action %s") % new_file % action_id);

        try {
            fs::mkdir_p(new_file.branch_path(), 0755);
            sqlite::database db = store::detail::open_and_setup(
                new_file, sqlite::open_readwrite | sqlite::open_create);
            store::detail::initialize(db);
            db.close();
            migrate_schema_step(new_file,
                                first_chunked_schema_version - 1,
                                first_chunked_schema_version,
                                utils::make_optional(action_id),
                                utils::make_optional(old_file));
        } catch (...) {
            // TODO(jmmv): Handle this better.
            fs::unlink(new_file);
        }
    }

    fs::unlink(old_file);
}


}  // anonymous namespace


/// Calculates the path to a schema migration file.
///
/// \param version_from The version from which the database is being upgraded.
/// \param version_to The version to which the database is being upgraded.
///
/// \return The path to the installed migrate_vX_vY.sql file.
fs::path
store::detail::migration_file(const int version_from, const int version_to)
{
    return fs::path(utils::getenv_with_default("KYUA_STOREDIR", KYUA_STOREDIR))
        / (F("migrate_v%s_v%s.sql") % version_from % version_to);
}


/// Backs up a database for schema migration purposes.
///
/// \todo We should probably use the SQLite backup API instead of doing a raw
/// file copy.  We issue our backup call with the database already open, but
/// because it is quiescent, it's OK to do so.
///
/// \param source Location of the database to be backed up.
/// \param old_version Version of the database's CURRENT schema, used to
///     determine the name of the backup file.
///
/// \throw error If there is a problem during the backup.
void
store::detail::backup_database(const fs::path& source, const int old_version)
{
    const fs::path target(F("%s.v%s.backup") % source.str() % old_version);

    LI(F("Backing up database %s to %s") % source % target);
    try {
        fs::copy(source, target);
    } catch (const fs::error& e) {
        throw store::error(e.what());
    }
}


/// Migrates the schema of a database to the current version.
///
/// The algorithm implemented here performs a migration step for every
/// intermediate version between the schema version in the database to the
/// version implemented in this file.  This should permit upgrades from
/// arbitrary old databases.
///
/// \param file The database whose schema to upgrade.
///
/// \throw error If there is a problem with the migration.
void
store::migrate_schema(const utils::fs::path& file)
{
    const int version_from = get_schema_version(file);
    const int version_to = detail::current_schema_version;
    if (version_from == version_to) {
        throw error(F("Database already at schema version %s; migration not "
                      "needed") % version_from);
    } else if (version_from > version_to) {
        throw error(F("Database at schema version %s, which is newer than the "
                      "supported version %s") % version_from % version_to);
    }

    detail::backup_database(file, version_from);

    int i;
    for (i = version_from; i < first_chunked_schema_version - 1; ++i) {
        migrate_schema_step(file, i, i + 1);
    }
    chunk_database(file);
    INV(version_to == first_chunked_schema_version);
}
