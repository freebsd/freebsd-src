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

/// \file utils/fs/path.hpp
/// Provides the utils::fs::path class.
///
/// This is a poor man's reimplementation of the path class provided by
/// Boost.Filesystem, in the sense that it tries to follow the same API but is
/// much simplified.

#if !defined(UTILS_FS_PATH_HPP)
#define UTILS_FS_PATH_HPP

#include "utils/fs/path_fwd.hpp"

#include <string>
#include <ostream>

namespace utils {
namespace fs {


/// Representation and manipulation of a file system path.
///
/// Application code should always use this class to represent a path instead of
/// std::string, because this class is more semantically representative, ensures
/// that the values are valid and provides some useful manipulation functions.
///
/// Conversions to and from strings are always explicit.
class path {
    /// Internal representation of the path.
    std::string _repr;

public:
    explicit path(const std::string&);

    const char* c_str(void) const;
    const std::string& str(void) const;

    path branch_path(void) const;
    std::string leaf_name(void) const;
    path to_absolute(void) const;

    bool is_absolute(void) const;
    bool is_parent_of(path) const;
    int ncomponents(void) const;

    bool operator<(const path&) const;
    bool operator==(const path&) const;
    bool operator!=(const path&) const;
    path operator/(const std::string&) const;
    path operator/(const path&) const;
};


std::ostream& operator<<(std::ostream&, const path&);


}  // namespace fs
}  // namespace utils

#endif  // !defined(UTILS_FS_PATH_HPP)
