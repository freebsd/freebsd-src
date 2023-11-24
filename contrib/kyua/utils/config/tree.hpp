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

/// \file utils/config/tree.hpp
/// Data type to represent a tree of arbitrary values with string keys.

#if !defined(UTILS_CONFIG_TREE_HPP)
#define UTILS_CONFIG_TREE_HPP

#include "utils/config/tree_fwd.hpp"

#include <memory>
#include <string>

#include <lutok/state.hpp>

#include "utils/config/keys_fwd.hpp"
#include "utils/config/nodes_fwd.hpp"

namespace utils {
namespace config {


/// Representation of a tree.
///
/// The string keys of the tree are in dotted notation and actually represent
/// path traversals through the nodes.
///
/// Our trees are "strictly-keyed": keys must be defined as "existent" before
/// their values can be set.  Defining a key is a separate action from setting
/// its value.  The rationale is that we want to be able to control what keys
/// get defined: because trees are used to hold configuration, we want to catch
/// typos as early as possible.  Also, users cannot set keys unless the types
/// are known in advance because our leaf nodes are strictly typed.
///
/// However, there is an exception to the strict keys: the inner nodes of the
/// tree can be static or dynamic.  Static inner nodes have a known subset of
/// children and attempting to set keys not previously defined will result in an
/// error.  Dynamic inner nodes do not have a predefined set of keys and can be
/// used to accept arbitrary user input.
///
/// For simplicity reasons, we force the root of the tree to be a static inner
/// node.  In other words, the root can never contain a value by itself and this
/// is not a problem because the root is not addressable by the key space.
/// Additionally, the root is strict so all of its direct children must be
/// explicitly defined.
///
/// This is, effectively, a simple wrapper around the node representing the
/// root.  Having a separate class aids in clearly representing the concept of a
/// tree and all of its public methods.  Also, the tree accepts dotted notations
/// for the keys while the internal structures do not.
///
/// Note that trees are shallow-copied unless a deep copy is requested with
/// deep_copy().
class tree {
    /// Whether keys must be validated at "set" time.
    bool _strict;

    /// The root of the tree.
    std::shared_ptr< detail::static_inner_node > _root;

    tree(const bool, detail::static_inner_node*);

public:
    tree(const bool = true);
    ~tree(void);

    tree deep_copy(void) const;
    tree combine(const tree&) const;

    template< class LeafType >
    void define(const std::string&);

    void define_dynamic(const std::string&);

    bool is_set(const std::string&) const;

    template< class LeafType >
    const typename LeafType::value_type& lookup(const std::string&) const;
    template< class LeafType >
    typename LeafType::value_type& lookup_rw(const std::string&);

    template< class LeafType >
    void set(const std::string&, const typename LeafType::value_type&);

    void push_lua(const std::string&, lutok::state&) const;
    void set_lua(const std::string&, lutok::state&, const int);

    std::string lookup_string(const std::string&) const;
    void set_string(const std::string&, const std::string&);

    properties_map all_properties(const std::string& = "",
                                  const bool = false) const;

    bool operator==(const tree&) const;
    bool operator!=(const tree&) const;
};


}  // namespace config
}  // namespace utils

#endif  // !defined(UTILS_CONFIG_TREE_HPP)
