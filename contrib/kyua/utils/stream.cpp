// Copyright 2011 The Kyua Authors.
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

#include "utils/stream.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/sanity.hpp"

namespace fs = utils::fs;


namespace {


/// Constant that represents the path to stdout.
static const fs::path stdout_path("/dev/stdout");


/// Constant that represents the path to stderr.
static const fs::path stderr_path("/dev/stderr");


}  // anonymous namespace


/// Opens a new file for output, respecting the stdout and stderr streams.
///
/// \param path The path to the output file to be created.
///
/// \return A pointer to a new output stream.
std::unique_ptr< std::ostream >
utils::open_ostream(const fs::path& path)
{
    std::unique_ptr< std::ostream > out;
    if (path == stdout_path) {
        out.reset(new std::ofstream());
        out->copyfmt(std::cout);
        out->clear(std::cout.rdstate());
        out->rdbuf(std::cout.rdbuf());
    } else if (path == stderr_path) {
        out.reset(new std::ofstream());
        out->copyfmt(std::cerr);
        out->clear(std::cerr.rdstate());
        out->rdbuf(std::cerr.rdbuf());
    } else {
        out.reset(new std::ofstream(path.c_str()));
        if (!(*out)) {
            throw std::runtime_error(F("Cannot open output file %s") % path);
        }
    }
    INV(out.get() != NULL);
    return out;
}


/// Gets the length of a stream.
///
/// \param is The input stream for which to calculate its length.
///
/// \return The length of the stream.  This is of size_t type instead of
/// directly std::streampos to simplify the caller.  Some systems do not
/// support comparing a std::streampos directly to an integer (see
/// NetBSD 1.5.x), which is what we often want to do.
///
/// \throw std::exception If calculating the length fails due to a stream error.
std::size_t
utils::stream_length(std::istream& is)
{
    const std::streampos current_pos = is.tellg();
    try {
        is.seekg(0, std::ios::end);
        const std::streampos length = is.tellg();
        is.seekg(current_pos, std::ios::beg);
        return static_cast< std::size_t >(length);
    } catch (...) {
        is.seekg(current_pos, std::ios::beg);
        throw;
    }
}


/// Reads a whole file into memory.
///
/// \param path The file to read.
///
/// \return A plain string containing the raw contents of the file.
///
/// \throw std::runtime_error If the file cannot be opened.
std::string
utils::read_file(const fs::path& path)
{
    std::ifstream input(path.c_str());
    if (!input)
        throw std::runtime_error(F("Failed to open '%s' for read") % path);
    return read_stream(input);
}


/// Reads the whole contents of a stream into memory.
///
/// \param input The input stream from which to read.
///
/// \return A plain string containing the raw contents of the file.
std::string
utils::read_stream(std::istream& input)
{
    std::ostringstream buffer;

    char tmp[1024];
    while (input.good()) {
        input.read(tmp, sizeof(tmp));
        if (input.good() || input.eof()) {
            buffer.write(tmp, input.gcount());
        }
    }

    return buffer.str();
}
