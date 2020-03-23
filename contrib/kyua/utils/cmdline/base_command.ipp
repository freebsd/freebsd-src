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

#if !defined(UTILS_CMDLINE_BASE_COMMAND_IPP)
#define UTILS_CMDLINE_BASE_COMMAND_IPP

#include "utils/cmdline/base_command.hpp"


namespace utils {
namespace cmdline {


/// Adds an option to the command.
///
/// This is to be called from the constructor of the subclass that implements
/// the command.
///
/// \param option_ The option to add.
template< typename Option >
void
command_proto::add_option(const Option& option_)
{
    add_option_ptr(new Option(option_));
}


/// Creates a new command.
///
/// \param name_ The name of the command.  Must be unique within the context of
///     a program and have no spaces.
/// \param arg_list_ A textual description of the arguments received by the
///     command.  May be empty.
/// \param min_args_ The minimum number of arguments required by the command.
/// \param max_args_ The maximum number of arguments required by the command.
///     -1 means infinity.
/// \param short_description_ A description of the purpose of the command.
template< typename Data >
base_command< Data >::base_command(const std::string& name_,
                                   const std::string& arg_list_,
                                   const int min_args_,
                                   const int max_args_,
                                   const std::string& short_description_) :
    command_proto(name_, arg_list_, min_args_, max_args_, short_description_)
{
}


/// Entry point for the command.
///
/// This delegates execution to the run() abstract function after the command
/// line provided in args has been parsed.
///
/// If this function returns, the command is assumed to have been executed
/// successfully.  Any error must be reported by means of exceptions.
///
/// \param ui Object to interact with the I/O of the command.  The command must
///     always use this object to write to stdout and stderr.
/// \param args The command line passed to the command broken by word, which
///     includes options and arguments.
/// \param data An opaque data structure to pass to the run method.
///
/// \return The exit code that the program has to return.  0 on success, some
///     other value on error.
/// \throw usage_error If args is invalid (i.e. if the options are mispecified
///     or if the arguments are invalid).
template< typename Data >
int
base_command< Data >::main(ui* ui, const args_vector& args, const Data& data)
{
    return run(ui, parse_cmdline(args), data);
}


}  // namespace cli
}  // namespace utils


#endif  // !defined(UTILS_CMDLINE_BASE_COMMAND_IPP)
