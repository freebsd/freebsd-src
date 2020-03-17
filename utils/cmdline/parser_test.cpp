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

#include "utils/cmdline/parser.ipp"

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

extern "C" {
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
}

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>

#include <atf-c++.hpp>

#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/format/macros.hpp"
#include "utils/sanity.hpp"

namespace cmdline = utils::cmdline;

using cmdline::base_option;
using cmdline::bool_option;
using cmdline::int_option;
using cmdline::parse;
using cmdline::parsed_cmdline;
using cmdline::string_option;


namespace {


/// Mock option type to check the validate and convert methods sequence.
///
/// Instances of this option accept a string argument that must be either "zero"
/// or "one".  These are validated and converted to integers.
class mock_option : public base_option {
public:
    /// Constructs the new option.
    ///
    /// \param long_name_ The long name for the option.  All other option
    ///     properties are irrelevant for the tests using this, so they are set
    ///     to arbitrary values.
    mock_option(const char* long_name_) :
        base_option(long_name_, "Irrelevant description", "arg")
    {
    }

    /// The type of the argument of this option.
    typedef int option_type;

    /// Checks that the user-provided option is valid.
    ///
    /// \param str The user argument; must be "zero" or "one".
    ///
    /// \throw cmdline::option_argument_value_error If str is not valid.
    void
    validate(const std::string& str) const
    {
        if (str != "zero" && str != "one")
            throw cmdline::option_argument_value_error(F("--%s") % long_name(),
                                                       str, "Unknown value");
    }

