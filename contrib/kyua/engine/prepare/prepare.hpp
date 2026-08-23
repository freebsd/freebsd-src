// Copyright 2024 The Kyua Authors.
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

/// \file engine/prepare/prepare.hpp
/// Requirement preparation subsystem interface.

#if !defined(ENGINE_PREPARE_HPP)
#define ENGINE_PREPARE_HPP

#include <string>
#include <vector>

#include "utils/cmdline/parser.ipp"
#include "utils/cmdline/ui.hpp"
#include "utils/config/tree.ipp"

namespace cmdline = utils::cmdline;
namespace config = utils::config;


namespace engine {
namespace prepare {


/// Abstract interface of a requirement preparation handler.
class handler {
public:
    /// Constructor.
    handler() {}

    /// Destructor.
    virtual ~handler() {}

    /// Returns name of the handler.
    virtual const std::string& name() const = 0;

    /// Returns short description of the handler.
    virtual const std::string& description() const = 0;

    /// Runs the requirement preparation handler.
    ///
    /// \param ui Object to interact with the I/O of the program.
    /// \param cmdline Representation of the command line to the subcommand.
    /// \param user_config The runtime configuration of the program.
    ///
    /// \return 0 to indicate success.
    virtual int exec(cmdline::ui* ui,
                     const cmdline::parsed_cmdline& cmdline,
                     const config::tree& user_config) const = 0;
};


/// Registers a requirement preparation handler.
///
/// \param handler A requirement preparation handler.
void register_handler(const std::shared_ptr< handler > handler);


/// Returns the list of registered requirement preparation handlers.
///
/// \return A vector of pointers to requirement preparation handlers.
const std::vector< std::shared_ptr< handler > > handlers();


/// Run named handlers.
///
/// \param handler_names Names of the handlers to run.
/// \param ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
/// \param user_config The runtime configuration of the program.
///
/// \return 0 to indicate success.
int run(const std::vector< std::string >& handler_names,
        cmdline::ui* ui,
        const cmdline::parsed_cmdline& cmdline,
        const config::tree& user_config);


}  // namespace prepare
}  // namespace engine

#endif  // !defined(ENGINE_PREPARE_HPP)
