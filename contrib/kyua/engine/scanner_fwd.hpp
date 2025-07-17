// Copyright 2015 The Kyua Authors.
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

/// \file engine/scanner_fwd.hpp
/// Forward declarations for engine/scanner.hpp

#if !defined(ENGINE_SCANNER_FWD_HPP)
#define ENGINE_SCANNER_FWD_HPP

#include <string>
#include <utility>

#include "model/test_program_fwd.hpp"

namespace engine {


/// Result type yielded by the scanner: a (test program, test case name) pair.
///
/// We must use model::test_program_ptr here instead of model::test_program
/// because we must keep the polimorphic properties of the test program.  In
/// particular, if the test program comes from the Kyuafile and is of the type
/// model::lazy_test_program, we must keep access to the loaded list of test
/// cases (which, for obscure reasons, is kept in the subclass).
/// TODO(jmmv): This is ugly, very ugly.  There has to be a better way.
typedef std::pair< model::test_program_ptr, std::string > scan_result;


class scanner;


}  // namespace engine

#endif  // !defined(ENGINE_SCANNER_FWD_HPP)
