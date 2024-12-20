# Copyright 2011 The Kyua Authors.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
# * Neither the name of Google Inc. nor the names of its contributors
#   may be used to endorse or promote products derived from this software
#   without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


# Executes a mock test suite to generate data in the database.
#
# \param mock_env The value to store in a MOCK variable in the environment.
#     Use this to be able to differentiate executions by inspecting the
#     context of the output.
# \param dbfile_name File to which to write the path to the generated database
#     file.
run_tests() {
    local mock_env="${1}"; shift
    local dbfile_name="${1}"; shift

    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="simple_all_pass"}
EOF

    utils_cp_helper simple_all_pass .
    atf_check -s exit:0 -o save:stdout -e empty env \
        MOCK="${mock_env}" _='fake-value' kyua test
    grep '^Results saved to ' stdout | cut -d ' ' -f 4 >"${dbfile_name}"
    rm stdout

    # Ensure the results of 'report' come from the database.
    rm Kyuafile simple_all_pass
}


utils_test_case default_behavior__ok
default_behavior__ok_body() {
    utils_install_times_wrapper

    run_tests "mock1" dbfile_name1

    cat >expout <<EOF
===> Skipped tests
simple_all_pass:skip  ->  skipped: The reason for skipping is this  [S.UUUs]
===> Summary
Results read from $(cat dbfile_name1)
Test cases: 2 total, 1 skipped, 0 expected failures, 0 broken, 0 failed
Total time: S.UUUs
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua report

    run_tests "mock2" dbfile_name2

    cat >expout <<EOF
===> Skipped tests
simple_all_pass:skip  ->  skipped: The reason for skipping is this  [S.UUUs]
===> Summary
Results read from $(cat dbfile_name2)
Test cases: 2 total, 1 skipped, 0 expected failures, 0 broken, 0 failed
Total time: S.UUUs
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua report
}


utils_test_case default_behavior__no_store
default_behavior__no_store_body() {
    echo 'kyua: E: No previous results file found for test suite' \
        "$(utils_test_suite_id)." >experr
    atf_check -s exit:2 -o empty -e file:experr kyua report
}


utils_test_case results_file__explicit
results_file__explicit_body() {
    run_tests "mock1" dbfile_name1
    run_tests "mock2" dbfile_name2

    atf_check -s exit:0 -o match:"MOCK=mock1" -o not-match:"MOCK=mock2" \
        -e empty kyua report --results-file="$(cat dbfile_name1)" \
        --verbose
    atf_check -s exit:0 -o not-match:"MOCK=mock1" -o match:"MOCK=mock2" \
        -e empty kyua report --results-file="$(cat dbfile_name2)" \
        --verbose
}


utils_test_case results_file__not_found
results_file__not_found_body() {
    atf_check -s exit:2 -o empty -e match:"kyua: E: No previous results.*foo" \
        kyua report --results-file=foo
}


utils_test_case output__explicit
output__explicit_body() {
    run_tests unused_mock dbfile_name

    cat >report <<EOF
===> Skipped tests
simple_all_pass:skip  ->  skipped: The reason for skipping is this  [S.UUUs]
===> Summary
Results read from $(cat dbfile_name)
Test cases: 2 total, 1 skipped, 0 expected failures, 0 broken, 0 failed
Total time: S.UUUs
EOF

    atf_check -s exit:0 -o file:report -e empty -x kyua report \
        --output=/dev/stdout "| ${utils_strip_times_but_not_ids}"
    atf_check -s exit:0 -o empty -e save:stderr kyua report \
        --output=/dev/stderr
    atf_check -s exit:0 -o file:report -x cat stderr \
        "| ${utils_strip_times_but_not_ids}"

    atf_check -s exit:0 -o empty -e empty kyua report \
        --output=my-file
    atf_check -s exit:0 -o file:report -x cat my-file \
        "| ${utils_strip_times_but_not_ids}"
}


utils_test_case filter__ok
filter__ok_body() {
    utils_install_times_wrapper

    run_tests "mock1" dbfile_name1

    cat >expout <<EOF
===> Skipped tests
simple_all_pass:skip  ->  skipped: The reason for skipping is this  [S.UUUs]
===> Summary
Results read from $(cat dbfile_name1)
Test cases: 1 total, 1 skipped, 0 expected failures, 0 broken, 0 failed
Total time: S.UUUs
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua report \
        simple_all_pass:skip
}


utils_test_case filter__ok_passed_excluded_by_default
filter__ok_passed_excluded_by_default_body() {
    utils_install_times_wrapper

    run_tests "mock1" dbfile_name1

    # Passed results are excluded by default so they are not displayed even if
    # requested with a test case filter.  This might be somewhat confusing...
    cat >expout <<EOF
===> Summary
Results read from $(cat dbfile_name1)
Test cases: 1 total, 0 skipped, 0 expected failures, 0 broken, 0 failed
Total time: S.UUUs
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua report \
        simple_all_pass:pass
    cat >expout <<EOF
===> Passed tests
simple_all_pass:pass  ->  passed  [S.UUUs]
===> Summary
Results read from $(cat dbfile_name1)
Test cases: 1 total, 0 skipped, 0 expected failures, 0 broken, 0 failed
Total time: S.UUUs
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua report \
        --results-filter= simple_all_pass:pass
}


