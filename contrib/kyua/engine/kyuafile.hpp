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

/// \file engine/kyuafile.hpp
/// Test suite configuration parsing and representation.

#if !defined(ENGINE_KYUAFILE_HPP)
#define ENGINE_KYUAFILE_HPP

#include "engine/kyuafile_fwd.hpp"

#include <string>
#include <vector>

#include <lutok/state.hpp>

#include "engine/scheduler_fwd.hpp"
#include "model/test_program_fwd.hpp"
#include "utils/config/tree_fwd.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional_fwd.hpp"

namespace engine {


/// Representation of the configuration of a test suite.
///
/// Test suites are collections of related test programs.  They are described by
/// a configuration file.
///
/// Test suites have two path references: one to the "source root" and another
/// one to the "build root".  The source root points to the directory from which
/// the Kyuafile is being read, and all recursive inclusions are resolved
/// relative to that directory.  The build root points to the directory
/// containing the generated test programs and is prepended to the absolute path
/// of the test programs referenced by the Kyuafiles.  In general, the build
/// root will be the same as the source root; however, when using a build system
/// that supports "build directories", providing this option comes in handy to
/// allow running the tests without much hassle.
///
/// This class provides the parser for test suite configuration files and
/// methods to access the parsed data.
class kyuafile {
    /// Path to the directory containing the top-level Kyuafile loaded.
    utils::fs::path _source_root;

    /// Path to the directory containing the test programs.
    utils::fs::path _build_root;

    /// Collection of the test programs defined in the Kyuafile.
    model::test_programs_vector _test_programs;

public:
    explicit kyuafile(const utils::fs::path&, const utils::fs::path&,
                      const model::test_programs_vector&);
    ~kyuafile(void);

    static kyuafile load(const utils::fs::path&,
                         const utils::optional< utils::fs::path >,
                         const utils::config::tree&,
                         scheduler::scheduler_handle&);

    const utils::fs::path& source_root(void) const;
    const utils::fs::path& build_root(void) const;
    const model::test_programs_vector& test_programs(void) const;
};


}  // namespace engine

#endif  // !defined(ENGINE_KYUAFILE_HPP)
