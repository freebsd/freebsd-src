// Copyright 2023 The Kyua Authors.
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

#include "engine/execenv/execenv.hpp"

#include "engine/execenv/execenv_host.hpp"

namespace execenv = engine::execenv;

using utils::none;


const char* execenv::default_execenv_name = "host";


/// List of registered execution environments, except default host one.
///
/// Use register_execenv() to add an entry to this global list.
static std::vector< std::shared_ptr< execenv::manager > >
    execenv_managers;


void
execenv::register_execenv(const std::shared_ptr< execenv::manager > manager)
{
    execenv_managers.push_back(manager);
}


const std::vector< std::shared_ptr< execenv::manager> >
execenv::execenvs()
{
    return execenv_managers;
}


std::unique_ptr< execenv::interface >
execenv::get(const model::test_program& test_program,
             const std::string& test_case_name)
{
    for (auto m : execenv_managers) {
        auto e = m->probe(test_program, test_case_name);
        if (e != nullptr)
            return e;
    }

    return std::unique_ptr< execenv::interface >(
        new execenv::execenv_host(test_program, test_case_name));
}
