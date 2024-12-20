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

#include "utils/config/nodes.hpp"

#if !defined(UTILS_CONFIG_NODES_IPP)
#define UTILS_CONFIG_NODES_IPP

#include <memory>
#include <typeinfo>

#include "utils/config/exceptions.hpp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/optional.ipp"
#include "utils/text/exceptions.hpp"
#include "utils/text/operations.ipp"
#include "utils/sanity.hpp"

namespace utils {


namespace config {
namespace detail {


/// Type of the new_node() family of functions.
typedef base_node* (*new_node_hook)(void);


/// Creates a new leaf node of a given type.
///
/// \tparam NodeType The type of the leaf node to create.
///
/// \return A pointer to the newly-created node.
template< class NodeType >
base_node*
new_node(void)
{
    return new NodeType();
}


/// Internal node of the tree.
///
/// This abstract base class provides the mechanism to implement both static and
/// dynamic nodes.  Ideally, the implementation would be split in subclasses and
/// this class would not include the knowledge of whether the node is dynamic or
/// not.  However, because the static/dynamic difference depends on the leaf
/// types, we need to declare template functions and these cannot be virtual.
class inner_node : public base_node {
    /// Whether the node is dynamic or not.
    bool _dynamic;

protected:
    /// Type to represent the collection of children of this node.
    ///
    /// Note that these are one-level keys.  They cannot contain dots, and thus
    /// is why we use a string rather than a tree_key.
    typedef std::map< std::string, base_node* > children_map;

    /// Mapping of keys to values that are descendants of this node.
    children_map _children;

    void copy_into(inner_node*) const;
    void combine_into(const tree_key&, const base_node*, inner_node*) const;

private:
    void combine_children_into(const tree_key&,
                               const children_map&, const children_map&,
                               inner_node*) const;

public:
    inner_node(const bool);
    virtual ~inner_node(void) = 0;

    const base_node* lookup_ro(const tree_key&,
                               const tree_key::size_type) const;
    leaf_node* lookup_rw(const tree_key&, const tree_key::size_type,
                         new_node_hook);

    void all_properties(properties_map&, const tree_key&) const;
};


/// Static internal node of the tree.
///
/// The direct children of this node must be pre-defined by calls to define().
/// Attempts to traverse this node and resolve a key that is not a pre-defined
/// children will result in an "unknown key" error.
class static_inner_node : public config::detail::inner_node {
public:
    static_inner_node(void);

    virtual base_node* deep_copy(void) const;
    virtual base_node* combine(const tree_key&, const base_node*) const;

    void define(const tree_key&, const tree_key::size_type, new_node_hook);
};


/// Dynamic internal node of the tree.
///
/// The children of this node need not be pre-defined.  Attempts to traverse
/// this node and resolve a key will result in such key being created.  Any
/// intermediate non-existent nodes of the traversal will be created as dynamic
/// inner nodes as well.
class dynamic_inner_node : public config::detail::inner_node {
public:
    virtual base_node* deep_copy(void) const;
    virtual base_node* combine(const tree_key&, const base_node*) const;

