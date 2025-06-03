// Copyright 2025 The Kyua Authors.
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

/// \file engine/debugger.hpp
/// The interface between the engine and the users outside.

#if !defined(ENGINE_DEBUGGER_HPP)
#define ENGINE_DEBUGGER_HPP

#include "model/test_case_fwd.hpp"
#include "model/test_program_fwd.hpp"
#include "model/test_result_fwd.hpp"
#include "utils/optional_fwd.hpp"
#include "utils/process/executor_fwd.hpp"

namespace executor = utils::process::executor;

using utils::optional;


namespace engine {


/// Abstract debugger interface.
class debugger {
public:
    debugger() {}
    virtual ~debugger() {}

    /// Called right before test cleanup.
    virtual void before_cleanup(
        const model::test_program_ptr&,
        const model::test_case&,
        optional< model::test_result >&,
        executor::exit_handle&) const = 0;
};


/// Pointer to a debugger implementation.
typedef std::shared_ptr< debugger > debugger_ptr;


}  // namespace engine


#endif  // !defined(ENGINE_DEBUGGER_HPP)
