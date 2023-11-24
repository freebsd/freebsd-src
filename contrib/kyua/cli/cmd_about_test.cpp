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

#include "cli/cmd_about.hpp"

#if defined(HAVE_CONFIG_H)
#   include "config.h"
#endif

#include <cstdlib>

#include <atf-c++.hpp>

#include "cli/common.ipp"
#include "engine/config.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/parser.hpp"
#include "utils/cmdline/ui_mock.hpp"
#include "utils/config/tree.ipp"
#include "utils/env.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"

namespace cmdline = utils::cmdline;
namespace fs = utils::fs;

using cli::cmd_about;


ATF_TEST_CASE_WITHOUT_HEAD(all_topics__ok);
ATF_TEST_CASE_BODY(all_topics__ok)
{
    cmdline::args_vector args;
    args.push_back("about");

    fs::mkdir(fs::path("fake-docs"), 0755);
    atf::utils::create_file("fake-docs/AUTHORS",
                            "Content of AUTHORS\n"
                            "* First author\n"
                            " * garbage\n"
                            "* Second author\n");
    atf::utils::create_file("fake-docs/CONTRIBUTORS",
                            "Content of CONTRIBUTORS\n"
                            "* First contributor\n"
                            " * garbage\n"
                            "* Second contributor\n");
    atf::utils::create_file("fake-docs/LICENSE", "Content of LICENSE\n");

    utils::setenv("KYUA_DOCDIR", "fake-docs");
    cmd_about cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_SUCCESS, cmd.main(&ui, args, engine::default_config()));

    ATF_REQUIRE(atf::utils::grep_string(PACKAGE_NAME, ui.out_log()[0]));
    ATF_REQUIRE(atf::utils::grep_string(PACKAGE_VERSION, ui.out_log()[0]));

    ATF_REQUIRE(!atf::utils::grep_collection("Content of AUTHORS",
                                             ui.out_log()));
    ATF_REQUIRE(atf::utils::grep_collection("\\* First author", ui.out_log()));
    ATF_REQUIRE(!atf::utils::grep_collection("garbage", ui.out_log()));
    ATF_REQUIRE(atf::utils::grep_collection("\\* Second author", ui.out_log()));

    ATF_REQUIRE(!atf::utils::grep_collection("Content of CONTRIBUTORS",
                                             ui.out_log()));
    ATF_REQUIRE(atf::utils::grep_collection("\\* First contributor",
                                            ui.out_log()));
    ATF_REQUIRE(!atf::utils::grep_collection("garbage", ui.out_log()));
    ATF_REQUIRE(atf::utils::grep_collection("\\* Second contributor",
                                            ui.out_log()));

    ATF_REQUIRE(atf::utils::grep_collection("Content of LICENSE",
                                            ui.out_log()));

    ATF_REQUIRE(atf::utils::grep_collection("Homepage", ui.out_log()));
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(all_topics__missing_docs);
ATF_TEST_CASE_BODY(all_topics__missing_docs)
{
    cmdline::args_vector args;
    args.push_back("about");

    utils::setenv("KYUA_DOCDIR", "fake-docs");
    cmd_about cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_FAILURE, cmd.main(&ui, args, engine::default_config()));

    ATF_REQUIRE(atf::utils::grep_string(PACKAGE_NAME, ui.out_log()[0]));
    ATF_REQUIRE(atf::utils::grep_string(PACKAGE_VERSION, ui.out_log()[0]));

    ATF_REQUIRE(atf::utils::grep_collection("Homepage", ui.out_log()));

    ATF_REQUIRE(atf::utils::grep_collection("Failed to open.*AUTHORS",
                                            ui.err_log()));
    ATF_REQUIRE(atf::utils::grep_collection("Failed to open.*CONTRIBUTORS",
                                            ui.err_log()));
    ATF_REQUIRE(atf::utils::grep_collection("Failed to open.*LICENSE",
                                            ui.err_log()));
}


