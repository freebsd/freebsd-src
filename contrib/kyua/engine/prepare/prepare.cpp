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

#include "engine/prepare/prepare.hpp"

#include "engine/prepare/prepare_all.hpp"

namespace prepare = engine::prepare;


/// List of registered requirement preparation handlers.
///
/// Use register_handler() to add an entry to this global list.
static std::vector< std::shared_ptr< prepare::handler > > _handlers = {
    std::shared_ptr< prepare::handler >(new prepare::prepare_all())
};


void
prepare::register_handler(const std::shared_ptr< handler > handler)
{
    _handlers.push_back(handler);
}


const std::vector< std::shared_ptr< prepare::handler > >
prepare::handlers()
{
    return _handlers;
}


int
prepare::run(const std::vector< std::string >& handler_names,
        cmdline::ui* ui,
        const cmdline::parsed_cmdline& cmdline,
        const config::tree& user_config)
{
    for (auto& hname : handler_names) {
        std::shared_ptr< prepare::handler > handler = nullptr;
        for (auto& h : prepare::handlers())
            if (h->name() == hname) {
                handler = h;
                break;
            }

        if (handler == nullptr) {
            ui->out(F("Unknown requirement preparation handler: %s") % hname);
            return EXIT_FAILURE;
        }

        if (handler->exec(ui, cmdline, user_config) != 0)
            // suppress the actual code -- main limits possible exit codes
            return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
