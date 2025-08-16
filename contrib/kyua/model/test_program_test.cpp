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

#include "model/test_program.hpp"

extern "C" {
#include <sys/stat.h>

#include <signal.h>
}

#include <set>
#include <sstream>

#include <atf-c++.hpp>

#include "model/exceptions.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/test_result.hpp"
#include "utils/env.hpp"
#include "utils/format/containers.ipp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"

namespace fs = utils::fs;


namespace {


/// Test program that sets its test cases lazily.
///
/// This test class exists to test the behavior of a test_program object when
/// the class is extended to offer lazy loading of test cases.  We simulate such
/// lazy loading here by storing the list of test cases aside at construction
/// time and later setting it lazily the first time test_cases() is called.
class lazy_test_program : public model::test_program {
    /// Whether set_test_cases() has yet been called or not.
    mutable bool _set_test_cases_called;

    /// The list of test cases for this test program.
    ///
    /// Only use this in the call to set_test_cases().  All other reads of the
    /// test cases list should happen via the parent class' test_cases() method.
    model::test_cases_map _lazy_test_cases;

public:
    /// Constructs a new test program.
    ///
    /// \param interface_name_ Name of the test program interface.
    /// \param binary_ The name of the test program binary relative to root_.
    /// \param root_ The root of the test suite containing the test program.
    /// \param test_suite_name_ The name of the test suite.
    /// \param metadata_ Metadata of the test program.
    /// \param test_cases_ The collection of test cases in the test program.
    lazy_test_program(const std::string& interface_name_,
                      const utils::fs::path& binary_,
                      const utils::fs::path& root_,
                      const std::string& test_suite_name_,
                      const model::metadata& metadata_,
                      const model::test_cases_map& test_cases_) :
        test_program(interface_name_, binary_, root_, test_suite_name_,
                     metadata_, model::test_cases_map()),
        _set_test_cases_called(false),
        _lazy_test_cases(test_cases_)
    {
    }

    /// Lazily sets the test cases on the parent and returns them.
    ///
    /// \return The list of test cases.
    const model::test_cases_map&
    test_cases(void) const
    {
        if (!_set_test_cases_called) {
            const_cast< lazy_test_program* >(this)->set_test_cases(
                _lazy_test_cases);
            _set_test_cases_called = true;
        }
        return test_program::test_cases();
    }
};


}  // anonymous namespace


/// Runs a ctor_and_getters test.
///
/// \tparam TestProgram Either model::test_program or lazy_test_program.
template< class TestProgram >
static void
check_ctor_and_getters(void)
{
    const model::metadata tp_md = model::metadata_builder()
        .add_custom("first", "foo")
        .add_custom("second", "bar")
        .build();
    const model::metadata tc_md = model::metadata_builder()
        .add_custom("first", "baz")
        .build();

    const TestProgram test_program(
        "mock", fs::path("binary"), fs::path("root"), "suite-name", tp_md,
        model::test_cases_map_builder().add("foo", tc_md).build());


    ATF_REQUIRE_EQ("mock", test_program.interface_name());
    ATF_REQUIRE_EQ(fs::path("binary"), test_program.relative_path());
    ATF_REQUIRE_EQ(fs::current_path() / "root/binary",
                   test_program.absolute_path());
    ATF_REQUIRE_EQ(fs::path("root"), test_program.root());
    ATF_REQUIRE_EQ("suite-name", test_program.test_suite_name());
    ATF_REQUIRE_EQ(tp_md, test_program.get_metadata());

    const model::metadata exp_tc_md = model::metadata_builder()
        .add_custom("first", "baz")
        .add_custom("second", "bar")
        .build();
    const model::test_cases_map exp_tcs = model::test_cases_map_builder()
        .add("foo", exp_tc_md)
        .build();
    ATF_REQUIRE_EQ(exp_tcs, test_program.test_cases());
}


ATF_TEST_CASE_WITHOUT_HEAD(ctor_and_getters);
ATF_TEST_CASE_BODY(ctor_and_getters)
{
    check_ctor_and_getters< model::test_program >();
}


ATF_TEST_CASE_WITHOUT_HEAD(derived__ctor_and_getters);
ATF_TEST_CASE_BODY(derived__ctor_and_getters)
{
    check_ctor_and_getters< lazy_test_program >();
}


