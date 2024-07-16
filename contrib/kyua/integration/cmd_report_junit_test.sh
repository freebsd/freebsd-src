# Copyright 2014 The Kyua Authors.
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
    atf_check -s exit:0 -o save:stdout -e empty env MOCK="${mock_env}" kyua test
    grep '^Results saved to ' stdout | cut -d ' ' -f 4 >"${dbfile_name}"
    rm stdout

    # Ensure the results of 'report-junit' come from the database.
    rm Kyuafile simple_all_pass
}


# Removes the contents of a properties tag from stdout.
strip_properties='awk "
BEGIN { skip = 0; }

/<\/properties>/ {
    print \"</properties>\";
    skip = 0;
    next;
}

/<properties>/ {
    print \"<properties>\";
    print \"CONTENTS STRIPPED BY TEST\";
    skip = 1;
    next;
}

{ if (!skip) print; }"'


utils_test_case default_behavior__ok
default_behavior__ok_body() {
    utils_install_times_wrapper

    run_tests "mock1
this should not be seen
mock1 new line" unused_dbfile_name

    cat >expout <<EOF
<?xml version="1.0" encoding="iso-8859-1"?>
<testsuite>
<properties>
CONTENTS STRIPPED BY TEST
</properties>
<testcase classname="simple_all_pass" name="pass" time="S.UUU">
<system-out>This is the stdout of pass
</system-out>
<system-err>Test case metadata
------------------

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

Timing information
------------------

Start time: YYYY-MM-DDTHH:MM:SS.ssssssZ
End time:   YYYY-MM-DDTHH:MM:SS.ssssssZ
Duration:   S.UUUs

Original stderr
---------------

This is the stderr of pass
</system-err>
</testcase>
<testcase classname="simple_all_pass" name="skip" time="S.UUU">
<skipped/>
<system-out>This is the stdout of skip
</system-out>
<system-err>Skipped result details
----------------------

The reason for skipping is this

Test case metadata
------------------

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

Timing information
------------------

Start time: YYYY-MM-DDTHH:MM:SS.ssssssZ
End time:   YYYY-MM-DDTHH:MM:SS.ssssssZ
Duration:   S.UUUs

Original stderr
---------------

This is the stderr of skip
</system-err>
</testcase>
</testsuite>
EOF
    atf_check -s exit:0 -o file:expout -e empty -x "kyua report-junit" \
        "| ${strip_properties}"
}


utils_test_case default_behavior__no_store
default_behavior__no_store_body() {
    echo 'kyua: E: No previous results file found for test suite' \
        "$(utils_test_suite_id)." >experr
    atf_check -s exit:2 -o empty -e file:experr kyua report-junit
}


utils_test_case results_file__explicit
results_file__explicit_body() {
    run_tests "mock1" dbfile_name1
    run_tests "mock2" dbfile_name2

    atf_check -s exit:0 -o match:"MOCK.*mock1" -o not-match:"MOCK.*mock2" \
        -e empty kyua report-junit --results-file="$(cat dbfile_name1)"
    atf_check -s exit:0 -o not-match:"MOCK.*mock1" -o match:"MOCK.*mock2" \
        -e empty kyua report-junit --results-file="$(cat dbfile_name2)"
}


utils_test_case results_file__not_found
results_file__not_found_body() {
    atf_check -s exit:2 -o empty -e match:"kyua: E: No previous results.*foo" \
        kyua report-junit --results-file=foo
}


utils_test_case output__explicit
output__explicit_body() {
    run_tests unused_mock unused_dbfile_name

    cat >report <<EOF
<?xml version="1.0" encoding="iso-8859-1"?>
<testsuite>
<properties>
CONTENTS STRIPPED BY TEST
</properties>
<testcase classname="simple_all_pass" name="pass" time="S.UUU">
<system-out>This is the stdout of pass
</system-out>
<system-err>Test case metadata
------------------

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

Timing information
------------------

Start time: YYYY-MM-DDTHH:MM:SS.ssssssZ
End time:   YYYY-MM-DDTHH:MM:SS.ssssssZ
Duration:   S.UUUs

Original stderr
---------------

This is the stderr of pass
</system-err>
</testcase>
<testcase classname="simple_all_pass" name="skip" time="S.UUU">
<skipped/>
<system-out>This is the stdout of skip
</system-out>
<system-err>Skipped result details
----------------------

The reason for skipping is this

Test case metadata
------------------

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

Timing information
------------------

Start time: YYYY-MM-DDTHH:MM:SS.ssssssZ
End time:   YYYY-MM-DDTHH:MM:SS.ssssssZ
Duration:   S.UUUs

Original stderr
---------------

This is the stderr of skip
</system-err>
</testcase>
</testsuite>
EOF

    atf_check -s exit:0 -o file:report -e empty -x kyua report-junit \
        --output=/dev/stdout "| ${strip_properties} | ${utils_strip_times}"
    atf_check -s exit:0 -o empty -e save:stderr kyua report-junit \
        --output=/dev/stderr
    atf_check -s exit:0 -o file:report -x cat stderr \
        "| ${strip_properties} | ${utils_strip_times}"

    atf_check -s exit:0 -o empty -e empty kyua report-junit \
        --output=my-file
    atf_check -s exit:0 -o file:report -x cat my-file \
        "| ${strip_properties} | ${utils_strip_times}"
}


atf_init_test_cases() {
    atf_add_test_case default_behavior__ok
    atf_add_test_case default_behavior__no_store

    atf_add_test_case results_file__explicit
    atf_add_test_case results_file__not_found

    atf_add_test_case output__explicit
}
