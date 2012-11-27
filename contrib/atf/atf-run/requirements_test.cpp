//
// Automated Testing Framework (atf)
//
// Copyright (c) 2010 The NetBSD Foundation, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
// CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
// IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include "atf-c++/config.hpp"
#include "atf-c++/detail/text.hpp"
#include "atf-c++/macros.hpp"

#include "requirements.hpp"
#include "user.hpp"

namespace impl = atf::atf_run;

// -------------------------------------------------------------------------
// Auxiliary functions.
// -------------------------------------------------------------------------

namespace {

const atf::tests::vars_map no_config;

void
do_check(const std::string& expected, const atf::tests::vars_map& metadata,
         const atf::tests::vars_map& config = no_config)
{
    const std::string actual = impl::check_requirements(metadata, config);
    if (!atf::text::match(actual, expected))
        ATF_FAIL("Requirements failure reason \"" + actual + "\" does not "
                 "match \"" + expected + "\"");
}

} // anonymous namespace

// -------------------------------------------------------------------------
// Tests for the require.arch metadata property.
// -------------------------------------------------------------------------

ATF_TEST_CASE(require_arch_one_ok);
ATF_TEST_CASE_HEAD(require_arch_one_ok) {}
ATF_TEST_CASE_BODY(require_arch_one_ok) {
    atf::tests::vars_map metadata;
    metadata["require.arch"] = atf::config::get("atf_arch");
    do_check("", metadata);
}

ATF_TEST_CASE(require_arch_one_fail);
ATF_TEST_CASE_HEAD(require_arch_one_fail) {}
ATF_TEST_CASE_BODY(require_arch_one_fail) {
    atf::tests::vars_map metadata;
    metadata["require.arch"] = "__fake_arch__";
    do_check("Requires the '__fake_arch__' architecture", metadata);
}

ATF_TEST_CASE(require_arch_many_ok);
ATF_TEST_CASE_HEAD(require_arch_many_ok) {}
ATF_TEST_CASE_BODY(require_arch_many_ok) {
    atf::tests::vars_map metadata;
    metadata["require.arch"] = "__foo__ " + atf::config::get("atf_arch") +
        " __bar__";
    do_check("", metadata);
}

ATF_TEST_CASE(require_arch_many_fail);
ATF_TEST_CASE_HEAD(require_arch_many_fail) {}
ATF_TEST_CASE_BODY(require_arch_many_fail) {
    atf::tests::vars_map metadata;
    metadata["require.arch"] = "__foo__ __bar__ __baz__";
    do_check("Requires one of the '__foo__ __bar__ __baz__' architectures",
             metadata);
}

// -------------------------------------------------------------------------
// Tests for the require.config metadata property.
// -------------------------------------------------------------------------

ATF_TEST_CASE(require_config_one_ok);
ATF_TEST_CASE_HEAD(require_config_one_ok) {}
ATF_TEST_CASE_BODY(require_config_one_ok) {
    atf::tests::vars_map metadata, config;
    metadata["require.config"] = "var1";
    config["var1"] = "some-value";
    do_check("", metadata, config);
}

ATF_TEST_CASE(require_config_one_fail);
ATF_TEST_CASE_HEAD(require_config_one_fail) {}
ATF_TEST_CASE_BODY(require_config_one_fail) {
    atf::tests::vars_map metadata, config;
    metadata["require.config"] = "var1";
    do_check("Required configuration variable 'var1' not defined", metadata,
             config);
}

ATF_TEST_CASE(require_config_many_ok);
ATF_TEST_CASE_HEAD(require_config_many_ok) {}
ATF_TEST_CASE_BODY(require_config_many_ok) {
    atf::tests::vars_map metadata, config;
    metadata["require.config"] = "var1 var2 var3";
    config["var1"] = "first-value";
    config["var2"] = "second-value";
    config["var3"] = "third-value";
    do_check("", metadata, config);
}

