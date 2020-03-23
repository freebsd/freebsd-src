// Copyright 2014 The Kyua Authors.
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

/// \file store/layout.hpp
/// File system layout definition for the Kyua data files.
///
/// Tests results files are all stored in a centralized directory by default.
/// In the general case, we do not want the user to have to worry about files:
/// we expose an identifier-based interface where each tests results file has a
/// unique identifier.  However, we also want to give full freedom to the user
/// to store such files wherever he likes so we have to deal with paths as well.
///
/// When creating a new results file, the inputs to resolve the path can be:
/// - NEW: Automatic generation of a new results file, so we want to return its
///   public identifier and the path for internal consumption.
/// - A path: The user provided the specific location where he wants the file
///   stored, so we just obey that.  There is no public identifier in this case
///   because there is no naming scheme imposed on the generated files.
///
/// When opening an existing results file, the inputs to resolve the path can
/// be:
/// - LATEST: Given the current directory, we derive the corresponding test
///   suite name and find the latest timestamped file in the centralized
///   location.
/// - A path: If the file exists, we just open that.  If it doesn't exist or if
///   it is a directory, we try to resolve that as a test suite name and locate
///   the latest matching timestamped file.
/// - Everything else: Treated as a test suite identifier, so we try to locate
///   the latest matchin timestamped file.

#if !defined(STORE_LAYOUT_HPP)
#define STORE_LAYOUT_HPP

#include "store/layout_fwd.hpp"

#include <string>

#include "utils/datetime_fwd.hpp"
#include "utils/fs/path_fwd.hpp"

namespace store {
namespace layout {


extern const char* results_auto_create_name;
extern const char* results_auto_open_name;

utils::fs::path find_results(const std::string&);
results_id_file_pair new_db(const std::string&, const utils::fs::path&);
utils::fs::path new_db_for_migration(const utils::fs::path&,
                                     const utils::datetime::timestamp&);
utils::fs::path query_store_dir(void);
std::string test_suite_for_path(const utils::fs::path&);


}  // namespace layout
}  // namespace store

#endif  // !defined(STORE_LAYOUT_HPP)
