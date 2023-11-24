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

#include "cli/cmd_db_exec.hpp"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <sstream>
#include <string>

#include "cli/common.ipp"
#include "store/exceptions.hpp"
#include "store/layout.hpp"
#include "store/read_backend.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/cmdline/ui.hpp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/sanity.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/exceptions.hpp"
#include "utils/sqlite/statement.hpp"

namespace cmdline = utils::cmdline;
namespace config = utils::config;
namespace fs = utils::fs;
namespace layout = store::layout;
namespace sqlite = utils::sqlite;

using cli::cmd_db_exec;


namespace {


/// Concatenates a vector into a string using ' ' as a separator.
///
/// \param args The objects to join.  This cannot be empty.
///
/// \return The concatenation of all the objects in the set.
static std::string
flatten_args(const cmdline::args_vector& args)
{
    std::ostringstream output;
    std::copy(args.begin(), args.end(),
              std::ostream_iterator< std::string >(output, " "));

    std::string result = output.str();
    result.erase(result.end() - 1);
    return result;
}


}  // anonymous namespace


/// Formats a particular cell of a statement result.
///
/// \param stmt The statement whose cell to format.
/// \param index The index of the cell to format.
///
/// \return A textual representation of the cell.
std::string
cli::format_cell(sqlite::statement& stmt, const int index)
{
    switch (stmt.column_type(index)) {
    case sqlite::type_blob: {
        const sqlite::blob blob = stmt.column_blob(index);
        return F("BLOB of %s bytes") % blob.size;
    }

    case sqlite::type_float:
        return F("%s") % stmt.column_double(index);

    case sqlite::type_integer:
        return F("%s") % stmt.column_int64(index);

    case sqlite::type_null:
        return "NULL";

    case sqlite::type_text:
        return stmt.column_text(index);
    }

    UNREACHABLE;
}


/// Formats the column names of a statement for output as CSV.
///
/// \param stmt The statement whose columns to format.
///
/// \return A comma-separated list of column names.
std::string
cli::format_headers(sqlite::statement& stmt)
{
    std::string output;
    int i = 0;
    for (; i < stmt.column_count() - 1; ++i)
        output += stmt.column_name(i) + ',';
    output += stmt.column_name(i);
    return output;
}


/// Formats a row of a statement for output as CSV.
///
/// \param stmt The statement whose current row to format.
///
/// \return A comma-separated list of values.
std::string
cli::format_row(sqlite::statement& stmt)
{
    std::string output;
    int i = 0;
    for (; i < stmt.column_count() - 1; ++i)
        output += cli::format_cell(stmt, i) + ',';
    output += cli::format_cell(stmt, i);
    return output;
}


/// Default constructor for cmd_db_exec.
cmd_db_exec::cmd_db_exec(void) : cli_command(
    "db-exec", "sql_statement", 1, -1,
    "Executes an arbitrary SQL statement in a results file and prints "
    "the resulting table")
{
    add_option(results_file_open_option);
    add_option(cmdline::bool_option("no-headers", "Do not show headers in the "
                                    "output table"));
}


/// Entry point for the "db-exec" subcommand.
///
/// \param ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
///
/// \return 0 if everything is OK, 1 if the statement is invalid or if there is
/// any other problem.
int
cmd_db_exec::run(cmdline::ui* ui, const cmdline::parsed_cmdline& cmdline,
                 const config::tree& /* user_config */)
{
    try {
        const fs::path results_file = layout::find_results(
            results_file_open(cmdline));

        // TODO(jmmv): Shouldn't be using store::detail here...
        sqlite::database db = store::detail::open_and_setup(
            results_file, sqlite::open_readwrite);
        sqlite::statement stmt = db.create_statement(
            flatten_args(cmdline.arguments()));

        if (stmt.step()) {
            if (!cmdline.has_option("no-headers"))
                ui->out(cli::format_headers(stmt));
            do
                ui->out(cli::format_row(stmt));
            while (stmt.step());
        }

        return EXIT_SUCCESS;
    } catch (const sqlite::error& e) {
        cmdline::print_error(ui, F("SQLite error: %s.") % e.what());
        return EXIT_FAILURE;
    } catch (const store::error& e) {
        cmdline::print_error(ui, F("%s.") % e.what());
        return EXIT_FAILURE;
    }
}