utils_test_case filter__no_match
filter__no_match_body() {
    utils_install_times_wrapper

    run_tests "mock1" dbfile_name1

    cat >expout <<EOF
===> Skipped tests
simple_all_pass:skip  ->  skipped: The reason for skipping is this  [S.UUUs]
===> Summary
Results read from $(cat dbfile_name1)
Test cases: 1 total, 1 skipped, 0 expected failures, 0 broken, 0 failed
Total time: S.UUUs
EOF
    cat >experr <<EOF
kyua: W: No test cases matched by the filter 'first'.
kyua: W: No test cases matched by the filter 'simple_all_pass:second'.
EOF
    atf_check -s exit:1 -o file:expout -e file:experr kyua report \
        first simple_all_pass:skip simple_all_pass:second
}


utils_test_case verbose
verbose_body() {
    # Switch to the current directory using its physical location and update
    # HOME accordingly.  Otherwise, the test below where we compare the value
    # of HOME in the output might fail if the path to HOME contains a symlink
    # (as is the case in OS X when HOME points to the temporary directory.)
    local real_cwd="$(pwd -P)"
    cd "${real_cwd}"
    HOME="${real_cwd}"

    run_tests "mock1
has multiple lines
and terminates here" dbfile_name

    cat >expout <<EOF
===> Execution context
Current directory: ${real_cwd}
Environment variables:
EOF
    # $_ is a bash variable.  To keep our tests stable, we override its value
    # below to match the hardcoded value in run_tests.
    env \
        HOME="${real_cwd}" \
        MOCK="mock1
has multiple lines
and terminates here" \
        _='fake-value' \
        "$(atf_get_srcdir)/helpers/dump_env" '    ' '        ' >>expout
    cat >>expout <<EOF
===> simple_all_pass:skip
Result:     skipped: The reason for skipping is this
Start time: YYYY-MM-DDTHH:MM:SS.ssssssZ
End time:   YYYY-MM-DDTHH:MM:SS.ssssssZ
Duration:   S.UUUs

Metadata:
    allowed_architectures is empty
    allowed_platforms is empty
    description is empty
    execenv is empty
    execenv_jail_params is empty
    has_cleanup = false
    is_exclusive = false
    required_configs is empty
    required_disk_space = 0
    required_files is empty
    required_memory = 0
    required_programs is empty
    required_user is empty
    timeout = 300

Standard output:
This is the stdout of skip

Standard error:
This is the stderr of skip
===> Skipped tests
simple_all_pass:skip  ->  skipped: The reason for skipping is this  [S.UUUs]
===> Summary
Results read from $(cat dbfile_name)
Test cases: 2 total, 1 skipped, 0 expected failures, 0 broken, 0 failed
Start time: YYYY-MM-DDTHH:MM:SS.ssssssZ
End time:   YYYY-MM-DDTHH:MM:SS.ssssssZ
Total time: S.UUUs
EOF
    atf_check -s exit:0 -o file:expout -e empty -x kyua report --verbose \
        "| ${utils_strip_times_but_not_ids}"
}


utils_test_case results_filter__empty
results_filter__empty_body() {
    utils_install_times_wrapper

    run_tests "mock1" dbfile_name1

    cat >expout <<EOF
===> Passed tests
simple_all_pass:pass  ->  passed  [S.UUUs]
===> Skipped tests
simple_all_pass:skip  ->  skipped: The reason for skipping is this  [S.UUUs]
===> Summary
Results read from $(cat dbfile_name1)
Test cases: 2 total, 1 skipped, 0 expected failures, 0 broken, 0 failed
Total time: S.UUUs
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua report --results-filter=
}


utils_test_case results_filter__one
results_filter__one_body() {
    utils_install_times_wrapper

    run_tests "mock1" dbfile_name1

    cat >expout <<EOF
===> Passed tests
simple_all_pass:pass  ->  passed  [S.UUUs]
===> Summary
Results read from $(cat dbfile_name1)
Test cases: 2 total, 1 skipped, 0 expected failures, 0 broken, 0 failed
Total time: S.UUUs
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua report \
        --results-filter=passed
}


utils_test_case results_filter__multiple_all_match
results_filter__multiple_all_match_body() {
    utils_install_times_wrapper

    run_tests "mock1" dbfile_name1

    cat >expout <<EOF
===> Skipped tests
simple_all_pass:skip  ->  skipped: The reason for skipping is this  [S.UUUs]
===> Passed tests
simple_all_pass:pass  ->  passed  [S.UUUs]
===> Summary
Results read from $(cat dbfile_name1)
Test cases: 2 total, 1 skipped, 0 expected failures, 0 broken, 0 failed
Total time: S.UUUs
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua report \
        --results-filter=skipped,passed
}


utils_test_case results_filter__multiple_some_match
results_filter__multiple_some_match_body() {
    utils_install_times_wrapper

    run_tests "mock1" dbfile_name1

    cat >expout <<EOF
===> Skipped tests
simple_all_pass:skip  ->  skipped: The reason for skipping is this  [S.UUUs]
===> Summary
Results read from $(cat dbfile_name1)
Test cases: 2 total, 1 skipped, 0 expected failures, 0 broken, 0 failed
Total time: S.UUUs
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua report \
        --results-filter=skipped,xfail,broken,failed
}


atf_init_test_cases() {
    atf_add_test_case default_behavior__ok
    atf_add_test_case default_behavior__no_store

    atf_add_test_case results_file__explicit
    atf_add_test_case results_file__not_found

    atf_add_test_case filter__ok
    atf_add_test_case filter__ok_passed_excluded_by_default
    atf_add_test_case filter__no_match

    atf_add_test_case verbose

    atf_add_test_case output__explicit

    atf_add_test_case results_filter__empty
    atf_add_test_case results_filter__one
    atf_add_test_case results_filter__multiple_all_match
    atf_add_test_case results_filter__multiple_some_match
}
