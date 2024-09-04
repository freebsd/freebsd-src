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

#include "os/freebsd/execenv_jail_manager.hpp"

#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "os/freebsd/execenv_jail.hpp"

static const std::string execenv_name = "jail";

const std::string&
freebsd::execenv_jail_manager::name() const
{
    return execenv_name;
}


bool
freebsd::execenv_jail_manager::is_supported() const
{
    return freebsd::execenv_jail_supported;
}


std::unique_ptr< execenv::interface >
freebsd::execenv_jail_manager::probe(
    const model::test_program& test_program,
    const std::string& test_case_name) const
{
    auto test_case = test_program.find(test_case_name);
    if (test_case.get_metadata().execenv() != execenv_name)
        return nullptr;

    return std::unique_ptr< execenv::interface >(
        new freebsd::execenv_jail(test_program, test_case_name)
    );
}
