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

#include "engine/config.hpp"

#if defined(HAVE_CONFIG_H)
#   include "config.h"
#endif

#include <stdexcept>

#include "engine/exceptions.hpp"
#include "engine/execenv/execenv.hpp"
#include "utils/config/exceptions.hpp"
#include "utils/config/parser.hpp"
#include "utils/config/tree.ipp"
#include "utils/passwd.hpp"
#include "utils/text/exceptions.hpp"
#include "utils/text/operations.ipp"

namespace config = utils::config;
namespace execenv = engine::execenv;
namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace text = utils::text;


namespace {


/// Defines the schema of a configuration tree.
///
/// \param [in,out] tree The tree to populate.  The tree should be empty on
///     entry to prevent collisions with the keys defined in here.
static void
init_tree(config::tree& tree)
{
    tree.define< config::string_node >("architecture");
    tree.define< config::strings_set_node >("execenvs");
    tree.define< config::positive_int_node >("parallelism");
    tree.define< config::string_node >("platform");
    tree.define< engine::user_node >("unprivileged_user");
    tree.define_dynamic("test_suites");
}


/// Fills in a configuration tree with default values.
///
/// \param [in,out] tree The tree to populate.  init_tree() must have been
///     called on it beforehand.
static void
set_defaults(config::tree& tree)
{
    tree.set< config::string_node >("architecture", KYUA_ARCHITECTURE);

    std::set< std::string > supported;
    for (auto em : execenv::execenvs())
        if (em->is_supported())
            supported.insert(em->name());
    supported.insert(execenv::default_execenv_name);
    tree.set< config::strings_set_node >("execenvs", supported);

    // TODO(jmmv): Automatically derive this from the number of CPUs in the
    // machine and forcibly set to a value greater than 1.  Still testing
    // the new parallel implementation as of 2015-02-27 though.
    tree.set< config::positive_int_node >("parallelism", 1);
    tree.set< config::string_node >("platform", KYUA_PLATFORM);
}


/// Configuration parser specialization for Kyua configuration files.
class config_parser : public config::parser {
    /// Initializes the configuration tree.
    ///
    /// This is a callback executed when the configuration script invokes the
    /// syntax() method.  We populate the configuration tree from here with the
    /// schema version requested by the file.
    ///
    /// \param [in,out] tree The tree to populate.
    /// \param syntax_version The version of the file format as specified in the
    ///     configuration file.
    ///
    /// \throw config::syntax_error If the syntax_format/syntax_version
    /// combination is not supported.
    void
    setup(config::tree& tree, const int syntax_version)
    {
        if (syntax_version < 1 || syntax_version > 2)
            throw config::syntax_error(F("Unsupported config version %s") %
                                       syntax_version);

        init_tree(tree);
        set_defaults(tree);
    }

public:
    /// Initializes the parser.
    ///
    /// \param [out] tree_ The tree in which the results of the parsing will be
    ///     stored when parse() is called.  Should be empty on entry.  Because
    ///     we grab a reference to this object, the tree must remain valid for
    ///     the existence of the parser object.
    explicit config_parser(config::tree& tree_) :
        config::parser(tree_)
    {
    }
};


}  // anonymous namespace


/// Copies the node.
///
/// \return A dynamically-allocated node.
config::detail::base_node*
engine::user_node::deep_copy(void) const
{
    std::unique_ptr< user_node > new_node(new user_node());
    new_node->_value = _value;
    return new_node.release();
}


/// Pushes the node's value onto the Lua stack.
///
/// \param state The Lua state onto which to push the value.
void
engine::user_node::push_lua(lutok::state& state) const
{
    state.push_string(value().name);
}


/// Sets the value of the node from an entry in the Lua stack.
///
/// \param state The Lua state from which to get the value.
/// \param value_index The stack index in which the value resides.
///
/// \throw value_error If the value in state(value_index) cannot be
///     processed by this node.
void
engine::user_node::set_lua(lutok::state& state, const int value_index)
{
    if (state.is_number(value_index)) {
        config::typed_leaf_node< passwd::user >::set(
            passwd::find_user_by_uid(state.to_integer(-1)));
    } else if (state.is_string(value_index)) {
        config::typed_leaf_node< passwd::user >::set(
            passwd::find_user_by_name(state.to_string(-1)));
    } else
        throw config::value_error("Invalid user identifier");
}


/// Sets the value of the node from a raw string representation.
///
/// \param raw_value The value to set the node to.
///
/// \throw value_error If the value is invalid.
void
engine::user_node::set_string(const std::string& raw_value)
{
    try {
        config::typed_leaf_node< passwd::user >::set(
            passwd::find_user_by_name(raw_value));
    } catch (const std::runtime_error& e) {
        int uid;
        try {
            uid = text::to_type< int >(raw_value);
        } catch (const text::value_error& e2) {
            throw error(F("Cannot find user with name '%s'") % raw_value);
        }

        try {
            config::typed_leaf_node< passwd::user >::set(
                passwd::find_user_by_uid(uid));
        } catch (const std::runtime_error& e2) {
            throw error(F("Cannot find user with UID %s") % uid);
        }
    }
}


/// Converts the contents of the node to a string.
///
/// \pre The node must have a value.
///
/// \return A string representation of the value held by the node.
std::string
engine::user_node::to_string(void) const
{
    return config::typed_leaf_node< passwd::user >::value().name;
}


/// Constructs a config with the built-in settings.
///
/// \return A default test suite configuration.
config::tree
engine::default_config(void)
{
    config::tree tree(false);
    init_tree(tree);
    set_defaults(tree);
    return tree;
}


/// Constructs a config with the built-in settings.
///
/// \return An empty test suite configuration.
config::tree
engine::empty_config(void)
{
    config::tree tree(false);
    init_tree(tree);

    // Tests of Kyua itself tend to use an empty config, i.e. default
    // execution environment is used. Let's allow it.
    std::set< std::string > supported;
    supported.insert(engine::execenv::default_execenv_name);
    tree.set< config::strings_set_node >("execenvs", supported);

    return tree;
}


/// Parses a test suite configuration file.
///
/// \param file The file to parse.
///
/// \return High-level representation of the configuration file.
///
/// \throw load_error If there is any problem loading the file.  This includes
///     file access errors and syntax errors.
config::tree
engine::load_config(const utils::fs::path& file)
{
    config::tree tree(false);
    try {
        config_parser(tree).parse(file);
    } catch (const config::error& e) {
        throw load_error(file, e.what());
    }
    return tree;
}
