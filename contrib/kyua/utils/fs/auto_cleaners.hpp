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

/// \file utils/fs/auto_cleaners.hpp
/// RAII wrappers to automatically remove file system entries.

#if !defined(UTILS_FS_AUTO_CLEANERS_HPP)
#define UTILS_FS_AUTO_CLEANERS_HPP

#include "utils/fs/auto_cleaners_fwd.hpp"

#include <memory>
#include <string>

#include "utils/fs/path_fwd.hpp"

namespace utils {
namespace fs {


/// Grabs ownership of a directory and removes it upon destruction.
///
/// This class is reference-counted and therefore only the destruction of the
/// last instance will cause the removal of the directory.
class auto_directory {
    struct impl;
    /// Reference-counted, shared implementation.
    std::shared_ptr< impl > _pimpl;

public:
    explicit auto_directory(const path&);
    ~auto_directory(void);

    static auto_directory mkdtemp_public(const std::string&);

    const path& directory(void) const;
    void cleanup(void);
};


/// Grabs ownership of a file and removes it upon destruction.
///
/// This class is reference-counted and therefore only the destruction of the
/// last instance will cause the removal of the file.
class auto_file {
    struct impl;
    /// Reference-counted, shared implementation.
    std::shared_ptr< impl > _pimpl;

public:
    explicit auto_file(const path&);
    ~auto_file(void);

    static auto_file mkstemp(const std::string&);

    const path& file(void) const;
    void remove(void);
};


}  // namespace fs
}  // namespace utils

#endif  // !defined(UTILS_FS_AUTO_CLEANERS_HPP)
