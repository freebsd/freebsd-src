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

#include "os/freebsd/main.hpp"

#include "engine/execenv/execenv.hpp"
#include "os/freebsd/execenv_jail_manager.hpp"

#include "engine/requirements.hpp"
#include "os/freebsd/reqs_checker_kmods.hpp"

namespace execenv = engine::execenv;

/// FreeBSD related features initialization.
///
/// \param argc The number of arguments passed on the command line.
/// \param argv NULL-terminated array containing the command line arguments.
///
/// \return 0 on success, some other integer on error.
///
/// \throw std::exception This throws any uncaught exception.  Such exceptions
///     are bugs, but we let them propagate so that the runtime will abort and
///     dump core.
int
freebsd::main(const int, const char* const* const)
{
    execenv::register_execenv(
        std::shared_ptr< execenv::manager >(new freebsd::execenv_jail_manager())
    );

#ifdef __FreeBSD__
    engine::register_reqs_checker(
        std::shared_ptr< engine::reqs_checker >(
            new freebsd::reqs_checker_kmods()
        )
    );
#endif

    return 0;
}
