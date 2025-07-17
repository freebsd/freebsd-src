// Copyright 2011 The Kyua Authors.
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

/// \file model/context.hpp
/// Representation of runtime contexts.

#if !defined(MODEL_CONTEXT_HPP)
#define MODEL_CONTEXT_HPP

#include "model/context_fwd.hpp"

#include <map>
#include <memory>
#include <ostream>
#include <string>

#include "utils/fs/path_fwd.hpp"

namespace model {


/// Representation of a runtime context.
///
/// The instances of this class are unique (i.e. copying the objects only yields
/// a shallow copy that shares the same internal implementation).  This is a
/// requirement for the 'store' API model.
class context {
    struct impl;

    /// Pointer to the shared internal implementation.
    std::shared_ptr< impl > _pimpl;

public:
    context(const utils::fs::path&,
            const std::map< std::string, std::string >&);
    ~context(void);

    const utils::fs::path& cwd(void) const;
    const std::map< std::string, std::string >& env(void) const;

    bool operator==(const context&) const;
    bool operator!=(const context&) const;
};


std::ostream& operator<<(std::ostream&, const context&);


}  // namespace model

#endif  // !defined(MODEL_CONTEXT_HPP)
