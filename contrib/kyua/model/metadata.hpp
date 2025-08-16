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

/// \file model/metadata.hpp
/// Definition of the "test metadata" concept.

#if !defined(MODEL_METADATA_HPP)
#define MODEL_METADATA_HPP

#include "model/metadata_fwd.hpp"

#include <memory>
#include <ostream>
#include <string>

#include "model/types.hpp"
#include "utils/config/tree_fwd.hpp"
#include "utils/datetime_fwd.hpp"
#include "utils/fs/path_fwd.hpp"
#include "utils/noncopyable.hpp"
#include "utils/units_fwd.hpp"

namespace model {


/// Collection of metadata properties of a test.
class metadata {
    struct impl;

    /// Pointer to the shared internal implementation.
    std::shared_ptr< impl > _pimpl;

    friend class metadata_builder;

public:
    metadata(const utils::config::tree&);
    ~metadata(void);

    metadata apply_overrides(const metadata&) const;

    const strings_set& allowed_architectures(void) const;
    const strings_set& allowed_platforms(void) const;
    model::properties_map custom(void) const;
    const std::string& description(void) const;
    const std::string& execenv(void) const;
    const std::string& execenv_jail_params(void) const;
    bool has_cleanup(void) const;
    bool has_execenv(void) const;
    bool is_exclusive(void) const;
    const strings_set& required_configs(void) const;
    const utils::units::bytes& required_disk_space(void) const;
    const paths_set& required_files(void) const;
    const utils::units::bytes& required_memory(void) const;
    const strings_set& required_kmods(void) const;
    const paths_set& required_programs(void) const;
    const std::string& required_user(void) const;
    const utils::datetime::delta& timeout(void) const;

    model::properties_map to_properties(void) const;

    bool operator==(const metadata&) const;
    bool operator!=(const metadata&) const;
};


std::ostream& operator<<(std::ostream&, const metadata&);


/// Builder for a metadata object.
class metadata_builder : utils::noncopyable {
    struct impl;

    /// Pointer to the shared internal implementation.
    std::unique_ptr< impl > _pimpl;

public:
    metadata_builder(void);
    explicit metadata_builder(const metadata&);
    ~metadata_builder(void);

    metadata_builder& add_allowed_architecture(const std::string&);
    metadata_builder& add_allowed_platform(const std::string&);
    metadata_builder& add_custom(const std::string&, const std::string&);
    metadata_builder& add_required_config(const std::string&);
    metadata_builder& add_required_file(const utils::fs::path&);
    metadata_builder& add_required_program(const utils::fs::path&);

    metadata_builder& set_allowed_architectures(const strings_set&);
    metadata_builder& set_allowed_platforms(const strings_set&);
    metadata_builder& set_custom(const model::properties_map&);
    metadata_builder& set_description(const std::string&);
    metadata_builder& set_execenv(const std::string&);
    metadata_builder& set_execenv_jail_params(const std::string&);
    metadata_builder& set_has_cleanup(const bool);
    metadata_builder& set_is_exclusive(const bool);
    metadata_builder& set_required_configs(const strings_set&);
    metadata_builder& set_required_disk_space(const utils::units::bytes&);
    metadata_builder& set_required_files(const paths_set&);
    metadata_builder& set_required_memory(const utils::units::bytes&);
    metadata_builder& set_required_kmods(const strings_set&);
    metadata_builder& set_required_programs(const paths_set&);
    metadata_builder& set_required_user(const std::string&);
    metadata_builder& set_string(const std::string&, const std::string&);
    metadata_builder& set_timeout(const utils::datetime::delta&);

    metadata build(void) const;
};


}  // namespace model

#endif  // !defined(MODEL_METADATA_HPP)