ATF_TEST_CASE(require_config_many_fail);
ATF_TEST_CASE_HEAD(require_config_many_fail) {}
ATF_TEST_CASE_BODY(require_config_many_fail) {
    atf::tests::vars_map metadata, config;
    metadata["require.config"] = "var1 var2 var3";
    config["var1"] = "first-value";
    config["var3"] = "third-value";
    do_check("Required configuration variable 'var2' not defined", metadata,
             config);
}

// -------------------------------------------------------------------------
// Tests for the require.files metadata property.
// -------------------------------------------------------------------------

ATF_TEST_CASE_WITHOUT_HEAD(require_files_one_ok);
ATF_TEST_CASE_BODY(require_files_one_ok) {
    atf::tests::vars_map metadata;
    metadata["require.files"] = "/bin/ls";
    do_check("", metadata);
}

ATF_TEST_CASE_WITHOUT_HEAD(require_files_one_missing);
ATF_TEST_CASE_BODY(require_files_one_missing) {
    atf::tests::vars_map metadata;
    metadata["require.files"] = "/non-existent/foo";
    do_check("Required file '/non-existent/foo' not found", metadata);
}

ATF_TEST_CASE_WITHOUT_HEAD(require_files_one_fail);
ATF_TEST_CASE_BODY(require_files_one_fail) {
    atf::tests::vars_map metadata;
    metadata["require.files"] = "/bin/cp this-is-relative";
    ATF_REQUIRE_THROW_RE(std::runtime_error, "Relative.*(this-is-relative)",
                         impl::check_requirements(metadata, no_config));
}

ATF_TEST_CASE_WITHOUT_HEAD(require_files_many_ok);
ATF_TEST_CASE_BODY(require_files_many_ok) {
    atf::tests::vars_map metadata;
    metadata["require.files"] = "/bin/ls /bin/cp";
    do_check("", metadata);
}

ATF_TEST_CASE_WITHOUT_HEAD(require_files_many_missing);
ATF_TEST_CASE_BODY(require_files_many_missing) {
    atf::tests::vars_map metadata;
    metadata["require.files"] = "/bin/ls /non-existent/bar /bin/cp";
    do_check("Required file '/non-existent/bar' not found", metadata);
}

ATF_TEST_CASE_WITHOUT_HEAD(require_files_many_fail);
ATF_TEST_CASE_BODY(require_files_many_fail) {
    atf::tests::vars_map metadata;
    metadata["require.files"] = "/bin/cp also-relative";
    ATF_REQUIRE_THROW_RE(std::runtime_error, "Relative.*(also-relative)",
                         impl::check_requirements(metadata, no_config));
}

// -------------------------------------------------------------------------
// Tests for the require.machine metadata property.
// -------------------------------------------------------------------------

ATF_TEST_CASE(require_machine_one_ok);
ATF_TEST_CASE_HEAD(require_machine_one_ok) {}
ATF_TEST_CASE_BODY(require_machine_one_ok) {
    atf::tests::vars_map metadata;
    metadata["require.machine"] = atf::config::get("atf_machine");
    do_check("", metadata);
}

ATF_TEST_CASE(require_machine_one_fail);
ATF_TEST_CASE_HEAD(require_machine_one_fail) {}
ATF_TEST_CASE_BODY(require_machine_one_fail) {
    atf::tests::vars_map metadata;
    metadata["require.machine"] = "__fake_machine__";
    do_check("Requires the '__fake_machine__' machine type", metadata);
}

ATF_TEST_CASE(require_machine_many_ok);
ATF_TEST_CASE_HEAD(require_machine_many_ok) {}
ATF_TEST_CASE_BODY(require_machine_many_ok) {
    atf::tests::vars_map metadata;
    metadata["require.machine"] = "__foo__ " + atf::config::get("atf_machine") +
        " __bar__";
    do_check("", metadata);
}

