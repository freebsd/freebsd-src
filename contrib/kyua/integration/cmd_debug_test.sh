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


utils_test_case no_args
no_args_body() {
    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="simple_all_pass"}
EOF
    utils_cp_helper simple_all_pass .

    cat >experr <<EOF
Usage error for command debug: Not enough arguments.
Type 'kyua help debug' for usage information.
EOF
    atf_check -s exit:3 -o empty -e file:experr kyua debug
}


utils_test_case many_args
many_args_body() {
    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="first"}
atf_test_program{name="second"}
EOF
    utils_cp_helper simple_all_pass first
    utils_cp_helper simple_all_pass second

    cat >experr <<EOF
Usage error for command debug: Too many arguments.
Type 'kyua help debug' for usage information.
EOF
    atf_check -s exit:3 -o empty -e file:experr kyua debug first:pass \
        second:pass
}


utils_test_case one_arg__ok_pass
one_arg__ok_pass_body() {
    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="first"}
atf_test_program{name="second"}
EOF
    utils_cp_helper expect_all_pass first
    utils_cp_helper simple_all_pass second

    cat >expout <<EOF
This is the stdout of pass
second:pass  ->  passed
EOF
cat >experr <<EOF
This is the stderr of pass
EOF
    atf_check -s exit:0 -o file:expout -e file:experr kyua debug second:pass
}


utils_test_case one_arg__ok_fail
one_arg__ok_fail_body() {
    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="first"}
EOF
    utils_cp_helper simple_some_fail first

    cat >expout <<EOF
This is the stdout of fail
first:fail  ->  failed: This fails on purpose
EOF
    cat >experr <<EOF
This is the stderr of fail
EOF
    atf_check -s exit:1 -o file:expout -e file:experr kyua debug first:fail
}


utils_test_case one_arg__no_match
one_arg__no_match_body() {
    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="first"}
atf_test_program{name="second"}
EOF
    utils_cp_helper expect_all_pass first
    utils_cp_helper simple_all_pass second

    cat >experr <<EOF
kyua: E: Unknown test case 'second:die'.
EOF
    atf_check -s exit:2 -o empty -e file:experr kyua debug second:die
}


utils_test_case one_arg__no_test_case
one_arg__no_test_case_body() {
    # CHECK_STYLE_DISABLE
    cat >experr <<EOF
Usage error for command debug: 'foo' is not a test case identifier (missing ':'?).
Type 'kyua help debug' for usage information.
EOF
    # CHECK_STYLE_ENABLE
    atf_check -s exit:3 -o empty -e file:experr kyua debug foo
}


utils_test_case one_arg__bad_filter
one_arg__bad_filter_body() {
    cat >experr <<EOF
kyua: E: Test case component in 'foo:' is empty.
EOF
    atf_check -s exit:2 -o empty -e file:experr kyua debug foo:
}


utils_test_case body_and_cleanup
body_and_cleanup_body() {
    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="single"}
EOF
    utils_cp_helper metadata single

    cat >expout <<EOF
single:with_cleanup  ->  passed
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua debug \
        --stdout=saved.out --stderr=saved.err single:with_cleanup

    cat >expout <<EOF
Body message to stdout
Cleanup message to stdout
EOF
    atf_check -s exit:0 -o file:expout -e empty cat saved.out

    cat >experr <<EOF
Body message to stderr
Cleanup message to stderr
EOF
    atf_check -s exit:0 -o file:experr -e empty cat saved.err
}


utils_test_case stdout_stderr_flags
stdout_stderr_flags_body() {
    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="first"}
atf_test_program{name="second"}
EOF
    utils_cp_helper expect_all_pass first
    utils_cp_helper simple_all_pass second

    cat >expout <<EOF
second:pass  ->  passed
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua debug \
        --stdout=saved.out --stderr=saved.err second:pass

    cat >expout <<EOF
This is the stdout of pass
EOF
    cmp -s saved.out expout || atf_fail "--stdout did not redirect the" \
        "standard output to the desired file"

    cat >experr <<EOF
This is the stderr of pass
EOF
    cmp -s saved.err experr || atf_fail "--stderr did not redirect the" \
        "standard error to the desired file"
}


utils_test_case args_are_relative
args_are_relative_body() {
    mkdir root
    cat >root/Kyuafile <<EOF
syntax(2)
test_suite("integration")
include("subdir/Kyuafile")
atf_test_program{name="prog"}
EOF
    utils_cp_helper simple_all_pass root/prog

    mkdir root/subdir
    cat >root/subdir/Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="prog"}
EOF
    utils_cp_helper simple_some_fail root/subdir/prog

    cat >expout <<EOF
This is the stdout of fail
subdir/prog:fail  ->  failed: This fails on purpose
EOF
    cat >experr <<EOF
