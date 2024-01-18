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


utils_test_case global
global_body() {
    atf_check -s exit:0 -o save:stdout -e empty kyua help
    grep -E 'kyua .*[0-9]+\.[0-9]+' stdout || atf_fail 'No version reported'
    grep '^Usage: kyua' stdout || atf_fail 'No usage line printed'
    grep -- '--loglevel' stdout || atf_fail 'Generic options not printed'
    if grep -- '--show' stdout; then
        atf_fail 'One option of the about subcommand appeared in the output'
    fi
    grep 'about  *Shows detailed' stdout || atf_fail 'Commands not printed'
}


utils_test_case one_command
one_command_body() {
    atf_check -s exit:0 -o save:stdout -e empty kyua help test
    grep -E 'kyua .*[0-9]+\.[0-9]+' stdout || atf_fail 'No version reported'
    grep '^Usage: kyua' stdout || atf_fail 'No usage line printed'
    grep '^Run tests' stdout || atf_fail 'No description printed'
    grep -- '--loglevel' stdout || atf_fail 'Generic options not printed'
    grep -- '--kyuafile' stdout || atf_fail 'Command options not printed'
    if grep 'about: Shows detailed' stdout; then
        atf_fail 'Printed table of commands, but should not have done so'
    fi
}


utils_test_case ignore_bad_config
ignore_bad_config_body() {
    echo 'this is an invalid configuration file' >bad-config
    atf_check -s exit:0 -o save:stdout -e empty kyua -c bad-config help
    grep '^Usage: kyua' stdout || atf_fail 'No usage line printed'
    grep -- '--loglevel' stdout || atf_fail 'Generic options not printed'
}


utils_test_case unknown_command
unknown_command_body() {
    cat >stderr <<EOF
Usage error for command help: The command abc does not exist.
Type 'kyua help help' for usage information.
EOF
    atf_check -s exit:3 -o empty -e file:stderr kyua help abc
}


utils_test_case too_many_arguments
too_many_arguments_body() {
    cat >stderr <<EOF
Usage error for command help: Too many arguments.
Type 'kyua help help' for usage information.
EOF
    atf_check -s exit:3 -o empty -e file:stderr kyua help about cde
}


atf_init_test_cases() {
    atf_add_test_case global
    atf_add_test_case one_command

    atf_add_test_case ignore_bad_config
    atf_add_test_case unknown_command
    atf_add_test_case too_many_arguments
}
