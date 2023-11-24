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

#include "utils/fs/directory.hpp"

#include <sstream>

#include <atf-c++.hpp>

#include "utils/format/containers.ipp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"

namespace fs = utils::fs;


ATF_TEST_CASE_WITHOUT_HEAD(directory_entry__public_fields);
ATF_TEST_CASE_BODY(directory_entry__public_fields)
{
    const fs::directory_entry entry("name");
    ATF_REQUIRE_EQ("name", entry.name);
}


ATF_TEST_CASE_WITHOUT_HEAD(directory_entry__equality);
ATF_TEST_CASE_BODY(directory_entry__equality)
{
    const fs::directory_entry entry1("name");
    const fs::directory_entry entry2("other-name");

    ATF_REQUIRE(  entry1 == entry1);
    ATF_REQUIRE(!(entry1 != entry1));

    ATF_REQUIRE(!(entry1 == entry2));
    ATF_REQUIRE(  entry1 != entry2);
}


ATF_TEST_CASE_WITHOUT_HEAD(directory_entry__sorting);
ATF_TEST_CASE_BODY(directory_entry__sorting)
{
    const fs::directory_entry entry1("name");
    const fs::directory_entry entry2("other-name");

    ATF_REQUIRE(!(entry1 < entry1));
    ATF_REQUIRE(!(entry2 < entry2));
    ATF_REQUIRE(  entry1 < entry2);
    ATF_REQUIRE(!(entry2 < entry1));
}


ATF_TEST_CASE_WITHOUT_HEAD(directory_entry__format);
ATF_TEST_CASE_BODY(directory_entry__format)
{
    const fs::directory_entry entry("this is the name");
    std::ostringstream output;
    output << entry;
    ATF_REQUIRE_EQ("directory_entry{name='this is the name'}", output.str());
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__empty);
ATF_TEST_CASE_BODY(integration__empty)
{
    fs::mkdir(fs::path("empty"), 0755);

    std::set< fs::directory_entry > contents;
    const fs::directory dir(fs::path("empty"));
    for (fs::directory::const_iterator iter = dir.begin(); iter != dir.end();
         ++iter) {
        contents.insert(*iter);
        // While we are here, make sure both * and -> represent the same.
        ATF_REQUIRE((*iter).name == iter->name);
    }

    std::set< fs::directory_entry > exp_contents;
    exp_contents.insert(fs::directory_entry("."));
    exp_contents.insert(fs::directory_entry(".."));

    ATF_REQUIRE_EQ(exp_contents, contents);
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__some_contents);
ATF_TEST_CASE_BODY(integration__some_contents)
{
    fs::mkdir(fs::path("full"), 0755);
    atf::utils::create_file("full/a file", "");
    atf::utils::create_file("full/something-else", "");
    atf::utils::create_file("full/.hidden", "");
    fs::mkdir(fs::path("full/subdir"), 0755);
    atf::utils::create_file("full/subdir/not-listed", "");

    std::set< fs::directory_entry > contents;
    const fs::directory dir(fs::path("full"));
    for (fs::directory::const_iterator iter = dir.begin(); iter != dir.end();
         ++iter) {
        contents.insert(*iter);
        // While we are here, make sure both * and -> represent the same.
        ATF_REQUIRE((*iter).name == iter->name);
    }

    std::set< fs::directory_entry > exp_contents;
    exp_contents.insert(fs::directory_entry("."));
    exp_contents.insert(fs::directory_entry(".."));
    exp_contents.insert(fs::directory_entry(".hidden"));
    exp_contents.insert(fs::directory_entry("a file"));
    exp_contents.insert(fs::directory_entry("something-else"));
    exp_contents.insert(fs::directory_entry("subdir"));

    ATF_REQUIRE_EQ(exp_contents, contents);
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__open_failure);
ATF_TEST_CASE_BODY(integration__open_failure)
{
    const fs::directory directory(fs::path("non-existent"));
    ATF_REQUIRE_THROW_RE(fs::system_error, "opendir(.*non-existent.*) failed",
                         directory.begin());
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__iterators_equality);
ATF_TEST_CASE_BODY(integration__iterators_equality)
{
    const fs::directory directory(fs::path("."));

    fs::directory::const_iterator iter_ok1 = directory.begin();
    fs::directory::const_iterator iter_ok2 = directory.begin();
    fs::directory::const_iterator iter_end = directory.end();

    ATF_REQUIRE(  iter_ok1 == iter_ok1);
    ATF_REQUIRE(!(iter_ok1 != iter_ok1));

    ATF_REQUIRE(  iter_ok2 == iter_ok2);
    ATF_REQUIRE(!(iter_ok2 != iter_ok2));

    ATF_REQUIRE(!(iter_ok1 == iter_ok2));
    ATF_REQUIRE(  iter_ok1 != iter_ok2);

    ATF_REQUIRE(!(iter_ok1 == iter_end));
    ATF_REQUIRE(  iter_ok1 != iter_end);

    ATF_REQUIRE(!(iter_ok2 == iter_end));
    ATF_REQUIRE(  iter_ok2 != iter_end);

    ATF_REQUIRE(  iter_end == iter_end);
    ATF_REQUIRE(!(iter_end != iter_end));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, directory_entry__public_fields);
    ATF_ADD_TEST_CASE(tcs, directory_entry__equality);
    ATF_ADD_TEST_CASE(tcs, directory_entry__sorting);
    ATF_ADD_TEST_CASE(tcs, directory_entry__format);

    ATF_ADD_TEST_CASE(tcs, integration__empty);
    ATF_ADD_TEST_CASE(tcs, integration__some_contents);
    ATF_ADD_TEST_CASE(tcs, integration__open_failure);
    ATF_ADD_TEST_CASE(tcs, integration__iterators_equality);
}
