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


utils_test_case one_test_program__all_pass
one_test_program__all_pass_body() {
    utils_install_stable_test_wrapper

    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="simple_all_pass"}
EOF

    cat >expout <<EOF
simple_all_pass:pass  ->  passed  [S.UUUs]
simple_all_pass:skip  ->  skipped: The reason for skipping is this  [S.UUUs]

Results file id is $(utils_results_id)
Results saved to $(utils_results_file)

1/2 passed (0 broken, 0 failed, 1 skipped)
EOF

    utils_cp_helper simple_all_pass .
    atf_check -s exit:0 -o file:expout -e empty kyua test
}


utils_test_case one_test_program__some_fail
one_test_program__some_fail_body() {
    utils_install_stable_test_wrapper

    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="simple_some_fail"}
EOF

    cat >expout <<EOF
simple_some_fail:fail  ->  failed: This fails on purpose  [S.UUUs]
simple_some_fail:pass  ->  passed  [S.UUUs]

Results file id is $(utils_results_id)
Results saved to $(utils_results_file)

1/2 passed (0 broken, 1 failed, 0 skipped)
EOF

    utils_cp_helper simple_some_fail .
    atf_check -s exit:1 -o file:expout -e empty kyua test
}


utils_test_case many_test_programs__all_pass
many_test_programs__all_pass_body() {
    utils_install_stable_test_wrapper

    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="first"}
atf_test_program{name="second"}
atf_test_program{name="third"}
plain_test_program{name="fourth", required_files="/non-existent/foo"}
EOF

    cat >expout <<EOF
first:pass  ->  passed  [S.UUUs]
first:skip  ->  skipped: The reason for skipping is this  [S.UUUs]
fourth:main  ->  skipped: Required file '/non-existent/foo' not found  [S.UUUs]
second:pass  ->  passed  [S.UUUs]
second:skip  ->  skipped: The reason for skipping is this  [S.UUUs]
third:pass  ->  passed  [S.UUUs]
third:skip  ->  skipped: The reason for skipping is this  [S.UUUs]

Results file id is $(utils_results_id)
Results saved to $(utils_results_file)

3/7 passed (0 broken, 0 failed, 4 skipped)
EOF

    utils_cp_helper simple_all_pass first
    utils_cp_helper simple_all_pass second
    utils_cp_helper simple_all_pass third
    echo "not executed" >fourth; chmod +x fourth
    atf_check -s exit:0 -o file:expout -e empty kyua test
}


utils_test_case many_test_programs__some_fail
many_test_programs__some_fail_body() {
    utils_install_stable_test_wrapper

    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="first"}
atf_test_program{name="second"}
atf_test_program{name="third"}
plain_test_program{name="fourth"}
EOF

    cat >expout <<EOF
first:fail  ->  failed: This fails on purpose  [S.UUUs]
first:pass  ->  passed  [S.UUUs]
fourth:main  ->  failed: Returned non-success exit status 76  [S.UUUs]
second:fail  ->  failed: This fails on purpose  [S.UUUs]
second:pass  ->  passed  [S.UUUs]
third:pass  ->  passed  [S.UUUs]
third:skip  ->  skipped: The reason for skipping is this  [S.UUUs]

Results file id is $(utils_results_id)
Results saved to $(utils_results_file)

3/7 passed (0 broken, 3 failed, 1 skipped)
EOF

    utils_cp_helper simple_some_fail first
    utils_cp_helper simple_some_fail second
    utils_cp_helper simple_all_pass third
    echo '#! /bin/sh' >fourth
    echo 'exit 76' >>fourth
    chmod +x fourth
    atf_check -s exit:1 -o file:expout -e empty kyua test
}


utils_test_case expect__all_pass
expect__all_pass_body() {
    utils_install_stable_test_wrapper

    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="expect_all_pass"}
EOF

# CHECK_STYLE_DISABLE
    cat >expout <<EOF
expect_all_pass:die  ->  expected_failure: This is the reason for death  [S.UUUs]
expect_all_pass:exit  ->  expected_failure: Exiting with correct code  [S.UUUs]
expect_all_pass:failure  ->  expected_failure: Oh no: Forced failure  [S.UUUs]
expect_all_pass:signal  ->  expected_failure: Exiting with correct signal  [S.UUUs]
expect_all_pass:timeout  ->  expected_failure: This times out  [S.UUUs]

