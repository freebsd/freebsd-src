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

#include "engine/exceptions.hpp"

#include "utils/format/macros.hpp"

namespace fs = utils::fs;

/// Constructs a new error with a plain-text message.
///
/// \param message The plain-text error message.
engine::error::error(const std::string& message) :
    std::runtime_error(message)
{
}


/// Destructor for the error.
engine::error::~error(void) throw()
{
}


/// Constructs a new format_error.
///
/// \param reason_ Description of the format problem.
engine::format_error::format_error(const std::string& reason_) :
    error(reason_)
{
}


/// Destructor for the error.
engine::format_error::~format_error(void) throw()
{
}


/// Constructs a new load_error.
///
/// \param file_ The file in which the error was encountered.
/// \param reason_ Description of the load problem.
engine::load_error::load_error(const fs::path& file_,
                               const std::string& reason_) :
    error(F("Load of '%s' failed: %s") % file_ % reason_),
    file(file_),
    reason(reason_)
{
}


/// Destructor for the error.
engine::load_error::~load_error(void) throw()
{
}
