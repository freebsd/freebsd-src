// Copyright 2024 The Kyua Authors.
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

#include "cli/cmd_prepare.hpp"

#include "cli/common.ipp"
#include "engine/prepare/prepare.hpp"
#include "utils/cmdline/options.hpp"

namespace cmdline = utils::cmdline;
namespace config = utils::config;
namespace prepare = engine::prepare;

using cli::cmd_prepare;


/// Default constructor for cmd_prepare.
cmd_prepare::cmd_prepare(void) : cli_command(
    "prepare", "[handler-name ...]", 0, -1,
    "Prepares environment and declared requirements before testing")
{
    add_option(kyuafile_option);
    add_option(build_root_option);
    add_option(cmdline::bool_option('n', "dry-run", "Do not alter the system"));
}


/// Entry point for the "prepare" subcommand.
///
/// \param ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
/// \param user_config The runtime configuration of the program.
///
/// \return 0 if successful, 1 otherwise.
int
cmd_prepare::run(cmdline::ui* ui, const cmdline::parsed_cmdline& cmdline,
            const config::tree& user_config)
{
    // List available preparation handlers
    if (cmdline.arguments().empty()) {
        for (auto& h : prepare::handlers()) {
            ui->out(h->name(), false);
            ui->out("\t\t\t", false);
            ui->out(h->description());
        }
        return EXIT_SUCCESS;
    }

    // Or run the given ones
    return prepare::run(cmdline.arguments(), ui, cmdline, user_config);
}