    dynamic_inner_node(void);
};


}  // namespace detail
}  // namespace config


/// Constructor for a node with an undefined value.
///
/// This should only be called by the tree's define() method as a way to
/// register a node as known but undefined.  The node will then serve as a
/// placeholder for future values.
template< typename ValueType >
config::typed_leaf_node< ValueType >::typed_leaf_node(void) :
    _value(none)
{
}


/// Checks whether the node has been set by the user.
///
/// Nodes of the tree are predefined by the caller to specify the valid
/// types of the leaves.  Such predefinition results in the creation of
/// nodes within the tree, but these nodes have not yet been set.
/// Traversing these nodes is invalid and should result in an "unknown key"
/// error.
///
/// \return True if a value has been set in the node.
template< typename ValueType >
bool
config::typed_leaf_node< ValueType >::is_set(void) const
{
    return static_cast< bool >(_value);
}


/// Gets the value stored in the node.
///
/// \pre The node must have a value.
///
/// \return The value in the node.
template< typename ValueType >
const typename config::typed_leaf_node< ValueType >::value_type&
config::typed_leaf_node< ValueType >::value(void) const
{
    PRE(is_set());
    return _value.get();
}


/// Gets the read-write value stored in the node.
///
/// \pre The node must have a value.
///
/// \return The value in the node.
template< typename ValueType >
typename config::typed_leaf_node< ValueType >::value_type&
config::typed_leaf_node< ValueType >::value(void)
{
    PRE(is_set());
    return _value.get();
}


/// Sets the value of the node.
///
/// \param value_ The new value to set the node to.
///
/// \throw value_error If the value is invalid, according to validate().
template< typename ValueType >
void
config::typed_leaf_node< ValueType >::set(const value_type& value_)
{
    validate(value_);
    _value = optional< value_type >(value_);
}


/// Checks a given value for validity.
///
/// This is called internally by the node right before updating the recorded
/// value.  This method can be redefined by subclasses.
///
/// \throw value_error If the value is not valid.
template< typename ValueType >
void
config::typed_leaf_node< ValueType >::validate(
    const value_type& /* new_value */) const
{
}


/// Sets the value of the node from a raw string representation.
///
/// \param raw_value The value to set the node to.
///
/// \throw value_error If the value is invalid.
template< typename ValueType >
void
config::native_leaf_node< ValueType >::set_string(const std::string& raw_value)
{
    try {
        typed_leaf_node< ValueType >::set(text::to_type< ValueType >(
            raw_value));
    } catch (const text::value_error& e) {
        throw config::value_error(F("Failed to convert string value '%s' to "
                                    "the node's type") % raw_value);
    }
}


/// Converts the contents of the node to a string.
///
/// \pre The node must have a value.
///
/// \return A string representation of the value held by the node.
template< typename ValueType >
std::string
config::native_leaf_node< ValueType >::to_string(void) const
{
    PRE(typed_leaf_node< ValueType >::is_set());
    return F("%s") % typed_leaf_node< ValueType >::value();
}


/// Constructor for a node with an undefined value.
///
/// This should only be called by the tree's define() method as a way to
/// register a node as known but undefined.  The node will then serve as a
/// placeholder for future values.
template< typename ValueType >
config::base_set_node< ValueType >::base_set_node(void) :
    _value(none)
{
}


/// Checks whether the node has been set.
///
/// Remember that a node can exist before holding a value (i.e. when the node
/// has been defined as "known" but not yet set by the user).  This function
/// checks whether the node laready holds a value.
///
/// \return True if a value has been set in the node.
template< typename ValueType >
bool
config::base_set_node< ValueType >::is_set(void) const
{
    return static_cast< bool >(_value);
}


/// Gets the value stored in the node.
///
/// \pre The node must have a value.
///
/// \return The value in the node.
template< typename ValueType >
const typename config::base_set_node< ValueType >::value_type&
config::base_set_node< ValueType >::value(void) const
{
    PRE(is_set());
    return _value.get();
}


/// Gets the read-write value stored in the node.
///
/// \pre The node must have a value.
///
/// \return The value in the node.
template< typename ValueType >
typename config::base_set_node< ValueType >::value_type&
config::base_set_node< ValueType >::value(void)
{
    PRE(is_set());
    return _value.get();
}


/// Sets the value of the node.
///
/// \param value_ The new value to set the node to.
///
/// \throw value_error If the value is invalid, according to validate().
template< typename ValueType >
void
config::base_set_node< ValueType >::set(const value_type& value_)
{
    validate(value_);
    _value = optional< value_type >(value_);
}


/// Sets the value of the node from a raw string representation.
///
/// \param raw_value The value to set the node to.
///
/// \throw value_error If the value is invalid.
template< typename ValueType >
void
config::base_set_node< ValueType >::set_string(const std::string& raw_value)
{
    std::set< ValueType > new_value;

    const std::vector< std::string > words = text::split(raw_value, ' ');
    for (std::vector< std::string >::const_iterator iter = words.begin();
         iter != words.end(); ++iter) {
        if (!(*iter).empty())
            new_value.insert(parse_one(*iter));
    }

    set(new_value);
}


/// Converts the contents of the node to a string.
///
/// \pre The node must have a value.
///
/// \return A string representation of the value held by the node.
template< typename ValueType >
std::string
config::base_set_node< ValueType >::to_string(void) const
{
    PRE(is_set());
    return text::join(_value.get(), " ");
}


/// Pushes the node's value onto the Lua stack.
template< typename ValueType >
void
config::base_set_node< ValueType >::push_lua(lutok::state& /* state */) const
{
    UNREACHABLE;
}


/// Sets the value of the node from an entry in the Lua stack.
///
/// \throw value_error If the value in state(value_index) cannot be
///     processed by this node.
template< typename ValueType >
void
config::base_set_node< ValueType >::set_lua(
    lutok::state& state,
    const int value_index)
{
    if (state.is_string(value_index)) {
        set_string(state.to_string(value_index));
        return;
    }

    UNREACHABLE;
}


/// Checks a given value for validity.
///
/// This is called internally by the node right before updating the recorded
/// value.  This method can be redefined by subclasses.
///
/// \throw value_error If the value is not valid.
template< typename ValueType >
void
config::base_set_node< ValueType >::validate(
    const value_type& /* new_value */) const
{
}


}  // namespace utils

#endif  // !defined(UTILS_CONFIG_NODES_IPP)
