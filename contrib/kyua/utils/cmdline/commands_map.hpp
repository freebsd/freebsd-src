// Copyright 2011 The Kyua Authors.
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

/// \file utils/cmdline/commands_map.hpp
/// Maintains a collection of dynamically-instantiated commands.
///
/// Commands need to be dynamically-instantiated because they are often
/// complex data structures.  Instantiating them as static variables causes
/// problems with the order of construction of globals.  The commands_map class
/// provided by this module provides a mechanism to maintain these instantiated
/// objects.

#if !defined(UTILS_CMDLINE_COMMANDS_MAP_HPP)
#define UTILS_CMDLINE_COMMANDS_MAP_HPP

#include "utils/cmdline/commands_map_fwd.hpp"

#include <map>
#include <memory>
#include <set>
#include <string>

#include "utils/noncopyable.hpp"


namespace utils {
namespace cmdline {


/// Collection of dynamically-instantiated commands.
template< typename BaseCommand >
class commands_map : noncopyable {
    /// Map of command names to their implementations.
    typedef std::map< std::string, BaseCommand* > impl_map;

    /// Map of category names to the command names they contain.
    typedef std::map< std::string, std::set< std::string > > categories_map;

    /// Collection of all available commands.
    impl_map _commands;

    /// Collection of defined categories and their commands.
    categories_map _categories;

public:
    commands_map(void);
    ~commands_map(void);

    /// Scoped, strictly-owned pointer to a command from this map.
    typedef typename std::unique_ptr< BaseCommand > command_ptr;
    void insert(command_ptr, const std::string& = "");
    void insert(BaseCommand*, const std::string& = "");

    /// Type for a constant iterator.
    typedef typename categories_map::const_iterator const_iterator;

    bool empty(void) const;

    const_iterator begin(void) const;
    const_iterator end(void) const;

    BaseCommand* find(const std::string&);
    const BaseCommand* find(const std::string&) const;
};


}  // namespace cmdline
}  // namespace utils


#endif  // !defined(UTILS_CMDLINE_BASE_COMMAND_HPP)
