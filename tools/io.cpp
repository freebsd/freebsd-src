//
// Automated Testing Framework (atf)
//
// Copyright (c) 2007 The NetBSD Foundation, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
// CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
// IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

extern "C" {
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
}

#include <cassert>
#include <cerrno>
#include <cstring>

#include "auto_array.hpp"
#include "exceptions.hpp"
#include "io.hpp"

namespace impl = tools::io;
#define IMPL_NAME "tools::io"

// ------------------------------------------------------------------------
// The "file_handle" class.
// ------------------------------------------------------------------------

impl::file_handle::file_handle(void) :
    m_handle(invalid_value())
{
}

impl::file_handle::file_handle(handle_type h) :
    m_handle(h)
{
    assert(m_handle != invalid_value());
}

impl::file_handle::file_handle(const file_handle& fh) :
    m_handle(fh.m_handle)
{
    fh.m_handle = invalid_value();
}

impl::file_handle::~file_handle(void)
{
    if (is_valid())
        close();
}

impl::file_handle&
impl::file_handle::operator=(const file_handle& fh)
{
    m_handle = fh.m_handle;
    fh.m_handle = invalid_value();

    return *this;
}

bool
impl::file_handle::is_valid(void)
    const
{
    return m_handle != invalid_value();
}

void
impl::file_handle::close(void)
{
    assert(is_valid());

    ::close(m_handle);

    m_handle = invalid_value();
}

impl::file_handle::handle_type
impl::file_handle::disown(void)
{
    assert(is_valid());

    handle_type h = m_handle;
    m_handle = invalid_value();
    return h;
}

impl::file_handle::handle_type
impl::file_handle::get(void)
    const
{
    assert(is_valid());

    return m_handle;
}

void
impl::file_handle::posix_remap(handle_type h)
{
    assert(is_valid());

    if (m_handle == h)
        return;

    if (::dup2(m_handle, h) == -1)
        throw tools::system_error(IMPL_NAME "::file_handle::posix_remap",
                                "dup2(2) failed", errno);

    if (::close(m_handle) == -1) {
        ::close(h);
        throw tools::system_error(IMPL_NAME "::file_handle::posix_remap",
                                "close(2) failed", errno);
    }

    m_handle = h;
}

impl::file_handle::handle_type
impl::file_handle::invalid_value(void)
{
    return -1;
}

// ------------------------------------------------------------------------
// The "systembuf" class.
// ------------------------------------------------------------------------

impl::systembuf::systembuf(handle_type h, std::size_t bufsize) :
    m_handle(h),
    m_bufsize(bufsize),
    m_read_buf(NULL),
    m_write_buf(NULL)
{
    assert(m_handle >= 0);
    assert(m_bufsize > 0);

    try {
        m_read_buf = new char[bufsize];
        m_write_buf = new char[bufsize];
    } catch (...) {
        if (m_read_buf != NULL)
            delete [] m_read_buf;
        if (m_write_buf != NULL)
            delete [] m_write_buf;
        throw;
    }

    setp(m_write_buf, m_write_buf + m_bufsize);
}

impl::systembuf::~systembuf(void)
{
    delete [] m_read_buf;
    delete [] m_write_buf;
}

impl::systembuf::int_type
impl::systembuf::underflow(void)
{
    assert(gptr() >= egptr());

    bool ok;
    ssize_t cnt = ::read(m_handle, m_read_buf, m_bufsize);
    ok = (cnt != -1 && cnt != 0);

    if (!ok)
        return traits_type::eof();
    else {
        setg(m_read_buf, m_read_buf, m_read_buf + cnt);
        return traits_type::to_int_type(*gptr());
    }
}

impl::systembuf::int_type
impl::systembuf::overflow(int c)
{
    assert(pptr() >= epptr());
    if (sync() == -1)
        return traits_type::eof();
    if (!traits_type::eq_int_type(c, traits_type::eof())) {
        traits_type::assign(*pptr(), c);
        pbump(1);
    }
    return traits_type::not_eof(c);
}

