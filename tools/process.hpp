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

#if !defined(TOOLS_PROCESS_HPP)
#define TOOLS_PROCESS_HPP

extern "C" {
#include <sys/types.h>

#include <unistd.h>
}

#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "auto_array.hpp"
#include "exceptions.hpp"
#include "fs.hpp"

namespace tools {
namespace process {

class child;
class status;

// ------------------------------------------------------------------------
// The "argv_array" type.
// ------------------------------------------------------------------------

class argv_array {
    typedef std::vector< std::string > args_vector;
    args_vector m_args;

    // TODO: This is immutable, so we should be able to use
    // std::tr1::shared_array instead when it becomes widely available.
    // The reason would be to remove all copy constructors and assignment
    // operators from this class.
    auto_array< const char* > m_exec_argv;
    void ctor_init_exec_argv(void);

public:
    typedef args_vector::const_iterator const_iterator;
    typedef args_vector::size_type size_type;

    argv_array(void);
    argv_array(const char*, ...);
    explicit argv_array(const char* const*);
    template< class C > explicit argv_array(const C&);
    argv_array(const argv_array&);

    const char* const* exec_argv(void) const;
    size_type size(void) const;
    const char* operator[](int) const;

    const_iterator begin(void) const;
    const_iterator end(void) const;

    argv_array& operator=(const argv_array&);
};

template< class C >
argv_array::argv_array(const C& c)
{
    for (typename C::const_iterator iter = c.begin(); iter != c.end();
         iter++)
        m_args.push_back(*iter);
    ctor_init_exec_argv();
}

// ------------------------------------------------------------------------
// The "stream" types.
// ------------------------------------------------------------------------

class stream_capture {
    int m_pipefds[2];

    // Allow access to the getters.
    template< class OutStream, class ErrStream > friend
    child fork(void (*)(void*), OutStream, ErrStream, void*);
    template< class OutStream, class ErrStream > friend
    status exec(const tools::fs::path&, const argv_array&,
                const OutStream&, const ErrStream&, void (*)(void));

    void prepare(void);
    int connect_parent(void);
    void connect_child(const int);

public:
    stream_capture(void);
    ~stream_capture(void);
};

class stream_connect {
    int m_src_fd;
    int m_tgt_fd;

    // Allow access to the getters.
    template< class OutStream, class ErrStream > friend
    child fork(void (*)(void*), OutStream, ErrStream, void*);
    template< class OutStream, class ErrStream > friend
    status exec(const tools::fs::path&, const argv_array&,
                const OutStream&, const ErrStream&, void (*)(void));

    void prepare(void);
    int connect_parent(void);
    void connect_child(const int);

public:
    stream_connect(const int, const int);
};

class stream_inherit {
    // Allow access to the getters.
    template< class OutStream, class ErrStream > friend
    child fork(void (*)(void*), OutStream, ErrStream, void*);
    template< class OutStream, class ErrStream > friend
    status exec(const tools::fs::path&, const argv_array&,
                const OutStream&, const ErrStream&, void (*)(void));

    void prepare(void);
    int connect_parent(void);
    void connect_child(const int);

public:
    stream_inherit(void);
};

class stream_redirect_fd {
    int m_fd;

    // Allow access to the getters.
    template< class OutStream, class ErrStream > friend
    child fork(void (*)(void*), OutStream, ErrStream, void*);
    template< class OutStream, class ErrStream > friend
    status exec(const tools::fs::path&, const argv_array&,
                const OutStream&, const ErrStream&, void (*)(void));

    void prepare(void);
    int connect_parent(void);
    void connect_child(const int);

public:
    stream_redirect_fd(const int);
};

class stream_redirect_path {
    const tools::fs::path m_path;

    // Allow access to the getters.
    template< class OutStream, class ErrStream > friend
    child fork(void (*)(void*), OutStream, ErrStream, void*);
    template< class OutStream, class ErrStream > friend
    status exec(const tools::fs::path&, const argv_array&,
                const OutStream&, const ErrStream&, void (*)(void));

    void prepare(void);
    int connect_parent(void);
    void connect_child(const int);

public:
    stream_redirect_path(const tools::fs::path&);
};

// ------------------------------------------------------------------------
// The "status" type.
// ------------------------------------------------------------------------

class status {
    int m_status;

    friend class child;
    template< class OutStream, class ErrStream > friend
    status exec(const tools::fs::path&, const argv_array&,
                const OutStream&, const ErrStream&, void (*)(void));

    status(int);

public:
    ~status(void);

    bool exited(void) const;
    int exitstatus(void) const;

    bool signaled(void) const;
    int termsig(void) const;
    bool coredump(void) const;
};

// ------------------------------------------------------------------------
// The "child" type.
// ------------------------------------------------------------------------

class child {
    pid_t m_pid;

    int m_stdout;
    int m_stderr;

    bool m_waited;

    template< class OutStream, class ErrStream > friend
    child fork(void (*)(void*), OutStream, ErrStream, void*);

    child(const pid_t, const int, const int);

public:
    ~child(void);

    status wait(void);

    pid_t pid(void) const;
    int stdout_fd(void);
    int stderr_fd(void);
};

// ------------------------------------------------------------------------
// Free functions.
// ------------------------------------------------------------------------

namespace detail {
void flush_streams(void);

struct exec_args {
    const tools::fs::path m_prog;
    const argv_array& m_argv;
    void (*m_prehook)(void);
};

void do_exec(void *);
} // namespace detail

// TODO: The void* cookie can probably be templatized, thus also allowing
// const data structures.
template< class OutStream, class ErrStream >
child
fork(void (*start)(void*), OutStream outsb, ErrStream errsb, void* v)
{
    detail::flush_streams();

    outsb.prepare();
    errsb.prepare();

    pid_t pid = ::fork();
    if (pid == -1) {
        throw system_error("tools::process::child::fork",
                           "Failed to fork", errno);
    } else if (pid == 0) {
        try {
            outsb.connect_child(STDOUT_FILENO);
            errsb.connect_child(STDERR_FILENO);
            start(v);
            std::abort();
        } catch (...) {
            std::cerr << "Unhandled error while running subprocess\n";
            std::exit(EXIT_FAILURE);
        }
    } else {
        const int stdout_fd = outsb.connect_parent();
        const int stderr_fd = errsb.connect_parent();
        return child(pid, stdout_fd, stderr_fd);
    }
}

template< class OutStream, class ErrStream >
status
exec(const tools::fs::path& prog, const argv_array& argv,
     const OutStream& outsb, const ErrStream& errsb,
     void (*prehook)(void))
{
    struct detail::exec_args ea = { prog, argv, prehook };
    child c = fork(detail::do_exec, outsb, errsb, &ea);

again:
    try {
        return c.wait();
    } catch (const system_error& e) {
        if (e.code() == EINTR)
            goto again;
        else
            throw e;
    }
}

template< class OutStream, class ErrStream >
status
exec(const tools::fs::path& prog, const argv_array& argv,
     const OutStream& outsb, const ErrStream& errsb)
{
    return exec(prog, argv, outsb, errsb, NULL);
}

} // namespace process
} // namespace tools

#endif // !defined(TOOLS_PROCESS_HPP)
