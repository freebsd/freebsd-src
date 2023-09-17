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

extern "C" {
#include <unistd.h>
}

#include <atf-c++.hpp>

#include "utils/process/systembuf.hpp"

using utils::process::ifdstream;
using utils::process::systembuf;


ATF_TEST_CASE(ifdstream);
ATF_TEST_CASE_HEAD(ifdstream)
{
    set_md_var("descr", "Tests the ifdstream class");
}
ATF_TEST_CASE_BODY(ifdstream)
{
    int fds[2];
    ATF_REQUIRE(::pipe(fds) != -1);

    ifdstream rend(fds[0]);

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


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, ifdstream);
}