int
impl::systembuf::sync(void)
{
    ssize_t cnt = pptr() - pbase();

    bool ok;
    ok = ::write(m_handle, pbase(), cnt) == cnt;

    if (ok)
        pbump(-cnt);
    return ok ? 0 : -1;
}

// ------------------------------------------------------------------------
// The "pistream" class.
// ------------------------------------------------------------------------

impl::pistream::pistream(const int fd) :
    std::istream(NULL),
    m_systembuf(fd)
{
    rdbuf(&m_systembuf);
}

// ------------------------------------------------------------------------
// The "muxer" class.
// ------------------------------------------------------------------------

static int
safe_poll(struct pollfd fds[], nfds_t nfds, int timeout)
{
    int ret = ::poll(fds, nfds, timeout);
    if (ret == -1) {
        if (errno == EINTR)
            ret = 0;
        else
            throw tools::system_error(IMPL_NAME "::safe_poll", "poll(2) failed",
                                    errno);
    }
    assert(ret >= 0);
    return ret;
}

static size_t
safe_read(const int fd, void* buffer, const size_t nbytes,
          const bool report_errors)
{
    int ret;
    while ((ret = ::read(fd, buffer, nbytes)) == -1 && errno == EINTR) {}
    if (ret == -1) {
        assert(errno != EINTR);

        if (report_errors)
            throw tools::system_error(IMPL_NAME "::safe_read", "read(2) failed",
                                    errno);
        else
            ret = 0;
    }
    assert(ret >= 0);
    return static_cast< size_t >(ret);
}

impl::muxer::muxer(const int* fds, const size_t nfds, const size_t bufsize) :
    m_fds(fds),
    m_nfds(nfds),
    m_bufsize(bufsize),
    m_buffers(new std::string[nfds])
{
}

impl::muxer::~muxer(void)
{
}

size_t
impl::muxer::read_one(const size_t index, const int fd, std::string& accum,
                      const bool report_errors)
{
    tools::auto_array< char > buffer(new char[m_bufsize]);
    const size_t nbytes = safe_read(fd, buffer.get(), m_bufsize - 1,
                                    report_errors);
    assert(nbytes < m_bufsize);
    buffer[nbytes] = '\0';

    std::string line(accum);

    size_t line_start = 0;
    for (size_t i = 0; i < nbytes; i++) {
        if (buffer[i] == '\n') {
            line_callback(index, line);
            line.clear();
            accum.clear();
            line_start = i + 1;
        } else if (buffer[i] == '\r') {
            // Do nothing.
        } else {
            line.append(1, buffer[i]);
        }
    }
    accum.append(&buffer[line_start]);

    return nbytes;
}

void
impl::muxer::mux(volatile const bool& terminate)
{
    tools::auto_array< struct pollfd > poll_fds(new struct pollfd[m_nfds]);
    for (size_t i = 0; i < m_nfds; i++) {
        poll_fds[i].fd = m_fds[i];
        poll_fds[i].events = POLLIN;
    }

    size_t nactive = m_nfds;
    while (nactive > 0 && !terminate) {
        int ret;
        while (!terminate && (ret = safe_poll(poll_fds.get(), 2, 250)) == 0) {}

        for (size_t i = 0; !terminate && i < m_nfds; i++) {
            if (poll_fds[i].events == 0)
                continue;

            if (poll_fds[i].revents & POLLHUP) {
                // Any data still available at this point will be processed by
                // a call to the flush method.
                poll_fds[i].events = 0;

                assert(nactive >= 1);
                nactive--;
            } else if (poll_fds[i].revents & (POLLIN | POLLRDNORM | POLLRDBAND |
                                       POLLPRI)) {
                (void)read_one(i, poll_fds[i].fd, m_buffers[i], true);
            }
        }
    }
}

void
impl::muxer::flush(void)
{
    for (size_t i = 0; i < m_nfds; i++) {
        while (read_one(i, m_fds[i], m_buffers[i], false) > 0) {}

        if (!m_buffers[i].empty())
            line_callback(i, m_buffers[i]);
    }
}