    /// Converts the user-provided argument to our native integer type.
    ///
    /// \param str The user argument; must be "zero" or "one".
    ///
    /// \return 0 if the input is "zero", or 1 if the input is "one".
    ///
    /// \throw std::runtime_error If str is not valid.  In real life, this
    ///     should be a precondition because validate() has already ensured that
    ///     the values passed to convert() are correct.  However, we raise an
    ///     exception here because we are actually validating that this code
    ///     sequence holds true.
    static int
    convert(const std::string& str)
    {
        if (str == "zero")
            return 0;
        else if (str == "one")
            return 1;
        else {
            // This would generally be an assertion but, given that this is
            // test code, we want to catch any errors regardless of how the
            // binary is built.
            throw std::runtime_error("Value not validated properly.");
        }
    }
};


/// Redirects stdout and stderr to a file.
///
/// This fails the test case in case of any error.
///
/// \param file The name of the file to redirect stdout and stderr to.
///
/// \return A copy of the old stdout and stderr file descriptors.
static std::pair< int, int >
mock_stdfds(const char* file)
{
    std::cout.flush();
    std::cerr.flush();

    const int oldout = ::dup(STDOUT_FILENO);
    ATF_REQUIRE(oldout != -1);
    const int olderr = ::dup(STDERR_FILENO);
    ATF_REQUIRE(olderr != -1);

    const int fd = ::open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    ATF_REQUIRE(fd != -1);
    ATF_REQUIRE(::dup2(fd, STDOUT_FILENO) != -1);
    ATF_REQUIRE(::dup2(fd, STDERR_FILENO) != -1);
    ::close(fd);

    return std::make_pair(oldout, olderr);
}


/// Restores stdout and stderr after a call to mock_stdfds.
///
/// \param oldfds The copy of the previous stdout and stderr as returned by the
///     call to mock_fds().
static void
restore_stdfds(const std::pair< int, int >& oldfds)
{
    ATF_REQUIRE(::dup2(oldfds.first, STDOUT_FILENO) != -1);
    ::close(oldfds.first);
    ATF_REQUIRE(::dup2(oldfds.second, STDERR_FILENO) != -1);
    ::close(oldfds.second);
}


/// Checks whether a '+:' prefix to the short options of getopt_long works.
///
/// It turns out that the getopt_long(3) implementation of Ubuntu 10.04.1 (and
/// very likely other distributions) does not properly report a missing argument
/// to a second long option as such.  Instead of returning ':' when the second
/// long option provided on the command line does not carry a required argument,
/// it will mistakenly return '?' which translates to "unknown option".
///
/// As a result of this bug, we cannot properly detect that 'flag2' requires an
/// argument in a command line like: 'progname --flag1=foo --flag2'.
///
/// I am not sure if we could fully workaround the issue in the implementation
/// of our library.  For the time being I am just using this bug detection in
/// the test cases to prevent failures that are not really our fault.
///
/// \return bool True if getopt_long is broken and does not interpret '+:'
///     correctly; False otherwise.
static bool
is_getopt_long_pluscolon_broken(void)
{
    struct ::option long_options[] = {
        { "flag1", 1, NULL, '1' },
        { "flag2", 1, NULL, '2' },
        { NULL, 0, NULL, 0 }
    };

    const int argc = 3;
    char* argv[4];
    argv[0] = ::strdup("progname");
    argv[1] = ::strdup("--flag1=a");
    argv[2] = ::strdup("--flag2");
    argv[3] = NULL;

    const int old_opterr = ::opterr;
    ::opterr = 0;

    bool got_colon = false;

    int opt;
    while ((opt = ::getopt_long(argc, argv, "+:", long_options, NULL)) != -1) {
        switch (opt) {
        case '1': break;
        case '2': break;
        case ':': got_colon = true; break;
        case '?': break;
        default:  UNREACHABLE; break;
        }
    }

    ::opterr = old_opterr;
    ::optind = 1;
#if defined(HAVE_GETOPT_WITH_OPTRESET)
    ::optreset = 1;
#endif

    for (char** arg = &argv[0]; *arg != NULL; arg++)
        std::free(*arg);

    return !got_colon;
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(progname__no_options);
ATF_TEST_CASE_BODY(progname__no_options)
{
    const int argc = 1;
    const char* const argv[] = {"progname", NULL};
    std::vector< const base_option* > options;
    const parsed_cmdline cmdline = parse(argc, argv, options);

    ATF_REQUIRE(cmdline.arguments().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(progname__some_options);
ATF_TEST_CASE_BODY(progname__some_options)
{
    const int argc = 1;
    const char* const argv[] = {"progname", NULL};
    const string_option a('a', "a_option", "Foo", NULL);
    const string_option b('b', "b_option", "Bar", "arg", "foo");
    const string_option c("c_option", "Baz", NULL);
    const string_option d("d_option", "Wohoo", "arg", "bar");
    std::vector< const base_option* > options;
    options.push_back(&a);
    options.push_back(&b);
    options.push_back(&c);
    options.push_back(&d);
    const parsed_cmdline cmdline = parse(argc, argv, options);

    ATF_REQUIRE_EQ("foo", cmdline.get_option< string_option >("b_option"));
    ATF_REQUIRE_EQ("bar", cmdline.get_option< string_option >("d_option"));
    ATF_REQUIRE(cmdline.arguments().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(some_args__no_options);
ATF_TEST_CASE_BODY(some_args__no_options)
{
    const int argc = 5;
    const char* const argv[] = {"progname", "foo", "-c", "--opt", "bar", NULL};
    std::vector< const base_option* > options;
    const parsed_cmdline cmdline = parse(argc, argv, options);

    ATF_REQUIRE(!cmdline.has_option("c"));
    ATF_REQUIRE(!cmdline.has_option("opt"));
    ATF_REQUIRE_EQ(4, cmdline.arguments().size());
    ATF_REQUIRE_EQ("foo", cmdline.arguments()[0]);
    ATF_REQUIRE_EQ("-c", cmdline.arguments()[1]);
    ATF_REQUIRE_EQ("--opt", cmdline.arguments()[2]);
    ATF_REQUIRE_EQ("bar", cmdline.arguments()[3]);
}


ATF_TEST_CASE_WITHOUT_HEAD(some_args__some_options);
ATF_TEST_CASE_BODY(some_args__some_options)
{
    const int argc = 5;
    const char* const argv[] = {"progname", "foo", "-c", "--opt", "bar", NULL};
    const string_option c('c', "opt", "Description", NULL);
    std::vector< const base_option* > options;
    options.push_back(&c);
    const parsed_cmdline cmdline = parse(argc, argv, options);

    ATF_REQUIRE(!cmdline.has_option("c"));
    ATF_REQUIRE(!cmdline.has_option("opt"));
    ATF_REQUIRE_EQ(4, cmdline.arguments().size());
    ATF_REQUIRE_EQ("foo", cmdline.arguments()[0]);
    ATF_REQUIRE_EQ("-c", cmdline.arguments()[1]);
    ATF_REQUIRE_EQ("--opt", cmdline.arguments()[2]);
    ATF_REQUIRE_EQ("bar", cmdline.arguments()[3]);
}


ATF_TEST_CASE_WITHOUT_HEAD(some_options__all_known);
ATF_TEST_CASE_BODY(some_options__all_known)
{
    const int argc = 14;
    const char* const argv[] = {
        "progname",
        "-a",
        "-bvalue_b",
        "-c", "value_c",
        //"-d",  // Options with default optional values are unsupported.
        "-evalue_e",  // Has default; overriden.
        "--f_long",
        "--g_long=value_g",
        "--h_long", "value_h",
        //"--i_long",  // Options with default optional values are unsupported.
        "--j_long", "value_j",  // Has default; overriden as separate argument.
        "arg1", "arg2", NULL,
    };
    const bool_option a('a', "a_long", "");
    const string_option b('b', "b_long", "Description", "arg");
    const string_option c('c', "c_long", "ABCD", "foo");
    const string_option d('d', "d_long", "Description", "bar", "default_d");
    const string_option e('e', "e_long", "Description", "baz", "default_e");
    const bool_option f("f_long", "Description");
    const string_option g("g_long", "Description", "arg");
    const string_option h("h_long", "Description", "foo");
    const string_option i("i_long", "EFGH", "bar", "default_i");
    const string_option j("j_long", "Description", "baz", "default_j");
    std::vector< const base_option* > options;
    options.push_back(&a);
    options.push_back(&b);
    options.push_back(&c);
    options.push_back(&d);
    options.push_back(&e);
    options.push_back(&f);
    options.push_back(&g);
    options.push_back(&h);
    options.push_back(&i);
    options.push_back(&j);
    const parsed_cmdline cmdline = parse(argc, argv, options);

    ATF_REQUIRE(cmdline.has_option("a_long"));
    ATF_REQUIRE_EQ("value_b", cmdline.get_option< string_option >("b_long"));
    ATF_REQUIRE_EQ("value_c", cmdline.get_option< string_option >("c_long"));
    ATF_REQUIRE_EQ("default_d", cmdline.get_option< string_option >("d_long"));
    ATF_REQUIRE_EQ("value_e", cmdline.get_option< string_option >("e_long"));
    ATF_REQUIRE(cmdline.has_option("f_long"));
    ATF_REQUIRE_EQ("value_g", cmdline.get_option< string_option >("g_long"));
    ATF_REQUIRE_EQ("value_h", cmdline.get_option< string_option >("h_long"));
    ATF_REQUIRE_EQ("default_i", cmdline.get_option< string_option >("i_long"));
    ATF_REQUIRE_EQ("value_j", cmdline.get_option< string_option >("j_long"));
    ATF_REQUIRE_EQ(2, cmdline.arguments().size());
    ATF_REQUIRE_EQ("arg1", cmdline.arguments()[0]);
    ATF_REQUIRE_EQ("arg2", cmdline.arguments()[1]);
}


ATF_TEST_CASE_WITHOUT_HEAD(some_options__multi);
ATF_TEST_CASE_BODY(some_options__multi)
{
    const int argc = 9;
    const char* const argv[] = {
        "progname",
        "-a1",
        "-bvalue1",
        "-a2",
        "--a_long=3",
        "-bvalue2",
        "--b_long=value3",
        "arg1", "arg2", NULL,
    };
    const int_option a('a', "a_long", "Description", "arg");
    const string_option b('b', "b_long", "Description", "arg");
    std::vector< const base_option* > options;
    options.push_back(&a);
    options.push_back(&b);
    const parsed_cmdline cmdline = parse(argc, argv, options);

    {
        ATF_REQUIRE_EQ(3, cmdline.get_option< int_option >("a_long"));
        const std::vector< int > multi =
            cmdline.get_multi_option< int_option >("a_long");
        ATF_REQUIRE_EQ(3, multi.size());
        ATF_REQUIRE_EQ(1, multi[0]);
        ATF_REQUIRE_EQ(2, multi[1]);
        ATF_REQUIRE_EQ(3, multi[2]);
    }

    {
        ATF_REQUIRE_EQ("value3", cmdline.get_option< string_option >("b_long"));
        const std::vector< std::string > multi =
            cmdline.get_multi_option< string_option >("b_long");
        ATF_REQUIRE_EQ(3, multi.size());
        ATF_REQUIRE_EQ("value1", multi[0]);
        ATF_REQUIRE_EQ("value2", multi[1]);
        ATF_REQUIRE_EQ("value3", multi[2]);
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(subcommands);
ATF_TEST_CASE_BODY(subcommands)
{
    const int argc = 5;
    const char* const argv[] = {"progname", "--flag1", "subcommand",
                                "--flag2", "arg", NULL};
    const bool_option flag1("flag1", "");
    std::vector< const base_option* > options;
    options.push_back(&flag1);
    const parsed_cmdline cmdline = parse(argc, argv, options);

    ATF_REQUIRE( cmdline.has_option("flag1"));
    ATF_REQUIRE(!cmdline.has_option("flag2"));
    ATF_REQUIRE_EQ(3, cmdline.arguments().size());
    ATF_REQUIRE_EQ("subcommand", cmdline.arguments()[0]);
    ATF_REQUIRE_EQ("--flag2", cmdline.arguments()[1]);
    ATF_REQUIRE_EQ("arg", cmdline.arguments()[2]);

    const bool_option flag2("flag2", "");
    std::vector< const base_option* > options2;
    options2.push_back(&flag2);
    const parsed_cmdline cmdline2 = parse(cmdline.arguments(), options2);

    ATF_REQUIRE(!cmdline2.has_option("flag1"));
    ATF_REQUIRE( cmdline2.has_option("flag2"));
    ATF_REQUIRE_EQ(1, cmdline2.arguments().size());
    ATF_REQUIRE_EQ("arg", cmdline2.arguments()[0]);
}


ATF_TEST_CASE_WITHOUT_HEAD(missing_option_argument_error__short);
ATF_TEST_CASE_BODY(missing_option_argument_error__short)
{
    const int argc = 3;
    const char* const argv[] = {"progname", "-a3", "-b", NULL};
    const string_option flag1('a', "flag1", "Description", "arg");
    const string_option flag2('b', "flag2", "Description", "arg");
    std::vector< const base_option* > options;
    options.push_back(&flag1);
    options.push_back(&flag2);

    try {
        parse(argc, argv, options);
        fail("missing_option_argument_error not raised");
    } catch (const cmdline::missing_option_argument_error& e) {
        ATF_REQUIRE_EQ("-b", e.option());
    } catch (const cmdline::unknown_option_error& e) {
        if (is_getopt_long_pluscolon_broken())
            expect_fail("Your getopt_long is broken");
        fail("Got unknown_option_error instead of "
             "missing_option_argument_error");
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(missing_option_argument_error__shortblock);
ATF_TEST_CASE_BODY(missing_option_argument_error__shortblock)
{
    const int argc = 3;
    const char* const argv[] = {"progname", "-ab3", "-ac", NULL};
    const bool_option flag1('a', "flag1", "Description");
    const string_option flag2('b', "flag2", "Description", "arg");
    const string_option flag3('c', "flag2", "Description", "arg");
    std::vector< const base_option* > options;
    options.push_back(&flag1);
    options.push_back(&flag2);
    options.push_back(&flag3);

    try {
        parse(argc, argv, options);
        fail("missing_option_argument_error not raised");
    } catch (const cmdline::missing_option_argument_error& e) {
        ATF_REQUIRE_EQ("-c", e.option());
    } catch (const cmdline::unknown_option_error& e) {
        if (is_getopt_long_pluscolon_broken())
            expect_fail("Your getopt_long is broken");
        fail("Got unknown_option_error instead of "
             "missing_option_argument_error");
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(missing_option_argument_error__long);
ATF_TEST_CASE_BODY(missing_option_argument_error__long)
{
    const int argc = 3;
    const char* const argv[] = {"progname", "--flag1=a", "--flag2", NULL};
    const string_option flag1("flag1", "Description", "arg");
    const string_option flag2("flag2", "Description", "arg");
    std::vector< const base_option* > options;
    options.push_back(&flag1);
    options.push_back(&flag2);

    try {
        parse(argc, argv, options);
        fail("missing_option_argument_error not raised");
    } catch (const cmdline::missing_option_argument_error& e) {
        ATF_REQUIRE_EQ("--flag2", e.option());
    } catch (const cmdline::unknown_option_error& e) {
        if (is_getopt_long_pluscolon_broken())
            expect_fail("Your getopt_long is broken");
        fail("Got unknown_option_error instead of "
             "missing_option_argument_error");
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(unknown_option_error__short);
ATF_TEST_CASE_BODY(unknown_option_error__short)
{
    const int argc = 3;
    const char* const argv[] = {"progname", "-a", "-b", NULL};
    const bool_option flag1('a', "flag1", "Description");
    std::vector< const base_option* > options;
    options.push_back(&flag1);

    try {
        parse(argc, argv, options);
        fail("unknown_option_error not raised");
    } catch (const cmdline::unknown_option_error& e) {
        ATF_REQUIRE_EQ("-b", e.option());
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(unknown_option_error__shortblock);
ATF_TEST_CASE_BODY(unknown_option_error__shortblock)
{
    const int argc = 3;
    const char* const argv[] = {"progname", "-a", "-bdc", NULL};
    const bool_option flag1('a', "flag1", "Description");
    const bool_option flag2('b', "flag2", "Description");
    const bool_option flag3('c', "flag3", "Description");
    std::vector< const base_option* > options;
    options.push_back(&flag1);
    options.push_back(&flag2);
    options.push_back(&flag3);

    try {
        parse(argc, argv, options);
        fail("unknown_option_error not raised");
    } catch (const cmdline::unknown_option_error& e) {
        ATF_REQUIRE_EQ("-d", e.option());
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(unknown_option_error__long);
ATF_TEST_CASE_BODY(unknown_option_error__long)
{
    const int argc = 3;
    const char* const argv[] = {"progname", "--flag1=a", "--flag2", NULL};
    const string_option flag1("flag1", "Description", "arg");
    std::vector< const base_option* > options;
    options.push_back(&flag1);

    try {
        parse(argc, argv, options);
        fail("unknown_option_error not raised");
    } catch (const cmdline::unknown_option_error& e) {
        ATF_REQUIRE_EQ("--flag2", e.option());
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(unknown_plus_option_error);
ATF_TEST_CASE_BODY(unknown_plus_option_error)
{
    const int argc = 2;
    const char* const argv[] = {"progname", "-+", NULL};
    const cmdline::options_vector options;

    try {
        parse(argc, argv, options);
        fail("unknown_option_error not raised");
    } catch (const cmdline::unknown_option_error& e) {
        ATF_REQUIRE_EQ("-+", e.option());
    } catch (const cmdline::missing_option_argument_error& e) {
        fail("Looks like getopt_long thinks a + option is defined and it "
             "even requires an argument");
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(option_types);
ATF_TEST_CASE_BODY(option_types)
{
    const int argc = 3;
    const char* const argv[] = {"progname", "--flag1=a", "--flag2=one", NULL};
    const string_option flag1("flag1", "The flag1", "arg");
    const mock_option flag2("flag2");
    std::vector< const base_option* > options;
    options.push_back(&flag1);
    options.push_back(&flag2);

    const parsed_cmdline cmdline = parse(argc, argv, options);

    ATF_REQUIRE(cmdline.has_option("flag1"));
    ATF_REQUIRE(cmdline.has_option("flag2"));
    ATF_REQUIRE_EQ("a", cmdline.get_option< string_option >("flag1"));
    ATF_REQUIRE_EQ(1, cmdline.get_option< mock_option >("flag2"));
}


ATF_TEST_CASE_WITHOUT_HEAD(option_validation_error);
ATF_TEST_CASE_BODY(option_validation_error)
{
    const int argc = 3;
    const char* const argv[] = {"progname", "--flag1=zero", "--flag2=foo",
                                NULL};
    const mock_option flag1("flag1");
    const mock_option flag2("flag2");
    std::vector< const base_option* > options;
    options.push_back(&flag1);
    options.push_back(&flag2);

    try {
        parse(argc, argv, options);
        fail("option_argument_value_error not raised");
    } catch (const cmdline::option_argument_value_error& e) {
        ATF_REQUIRE_EQ("--flag2", e.option());
        ATF_REQUIRE_EQ("foo", e.argument());
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(silent_errors);
ATF_TEST_CASE_BODY(silent_errors)
{
    const int argc = 2;
    const char* const argv[] = {"progname", "-h", NULL};
    cmdline::options_vector options;

    try {
        std::pair< int, int > oldfds = mock_stdfds("output.txt");
        try {
            parse(argc, argv, options);
        } catch (...) {
            restore_stdfds(oldfds);
            throw;
        }
        restore_stdfds(oldfds);
        fail("unknown_option_error not raised");
    } catch (const cmdline::unknown_option_error& e) {
        ATF_REQUIRE_EQ("-h", e.option());
    }

    std::ifstream input("output.txt");
    ATF_REQUIRE(input);

    bool has_output = false;
    std::string line;
    while (std::getline(input, line).good()) {
        std::cout << line << '\n';
        has_output = true;
    }

    if (has_output)
        fail("getopt_long printed messages on stdout/stderr by itself");
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, progname__no_options);
    ATF_ADD_TEST_CASE(tcs, progname__some_options);
    ATF_ADD_TEST_CASE(tcs, some_args__no_options);
    ATF_ADD_TEST_CASE(tcs, some_args__some_options);
    ATF_ADD_TEST_CASE(tcs, some_options__all_known);
    ATF_ADD_TEST_CASE(tcs, some_options__multi);
    ATF_ADD_TEST_CASE(tcs, subcommands);
    ATF_ADD_TEST_CASE(tcs, missing_option_argument_error__short);
    ATF_ADD_TEST_CASE(tcs, missing_option_argument_error__shortblock);
    ATF_ADD_TEST_CASE(tcs, missing_option_argument_error__long);
    ATF_ADD_TEST_CASE(tcs, unknown_option_error__short);
    ATF_ADD_TEST_CASE(tcs, unknown_option_error__shortblock);
    ATF_ADD_TEST_CASE(tcs, unknown_option_error__long);
    ATF_ADD_TEST_CASE(tcs, unknown_plus_option_error);
    ATF_ADD_TEST_CASE(tcs, option_types);
    ATF_ADD_TEST_CASE(tcs, option_validation_error);
    ATF_ADD_TEST_CASE(tcs, silent_errors);
}
