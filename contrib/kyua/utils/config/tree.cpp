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

#include "utils/config/tree.ipp"

#include "utils/config/exceptions.hpp"
#include "utils/config/keys.hpp"
#include "utils/config/nodes.ipp"
#include "utils/format/macros.hpp"

namespace config = utils::config;


/// Constructor.
///
/// \param strict Whether keys must be validated at "set" time.
config::tree::tree(const bool strict) :
    _strict(strict), _root(new detail::static_inner_node())
{
}


/// Constructor with a non-empty root.
///
/// \param strict Whether keys must be validated at "set" time.
/// \param root The root to the tree to be owned by this instance.
config::tree::tree(const bool strict, detail::static_inner_node* root) :
    _strict(strict), _root(root)
{
}


/// Destructor.
config::tree::~tree(void)
{
}


/// Generates a deep copy of the input tree.
///
/// \return A new tree that is an exact copy of this tree.
config::tree
config::tree::deep_copy(void) const
{
    detail::static_inner_node* new_root =
        dynamic_cast< detail::static_inner_node* >(_root->deep_copy());
    return config::tree(_strict, new_root);
}


/// Combines two trees.
///
/// By combination we understand a new tree that contains the full key space of
/// the two input trees and, for the keys that match, respects the value of the
/// right-hand side (aka "other") tree.
///
/// Any nodes marked as dynamic "win" over non-dynamic nodes and the resulting
/// tree will have the dynamic property set on those.
///
/// \param overrides The tree to use as value overrides.
///
/// \return The combined tree.
///
/// \throw bad_combination_error If the two trees cannot be combined; for
///     example, if a single key represents an inner node in one tree but a leaf
///     node in the other one.
config::tree
config::tree::combine(const tree& overrides) const
{
    const detail::static_inner_node* other_root =
        dynamic_cast< const detail::static_inner_node * >(
            overrides._root.get());

    detail::static_inner_node* new_root =
        dynamic_cast< detail::static_inner_node* >(
            _root->combine(detail::tree_key(), other_root));
    return config::tree(_strict, new_root);
}


/// Registers a node as being dynamic.
///
/// This operation creates the given key as an inner node.  Further set
/// operations that trespass this node will automatically create any missing
/// keys.
///
/// This method does not raise errors on invalid/unknown keys or other
/// tree-related issues.  The reasons is that define() is a method that does not
/// depend on user input: it is intended to pre-populate the tree with a
/// specific structure, and that happens once at coding time.
///
/// \param dotted_key The key to be registered in dotted representation.
void
config::tree::define_dynamic(const std::string& dotted_key)
{
    try {
        const detail::tree_key key = detail::parse_key(dotted_key);
        _root->define(key, 0, detail::new_node< detail::dynamic_inner_node >);
    } catch (const error& e) {
        UNREACHABLE_MSG("define() failing due to key errors is a programming "
                        "mistake: " + std::string(e.what()));
    }
}


/// Checks if a given node is set.
///
/// \param dotted_key The key to be checked.
///
/// \return True if the key is set to a specific value (not just defined).
/// False if the key is not set or if the key does not exist.
///
/// \throw invalid_key_error If the provided key has an invalid format.
bool
config::tree::is_set(const std::string& dotted_key) const
{
    const detail::tree_key key = detail::parse_key(dotted_key);
    try {
        const detail::base_node* raw_node = _root->lookup_ro(key, 0);
        try {
            const leaf_node& child = dynamic_cast< const leaf_node& >(
                *raw_node);
            return child.is_set();
        } catch (const std::bad_cast& unused_error) {
            return false;
        }
    } catch (const unknown_key_error& unused_error) {
        return false;
    }
}


/// Pushes a leaf node's value onto the Lua stack.
///
/// \param dotted_key The key to be pushed.
/// \param state The Lua state into which to push the key's value.
///
/// \throw invalid_key_error If the provided key has an invalid format.
/// \throw unknown_key_error If the provided key is unknown.
void
config::tree::push_lua(const std::string& dotted_key, lutok::state& state) const
{
    const detail::tree_key key = detail::parse_key(dotted_key);
    const detail::base_node* raw_node = _root->lookup_ro(key, 0);
    try {
        const leaf_node& child = dynamic_cast< const leaf_node& >(*raw_node);
        child.push_lua(state);
    } catch (const std::bad_cast& unused_error) {
        throw unknown_key_error(key);
    }
}