This is the stderr of fail
EOF
    atf_check -s exit:1 -o file:expout -e file:experr kyua debug \
        -k "$(pwd)/root/Kyuafile" subdir/prog:fail
}


utils_test_case only_load_used_test_programs
only_load_used_test_programs_body() {
    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="first"}
atf_test_program{name="second"}
EOF
    utils_cp_helper simple_all_pass first
    utils_cp_helper bad_test_program second

    cat >expout <<EOF
This is the stdout of pass
first:pass  ->  passed
EOF
    cat >experr <<EOF
This is the stderr of pass
EOF
    CREATE_COOKIE="$(pwd)/cookie"; export CREATE_COOKIE
    atf_check -s exit:0 -o file:expout -e file:experr kyua debug first:pass
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

    atf_check -s exit:1 -o match:'failed' -e empty \
        kyua -c my-config -v test_suites.suite2.the-variable=value2 \
        debug config1:get_variable
    atf_check -s exit:0 -o match:'passed' -e empty \
        kyua -c my-config -v test_suites.suite2.the-variable=value2 \
        debug config2:get_variable
    atf_check -s exit:0 -o match:'skipped' -e empty \
        kyua -c my-config -v test_suites.suite2.the-variable=value2 \
        debug config3:get_variable
}


utils_test_case build_root_flag
build_root_flag_body() {
    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="first"}
atf_test_program{name="second"}
EOF
    mkdir build
    utils_cp_helper expect_all_pass build/first
    utils_cp_helper simple_all_pass build/second

    cat >expout <<EOF
This is the stdout of pass
second:pass  ->  passed
EOF
cat >experr <<EOF
This is the stderr of pass
EOF
    atf_check -s exit:0 -o file:expout -e file:experr \
        kyua debug --build-root=build second:pass
}


utils_test_case kyuafile_flag__ok
kyuafile_flag__ok_body() {
    cat >Kyuafile <<EOF
This file is bogus but it is not loaded.
EOF

    cat >myfile <<EOF
syntax(2)
test_suite("hello-world")
atf_test_program{name="sometest"}
EOF
    utils_cp_helper simple_all_pass sometest

    atf_check -s exit:0 -o match:passed -e empty kyua test -k myfile sometest
    atf_check -s exit:0 -o match:passed -e empty kyua test --kyuafile=myfile \
        sometest
}


utils_test_case missing_kyuafile
missing_kyuafile_body() {
    cat >experr <<EOF
kyua: E: Load of 'Kyuafile' failed: File 'Kyuafile' not found.
EOF
    atf_check -s exit:2 -o empty -e file:experr kyua debug foo:bar
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
    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="crash_on_list"}
atf_test_program{name="non_executable"}
EOF
    utils_cp_helper bad_test_program crash_on_list
    echo 'I am not executable' >non_executable

    cat >experr <<EOF
kyua: E: Unknown test case 'crash_on_list:a'.
EOF
    atf_check -s exit:2 -o empty -e file:experr kyua debug crash_on_list:a

    cat >experr <<EOF
kyua: E: Unknown test case 'non_executable:a'.
EOF
    atf_check -s exit:2 -o empty -e file:experr kyua debug non_executable:a

    # CHECK_STYLE_DISABLE
    cat >expout <<EOF
crash_on_list:__test_cases_list__  ->  broken: Invalid header for test case list; expecting Content-Type for application/X-atf-tp version 1, got ''
EOF
    # CHECK_STYLE_ENABLE
    atf_check -s exit:1 -o file:expout -e empty kyua debug \
        crash_on_list:__test_cases_list__

    # CHECK_STYLE_DISABLE
    cat >expout <<EOF
non_executable:__test_cases_list__  ->  broken: Permission denied to run test program
EOF
    # CHECK_STYLE_ENABLE
    atf_check -s exit:1 -o file:expout -e empty kyua debug \
        non_executable:__test_cases_list__
}


atf_init_test_cases() {
    atf_add_test_case no_args
    atf_add_test_case many_args
    atf_add_test_case one_arg__ok_pass
    atf_add_test_case one_arg__ok_fail
    atf_add_test_case one_arg__no_match
    atf_add_test_case one_arg__no_test_case
    atf_add_test_case one_arg__bad_filter

    atf_add_test_case body_and_cleanup

    atf_add_test_case stdout_stderr_flags

    atf_add_test_case args_are_relative

    atf_add_test_case only_load_used_test_programs

    atf_add_test_case config_behavior

    atf_add_test_case build_root_flag
    atf_add_test_case kyuafile_flag__ok
    atf_add_test_case missing_kyuafile
    atf_add_test_case bogus_kyuafile
    atf_add_test_case bogus_test_program
}