Results file id is $(utils_results_id)
Results saved to $(utils_results_file)

5/5 passed (0 broken, 0 failed, 0 skipped)
EOF
# CHECK_STYLE_ENABLE

    utils_cp_helper expect_all_pass .
    atf_check -s exit:0 -o file:expout -e empty kyua test
}


utils_test_case expect__some_fail
expect__some_fail_body() {
    utils_install_stable_test_wrapper

    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="expect_some_fail"}
EOF

# CHECK_STYLE_DISABLE
    cat >expout <<EOF
expect_some_fail:die  ->  failed: Test case was expected to terminate abruptly but it continued execution  [S.UUUs]
expect_some_fail:exit  ->  failed: Test case expected to exit with code 12 but got code 34  [S.UUUs]
expect_some_fail:failure  ->  failed: Test case was expecting a failure but none were raised  [S.UUUs]
expect_some_fail:pass  ->  passed  [S.UUUs]
expect_some_fail:signal  ->  failed: Test case expected to receive signal 15 but got 9  [S.UUUs]
expect_some_fail:timeout  ->  failed: Test case was expected to hang but it continued execution  [S.UUUs]

Results file id is $(utils_results_id)
Results saved to $(utils_results_file)

1/6 passed (0 broken, 5 failed, 0 skipped)
EOF
# CHECK_STYLE_ENABLE

    utils_cp_helper expect_some_fail .
    atf_check -s exit:1 -o file:expout -e empty kyua test
}


utils_test_case premature_exit
premature_exit_body() {
    utils_install_stable_test_wrapper

    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="bogus_test_cases"}
EOF

# CHECK_STYLE_DISABLE
    cat >expout <<EOF
bogus_test_cases:die  ->  broken: Premature exit; test case received signal 9  [S.UUUs]
bogus_test_cases:exit  ->  broken: Premature exit; test case exited with code 0  [S.UUUs]
bogus_test_cases:pass  ->  passed  [S.UUUs]

Results file id is $(utils_results_id)
Results saved to $(utils_results_file)

1/3 passed (2 broken, 0 failed, 0 skipped)
EOF
# CHECK_STYLE_ENABLE

    utils_cp_helper bogus_test_cases .
    atf_check -s exit:1 -o file:expout -e empty kyua test
}


utils_test_case no_args
no_args_body() {
    utils_install_stable_test_wrapper

    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="simple_all_pass"}
include("subdir/Kyuafile")
EOF
    utils_cp_helper metadata .
    utils_cp_helper simple_all_pass .

    mkdir subdir
    cat >subdir/Kyuafile <<EOF
syntax(2)
test_suite("integration2")
atf_test_program{name="simple_some_fail"}
EOF
    utils_cp_helper simple_some_fail subdir

    cat >expout <<EOF
simple_all_pass:pass  ->  passed  [S.UUUs]
simple_all_pass:skip  ->  skipped: The reason for skipping is this  [S.UUUs]
subdir/simple_some_fail:fail  ->  failed: This fails on purpose  [S.UUUs]
subdir/simple_some_fail:pass  ->  passed  [S.UUUs]

Results file id is $(utils_results_id)
Results saved to $(utils_results_file)

2/4 passed (0 broken, 1 failed, 1 skipped)
EOF
    atf_check -s exit:1 -o file:expout -e empty kyua test
}


utils_test_case one_arg__subdir
one_arg__subdir_body() {
    utils_install_stable_test_wrapper

    cat >Kyuafile <<EOF
syntax(2)
test_suite("top-level")
include("subdir/Kyuafile")
EOF

    mkdir subdir
    cat >subdir/Kyuafile <<EOF
syntax(2)
test_suite("in-subdir")
atf_test_program{name="simple_all_pass"}
EOF
    utils_cp_helper simple_all_pass subdir

# CHECK_STYLE_DISABLE
    cat >expout <<EOF
subdir/simple_all_pass:pass  ->  passed  [S.UUUs]
subdir/simple_all_pass:skip  ->  skipped: The reason for skipping is this  [S.UUUs]

Results file id is $(utils_results_id)
Results saved to $(utils_results_file)

1/2 passed (0 broken, 0 failed, 1 skipped)
EOF
# CHECK_STYLE_ENABLE
    atf_check -s exit:0 -o file:expout -e empty kyua test subdir
}


