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

#include "utils/config/tree.hpp"

#if !defined(UTILS_CONFIG_TREE_IPP)
#define UTILS_CONFIG_TREE_IPP

#include <typeinfo>

#include "utils/config/exceptions.hpp"
#include "utils/config/keys.hpp"
#include "utils/config/nodes.ipp"
#include "utils/format/macros.hpp"
#include "utils/sanity.hpp"

namespace utils {


/// Registers a key as valid and having a specific type.
///
/// This method does not raise errors on invalid/unknown keys or other
/// tree-related issues.  The reasons is that define() is a method that does not
/// depend on user input: it is intended to pre-populate the tree with a
/// specific structure, and that happens once at coding time.
///
/// \tparam LeafType The node type of the leaf we are defining.
/// \param dotted_key The key to be registered in dotted representation.
template< class LeafType >
void
config::tree::define(const std::string& dotted_key)
{
    try {
        const detail::tree_key key = detail::parse_key(dotted_key);
        _root->define(key, 0, detail::new_node< LeafType >);
    } catch (const error& e) {
        UNREACHABLE_MSG(F("define() failing due to key errors is a programming "
                          "mistake: %s") % e.what());
    }
}


/// Gets a read-only reference to the value of a leaf addressed by its key.
///
/// \tparam LeafType The node type of the leaf we are querying.
/// \param dotted_key The key to be registered in dotted representation.
///
/// \return A reference to the value in the located leaf, if successful.
///
/// \throw invalid_key_error If the provided key has an invalid format.
/// \throw unknown_key_error If the provided key is unknown.
template< class LeafType >
const typename LeafType::value_type&
config::tree::lookup(const std::string& dotted_key) const
{
    const detail::tree_key key = detail::parse_key(dotted_key);
    const detail::base_node* raw_node = _root->lookup_ro(key, 0);
    try {
        const LeafType& child = dynamic_cast< const LeafType& >(*raw_node);
        if (child.is_set())
            return child.value();
        else
            throw unknown_key_error(key);
    } catch (const std::bad_cast& unused_error) {
        throw unknown_key_error(key);
    }
}


/// Gets a read-write reference to the value of a leaf addressed by its key.
///
/// \tparam LeafType The node type of the leaf we are querying.
/// \param dotted_key The key to be registered in dotted representation.
///
/// \return A reference to the value in the located leaf, if successful.
///
/// \throw invalid_key_error If the provided key has an invalid format.
/// \throw unknown_key_error If the provided key is unknown.
template< class LeafType >
typename LeafType::value_type&
config::tree::lookup_rw(const std::string& dotted_key)
{
    const detail::tree_key key = detail::parse_key(dotted_key);
    detail::base_node* raw_node = _root->lookup_rw(
        key, 0, detail::new_node< LeafType >);
    try {
        LeafType& child = dynamic_cast< LeafType& >(*raw_node);
        if (child.is_set())
            return child.value();
        else
            throw unknown_key_error(key);
    } catch (const std::bad_cast& unused_error) {
        throw unknown_key_error(key);
    }
}


/// Sets the value of a leaf addressed by its key.
///
/// \tparam LeafType The node type of the leaf we are setting.
/// \param dotted_key The key to be registered in dotted representation.
/// \param value The value to set into the node.
///
/// \throw invalid_key_error If the provided key has an invalid format.
/// \throw invalid_key_value If the value mismatches the node type.
/// \throw unknown_key_error If the provided key is unknown.
template< class LeafType >
void
config::tree::set(const std::string& dotted_key,
                  const typename LeafType::value_type& value)
{
    const detail::tree_key key = detail::parse_key(dotted_key);
    try {
        leaf_node* raw_node = _root->lookup_rw(key, 0,
                                               detail::new_node< LeafType >);
        LeafType& child = dynamic_cast< LeafType& >(*raw_node);
        child.set(value);
    } catch (const unknown_key_error& e) {
        if (_strict)
            throw e;
    } catch (const value_error& e) {
        throw invalid_key_value(key, e.what());
    } catch (const std::bad_cast& unused_error) {
        throw invalid_key_value(key, "Type mismatch");
    }
}


}  // namespace utils

#endif  // !defined(UTILS_CONFIG_TREE_IPP)
