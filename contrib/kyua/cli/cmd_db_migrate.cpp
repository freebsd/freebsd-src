// Copyright 2013 The Kyua Authors.
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

#include "cli/cmd_db_migrate.hpp"

#include <cstdlib>

#include "cli/common.ipp"
#include "store/exceptions.hpp"
#include "store/layout.hpp"
#include "store/migrate.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/ui.hpp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"

namespace cmdline = utils::cmdline;
namespace config = utils::config;
namespace fs = utils::fs;
namespace layout = store::layout;

using cli::cmd_db_migrate;


/// Default constructor for cmd_db_migrate.
cmd_db_migrate::cmd_db_migrate(void) : cli_command(
    "db-migrate", "", 0, 0,
    "Upgrades the schema of an existing results file to the currently "
    "implemented version.  A backup of the results file is created, but "
    "this operation is not reversible")
{
    add_option(results_file_open_option);
}


/// Entry point for the "db-migrate" subcommand.
///
/// \param ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
///
/// \return 0 if everything is OK, 1 if the statement is invalid or if there is
/// any other problem.
int
cmd_db_migrate::run(cmdline::ui* ui, const cmdline::parsed_cmdline& cmdline,
                    const config::tree& /* user_config */)
{
    try {
        const fs::path results_file = layout::find_results(
            results_file_open(cmdline));
        store::migrate_schema(results_file);
        return EXIT_SUCCESS;
    } catch (const store::error& e) {
        cmdline::print_error(ui, F("Migration failed: %s.") % e.what());
        return EXIT_FAILURE;
    }
}
