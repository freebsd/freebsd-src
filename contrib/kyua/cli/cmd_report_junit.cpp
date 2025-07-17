// Copyright 2014 The Kyua Authors.
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

#include "cli/cmd_report_junit.hpp"

#include <cstddef>
#include <cstdlib>
#include <set>

#include "cli/common.ipp"
#include "drivers/report_junit.hpp"
#include "drivers/scan_results.hpp"
#include "engine/filters.hpp"
#include "store/layout.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/defs.hpp"
#include "utils/optional.ipp"
#include "utils/stream.hpp"

namespace cmdline = utils::cmdline;
namespace config = utils::config;
namespace fs = utils::fs;
namespace layout = store::layout;

using cli::cmd_report_junit;
using utils::optional;


/// Default constructor for cmd_report.
cmd_report_junit::cmd_report_junit(void) : cli_command(
    "report-junit", "", 0, 0,
    "Generates a JUnit report with the result of a test suite run")
{
    add_option(results_file_open_option);
    add_option(cmdline::path_option("output", "Path to the output file", "path",
                                    "/dev/stdout"));
}


/// Entry point for the "report" subcommand.
///
/// \param cmdline Representation of the command line to the subcommand.
///
/// \return 0 if everything is OK, 1 if the statement is invalid or if there is
/// any other problem.
int
cmd_report_junit::run(cmdline::ui* /* ui */,
                      const cmdline::parsed_cmdline& cmdline,
                      const config::tree& /* user_config */)
{
    const fs::path results_file = layout::find_results(
        results_file_open(cmdline));

    std::unique_ptr< std::ostream > output = utils::open_ostream(
        cmdline.get_option< cmdline::path_option >("output"));

    drivers::report_junit_hooks hooks(*output.get());
    drivers::scan_results::drive(results_file,
                                 std::set< engine::test_filter >(),
                                 hooks);

    return EXIT_SUCCESS;
}