ATF_TEST_CASE(require_machine_many_fail);
ATF_TEST_CASE_HEAD(require_machine_many_fail) {}
ATF_TEST_CASE_BODY(require_machine_many_fail) {
    atf::tests::vars_map metadata;
    metadata["require.machine"] = "__foo__ __bar__ __baz__";
    do_check("Requires one of the '__foo__ __bar__ __baz__' machine types",
             metadata);
}

// -------------------------------------------------------------------------
// Tests for the require.memory metadata property.
// -------------------------------------------------------------------------

ATF_TEST_CASE_WITHOUT_HEAD(require_memory_ok);
ATF_TEST_CASE_BODY(require_memory_ok) {
    atf::tests::vars_map metadata;
    metadata["require.memory"] = "1m";
    do_check("", metadata);
}

ATF_TEST_CASE_WITHOUT_HEAD(require_memory_not_enough);
ATF_TEST_CASE_BODY(require_memory_not_enough) {
    atf::tests::vars_map metadata;
    metadata["require.memory"] = "128t";
#if defined(__APPLE__) || defined(__DragonFly__) || \
    defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    do_check("Not enough memory; needed 140737488355328, available [0-9]*",
             metadata);
#else
    skip("Don't know how to check for the amount of physical memory");
#endif
}

ATF_TEST_CASE_WITHOUT_HEAD(require_memory_fail);
ATF_TEST_CASE_BODY(require_memory_fail) {
    atf::tests::vars_map metadata;
    metadata["require.memory"] = "foo";
    ATF_REQUIRE_THROW(std::runtime_error,
                      impl::check_requirements(metadata, no_config));
}

// -------------------------------------------------------------------------
// Tests for the require.progs metadata property.
// -------------------------------------------------------------------------

ATF_TEST_CASE(require_progs_one_ok);
ATF_TEST_CASE_HEAD(require_progs_one_ok) {}
ATF_TEST_CASE_BODY(require_progs_one_ok) {
    atf::tests::vars_map metadata;
    metadata["require.progs"] = "cp";
    do_check("", metadata);
}

ATF_TEST_CASE(require_progs_one_missing);
ATF_TEST_CASE_HEAD(require_progs_one_missing) {}
ATF_TEST_CASE_BODY(require_progs_one_missing) {
    atf::tests::vars_map metadata;
    metadata["require.progs"] = "cp __non-existent__";
    do_check("Required program '__non-existent__' not found in the PATH",
             metadata);
}

ATF_TEST_CASE(require_progs_one_fail);
ATF_TEST_CASE_HEAD(require_progs_one_fail) {}
ATF_TEST_CASE_BODY(require_progs_one_fail) {
    atf::tests::vars_map metadata;
    metadata["require.progs"] = "bin/cp";
    ATF_REQUIRE_THROW(std::runtime_error,
                    impl::check_requirements(metadata, no_config));
}

ATF_TEST_CASE(require_progs_many_ok);
ATF_TEST_CASE_HEAD(require_progs_many_ok) {}
ATF_TEST_CASE_BODY(require_progs_many_ok) {
    atf::tests::vars_map metadata;
    metadata["require.progs"] = "cp ls mv";
    do_check("", metadata);
}

ATF_TEST_CASE(require_progs_many_missing);
ATF_TEST_CASE_HEAD(require_progs_many_missing) {}
ATF_TEST_CASE_BODY(require_progs_many_missing) {
    atf::tests::vars_map metadata;
    metadata["require.progs"] = "mv ls __foo__ cp";
    do_check("Required program '__foo__' not found in the PATH", metadata);
}

ATF_TEST_CASE(require_progs_many_fail);
ATF_TEST_CASE_HEAD(require_progs_many_fail) {}
ATF_TEST_CASE_BODY(require_progs_many_fail) {
    atf::tests::vars_map metadata;
    metadata["require.progs"] = "ls cp ../bin/cp";
    ATF_REQUIRE_THROW(std::runtime_error,
                    impl::check_requirements(metadata, no_config));
}

// -------------------------------------------------------------------------
// Tests for the require.user metadata property.
// -------------------------------------------------------------------------

