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

#include <cstdlib>
#include <iostream>

#include "tools/application.hpp"

class h_app_opts_args : public tools::application::app {
    static const char* m_description;

    std::string specific_args(void) const;
    options_set specific_options(void) const;
    void process_option(int, const char*);

public:
    h_app_opts_args(void);

    int main(void);
};

const char* h_app_opts_args::m_description =
    "A helper application for the bootstrap test suite that redefines the "
    "methods to specify custom options and arguments.";

h_app_opts_args::h_app_opts_args(void) :
    app(m_description, "h_app_opts_args(1)", "atf(7)")
{
}

std::string
h_app_opts_args::specific_args(void)
    const
{
    return "<arg1> <arg2>";
}

h_app_opts_args::options_set
h_app_opts_args::specific_options(void)
    const
{
    using tools::application::option;
    options_set opts;
    opts.insert(option('d', "", "Debug mode"));
    opts.insert(option('v', "level", "Verbosity level"));
    return opts;
}

void
h_app_opts_args::process_option(int ch, const char* arg)
{
    switch (ch) {
    case 'd':
        std::cout << "-d given\n";
        break;

    case 'v':
        std::cout << "-v given with argument " << arg << "\n";
        break;

    default:
        std::abort();
    }
}

int
h_app_opts_args::main(void)
{
    return EXIT_SUCCESS;
}

int
main(int argc, char* const* argv)
{
    return h_app_opts_args().run(argc, argv);
}
