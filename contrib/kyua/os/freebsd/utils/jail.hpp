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

/// \file os/freebsd/utils/jail.hpp
/// FreeBSD jail utilities.

#if !defined(FREEBSD_UTILS_JAIL_HPP)
#define FREEBSD_UTILS_JAIL_HPP

#include "utils/defs.hpp"
#include "utils/fs/path_fwd.hpp"
#include "utils/process/operations_fwd.hpp"

namespace fs = utils::fs;

using utils::process::args_vector;

namespace freebsd {
namespace utils {


class jail {
public:
    std::vector< std::string > parse_params_string(const std::string& str);
    std::string make_name(const fs::path& program,
                          const std::string& test_case_name);
    void create(const std::string& jail_name,
                const std::string& jail_params);
    void exec(const std::string& jail_name,
              const fs::path& program,
              const args_vector& args) throw() UTILS_NORETURN;
    void remove(const std::string& jail_name);
};


}  // namespace utils
}  // namespace freebsd

#endif  // !defined(FREEBSD_UTILS_JAIL_HPP)
