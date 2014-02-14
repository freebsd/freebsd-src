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
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>
}

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <istream>
#include <ostream>

#include <atf-c++.hpp>

#include "io.hpp"
#include "signals.hpp"

// ------------------------------------------------------------------------
// Auxiliary functions.
// ------------------------------------------------------------------------

static
void
systembuf_check_data(std::istream& is, std::size_t length)
{
    char ch = 'A', chr;
    std::size_t cnt = 0;
    while (is >> chr) {
        ATF_REQUIRE_EQ(ch, chr);
        if (ch == 'Z')
            ch = 'A';
        else
            ch++;
        cnt++;
    }
    ATF_REQUIRE_EQ(cnt, length);
}

static
void
systembuf_write_data(std::ostream& os, std::size_t length)
{
    char ch = 'A';
    for (std::size_t i = 0; i < length; i++) {
        os << ch;
        if (ch == 'Z')
            ch = 'A';
        else
            ch++;
    }
    os.flush();
}

static
void
systembuf_test_read(std::size_t length, std::size_t bufsize)
{
    using tools::io::systembuf;

    std::ofstream f("test_read.txt");
    systembuf_write_data(f, length);
    f.close();

    int fd = ::open("test_read.txt", O_RDONLY);
    ATF_REQUIRE(fd != -1);
    systembuf sb(fd, bufsize);
    std::istream is(&sb);
    systembuf_check_data(is, length);
    ::close(fd);
    ::unlink("test_read.txt");
}

