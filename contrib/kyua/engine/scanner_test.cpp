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

#include "engine/scanner.hpp"

#include <cstdarg>
#include <cstddef>
#include <typeinfo>

#include <atf-c++.hpp>

#include "engine/filters.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "utils/format/containers.ipp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"

namespace fs = utils::fs;

using utils::optional;


namespace {


/// Test program that implements a mock test_cases() lazy call.
class mock_test_program : public model::test_program {
    /// Number of times test_cases has been called.
    mutable std::size_t _num_calls;

    /// Collection of test cases; lazily initialized.
    mutable model::test_cases_map _test_cases;

public:
    /// Constructs a new test program.
    ///
    /// \param binary_ The name of the test program binary relative to root_.
    mock_test_program(const fs::path& binary_) :
        test_program("unused-interface", binary_, fs::path("unused-root"),
                     "unused-suite", model::metadata_builder().build(),
                     model::test_cases_map()),
        _num_calls(0)
    {
    }

    /// Gets or loads the list of test cases from the test program.
    ///
    /// \return The list of test cases provided by the test program.
    const model::test_cases_map&
    test_cases(void) const
    {
        if (_num_calls == 0) {
            const model::metadata metadata = model::metadata_builder().build();
            const model::test_case tc1("one", metadata);
            const model::test_case tc2("two", metadata);
            _test_cases.insert(model::test_cases_map::value_type("one", tc1));
            _test_cases.insert(model::test_cases_map::value_type("two", tc2));
        }
        _num_calls++;
        return _test_cases;
    }