ATF_TEST_CASE_WITHOUT_HEAD(topic_authors__ok);
ATF_TEST_CASE_BODY(topic_authors__ok)
{
    cmdline::args_vector args;
    args.push_back("about");
    args.push_back("authors");

    fs::mkdir(fs::path("fake-docs"), 0755);
    atf::utils::create_file("fake-docs/AUTHORS",
                            "Content of AUTHORS\n"
                            "* First author\n"
                            " * garbage\n"
                            "* Second author\n");
    atf::utils::create_file("fake-docs/CONTRIBUTORS",
                            "Content of CONTRIBUTORS\n"
                            "* First contributor\n"
                            " * garbage\n"
                            "* Second contributor\n");

    utils::setenv("KYUA_DOCDIR", "fake-docs");
    cmd_about cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_SUCCESS, cmd.main(&ui, args, engine::default_config()));
    ATF_REQUIRE(!atf::utils::grep_string(PACKAGE_NAME, ui.out_log()[0]));

    ATF_REQUIRE(!atf::utils::grep_collection("Content of AUTHORS",
                                             ui.out_log()));
    ATF_REQUIRE(atf::utils::grep_collection("\\* First author", ui.out_log()));
    ATF_REQUIRE(!atf::utils::grep_collection("garbage", ui.out_log()));
    ATF_REQUIRE(atf::utils::grep_collection("\\* Second author", ui.out_log()));

    ATF_REQUIRE(!atf::utils::grep_collection("Content of CONTRIBUTORS",
                                             ui.out_log()));
    ATF_REQUIRE(atf::utils::grep_collection("\\* First contributor",
                                            ui.out_log()));
    ATF_REQUIRE(!atf::utils::grep_collection("garbage", ui.out_log()));
    ATF_REQUIRE(atf::utils::grep_collection("\\* Second contributor",
                                            ui.out_log()));

    ATF_REQUIRE(!atf::utils::grep_collection("LICENSE", ui.out_log()));
    ATF_REQUIRE(!atf::utils::grep_collection("Homepage", ui.out_log()));
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(topic_authors__missing_doc);
ATF_TEST_CASE_BODY(topic_authors__missing_doc)
{
    cmdline::args_vector args;
    args.push_back("about");
    args.push_back("authors");

    utils::setenv("KYUA_DOCDIR", "fake-docs");
    cmd_about cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_FAILURE, cmd.main(&ui, args, engine::default_config()));

    ATF_REQUIRE_EQ(0, ui.out_log().size());

    ATF_REQUIRE(atf::utils::grep_collection("Failed to open.*AUTHORS",
                                            ui.err_log()));
    ATF_REQUIRE(atf::utils::grep_collection("Failed to open.*CONTRIBUTORS",
                                            ui.err_log()));
    ATF_REQUIRE(!atf::utils::grep_collection("Failed to open.*LICENSE",
                                             ui.err_log()));
}


ATF_TEST_CASE_WITHOUT_HEAD(topic_license__ok);
ATF_TEST_CASE_BODY(topic_license__ok)
{
    cmdline::args_vector args;
    args.push_back("about");
    args.push_back("license");

    fs::mkdir(fs::path("fake-docs"), 0755);
    atf::utils::create_file("fake-docs/LICENSE", "Content of LICENSE\n");

    utils::setenv("KYUA_DOCDIR", "fake-docs");
    cmd_about cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_SUCCESS, cmd.main(&ui, args, engine::default_config()));
    ATF_REQUIRE(!atf::utils::grep_string(PACKAGE_NAME, ui.out_log()[0]));
    ATF_REQUIRE(!atf::utils::grep_collection("AUTHORS", ui.out_log()));
    ATF_REQUIRE(!atf::utils::grep_collection("CONTRIBUTORS", ui.out_log()));
    ATF_REQUIRE(atf::utils::grep_collection("Content of LICENSE",
                                            ui.out_log()));
    ATF_REQUIRE(!atf::utils::grep_collection("Homepage", ui.out_log()));
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(topic_license__missing_doc);
ATF_TEST_CASE_BODY(topic_license__missing_doc)
{
    cmdline::args_vector args;
    args.push_back("about");
    args.push_back("license");

    utils::setenv("KYUA_DOCDIR", "fake-docs");
    cmd_about cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_FAILURE, cmd.main(&ui, args, engine::default_config()));

    ATF_REQUIRE_EQ(0, ui.out_log().size());

    ATF_REQUIRE(!atf::utils::grep_collection("Failed to open.*AUTHORS",
                                             ui.err_log()));
    ATF_REQUIRE(!atf::utils::grep_collection("Failed to open.*CONTRIBUTORS",
                                             ui.err_log()));
    ATF_REQUIRE(atf::utils::grep_collection("Failed to open.*LICENSE",
                                            ui.err_log()));
}


ATF_TEST_CASE_WITHOUT_HEAD(topic_version__ok);
ATF_TEST_CASE_BODY(topic_version__ok)
{
    cmdline::args_vector args;
    args.push_back("about");
    args.push_back("version");

    utils::setenv("KYUA_DOCDIR", "fake-docs");
    cmd_about cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_SUCCESS, cmd.main(&ui, args, engine::default_config()));
    ATF_REQUIRE_EQ(1, ui.out_log().size());
    ATF_REQUIRE(atf::utils::grep_string(PACKAGE_NAME, ui.out_log()[0]));
    ATF_REQUIRE(atf::utils::grep_string(PACKAGE_VERSION, ui.out_log()[0]));
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(invalid_args);
ATF_TEST_CASE_BODY(invalid_args)
{
    cmdline::args_vector args;
    args.push_back("about");
    args.push_back("first");
    args.push_back("second");

    cmd_about cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "Too many arguments",
                         cmd.main(&ui, args, engine::default_config()));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(invalid_topic);
ATF_TEST_CASE_BODY(invalid_topic)
{
    cmdline::args_vector args;
    args.push_back("about");
    args.push_back("foo");

    cmd_about cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "Invalid about topic 'foo'",
                         cmd.main(&ui, args, engine::default_config()));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, all_topics__ok);
    ATF_ADD_TEST_CASE(tcs, all_topics__missing_docs);
    ATF_ADD_TEST_CASE(tcs, topic_authors__ok);
    ATF_ADD_TEST_CASE(tcs, topic_authors__missing_doc);
    ATF_ADD_TEST_CASE(tcs, topic_license__ok);
    ATF_ADD_TEST_CASE(tcs, topic_license__missing_doc);
    ATF_ADD_TEST_CASE(tcs, topic_version__ok);
    ATF_ADD_TEST_CASE(tcs, invalid_args);
    ATF_ADD_TEST_CASE(tcs, invalid_topic);
}
