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

#include "engine/tap_parser.hpp"

#include <fstream>

#include <atf-c++.hpp>

#include "engine/exceptions.hpp"
#include "utils/format/containers.ipp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"

namespace fs = utils::fs;


namespace {


/// Helper to execute parse_tap_output() on inline text contents.
///
/// \param contents The TAP output to parse.
///
/// \return The tap_summary object resultingafter the parse.
///
/// \throw engine::load_error If parse_tap_output() fails.
static engine::tap_summary
do_parse(const std::string& contents)
{
    std::ofstream output("tap.txt");
    ATF_REQUIRE(output);
    output << contents;
    output.close();
    return engine::parse_tap_output(fs::path("tap.txt"));
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(tap_summary__bailed_out);
ATF_TEST_CASE_BODY(tap_summary__bailed_out)
{
    const engine::tap_summary summary = engine::tap_summary::new_bailed_out();
    ATF_REQUIRE(summary.bailed_out());
}


ATF_TEST_CASE_WITHOUT_HEAD(tap_summary__some_results);
ATF_TEST_CASE_BODY(tap_summary__some_results)
{
    const engine::tap_summary summary = engine::tap_summary::new_results(
        engine::tap_plan(1, 5), 3, 2);
    ATF_REQUIRE(!summary.bailed_out());
    ATF_REQUIRE_EQ(engine::tap_plan(1, 5), summary.plan());
    ATF_REQUIRE_EQ(3, summary.ok_count());
    ATF_REQUIRE_EQ(2, summary.not_ok_count());
}


ATF_TEST_CASE_WITHOUT_HEAD(tap_summary__all_skipped);
ATF_TEST_CASE_BODY(tap_summary__all_skipped)
{
    const engine::tap_summary summary = engine::tap_summary::new_all_skipped(
        "Skipped");
    ATF_REQUIRE(!summary.bailed_out());
    ATF_REQUIRE_EQ(engine::tap_plan(1, 0), summary.plan());
    ATF_REQUIRE_EQ("Skipped", summary.all_skipped_reason());
}


ATF_TEST_CASE_WITHOUT_HEAD(tap_summary__equality_operators);
ATF_TEST_CASE_BODY(tap_summary__equality_operators)
{
    const engine::tap_summary bailed_out =
        engine::tap_summary::new_bailed_out();
    const engine::tap_summary all_skipped_1 =
        engine::tap_summary::new_all_skipped("Reason 1");
    const engine::tap_summary results_1 =
        engine::tap_summary::new_results(engine::tap_plan(1, 5), 3, 2);

    // Self-equality checks.
    ATF_REQUIRE(  bailed_out == bailed_out);
    ATF_REQUIRE(!(bailed_out != bailed_out));
    ATF_REQUIRE(  all_skipped_1 == all_skipped_1);
    ATF_REQUIRE(!(all_skipped_1 != all_skipped_1));
    ATF_REQUIRE(  results_1 == results_1);
    ATF_REQUIRE(!(results_1 != results_1));

    // Cross-equality checks.
    ATF_REQUIRE(!(bailed_out == all_skipped_1));
    ATF_REQUIRE(  bailed_out != all_skipped_1);
    ATF_REQUIRE(!(bailed_out == results_1));
    ATF_REQUIRE(  bailed_out != results_1);
    ATF_REQUIRE(!(all_skipped_1 == results_1));
    ATF_REQUIRE(  all_skipped_1 != results_1);

    // Checks for the all_skipped "type".
    const engine::tap_summary all_skipped_2 =
        engine::tap_summary::new_all_skipped("Reason 2");
    ATF_REQUIRE(!(all_skipped_1 == all_skipped_2));
    ATF_REQUIRE(  all_skipped_1 != all_skipped_2);


    // Checks for the results "type", different plan.
    const engine::tap_summary results_2 =
        engine::tap_summary::new_results(engine::tap_plan(2, 6),
                                         results_1.ok_count(),
                                         results_1.not_ok_count());
    ATF_REQUIRE(!(results_1 == results_2));
    ATF_REQUIRE(  results_1 != results_2);


    // Checks for the results "type", different counts.
    const engine::tap_summary results_3 =
        engine::tap_summary::new_results(results_1.plan(),
                                         results_1.not_ok_count(),
                                         results_1.ok_count());
    ATF_REQUIRE(!(results_1 == results_3));
    ATF_REQUIRE(  results_1 != results_3);
}


ATF_TEST_CASE_WITHOUT_HEAD(tap_summary__output);
ATF_TEST_CASE_BODY(tap_summary__output)
{
    {
        const engine::tap_summary summary =
            engine::tap_summary::new_bailed_out();
        ATF_REQUIRE_EQ(
            "tap_summary{bailed_out=true}",
            (F("%s") % summary).str());
    }

    {
        const engine::tap_summary summary =
            engine::tap_summary::new_results(engine::tap_plan(5, 10), 2, 4);
        ATF_REQUIRE_EQ(
            "tap_summary{bailed_out=false, plan=5..10, ok_count=2, "
            "not_ok_count=4}",
            (F("%s") % summary).str());
    }

    {
        const engine::tap_summary summary =
            engine::tap_summary::new_all_skipped("Who knows");
        ATF_REQUIRE_EQ(
            "tap_summary{bailed_out=false, plan=1..0, "
            "all_skipped_reason=Who knows}",
            (F("%s") % summary).str());
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_tap_output__only_one_result);
ATF_TEST_CASE_BODY(parse_tap_output__only_one_result)
{
    const engine::tap_summary summary = do_parse(
        "1..1\n"
        "ok - 1\n");

    const engine::tap_summary exp_summary =
        engine::tap_summary::new_results(engine::tap_plan(1, 1), 1, 0);
    ATF_REQUIRE_EQ(exp_summary, summary);
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_tap_output__all_pass);
ATF_TEST_CASE_BODY(parse_tap_output__all_pass)
{
    const engine::tap_summary summary = do_parse(
        "1..8\n"
        "ok - 1\n"
        "    Some diagnostic message\n"
        "ok - 2 This test also passed\n"
        "garbage line\n"
        "ok - 3 This test passed\n"
        "not ok 4 # SKIP Some reason\n"
        "not ok 5 # TODO Another reason\n"
        "ok - 6 Doesn't make a difference SKIP\n"
        "ok - 7 Doesn't make a difference either TODO\n"
        "ok # Also works without a number\n");

    const engine::tap_summary exp_summary =
        engine::tap_summary::new_results(engine::tap_plan(1, 8), 8, 0);
    ATF_REQUIRE_EQ(exp_summary, summary);
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_tap_output__some_fail);
ATF_TEST_CASE_BODY(parse_tap_output__some_fail)
{
    const engine::tap_summary summary = do_parse(
        "garbage line\n"
        "not ok - 1 This test failed\n"
        "ok - 2 This test passed\n"
        "not ok - 3 This test failed\n"
        "1..6\n"
        "not ok - 4 This test failed\n"
        "ok - 5 This test passed\n"
        "not ok # Fails as well without a number\n");

    const engine::tap_summary exp_summary =
        engine::tap_summary::new_results(engine::tap_plan(1, 6), 2, 4);
    ATF_REQUIRE_EQ(exp_summary, summary);
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_tap_output__skip_and_todo_variants);
ATF_TEST_CASE_BODY(parse_tap_output__skip_and_todo_variants)
{
    const engine::tap_summary summary = do_parse(
        "1..8\n"
        "not ok - 1 # SKIP Some reason\n"
        "not ok - 2 # skip Some reason\n"
        "not ok - 3 # Skipped Some reason\n"
        "not ok - 4 # skipped Some reason\n"
        "not ok - 5 # Skipped: Some reason\n"
        "not ok - 6 # skipped: Some reason\n"
        "not ok - 7 # TODO Some reason\n"
        "not ok - 8 # todo Some reason\n");

    const engine::tap_summary exp_summary =
        engine::tap_summary::new_results(engine::tap_plan(1, 8), 8, 0);
    ATF_REQUIRE_EQ(exp_summary, summary);
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_tap_output__skip_all_with_reason);
ATF_TEST_CASE_BODY(parse_tap_output__skip_all_with_reason)
{
    const engine::tap_summary summary = do_parse(
        "1..0 SKIP Some reason for skipping\n"
        "ok - 1\n"
        "    Some diagnostic message\n"
        "ok - 6 Doesn't make a difference SKIP\n"
        "ok - 7 Doesn't make a difference either TODO\n");

    const engine::tap_summary exp_summary =
        engine::tap_summary::new_all_skipped("Some reason for skipping");
    ATF_REQUIRE_EQ(exp_summary, summary);
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_tap_output__skip_all_without_reason);
ATF_TEST_CASE_BODY(parse_tap_output__skip_all_without_reason)
{
    const engine::tap_summary summary = do_parse(
        "1..0 unrecognized # garbage skip\n");

    const engine::tap_summary exp_summary =
        engine::tap_summary::new_all_skipped("No reason specified");
    ATF_REQUIRE_EQ(exp_summary, summary);
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_tap_output__skip_all_invalid);
ATF_TEST_CASE_BODY(parse_tap_output__skip_all_invalid)
{
    ATF_REQUIRE_THROW_RE(engine::load_error,
                         "Skipped plan must be 1\\.\\.0",
                         do_parse("1..3 # skip\n"));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_tap_output__plan_at_end);
ATF_TEST_CASE_BODY(parse_tap_output__plan_at_end)
{
    const engine::tap_summary summary = do_parse(
        "ok - 1\n"
        "    Some diagnostic message\n"
        "ok - 2 This test also passed\n"
        "garbage line\n"
        "ok - 3 This test passed\n"
        "not ok 4 # SKIP Some reason\n"
        "not ok 5 # TODO Another reason\n"
        "ok - 6 Doesn't make a difference SKIP\n"
        "ok - 7 Doesn't make a difference either TODO\n"
        "1..7\n");

    const engine::tap_summary exp_summary =
        engine::tap_summary::new_results(engine::tap_plan(1, 7), 7, 0);
    ATF_REQUIRE_EQ(exp_summary, summary);
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_tap_output__stray_oks);
ATF_TEST_CASE_BODY(parse_tap_output__stray_oks)
{
    const engine::tap_summary summary = do_parse(
        "1..3\n"
        "ok - 1\n"
        "ok\n"
        "ok - 2 This test also passed\n"
        "not ok\n"
        "ok - 3 This test passed\n");

    const engine::tap_summary exp_summary =
        engine::tap_summary::new_results(engine::tap_plan(1, 3), 3, 0);
    ATF_REQUIRE_EQ(exp_summary, summary);
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_tap_output__no_plan);
ATF_TEST_CASE_BODY(parse_tap_output__no_plan)
{
    ATF_REQUIRE_THROW_RE(
        engine::load_error,
        "Output did not contain any TAP plan",
        do_parse(
            "not ok - 1 This test failed\n"
            "ok - 2 This test passed\n"));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_tap_output__double_plan);
ATF_TEST_CASE_BODY(parse_tap_output__double_plan)
{
    ATF_REQUIRE_THROW_RE(
        engine::load_error,
        "Found duplicate plan",
        do_parse(
            "garbage line\n"
            "1..5\n"
            "not ok - 1 This test failed\n"
            "ok - 2 This test passed\n"
            "1..8\n"
            "ok\n"));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_tap_output__inconsistent_plan);
ATF_TEST_CASE_BODY(parse_tap_output__inconsistent_plan)
{
    ATF_REQUIRE_THROW_RE(
        engine::load_error,
        "Reported plan differs from actual executed tests",
        do_parse(
            "1..3\n"
            "not ok - 1 This test failed\n"
            "ok - 2 This test passed\n"));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_tap_output__inconsistent_trailing_plan);
ATF_TEST_CASE_BODY(parse_tap_output__inconsistent_trailing_plan)
{
    ATF_REQUIRE_THROW_RE(
        engine::load_error,
        "Reported plan differs from actual executed tests",
        do_parse(
            "not ok - 1 This test failed\n"
            "ok - 2 This test passed\n"
            "1..3\n"));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_tap_output__insane_plan);
ATF_TEST_CASE_BODY(parse_tap_output__insane_plan)
{
    ATF_REQUIRE_THROW_RE(
        engine::load_error, "Invalid value",
        do_parse("120830981209831..234891793874080981092803981092312\n"));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_tap_output__reversed_plan);
ATF_TEST_CASE_BODY(parse_tap_output__reversed_plan)
{
    ATF_REQUIRE_THROW_RE(engine::load_error,
                         "Found reversed plan 8\\.\\.5",
                         do_parse("8..5\n"));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_tap_output__bail_out);
ATF_TEST_CASE_BODY(parse_tap_output__bail_out)
{
    const engine::tap_summary summary = do_parse(
        "1..3\n"
        "not ok - 1 This test failed\n"
        "Bail out! There is some unknown problem\n"
        "ok - 2 This test passed\n");

    const engine::tap_summary exp_summary =
        engine::tap_summary::new_bailed_out();
    ATF_REQUIRE_EQ(exp_summary, summary);
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_tap_output__bail_out_wins_over_no_plan);
ATF_TEST_CASE_BODY(parse_tap_output__bail_out_wins_over_no_plan)
{
    const engine::tap_summary summary = do_parse(
        "not ok - 1 This test failed\n"
        "Bail out! There is some unknown problem\n"
        "ok - 2 This test passed\n");

    const engine::tap_summary exp_summary =
        engine::tap_summary::new_bailed_out();
    ATF_REQUIRE_EQ(exp_summary, summary);
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_tap_output__open_failure);
ATF_TEST_CASE_BODY(parse_tap_output__open_failure)
{
    ATF_REQUIRE_THROW_RE(engine::load_error, "hello.txt.*Failed to open",
                         engine::parse_tap_output(fs::path("hello.txt")));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, tap_summary__bailed_out);
    ATF_ADD_TEST_CASE(tcs, tap_summary__some_results);
    ATF_ADD_TEST_CASE(tcs, tap_summary__all_skipped);
    ATF_ADD_TEST_CASE(tcs, tap_summary__equality_operators);
    ATF_ADD_TEST_CASE(tcs, tap_summary__output);

    ATF_ADD_TEST_CASE(tcs, parse_tap_output__only_one_result);
    ATF_ADD_TEST_CASE(tcs, parse_tap_output__all_pass);
    ATF_ADD_TEST_CASE(tcs, parse_tap_output__some_fail);
    ATF_ADD_TEST_CASE(tcs, parse_tap_output__skip_and_todo_variants);
    ATF_ADD_TEST_CASE(tcs, parse_tap_output__skip_all_without_reason);
    ATF_ADD_TEST_CASE(tcs, parse_tap_output__skip_all_with_reason);
    ATF_ADD_TEST_CASE(tcs, parse_tap_output__skip_all_invalid);
    ATF_ADD_TEST_CASE(tcs, parse_tap_output__plan_at_end);
    ATF_ADD_TEST_CASE(tcs, parse_tap_output__stray_oks);
    ATF_ADD_TEST_CASE(tcs, parse_tap_output__no_plan);
    ATF_ADD_TEST_CASE(tcs, parse_tap_output__double_plan);
    ATF_ADD_TEST_CASE(tcs, parse_tap_output__inconsistent_plan);
    ATF_ADD_TEST_CASE(tcs, parse_tap_output__inconsistent_trailing_plan);
    ATF_ADD_TEST_CASE(tcs, parse_tap_output__insane_plan);
    ATF_ADD_TEST_CASE(tcs, parse_tap_output__reversed_plan);
    ATF_ADD_TEST_CASE(tcs, parse_tap_output__bail_out);
    ATF_ADD_TEST_CASE(tcs, parse_tap_output__bail_out_wins_over_no_plan);
    ATF_ADD_TEST_CASE(tcs, parse_tap_output__open_failure);
}
