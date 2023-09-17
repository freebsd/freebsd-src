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

#include "utils/process/fdstream.hpp"

#include "utils/noncopyable.hpp"
#include "utils/process/systembuf.hpp"


namespace utils {
namespace process {


/// Private implementation fields for ifdstream.
struct ifdstream::impl : utils::noncopyable {
    /// The systembuf backing this file descriptor.
    systembuf _systembuf;

    /// Initializes private implementation data.
    ///
    /// \param fd The file descriptor.
    impl(const int fd) : _systembuf(fd) {}
};


}  // namespace process
}  // namespace utils


namespace process = utils::process;


/// Constructs a new ifdstream based on an open file descriptor.
///
/// This grabs ownership of the file descriptor.
///
/// \param fd The file descriptor to read from.  Must be open and valid.
process::ifdstream::ifdstream(const int fd) :
    std::istream(NULL),
    _pimpl(new impl(fd))
{
    rdbuf(&_pimpl->_systembuf);
}


/// Destroys an ifdstream object.
///
/// \post The file descriptor attached to this stream is closed.
process::ifdstream::~ifdstream(void)
{
}