ATF_TEST_CASE(require_user_root);
ATF_TEST_CASE_HEAD(require_user_root) {}
ATF_TEST_CASE_BODY(require_user_root) {
    atf::tests::vars_map metadata;
    metadata["require.user"] = "root";
    if (atf::atf_run::is_root())
        do_check("", metadata);
    else
        do_check("Requires root privileges", metadata);
}

ATF_TEST_CASE(require_user_unprivileged);
ATF_TEST_CASE_HEAD(require_user_unprivileged) {}
ATF_TEST_CASE_BODY(require_user_unprivileged) {
    atf::tests::vars_map metadata;
    metadata["require.user"] = "unprivileged";
    if (atf::atf_run::is_root())
        do_check("Requires an unprivileged user and the 'unprivileged-user' "
                 "configuration variable is not set", metadata);
    else
        do_check("", metadata);
}

ATF_TEST_CASE(require_user_fail);
ATF_TEST_CASE_HEAD(require_user_fail) {}
ATF_TEST_CASE_BODY(require_user_fail) {
    atf::tests::vars_map metadata;
    metadata["require.user"] = "nobody";
    ATF_REQUIRE_THROW(std::runtime_error,
                    impl::check_requirements(metadata, no_config));
}

// -------------------------------------------------------------------------
// Main.
// -------------------------------------------------------------------------

ATF_INIT_TEST_CASES(tcs)
{
    // Add test cases for require.arch.
    ATF_ADD_TEST_CASE(tcs, require_arch_one_ok);
    ATF_ADD_TEST_CASE(tcs, require_arch_one_fail);
    ATF_ADD_TEST_CASE(tcs, require_arch_many_ok);
    ATF_ADD_TEST_CASE(tcs, require_arch_many_fail);

    // Add test cases for require.config.
    ATF_ADD_TEST_CASE(tcs, require_config_one_ok);
    ATF_ADD_TEST_CASE(tcs, require_config_one_fail);
    ATF_ADD_TEST_CASE(tcs, require_config_many_ok);
    ATF_ADD_TEST_CASE(tcs, require_config_many_fail);

    // Add test cases for require.files.
    ATF_ADD_TEST_CASE(tcs, require_files_one_ok);
    ATF_ADD_TEST_CASE(tcs, require_files_one_missing);
    ATF_ADD_TEST_CASE(tcs, require_files_one_fail);
    ATF_ADD_TEST_CASE(tcs, require_files_many_ok);
    ATF_ADD_TEST_CASE(tcs, require_files_many_missing);
    ATF_ADD_TEST_CASE(tcs, require_files_many_fail);

    // Add test cases for require.machine.
    ATF_ADD_TEST_CASE(tcs, require_machine_one_ok);
    ATF_ADD_TEST_CASE(tcs, require_machine_one_fail);
    ATF_ADD_TEST_CASE(tcs, require_machine_many_ok);
    ATF_ADD_TEST_CASE(tcs, require_machine_many_fail);

    // Add test cases for require.memory.
    ATF_ADD_TEST_CASE(tcs, require_memory_ok);
    ATF_ADD_TEST_CASE(tcs, require_memory_not_enough);
    ATF_ADD_TEST_CASE(tcs, require_memory_fail);

    // Add test cases for require.progs.
    ATF_ADD_TEST_CASE(tcs, require_progs_one_ok);
    ATF_ADD_TEST_CASE(tcs, require_progs_one_missing);
    ATF_ADD_TEST_CASE(tcs, require_progs_one_fail);
    ATF_ADD_TEST_CASE(tcs, require_progs_many_ok);
    ATF_ADD_TEST_CASE(tcs, require_progs_many_missing);
    ATF_ADD_TEST_CASE(tcs, require_progs_many_fail);

    // Add test cases for require.user.
    ATF_ADD_TEST_CASE(tcs, require_user_root);
    ATF_ADD_TEST_CASE(tcs, require_user_unprivileged);
    ATF_ADD_TEST_CASE(tcs, require_user_fail);
}