utils_test_case one_arg__test_case
one_arg__test_case_body() {
    utils_install_stable_test_wrapper

    cat >Kyuafile <<EOF
syntax(2)
test_suite("top-level")
atf_test_program{name="first"}
atf_test_program{name="second"}
EOF
    utils_cp_helper simple_all_pass first
    utils_cp_helper simple_all_pass second

    cat >expout <<EOF
first:skip  ->  skipped: The reason for skipping is this  [S.UUUs]

Results file id is $(utils_results_id)
Results saved to $(utils_results_file)

0/1 passed (0 broken, 0 failed, 1 skipped)
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua test first:skip
}


utils_test_case one_arg__test_program
one_arg__test_program_body() {
    utils_install_stable_test_wrapper

    cat >Kyuafile <<EOF
syntax(2)
test_suite("top-level")
atf_test_program{name="first"}
atf_test_program{name="second"}
EOF
    utils_cp_helper simple_all_pass first
    utils_cp_helper simple_some_fail second

    cat >expout <<EOF
second:fail  ->  failed: This fails on purpose  [S.UUUs]
second:pass  ->  passed  [S.UUUs]

Results file id is $(utils_results_id)
Results saved to $(utils_results_file)

1/2 passed (0 broken, 1 failed, 0 skipped)
EOF
    atf_check -s exit:1 -o file:expout -e empty kyua test second
}


utils_test_case one_arg__invalid
one_arg__invalid_body() {
cat >experr <<EOF
kyua: E: Test case component in 'foo:' is empty.
EOF
    atf_check -s exit:2 -o empty -e file:experr kyua test foo:

cat >experr <<EOF
kyua: E: Program name '/a/b' must be relative to the test suite, not absolute.
EOF
    atf_check -s exit:2 -o empty -e file:experr kyua test /a/b
}


utils_test_case many_args__ok
many_args__ok_body() {
    utils_install_stable_test_wrapper

    cat >Kyuafile <<EOF
syntax(2)
test_suite("top-level")
include("subdir/Kyuafile")
atf_test_program{name="first"}
EOF
    utils_cp_helper simple_all_pass first

    mkdir subdir
    cat >subdir/Kyuafile <<EOF
syntax(2)
test_suite("in-subdir")
atf_test_program{name="second"}
EOF
    utils_cp_helper simple_some_fail subdir/second

    cat >expout <<EOF
first:pass  ->  passed  [S.UUUs]
subdir/second:fail  ->  failed: This fails on purpose  [S.UUUs]
subdir/second:pass  ->  passed  [S.UUUs]

Results file id is $(utils_results_id)
Results saved to $(utils_results_file)

2/3 passed (0 broken, 1 failed, 0 skipped)
EOF
    atf_check -s exit:1 -o file:expout -e empty kyua test subdir first:pass
}


utils_test_case many_args__invalid
many_args__invalid_body() {
cat >experr <<EOF
kyua: E: Program name component in ':badbad' is empty.
EOF
    atf_check -s exit:2 -o empty -e file:experr kyua test this-is-ok :badbad

cat >experr <<EOF
kyua: E: Program name '/foo' must be relative to the test suite, not absolute.
EOF
    atf_check -s exit:2 -o empty -e file:experr kyua test this-is-ok /foo
}


utils_test_case many_args__no_match__all
many_args__no_match__all_body() {
    utils_install_stable_test_wrapper

    cat >Kyuafile <<EOF
syntax(2)
test_suite("top-level")
atf_test_program{name="first"}
atf_test_program{name="second"}
EOF
    utils_cp_helper simple_all_pass first
    utils_cp_helper simple_all_pass second

    cat >expout <<EOF
Results file id is $(utils_results_id)
Results saved to $(utils_results_file)
EOF
    cat >experr <<EOF
kyua: W: No test cases matched by the filter 'first1'.
EOF
    atf_check -s exit:1 -o file:expout -e file:experr kyua test first1
}


