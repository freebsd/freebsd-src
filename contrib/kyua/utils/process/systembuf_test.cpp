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
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>
}

#include <fstream>

#include <atf-c++.hpp>

using utils::process::systembuf;


static void
check_data(std::istream& is, std::size_t length)
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


static void
write_data(std::ostream& os, std::size_t length)
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


static void
test_read(std::size_t length, std::size_t bufsize)
{
    std::ofstream f("test_read.txt");
    write_data(f, length);
    f.close();

    int fd = ::open("test_read.txt", O_RDONLY);
    ATF_REQUIRE(fd != -1);
    systembuf sb(fd, bufsize);
    std::istream is(&sb);
    check_data(is, length);
    ::close(fd);
    ::unlink("test_read.txt");
}


static void
test_write(std::size_t length, std::size_t bufsize)
{
    int fd = ::open("test_write.txt", O_WRONLY | O_CREAT | O_TRUNC,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    ATF_REQUIRE(fd != -1);
    systembuf sb(fd, bufsize);
    std::ostream os(&sb);
    write_data(os, length);
    ::close(fd);

    std::ifstream is("test_write.txt");
    check_data(is, length);
    is.close();
    ::unlink("test_write.txt");
}


ATF_TEST_CASE(short_read);
ATF_TEST_CASE_HEAD(short_read)
{
    set_md_var("descr", "Tests that a short read (one that fits in the "
               "internal buffer) works when using systembuf");
}
ATF_TEST_CASE_BODY(short_read)
{
    test_read(64, 1024);
}


ATF_TEST_CASE(long_read);
ATF_TEST_CASE_HEAD(long_read)
{
    set_md_var("descr", "Tests that a long read (one that does not fit in "
               "the internal buffer) works when using systembuf");
}
ATF_TEST_CASE_BODY(long_read)
{
    test_read(64 * 1024, 1024);
}


ATF_TEST_CASE(short_write);
ATF_TEST_CASE_HEAD(short_write)
{
    set_md_var("descr", "Tests that a short write (one that fits in the "
               "internal buffer) works when using systembuf");
}
ATF_TEST_CASE_BODY(short_write)
{
    test_write(64, 1024);
}


ATF_TEST_CASE(long_write);
ATF_TEST_CASE_HEAD(long_write)
{
    set_md_var("descr", "Tests that a long write (one that does not fit "
               "in the internal buffer) works when using systembuf");
}
ATF_TEST_CASE_BODY(long_write)
{
    test_write(64 * 1024, 1024);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, short_read);
    ATF_ADD_TEST_CASE(tcs, long_read);
    ATF_ADD_TEST_CASE(tcs, short_write);
    ATF_ADD_TEST_CASE(tcs, long_write);
}
