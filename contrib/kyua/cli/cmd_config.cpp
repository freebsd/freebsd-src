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

#include "cli/cmd_config.hpp"

#include <cstdlib>

#include "cli/common.ipp"
#include "utils/cmdline/parser.ipp"
#include "utils/cmdline/ui.hpp"
#include "utils/config/tree.ipp"
#include "utils/format/macros.hpp"

namespace cmdline = utils::cmdline;
namespace config = utils::config;

using cli::cmd_config;


namespace {


/// Prints all configuration variables.
///
/// \param ui Object to interact with the I/O of the program.
/// \param properties The key/value map representing all the configuration
///     variables.
///
/// \return 0 for success.
static int
print_all(cmdline::ui* ui, const config::properties_map& properties)
{
    for (config::properties_map::const_iterator iter = properties.begin();
         iter != properties.end(); iter++)
        ui->out(F("%s = %s") % (*iter).first % (*iter).second);
    return EXIT_SUCCESS;
}


/// Prints the configuration variables that the user requests.
///
/// \param ui Object to interact with the I/O of the program.
/// \param properties The key/value map representing all the configuration
///     variables.
/// \param filters The names of the configuration variables to print.
///
/// \return 0 if all specified filters are valid; 1 otherwise.
static int
print_some(cmdline::ui* ui, const config::properties_map& properties,
           const cmdline::args_vector& filters)
{
    bool ok = true;

    for (cmdline::args_vector::const_iterator iter = filters.begin();
         iter != filters.end(); iter++) {
        const config::properties_map::const_iterator match =
            properties.find(*iter);
        if (match == properties.end()) {
            cmdline::print_warning(ui, F("'%s' is not defined.") % *iter);
            ok = false;
        } else
            ui->out(F("%s = %s") % (*match).first % (*match).second);
    }

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}


}  // anonymous namespace


/// Default constructor for cmd_config.
cmd_config::cmd_config(void) : cli_command(
    "config", "[variable1 .. variableN]", 0, -1,
    "Inspects the values of configuration variables")
{
}


/// Entry point for the "config" subcommand.
///
/// \param ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
/// \param user_config The runtime configuration of the program.
///
/// \return 0 if everything is OK, 1 if any of the necessary documents cannot be
/// opened.
int
cmd_config::run(cmdline::ui* ui, const cmdline::parsed_cmdline& cmdline,
               const config::tree& user_config)
{
    const config::properties_map properties = user_config.all_properties();
    if (cmdline.arguments().empty())
        return print_all(ui, properties);
    else
        return print_some(ui, properties, cmdline.arguments());
}
