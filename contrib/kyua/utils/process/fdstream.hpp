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

/// \file utils/process/fdstream.hpp
/// Provides the utils::process::ifdstream class.

#if !defined(UTILS_PROCESS_FDSTREAM_HPP)
#define UTILS_PROCESS_FDSTREAM_HPP

#include "utils/process/fdstream_fwd.hpp"

#include <istream>
#include <memory>

#include "utils/noncopyable.hpp"

namespace utils {
namespace process {


/// An input stream backed by a file descriptor.
///
/// This class grabs ownership of the file descriptor.  I.e. when the class is
/// destroyed, the file descriptor is closed unconditionally.
class ifdstream : public std::istream, noncopyable
{
    struct impl;

    /// Pointer to the shared internal implementation.
    std::unique_ptr< impl > _pimpl;

public:
    explicit ifdstream(const int);
    ~ifdstream(void);
};


}  // namespace process
}  // namespace utils

#endif  // !defined(UTILS_PROCESS_FDSTREAM_HPP)