utils_test_case many_args__no_match__some
many_args__no_match__some_body() {
    utils_install_stable_test_wrapper

    cat >Kyuafile <<EOF
syntax(2)
test_suite("top-level")
atf_test_program{name="first"}
atf_test_program{name="second"}
atf_test_program{name="third"}
EOF
    utils_cp_helper simple_all_pass first
    utils_cp_helper simple_all_pass second
    utils_cp_helper simple_some_fail third

    cat >expout <<EOF
first:pass  ->  passed  [S.UUUs]
first:skip  ->  skipped: The reason for skipping is this  [S.UUUs]
third:fail  ->  failed: This fails on purpose  [S.UUUs]
third:pass  ->  passed  [S.UUUs]

Results file id is $(utils_results_id)
Results saved to $(utils_results_file)

2/4 passed (0 broken, 1 failed, 1 skipped)
EOF

    cat >experr <<EOF
kyua: W: No test cases matched by the filter 'fifth'.
kyua: W: No test cases matched by the filter 'fourth'.
EOF
    atf_check -s exit:1 -o file:expout -e file:experr kyua test first fourth \
        third fifth
}


utils_test_case args_are_relative
args_are_relative_body() {
    utils_install_stable_test_wrapper

    mkdir root
    cat >root/Kyuafile <<EOF
syntax(2)
test_suite("integration-1")
atf_test_program{name="first"}
atf_test_program{name="second"}
include("subdir/Kyuafile")
EOF
    utils_cp_helper simple_all_pass root/first
    utils_cp_helper simple_some_fail root/second

    mkdir root/subdir
    cat >root/subdir/Kyuafile <<EOF
syntax(2)
test_suite("integration-2")
atf_test_program{name="third"}
atf_test_program{name="fourth"}
EOF
    utils_cp_helper simple_all_pass root/subdir/third
    utils_cp_helper simple_some_fail root/subdir/fourth

    cat >expout <<EOF
first:pass  ->  passed  [S.UUUs]
first:skip  ->  skipped: The reason for skipping is this  [S.UUUs]
subdir/fourth:fail  ->  failed: This fails on purpose  [S.UUUs]

Results file id is $(utils_results_id root)
Results saved to $(utils_results_file root)

1/3 passed (0 broken, 1 failed, 1 skipped)
EOF
    atf_check -s exit:1 -o file:expout -e empty kyua test \
        -k "$(pwd)/root/Kyuafile" first subdir/fourth:fail
}


utils_test_case only_load_used_test_programs
only_load_used_test_programs_body() {
    utils_install_stable_test_wrapper

    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="first"}
atf_test_program{name="second"}
EOF
    utils_cp_helper simple_all_pass first
    utils_cp_helper bad_test_program second

    cat >expout <<EOF
first:pass  ->  passed  [S.UUUs]
first:skip  ->  skipped: The reason for skipping is this  [S.UUUs]

Results file id is $(utils_results_id)
Results saved to $(utils_results_file)

1/2 passed (0 broken, 0 failed, 1 skipped)
EOF
    CREATE_COOKIE="$(pwd)/cookie"; export CREATE_COOKIE
    atf_check -s exit:0 -o file:expout -e empty kyua test first
    if [ -f "${CREATE_COOKIE}" ]; then
        atf_fail "An unmatched test case has been executed, which harms" \
            "performance"
    fi
}