/// Sets a leaf node's value from a value in the Lua stack.
///
/// \param dotted_key The key to be set.
/// \param state The Lua state from which to retrieve the value.
/// \param value_index The position in the Lua stack holding the value.
///
/// \throw invalid_key_error If the provided key has an invalid format.
/// \throw invalid_key_value If the value mismatches the node type.
/// \throw unknown_key_error If the provided key is unknown.
void
config::tree::set_lua(const std::string& dotted_key, lutok::state& state,
                      const int value_index)
{
    const detail::tree_key key = detail::parse_key(dotted_key);
    try {
        detail::base_node* raw_node = _root->lookup_rw(
            key, 0, detail::new_node< string_node >);
        leaf_node& child = dynamic_cast< leaf_node& >(*raw_node);
        child.set_lua(state, value_index);
    } catch (const unknown_key_error& e) {
        if (_strict)
            throw e;
    } catch (const value_error& e) {
        throw invalid_key_value(key, e.what());
    } catch (const std::bad_cast& unused_error) {
        throw invalid_key_value(key, "Type mismatch");
    }
}


/// Gets the value of a node as a plain string.
///
/// \param dotted_key The key to be looked up.
///
/// \return The value of the located node as a string.
///
/// \throw invalid_key_error If the provided key has an invalid format.
/// \throw unknown_key_error If the provided key is unknown.
std::string
config::tree::lookup_string(const std::string& dotted_key) const
{
    const detail::tree_key key = detail::parse_key(dotted_key);
    const detail::base_node* raw_node = _root->lookup_ro(key, 0);
    try {
        const leaf_node& child = dynamic_cast< const leaf_node& >(*raw_node);
        return child.to_string();
    } catch (const std::bad_cast& unused_error) {
        throw unknown_key_error(key);
    }
}


/// Sets the value of a leaf addressed by its key from a string value.
///
/// This respects the native types of all the nodes that have been predefined.
/// For new nodes under a dynamic subtree, this has no mechanism of determining
/// what type they need to have, so they are created as plain string nodes.
///
/// \param dotted_key The key to be registered in dotted representation.
/// \param raw_value The string representation of the value to set the node to.
///
/// \throw invalid_key_error If the provided key has an invalid format.
/// \throw invalid_key_value If the value mismatches the node type.
/// \throw unknown_key_error If the provided key is unknown.
void
config::tree::set_string(const std::string& dotted_key,
                         const std::string& raw_value)
{
    const detail::tree_key key = detail::parse_key(dotted_key);
    try {
        detail::base_node* raw_node = _root->lookup_rw(
            key, 0, detail::new_node< string_node >);
        leaf_node& child = dynamic_cast< leaf_node& >(*raw_node);
        child.set_string(raw_value);
    } catch (const unknown_key_error& e) {
        if (_strict)
            throw e;
    } catch (const value_error& e) {
        throw invalid_key_value(key, e.what());
    } catch (const std::bad_cast& unused_error) {
        throw invalid_key_value(key, "Type mismatch");
    }
}


/// Converts the tree to a collection of key/value string pairs.
///
/// \param dotted_key Subtree from which to start the export.
/// \param strip_key If true, remove the dotted_key prefix from the resulting
///     properties.
///
/// \return A map of keys to values in their textual representation.
///
/// \throw invalid_key_error If the provided key has an invalid format.
/// \throw unknown_key_error If the provided key is unknown.
/// \throw value_error If the provided key points to a leaf.
config::properties_map
config::tree::all_properties(const std::string& dotted_key,
                             const bool strip_key) const
{
    PRE(!strip_key || !dotted_key.empty());

    properties_map properties;

    detail::tree_key key;
    const detail::base_node* raw_node;
    if (dotted_key.empty()) {
        raw_node = _root.get();
    } else {
        key = detail::parse_key(dotted_key);
        raw_node = _root->lookup_ro(key, 0);
    }
    try {
        const detail::inner_node& child =
            dynamic_cast< const detail::inner_node& >(*raw_node);
        child.all_properties(properties, key);
    } catch (const std::bad_cast& unused_error) {
        INV(!dotted_key.empty());
        throw value_error(F("Cannot export properties from a leaf node; "
                            "'%s' given") % dotted_key);
    }

    if (strip_key) {
        properties_map stripped;
        for (properties_map::const_iterator iter = properties.begin();
             iter != properties.end(); ++iter) {
            stripped[(*iter).first.substr(dotted_key.length() + 1)] =
                (*iter).second;
        }
        properties = stripped;
    }

    return properties;
}


/// Equality comparator.
///
/// \param other The other object to compare this one to.
///
/// \return True if this object and other are equal; false otherwise.
bool
config::tree::operator==(const tree& other) const
{
    // TODO(jmmv): Would be nicer to perform the comparison directly on the
    // nodes, instead of exporting the values to strings first.
    return _root == other._root || all_properties() == other.all_properties();
}


/// Inequality comparator.
///
/// \param other The other object to compare this one to.
///
/// \return True if this object and other are different; false otherwise.
bool
config::tree::operator!=(const tree& other) const
{
    return !(*this == other);
}
