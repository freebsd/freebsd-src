#
# Automated Testing Framework (atf)
#
# Copyright (c) 2010 The NetBSD Foundation, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
# CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

create_test_program() {
    echo '#! /usr/bin/env atf-sh' >"${1}"
    cat >>"${1}"
    chmod +x "${1}"
}

atf_test_case no_args
no_args_body()
{
    cat >experr <<EOF
atf-sh: ERROR: No test program provided
atf-sh: See atf-sh(1) for usage details.
EOF
    atf_check -s eq:1 -o ignore -e file:experr atf-sh
}

atf_test_case missing_script
missing_script_body()
{
    cat >experr <<EOF
atf-sh: ERROR: The test program 'non-existent' does not exist
EOF
    atf_check -s eq:1 -o ignore -e file:experr atf-sh non-existent
}

atf_test_case arguments
arguments_body()
{
    create_test_program tp <<EOF
main() {
    echo ">>>\${0}<<<"
    while test \${#} -gt 0; do
        echo ">>>\${1}<<<"
        shift
    done
    true
}
EOF

    cat >expout <<EOF
>>>./tp<<<
>>> a b <<<
>>>foo<<<
EOF
    atf_check -s eq:0 -o file:expout -e empty ./tp ' a b ' foo

    cat >expout <<EOF
>>>tp<<<
>>> hello bye <<<
>>>foo bar<<<
EOF
    atf_check -s eq:0 -o file:expout -e empty atf-sh tp ' hello bye ' 'foo bar'
}

atf_init_test_cases()
{
    atf_add_test_case no_args
    atf_add_test_case missing_script
    atf_add_test_case arguments
}

# vim: syntax=sh:expandtab:shiftwidth=4:softtabstop=4