utils_test_case config_behavior
config_behavior_body() {
    cat >"my-config" <<EOF
syntax(2)
test_suites.suite1["the-variable"] = "value1"
test_suites.suite2["the-variable"] = "override me"
EOF

    cat >Kyuafile <<EOF
syntax(2)
atf_test_program{name="config1", test_suite="suite1"}
atf_test_program{name="config2", test_suite="suite2"}
atf_test_program{name="config3", test_suite="suite3"}
EOF
    utils_cp_helper config config1
    utils_cp_helper config config2
    utils_cp_helper config config3

    atf_check -s exit:1 -o save:stdout -e empty \
        kyua -c my-config -v test_suites.suite2.the-variable=value2 test
    atf_check -s exit:0 -o ignore -e empty \
        grep 'config1:get_variable.*failed' stdout
    atf_check -s exit:0 -o ignore -e empty \
        grep 'config2:get_variable.*passed' stdout
    atf_check -s exit:0 -o ignore -e empty \
        grep 'config3:get_variable.*skipped' stdout

    CONFIG_VAR_FILE="$(pwd)/cookie"; export CONFIG_VAR_FILE
    if [ -f "${CONFIG_VAR_FILE}" ]; then
        atf_fail "Cookie file already created; test case list may have gotten" \
            "a bad configuration"
    fi
    atf_check -s exit:1 -o ignore -e empty kyua -c my-config test config1
    [ -f "${CONFIG_VAR_FILE}" ] || \
        atf_fail "Cookie file not created; test case list did not get" \
            "configuration variables"
    value="$(cat "${CONFIG_VAR_FILE}")"
    [ "${value}" = "value1" ] || \
        atf_fail "Invalid value (${value}) in cookie file; test case list did" \
            "not get the correct configuration variables"
}


utils_test_case config_unprivileged_user
config_unprivileged_user_body() {
    cat >"my-config" <<EOF
syntax(2)
unprivileged_user = "nobody"
EOF
    cat >Kyuafile <<EOF
syntax(2)
atf_test_program{name="config1", test_suite="suite1"}
EOF
    utils_cp_helper config config1

    CONFIG_VAR_FILE="$(pwd)/cookie"; export CONFIG_VAR_FILE
    if [ -f "${CONFIG_VAR_FILE}" ]; then
        atf_fail "Cookie file already created; test case list may have gotten" \
            "a bad configuration"
    fi

    CONFIG_VAR_NAME="unprivileged-user"; export CONFIG_VAR_NAME
    atf_check -s exit:1 -o ignore -e ignore kyua -c my-config test config1
    [ -f "${CONFIG_VAR_FILE}" ] || \
        atf_fail "Cookie file not created; test case list did not get" \
            "configuration variables"
    value="$(cat "${CONFIG_VAR_FILE}")"
    [ "${value}" = "nobody" ] || \
        atf_fail "Invalid value (${value}) in cookie file; test case list did" \
            "not get the correct configuration variables"

    rm "${CONFIG_VAR_FILE}"

    CONFIG_VAR_NAME="unprivileged_user"; export CONFIG_VAR_NAME
    atf_check -s exit:1 -o ignore -e ignore kyua -c my-config test config1
    [ -f "${CONFIG_VAR_FILE}" ] || \
        atf_fail "Cookie file not created; test case list did not get" \
            "configuration variables"
    value="$(cat "${CONFIG_VAR_FILE}")"
    [ "${value}" = "nobody" ] || \
        atf_fail "Invalid value (${value}) in cookie file; test case list did" \
            "not get the correct configuration variables"
}


utils_test_case store_contents
store_contents_body() {
    utils_install_stable_test_wrapper

    cat >Kyuafile <<EOF
syntax(2)
atf_test_program{name="some-program", test_suite="suite1"}
EOF
    utils_cp_helper simple_some_fail some-program
    cat >expout <<EOF
some-program:fail  ->  failed: This fails on purpose  [S.UUUs]
some-program:pass  ->  passed  [S.UUUs]

Results file id is $(utils_results_id)
Results saved to $(utils_results_file)

1/2 passed (0 broken, 1 failed, 0 skipped)
EOF

    atf_check -s exit:1 -o file:expout -e empty kyua test

cat >expout <<EOF
some-program,fail,failed,This fails on purpose
some-program,pass,passed,NULL
EOF
    atf_check -s exit:0 -o file:expout -e empty \
        kyua db-exec --no-headers \
        "SELECT " \
        "       test_programs.relative_path, test_cases.name, " \
        "       test_results.result_type, test_results.result_reason " \
        "FROM test_programs " \
        "     JOIN test_cases " \
        "     ON test_programs.test_program_id = test_cases.test_program_id " \
        "     JOIN test_results " \
        "     ON test_cases.test_case_id = test_results.test_case_id " \
        "ORDER BY test_programs.relative_path, test_cases.name"
}


