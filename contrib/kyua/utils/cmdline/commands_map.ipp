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

#include "utils/cmdline/commands_map.hpp"
#include "utils/sanity.hpp"


namespace utils {


/// Constructs an empty set of commands.
template< typename BaseCommand >
cmdline::commands_map< BaseCommand >::commands_map(void)
{
}


/// Destroys a set of commands.
///
/// This releases the dynamically-instantiated objects.
template< typename BaseCommand >
cmdline::commands_map< BaseCommand >::~commands_map(void)
{
    for (typename impl_map::iterator iter = _commands.begin();
         iter != _commands.end(); iter++)
        delete (*iter).second;
}


/// Inserts a new command into the map.
///
/// \param command The command to insert.  This must have been dynamically
///     allocated with new.  The call grabs ownership of the command, or the
///     command is freed if the call fails.
/// \param category The category this command belongs to.  Defaults to the empty
///     string, which indicates that the command has not be categorized.
template< typename BaseCommand >
void
cmdline::commands_map< BaseCommand >::insert(command_ptr command,
                                             const std::string& category)
{
    INV(_commands.find(command->name()) == _commands.end());
    BaseCommand* ptr = command.release();
    INV(ptr != NULL);
    _commands[ptr->name()] = ptr;
    _categories[category].insert(ptr->name());
}


/// Inserts a new command into the map.
///
/// This grabs ownership of the pointer, so it is ONLY safe to use with the
/// following idiom: insert(new foo()).
///
/// \param command The command to insert.  This must have been dynamically
///     allocated with new.  The call grabs ownership of the command, or the
///     command is freed if the call fails.
/// \param category The category this command belongs to.  Defaults to the empty
///     string, which indicates that the command has not be categorized.
template< typename BaseCommand >
void
cmdline::commands_map< BaseCommand >::insert(BaseCommand* command,
                                             const std::string& category)
{
    insert(command_ptr(command), category);
}


/// Checks whether the list of commands is empty.
///
/// \return True if there are no commands in this map.
template< typename BaseCommand >
bool
cmdline::commands_map< BaseCommand >::empty(void) const
{
    return _commands.empty();
}


/// Returns a constant iterator to the beginning of the categories mapping.
///
/// \return A map (string -> BaseCommand*) iterator.
template< typename BaseCommand >
typename cmdline::commands_map< BaseCommand >::const_iterator
cmdline::commands_map< BaseCommand >::begin(void) const
{
    return _categories.begin();
}


/// Returns a constant iterator to the end of the categories mapping.
///
/// \return A map (string -> BaseCommand*) iterator.
template< typename BaseCommand >
typename cmdline::commands_map< BaseCommand >::const_iterator
cmdline::commands_map< BaseCommand >::end(void) const
{
    return _categories.end();
}


/// Finds a command by name; mutable version.
///
/// \param name The name of the command to locate.
///
/// \return The command itself or NULL if it does not exist.
template< typename BaseCommand >
BaseCommand*
cmdline::commands_map< BaseCommand >::find(const std::string& name)
{
    typename impl_map::iterator iter = _commands.find(name);
    if (iter == _commands.end())
        return NULL;
    else
        return (*iter).second;
}


/// Finds a command by name; constant version.
///
/// \param name The name of the command to locate.
///
/// \return The command itself or NULL if it does not exist.
template< typename BaseCommand >
const BaseCommand*
cmdline::commands_map< BaseCommand >::find(const std::string& name) const
{
    typename impl_map::const_iterator iter = _commands.find(name);
    if (iter == _commands.end())
        return NULL;
    else
        return (*iter).second;
}


}  // namespace utils
