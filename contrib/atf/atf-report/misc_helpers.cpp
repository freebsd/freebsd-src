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

#include <iostream>

#include "atf-c++/macros.hpp"

// ------------------------------------------------------------------------
// Helper tests for "t_integration".
// ------------------------------------------------------------------------

ATF_TEST_CASE(diff);
ATF_TEST_CASE_HEAD(diff)
{
    set_md_var("descr", "Helper test case for the t_integration program");
}
ATF_TEST_CASE_BODY(diff)
{
    std::cout << "--- a	2007-11-04 14:00:41.000000000 +0100\n";
    std::cout << "+++ b	2007-11-04 14:00:48.000000000 +0100\n";
    std::cout << "@@ -1,7 +1,7 @@\n";
    std::cout << " This test is meant to simulate a diff.\n";
    std::cout << " Blank space at beginning of context lines must be "
                 "preserved.\n";
    std::cout << " \n";
    std::cout << "-First original line.\n";
    std::cout << "-Second original line.\n";
    std::cout << "+First modified line.\n";
    std::cout << "+Second modified line.\n";
    std::cout << " \n";
    std::cout << " EOF\n";
}

// ------------------------------------------------------------------------
// Main.
// ------------------------------------------------------------------------

ATF_INIT_TEST_CASES(tcs)
{
    // Add helper tests for t_integration.
    ATF_ADD_TEST_CASE(tcs, diff);
}