utils_test_case results_file__ok
results_file__ok_body() {
    cat >Kyuafile <<EOF
syntax(2)
atf_test_program{name="config1", test_suite="suite1"}
EOF
    utils_cp_helper config config1

    atf_check -s exit:0 -o ignore -e empty kyua test -r foo1.db
   test -f foo1.db || atf_fail "-s did not work"
    atf_check -s exit:0 -o ignore -e empty kyua test --results-file=foo2.db
    test -f foo2.db || atf_fail "--results-file did not work"
    test ! -f .kyua/store.db || atf_fail "Default database created"
}


utils_test_case results_file__fail
results_file__fail_body() {
    cat >Kyuafile <<EOF
syntax(2)
atf_test_program{name="config1", test_suite="suite1"}
EOF
    utils_cp_helper config config1

    atf_check -s exit:3 -o empty -e match:"Invalid.*--results-file" \
        kyua test --results-file=
}


utils_test_case results_file__reuse
results_file__reuse_body() {
    utils_install_stable_test_wrapper

    cat >Kyuafile <<EOF
syntax(2)
atf_test_program{name="simple_all_pass", test_suite="integration"}
EOF
    utils_cp_helper simple_all_pass .
    atf_check -s exit:0 -o ignore -e empty kyua test -r results.db

    atf_check -s exit:2 -o empty -e match:"results.db already exists" \
        kyua test --results-file="results.db"
}


utils_test_case build_root_flag
build_root_flag_body() {
    utils_install_stable_test_wrapper

    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="first"}
include("subdir/Kyuafile")
EOF

    mkdir subdir
    cat >subdir/Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="second"}
atf_test_program{name="third"}
EOF

    cat >expout <<EOF
first:pass  ->  passed  [S.UUUs]
first:skip  ->  skipped: The reason for skipping is this  [S.UUUs]
subdir/second:pass  ->  passed  [S.UUUs]
subdir/second:skip  ->  skipped: The reason for skipping is this  [S.UUUs]
subdir/third:pass  ->  passed  [S.UUUs]
subdir/third:skip  ->  skipped: The reason for skipping is this  [S.UUUs]

Results file id is $(utils_results_id)
Results saved to $(utils_results_file)

3/6 passed (0 broken, 0 failed, 3 skipped)
EOF

    mkdir build
    mkdir build/subdir
    utils_cp_helper simple_all_pass build/first
    utils_cp_helper simple_all_pass build/subdir/second
    utils_cp_helper simple_all_pass build/subdir/third

    atf_check -s exit:0 -o file:expout -e empty kyua test --build-root=build
}


utils_test_case kyuafile_flag__no_args
kyuafile_flag__no_args_body() {
    utils_install_stable_test_wrapper

    cat >Kyuafile <<EOF
This file is bogus but it is not loaded.
EOF

    cat >myfile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="sometest"}
EOF
    utils_cp_helper simple_all_pass sometest

    cat >expout <<EOF
sometest:pass  ->  passed  [S.UUUs]
sometest:skip  ->  skipped: The reason for skipping is this  [S.UUUs]

Results file id is $(utils_results_id)
Results saved to $(utils_results_file)

1/2 passed (0 broken, 0 failed, 1 skipped)
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua test -k myfile
    atf_check -s exit:0 -o file:expout -e empty kyua test --kyuafile=myfile
}


utils_test_case kyuafile_flag__some_args
kyuafile_flag__some_args_body() {
    utils_install_stable_test_wrapper

    cat >Kyuafile <<EOF
This file is bogus but it is not loaded.
EOF

    cat >myfile <<EOF
syntax(2)
test_suite("hello-world")
atf_test_program{name="sometest"}
EOF
    utils_cp_helper simple_all_pass sometest

    cat >expout <<EOF
sometest:pass  ->  passed  [S.UUUs]
sometest:skip  ->  skipped: The reason for skipping is this  [S.UUUs]

Results file id is $(utils_results_id)
Results saved to $(utils_results_file)

1/2 passed (0 broken, 0 failed, 1 skipped)
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua test -k myfile sometest
    cat >expout <<EOF
sometest:pass  ->  passed  [S.UUUs]
sometest:skip  ->  skipped: The reason for skipping is this  [S.UUUs]

Results file id is $(utils_results_id)
Results saved to $(utils_results_file)

1/2 passed (0 broken, 0 failed, 1 skipped)
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua test --kyuafile=myfile \
        sometest
}


