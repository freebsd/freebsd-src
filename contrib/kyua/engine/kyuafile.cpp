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

#include "engine/kyuafile.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>

#include <lutok/exceptions.hpp>
#include <lutok/operations.hpp>
#include <lutok/stack_cleaner.hpp>
#include <lutok/state.ipp>

#include "engine/exceptions.hpp"
#include "engine/scheduler.hpp"
#include "model/metadata.hpp"
#include "model/test_program.hpp"
#include "utils/config/exceptions.hpp"
#include "utils/config/tree.ipp"
#include "utils/datetime.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/lua_module.hpp"
#include "utils/fs/operations.hpp"
#include "utils/logging/macros.hpp"
#include "utils/noncopyable.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"

namespace config = utils::config;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace scheduler = engine::scheduler;

using utils::none;
using utils::optional;


// History of Kyuafile file versions:
//
// 3 - DOES NOT YET EXIST.  Pending changes for when this is introduced:
//
//     * Revisit what to do about the test_suite definition.  Support for
//       per-test program overrides is deprecated and should be removed.
//       But, maybe, the whole test_suite definition idea is wrong and we
//       should instead be explicitly telling which configuration variables
//       to "inject" into each test program.
//
// 2 - Changed the syntax() call to take only a version number, instead of the
//     word 'config' as the first argument and the version as the second one.
//     Files now start with syntax(2) instead of syntax('kyuafile', 1).
//
// 1 - Initial version.


namespace {


static int lua_current_kyuafile(lutok::state&);
static int lua_generic_test_program(lutok::state&);
static int lua_include(lutok::state&);
static int lua_syntax(lutok::state&);
static int lua_test_suite(lutok::state&);


/// Concatenates two paths while avoiding paths to start with './'.
///
/// \param root Path to the directory containing the file.
/// \param file Path to concatenate to root.  Cannot be absolute.
///
/// \return The concatenated path.
static fs::path
relativize(const fs::path& root, const fs::path& file)
{
    PRE(!file.is_absolute());

    if (root == fs::path("."))
        return file;
    else
        return root / file;
}


/// Implementation of a parser for Kyuafiles.
///
/// The main purpose of having this as a class is to keep track of global state
/// within the Lua files and allowing the Lua callbacks to easily access such
/// data.
class parser : utils::noncopyable {
    /// Lua state to parse a single Kyuafile file.
    lutok::state _state;

    /// Root directory of the test suite represented by the Kyuafile.
    const fs::path _source_root;

    /// Root directory of the test programs.
    const fs::path _build_root;

    /// Name of the Kyuafile to load relative to _source_root.
    const fs::path _relative_filename;

    /// Version of the Kyuafile file format requested by the parsed file.
    ///
    /// This is set once the Kyuafile invokes the syntax() call.
    optional< int > _version;

    /// Name of the test suite defined by the Kyuafile.
    ///
    /// This is set once the Kyuafile invokes the test_suite() call.
    optional< std::string > _test_suite;

    /// Collection of test programs defined by the Kyuafile.
    ///
    /// This acts as an accumulator for all the *_test_program() calls within
    /// the Kyuafile.
    model::test_programs_vector _test_programs;

