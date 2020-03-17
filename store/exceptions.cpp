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

#include "store/exceptions.hpp"

#include "utils/format/macros.hpp"


/// Constructs a new error with a plain-text message.
///
/// \param message The plain-text error message.
store::error::error(const std::string& message) :
    std::runtime_error(message)
{
}


/// Destructor for the error.
store::error::~error(void) throw()
{
}


/// Constructs a new error with a plain-text message.
///
/// \param message The plain-text error message.
store::integrity_error::integrity_error(const std::string& message) :
    error(message)
{
}


/// Destructor for the error.
store::integrity_error::~integrity_error(void) throw()
{
}


/// Constructs a new error with a plain-text message.
///
/// \param version Version of the current schema.
store::old_schema_error::old_schema_error(const int version) :
    error(F("The database contains version %s of the schema, which is "
            "stale and needs to be upgraded") % version),
    _old_version(version)
{
}


/// Destructor for the error.
store::old_schema_error::~old_schema_error(void) throw()
{
}


/// Returns the current schema version in the database.
///
/// \return A version number.
int
store::old_schema_error::old_version(void) const
{
    return _old_version;
}
