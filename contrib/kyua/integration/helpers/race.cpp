// Copyright 2015 The Kyua Authors.
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

/// \file integration/helpers/race.cpp
/// Creates a file and reads it back, looking for races.
///
/// This program should fail with high chances if it is called multiple times at
/// once with TEST_ENV_shared_file pointing to the same file.

extern "C" {
#include <sys/types.h>

#include <unistd.h>
}

#include <cstdlib>
#include <fstream>
#include <iostream>

#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/env.hpp"
#include "utils/optional.ipp"
#include "utils/stream.hpp"

namespace fs = utils::fs;

using utils::optional;


/// Entry point to the helper test program.
///
/// \return EXIT_SUCCESS if no race is detected; EXIT_FAILURE otherwise.
int
main(void)
{
    const optional< std::string > shared_file = utils::getenv(
        "TEST_ENV_shared_file");
    if (!shared_file) {
        std::cerr << "Environment variable TEST_ENV_shared_file not defined\n";
        std::exit(EXIT_FAILURE);
    }
    const fs::path shared_path(shared_file.get());

    if (fs::exists(shared_path)) {
        std::cerr << "Shared file already exists; created by a concurrent "
            "test?";
        std::exit(EXIT_FAILURE);
    }

    const std::string contents = F("%s") % ::getpid();

    std::ofstream output(shared_path.c_str());
    if (!output) {
        std::cerr << "Failed to create shared file; conflict with a concurrent "
            "test?";
        std::exit(EXIT_FAILURE);
    }
    output << contents;
    output.close();

    ::usleep(10000);

    const std::string read_contents = utils::read_file(shared_path);
    if (read_contents != contents) {
        std::cerr << "Shared file contains unexpected contents; modified by a "
            "concurrent test?";
        std::exit(EXIT_FAILURE);
    }

    fs::unlink(shared_path);
    std::exit(EXIT_SUCCESS);
}