    /// Safely gets _test_suite and respects any test program overrides.
    ///
    /// \param program_override The test program-specific test suite name.  May
    ///     be empty to indicate no override.
    ///
    /// \return The name of the test suite.
    ///
    /// \throw std::runtime_error If program_override is empty and the Kyuafile
    /// did not yet define the global name of the test suite.
    std::string
    get_test_suite(const std::string& program_override)
    {
        std::string test_suite;

        if (program_override.empty()) {
            if (!_test_suite) {
                throw std::runtime_error("No test suite defined in the "
                                         "Kyuafile and no override provided in "
                                         "the test_program definition");
            }
            test_suite = _test_suite.get();
        } else {
            test_suite = program_override;
        }

        return test_suite;
    }

public:
    /// Initializes the parser and the Lua state.
    ///
    /// \param source_root_ The root directory of the test suite represented by
    ///     the Kyuafile.
    /// \param build_root_ The root directory of the test programs.
    /// \param relative_filename_ Name of the Kyuafile to load relative to
    ///     source_root_.
    /// \param user_config User configuration holding any test suite properties
    ///     to be passed to the list operation.
    /// \param scheduler_handle The scheduler context to use for loading the
    ///     test case lists.
    parser(const fs::path& source_root_, const fs::path& build_root_,
           const fs::path& relative_filename_,
           const config::tree& user_config,
           scheduler::scheduler_handle& scheduler_handle) :
        _source_root(source_root_), _build_root(build_root_),
        _relative_filename(relative_filename_)
    {
        lutok::stack_cleaner cleaner(_state);

        _state.push_cxx_function(lua_syntax);
        _state.set_global("syntax");

        *_state.new_userdata< parser* >() = this;
        _state.set_global("_parser");

        _state.push_cxx_function(lua_current_kyuafile);
        _state.set_global("current_kyuafile");

        *_state.new_userdata< const config::tree* >() = &user_config;
        *_state.new_userdata< scheduler::scheduler_handle* >() =
            &scheduler_handle;
        _state.push_cxx_closure(lua_include, 2);
        _state.set_global("include");

        _state.push_cxx_function(lua_test_suite);
        _state.set_global("test_suite");

        const std::set< std::string > interfaces =
            scheduler::registered_interface_names();
        for (std::set< std::string >::const_iterator iter = interfaces.begin();
             iter != interfaces.end(); ++iter) {
            const std::string& interface = *iter;

            _state.push_string(interface);
            *_state.new_userdata< const config::tree* >() = &user_config;
            *_state.new_userdata< scheduler::scheduler_handle* >() =
                &scheduler_handle;
            _state.push_cxx_closure(lua_generic_test_program, 3);
            _state.set_global(interface + "_test_program");
        }

        _state.open_base();
        _state.open_string();
        _state.open_table();
        fs::open_fs(_state, callback_current_kyuafile().branch_path());
    }

    /// Destructor.
    ~parser(void)
    {
    }

    /// Gets the parser object associated to a Lua state.
    ///
    /// \param state The Lua state from which to obtain the parser object.
    ///
    /// \return A pointer to the parser.
    static parser*
    get_from_state(lutok::state& state)
    {
        lutok::stack_cleaner cleaner(state);
        state.get_global("_parser");
        return *state.to_userdata< parser* >(-1);
    }

    /// Callback for the Kyuafile current_kyuafile() function.
    ///
    /// \return Returns the absolute path to the current Kyuafile.
    fs::path
    callback_current_kyuafile(void) const
    {
        const fs::path file = relativize(_source_root, _relative_filename);
        if (file.is_absolute())
            return file;
        else
            return file.to_absolute();
    }

    /// Callback for the Kyuafile include() function.
    ///
    /// \post _test_programs is extended with the the test programs defined by
    /// the included file.
    ///
    /// \param raw_file Path to the file to include.
    /// \param user_config User configuration holding any test suite properties
    ///     to be passed to the list operation.
    /// \param scheduler_handle Scheduler context to run test programs in.
    void
    callback_include(const fs::path& raw_file,
                     const config::tree& user_config,
                     scheduler::scheduler_handle& scheduler_handle)
    {
        const fs::path file = relativize(_relative_filename.branch_path(),
                                         raw_file);
        const model::test_programs_vector subtps =
            parser(_source_root, _build_root, file, user_config,
                   scheduler_handle).parse();

        std::copy(subtps.begin(), subtps.end(),
                  std::back_inserter(_test_programs));
    }

    /// Callback for the Kyuafile syntax() function.
    ///
    /// \post _version is set to the requested version.
    ///
    /// \param version Version of the Kyuafile syntax requested by the file.
    ///
    /// \throw std::runtime_error If the format or the version are invalid, or
    /// if syntax() has already been called.
    void
    callback_syntax(const int version)
    {
        if (_version)
            throw std::runtime_error("Can only call syntax() once");

        if (version < 1 || version > 2)
            throw std::runtime_error(F("Unsupported file version %s") %
                                     version);

        _version = utils::make_optional(version);
    }