utils_test_case interrupt
interrupt_body() {
    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="interrupts"}
EOF
    utils_cp_helper interrupts .

    kyua \
        -v test_suites.integration.body-cookie="$(pwd)/body" \
        -v test_suites.integration.cleanup-cookie="$(pwd)/cleanup" \
        test >stdout 2>stderr &
    pid=${!}
    echo "Kyua subprocess is PID ${pid}"

    while [ ! -f body ]; do
        echo "Waiting for body to start"
        sleep 1
    done
    echo "Body started"
    sleep 1

    echo "Sending INT signal to ${pid}"
    kill -INT ${pid}
    echo "Waiting for process ${pid} to exit"
    wait ${pid}
    ret=${?}
    sed -e 's,^,kyua stdout:,' stdout
    sed -e 's,^,kyua stderr:,' stderr
    echo "Process ${pid} exited"
    [ ${ret} -ne 0 ] || atf_fail 'No error code reported'

    [ -f cleanup ] || atf_fail 'Cleanup part not executed after signal'
    atf_expect_pass

    atf_check -s exit:0 -o ignore -e empty grep 'Signal caught' stderr
    atf_check -s exit:0 -o ignore -e empty \
        grep 'kyua: E: Interrupted by signal' stderr
}


utils_test_case exclusive_tests
exclusive_tests_body() {
    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
EOF
    for i in $(seq 100); do
        echo 'plain_test_program{name="race", is_exclusive=true}' >>Kyuafile
    done
    utils_cp_helper race .

    atf_check \
        -s exit:0 \
        -o match:"100/100 passed" \
        kyua \
        -v parallelism=20 \
        -v test_suites.integration.shared_file="$(pwd)/shared_file" \
        test
}


utils_test_case no_test_program_match
no_test_program_match_body() {
    utils_install_stable_test_wrapper

    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="first"}
EOF
    utils_cp_helper simple_all_pass first
    utils_cp_helper simple_all_pass second

    cat >expout <<EOF
Results file id is $(utils_results_id)
Results saved to $(utils_results_file)
EOF
    cat >experr <<EOF
kyua: W: No test cases matched by the filter 'second'.
EOF
    atf_check -s exit:1 -o file:expout -e file:experr kyua test second
}


utils_test_case no_test_case_match
no_test_case_match_body() {
    utils_install_stable_test_wrapper

    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="first"}
EOF
    utils_cp_helper simple_all_pass first

    cat >expout <<EOF
Results file id is $(utils_results_id)
Results saved to $(utils_results_file)
EOF
    cat >experr <<EOF
kyua: W: No test cases matched by the filter 'first:foobar'.
EOF
    atf_check -s exit:1 -o file:expout -e file:experr kyua test first:foobar
}


utils_test_case missing_kyuafile__no_args
missing_kyuafile__no_args_body() {
    cat >experr <<EOF
kyua: E: Load of 'Kyuafile' failed: File 'Kyuafile' not found.
EOF
    atf_check -s exit:2 -o empty -e file:experr kyua test
}


utils_test_case missing_kyuafile__test_program
missing_kyuafile__test_program_body() {
    mkdir subdir
    cat >subdir/Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="unused"}
EOF
    utils_cp_helper simple_all_pass subdir/unused

    cat >experr <<EOF
kyua: E: Load of 'Kyuafile' failed: File 'Kyuafile' not found.
EOF
    atf_check -s exit:2 -o empty -e file:experr kyua test subdir/unused
}


utils_test_case missing_kyuafile__subdir
missing_kyuafile__subdir_body() {
    mkdir subdir
    cat >subdir/Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="unused"}
EOF
    utils_cp_helper simple_all_pass subdir/unused

    cat >experr <<EOF
