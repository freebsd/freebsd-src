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

#include "utils/process/systembuf.hpp"

extern "C" {
#include <unistd.h>
}

#include "utils/auto_array.ipp"
#include "utils/noncopyable.hpp"
#include "utils/sanity.hpp"

using utils::process::systembuf;


/// Private implementation fields for systembuf.
struct systembuf::impl : utils::noncopyable {
    /// File descriptor attached to the systembuf.
    int _fd;

    /// Size of the _read_buf and _write_buf buffers.
    std::size_t _bufsize;

    /// In-memory buffer for read operations.
    utils::auto_array< char > _read_buf;

    /// In-memory buffer for write operations.
    utils::auto_array< char > _write_buf;

    /// Initializes private implementation data.
    ///
    /// \param fd The file descriptor.
    /// \param bufsize The size of the created read and write buffers.
    impl(const int fd, const std::size_t bufsize) :
        _fd(fd),
        _bufsize(bufsize),
        _read_buf(new char[bufsize]),
        _write_buf(new char[bufsize])
    {
    }
};


/// Constructs a new systembuf based on an open file descriptor.
///
/// This grabs ownership of the file descriptor.
///
/// \param fd The file descriptor to wrap.  Must be open and valid.
/// \param bufsize The size to use for the internal read/write buffers.
systembuf::systembuf(const int fd, std::size_t bufsize) :
    _pimpl(new impl(fd, bufsize))
{
    setp(_pimpl->_write_buf.get(), _pimpl->_write_buf.get() + _pimpl->_bufsize);
}


/// Destroys a systembuf object.
///
/// \post The file descriptor attached to this systembuf is closed.
systembuf::~systembuf(void)
{
    ::close(_pimpl->_fd);
}


/// Reads new data when the systembuf read buffer underflows.
///
/// \return The new character to be read, or EOF if no more.
systembuf::int_type
systembuf::underflow(void)
{
    PRE(gptr() >= egptr());

    bool ok;
    ssize_t cnt = ::read(_pimpl->_fd, _pimpl->_read_buf.get(),
                         _pimpl->_bufsize);
    ok = (cnt != -1 && cnt != 0);

    if (!ok)
        return traits_type::eof();
    else {
        setg(_pimpl->_read_buf.get(), _pimpl->_read_buf.get(),
             _pimpl->_read_buf.get() + cnt);
        return traits_type::to_int_type(*gptr());
    }
}


/// Writes data to the file descriptor when the write buffer overflows.
///
/// \param c The character that causes the overflow.
///
/// \return EOF if error, some other value for success.
///
/// \throw something TODO(jmmv): According to the documentation, it is OK for
///     this method to throw in case of errors.  Revisit this code to see if we
///     can do better.
systembuf::int_type
systembuf::overflow(int c)
{
    PRE(pptr() >= epptr());
    if (sync() == -1)
        return traits_type::eof();
    if (!traits_type::eq_int_type(c, traits_type::eof())) {
        traits_type::assign(*pptr(), c);
        pbump(1);
    }
    return traits_type::not_eof(c);
}


/// Synchronizes the stream with the file descriptor.
///
/// \return 0 on success, -1 on error.
int
systembuf::sync(void)
{
    ssize_t cnt = pptr() - pbase();

    bool ok;
    ok = ::write(_pimpl->_fd, pbase(), cnt) == cnt;

    if (ok)
        pbump(-cnt);
    return ok ? 0 : -1;
}