    /// Callback for the various Kyuafile *_test_program() functions.
    ///
    /// \post _test_programs is extended to include the newly defined test
    /// program.
    ///
    /// \param interface Name of the test program interface.
    /// \param raw_path Path to the test program, relative to the Kyuafile.
    ///     This has to be adjusted according to the relative location of this
    ///     Kyuafile to _source_root.
    /// \param test_suite_override Name of the test suite this test program
    ///     belongs to, if explicitly defined at the test program level.
    /// \param metadata Metadata variables passed to the test program.
    /// \param user_config User configuration holding any test suite properties
    ///     to be passed to the list operation.
    /// \param scheduler_handle Scheduler context to run test programs in.
    ///
    /// \throw std::runtime_error If the test program definition is invalid or
    ///     if the test program does not exist.
    void
    callback_test_program(const std::string& interface,
                          const fs::path& raw_path,
                          const std::string& test_suite_override,
                          const model::metadata& metadata,
                          const config::tree& user_config,
                          scheduler::scheduler_handle& scheduler_handle)
    {
        if (raw_path.is_absolute())
            throw std::runtime_error(F("Got unexpected absolute path for test "
                                       "program '%s'") % raw_path);
        else if (raw_path.str() != raw_path.leaf_name())
            throw std::runtime_error(F("Test program '%s' cannot contain path "
                                       "components") % raw_path);

        const fs::path path = relativize(_relative_filename.branch_path(),
                                         raw_path);

        if (!fs::exists(_build_root / path))
            throw std::runtime_error(F("Non-existent test program '%s'") %
                                     path);

        const std::string test_suite = get_test_suite(test_suite_override);

        _test_programs.push_back(model::test_program_ptr(
            new scheduler::lazy_test_program(interface, path, _build_root,
                                             test_suite, metadata, user_config,
                                             scheduler_handle)));
    }

    /// Callback for the Kyuafile test_suite() function.
    ///
    /// \post _version is set to the requested version.
    ///
    /// \param name Name of the test suite.
    ///
    /// \throw std::runtime_error If test_suite() has already been called.
    void
    callback_test_suite(const std::string& name)
    {
        if (_test_suite)
            throw std::runtime_error("Can only call test_suite() once");
        _test_suite = utils::make_optional(name);
    }