/// Runs a find_ok test.
///
/// \tparam TestProgram Either model::test_program or lazy_test_program.
template< class TestProgram >
static void
check_find_ok(void)
{
    const model::test_case test_case("main", model::metadata_builder().build());

    const TestProgram test_program(
        "mock", fs::path("non-existent"), fs::path("."), "suite-name",
        model::metadata_builder().build(),
        model::test_cases_map_builder().add(test_case).build());

    const model::test_case& found_test_case = test_program.find("main");
    ATF_REQUIRE_EQ(test_case, found_test_case);
}


ATF_TEST_CASE_WITHOUT_HEAD(find__ok);
ATF_TEST_CASE_BODY(find__ok)
{
    check_find_ok< model::test_program >();
}


ATF_TEST_CASE_WITHOUT_HEAD(derived__find__ok);
ATF_TEST_CASE_BODY(derived__find__ok)
{
    check_find_ok< lazy_test_program >();
}


/// Runs a find_missing test.
///
/// \tparam TestProgram Either model::test_program or lazy_test_program.
template< class TestProgram >
static void
check_find_missing(void)
{
    const TestProgram test_program(
        "mock", fs::path("non-existent"), fs::path("."), "suite-name",
        model::metadata_builder().build(),
        model::test_cases_map_builder().add("main").build());

    ATF_REQUIRE_THROW_RE(model::not_found_error,
                         "case.*abc.*program.*non-existent",
                         test_program.find("abc"));
}


ATF_TEST_CASE_WITHOUT_HEAD(find__missing);
ATF_TEST_CASE_BODY(find__missing)
{
    check_find_missing< model::test_program >();
}


ATF_TEST_CASE_WITHOUT_HEAD(derived__find__missing);
ATF_TEST_CASE_BODY(derived__find__missing)
{
    check_find_missing< lazy_test_program >();
}


