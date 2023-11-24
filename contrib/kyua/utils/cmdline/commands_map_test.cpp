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

#include "utils/cmdline/commands_map.ipp"

#include <atf-c++.hpp>

#include "utils/cmdline/base_command.hpp"
#include "utils/defs.hpp"
#include "utils/sanity.hpp"

namespace cmdline = utils::cmdline;


namespace {


/// Fake command to validate the behavior of commands_map.
///
/// Note that this command does not do anything.  It is only intended to provide
/// a specific class that can be inserted into commands_map instances and check
/// that it can be located properly.
class mock_cmd : public cmdline::base_command_no_data {
public:
    /// Constructor for the mock command.
    ///
    /// \param mock_name The name of the command.  All other settings are set to
    ///     irrelevant values.
    mock_cmd(const char* mock_name) :
        cmdline::base_command_no_data(mock_name, "", 0, 0,
                                      "Command for testing.")
    {
    }

    /// Runs the mock command.
    ///
    /// \return Nothing because this function is never called.
    int
    run(cmdline::ui* /* ui */,
        const cmdline::parsed_cmdline& /* cmdline */)
    {
        UNREACHABLE;
    }
};


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(empty);
ATF_TEST_CASE_BODY(empty)
{
    cmdline::commands_map< cmdline::base_command_no_data > commands;
    ATF_REQUIRE(commands.empty());
    ATF_REQUIRE(commands.begin() == commands.end());
}


ATF_TEST_CASE_WITHOUT_HEAD(some);
ATF_TEST_CASE_BODY(some)
{
    cmdline::commands_map< cmdline::base_command_no_data > commands;
    cmdline::base_command_no_data* cmd1 = new mock_cmd("cmd1");
    commands.insert(cmd1);
    cmdline::base_command_no_data* cmd2 = new mock_cmd("cmd2");
    commands.insert(cmd2, "foo");

    ATF_REQUIRE(!commands.empty());

    cmdline::commands_map< cmdline::base_command_no_data >::const_iterator
        iter = commands.begin();
    ATF_REQUIRE_EQ("", (*iter).first);
    ATF_REQUIRE_EQ(1, (*iter).second.size());
    ATF_REQUIRE_EQ("cmd1", *(*iter).second.begin());

    ++iter;
    ATF_REQUIRE_EQ("foo", (*iter).first);
    ATF_REQUIRE_EQ(1, (*iter).second.size());
    ATF_REQUIRE_EQ("cmd2", *(*iter).second.begin());

    ATF_REQUIRE(++iter == commands.end());
}


ATF_TEST_CASE_WITHOUT_HEAD(find__match);
ATF_TEST_CASE_BODY(find__match)
{
    cmdline::commands_map< cmdline::base_command_no_data > commands;
    cmdline::base_command_no_data* cmd1 = new mock_cmd("cmd1");
    commands.insert(cmd1);
    cmdline::base_command_no_data* cmd2 = new mock_cmd("cmd2");
    commands.insert(cmd2);

    ATF_REQUIRE(cmd1 == commands.find("cmd1"));
    ATF_REQUIRE(cmd2 == commands.find("cmd2"));
}


ATF_TEST_CASE_WITHOUT_HEAD(find__nomatch);
ATF_TEST_CASE_BODY(find__nomatch)
{
    cmdline::commands_map< cmdline::base_command_no_data > commands;
    commands.insert(new mock_cmd("cmd1"));

    ATF_REQUIRE(NULL == commands.find("cmd2"));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, empty);
    ATF_ADD_TEST_CASE(tcs, some);
    ATF_ADD_TEST_CASE(tcs, find__match);
    ATF_ADD_TEST_CASE(tcs, find__nomatch);
}
