//
// Automated Testing Framework (atf)
//
// Copyright (c) 2008 The NetBSD Foundation, Inc.
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
#include <sys/types.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <signal.h>
}

#include <cassert>
#include <cstdarg>
#include <cerrno>
#include <cstring>
#include <iostream>

#include "defs.hpp"
#include "exceptions.hpp"
#include "text.hpp"
#include "process.hpp"

namespace detail = tools::process::detail;
namespace impl = tools::process;
#define IMPL_NAME "tools::process"

// ------------------------------------------------------------------------
// Auxiliary functions.
// ------------------------------------------------------------------------

template< class C >
tools::auto_array< const char* >
collection_to_argv(const C& c)
{
    tools::auto_array< const char* > argv(new const char*[c.size() + 1]);

    std::size_t pos = 0;
    for (typename C::const_iterator iter = c.begin(); iter != c.end();
         iter++) {
        argv[pos] = (*iter).c_str();
        pos++;
    }
    assert(pos == c.size());
    argv[pos] = NULL;

    return argv;
}

template< class C >
C
argv_to_collection(const char* const* argv)
{
    C c;

    for (const char* const* iter = argv; *iter != NULL; iter++)
        c.push_back(std::string(*iter));

    return c;
}

static
void
safe_dup(const int oldfd, const int newfd)
{
    if (oldfd != newfd) {
        if (dup2(oldfd, newfd) == -1) {
            throw tools::system_error(IMPL_NAME "::safe_dup",
                                      "Could not allocate file descriptor",
                                      errno);
        } else {
            ::close(oldfd);
        }
    }
}

static
int
const_execvp(const char *file, const char *const *argv)
{
#define UNCONST(a) ((void *)(unsigned long)(const void *)(a))
    return ::execvp(file, (char* const*)(UNCONST(argv)));
#undef UNCONST
}

void
detail::do_exec(void *v)
{
    struct exec_args *ea = reinterpret_cast<struct exec_args *>(v);

    if (ea->m_prehook != NULL)
        ea->m_prehook();

    const int ret = const_execvp(ea->m_prog.c_str(), ea->m_argv.exec_argv());
    const int errnocopy = errno;
    assert(ret == -1);
    std::cerr << "exec(" << ea->m_prog.str() << ") failed: "
              << std::strerror(errnocopy) << "\n";
    std::exit(EXIT_FAILURE);
}

// ------------------------------------------------------------------------
// The "argv_array" type.
// ------------------------------------------------------------------------

impl::argv_array::argv_array(void) :
    m_exec_argv(collection_to_argv(m_args))
{
}

impl::argv_array::argv_array(const char* arg1, ...)
{
    m_args.push_back(arg1);

    {
        va_list ap;
        const char* nextarg;

        va_start(ap, arg1);
        while ((nextarg = va_arg(ap, const char*)) != NULL)
            m_args.push_back(nextarg);
        va_end(ap);
    }

    ctor_init_exec_argv();
}

impl::argv_array::argv_array(const char* const* ca) :
    m_args(argv_to_collection< args_vector >(ca)),
    m_exec_argv(collection_to_argv(m_args))
{
}

impl::argv_array::argv_array(const argv_array& a) :
    m_args(a.m_args),
    m_exec_argv(collection_to_argv(m_args))
{
}

void
impl::argv_array::ctor_init_exec_argv(void)
{
    m_exec_argv = collection_to_argv(m_args);
}

const char* const*
impl::argv_array::exec_argv(void)
    const
{
    return m_exec_argv.get();
}

impl::argv_array::size_type
impl::argv_array::size(void)
    const
{
    return m_args.size();
}

const char*
impl::argv_array::operator[](int idx)
    const
{
    return m_args[idx].c_str();
}

impl::argv_array::const_iterator
impl::argv_array::begin(void)
    const
{
    return m_args.begin();
}

impl::argv_array::const_iterator
impl::argv_array::end(void)
    const
{
    return m_args.end();
}

impl::argv_array&
impl::argv_array::operator=(const argv_array& a)
{
    if (this != &a) {
        m_args = a.m_args;
        m_exec_argv = collection_to_argv(m_args);
    }
    return *this;
}

// ------------------------------------------------------------------------
// The "stream" types.
// ------------------------------------------------------------------------

impl::stream_capture::stream_capture(void)
{
    for (int i = 0; i < 2; i++)
        m_pipefds[i] = -1;
}

impl::stream_capture::~stream_capture(void)
{
    for (int i = 0; i < 2; i++)
        if (m_pipefds[i] != -1)
            ::close(m_pipefds[i]);
}

void
impl::stream_capture::prepare(void)
{
    if (pipe(m_pipefds) == -1)
        throw system_error(IMPL_NAME "::stream_capture::prepare",
                           "Failed to create pipe", errno);
}

int
impl::stream_capture::connect_parent(void)
{
    ::close(m_pipefds[1]); m_pipefds[1] = -1;
    const int fd = m_pipefds[0];
    m_pipefds[0] = -1;
    return fd;
}

