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

#include "utils/config/lua_module.hpp"

#include <lutok/stack_cleaner.hpp>
#include <lutok/state.ipp>

#include "utils/config/exceptions.hpp"
#include "utils/config/keys.hpp"
#include "utils/config/tree.ipp"

namespace config = utils::config;
namespace detail = utils::config::detail;


namespace {


/// Gets the tree singleton stored in the Lua state.
///
/// \param state The Lua state.  The registry must contain a key named
///    "tree" with a pointer to the singleton.
///
/// \return A reference to the tree associated with the Lua state.
///
/// \throw syntax_error If the tree cannot be located.
config::tree&
get_global_tree(lutok::state& state)
{
    lutok::stack_cleaner cleaner(state);

    state.push_value(lutok::registry_index);
    state.push_string("tree");
    state.get_table(-2);
    if (state.is_nil(-1))
        throw config::syntax_error("Cannot find tree singleton; global state "
                                   "corrupted?");
    config::tree& tree = **state.to_userdata< config::tree* >(-1);
    state.pop(1);
    return tree;
}


/// Gets a fully-qualified tree key from the state.
///
/// \param state The Lua state.
/// \param table_index An index to the Lua stack pointing to the table being
///     accessed.  If this table contains a tree_key metadata property, this is
///     considered to be the prefix of the tree key.
/// \param field_index An index to the Lua stack pointing to the entry
///     containing the name of the field being indexed.
///
/// \return A dotted key.
///
/// \throw invalid_key_error If the name of the key is invalid.
static std::string
get_tree_key(lutok::state& state, const int table_index, const int field_index)
{
    PRE(state.is_string(field_index));
    const std::string field = state.to_string(field_index);
    if (!field.empty() && field[0] == '_')
        throw config::invalid_key_error(
            F("Configuration key cannot have an underscore as a prefix; "
              "found %s") % field);

    std::string tree_key;
    if (state.get_metafield(table_index, "tree_key")) {
        tree_key = state.to_string(-1) + "." + state.to_string(field_index - 1);
        state.pop(1);
    } else
        tree_key = state.to_string(field_index);
    return tree_key;
}


static int redirect_newindex(lutok::state&);
static int redirect_index(lutok::state&);


/// Creates a table for a new configuration inner node.
///
/// \post state(-1) Contains the new table.
///
/// \param state The Lua state in which to push the table.
/// \param tree_key The key to which the new table corresponds.
static void
new_table_for_key(lutok::state& state, const std::string& tree_key)
{
    state.new_table();
    {
        state.new_table();
        {
            state.push_string("__index");
            state.push_cxx_function(redirect_index);
            state.set_table(-3);

            state.push_string("__newindex");
            state.push_cxx_function(redirect_newindex);
            state.set_table(-3);

            state.push_string("tree_key");
            state.push_string(tree_key);
            state.set_table(-3);
        }
        state.set_metatable(-2);
    }
}


/// Sets the value of an configuration node.
///
/// \pre state(-3) The table to index.  If this is not _G, then the table
///     metadata must contain a tree_key property describing the path to
///     current level.
/// \pre state(-2) The field to index into the table.  Must be a string.
/// \pre state(-1) The value to set the indexed table field to.
///
/// \param state The Lua state in which to operate.
///
/// \return The number of result values on the Lua stack; always 0.
///
/// \throw invalid_key_error If the provided key is invalid.
/// \throw unknown_key_error If the key cannot be located.
/// \throw value_error If the value has an unsupported type or cannot be
///     set on the key, or if the input table or index are invalid.
static int
redirect_newindex(lutok::state& state)
{
    if (!state.is_table(-3))
        throw config::value_error("Indexed object is not a table");
    if (!state.is_string(-2))
        throw config::value_error("Invalid field in configuration object "
                                  "reference; must be a string");

    const std::string dotted_key = get_tree_key(state, -3, -2);
    try {
        config::tree& tree = get_global_tree(state);
        tree.set_lua(dotted_key, state, -1);
    } catch (const config::value_error& e) {
        throw config::invalid_key_value(detail::parse_key(dotted_key),
                                        e.what());
    }

    // Now really set the key in the Lua table, but prevent direct accesses from
    // the user by prefixing it.  We do this to ensure that re-setting the same
    // key of the tree results in a call to __newindex instead of __index.
    state.push_string("_" + state.to_string(-2));
    state.push_value(-2);
    state.raw_set(-5);

    return 0;
}


/// Indexes a configuration node.
///
/// \pre state(-3) The table to index.  If this is not _G, then the table
///     metadata must contain a tree_key property describing the path to
///     current level.  If the field does not exist, a new table is created.
/// \pre state(-1) The field to index into the table.  Must be a string.
///
/// \param state The Lua state in which to operate.
///
/// \return The number of result values on the Lua stack; always 1.
///
/// \throw value_error If the input table or index are invalid.
static int
redirect_index(lutok::state& state)
{
    if (!state.is_table(-2))
        throw config::value_error("Indexed object is not a table");
    if (!state.is_string(-1))
        throw config::value_error("Invalid field in configuration object "
                                  "reference; must be a string");

    // Query if the key has already been set by a call to redirect_newindex.
    state.push_string("_" + state.to_string(-1));
    state.raw_get(-3);
    if (!state.is_nil(-1))
        return 1;
    state.pop(1);

    state.push_value(-1);  // Duplicate the field name.
    state.raw_get(-3);  // Get table[field] to see if it's defined.
    if (state.is_nil(-1)) {
        state.pop(1);

        // The stack is now the same as when we entered the function, but we
        // know that the field is undefined and thus have to create a new
        // configuration table.
        INV(state.is_table(-2));
        INV(state.is_string(-1));

        const config::tree& tree = get_global_tree(state);
        const std::string tree_key = get_tree_key(state, -2, -1);
        if (tree.is_set(tree_key)) {
            // Publish the pre-recorded value in the tree to the Lua state,
            // instead of considering this table key a new inner node.
            tree.push_lua(tree_key, state);
        } else {
            state.push_string("_" + state.to_string(-1));
            state.insert(-2);
            state.pop(1);

            new_table_for_key(state, tree_key);

            // Duplicate the newly created table and place it deep in the stack
            // so that the raw_set below leaves us with the return value of this
            // function at the top of the stack.
            state.push_value(-1);
            state.insert(-4);

            state.raw_set(-3);
            state.pop(1);
        }
    }
    return 1;
}


}  // anonymous namespace


/// Install wrappers for globals to set values in the configuration tree.
///
/// This function installs wrappers to capture all accesses to global variables.
/// Such wrappers redirect the reads and writes to the out_tree, which is the
/// entity that defines what configuration variables exist.
///
/// \param state The Lua state into which to install the wrappers.
/// \param out_tree The tree with the layout definition and where the
///     configuration settings will be collected.
void
config::redirect(lutok::state& state, tree& out_tree)
{
    lutok::stack_cleaner cleaner(state);

    state.get_global_table();
    {
        state.push_string("__index");
        state.push_cxx_function(redirect_index);
        state.set_table(-3);

        state.push_string("__newindex");
        state.push_cxx_function(redirect_newindex);
        state.set_table(-3);
    }
    state.set_metatable(-1);

    state.push_value(lutok::registry_index);
    state.push_string("tree");
    config::tree** tree = state.new_userdata< config::tree* >();
    *tree = &out_tree;
    state.set_table(-3);
    state.pop(1);
}
