// Copyright 2012 The Kyua Authors.
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

/// \file engine/requirements.hpp
/// Handling of test case requirements.

#if !defined(ENGINE_REQUIREMENTS_HPP)
#define ENGINE_REQUIREMENTS_HPP

#include <string>

#include "model/metadata_fwd.hpp"
#include "utils/config/tree_fwd.hpp"
#include "utils/fs/path_fwd.hpp"

namespace engine {


std::string check_reqs(const model::metadata&, const utils::config::tree&,
                       const std::string&, const utils::fs::path&);

/// Abstract interface of a requirement checker.
class reqs_checker {
public:
    /// Constructor.
    reqs_checker() {}

    /// Destructor.
    virtual ~reqs_checker() {}

    /// Run the checker.
    virtual std::string exec(const model::metadata&,
                             const utils::config::tree&,
                             const std::string&,
                             const utils::fs::path&) const = 0;
};

/// Register an extra requirement checker.
///
/// \param checker A requirement checker.
void register_reqs_checker(const std::shared_ptr< reqs_checker > checker);

/// Returns the list of registered extra requirement checkers.
///
/// \return A vector of pointers to extra requirement checkers.
const std::vector< std::shared_ptr< reqs_checker > > reqs_checkers();


}  // namespace engine


#endif  // !defined(ENGINE_REQUIREMENTS_HPP)