kyua: E: Load of 'Kyuafile' failed: File 'Kyuafile' not found.
EOF
    atf_check -s exit:2 -o empty -e file:experr kyua test subdir
}


utils_test_case bogus_config
bogus_config_body() {
    mkdir .kyua
    cat >"${HOME}/.kyua/kyua.conf" <<EOF
Hello, world.
EOF

    file_re='.*\.kyua/kyua.conf'
    atf_check -s exit:2 -o empty \
        -e match:"^kyua: E: Load of '${file_re}' failed: Failed to load Lua" \
        kyua test
}


utils_test_case bogus_kyuafile
bogus_kyuafile_body() {
    cat >Kyuafile <<EOF
Hello, world.
EOF
    atf_check -s exit:2 -o empty \
        -e match:"Load of 'Kyuafile' failed: .* Kyuafile:2:" kyua list
}


utils_test_case bogus_test_program
bogus_test_program_body() {
    utils_install_stable_test_wrapper

    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="crash_on_list"}
atf_test_program{name="non_executable"}
EOF
    utils_cp_helper bad_test_program crash_on_list
    echo 'I am not executable' >non_executable

# CHECK_STYLE_DISABLE
    cat >expout <<EOF
crash_on_list:__test_cases_list__  ->  broken: Invalid header for test case list; expecting Content-Type for application/X-atf-tp version 1, got ''  [S.UUUs]
non_executable:__test_cases_list__  ->  broken: Permission denied to run test program  [S.UUUs]

Results file id is $(utils_results_id)
Results saved to $(utils_results_file)

0/2 passed (2 broken, 0 failed, 0 skipped)
EOF
# CHECK_STYLE_ENABLE
    atf_check -s exit:1 -o file:expout -e empty kyua test
}


utils_test_case missing_test_program
missing_test_program_body() {
    cat >Kyuafile <<EOF
syntax(2)
include("subdir/Kyuafile")
EOF
    mkdir subdir
    cat >subdir/Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="ok"}
atf_test_program{name="i-am-missing"}
EOF
    echo 'I should not be touched because the Kyuafile is bogus' >subdir/ok

# CHECK_STYLE_DISABLE
    cat >experr <<EOF
kyua: E: Load of 'Kyuafile' failed: .*Non-existent test program 'subdir/i-am-missing'.
EOF
# CHECK_STYLE_ENABLE
    atf_check -s exit:2 -o empty -e "match:$(cat experr)" kyua list
}


atf_init_test_cases() {
    atf_add_test_case one_test_program__all_pass
    atf_add_test_case one_test_program__some_fail
    atf_add_test_case many_test_programs__all_pass
    atf_add_test_case many_test_programs__some_fail
    atf_add_test_case expect__all_pass
    atf_add_test_case expect__some_fail
    atf_add_test_case premature_exit

    atf_add_test_case no_args
    atf_add_test_case one_arg__subdir
    atf_add_test_case one_arg__test_case
    atf_add_test_case one_arg__test_program
    atf_add_test_case one_arg__invalid
    atf_add_test_case many_args__ok
    atf_add_test_case many_args__invalid
    atf_add_test_case many_args__no_match__all
    atf_add_test_case many_args__no_match__some

    atf_add_test_case args_are_relative

    atf_add_test_case only_load_used_test_programs

    atf_add_test_case config_behavior
    atf_add_test_case config_unprivileged_user

    atf_add_test_case store_contents
    atf_add_test_case results_file__ok
    atf_add_test_case results_file__fail
    atf_add_test_case results_file__reuse

    atf_add_test_case build_root_flag

    atf_add_test_case kyuafile_flag__no_args
    atf_add_test_case kyuafile_flag__some_args

    atf_add_test_case interrupt

    atf_add_test_case exclusive_tests

    atf_add_test_case no_test_program_match
    atf_add_test_case no_test_case_match

    atf_add_test_case missing_kyuafile__no_args
    atf_add_test_case missing_kyuafile__test_program
    atf_add_test_case missing_kyuafile__subdir

    atf_add_test_case bogus_config
    atf_add_test_case bogus_kyuafile
    atf_add_test_case bogus_test_program
    atf_add_test_case missing_test_program
}