    /// Returns the number of times test_cases() has been called.
    ///
    /// \return A counter.
    std::size_t
    num_calls(void) const
    {
        return _num_calls;
    }
};


/// Syntactic sugar to instantiate a test program with various test cases.
///
/// The scanner only cares about the relative path of the test program object
/// and the names of the test cases.  This function helps in instantiating a
/// test program that has the minimum set of details only.
///
/// \param relative_path Relative path to the test program.
/// \param ... List of test case names to add to the test program.  Must be
///     NULL-terminated.
///
/// \return A constructed test program.
static model::test_program_ptr
new_test_program(const char* relative_path, ...)
{
    model::test_program_builder builder(
        "unused-interface", fs::path(relative_path), fs::path("unused-root"),
        "unused-suite");

    va_list ap;
    va_start(ap, relative_path);
    const char* test_case_name;
    while ((test_case_name = va_arg(ap, const char*)) != NULL) {
        builder.add_test_case(test_case_name);
    }
    va_end(ap);

    return builder.build_ptr();
}


/// Yields all test cases in the scanner for simplicity of testing.
///
/// In most of the tests below, we just care about the scanner returning the
/// full set of matching test cases, not the specific behavior of every single
/// yield() call.  This function just returns the whole set, which helps in
/// writing functional tests.
///
/// \param scanner The scanner on which to iterate.
///
/// \return The full collection of results yielded by the scanner.
static std::set< engine::scan_result >
yield_all(engine::scanner& scanner)
{
    std::set< engine::scan_result > results;
    while (!scanner.done()) {
        const optional< engine::scan_result > result = scanner.yield();
        ATF_REQUIRE(result);
        results.insert(result.get());
    }
    ATF_REQUIRE(!scanner.yield());
    ATF_REQUIRE(scanner.done());
    return results;
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(scanner__no_filters__no_tests);
ATF_TEST_CASE_BODY(scanner__no_filters__no_tests)
{
    const model::test_programs_vector test_programs;
    const std::set< engine::test_filter > filters;

    engine::scanner scanner(test_programs, filters);
    ATF_REQUIRE(scanner.done());
    ATF_REQUIRE(!scanner.yield());
    ATF_REQUIRE(scanner.unused_filters().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(scanner__no_filters__one_test_in_one_program);
ATF_TEST_CASE_BODY(scanner__no_filters__one_test_in_one_program)
{
    const model::test_program_ptr test_program = new_test_program(
        "dir/program", "lone_test", NULL);

    model::test_programs_vector test_programs;
    test_programs.push_back(test_program);

    const std::set< engine::test_filter > filters;

    std::set< engine::scan_result > exp_results;
    exp_results.insert(engine::scan_result(test_program, "lone_test"));

    engine::scanner scanner(test_programs, filters);
    const std::set< engine::scan_result > results = yield_all(scanner);
    ATF_REQUIRE_EQ(exp_results, results);
    ATF_REQUIRE(scanner.unused_filters().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(scanner__no_filters__one_test_per_many_programs);
ATF_TEST_CASE_BODY(scanner__no_filters__one_test_per_many_programs)
{
    const model::test_program_ptr test_program1 = new_test_program(
        "dir/program1", "foo_test", NULL);
    const model::test_program_ptr test_program2 = new_test_program(
        "program2", "bar_test", NULL);
    const model::test_program_ptr test_program3 = new_test_program(
        "a/b/c/d/e/program3", "baz_test", NULL);

    model::test_programs_vector test_programs;
    test_programs.push_back(test_program1);
    test_programs.push_back(test_program2);
    test_programs.push_back(test_program3);

    const std::set< engine::test_filter > filters;

    std::set< engine::scan_result > exp_results;
    exp_results.insert(engine::scan_result(test_program1, "foo_test"));
    exp_results.insert(engine::scan_result(test_program2, "bar_test"));
    exp_results.insert(engine::scan_result(test_program3, "baz_test"));

    engine::scanner scanner(test_programs, filters);
    const std::set< engine::scan_result > results = yield_all(scanner);
    ATF_REQUIRE_EQ(exp_results, results);
    ATF_REQUIRE(scanner.unused_filters().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(scanner__no_filters__many_tests_in_one_program);
ATF_TEST_CASE_BODY(scanner__no_filters__many_tests_in_one_program)
{
    const model::test_program_ptr test_program = new_test_program(
        "dir/program", "first_test", "second_test", "third_test", NULL);

    model::test_programs_vector test_programs;
    test_programs.push_back(test_program);

    const std::set< engine::test_filter > filters;

    std::set< engine::scan_result > exp_results;
    exp_results.insert(engine::scan_result(test_program, "first_test"));
    exp_results.insert(engine::scan_result(test_program, "second_test"));
    exp_results.insert(engine::scan_result(test_program, "third_test"));

    engine::scanner scanner(test_programs, filters);
    const std::set< engine::scan_result > results = yield_all(scanner);
    ATF_REQUIRE_EQ(exp_results, results);
    ATF_REQUIRE(scanner.unused_filters().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(scanner__no_filters__many_tests_per_many_programs);
ATF_TEST_CASE_BODY(scanner__no_filters__many_tests_per_many_programs)
{
    const model::test_program_ptr test_program1 = new_test_program(
        "dir/program1", "foo_test", "bar_test", "baz_test", NULL);
    const model::test_program_ptr test_program2 = new_test_program(
        "program2", "lone_test", NULL);
    const model::test_program_ptr test_program3 = new_test_program(
        "a/b/c/d/e/program3", "another_test", "last_test", NULL);

    model::test_programs_vector test_programs;
    test_programs.push_back(test_program1);
    test_programs.push_back(test_program2);
    test_programs.push_back(test_program3);

    const std::set< engine::test_filter > filters;

    std::set< engine::scan_result > exp_results;
    exp_results.insert(engine::scan_result(test_program1, "foo_test"));
    exp_results.insert(engine::scan_result(test_program1, "bar_test"));
    exp_results.insert(engine::scan_result(test_program1, "baz_test"));
    exp_results.insert(engine::scan_result(test_program2, "lone_test"));
    exp_results.insert(engine::scan_result(test_program3, "another_test"));
    exp_results.insert(engine::scan_result(test_program3, "last_test"));

    engine::scanner scanner(test_programs, filters);
    const std::set< engine::scan_result > results = yield_all(scanner);
    ATF_REQUIRE_EQ(exp_results, results);
    ATF_REQUIRE(scanner.unused_filters().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(scanner__no_filters__verify_lazy_loads);
ATF_TEST_CASE_BODY(scanner__no_filters__verify_lazy_loads)
{
    const model::test_program_ptr test_program1(new mock_test_program(
        fs::path("first")));
    const mock_test_program* mock_program1 =
        dynamic_cast< const mock_test_program* >(test_program1.get());
    const model::test_program_ptr test_program2(new mock_test_program(
        fs::path("second")));
    const mock_test_program* mock_program2 =
        dynamic_cast< const mock_test_program* >(test_program2.get());

    model::test_programs_vector test_programs;
    test_programs.push_back(test_program1);
    test_programs.push_back(test_program2);

    const std::set< engine::test_filter > filters;

    std::set< engine::scan_result > exp_results;
    exp_results.insert(engine::scan_result(test_program1, "one"));
    exp_results.insert(engine::scan_result(test_program1, "two"));
    exp_results.insert(engine::scan_result(test_program2, "one"));
    exp_results.insert(engine::scan_result(test_program2, "two"));

    engine::scanner scanner(test_programs, filters);
    std::set< engine::scan_result > results;
    ATF_REQUIRE_EQ(0, mock_program1->num_calls());
    ATF_REQUIRE_EQ(0, mock_program2->num_calls());

    // This abuses the internal implementation of the scanner by making
    // assumptions on the order of the results.
    results.insert(scanner.yield().get());
    ATF_REQUIRE_EQ(1, mock_program1->num_calls());
    ATF_REQUIRE_EQ(0, mock_program2->num_calls());
    results.insert(scanner.yield().get());
    ATF_REQUIRE_EQ(1, mock_program1->num_calls());
    ATF_REQUIRE_EQ(0, mock_program2->num_calls());
    results.insert(scanner.yield().get());
    ATF_REQUIRE_EQ(1, mock_program1->num_calls());
    ATF_REQUIRE_EQ(1, mock_program2->num_calls());
    results.insert(scanner.yield().get());
    ATF_REQUIRE_EQ(1, mock_program1->num_calls());
    ATF_REQUIRE_EQ(1, mock_program2->num_calls());
    ATF_REQUIRE(scanner.done());

    ATF_REQUIRE_EQ(exp_results, results);
    ATF_REQUIRE(scanner.unused_filters().empty());

    // Make sure we are still talking to the original objects.
    for (std::set< engine::scan_result >::const_iterator iter = results.begin();
         iter != results.end(); ++iter) {
        const mock_test_program* mock_program =
            dynamic_cast< const mock_test_program* >((*iter).first.get());
        ATF_REQUIRE_EQ(1, mock_program->num_calls());
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(scanner__with_filters__no_tests);
ATF_TEST_CASE_BODY(scanner__with_filters__no_tests)
{
    const model::test_programs_vector test_programs;

    std::set< engine::test_filter > filters;
    filters.insert(engine::test_filter(fs::path("foo"), "bar"));

    engine::scanner scanner(test_programs, filters);
    ATF_REQUIRE(scanner.done());
    ATF_REQUIRE(!scanner.yield());
    ATF_REQUIRE_EQ(filters, scanner.unused_filters());
}


ATF_TEST_CASE_WITHOUT_HEAD(scanner__with_filters__no_matches);
ATF_TEST_CASE_BODY(scanner__with_filters__no_matches)
{
    const model::test_program_ptr test_program1 = new_test_program(
        "dir/program1", "foo_test", "bar_test", "baz_test", NULL);
    const model::test_program_ptr test_program2 = new_test_program(
        "dir/program2", "bar_test", NULL);
    const model::test_program_ptr test_program3 = new_test_program(
        "program3", "another_test", "last_test", NULL);

    model::test_programs_vector test_programs;
    test_programs.push_back(test_program1);
    test_programs.push_back(test_program2);
    test_programs.push_back(test_program3);

    std::set< engine::test_filter > filters;
    filters.insert(engine::test_filter(fs::path("dir/program2"), "baz_test"));
    filters.insert(engine::test_filter(fs::path("program4"), "another_test"));
    filters.insert(engine::test_filter(fs::path("dir/program3"), ""));

    const std::set< engine::scan_result > exp_results;

    engine::scanner scanner(test_programs, filters);
    const std::set< engine::scan_result > results = yield_all(scanner);
    ATF_REQUIRE_EQ(exp_results, results);
    ATF_REQUIRE_EQ(filters, scanner.unused_filters());
}


ATF_TEST_CASE_WITHOUT_HEAD(scanner__with_filters__some_matches);
ATF_TEST_CASE_BODY(scanner__with_filters__some_matches)
{
    const model::test_program_ptr test_program1 = new_test_program(
        "dir/program1", "foo_test", "bar_test", "baz_test", NULL);
    const model::test_program_ptr test_program2 = new_test_program(
        "dir/program2", "bar_test", NULL);
    const model::test_program_ptr test_program3 = new_test_program(
        "program3", "another_test", "last_test", NULL);
    const model::test_program_ptr test_program4 = new_test_program(
        "program4", "more_test", NULL);

    model::test_programs_vector test_programs;
    test_programs.push_back(test_program1);
    test_programs.push_back(test_program2);
    test_programs.push_back(test_program3);
    test_programs.push_back(test_program4);

    std::set< engine::test_filter > filters;
    filters.insert(engine::test_filter(fs::path("dir/program1"), "baz_test"));
    filters.insert(engine::test_filter(fs::path("dir/program2"), "foo_test"));
    filters.insert(engine::test_filter(fs::path("program3"), ""));

    std::set< engine::test_filter > exp_filters;
    exp_filters.insert(engine::test_filter(fs::path("dir/program2"),
                                           "foo_test"));

    std::set< engine::scan_result > exp_results;
    exp_results.insert(engine::scan_result(test_program1, "baz_test"));
    exp_results.insert(engine::scan_result(test_program3, "another_test"));
    exp_results.insert(engine::scan_result(test_program3, "last_test"));

    engine::scanner scanner(test_programs, filters);
    const std::set< engine::scan_result > results = yield_all(scanner);
    ATF_REQUIRE_EQ(exp_results, results);

    ATF_REQUIRE_EQ(exp_filters, scanner.unused_filters());
}


ATF_TEST_CASE_WITHOUT_HEAD(scanner__with_filters__verify_lazy_loads);
ATF_TEST_CASE_BODY(scanner__with_filters__verify_lazy_loads)
{
    const model::test_program_ptr test_program1(new mock_test_program(
        fs::path("first")));
    const mock_test_program* mock_program1 =
        dynamic_cast< const mock_test_program* >(test_program1.get());
    const model::test_program_ptr test_program2(new mock_test_program(
        fs::path("second")));
    const mock_test_program* mock_program2 =
        dynamic_cast< const mock_test_program* >(test_program2.get());

    model::test_programs_vector test_programs;
    test_programs.push_back(test_program1);
    test_programs.push_back(test_program2);

    std::set< engine::test_filter > filters;
    filters.insert(engine::test_filter(fs::path("first"), ""));

    std::set< engine::scan_result > exp_results;
    exp_results.insert(engine::scan_result(test_program1, "one"));
    exp_results.insert(engine::scan_result(test_program1, "two"));

    engine::scanner scanner(test_programs, filters);
    std::set< engine::scan_result > results;
    ATF_REQUIRE_EQ(0, mock_program1->num_calls());
    ATF_REQUIRE_EQ(0, mock_program2->num_calls());

    results.insert(scanner.yield().get());
    ATF_REQUIRE_EQ(1, mock_program1->num_calls());
    ATF_REQUIRE_EQ(0, mock_program2->num_calls());
    results.insert(scanner.yield().get());
    ATF_REQUIRE_EQ(1, mock_program1->num_calls());
    ATF_REQUIRE_EQ(0, mock_program2->num_calls());
    ATF_REQUIRE(scanner.done());

    ATF_REQUIRE_EQ(exp_results, results);
    ATF_REQUIRE(scanner.unused_filters().empty());

    ATF_REQUIRE_EQ(1, mock_program1->num_calls());
    ATF_REQUIRE_EQ(0, mock_program2->num_calls());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, scanner__no_filters__no_tests);
    ATF_ADD_TEST_CASE(tcs, scanner__no_filters__one_test_in_one_program);
    ATF_ADD_TEST_CASE(tcs, scanner__no_filters__one_test_per_many_programs);
    ATF_ADD_TEST_CASE(tcs, scanner__no_filters__many_tests_in_one_program);
    ATF_ADD_TEST_CASE(tcs, scanner__no_filters__many_tests_per_many_programs);
    ATF_ADD_TEST_CASE(tcs, scanner__no_filters__verify_lazy_loads);

    ATF_ADD_TEST_CASE(tcs, scanner__with_filters__no_tests);
    ATF_ADD_TEST_CASE(tcs, scanner__with_filters__no_matches);
    ATF_ADD_TEST_CASE(tcs, scanner__with_filters__some_matches);
    ATF_ADD_TEST_CASE(tcs, scanner__with_filters__verify_lazy_loads);
}