    /// Parses the Kyuafile.
    ///
    /// \pre Can only be invoked once.
    ///
    /// \return The collection of test programs defined by the Kyuafile.
    ///
    /// \throw load_error If there is any problem parsing the file.
    const model::test_programs_vector&
    parse(void)
    {
        PRE(_test_programs.empty());

        const fs::path load_path = relativize(_source_root, _relative_filename);
        try {
            lutok::do_file(_state, load_path.str(), 0, 0, 0);
        } catch (const std::runtime_error& e) {
            // It is tempting to think that all of our various auxiliary
            // functions above could raise load_error by themselves thus making
            // this exception rewriting here unnecessary.  Howver, that would
            // not work because the helper functions above are executed within a
            // Lua context, and we lose their type when they are propagated out
            // of it.
            throw engine::load_error(load_path, e.what());
        }

        if (!_version)
            throw engine::load_error(load_path, "syntax() never called");

        return _test_programs;
    }
};


/// Glue to invoke parser::callback_test_program() from Lua.
///
/// This is a helper function for the various *_test_program() calls, as they
/// only differ in the interface of the defined test program.
///
/// \pre state(-1) A table with the arguments that define the test program.  The
/// special argument 'test_suite' provides an override to the global test suite
/// name.  The rest of the arguments are part of the test program metadata.
/// \pre state(upvalue 1) String with the name of the interface.
/// \pre state(upvalue 2) User configuration with the per-test suite settings.
/// \pre state(upvalue 3) Scheduler context to run test programs in.
///
/// \param state The Lua state that executed the function.
///
/// \return Number of return values left on the Lua stack.
///
/// \throw std::runtime_error If the arguments to the function are invalid.
static int
lua_generic_test_program(lutok::state& state)
{
    if (!state.is_string(state.upvalue_index(1)))
        throw std::runtime_error("Found corrupt state for test_program "
                                 "function");
    const std::string interface = state.to_string(state.upvalue_index(1));

    if (!state.is_userdata(state.upvalue_index(2)))
        throw std::runtime_error("Found corrupt state for test_program "
                                 "function");
    const config::tree* user_config = *state.to_userdata< const config::tree* >(
        state.upvalue_index(2));

    if (!state.is_userdata(state.upvalue_index(3)))
        throw std::runtime_error("Found corrupt state for test_program "
                                 "function");
    scheduler::scheduler_handle* scheduler_handle =
        *state.to_userdata< scheduler::scheduler_handle* >(
            state.upvalue_index(3));

    if (!state.is_table(-1))
        throw std::runtime_error(
            F("%s_test_program expects a table of properties as its single "
              "argument") % interface);

    scheduler::ensure_valid_interface(interface);

    lutok::stack_cleaner cleaner(state);

    state.push_string("name");
    state.get_table(-2);
    if (!state.is_string(-1))
        throw std::runtime_error("Test program name not defined or not a "
                                 "string");
    const fs::path path(state.to_string(-1));
    state.pop(1);

    state.push_string("test_suite");
    state.get_table(-2);
    std::string test_suite;
    if (state.is_nil(-1)) {
        // Leave empty to use the global test-suite value.
    } else if (state.is_string(-1)) {
        test_suite = state.to_string(-1);
    } else {
        throw std::runtime_error(F("Found non-string value in the test_suite "
                                   "property of test program '%s'") % path);
    }
    state.pop(1);

    model::metadata_builder mdbuilder;
    state.push_nil();
    while (state.next(-2)) {
        if (!state.is_string(-2))
            throw std::runtime_error(F("Found non-string metadata property "
                                       "name in test program '%s'") %
                                     path);
        const std::string property = state.to_string(-2);

        if (property != "name" && property != "test_suite") {
            std::string value;
            if (state.is_boolean(-1)) {
                value = F("%s") % state.to_boolean(-1);
            } else if (state.is_number(-1)) {
                value = F("%s") % state.to_integer(-1);
            } else if (state.is_string(-1)) {
                value = state.to_string(-1);
            } else {
                throw std::runtime_error(
                    F("Metadata property '%s' in test program '%s' cannot be "
                      "converted to a string") % property % path);
            }

            mdbuilder.set_string(property, value);
        }

        state.pop(1);
    }

    parser::get_from_state(state)->callback_test_program(
        interface, path, test_suite, mdbuilder.build(), *user_config,
        *scheduler_handle);
    return 0;
}


/// Glue to invoke parser::callback_current_kyuafile() from Lua.
///
/// \param state The Lua state that executed the function.
///
/// \return Number of return values left on the Lua stack.
static int
lua_current_kyuafile(lutok::state& state)
{
    state.push_string(parser::get_from_state(state)->
                      callback_current_kyuafile().str());
    return 1;
}


/// Glue to invoke parser::callback_include() from Lua.
///
/// \param state The Lua state that executed the function.
///
/// \pre state(upvalue 1) User configuration with the per-test suite settings.
/// \pre state(upvalue 2) Scheduler context to run test programs in.
///
/// \return Number of return values left on the Lua stack.
static int
lua_include(lutok::state& state)
{
    if (!state.is_userdata(state.upvalue_index(1)))
        throw std::runtime_error("Found corrupt state for test_program "
                                 "function");
    const config::tree* user_config = *state.to_userdata< const config::tree* >(
        state.upvalue_index(1));

    if (!state.is_userdata(state.upvalue_index(2)))
        throw std::runtime_error("Found corrupt state for test_program "
                                 "function");
    scheduler::scheduler_handle* scheduler_handle =
        *state.to_userdata< scheduler::scheduler_handle* >(
            state.upvalue_index(2));

    parser::get_from_state(state)->callback_include(
        fs::path(state.to_string(-1)), *user_config, *scheduler_handle);
    return 0;
}


/// Glue to invoke parser::callback_syntax() from Lua.
///
/// \pre state(-2) The syntax format name, if a v1 file.
/// \pre state(-1) The syntax format version.
///
/// \param state The Lua state that executed the function.
///
/// \return Number of return values left on the Lua stack.
static int
lua_syntax(lutok::state& state)
{
    if (!state.is_number(-1))
        throw std::runtime_error("Last argument to syntax must be a number");
    const int syntax_version = state.to_integer(-1);

    if (syntax_version == 1) {
        if (state.get_top() != 2)
            throw std::runtime_error("Version 1 files need two arguments to "
                                     "syntax()");
        if (!state.is_string(-2) || state.to_string(-2) != "kyuafile")
            throw std::runtime_error("First argument to syntax must be "
                                     "'kyuafile' for version 1 files");
    } else {
        if (state.get_top() != 1)
            throw std::runtime_error("syntax() only takes one argument");
    }

    parser::get_from_state(state)->callback_syntax(syntax_version);
    return 0;
}


/// Glue to invoke parser::callback_test_suite() from Lua.
///
/// \param state The Lua state that executed the function.
///
/// \return Number of return values left on the Lua stack.
static int
lua_test_suite(lutok::state& state)
{
    parser::get_from_state(state)->callback_test_suite(state.to_string(-1));
    return 0;
}


}  // anonymous namespace


