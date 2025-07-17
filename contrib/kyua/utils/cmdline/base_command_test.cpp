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

#include "utils/cmdline/base_command.ipp"

#include <atf-c++.hpp>

#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/cmdline/ui_mock.hpp"
#include "utils/defs.hpp"

namespace cmdline = utils::cmdline;


namespace {


/// Mock command to test the cmdline::base_command base class.
///
/// \param Data The type of the opaque data object passed to main().
/// \param ExpectedData The value run() will expect to find in the Data object
///     passed to main().
template< typename Data, Data ExpectedData >
class mock_cmd : public cmdline::base_command< Data > {
public:
    /// Indicates if run() has been called already and executed correctly.
    bool executed;

    /// Contains the argument of --the_string after run() is executed.
    std::string optvalue;

    /// Constructs a new mock command.
    mock_cmd(void) :
        cmdline::base_command< Data >("mock", "arg1 [arg2 [arg3]]", 1, 3,
                                      "Command for testing."),
        executed(false)
    {
        this->add_option(cmdline::string_option("the_string", "Test option",
                                                "arg"));
    }

    /// Executes the command.
    ///
    /// \param cmdline Representation of the command line to the subcommand.
    /// \param data Arbitrary data cookie passed to the command.
    ///
    /// \return A hardcoded number for testing purposes.
    int
    run(cmdline::ui* /* ui */,
        const cmdline::parsed_cmdline& cmdline, const Data& data)
    {
        if (cmdline.has_option("the_string"))
            optvalue = cmdline.get_option< cmdline::string_option >(
                "the_string");
        ATF_REQUIRE_EQ(ExpectedData, data);
        executed = true;
        return 1234;
    }
};


/// Mock command to test the cmdline::base_command_no_data base class.
class mock_cmd_no_data : public cmdline::base_command_no_data {
public:
    /// Indicates if run() has been called already and executed correctly.
    bool executed;

    /// Contains the argument of --the_string after run() is executed.
    std::string optvalue;

    /// Constructs a new mock command.
    mock_cmd_no_data(void) :
        cmdline::base_command_no_data("mock", "arg1 [arg2 [arg3]]", 1, 3,
                                      "Command for testing."),
        executed(false)
    {
        add_option(cmdline::string_option("the_string", "Test option", "arg"));
    }

    /// Executes the command.
    ///
    /// \param cmdline Representation of the command line to the subcommand.
    ///
    /// \return A hardcoded number for testing purposes.
    int
    run(cmdline::ui* /* ui */,
        const cmdline::parsed_cmdline& cmdline)
    {
        if (cmdline.has_option("the_string"))
            optvalue = cmdline.get_option< cmdline::string_option >(
                "the_string");
        executed = true;
        return 1234;
    }
};


/// Implementation of a command to get access to parse_cmdline().
class parse_cmdline_portal : public cmdline::command_proto {
public:
    /// Constructs a new mock command.
    parse_cmdline_portal(void) :
        cmdline::command_proto("portal", "arg1 [arg2 [arg3]]", 1, 3,
                               "Command for testing.")
    {
        this->add_option(cmdline::string_option("the_string", "Test option",
                                                "arg"));
    }

    /// Delegator for the internal parse_cmdline() method.
    ///
    /// \param args The input arguments to be parsed.
    ///
    /// \return The parsed command line, split in options and arguments.
    cmdline::parsed_cmdline
    operator()(const cmdline::args_vector& args) const
    {
        return parse_cmdline(args);
    }
};


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(command_proto__parse_cmdline__ok);
ATF_TEST_CASE_BODY(command_proto__parse_cmdline__ok)
{
    cmdline::args_vector args;
    args.push_back("portal");
    args.push_back("--the_string=foo bar");
    args.push_back("one arg");
    args.push_back("another arg");
    (void)parse_cmdline_portal()(args);
}


ATF_TEST_CASE_WITHOUT_HEAD(command_proto__parse_cmdline__parse_fail);
ATF_TEST_CASE_BODY(command_proto__parse_cmdline__parse_fail)
{
    cmdline::args_vector args;
    args.push_back("portal");
    args.push_back("--foo-bar");
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "Unknown.*foo-bar",
                         (void)parse_cmdline_portal()(args));
}


