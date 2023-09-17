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

/// \file utils/fs/operations.hpp
/// File system algorithms and access functions.
///
/// The functions in this module are exception-based, type-improved wrappers
/// over the functions provided by libc.

#if !defined(UTILS_FS_OPERATIONS_HPP)
#define UTILS_FS_OPERATIONS_HPP

#include <set>
#include <string>

#include "utils/fs/directory_fwd.hpp"
#include "utils/fs/path_fwd.hpp"
#include "utils/optional_fwd.hpp"
#include "utils/units_fwd.hpp"

namespace utils {
namespace fs {


void copy(const fs::path&, const fs::path&);
path current_path(void);
bool exists(const fs::path&);
utils::optional< path > find_in_path(const char*);
utils::units::bytes free_disk_space(const fs::path&);
bool is_directory(const fs::path&);
void mkdir(const path&, const int);
void mkdir_p(const path&, const int);
fs::path mkdtemp_public(const std::string&);
fs::path mkstemp(const std::string&);
void mount_tmpfs(const path&);
void mount_tmpfs(const path&, const units::bytes&);
void rm_r(const path&);
void rmdir(const path&);
std::set< directory_entry > scan_directory(const path&);
void unlink(const path&);
void unmount(const path&);


}  // namespace fs
}  // namespace utils

#endif  // !defined(UTILS_FS_OPERATIONS_HPP)
