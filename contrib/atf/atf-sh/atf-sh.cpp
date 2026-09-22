// Copyright (c) 2010 The NetBSD Foundation, Inc.
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

extern "C" {
#include <unistd.h>
}

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <utility>
#include <vector>

#include "atf-c++/detail/application.hpp"
#include "atf-c++/detail/env.hpp"
#include "atf-c++/detail/fs.hpp"
#include "atf-c++/detail/sanity.hpp"

// ------------------------------------------------------------------------
// Auxiliary functions.
// ------------------------------------------------------------------------

namespace {

static
std::string
fix_plain_name(const char *filename)
{
    const atf::fs::path filepath(filename);
    if (filepath.branch_path().str() == ".")
        return std::string("./") + filename;
    else
        return std::string(filename);
}

static
std::string
construct_script(const char* filename)
{
    const std::string libexecdir = atf::env::get(
        "ATF_LIBEXECDIR", ATF_LIBEXECDIR);
    const std::string pkgdatadir = atf::env::get(
        "ATF_PKGDATADIR", ATF_PKGDATADIR);
    const std::string shell = atf::env::get("ATF_SHELL", ATF_SHELL);

    std::string command;

    command += ("Atf_Check='" + libexecdir + "/atf-check' ; " +
                "Atf_Shell='" + shell + "' ; " +
                ". " + pkgdatadir + "/libatf-sh.subr ; " +
                ". " + fix_plain_name(filename) + " ; " +
                "main \"${@}\"");
    return command;
}

static
std::vector<std::string>
construct_argv(const std::string& shell, const int interpreter_argc,
               const char* const* interpreter_argv)
{
    PRE(interpreter_argc >= 1);
    PRE(interpreter_argv[0] != NULL);

    std::string script = construct_script(interpreter_argv[0]);

    std::vector<std::string> argv;

    argv.push_back(shell);
    argv.push_back(std::string("-c"));
    argv.push_back(std::move(script));
    argv.push_back(std::string(interpreter_argv[0]));

    for (int i = 1; i < interpreter_argc; i++)
        argv.push_back(interpreter_argv[i]);

    return argv;
}

} // anonymous namespace

// ------------------------------------------------------------------------
// The "atf_sh" class.
// ------------------------------------------------------------------------

class atf_sh : public atf::application::app {
    static const char* m_description;

    atf::fs::path m_shell;

    options_set specific_options(void) const;
    void process_option(int, const char*);

public:
    atf_sh(void);

    int main(void);
};

const char* atf_sh::m_description =
    "atf-sh is a shell interpreter that extends the functionality of the "
    "system sh(1) with the atf-sh library.";

atf_sh::atf_sh(void) :
    app(m_description, "atf-sh(1)"),
    m_shell(atf::fs::path(atf::env::get("ATF_SHELL", ATF_SHELL)))
{
}

atf_sh::options_set
atf_sh::specific_options(void)
    const
{
    using atf::application::option;
    options_set opts;

    INV(m_shell == atf::fs::path(atf::env::get("ATF_SHELL", ATF_SHELL)));
    opts.insert(option('s', "shell", "Path to the shell interpreter to use; "
                       "default: " + m_shell.str()));

    return opts;
}

void
atf_sh::process_option(int ch, const char* arg)
{
    switch (ch) {
    case 's':
        m_shell = atf::fs::path(arg);
        break;

    default:
        UNREACHABLE;
    }
}

int
atf_sh::main(void)
{
    if (m_argc < 1)
        throw atf::application::usage_error("No test program provided");

    const atf::fs::path script(m_argv[0]);
    if (!atf::fs::exists(script))
        throw std::runtime_error("The test program '" + script.str() + "' "
                                 "does not exist");

    std::vector<std::string> argvv = construct_argv(m_shell.str(), m_argc,
        m_argv);
    std::vector<const char *> argv;

    for (std::size_t i = 0; i < argvv.size(); i++)
        argv.push_back(argvv[i].c_str());
    argv.push_back(NULL);

    const int ret = execv(m_shell.c_str(),
        const_cast<char* const *>(argv.data()));
    INV(ret == -1);

    std::cerr << "Failed to execute " << m_shell.str() << ": "
              << std::strerror(errno) << "\n";
    return EXIT_FAILURE;
}

int
main(int argc, char* const* argv)
{
    return atf_sh().run(argc, argv);
}