void
impl::stream_capture::connect_child(const int fd)
{
    ::close(m_pipefds[0]); m_pipefds[0] = -1;
    if (m_pipefds[1] != fd) {
        safe_dup(m_pipefds[1], fd);
    }
    m_pipefds[1] = -1;
}

impl::stream_connect::stream_connect(const int src_fd, const int tgt_fd) :
    m_src_fd(src_fd), m_tgt_fd(tgt_fd)
{
}

void
impl::stream_connect::prepare(void)
{
}

int
impl::stream_connect::connect_parent(void)
{
    return -1;
}

void
impl::stream_connect::connect_child(const int fd ATF_DEFS_ATTRIBUTE_UNUSED)
{
    safe_dup(m_tgt_fd, m_src_fd);
}

impl::stream_inherit::stream_inherit(void)
{
}

void
impl::stream_inherit::prepare(void)
{
}

int
impl::stream_inherit::connect_parent(void)
{
    return -1;
}

void
impl::stream_inherit::connect_child(const int fd ATF_DEFS_ATTRIBUTE_UNUSED)
{
}

impl::stream_redirect_fd::stream_redirect_fd(const int fd) :
    m_fd(fd)
{
}

void
impl::stream_redirect_fd::prepare(void)
{
}

int
impl::stream_redirect_fd::connect_parent(void)
{
    return -1;
}

void
impl::stream_redirect_fd::connect_child(const int fd)
{
    safe_dup(m_fd, fd);
}

impl::stream_redirect_path::stream_redirect_path(const tools::fs::path& p) :
    m_path(p)
{
}

void
impl::stream_redirect_path::prepare(void)
{
}

int
impl::stream_redirect_path::connect_parent(void)
{
    return -1;
}

void
impl::stream_redirect_path::connect_child(const int fd)
{
    const int aux = ::open(m_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (aux == -1)
        throw system_error(IMPL_NAME "::stream_redirect_path::connect_child",
                           "Could not create " + m_path.str(), errno);
    else
        safe_dup(aux, fd);
}

// ------------------------------------------------------------------------
// The "status" type.
// ------------------------------------------------------------------------

impl::status::status(int s) :
    m_status(s)
{
}

impl::status::~status(void)
{
}

bool
impl::status::exited(void)
    const
{
    int mutable_status = m_status;
    return WIFEXITED(mutable_status);
}

int
impl::status::exitstatus(void)
    const
{
    assert(exited());
    int mutable_status = m_status;
    return WEXITSTATUS(mutable_status);
}

bool
impl::status::signaled(void)
    const
{
    int mutable_status = m_status;
    return WIFSIGNALED(mutable_status);
}

int
impl::status::termsig(void)
    const
{
    assert(signaled());
    int mutable_status = m_status;
    return WTERMSIG(mutable_status);
}

bool
impl::status::coredump(void)
    const
{
    assert(signaled());
#if defined(WCOREDUMP)
    int mutable_status = m_status;
    return WCOREDUMP(mutable_status);
#else
    return false;
#endif
}

// ------------------------------------------------------------------------
// The "child" type.
// ------------------------------------------------------------------------

impl::child::child(const pid_t pid_arg, const int stdout_fd_arg,
                   const int stderr_fd_arg) :
    m_pid(pid_arg),
    m_stdout(stdout_fd_arg),
    m_stderr(stderr_fd_arg),
    m_waited(false)
{
}

impl::child::~child(void)
{
    if (!m_waited) {
        ::kill(m_pid, SIGTERM);
        (void)wait();

        if (m_stdout != -1)
            ::close(m_stdout);
        if (m_stderr != -1)
            ::close(m_stderr);
    }
}

impl::status
impl::child::wait(void)
{
    int s;

    if (::waitpid(m_pid, &s, 0) == -1)
        throw system_error(IMPL_NAME "::child::wait", "Failed waiting for "
                           "process " + text::to_string(m_pid), errno);

    if (m_stdout != -1)
        ::close(m_stdout); m_stdout = -1;
    if (m_stderr != -1)
        ::close(m_stderr); m_stderr = -1;

    m_waited = true;
    return status(s);
}

pid_t
impl::child::pid(void)
    const
{
    return m_pid;
}

int
impl::child::stdout_fd(void)
{
    return m_stdout;
}

int
impl::child::stderr_fd(void)
{
    return m_stderr;
}

// ------------------------------------------------------------------------
// Free functions.
// ------------------------------------------------------------------------

void
detail::flush_streams(void)
{
    // This is a weird hack to ensure that the output of the parent process
    // is flushed before executing a child which prevents, for example, the
    // output of the atf-run hooks to appear before the output of atf-run
    // itself.
    //
    // TODO: This should only be executed when inheriting the stdout or
    // stderr file descriptors.  However, the flushing is specific to the
    // iostreams, so we cannot do it from the C library where all the process
    // logic is performed.  Come up with a better design.
    std::cout.flush();
    std::cerr.flush();
}
