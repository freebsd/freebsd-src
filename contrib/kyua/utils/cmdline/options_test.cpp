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

#include "utils/cmdline/options.hpp"

#include <atf-c++.hpp>

#include "utils/cmdline/exceptions.hpp"
#include "utils/defs.hpp"
#include "utils/fs/path.hpp"

namespace cmdline = utils::cmdline;

namespace {


/// Simple string-based option type for testing purposes.
class mock_option : public cmdline::base_option {
public:
    /// Constructs a mock option with a short name and a long name.
    ///
    ///
    /// \param short_name_ The short name for the option.
    /// \param long_name_ The long name for the option.
    /// \param description_ A user-friendly description for the option.
    /// \param arg_name_ If not NULL, specifies that the option must receive an
    ///     argument and specifies the name of such argument for documentation
    ///     purposes.
    /// \param default_value_ If not NULL, specifies that the option has a
    ///     default value for the mandatory argument.
    mock_option(const char short_name_, const char* long_name_,
                  const char* description_, const char* arg_name_ = NULL,
                  const char* default_value_ = NULL) :
        base_option(short_name_, long_name_, description_, arg_name_,
                    default_value_) {}

    /// Constructs a mock option with a long name only.
    ///
    /// \param long_name_ The long name for the option.
    /// \param description_ A user-friendly description for the option.
    /// \param arg_name_ If not NULL, specifies that the option must receive an
    ///     argument and specifies the name of such argument for documentation
    ///     purposes.
    /// \param default_value_ If not NULL, specifies that the option has a
    ///     default value for the mandatory argument.
    mock_option(const char* long_name_,
                  const char* description_, const char* arg_name_ = NULL,
                  const char* default_value_ = NULL) :
        base_option(long_name_, description_, arg_name_, default_value_) {}

    /// The data type of this option.
    typedef std::string option_type;

    /// Ensures that the argument passed to the option is valid.
    ///
    /// In this particular mock option, this does not perform any validation.
    void
    validate(const std::string& /* str */) const
    {
        // Do nothing.
    }

