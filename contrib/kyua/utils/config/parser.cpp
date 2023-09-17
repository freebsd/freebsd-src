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

#include <lutok/exceptions.hpp>
#include <lutok/operations.hpp>
#include <lutok/stack_cleaner.hpp>
#include <lutok/state.ipp>

#include "utils/config/exceptions.hpp"
#include "utils/config/lua_module.hpp"
#include "utils/config/tree.ipp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/noncopyable.hpp"

namespace config = utils::config;


// History of configuration file versions:
//
// 2 - Changed the syntax() call to take only a version number, instead of the
//     word 'config' as the first argument and the version as the second one.
//     Files now start with syntax(2) instead of syntax('config', 1).
//
// 1 - Initial version.


/// Internal implementation of the parser.
struct utils::config::parser::impl : utils::noncopyable {
    /// Pointer to the parent parser.  Needed for callbacks.
    parser* _parent;

    /// The Lua state used by this parser to process the configuration file.
    lutok::state _state;

    /// The tree to be filed in by the configuration parameters, as provided by
    /// the caller.
    config::tree& _tree;

    /// Whether syntax() has been called or not.
    bool _syntax_called;

    /// Constructs a new implementation.
    ///
    /// \param parent_ Pointer to the class being constructed.
    /// \param config_tree_ The configuration tree provided by the user.
    impl(parser* const parent_, tree& config_tree_) :
        _parent(parent_), _tree(config_tree_), _syntax_called(false)
    {
    }

    friend void lua_syntax(lutok::state&);

    /// Callback executed by the Lua syntax() function.
    ///
    /// \param syntax_version The syntax format version as provided by the
    ///     configuration file in the call to syntax().
    void
    syntax_callback(const int syntax_version)
    {
        if (_syntax_called)
            throw syntax_error("syntax() can only be called once");
        _syntax_called = true;

        // Allow the parser caller to populate the tree with its own schema
        // depending on the format/version combination.
        _parent->setup(_tree, syntax_version);

        // Export the config module to the Lua state so that all global variable
        // accesses are redirected to the configuration tree.
        config::redirect(_state, _tree);
    }
};


namespace {


static int
lua_syntax(lutok::state& state)
{
    if (!state.is_number(-1))
        throw config::value_error("Last argument to syntax must be a number");
    const int syntax_version = state.to_integer(-1);

    if (syntax_version == 1) {
        if (state.get_top() != 2)
            throw config::value_error("Version 1 files need two arguments to "
                                      "syntax()");
        if (!state.is_string(-2) || state.to_string(-2) != "config")
            throw config::value_error("First argument to syntax must be "
                                      "'config' for version 1 files");
    } else {
        if (state.get_top() != 1)
            throw config::value_error("syntax() only takes one argument");
    }

    state.get_global("_config_parser");
    config::parser::impl* impl =
        *state.to_userdata< config::parser::impl* >(-1);
    state.pop(1);

    impl->syntax_callback(syntax_version);

    return 0;
}


}  // anonymous namespace


/// Constructs a new parser.
///
/// \param [in,out] config_tree The configuration tree into which the values set
///     in the configuration file will be stored.
config::parser::parser(tree& config_tree) :
    _pimpl(new impl(this, config_tree))
{
    lutok::stack_cleaner cleaner(_pimpl->_state);

    _pimpl->_state.push_cxx_function(lua_syntax);
    _pimpl->_state.set_global("syntax");
    *_pimpl->_state.new_userdata< config::parser::impl* >() = _pimpl.get();
    _pimpl->_state.set_global("_config_parser");
}


/// Destructor.
config::parser::~parser(void)
{
}


/// Parses a configuration file.
///
/// \post The tree registered during the construction of this class is updated
/// to contain the values read from the configuration file.  If the processing
/// fails, the state of the output tree is undefined.
///
/// \param file The path to the file to process.
///
/// \throw syntax_error If there is any problem processing the file.
void
config::parser::parse(const fs::path& file)
{
    try {
        lutok::do_file(_pimpl->_state, file.str(), 0, 0, 0);
    } catch (const lutok::error& e) {
        throw syntax_error(e.what());
    }

    if (!_pimpl->_syntax_called)
        throw syntax_error("No syntax defined (no call to syntax() found)");
}
