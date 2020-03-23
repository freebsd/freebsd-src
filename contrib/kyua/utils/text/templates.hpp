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

/// \file utils/text/templates.hpp
/// Custom templating engine for text documents.
///
/// This module provides a simple mechanism to generate text documents based on
/// templates.  The templates are just text files that contain template
/// statements that instruct this processor to perform transformations on the
/// input.
///
/// While this was originally written to handle HTML templates, it is actually
/// generic enough to handle any kind of text document, hence why it lives
/// within the utils::text library.
///
/// An example of how the templates look like:
///
///   %if names
///   List of names
///   -------------
///   Amount of names: %%length(names)%%
///   Most preferred name: %%preferred_name%%
///   Full list:
///   %loop names iter
///     * %%last_names(iter)%%, %%names(iter)%%
///   %endloop
///   %endif names

#if !defined(UTILS_TEXT_TEMPLATES_HPP)
#define UTILS_TEXT_TEMPLATES_HPP

#include "utils/text/templates_fwd.hpp"

#include <istream>
#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "utils/fs/path_fwd.hpp"

namespace utils {
namespace text {


/// Definitions of the templates to apply to a file.
///
/// This class provides the environment (e.g. the list of variables) that the
/// templating system has to use when generating the output files.  This
/// definition is static in the sense that this is what the caller program
/// specifies.
class templates_def {
    /// Mapping of variable names to their values.
    typedef std::map< std::string, std::string > variables_map;

    /// Collection of global variables available to the templates.
    variables_map _variables;

    /// Convenience name for a vector of strings.
    typedef std::vector< std::string > strings_vector;

    /// Mapping of vector names to their contents.
    ///
    /// Ideally, these would be represented as part of the _variables, but we
    /// would need a complex mechanism to identify whether a variable is a
    /// string or a vector.
    typedef std::map< std::string, strings_vector > vectors_map;

    /// Collection of vectors available to the templates.
    vectors_map _vectors;

    const std::string& get_vector(const std::string&, const std::string&) const;

public:
    templates_def(void);

    void add_variable(const std::string&, const std::string&);
    void remove_variable(const std::string&);
    void add_vector(const std::string&);
    void add_to_vector(const std::string&, const std::string&);

    bool exists(const std::string&) const;
    const std::string& get_variable(const std::string&) const;
    const strings_vector& get_vector(const std::string&) const;

    std::string evaluate(const std::string&) const;
};


void instantiate(const templates_def&, std::istream&, std::ostream&);
void instantiate(const templates_def&, const fs::path&, const fs::path&);


}  // namespace text
}  // namespace utils

#endif  // !defined(UTILS_TEXT_TEMPLATES_HPP)