static
void
systembuf_test_write(std::size_t length, std::size_t bufsize)
{
    using tools::io::systembuf;

    int fd = ::open("test_write.txt", O_WRONLY | O_CREAT | O_TRUNC,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    ATF_REQUIRE(fd != -1);
    systembuf sb(fd, bufsize);
    std::ostream os(&sb);
    systembuf_write_data(os, length);
    ::close(fd);

    std::ifstream is("test_write.txt");
    systembuf_check_data(is, length);
    is.close();
    ::unlink("test_write.txt");
}

// ------------------------------------------------------------------------
// Test cases for the "file_handle" class.
// ------------------------------------------------------------------------

ATF_TEST_CASE(file_handle_ctor);
ATF_TEST_CASE_HEAD(file_handle_ctor)
{
    set_md_var("descr", "Tests file_handle's constructors");
}
ATF_TEST_CASE_BODY(file_handle_ctor)
{
    using tools::io::file_handle;

    file_handle fh1;
    ATF_REQUIRE(!fh1.is_valid());

    file_handle fh2(STDOUT_FILENO);
    ATF_REQUIRE(fh2.is_valid());
    fh2.disown();
}

ATF_TEST_CASE(file_handle_copy);
ATF_TEST_CASE_HEAD(file_handle_copy)
{
    set_md_var("descr", "Tests file_handle's copy constructor");
}
ATF_TEST_CASE_BODY(file_handle_copy)
{
    using tools::io::file_handle;

    file_handle fh1;
    file_handle fh2(STDOUT_FILENO);

    file_handle fh3(fh2);
    ATF_REQUIRE(!fh2.is_valid());
    ATF_REQUIRE(fh3.is_valid());

    fh1 = fh3;
    ATF_REQUIRE(!fh3.is_valid());
    ATF_REQUIRE(fh1.is_valid());

    fh1.disown();
}

ATF_TEST_CASE(file_handle_get);
ATF_TEST_CASE_HEAD(file_handle_get)
{
    set_md_var("descr", "Tests the file_handle::get method");
}
ATF_TEST_CASE_BODY(file_handle_get)
{
    using tools::io::file_handle;

    file_handle fh1(STDOUT_FILENO);
    ATF_REQUIRE_EQ(fh1.get(), STDOUT_FILENO);
}

ATF_TEST_CASE(file_handle_posix_remap);
ATF_TEST_CASE_HEAD(file_handle_posix_remap)
{
    set_md_var("descr", "Tests the file_handle::posix_remap method");
}
ATF_TEST_CASE_BODY(file_handle_posix_remap)
{
    using tools::io::file_handle;

    int pfd[2];

    ATF_REQUIRE(::pipe(pfd) != -1);
    file_handle rend(pfd[0]);
    file_handle wend(pfd[1]);

    ATF_REQUIRE(rend.get() != 10);
    ATF_REQUIRE(wend.get() != 10);
    wend.posix_remap(10);
    ATF_REQUIRE_EQ(wend.get(), 10);
    ATF_REQUIRE(::write(wend.get(), "test-posix-remap", 16) != -1);
    {
        char buf[17];
        ATF_REQUIRE_EQ(::read(rend.get(), buf, sizeof(buf)), 16);
        buf[16] = '\0';
        ATF_REQUIRE(std::strcmp(buf, "test-posix-remap") == 0);
    }

    // Redo previous to ensure that remapping over the same descriptor
    // has no side-effects.
    ATF_REQUIRE_EQ(wend.get(), 10);
    wend.posix_remap(10);
    ATF_REQUIRE_EQ(wend.get(), 10);
    ATF_REQUIRE(::write(wend.get(), "test-posix-remap", 16) != -1);
    {
        char buf[17];
        ATF_REQUIRE_EQ(::read(rend.get(), buf, sizeof(buf)), 16);
        buf[16] = '\0';
        ATF_REQUIRE(std::strcmp(buf, "test-posix-remap") == 0);
    }
}

// ------------------------------------------------------------------------
// Test cases for the "systembuf" class.
// ------------------------------------------------------------------------

ATF_TEST_CASE(systembuf_short_read);
ATF_TEST_CASE_HEAD(systembuf_short_read)
{
    set_md_var("descr", "Tests that a short read (one that fits in the "
               "internal buffer) works when using systembuf");
}
ATF_TEST_CASE_BODY(systembuf_short_read)
{
    systembuf_test_read(64, 1024);
}

ATF_TEST_CASE(systembuf_long_read);
ATF_TEST_CASE_HEAD(systembuf_long_read)
{
    set_md_var("descr", "Tests that a long read (one that does not fit in "
               "the internal buffer) works when using systembuf");
}
ATF_TEST_CASE_BODY(systembuf_long_read)
{
    systembuf_test_read(64 * 1024, 1024);
}

ATF_TEST_CASE(systembuf_short_write);
ATF_TEST_CASE_HEAD(systembuf_short_write)
{
    set_md_var("descr", "Tests that a short write (one that fits in the "
               "internal buffer) works when using systembuf");
}
ATF_TEST_CASE_BODY(systembuf_short_write)
{
    systembuf_test_write(64, 1024);
}

ATF_TEST_CASE(systembuf_long_write);
ATF_TEST_CASE_HEAD(systembuf_long_write)
{
    set_md_var("descr", "Tests that a long write (one that does not fit "
               "in the internal buffer) works when using systembuf");
}
ATF_TEST_CASE_BODY(systembuf_long_write)
{
    systembuf_test_write(64 * 1024, 1024);
}

// ------------------------------------------------------------------------
// Test cases for the "pistream" class.
// ------------------------------------------------------------------------

ATF_TEST_CASE(pistream);
ATF_TEST_CASE_HEAD(pistream)
{
    set_md_var("descr", "Tests the pistream class");
}
ATF_TEST_CASE_BODY(pistream)
{
    using tools::io::file_handle;
    using tools::io::pistream;
    using tools::io::systembuf;

    int fds[2];
    ATF_REQUIRE(::pipe(fds) != -1);

    pistream rend(fds[0]);

    systembuf wbuf(fds[1]);
    std::ostream wend(&wbuf);

    // XXX This assumes that the pipe's buffer is big enough to accept
    // the data written without blocking!
    wend << "1Test 1message\n";
    wend.flush();
    std::string tmp;
    rend >> tmp;
    ATF_REQUIRE_EQ(tmp, "1Test");
    rend >> tmp;
    ATF_REQUIRE_EQ(tmp, "1message");
}

// ------------------------------------------------------------------------
// Tests for the "muxer" class.
// ------------------------------------------------------------------------

namespace {

static void
check_stream(std::ostream& os)
{
    // If we receive a signal while writing to the stream, the bad bit gets set.
    // Things seem to behave fine afterwards if we clear such error condition.
    // However, I'm not sure if it's safe to query errno at this point.
    ATF_REQUIRE(os.good() || (os.bad() && errno == EINTR));
    os.clear();
}

class mock_muxer : public tools::io::muxer {
    void line_callback(const size_t index, const std::string& line)
    {
        // The following should be enabled but causes the output to be so big
        // that it is annoying.  Reenable at some point if we make atf store
        // the output of the test cases in some other way (e.g. only if a test
        // failes), because this message is the only help in seeing how the
        // test fails.
        //std::cout << "line_callback(" << index << ", " << line << ")\n";
        check_stream(std::cout);
        switch (index) {
        case 0: lines0.push_back(line); break;
        case 1: lines1.push_back(line); break;
        default: ATF_REQUIRE(false);
        }
    }

public:
    mock_muxer(const int* fds, const size_t nfds, const size_t bufsize) :
        muxer(fds, nfds, bufsize) {}

    std::vector< std::string > lines0;
    std::vector< std::string > lines1;
};

static bool child_finished = false;
static void sigchld_handler(int signo)
{
    assert(signo == SIGCHLD);
    child_finished = true;
}

static void
child_printer(const int pipeout[2], const int pipeerr[2],
              const size_t iterations)
{
    ::close(pipeout[0]);
    ::close(pipeerr[0]);
    ATF_REQUIRE(::dup2(pipeout[1], STDOUT_FILENO) != -1);
    ATF_REQUIRE(::dup2(pipeerr[1], STDERR_FILENO) != -1);
    ::close(pipeout[1]);
    ::close(pipeerr[1]);

    for (size_t i = 0; i < iterations; i++) {
        std::cout << "stdout " << i << "\n";
        std::cerr << "stderr " << i << "\n";
    }

    std::cout << "stdout eof\n";
    std::cerr << "stderr eof\n";
    std::exit(EXIT_SUCCESS);
}

static void
muxer_test(const size_t bufsize, const size_t iterations)
{
    int pipeout[2], pipeerr[2];
    ATF_REQUIRE(pipe(pipeout) != -1);
    ATF_REQUIRE(pipe(pipeerr) != -1);

    tools::signals::signal_programmer sigchld(SIGCHLD, sigchld_handler);

    std::cout.flush();
    std::cerr.flush();

    pid_t pid = ::fork();
    ATF_REQUIRE(pid != -1);
    if (pid == 0) {
        sigchld.unprogram();
        child_printer(pipeout, pipeerr, iterations);
        std::abort();
    }
    ::close(pipeout[1]);
    ::close(pipeerr[1]);

    int fds[2] = {pipeout[0], pipeerr[0]};
    mock_muxer mux(fds, 2, bufsize);

    mux.mux(child_finished);
    check_stream(std::cout);
    std::cout << "mux done\n";

    mux.flush();
    std::cout << "flush done\n";
    check_stream(std::cout);

    sigchld.unprogram();
    int status;
    ATF_REQUIRE(::waitpid(pid, &status, 0) != -1);
    ATF_REQUIRE(WIFEXITED(status));
    ATF_REQUIRE(WEXITSTATUS(status) == EXIT_SUCCESS);

    ATF_REQUIRE(std::cout.good());
    ATF_REQUIRE(std::cerr.good());
    for (size_t i = 0; i < iterations; i++) {
        std::ostringstream exp0, exp1;
        exp0 << "stdout " << i;
        exp1 << "stderr " << i;

        ATF_REQUIRE(mux.lines0.size() > i);
        ATF_REQUIRE_EQ(exp0.str(), mux.lines0[i]);
        ATF_REQUIRE(mux.lines1.size() > i);
        ATF_REQUIRE_EQ(exp1.str(), mux.lines1[i]);
    }
    ATF_REQUIRE_EQ("stdout eof", mux.lines0[iterations]);
    ATF_REQUIRE_EQ("stderr eof", mux.lines1[iterations]);
    std::cout << "all done\n";
}

} // anonymous namespace

ATF_TEST_CASE_WITHOUT_HEAD(muxer_small_buffer);
ATF_TEST_CASE_BODY(muxer_small_buffer)
{
    muxer_test(4, 20000);
}

ATF_TEST_CASE_WITHOUT_HEAD(muxer_large_buffer);
ATF_TEST_CASE_BODY(muxer_large_buffer)
{
    muxer_test(1024, 50000);
}

// ------------------------------------------------------------------------
// Main.
// ------------------------------------------------------------------------

ATF_INIT_TEST_CASES(tcs)
{
    // Add the tests for the "file_handle" class.
    ATF_ADD_TEST_CASE(tcs, file_handle_ctor);
    ATF_ADD_TEST_CASE(tcs, file_handle_copy);
    ATF_ADD_TEST_CASE(tcs, file_handle_get);
    ATF_ADD_TEST_CASE(tcs, file_handle_posix_remap);

    // Add the tests for the "systembuf" class.
    ATF_ADD_TEST_CASE(tcs, systembuf_short_read);
    ATF_ADD_TEST_CASE(tcs, systembuf_long_read);
    ATF_ADD_TEST_CASE(tcs, systembuf_short_write);
    ATF_ADD_TEST_CASE(tcs, systembuf_long_write);

    // Add the tests for the "pistream" class.
    ATF_ADD_TEST_CASE(tcs, pistream);

    // Add the tests for the "muxer" class.
    ATF_ADD_TEST_CASE(tcs, muxer_small_buffer);
    ATF_ADD_TEST_CASE(tcs, muxer_large_buffer);
}