ATF_TEST_CASE_WITHOUT_HEAD(command_proto__parse_cmdline__args_invalid);
ATF_TEST_CASE_BODY(command_proto__parse_cmdline__args_invalid)
{
    cmdline::args_vector args;
    args.push_back("portal");

    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "Not enough arguments",
                         (void)parse_cmdline_portal()(args));

    args.push_back("1");
    args.push_back("2");
    args.push_back("3");
    args.push_back("4");
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "Too many arguments",
                         (void)parse_cmdline_portal()(args));
}


ATF_TEST_CASE_WITHOUT_HEAD(base_command__getters);
ATF_TEST_CASE_BODY(base_command__getters)
{
    mock_cmd< int, 584 > cmd;
    ATF_REQUIRE_EQ("mock", cmd.name());
    ATF_REQUIRE_EQ("arg1 [arg2 [arg3]]", cmd.arg_list());
    ATF_REQUIRE_EQ("Command for testing.", cmd.short_description());
    ATF_REQUIRE_EQ(1, cmd.options().size());
    ATF_REQUIRE_EQ("the_string", cmd.options()[0]->long_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(base_command__main__ok)
ATF_TEST_CASE_BODY(base_command__main__ok)
{
    mock_cmd< int, 584 > cmd;

    cmdline::ui_mock ui;
    cmdline::args_vector args;
    args.push_back("mock");
    args.push_back("--the_string=foo bar");
    args.push_back("one arg");
    args.push_back("another arg");
    ATF_REQUIRE_EQ(1234, cmd.main(&ui, args, 584));
    ATF_REQUIRE(cmd.executed);
    ATF_REQUIRE_EQ("foo bar", cmd.optvalue);
}


ATF_TEST_CASE_WITHOUT_HEAD(base_command__main__parse_cmdline_fail)
ATF_TEST_CASE_BODY(base_command__main__parse_cmdline_fail)
{
    mock_cmd< int, 584 > cmd;

    cmdline::ui_mock ui;
    cmdline::args_vector args;
    args.push_back("mock");
    args.push_back("--foo-bar");
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "Unknown.*foo-bar",
                         cmd.main(&ui, args, 584));
    ATF_REQUIRE(!cmd.executed);
}


ATF_TEST_CASE_WITHOUT_HEAD(base_command_no_data__getters);
ATF_TEST_CASE_BODY(base_command_no_data__getters)
{
    mock_cmd_no_data cmd;
    ATF_REQUIRE_EQ("mock", cmd.name());
    ATF_REQUIRE_EQ("arg1 [arg2 [arg3]]", cmd.arg_list());
    ATF_REQUIRE_EQ("Command for testing.", cmd.short_description());
    ATF_REQUIRE_EQ(1, cmd.options().size());
    ATF_REQUIRE_EQ("the_string", cmd.options()[0]->long_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(base_command_no_data__main__ok)
ATF_TEST_CASE_BODY(base_command_no_data__main__ok)
{
    mock_cmd_no_data cmd;

    cmdline::ui_mock ui;
    cmdline::args_vector args;
    args.push_back("mock");
    args.push_back("--the_string=foo bar");
    args.push_back("one arg");
    args.push_back("another arg");
    ATF_REQUIRE_EQ(1234, cmd.main(&ui, args));
    ATF_REQUIRE(cmd.executed);
    ATF_REQUIRE_EQ("foo bar", cmd.optvalue);
}


ATF_TEST_CASE_WITHOUT_HEAD(base_command_no_data__main__parse_cmdline_fail)
ATF_TEST_CASE_BODY(base_command_no_data__main__parse_cmdline_fail)
{
    mock_cmd_no_data cmd;

    cmdline::ui_mock ui;
    cmdline::args_vector args;
    args.push_back("mock");
    args.push_back("--foo-bar");
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "Unknown.*foo-bar",
                         cmd.main(&ui, args));
    ATF_REQUIRE(!cmd.executed);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, command_proto__parse_cmdline__ok);
    ATF_ADD_TEST_CASE(tcs, command_proto__parse_cmdline__parse_fail);
    ATF_ADD_TEST_CASE(tcs, command_proto__parse_cmdline__args_invalid);

    ATF_ADD_TEST_CASE(tcs, base_command__getters);
    ATF_ADD_TEST_CASE(tcs, base_command__main__ok);
    ATF_ADD_TEST_CASE(tcs, base_command__main__parse_cmdline_fail);

    ATF_ADD_TEST_CASE(tcs, base_command_no_data__getters);
    ATF_ADD_TEST_CASE(tcs, base_command_no_data__main__ok);
    ATF_ADD_TEST_CASE(tcs, base_command_no_data__main__parse_cmdline_fail);
}