/// Constructs a kyuafile form initialized data.
///
/// Use load() to parse a test suite configuration file and construct a
/// kyuafile object.
///
/// \param source_root_ The root directory for the test suite represented by the
///     Kyuafile.  In other words, the directory containing the first Kyuafile
///     processed.
/// \param build_root_ The root directory for the test programs themselves.  In
///     general, this will be the same as source_root_.  If different, the
///     specified directory must follow the exact same layout of source_root_.
/// \param tps_ Collection of test programs that belong to this test suite.
engine::kyuafile::kyuafile(const fs::path& source_root_,
                           const fs::path& build_root_,
                           const model::test_programs_vector& tps_) :
    _source_root(source_root_),
    _build_root(build_root_),
    _test_programs(tps_)
{
}


/// Destructor.
engine::kyuafile::~kyuafile(void)
{
}


/// Parses a test suite configuration file.
///
/// \param file The file to parse.
/// \param user_build_root If not none, specifies a path to a directory
///     containing the test programs themselves.  The layout of the build root
///     must match the layout of the source root (which is just the directory
///     from which the Kyuafile is being read).
/// \param user_config User configuration holding any test suite properties
///     to be passed to the list operation.
/// \param scheduler_handle The scheduler context to use for loading the test
///     case lists.
///
/// \return High-level representation of the configuration file.
///
/// \throw load_error If there is any problem loading the file.  This includes
///     file access errors and syntax errors.
engine::kyuafile
engine::kyuafile::load(const fs::path& file,
                       const optional< fs::path > user_build_root,
                       const config::tree& user_config,
                       scheduler::scheduler_handle& scheduler_handle)
{
    const fs::path source_root_ = file.branch_path();
    const fs::path build_root_ = user_build_root ?
        user_build_root.get() : source_root_;

    // test_program.absolute_path() uses the current work directory and that
    // fails to resolve the correct path once we have used chdir to enter the
    // test work directory.  To prevent this causing issues down the road,
    // force the build root to be absolute so that absolute_path() does not
    // need to rely on the current work directory.
    const fs::path abs_build_root = build_root_.is_absolute() ?
        build_root_ : build_root_.to_absolute();

    return kyuafile(source_root_, build_root_,
                    parser(source_root_, abs_build_root,
                           fs::path(file.leaf_name()), user_config,
                           scheduler_handle).parse());
}


/// Gets the root directory of the test suite.
///
/// \return A path.
const fs::path&
engine::kyuafile::source_root(void) const
{
    return _source_root;
}


/// Gets the root directory of the test programs.
///
/// \return A path.
const fs::path&
engine::kyuafile::build_root(void) const
{
    return _build_root;
}


/// Gets the collection of test programs that belong to this test suite.
///
/// \return Collection of test program executable names.
const model::test_programs_vector&
engine::kyuafile::test_programs(void) const
{
    return _test_programs;
}
