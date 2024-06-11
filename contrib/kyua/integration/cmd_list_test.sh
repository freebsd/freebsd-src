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
atf_test_program{name="metadata"}
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
metadata:many_properties
metadata:no_properties
metadata:one_property
metadata:with_cleanup
simple_all_pass:pass
simple_all_pass:skip
subdir/simple_some_fail:fail
subdir/simple_some_fail:pass
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua list
}


utils_test_case one_arg__subdir
one_arg__subdir_body() {
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

    cat >expout <<EOF
subdir/simple_all_pass:pass
subdir/simple_all_pass:skip
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua list subdir
}


utils_test_case one_arg__test_case
one_arg__test_case_body() {
    cat >Kyuafile <<EOF
syntax(2)
test_suite("top-level")
atf_test_program{name="first"}
atf_test_program{name="second"}
EOF
    utils_cp_helper simple_all_pass first
    utils_cp_helper simple_all_pass second

    cat >expout <<EOF
first:skip
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua list first:skip
}


utils_test_case one_arg__test_program
one_arg__test_program_body() {
    cat >Kyuafile <<EOF
syntax(2)
test_suite("top-level")
atf_test_program{name="first"}
atf_test_program{name="second"}
EOF
    utils_cp_helper simple_all_pass first
    utils_cp_helper simple_some_fail second

    cat >expout <<EOF
second:fail
second:pass
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua list second
}


utils_test_case one_arg__invalid
one_arg__invalid_body() {
cat >experr <<EOF
kyua: E: Test case component in 'foo:' is empty.
EOF
    atf_check -s exit:2 -o empty -e file:experr kyua list foo:

cat >experr <<EOF
kyua: E: Program name '/a/b' must be relative to the test suite, not absolute.
EOF
    atf_check -s exit:2 -o empty -e file:experr kyua list /a/b
}


utils_test_case many_args__ok
many_args__ok_body() {
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
subdir/second:fail (in-subdir)
subdir/second:pass (in-subdir)
first:pass (top-level)
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua list -v subdir first:pass
}


utils_test_case many_args__invalid
many_args__invalid_body() {
cat >experr <<EOF
kyua: E: Program name component in ':badbad' is empty.
EOF
    atf_check -s exit:2 -o empty -e file:experr kyua list this-is-ok :badbad

cat >experr <<EOF
kyua: E: Program name '/foo' must be relative to the test suite, not absolute.
EOF
    atf_check -s exit:2 -o empty -e file:experr kyua list this-is-ok /foo
}


utils_test_case many_args__no_match__all
many_args__no_match__all_body() {
    cat >Kyuafile <<EOF
syntax(2)
test_suite("top-level")
atf_test_program{name="first"}
atf_test_program{name="second"}
EOF
    utils_cp_helper simple_all_pass first
    utils_cp_helper simple_all_pass second

    cat >experr <<EOF
kyua: W: No test cases matched by the filter 'first1'.
EOF
    atf_check -s exit:1 -o empty -e file:experr kyua list first1
}


utils_test_case many_args__no_match__some
many_args__no_match__some_body() {
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
first:pass
first:skip
third:fail
third:pass
EOF

    cat >experr <<EOF
kyua: W: No test cases matched by the filter 'fifth'.
kyua: W: No test cases matched by the filter 'fourth'.
EOF
    atf_check -s exit:1 -o file:expout -e file:experr kyua list first fourth \
        third fifth
}


utils_test_case args_are_relative
args_are_relative_body() {
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
first:pass (integration-1)
first:skip (integration-1)
subdir/fourth:fail (integration-2)
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua list \
        -v -k "$(pwd)/root/Kyuafile" first subdir/fourth:fail
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
first:pass
first:skip
EOF
    CREATE_COOKIE="$(pwd)/cookie"; export CREATE_COOKIE
    atf_check -s exit:0 -o file:expout -e empty kyua list first
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
    atf_check -s exit:0 -o ignore -e empty kyua -c my-config list
    [ -f "${CONFIG_VAR_FILE}" ] || \
        atf_fail "Cookie file not created; test case list did not get" \
            "configuration variables"
    value="$(cat "${CONFIG_VAR_FILE}")"
    [ "${value}" = "value1" ] || \
        atf_fail "Invalid value (${value}) in cookie file; test case list did" \
            "not get the correct configuration variables"
}


