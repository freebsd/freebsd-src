// Copyright 2012 The Kyua Authors.
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

/// \file utils/config/parser.hpp
/// Utilities to read a configuration file into memory.

#if !defined(UTILS_CONFIG_PARSER_HPP)
#define UTILS_CONFIG_PARSER_HPP

#include "utils/config/parser_fwd.hpp"

#include <memory>

#include "utils/config/tree_fwd.hpp"
#include "utils/fs/path_fwd.hpp"
#include "utils/noncopyable.hpp"

namespace utils {
namespace config {


/// A configuration parser.
///
/// This parser is a class rather than a function because we need to support
/// callbacks to perform the initialization of the config file schema.  The
/// configuration files always start with a call to syntax(), which define the
/// particular version of the schema being used.  Depending on such version, the
/// layout of the internal tree representation needs to be different.
///
/// A parser implementation must provide a setup() method to set up the
/// configuration schema based on the particular combination of syntax format
/// and version specified on the file.
///
/// Parser objects are not supposed to be reused, and specific trees are not
/// supposed to be passed to multiple parsers (even if sequentially).  Doing so
/// will cause all kinds of inconsistencies in the managed tree itself or in the
/// Lua state.
class parser : noncopyable {
public:
    struct impl;

private:
    /// Pointer to the internal implementation.
    std::unique_ptr< impl > _pimpl;

    /// Hook to initialize the tree keys before reading the file.
    ///
    /// This hook gets called when the configuration file defines its specific
    /// format by calling the syntax() function.  We have to delay the tree
    /// initialization until this point because, before we know what version of
    /// a configuration file we are parsing, we cannot know what keys are valid.
    ///
    /// \param [in,out] config_tree The tree in which to define the key
    ///     structure.
    /// \param syntax_version The version of the file format as specified in the
    ///     configuration file.
    virtual void setup(tree& config_tree, const int syntax_version) = 0;

public:
    explicit parser(tree&);
    virtual ~parser(void);

    void parse(const fs::path&);
};


}  // namespace config
}  // namespace utils

#endif  // !defined(UTILS_CONFIG_PARSER_HPP)
