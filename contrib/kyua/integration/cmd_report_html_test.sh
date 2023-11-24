# Copyright 2012 The Kyua Authors.
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
atf_test_program{name="simple_some_fail"}
atf_test_program{name="metadata"}
EOF

    utils_cp_helper simple_all_pass .
    utils_cp_helper simple_some_fail .
    utils_cp_helper metadata .
    atf_check -s exit:1 -o save:stdout -e empty env MOCK="${mock_env}" kyua test
    grep '^Results saved to ' stdout | cut -d ' ' -f 4 >"${dbfile_name}"
    rm stdout

    # Ensure the results of 'report-html' come from the database.
    rm Kyuafile simple_all_pass simple_some_fail metadata
}


# Ensure a file has a set of strings.
#
# \param file The name of the file to check.
# \param ... List of strings to check.
check_in_file() {
    local file="${1}"; shift

    while [ ${#} -gt 0 ]; do
        echo "Checking for presence of '${1}' in ${file}"
        if grep "${1}" "${file}" >/dev/null; then
            :
        else
            atf_fail "Test case output not found in HTML page ${file}"
        fi
        shift
    done
}


# Ensure a file does not have a set of strings.
#
# \param file The name of the file to check.
# \param ... List of strings to check.
check_not_in_file() {
    local file="${1}"; shift

    while [ ${#} -gt 0 ]; do
        echo "Checking for lack of '${1}' in ${file}"
        if grep "${1}" "${file}" >/dev/null; then
            atf_fail "Spurious test case output found in HTML page"
        fi
        shift
    done
}


utils_test_case default_behavior__ok
default_behavior__ok_body() {
    run_tests "mock1" unused_dbfile_name

    atf_check -s exit:0 -o ignore -e empty kyua report-html
    for f in \
        html/index.html \
        html/context.html \
        html/simple_all_pass_skip.html \
        html/simple_some_fail_fail.html
    do
        test -f "${f}" || atf_fail "Missing ${f}"
    done

    atf_check -o match:"2 TESTS FAILING" cat html/index.html

    check_in_file html/simple_all_pass_skip.html \
        "This is the stdout of skip" "This is the stderr of skip"
    check_not_in_file html/simple_all_pass_skip.html \
        "This is the stdout of pass" "This is the stderr of pass" \
        "This is the stdout of fail" "This is the stderr of fail" \
        "Test case did not write anything to"

    check_in_file html/simple_some_fail_fail.html \
        "This is the stdout of fail" "This is the stderr of fail"
    check_not_in_file html/simple_some_fail_fail.html \
        "This is the stdout of pass" "This is the stderr of pass" \
        "This is the stdout of skip" "This is the stderr of skip" \
        "Test case did not write anything to"

    check_in_file html/metadata_one_property.html \
        "description = Does nothing but has one metadata property"
    check_not_in_file html/metadata_one_property.html \
        "allowed_architectures = some-architecture"

    check_in_file html/metadata_many_properties.html \
        "allowed_architectures = some-architecture"
    check_not_in_file html/metadata_many_properties.html \
        "description = Does nothing but has one metadata property"
}


utils_test_case default_behavior__no_store
default_behavior__no_store_body() {
    echo 'kyua: E: No previous results file found for test suite' \
        "$(utils_test_suite_id)." >experr
    atf_check -s exit:2 -o empty -e file:experr kyua report-html
}


utils_test_case results_file__explicit
results_file__explicit_body() {
    run_tests "mock1" dbfile_name1
    run_tests "mock2" dbfile_name2

    atf_check -s exit:0 -o ignore -e empty kyua report-html \
        --results-file="$(cat dbfile_name1)"
    grep "MOCK.*mock1" html/context.html || atf_fail "Invalid context in report"

    rm -rf html
    atf_check -s exit:0 -o ignore -e empty kyua report-html \
        --results-file="$(cat dbfile_name2)"
    grep "MOCK.*mock2" html/context.html || atf_fail "Invalid context in report"
}


utils_test_case results_file__not_found
results_file__not_found_body() {
    atf_check -s exit:2 -o empty -e match:"kyua: E: No previous results.*foo" \
        kyua report-html --results-file=foo
}


utils_test_case force__yes
force__yes_body() {
    run_tests "mock1" unused_dbfile_name

    atf_check -s exit:0 -o ignore -e empty kyua report-html
    test -f html/index.html || atf_fail "Expected file not created"
    rm html/index.html
    atf_check -s exit:0 -o ignore -e empty kyua report-html --force
    test -f html/index.html || atf_fail "Expected file not created"
}


utils_test_case force__no
force__no_body() {
    run_tests "mock1" unused_dbfile_name

    atf_check -s exit:0 -o ignore -e empty kyua report-html
    test -f html/index.html || atf_fail "Expected file not created"
    rm html/index.html

cat >experr <<EOF
kyua: E: Output directory 'html' already exists; maybe use --force?.
EOF
    atf_check -s exit:2 -o empty -e file:experr kyua report-html
    test ! -f html/index.html || atf_fail "Not expected file created"
}


utils_test_case output__explicit
output__explicit_body() {
    run_tests "mock1" unused_dbfile_name

    mkdir output
    atf_check -s exit:0 -o ignore -e empty kyua report-html --output=output/foo
    test ! -d html || atf_fail "Not expected directory created"
    test -f output/foo/index.html || atf_fail "Expected file not created"
}


utils_test_case results_filter__ok
results_filter__ok_body() {
    run_tests "mock1" unused_dbfile_name

    atf_check -s exit:0 -o ignore -e empty kyua report-html \
        --results-filter=passed
    for f in \
        html/index.html \
        html/context.html \
        html/simple_all_pass_pass.html \
        html/simple_some_fail_pass.html \
        html/metadata_no_properties.html \
        html/metadata_with_cleanup.html
    do
        test -f "${f}" || atf_fail "Missing ${f}"
    done

    atf_check -o match:"2 TESTS FAILING" cat html/index.html

    check_in_file html/simple_all_pass_pass.html \
        "This is the stdout of pass" "This is the stderr of pass"
    check_not_in_file html/simple_all_pass_pass.html \
        "This is the stdout of skip" "This is the stderr of skip" \
        "This is the stdout of fail" "This is the stderr of fail" \
        "Test case did not write anything to"

    check_in_file html/simple_some_fail_pass.html \
        "Test case did not write anything to stdout" \
        "Test case did not write anything to stderr"
    check_not_in_file html/simple_some_fail_pass.html \
        "This is the stdout of pass" "This is the stderr of pass" \
        "This is the stdout of skip" "This is the stderr of skip" \
        "This is the stdout of fail" "This is the stderr of fail"
}


utils_test_case results_filter__invalid
results_filter__invalid_body() {
    echo "kyua: E: Unknown result type 'foo-bar'." >experr
    atf_check -s exit:2 -o empty -e file:experr kyua report-html \
        --results-filter=passed,foo-bar
}


atf_init_test_cases() {
    atf_add_test_case default_behavior__ok
    atf_add_test_case default_behavior__no_store

    atf_add_test_case results_file__explicit
    atf_add_test_case results_file__not_found

    atf_add_test_case force__yes
    atf_add_test_case force__no

    atf_add_test_case output__explicit

    atf_add_test_case results_filter__ok
    atf_add_test_case results_filter__invalid
}
