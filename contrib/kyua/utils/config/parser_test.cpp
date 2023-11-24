// Copyright 2012 The Kyua Authors.
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

#include "utils/config/parser.hpp"

#include <stdexcept>

#include <atf-c++.hpp>

#include "utils/config/exceptions.hpp"
#include "utils/config/parser.hpp"
#include "utils/config/tree.ipp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"

namespace config = utils::config;
namespace fs = utils::fs;


namespace {


/// Implementation of a parser for testing purposes.
class mock_parser : public config::parser {
    /// Initializes the tree keys before reading the file.
    ///
    /// \param [in,out] tree The tree in which to define the key structure.
    /// \param syntax_version The version of the file format as specified in the
    ///     configuration file.
    void
    setup(config::tree& tree, const int syntax_version)
    {
        if (syntax_version == 1) {
            // Do nothing on config_tree.
        } else if (syntax_version == 2) {
            tree.define< config::string_node >("top_string");
            tree.define< config::int_node >("inner.int");
            tree.define_dynamic("inner.dynamic");
        } else {
            throw std::runtime_error(F("Unknown syntax version %s") %
                                     syntax_version);
        }
    }

public:
    /// Initializes a parser.
    ///
    /// \param tree The mock config tree to parse.
    mock_parser(config::tree& tree) :
        config::parser(tree)
    {
    }
};


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(no_keys__ok);
ATF_TEST_CASE_BODY(no_keys__ok)
{
    atf::utils::create_file(
        "output.lua",
        "syntax(2)\n"
        "local foo = 'value'\n");

    config::tree tree;
    mock_parser(tree).parse(fs::path("output.lua"));
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::string_node >("foo"));
}


ATF_TEST_CASE_WITHOUT_HEAD(no_keys__unknown_key);
ATF_TEST_CASE_BODY(no_keys__unknown_key)
{
    atf::utils::create_file(
        "output.lua",
        "syntax(2)\n"
        "foo = 'value'\n");

    config::tree tree;
    ATF_REQUIRE_THROW_RE(config::syntax_error, "foo",
                         mock_parser(tree).parse(fs::path("output.lua")));
}


ATF_TEST_CASE_WITHOUT_HEAD(some_keys__ok);
ATF_TEST_CASE_BODY(some_keys__ok)
{
    atf::utils::create_file(
        "output.lua",
        "syntax(2)\n"
        "top_string = 'foo'\n"
        "inner.int = 12345\n"
        "inner.dynamic.foo = 78\n"
        "inner.dynamic.bar = 'some text'\n");

    config::tree tree;
    mock_parser(tree).parse(fs::path("output.lua"));
    ATF_REQUIRE_EQ("foo", tree.lookup< config::string_node >("top_string"));
    ATF_REQUIRE_EQ(12345, tree.lookup< config::int_node >("inner.int"));
    ATF_REQUIRE_EQ("78",
                   tree.lookup< config::string_node >("inner.dynamic.foo"));
    ATF_REQUIRE_EQ("some text",
                   tree.lookup< config::string_node >("inner.dynamic.bar"));
}


ATF_TEST_CASE_WITHOUT_HEAD(some_keys__not_strict);
ATF_TEST_CASE_BODY(some_keys__not_strict)
{
    atf::utils::create_file(
        "output.lua",
        "syntax(2)\n"
        "top_string = 'foo'\n"
        "unknown_string = 'bar'\n"
        "top_string = 'baz'\n");

    config::tree tree(false);
    mock_parser(tree).parse(fs::path("output.lua"));
    ATF_REQUIRE_EQ("baz", tree.lookup< config::string_node >("top_string"));
    ATF_REQUIRE(!tree.is_set("unknown_string"));
}


ATF_TEST_CASE_WITHOUT_HEAD(some_keys__unknown_key);
ATF_TEST_CASE_BODY(some_keys__unknown_key)
{
    atf::utils::create_file(
        "output.lua",
        "syntax(2)\n"
        "top_string2 = 'foo'\n");
    config::tree tree1;
    ATF_REQUIRE_THROW_RE(config::syntax_error,
                         "Unknown configuration property 'top_string2'",
                         mock_parser(tree1).parse(fs::path("output.lua")));

    atf::utils::create_file(
        "output.lua",
        "syntax(2)\n"
        "inner.int2 = 12345\n");
    config::tree tree2;
    ATF_REQUIRE_THROW_RE(config::syntax_error,
                         "Unknown configuration property 'inner.int2'",
                         mock_parser(tree2).parse(fs::path("output.lua")));
}


ATF_TEST_CASE_WITHOUT_HEAD(invalid_syntax);
ATF_TEST_CASE_BODY(invalid_syntax)
{
    config::tree tree;

    atf::utils::create_file("output.lua", "syntax(56)\n");
    ATF_REQUIRE_THROW_RE(config::syntax_error,
                         "Unknown syntax version 56",
                         mock_parser(tree).parse(fs::path("output.lua")));
}


ATF_TEST_CASE_WITHOUT_HEAD(syntax_deprecated_format);
ATF_TEST_CASE_BODY(syntax_deprecated_format)
{
    config::tree tree;

    atf::utils::create_file("output.lua", "syntax('config', 1)\n");
    (void)mock_parser(tree).parse(fs::path("output.lua"));

    atf::utils::create_file("output.lua", "syntax('foo', 1)\n");
    ATF_REQUIRE_THROW_RE(config::syntax_error, "must be 'config'",
                         mock_parser(tree).parse(fs::path("output.lua")));

    atf::utils::create_file("output.lua", "syntax('config', 2)\n");
    ATF_REQUIRE_THROW_RE(config::syntax_error, "only takes one argument",
                         mock_parser(tree).parse(fs::path("output.lua")));
}


ATF_TEST_CASE_WITHOUT_HEAD(syntax_not_called);
ATF_TEST_CASE_BODY(syntax_not_called)
{
    config::tree tree;
    tree.define< config::int_node >("var");

    atf::utils::create_file("output.lua", "var = 3\n");
    ATF_REQUIRE_THROW_RE(config::syntax_error, "No syntax defined",
                         mock_parser(tree).parse(fs::path("output.lua")));

    ATF_REQUIRE(!tree.is_set("var"));
}


ATF_TEST_CASE_WITHOUT_HEAD(syntax_called_more_than_once);
ATF_TEST_CASE_BODY(syntax_called_more_than_once)
{
    config::tree tree;
    tree.define< config::int_node >("var");

    atf::utils::create_file(
        "output.lua",
        "syntax(2)\n"
        "var = 3\n"
        "syntax(2)\n"
        "var = 5\n");
    ATF_REQUIRE_THROW_RE(config::syntax_error,
                         "syntax\\(\\) can only be called once",
                         mock_parser(tree).parse(fs::path("output.lua")));

    ATF_REQUIRE_EQ(3, tree.lookup< config::int_node >("var"));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, no_keys__ok);
    ATF_ADD_TEST_CASE(tcs, no_keys__unknown_key);

    ATF_ADD_TEST_CASE(tcs, some_keys__ok);
    ATF_ADD_TEST_CASE(tcs, some_keys__not_strict);
    ATF_ADD_TEST_CASE(tcs, some_keys__unknown_key);

    ATF_ADD_TEST_CASE(tcs, invalid_syntax);
    ATF_ADD_TEST_CASE(tcs, syntax_deprecated_format);
    ATF_ADD_TEST_CASE(tcs, syntax_not_called);
    ATF_ADD_TEST_CASE(tcs, syntax_called_more_than_once);
}
