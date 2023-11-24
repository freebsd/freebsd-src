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

/// \file utils/cmdline/base_command.hpp
/// Provides the utils::cmdline::base_command class.

#if !defined(UTILS_CMDLINE_BASE_COMMAND_HPP)
#define UTILS_CMDLINE_BASE_COMMAND_HPP

#include "utils/cmdline/base_command_fwd.hpp"

#include <string>

#include "utils/cmdline/options_fwd.hpp"
#include "utils/cmdline/parser_fwd.hpp"
#include "utils/cmdline/ui_fwd.hpp"
#include "utils/noncopyable.hpp"

namespace utils {
namespace cmdline {


/// Prototype class for the implementation of subcommands of a program.
///
/// Use the subclasses of command_proto defined in this module instead of
/// command_proto itself as base classes for your application-specific
/// commands.
class command_proto : noncopyable {
    /// The user-visible name of the command.
    const std::string _name;

    /// Textual description of the command arguments.
    const std::string _arg_list;

    /// The minimum number of required arguments.
    const int _min_args;

    /// The maximum number of allowed arguments; -1 for infinity.
    const int _max_args;

    /// A textual description of the command.
    const std::string _short_description;

    /// Collection of command-specific options.
    options_vector _options;

    void add_option_ptr(const base_option*);

protected:
    template< typename Option > void add_option(const Option&);
    parsed_cmdline parse_cmdline(const args_vector&) const;

public:
    command_proto(const std::string&, const std::string&, const int, const int,
                  const std::string&);
    virtual ~command_proto(void);

    const std::string& name(void) const;
    const std::string& arg_list(void) const;
    const std::string& short_description(void) const;
    const options_vector& options(void) const;
};


/// Unparametrized base subcommand for a program.
///
/// Use this class to define subcommands for your program that do not need any
/// information passed in from the main command-line dispatcher other than the
/// command-line arguments.
class base_command_no_data : public command_proto {
    /// Main code of the command.
    ///
    /// This is called from main() after the command line has been processed and
    /// validated.
    ///
    /// \param ui Object to interact with the I/O of the command.  The command
    ///     must always use this object to write to stdout and stderr.
    /// \param cmdline The parsed command line, containing the values of any
    ///     given options and arguments.
    ///
    /// \return The exit code that the program has to return.  0 on success,
    ///     some other value on error.
    ///
    /// \throw std::runtime_error Any errors detected during the execution of
    ///     the command are reported by means of exceptions.
    virtual int run(ui* ui, const parsed_cmdline& cmdline) = 0;

public:
    base_command_no_data(const std::string&, const std::string&, const int,
                         const int, const std::string&);

    int main(ui*, const args_vector&);
};


/// Parametrized base subcommand for a program.
///
/// Use this class to define subcommands for your program that need some kind of
/// runtime information passed in from the main command-line dispatcher.
///
/// \param Data The type of the object passed to the subcommand at runtime.
/// This is useful, for example, to pass around the runtime configuration of the
/// program.
template< typename Data >
class base_command : public command_proto {
    /// Main code of the command.
    ///
    /// This is called from main() after the command line has been processed and
    /// validated.
    ///
    /// \param ui Object to interact with the I/O of the command.  The command
    ///     must always use this object to write to stdout and stderr.
    /// \param cmdline The parsed command line, containing the values of any
    ///     given options and arguments.
    /// \param data An instance of the runtime data passed from main().
    ///
    /// \return The exit code that the program has to return.  0 on success,
    ///     some other value on error.
    ///
    /// \throw std::runtime_error Any errors detected during the execution of
    ///     the command are reported by means of exceptions.
    virtual int run(ui* ui, const parsed_cmdline& cmdline,
                    const Data& data) = 0;

public:
    base_command(const std::string&, const std::string&, const int, const int,
                 const std::string&);

    int main(ui*, const args_vector&, const Data&);
};


}  // namespace cmdline
}  // namespace utils


#endif  // !defined(UTILS_CMDLINE_BASE_COMMAND_HPP)
