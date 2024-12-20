// Copyright 2024 The Kyua Authors.
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

#include "os/freebsd/utils/jail.hpp"

extern "C" {
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>

// FreeBSD sysctl facility
#include <sys/sysctl.h>

// FreeBSD Jail syscalls
#include <sys/param.h>
#include <sys/jail.h>

// FreeBSD Jail library
#include <jail.h>
}

#include <fstream>
#include <iostream>
#include <regex>

#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "utils/fs/path.hpp"
#include "utils/process/child.ipp"
#include "utils/format/macros.hpp"
#include "utils/process/operations.hpp"
#include "utils/process/status.hpp"

namespace process = utils::process;
namespace fs = utils::fs;

using utils::process::args_vector;
using utils::process::child;


static const size_t jail_name_max_len = MAXHOSTNAMELEN - 1;
static const char* jail_name_prefix = "kyua";


/// Functor to run a program.
class run {
    /// Program binary absolute path.
    const utils::fs::path& _program;

    /// Program arguments.
    const args_vector& _args;

public:
    /// Constructor.
    ///
    /// \param program Program binary absolute path.
    /// \param args Program arguments.
    run(
        const utils::fs::path& program,
        const args_vector& args) :
        _program(program),
        _args(args)
    {
    }

    /// Body of the subprocess.
    void
    operator()(void)
    {
        process::exec(_program, _args);
    }
};


namespace freebsd {
namespace utils {


std::vector< std::string >
jail::parse_params_string(const std::string& str)
{
    std::vector< std::string > params;
    std::string p;
    char quote = 0;

    std::istringstream iss(str);
    while (iss >> p) {
        if (p.front() == '"' || p.front() == '\'') {
            quote = p.front();
            p.erase(p.begin());
            if (p.find(quote) == std::string::npos) {
                std::string rest;
                std::getline(iss, rest, quote);
                p += rest;
                iss.ignore();
            }
            if (p.back() == quote)
                p.erase(p.end() - 1);
        }
        params.push_back(p);
    }

    return params;
}


/// Constructs a jail name based on program and test case.
///
/// The formula is "kyua" + <program path> + "_" + <test case name>.
/// All non-alphanumeric chars are replaced with "_".
///
/// If a resulting string exceeds maximum allowed length of a jail name,
/// then it's shortened from the left side keeping the "kyua" prefix.
///
/// \param program The test program.
/// \param test_case_name Name of the test case.
///
/// \return A jail name string.
std::string
jail::make_name(const fs::path& program,
                const std::string& test_case_name)
{
    std::string name = std::regex_replace(
        program.str() + "_" + test_case_name,
        std::regex(R"([^A-Za-z0-9_])"),
        "_");

    const std::string::size_type limit =
        jail_name_max_len - strlen(jail_name_prefix);
    if (name.length() > limit)
        name.erase(0, name.length() - limit);

    return jail_name_prefix + name;
}


/// Create a jail with a given name and params string.
///
/// A new jail will always be 'persist', thus the caller is expected to remove
/// the jail eventually via remove().
///
/// It's expected to be run in a subprocess.
///
/// \param jail_name Name of a new jail.
/// \param jail_params String of jail parameters.
void
jail::create(const std::string& jail_name,
             const std::string& jail_params)
{
    args_vector av;

    // creation flag
    av.push_back("-qc");

    // jail name
    av.push_back("name=" + jail_name);

    // determine maximum allowed children.max
    const char* const oid = "security.jail.children.max";
    int max;
    size_t len = sizeof(max);
    if (::sysctlbyname(oid, &max, &len, NULL, 0) != 0) {
        std::cerr << "sysctlbyname(" << oid << ") errors: "
            << strerror(errno) << ".\n";
        std::exit(EXIT_FAILURE);
    }
    if (len < sizeof(max)) {
        std::cerr << "sysctlbyname(" << oid << ") provides less "
            "data (" << len << ") than expected (" << sizeof(max) << ").\n";
        std::exit(EXIT_FAILURE);
    }
    if (max < 0) {
        std::cerr << "sysctlbyname(" << oid << ") yields "
            "abnormal " << max << ".\n";
        std::exit(EXIT_FAILURE);
    }
    if (max > 0)
        max--; // a child jail must have less than parent's children.max
    av.push_back("children.max=" + std::to_string(max));

    // test defined jail params
    const std::vector< std::string > params = parse_params_string(jail_params);
    for (const std::string& p : params)
        av.push_back(p);

    // it must be persist
    av.push_back("persist");

    // invoke jail
    std::auto_ptr< process::child > child = child::fork_capture(
        run(fs::path("/usr/sbin/jail"), av));
    process::status status = child->wait();

    // expect success
    if (status.exited() && status.exitstatus() == EXIT_SUCCESS)
        return;

    // otherwise, let us know what jail thinks and fail fast
    std::cerr << child->output().rdbuf();
    std::exit(EXIT_FAILURE);
}


/// Executes an external binary in a jail and replaces the current process.
///
/// \param jail_name Name of the jail to run within.
/// \param program The test program binary absolute path.
/// \param args The arguments to pass to the binary, without the program name.
void
jail::exec(const std::string& jail_name,
           const fs::path& program,
           const args_vector& args) throw()
{
    // get work dir prepared by kyua
    char cwd[PATH_MAX];
    if (::getcwd(cwd, sizeof(cwd)) == NULL) {
        std::cerr << "jail::exec: getcwd() errors: "
            << strerror(errno) << ".\n";
        std::exit(EXIT_FAILURE);
    }

    // get jail id by its name
    int jid = ::jail_getid(jail_name.c_str());
    if (jid == -1) {
        std::cerr << "jail::exec: jail_getid() errors: "
            << strerror(errno) << ": " << jail_errmsg << ".\n";
        std::exit(EXIT_FAILURE);
    }

    // attach to the jail
    if (::jail_attach(jid) == -1) {
        std::cerr << "jail::exec: jail_attach() errors: "
            << strerror(errno) << ".\n";
        std::exit(EXIT_FAILURE);
    }

    // set back the expected work dir
    if (::chdir(cwd) == -1) {
        std::cerr << "jail::exec: chdir() errors: "
            << strerror(errno) << ".\n";
        std::exit(EXIT_FAILURE);
    }

    process::exec(program, args);
}


/// Removes a jail with a given name.
///
/// It's expected to be run in a subprocess.
///
/// \param jail_name Name of a jail to remove.
void
jail::remove(const std::string& jail_name)
{
    args_vector av;

    // removal flag
    av.push_back("-r");

    // jail name
    av.push_back(jail_name);

    // invoke jail
    std::auto_ptr< process::child > child = child::fork_capture(
        run(fs::path("/usr/sbin/jail"), av));
    process::status status = child->wait();

    // expect success
    if (status.exited() && status.exitstatus() == EXIT_SUCCESS)
        std::exit(EXIT_SUCCESS);

    // otherwise, let us know what jail thinks and fail fast
    std::cerr << child->output().rdbuf();
    std::exit(EXIT_FAILURE);
}


}  // namespace utils
}  // namespace freebsd