/// Runs a metadata_inheritance test.
///
/// \tparam TestProgram Either model::test_program or lazy_test_program.
template< class TestProgram >
static void
check_metadata_inheritance(void)
{
    const model::test_cases_map test_cases = model::test_cases_map_builder()
        .add("inherit-all")
        .add("inherit-some",
             model::metadata_builder()
             .set_description("Overriden description")
             .build())
        .add("inherit-none",
             model::metadata_builder()
             .add_allowed_architecture("overriden-arch")
             .add_allowed_platform("overriden-platform")
             .set_description("Overriden description")
             .build())
        .build();

    const model::metadata metadata = model::metadata_builder()
        .add_allowed_architecture("base-arch")
        .set_description("Base description")
        .build();
    const TestProgram test_program(
        "plain", fs::path("non-existent"), fs::path("."), "suite-name",
        metadata, test_cases);

    {
        const model::metadata exp_metadata = model::metadata_builder()
            .add_allowed_architecture("base-arch")
            .set_description("Base description")
            .build();
        ATF_REQUIRE_EQ(exp_metadata,
                       test_program.find("inherit-all").get_metadata());
    }

    {
        const model::metadata exp_metadata = model::metadata_builder()
            .add_allowed_architecture("base-arch")
            .set_description("Overriden description")
            .build();
        ATF_REQUIRE_EQ(exp_metadata,
                       test_program.find("inherit-some").get_metadata());
    }

    {
        const model::metadata exp_metadata = model::metadata_builder()
            .add_allowed_architecture("overriden-arch")
            .add_allowed_platform("overriden-platform")
            .set_description("Overriden description")
            .build();
        ATF_REQUIRE_EQ(exp_metadata,
                       test_program.find("inherit-none").get_metadata());
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(metadata_inheritance);
ATF_TEST_CASE_BODY(metadata_inheritance)
{
    check_metadata_inheritance< model::test_program >();
}


ATF_TEST_CASE_WITHOUT_HEAD(derived__metadata_inheritance);
ATF_TEST_CASE_BODY(derived__metadata_inheritance)
{
    check_metadata_inheritance< lazy_test_program >();
}


/// Runs a operators_eq_and_ne__copy test.
///
/// \tparam TestProgram Either model::test_program or lazy_test_program.
template< class TestProgram >
static void
check_operators_eq_and_ne__copy(void)
{
    const TestProgram tp1(
        "plain", fs::path("non-existent"), fs::path("."), "suite-name",
        model::metadata_builder().build(),
        model::test_cases_map());
    const TestProgram tp2 = tp1;
    ATF_REQUIRE(  tp1 == tp2);
    ATF_REQUIRE(!(tp1 != tp2));
}


ATF_TEST_CASE_WITHOUT_HEAD(operators_eq_and_ne__copy);
ATF_TEST_CASE_BODY(operators_eq_and_ne__copy)
{
    check_operators_eq_and_ne__copy< model::test_program >();
}


ATF_TEST_CASE_WITHOUT_HEAD(derived__operators_eq_and_ne__copy);
ATF_TEST_CASE_BODY(derived__operators_eq_and_ne__copy)
{
    check_operators_eq_and_ne__copy< lazy_test_program >();
}


/// Runs a operators_eq_and_ne__not_copy test.
///
/// \tparam TestProgram Either model::test_program or lazy_test_program.
template< class TestProgram >
static void
check_operators_eq_and_ne__not_copy(void)
{
    const std::string base_interface("plain");
    const fs::path base_relative_path("the/test/program");
    const fs::path base_root("/the/root");
    const std::string base_test_suite("suite-name");
    const model::metadata base_metadata = model::metadata_builder()
        .add_custom("foo", "bar")
        .build();

    const model::test_cases_map base_tcs = model::test_cases_map_builder()
        .add("main", model::metadata_builder()
             .add_custom("second", "baz")
             .build())
        .build();

    const TestProgram base_tp(
        base_interface, base_relative_path, base_root, base_test_suite,
        base_metadata, base_tcs);

    // Construct with all same values.
    {
        const model::test_cases_map other_tcs = model::test_cases_map_builder()
            .add("main", model::metadata_builder()
                 .add_custom("second", "baz")
                 .build())
            .build();

        const TestProgram other_tp(
            base_interface, base_relative_path, base_root, base_test_suite,
            base_metadata, other_tcs);

        ATF_REQUIRE(  base_tp == other_tp);
        ATF_REQUIRE(!(base_tp != other_tp));
    }

    // Construct with same final metadata values but using a different
    // intermediate representation.  The original test program has one property
    // in the base test program definition and another in the test case; here,
    // we put both definitions explicitly in the test case.
    {
        const model::test_cases_map other_tcs = model::test_cases_map_builder()
            .add("main", model::metadata_builder()
                 .add_custom("foo", "bar")
                 .add_custom("second", "baz")
                 .build())
            .build();

        const TestProgram other_tp(
            base_interface, base_relative_path, base_root, base_test_suite,
            base_metadata, other_tcs);

        ATF_REQUIRE(  base_tp == other_tp);
        ATF_REQUIRE(!(base_tp != other_tp));
    }

    // Different interface.
    {
        const TestProgram other_tp(
            "atf", base_relative_path, base_root, base_test_suite,
            base_metadata, base_tcs);

        ATF_REQUIRE(!(base_tp == other_tp));
        ATF_REQUIRE(  base_tp != other_tp);
    }

    // Different relative path.
    {
        const TestProgram other_tp(
            base_interface, fs::path("a/b/c"), base_root, base_test_suite,
            base_metadata, base_tcs);

        ATF_REQUIRE(!(base_tp == other_tp));
        ATF_REQUIRE(  base_tp != other_tp);
    }

    // Different root.
    {
        const TestProgram other_tp(
            base_interface, base_relative_path, fs::path("."), base_test_suite,
            base_metadata, base_tcs);

        ATF_REQUIRE(!(base_tp == other_tp));
        ATF_REQUIRE(  base_tp != other_tp);
    }

    // Different test suite.
    {
        const TestProgram other_tp(
            base_interface, base_relative_path, base_root, "different-suite",
            base_metadata, base_tcs);

        ATF_REQUIRE(!(base_tp == other_tp));
        ATF_REQUIRE(  base_tp != other_tp);
    }

    // Different metadata.
    {
        const TestProgram other_tp(
            base_interface, base_relative_path, base_root, base_test_suite,
            model::metadata_builder().build(), base_tcs);

        ATF_REQUIRE(!(base_tp == other_tp));
        ATF_REQUIRE(  base_tp != other_tp);
    }

    // Different test cases.
    {
        const model::test_cases_map other_tcs = model::test_cases_map_builder()
            .add("foo").build();

        const TestProgram other_tp(
            base_interface, base_relative_path, base_root, base_test_suite,
            base_metadata, other_tcs);

        ATF_REQUIRE(!(base_tp == other_tp));
        ATF_REQUIRE(  base_tp != other_tp);
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(operators_eq_and_ne__not_copy);
ATF_TEST_CASE_BODY(operators_eq_and_ne__not_copy)
{
    check_operators_eq_and_ne__not_copy< model::test_program >();
}


ATF_TEST_CASE_WITHOUT_HEAD(derived__operators_eq_and_ne__not_copy);
ATF_TEST_CASE_BODY(derived__operators_eq_and_ne__not_copy)
{
    check_operators_eq_and_ne__not_copy< lazy_test_program >();
}


/// Runs a operator_lt test.
///
/// \tparam TestProgram Either model::test_program or lazy_test_program.
template< class TestProgram >
static void
check_operator_lt(void)
{
    const TestProgram tp1(
        "plain", fs::path("a/b/c"), fs::path("/foo/bar"), "suite-name",
        model::metadata_builder().build(),
        model::test_cases_map());
    const TestProgram tp2(
        "atf", fs::path("c"), fs::path("/foo/bar"), "suite-name",
        model::metadata_builder().build(),
        model::test_cases_map());
    const TestProgram tp3(
        "plain", fs::path("a/b/c"), fs::path("/abc"), "suite-name",
        model::metadata_builder().build(),
        model::test_cases_map());

    ATF_REQUIRE(!(tp1 < tp1));

    ATF_REQUIRE(  tp1 < tp2);
    ATF_REQUIRE(!(tp2 < tp1));

    ATF_REQUIRE(!(tp1 < tp3));
    ATF_REQUIRE(  tp3 < tp1);

    // And now, test the actual reason why we want to have an < overload by
    // attempting to put the various programs in a set.
    std::set< TestProgram > programs;
    programs.insert(tp1);
    programs.insert(tp2);
    programs.insert(tp3);
}


ATF_TEST_CASE_WITHOUT_HEAD(operator_lt);
ATF_TEST_CASE_BODY(operator_lt)
{
    check_operator_lt< model::test_program >();
}


ATF_TEST_CASE_WITHOUT_HEAD(derived__operator_lt);
ATF_TEST_CASE_BODY(derived__operator_lt)
{
    check_operator_lt< lazy_test_program >();
}


/// Runs a output__no_test_cases test.
///
/// \tparam TestProgram Either model::test_program or lazy_test_program.
template< class TestProgram >
static void
check_output__no_test_cases(void)
{
    TestProgram tp(
        "plain", fs::path("binary/path"), fs::path("/the/root"), "suite-name",
        model::metadata_builder().add_allowed_architecture("a").build(),
        model::test_cases_map());

    std::ostringstream str;
    str << tp;
    ATF_REQUIRE_EQ(
        "test_program{interface='plain', binary='binary/path', "
        "root='/the/root', test_suite='suite-name', "
        "metadata=metadata{allowed_architectures='a', allowed_platforms='', "
        "description='', execenv='', execenv_jail_params='', "
        "has_cleanup='false', is_exclusive='false', "
        "required_configs='', required_disk_space='0', required_files='', "
        "required_kmods='', required_memory='0', "
        "required_programs='', required_user='', timeout='300'}, "
        "test_cases=map()}",
        str.str());
}


ATF_TEST_CASE_WITHOUT_HEAD(output__no_test_cases);
ATF_TEST_CASE_BODY(output__no_test_cases)
{
    check_output__no_test_cases< model::test_program >();
}


ATF_TEST_CASE_WITHOUT_HEAD(derived__output__no_test_cases);
ATF_TEST_CASE_BODY(derived__output__no_test_cases)
{
    check_output__no_test_cases< lazy_test_program >();
}


/// Runs a output__some_test_cases test.
///
/// \tparam TestProgram Either model::test_program or lazy_test_program.
template< class TestProgram >
static void
check_output__some_test_cases(void)
{
    const model::test_cases_map test_cases = model::test_cases_map_builder()
        .add("the-name", model::metadata_builder()
             .add_allowed_platform("foo")
             .add_custom("bar", "baz")
             .build())
        .add("another-name")
        .build();

    const TestProgram tp = TestProgram(
        "plain", fs::path("binary/path"), fs::path("/the/root"), "suite-name",
        model::metadata_builder().add_allowed_architecture("a").build(),
        test_cases);

    std::ostringstream str;
    str << tp;
    ATF_REQUIRE_EQ(
        "test_program{interface='plain', binary='binary/path', "
        "root='/the/root', test_suite='suite-name', "
        "metadata=metadata{allowed_architectures='a', allowed_platforms='', "
        "description='', execenv='', execenv_jail_params='', "
        "has_cleanup='false', is_exclusive='false', "
        "required_configs='', required_disk_space='0', required_files='', "
        "required_kmods='', required_memory='0', "
        "required_programs='', required_user='', timeout='300'}, "
        "test_cases=map("
        "another-name=test_case{name='another-name', "
        "metadata=metadata{allowed_architectures='a', allowed_platforms='', "
        "description='', execenv='', execenv_jail_params='', "
        "has_cleanup='false', is_exclusive='false', "
        "required_configs='', required_disk_space='0', required_files='', "
        "required_kmods='', required_memory='0', "
        "required_programs='', required_user='', timeout='300'}}, "
        "the-name=test_case{name='the-name', "
        "metadata=metadata{allowed_architectures='a', allowed_platforms='foo', "
        "custom.bar='baz', description='', execenv='', execenv_jail_params='', "
        "has_cleanup='false', is_exclusive='false', "
        "required_configs='', required_disk_space='0', required_files='', "
        "required_kmods='', required_memory='0', "
        "required_programs='', required_user='', timeout='300'}})}",
        str.str());
}


ATF_TEST_CASE_WITHOUT_HEAD(output__some_test_cases);
ATF_TEST_CASE_BODY(output__some_test_cases)
{
    check_output__some_test_cases< model::test_program >();
}


ATF_TEST_CASE_WITHOUT_HEAD(derived__output__some_test_cases);
ATF_TEST_CASE_BODY(derived__output__some_test_cases)
{
    check_output__some_test_cases< lazy_test_program >();
}


ATF_TEST_CASE_WITHOUT_HEAD(builder__defaults);
ATF_TEST_CASE_BODY(builder__defaults)
{
    const model::test_program expected(
        "mock", fs::path("non-existent"), fs::path("."), "suite-name",
        model::metadata_builder().build(), model::test_cases_map());

    const model::test_program built = model::test_program_builder(
        "mock", fs::path("non-existent"), fs::path("."), "suite-name")
        .build();

    ATF_REQUIRE_EQ(built, expected);
}


ATF_TEST_CASE_WITHOUT_HEAD(builder__overrides);
ATF_TEST_CASE_BODY(builder__overrides)
{
    const model::metadata md = model::metadata_builder()
        .add_custom("foo", "bar")
        .build();
    const model::test_cases_map tcs = model::test_cases_map_builder()
        .add("first")
        .add("second", md)
        .build();
    const model::test_program expected(
        "mock", fs::path("binary"), fs::path("root"), "suite-name", md, tcs);

    const model::test_program built = model::test_program_builder(
        "mock", fs::path("binary"), fs::path("root"), "suite-name")
        .add_test_case("first")
        .add_test_case("second", md)
        .set_metadata(md)
        .build();

    ATF_REQUIRE_EQ(built, expected);
}


ATF_TEST_CASE_WITHOUT_HEAD(builder__ptr);
ATF_TEST_CASE_BODY(builder__ptr)
{
    const model::test_program expected(
        "mock", fs::path("non-existent"), fs::path("."), "suite-name",
        model::metadata_builder().build(), model::test_cases_map());

    const model::test_program_ptr built = model::test_program_builder(
        "mock", fs::path("non-existent"), fs::path("."), "suite-name")
        .build_ptr();

    ATF_REQUIRE_EQ(*built, expected);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, ctor_and_getters);
    ATF_ADD_TEST_CASE(tcs, find__ok);
    ATF_ADD_TEST_CASE(tcs, find__missing);
    ATF_ADD_TEST_CASE(tcs, metadata_inheritance);
    ATF_ADD_TEST_CASE(tcs, operators_eq_and_ne__copy);
    ATF_ADD_TEST_CASE(tcs, operators_eq_and_ne__not_copy);
    ATF_ADD_TEST_CASE(tcs, operator_lt);
    ATF_ADD_TEST_CASE(tcs, output__no_test_cases);
    ATF_ADD_TEST_CASE(tcs, output__some_test_cases);

    ATF_ADD_TEST_CASE(tcs, derived__ctor_and_getters);
    ATF_ADD_TEST_CASE(tcs, derived__find__ok);
    ATF_ADD_TEST_CASE(tcs, derived__find__missing);
    ATF_ADD_TEST_CASE(tcs, derived__metadata_inheritance);
    ATF_ADD_TEST_CASE(tcs, derived__operators_eq_and_ne__copy);
    ATF_ADD_TEST_CASE(tcs, derived__operators_eq_and_ne__not_copy);
    ATF_ADD_TEST_CASE(tcs, derived__operator_lt);
    ATF_ADD_TEST_CASE(tcs, derived__output__no_test_cases);
    ATF_ADD_TEST_CASE(tcs, derived__output__some_test_cases);

    ATF_ADD_TEST_CASE(tcs, builder__defaults);
    ATF_ADD_TEST_CASE(tcs, builder__overrides);
    ATF_ADD_TEST_CASE(tcs, builder__ptr);
}
