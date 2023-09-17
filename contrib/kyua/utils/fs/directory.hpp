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

/// \file utils/fs/directory.hpp
/// Provides the utils::fs::directory class.

#if !defined(UTILS_FS_DIRECTORY_HPP)
#define UTILS_FS_DIRECTORY_HPP

#include "utils/fs/directory_fwd.hpp"

#include <memory>
#include <ostream>
#include <string>

#include "utils/fs/path_fwd.hpp"

namespace utils {
namespace fs {


/// Representation of a single directory entry.
struct directory_entry {
    /// Name of the directory entry.
    std::string name;

    explicit directory_entry(const std::string&);

    bool operator==(const directory_entry&) const;
    bool operator!=(const directory_entry&) const;
    bool operator<(const directory_entry&) const;
};


std::ostream& operator<<(std::ostream&, const directory_entry&);


namespace detail {


/// Forward directory iterator.
class directory_iterator {
    struct impl;

    /// Internal implementation details.
    std::shared_ptr< impl > _pimpl;

    directory_iterator(std::shared_ptr< impl >);

    friend class fs::directory;
    static directory_iterator new_begin(const path&);
    static directory_iterator new_end(void);

public:
    ~directory_iterator();

    bool operator==(const directory_iterator&) const;
    bool operator!=(const directory_iterator&) const;
    directory_iterator& operator++(void);

    const directory_entry& operator*(void) const;
    const directory_entry* operator->(void) const;
};


}  // namespace detail


/// Representation of a local filesystem directory.
///
/// This class is pretty much stateless.  All the directory manipulation
/// operations happen within the iterator.
class directory {
public:
    /// Public type for a constant forward directory iterator.
    typedef detail::directory_iterator const_iterator;

private:
    struct impl;

    /// Internal implementation details.
    std::shared_ptr< impl > _pimpl;

public:
    explicit directory(const path&);

    const_iterator begin(void) const;
    const_iterator end(void) const;
};


}  // namespace fs
}  // namespace utils

#endif  // !defined(UTILS_FS_DIRECTORY_HPP)
