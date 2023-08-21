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
    cat >experr <<EOF
Usage error: No command provided.
Type 'kyua help' for usage information.
EOF

    atf_check -s exit:3 -o empty -e file:experr kyua
}


utils_test_case unknown_option
unknown_option_body() {
    cat >experr <<EOF
Usage error: Unknown option --this_is_unknown.
Type 'kyua help' for usage information.
EOF

    atf_check -s exit:3 -o empty -e file:experr kyua --this_is_unknown
}


utils_test_case unknown_command
unknown_command_body() {
    cat >experr <<EOF
Usage error: Unknown command 'i_am_not_known'.
Type 'kyua help' for usage information.
EOF

    atf_check -s exit:3 -o empty -e file:experr kyua i_am_not_known
}


utils_test_case logfile__default
logfile__default_body() {
    atf_check -s exit:0 test ! -d .kyua/logs/
    atf_check -s exit:3 -o empty -e ignore kyua
    atf_check -s exit:0 test -d .kyua/logs/
}


utils_test_case logfile__override
logfile__override_body() {
    atf_check -s exit:0 test ! -f test.log
    atf_check -s exit:3 -o empty -e ignore kyua --logfile=test.log

    atf_check -s exit:0 test ! -d .kyua/logs/
    atf_check -s exit:0 test -f test.log

    grep ' E .* No command provided' test.log || atf_fail "Log file does" \
        "contain required message"
}


utils_test_case loglevel__default
loglevel__default_body() {
    atf_check -s exit:0 test ! -f test.log
    atf_check -s exit:3 -o empty -e ignore kyua --logfile=test.log

    atf_check -s exit:0 test ! -d .kyua/logs/
    atf_check -s exit:0 test -f test.log

    grep ' E .* No command provided' test.log || atf_fail "Log file does" \
        "contain required message"
    if grep ' D ' test.log; then
        atf_fail "Log file contains debug messages but it should not"
    fi
}


utils_test_case loglevel__lower
loglevel__lower_body() {
    atf_check -s exit:0 test ! -f test.log
    atf_check -s exit:3 -o empty -e ignore kyua --logfile=test.log \
        --loglevel=warning

    atf_check -s exit:0 test ! -d .kyua/logs/
    atf_check -s exit:0 test -f test.log

    grep ' E .* No command provided' test.log || atf_fail "Log file does" \
        "contain required message"
    if grep ' I ' test.log; then
        atf_fail "Log file contains info messages but it should not"
    fi
}


utils_test_case loglevel__higher
loglevel__higher_body() {
    atf_check -s exit:0 test ! -f test.log
    atf_check -s exit:3 -o empty -e ignore kyua --logfile=test.log \
        --loglevel=debug

    atf_check -s exit:0 test ! -d .kyua/logs/
    atf_check -s exit:0 test -f test.log

    grep ' E .* No command provided' test.log || atf_fail "Log file does" \
        "contain required message"
    grep ' D ' test.log || atf_fail "Log file does not contain debug messages"
}


atf_init_test_cases() {
    atf_add_test_case no_args
    atf_add_test_case unknown_option
    atf_add_test_case unknown_command

    atf_add_test_case logfile__default
    atf_add_test_case logfile__override

    atf_add_test_case loglevel__default
    atf_add_test_case loglevel__lower
    atf_add_test_case loglevel__higher

    # Tests for the global configuration-related flags are found in the
    # cmd_config_test test program.
}