utils_test_case build_root_flag
build_root_flag_body() {
    mkdir subdir
    mkdir build
    mkdir build/subdir

    cat >Kyuafile <<EOF
syntax(2)
test_suite("top-level")
include("subdir/Kyuafile")
atf_test_program{name="first"}
EOF
    echo 'invalid' >first
    utils_cp_helper simple_all_pass build/first

    cat >subdir/Kyuafile <<EOF
syntax(2)
test_suite("in-subdir")
atf_test_program{name="second"}
EOF
    echo 'invalid' >subdir/second
    utils_cp_helper simple_some_fail build/subdir/second

    cat >expout <<EOF
subdir/second:fail
subdir/second:pass
first:pass
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua list --build-root=build \
        subdir first:pass
}


utils_test_case kyuafile_flag__no_args
kyuafile_flag__no_args_body() {
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
sometest:pass
sometest:skip
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua list -k myfile
    atf_check -s exit:0 -o file:expout -e empty kyua list --kyuafile=myfile
}


utils_test_case kyuafile_flag__some_args
kyuafile_flag__some_args_body() {
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
sometest:pass (hello-world)
sometest:skip (hello-world)
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua list -v -k myfile sometest
    atf_check -s exit:0 -o file:expout -e empty kyua list -v --kyuafile=myfile \
        sometest
}


utils_test_case verbose_flag
verbose_flag_body() {
    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration-suite-1")
atf_test_program{name="simple_all_pass"}
plain_test_program{name="i_am_plain", timeout=654}
include("subdir/Kyuafile")
EOF
    utils_cp_helper simple_all_pass .
    touch i_am_plain

    mkdir subdir
    cat >subdir/Kyuafile <<EOF
syntax(2)
test_suite("integration-suite-2")
atf_test_program{name="metadata"}
EOF
    utils_cp_helper metadata subdir

    cat >expout <<EOF
simple_all_pass:pass (integration-suite-1)
simple_all_pass:skip (integration-suite-1)
i_am_plain:main (integration-suite-1)
    timeout = 654
subdir/metadata:many_properties (integration-suite-2)
    allowed_architectures = some-architecture
    allowed_platforms = some-platform
    custom.no-meaning = I am a custom variable
    description =     A description with some padding
    required_configs = var1 var2 var3
    required_files = /my/file1 /some/other/file
    required_programs = /nonexistent/bin3 bin1 bin2
    required_user = root
subdir/metadata:no_properties (integration-suite-2)
subdir/metadata:one_property (integration-suite-2)
    description = Does nothing but has one metadata property
subdir/metadata:with_cleanup (integration-suite-2)
    has_cleanup = true
    timeout = 250
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua list -v
    atf_check -s exit:0 -o file:expout -e empty kyua list --verbose
}


utils_test_case no_test_program_match
no_test_program_match_body() {
    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="first"}
EOF
    utils_cp_helper simple_all_pass first
    utils_cp_helper simple_all_pass second

    cat >experr <<EOF
kyua: W: No test cases matched by the filter 'second'.
EOF
    atf_check -s exit:1 -o empty -e file:experr kyua list second
}


utils_test_case no_test_case_match
no_test_case_match_body() {
    cat >Kyuafile <<EOF
syntax(2)
test_suite("integration")
atf_test_program{name="first"}
EOF
    utils_cp_helper simple_all_pass first

    cat >experr <<EOF
kyua: W: No test cases matched by the filter 'first:foobar'.
EOF
    atf_check -s exit:1 -o empty -e file:experr kyua list first:foobar
}


utils_test_case missing_kyuafile__no_args
missing_kyuafile__no_args_body() {
    cat >experr <<EOF
kyua: E: Load of 'Kyuafile' failed: File 'Kyuafile' not found.
EOF
    atf_check -s exit:2 -o empty -e file:experr kyua list
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
    atf_check -s exit:2 -o empty -e file:experr kyua list subdir/unused
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
    atf_check -s exit:2 -o empty -e file:experr kyua list subdir
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

    cat >expout <<EOF
crash_on_list:__test_cases_list__
non_executable:__test_cases_list__
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua list
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

    atf_add_test_case build_root_flag

    atf_add_test_case kyuafile_flag__no_args
    atf_add_test_case kyuafile_flag__some_args

    atf_add_test_case verbose_flag

    atf_add_test_case no_test_program_match
    atf_add_test_case no_test_case_match

    atf_add_test_case missing_kyuafile__no_args
    atf_add_test_case missing_kyuafile__test_program
    atf_add_test_case missing_kyuafile__subdir

    atf_add_test_case bogus_kyuafile
    atf_add_test_case bogus_test_program
    atf_add_test_case missing_test_program
}