    /// Returns the input parameter without any conversion.
    ///
    /// \param str The user-provided argument to the option.
    ///
    /// \return The same value as provided by the user without conversion.
    static std::string
    convert(const std::string& str)
    {
        return str;
    }
};


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(base_option__short_name__no_arg);
ATF_TEST_CASE_BODY(base_option__short_name__no_arg)
{
    const mock_option o('f', "force", "Force execution");
    ATF_REQUIRE(o.has_short_name());
    ATF_REQUIRE_EQ('f', o.short_name());
    ATF_REQUIRE_EQ("force", o.long_name());
    ATF_REQUIRE_EQ("Force execution", o.description());
    ATF_REQUIRE(!o.needs_arg());
    ATF_REQUIRE_EQ("-f", o.format_short_name());
    ATF_REQUIRE_EQ("--force", o.format_long_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(base_option__short_name__with_arg__no_default);
ATF_TEST_CASE_BODY(base_option__short_name__with_arg__no_default)
{
    const mock_option o('c', "conf_file", "Configuration file", "path");
    ATF_REQUIRE(o.has_short_name());
    ATF_REQUIRE_EQ('c', o.short_name());
    ATF_REQUIRE_EQ("conf_file", o.long_name());
    ATF_REQUIRE_EQ("Configuration file", o.description());
    ATF_REQUIRE(o.needs_arg());
    ATF_REQUIRE_EQ("path", o.arg_name());
    ATF_REQUIRE(!o.has_default_value());
    ATF_REQUIRE_EQ("-c path", o.format_short_name());
    ATF_REQUIRE_EQ("--conf_file=path", o.format_long_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(base_option__short_name__with_arg__with_default);
ATF_TEST_CASE_BODY(base_option__short_name__with_arg__with_default)
{
    const mock_option o('c', "conf_file", "Configuration file", "path",
                        "defpath");
    ATF_REQUIRE(o.has_short_name());
    ATF_REQUIRE_EQ('c', o.short_name());
    ATF_REQUIRE_EQ("conf_file", o.long_name());
    ATF_REQUIRE_EQ("Configuration file", o.description());
    ATF_REQUIRE(o.needs_arg());
    ATF_REQUIRE_EQ("path", o.arg_name());
    ATF_REQUIRE(o.has_default_value());
    ATF_REQUIRE_EQ("defpath", o.default_value());
    ATF_REQUIRE_EQ("-c path", o.format_short_name());
    ATF_REQUIRE_EQ("--conf_file=path", o.format_long_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(base_option__long_name__no_arg);
ATF_TEST_CASE_BODY(base_option__long_name__no_arg)
{
    const mock_option o("dryrun", "Dry run mode");
    ATF_REQUIRE(!o.has_short_name());
    ATF_REQUIRE_EQ("dryrun", o.long_name());
    ATF_REQUIRE_EQ("Dry run mode", o.description());
    ATF_REQUIRE(!o.needs_arg());
    ATF_REQUIRE_EQ("--dryrun", o.format_long_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(base_option__long_name__with_arg__no_default);
ATF_TEST_CASE_BODY(base_option__long_name__with_arg__no_default)
{
    const mock_option o("helper", "Path to helper", "path");
    ATF_REQUIRE(!o.has_short_name());
    ATF_REQUIRE_EQ("helper", o.long_name());
    ATF_REQUIRE_EQ("Path to helper", o.description());
    ATF_REQUIRE(o.needs_arg());
    ATF_REQUIRE_EQ("path", o.arg_name());
    ATF_REQUIRE(!o.has_default_value());
    ATF_REQUIRE_EQ("--helper=path", o.format_long_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(base_option__long_name__with_arg__with_default);
ATF_TEST_CASE_BODY(base_option__long_name__with_arg__with_default)
{
    const mock_option o("executable", "Executable name", "file", "foo");
    ATF_REQUIRE(!o.has_short_name());
    ATF_REQUIRE_EQ("executable", o.long_name());
    ATF_REQUIRE_EQ("Executable name", o.description());
    ATF_REQUIRE(o.needs_arg());
    ATF_REQUIRE_EQ("file", o.arg_name());
    ATF_REQUIRE(o.has_default_value());
    ATF_REQUIRE_EQ("foo", o.default_value());
    ATF_REQUIRE_EQ("--executable=file", o.format_long_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(bool_option__short_name);
ATF_TEST_CASE_BODY(bool_option__short_name)
{
    const cmdline::bool_option o('f', "force", "Force execution");
    ATF_REQUIRE(o.has_short_name());
    ATF_REQUIRE_EQ('f', o.short_name());
    ATF_REQUIRE_EQ("force", o.long_name());
    ATF_REQUIRE_EQ("Force execution", o.description());
    ATF_REQUIRE(!o.needs_arg());
}


ATF_TEST_CASE_WITHOUT_HEAD(bool_option__long_name);
ATF_TEST_CASE_BODY(bool_option__long_name)
{
    const cmdline::bool_option o("force", "Force execution");
    ATF_REQUIRE(!o.has_short_name());
    ATF_REQUIRE_EQ("force", o.long_name());
    ATF_REQUIRE_EQ("Force execution", o.description());
    ATF_REQUIRE(!o.needs_arg());
}


ATF_TEST_CASE_WITHOUT_HEAD(int_option__short_name);
ATF_TEST_CASE_BODY(int_option__short_name)
{
    const cmdline::int_option o('p', "int", "The int", "arg", "value");
    ATF_REQUIRE(o.has_short_name());
    ATF_REQUIRE_EQ('p', o.short_name());
    ATF_REQUIRE_EQ("int", o.long_name());
    ATF_REQUIRE_EQ("The int", o.description());
    ATF_REQUIRE(o.needs_arg());
    ATF_REQUIRE_EQ("arg", o.arg_name());
    ATF_REQUIRE(o.has_default_value());
    ATF_REQUIRE_EQ("value", o.default_value());
}


ATF_TEST_CASE_WITHOUT_HEAD(int_option__long_name);
ATF_TEST_CASE_BODY(int_option__long_name)
{
    const cmdline::int_option o("int", "The int", "arg", "value");
    ATF_REQUIRE(!o.has_short_name());
    ATF_REQUIRE_EQ("int", o.long_name());
    ATF_REQUIRE_EQ("The int", o.description());
    ATF_REQUIRE(o.needs_arg());
    ATF_REQUIRE_EQ("arg", o.arg_name());
    ATF_REQUIRE(o.has_default_value());
    ATF_REQUIRE_EQ("value", o.default_value());
}


ATF_TEST_CASE_WITHOUT_HEAD(int_option__type);
ATF_TEST_CASE_BODY(int_option__type)
{
    const cmdline::int_option o("int", "The int", "arg");

    o.validate("123");
    ATF_REQUIRE_EQ(123, cmdline::int_option::convert("123"));

    o.validate("-567");
    ATF_REQUIRE_EQ(-567, cmdline::int_option::convert("-567"));

    ATF_REQUIRE_THROW(cmdline::option_argument_value_error, o.validate(""));
    ATF_REQUIRE_THROW(cmdline::option_argument_value_error, o.validate("5a"));
    ATF_REQUIRE_THROW(cmdline::option_argument_value_error, o.validate("a5"));
    ATF_REQUIRE_THROW(cmdline::option_argument_value_error, o.validate("5 a"));
    ATF_REQUIRE_THROW(cmdline::option_argument_value_error, o.validate("5.0"));
}


ATF_TEST_CASE_WITHOUT_HEAD(list_option__short_name);
ATF_TEST_CASE_BODY(list_option__short_name)
{
    const cmdline::list_option o('p', "list", "The list", "arg", "value");
    ATF_REQUIRE(o.has_short_name());
    ATF_REQUIRE_EQ('p', o.short_name());
    ATF_REQUIRE_EQ("list", o.long_name());
    ATF_REQUIRE_EQ("The list", o.description());
    ATF_REQUIRE(o.needs_arg());
    ATF_REQUIRE_EQ("arg", o.arg_name());
    ATF_REQUIRE(o.has_default_value());
    ATF_REQUIRE_EQ("value", o.default_value());
}


ATF_TEST_CASE_WITHOUT_HEAD(list_option__long_name);
ATF_TEST_CASE_BODY(list_option__long_name)
{
    const cmdline::list_option o("list", "The list", "arg", "value");
    ATF_REQUIRE(!o.has_short_name());
    ATF_REQUIRE_EQ("list", o.long_name());
    ATF_REQUIRE_EQ("The list", o.description());
    ATF_REQUIRE(o.needs_arg());
    ATF_REQUIRE_EQ("arg", o.arg_name());
    ATF_REQUIRE(o.has_default_value());
    ATF_REQUIRE_EQ("value", o.default_value());
}


ATF_TEST_CASE_WITHOUT_HEAD(list_option__type);
ATF_TEST_CASE_BODY(list_option__type)
{
    const cmdline::list_option o("list", "The list", "arg");

    o.validate("");
    {
        const cmdline::list_option::option_type words =
            cmdline::list_option::convert("");
        ATF_REQUIRE(words.empty());
    }

    o.validate("foo");
    {
        const cmdline::list_option::option_type words =
            cmdline::list_option::convert("foo");
        ATF_REQUIRE_EQ(1, words.size());
        ATF_REQUIRE_EQ("foo", words[0]);
    }

    o.validate("foo,bar,baz");
    {
        const cmdline::list_option::option_type words =
            cmdline::list_option::convert("foo,bar,baz");
        ATF_REQUIRE_EQ(3, words.size());
        ATF_REQUIRE_EQ("foo", words[0]);
        ATF_REQUIRE_EQ("bar", words[1]);
        ATF_REQUIRE_EQ("baz", words[2]);
    }

    o.validate("foo,bar,");
    {
        const cmdline::list_option::option_type words =
            cmdline::list_option::convert("foo,bar,");
        ATF_REQUIRE_EQ(3, words.size());
        ATF_REQUIRE_EQ("foo", words[0]);
        ATF_REQUIRE_EQ("bar", words[1]);
        ATF_REQUIRE_EQ("", words[2]);
    }

    o.validate(",foo,bar");
    {
        const cmdline::list_option::option_type words =
            cmdline::list_option::convert(",foo,bar");
        ATF_REQUIRE_EQ(3, words.size());
        ATF_REQUIRE_EQ("", words[0]);
        ATF_REQUIRE_EQ("foo", words[1]);
        ATF_REQUIRE_EQ("bar", words[2]);
    }

    o.validate("foo,,bar");
    {
        const cmdline::list_option::option_type words =
            cmdline::list_option::convert("foo,,bar");
        ATF_REQUIRE_EQ(3, words.size());
        ATF_REQUIRE_EQ("foo", words[0]);
        ATF_REQUIRE_EQ("", words[1]);
        ATF_REQUIRE_EQ("bar", words[2]);
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(path_option__short_name);
ATF_TEST_CASE_BODY(path_option__short_name)
{
    const cmdline::path_option o('p', "path", "The path", "arg", "value");
    ATF_REQUIRE(o.has_short_name());
    ATF_REQUIRE_EQ('p', o.short_name());
    ATF_REQUIRE_EQ("path", o.long_name());
    ATF_REQUIRE_EQ("The path", o.description());
    ATF_REQUIRE(o.needs_arg());
    ATF_REQUIRE_EQ("arg", o.arg_name());
    ATF_REQUIRE(o.has_default_value());
    ATF_REQUIRE_EQ("value", o.default_value());
}


ATF_TEST_CASE_WITHOUT_HEAD(path_option__long_name);
ATF_TEST_CASE_BODY(path_option__long_name)
{
    const cmdline::path_option o("path", "The path", "arg", "value");
    ATF_REQUIRE(!o.has_short_name());
    ATF_REQUIRE_EQ("path", o.long_name());
    ATF_REQUIRE_EQ("The path", o.description());
    ATF_REQUIRE(o.needs_arg());
    ATF_REQUIRE_EQ("arg", o.arg_name());
    ATF_REQUIRE(o.has_default_value());
    ATF_REQUIRE_EQ("value", o.default_value());
}


ATF_TEST_CASE_WITHOUT_HEAD(path_option__type);
ATF_TEST_CASE_BODY(path_option__type)
{
    const cmdline::path_option o("path", "The path", "arg");

    o.validate("/some/path");

    try {
        o.validate("");
        fail("option_argument_value_error not raised");
    } catch (const cmdline::option_argument_value_error& e) {
        // Expected; ignore.
    }

    const cmdline::path_option::option_type path =
        cmdline::path_option::convert("/foo/bar");
    ATF_REQUIRE_EQ("bar", path.leaf_name());  // Ensure valid type.
}


ATF_TEST_CASE_WITHOUT_HEAD(property_option__short_name);
ATF_TEST_CASE_BODY(property_option__short_name)
{
    const cmdline::property_option o('p', "property", "The property", "a=b");
    ATF_REQUIRE(o.has_short_name());
    ATF_REQUIRE_EQ('p', o.short_name());
    ATF_REQUIRE_EQ("property", o.long_name());
    ATF_REQUIRE_EQ("The property", o.description());
    ATF_REQUIRE(o.needs_arg());
    ATF_REQUIRE_EQ("a=b", o.arg_name());
    ATF_REQUIRE(!o.has_default_value());
}


ATF_TEST_CASE_WITHOUT_HEAD(property_option__long_name);
ATF_TEST_CASE_BODY(property_option__long_name)
{
    const cmdline::property_option o("property", "The property", "a=b");
    ATF_REQUIRE(!o.has_short_name());
    ATF_REQUIRE_EQ("property", o.long_name());
    ATF_REQUIRE_EQ("The property", o.description());
    ATF_REQUIRE(o.needs_arg());
    ATF_REQUIRE_EQ("a=b", o.arg_name());
    ATF_REQUIRE(!o.has_default_value());
}


ATF_TEST_CASE_WITHOUT_HEAD(property_option__type);
ATF_TEST_CASE_BODY(property_option__type)
{
    typedef std::pair< std::string, std::string > string_pair;
    const cmdline::property_option o("property", "The property", "a=b");

    o.validate("foo=bar");
    ATF_REQUIRE(string_pair("foo", "bar") ==
                cmdline::property_option::convert("foo=bar"));

    o.validate(" foo  = bar  baz");
    ATF_REQUIRE(string_pair(" foo  ", " bar  baz") ==
                cmdline::property_option::convert(" foo  = bar  baz"));

    ATF_REQUIRE_THROW(cmdline::option_argument_value_error, o.validate(""));
    ATF_REQUIRE_THROW(cmdline::option_argument_value_error, o.validate("="));
    ATF_REQUIRE_THROW(cmdline::option_argument_value_error, o.validate("a="));
    ATF_REQUIRE_THROW(cmdline::option_argument_value_error, o.validate("=b"));
}


ATF_TEST_CASE_WITHOUT_HEAD(string_option__short_name);
ATF_TEST_CASE_BODY(string_option__short_name)
{
    const cmdline::string_option o('p', "string", "The string", "arg", "value");
    ATF_REQUIRE(o.has_short_name());
    ATF_REQUIRE_EQ('p', o.short_name());
    ATF_REQUIRE_EQ("string", o.long_name());
    ATF_REQUIRE_EQ("The string", o.description());
    ATF_REQUIRE(o.needs_arg());
    ATF_REQUIRE_EQ("arg", o.arg_name());
    ATF_REQUIRE(o.has_default_value());
    ATF_REQUIRE_EQ("value", o.default_value());
}


ATF_TEST_CASE_WITHOUT_HEAD(string_option__long_name);
ATF_TEST_CASE_BODY(string_option__long_name)
{
    const cmdline::string_option o("string", "The string", "arg", "value");
    ATF_REQUIRE(!o.has_short_name());
    ATF_REQUIRE_EQ("string", o.long_name());
    ATF_REQUIRE_EQ("The string", o.description());
    ATF_REQUIRE(o.needs_arg());
    ATF_REQUIRE_EQ("arg", o.arg_name());
    ATF_REQUIRE(o.has_default_value());
    ATF_REQUIRE_EQ("value", o.default_value());
}


ATF_TEST_CASE_WITHOUT_HEAD(string_option__type);
ATF_TEST_CASE_BODY(string_option__type)
{
    const cmdline::string_option o("string", "The string", "foo");

    o.validate("");
    o.validate("some string");

    const cmdline::string_option::option_type string =
        cmdline::string_option::convert("foo");
    ATF_REQUIRE_EQ(3, string.length());  // Ensure valid type.
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, base_option__short_name__no_arg);
    ATF_ADD_TEST_CASE(tcs, base_option__short_name__with_arg__no_default);
    ATF_ADD_TEST_CASE(tcs, base_option__short_name__with_arg__with_default);
    ATF_ADD_TEST_CASE(tcs, base_option__long_name__no_arg);
    ATF_ADD_TEST_CASE(tcs, base_option__long_name__with_arg__no_default);
    ATF_ADD_TEST_CASE(tcs, base_option__long_name__with_arg__with_default);

    ATF_ADD_TEST_CASE(tcs, bool_option__short_name);
    ATF_ADD_TEST_CASE(tcs, bool_option__long_name);

    ATF_ADD_TEST_CASE(tcs, int_option__short_name);
    ATF_ADD_TEST_CASE(tcs, int_option__long_name);
    ATF_ADD_TEST_CASE(tcs, int_option__type);

    ATF_ADD_TEST_CASE(tcs, list_option__short_name);
    ATF_ADD_TEST_CASE(tcs, list_option__long_name);
    ATF_ADD_TEST_CASE(tcs, list_option__type);

    ATF_ADD_TEST_CASE(tcs, path_option__short_name);
    ATF_ADD_TEST_CASE(tcs, path_option__long_name);
    ATF_ADD_TEST_CASE(tcs, path_option__type);

    ATF_ADD_TEST_CASE(tcs, property_option__short_name);
    ATF_ADD_TEST_CASE(tcs, property_option__long_name);
    ATF_ADD_TEST_CASE(tcs, property_option__type);

    ATF_ADD_TEST_CASE(tcs, string_option__short_name);
    ATF_ADD_TEST_CASE(tcs, string_option__long_name);
    ATF_ADD_TEST_CASE(tcs, string_option__type);
}
