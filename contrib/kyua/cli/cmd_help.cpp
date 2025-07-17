// Copyright 2010 The Kyua Authors.
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

#include "cli/cmd_help.hpp"

#include <algorithm>
#include <cstdlib>

#include "cli/common.ipp"
#include "utils/cmdline/commands_map.ipp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/globals.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.hpp"
#include "utils/cmdline/ui.hpp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/sanity.hpp"
#include "utils/text/table.hpp"

namespace cmdline = utils::cmdline;
namespace config = utils::config;
namespace text = utils::text;

using cli::cmd_help;


namespace {


/// Creates a table with the help of a set of options.
///
/// \param options The set of options to describe.  May be empty.
///
/// \return A 2-column wide table with the description of the options.
static text::table
options_help(const cmdline::options_vector& options)
{
    text::table table(2);

    for (cmdline::options_vector::const_iterator iter = options.begin();
         iter != options.end(); iter++) {
        const cmdline::base_option* option = *iter;

        std::string description = option->description();
        if (option->needs_arg() && option->has_default_value())
            description += F(" (default: %s)") % option->default_value();

        text::table_row row;

        if (option->has_short_name())
            row.push_back(F("%s, %s") % option->format_short_name() %
                          option->format_long_name());
        else
            row.push_back(F("%s") % option->format_long_name());
        row.push_back(F("%s.") % description);

        table.add_row(row);
    }

    return table;
}


/// Prints the summary of commands and generic options.
///
/// \param ui Object to interact with the I/O of the program.
/// \param options The set of program-wide options for which to print help.
/// \param commands The set of commands for which to print help.
static void
general_help(cmdline::ui* ui, const cmdline::options_vector* options,
             const cmdline::commands_map< cli::cli_command >* commands)
{
    PRE(!commands->empty());

    cli::write_version_header(ui);
    ui->out("");
    ui->out_tag_wrap(
        "Usage: ",
        F("%s [general_options] command [command_options] [args]") %
        cmdline::progname(), false);

    const text::table options_table = options_help(*options);
    text::widths_vector::value_type first_width =
        options_table.column_width(0);

    std::map< std::string, text::table > command_tables;

    for (cmdline::commands_map< cli::cli_command >::const_iterator
         iter = commands->begin(); iter != commands->end(); iter++) {
        const std::string& category = (*iter).first;
        const std::set< std::string >& command_names = (*iter).second;

        command_tables.insert(std::map< std::string, text::table >::value_type(
            category, text::table(2)));
        text::table& table = command_tables.find(category)->second;

        for (std::set< std::string >::const_iterator i2 = command_names.begin();
             i2 != command_names.end(); i2++) {
            const cli::cli_command* command = commands->find(*i2);
            text::table_row row;
            row.push_back(command->name());
            row.push_back(F("%s.") % command->short_description());
            table.add_row(row);
        }

        if (table.column_width(0) > first_width)
            first_width = table.column_width(0);
    }

    text::table_formatter formatter;
    formatter.set_column_width(0, first_width);
    formatter.set_column_width(1, text::table_formatter::width_refill);
    formatter.set_separator("  ");

    if (!options_table.empty()) {
        ui->out_wrap("");
        ui->out_wrap("Available general options:");
        ui->out_table(options_table, formatter, "  ");
    }

    // Iterate using the same loop as above to preserve ordering.
    for (cmdline::commands_map< cli::cli_command >::const_iterator
         iter = commands->begin(); iter != commands->end(); iter++) {
        const std::string& category = (*iter).first;
        ui->out_wrap("");
        ui->out_wrap(F("%s commands:") %
                (category.empty() ? "Generic" : category));
        ui->out_table(command_tables.find(category)->second, formatter, "  ");
    }

    ui->out_wrap("");
    ui->out_wrap("See kyua(1) for more details.");
}


/// Prints help for a particular subcommand.
///
/// \param ui Object to interact with the I/O of the program.
/// \param general_options The options that apply to all commands.
/// \param command Pointer to the command to describe.
static void
subcommand_help(cmdline::ui* ui,
                const utils::cmdline::options_vector* general_options,
                const cli::cli_command* command)
{
    cli::write_version_header(ui);
    ui->out("");
    ui->out_tag_wrap(
        "Usage: ", F("%s [general_options] %s%s%s") %
        cmdline::progname() % command->name() %
        (command->options().empty() ? "" : " [command_options]") %
        (command->arg_list().empty() ? "" : (" " + command->arg_list())),
        false);
    ui->out_wrap("");
    ui->out_wrap(F("%s.") % command->short_description());

    const text::table general_table = options_help(*general_options);
    const text::table command_table = options_help(command->options());

    const text::widths_vector::value_type first_width =
        std::max(general_table.column_width(0), command_table.column_width(0));
    text::table_formatter formatter;
    formatter.set_column_width(0, first_width);
    formatter.set_column_width(1, text::table_formatter::width_refill);
    formatter.set_separator("  ");

    if (!general_table.empty()) {
        ui->out_wrap("");
        ui->out_wrap("Available general options:");
        ui->out_table(general_table, formatter, "  ");
    }

    if (!command_table.empty()) {
        ui->out_wrap("");
        ui->out_wrap("Available command options:");
        ui->out_table(command_table, formatter, "  ");
    }

    ui->out_wrap("");
    ui->out_wrap(F("See kyua-%s(1) for more details.") % command->name());
}


}  // anonymous namespace


/// Default constructor for cmd_help.
///
/// \param options_ The set of program-wide options for which to provide help.
/// \param commands_ The set of commands for which to provide help.
cmd_help::cmd_help(const cmdline::options_vector* options_,
                   const cmdline::commands_map< cli_command >* commands_) :
    cli_command("help", "[subcommand]", 0, 1, "Shows usage information"),
    _options(options_),
    _commands(commands_)
{
}


/// Entry point for the "help" subcommand.
///
/// \param ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
///
/// \return 0 to indicate success.
int
cmd_help::run(utils::cmdline::ui* ui, const cmdline::parsed_cmdline& cmdline,
              const config::tree& /* user_config */)
{
    if (cmdline.arguments().empty()) {
        general_help(ui, _options, _commands);
    } else {
        INV(cmdline.arguments().size() == 1);
        const std::string& cmdname = cmdline.arguments()[0];
        const cli::cli_command* command = _commands->find(cmdname);
        if (command == NULL)
            throw cmdline::usage_error(F("The command %s does not exist") %
                                       cmdname);
        else
            subcommand_help(ui, _options, command);
    }

    return EXIT_SUCCESS;
}
