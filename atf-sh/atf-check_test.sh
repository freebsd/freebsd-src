# Copyright (c) 2008 The NetBSD Foundation, Inc.
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

# The Atf_Check and Atf-Shell variables are set by atf-sh.

h_pass()
{
    cmd="$1"; shift

    echo "Running [atf-check $*] against [${cmd}]"

    cat >script.sh <<EOF
#! ${Atf_Shell}
${cmd}
EOF
    chmod +x script.sh

    if ! ${Atf_Check} "${@}" ./script.sh >tmp; then
        cat tmp
        atf_fail "atf-check failed"
    fi
}

h_fail()
{
    cmd="$1"; shift

    echo "Running [atf-check $*] against [${cmd}]"

    cat >script.sh <<EOF
#! ${Atf_Shell}
${cmd}
EOF
    chmod +x script.sh

    if ${Atf_Check} "${@}" ./script.sh 2>tmp; then
        cat tmp
        atf_fail "atf-check succeeded but should fail"
    fi
}

atf_test_case sflag_eq_ne
sflag_eq_ne_head()
{
    atf_set "descr" "Tests for the -s option using the 'eq' and 'ne' qualifiers"
}
sflag_eq_ne_body()
{
    h_pass "true" -s eq:0
    h_pass "false" -s ne:0
    h_pass "exit 255" -s eq:255
    h_pass "exit 0" -s ne:255

    h_fail "exit 256" -s eq:256
    h_fail "exit -1" -s eq:-1
    h_fail "true" -s ne:256
    h_fail "true" -s ne:-1
}

atf_test_case sflag_exit
sflag_exit_head()
{
    atf_set "descr" "Tests for the -s option using the 'exit' qualifier"
}
sflag_exit_body()
{
    h_pass 'true' -s exit:0
    h_pass 'false' -s not-exit:0
    h_pass 'exit 255' -s exit:255
    h_pass 'exit 0' -s not-exit:255

    h_fail 'exit 256' -s exit:256
    h_fail 'exit -1' -s exit:-1
    h_fail 'true' -s not-exit:256
    h_fail 'true' -s not-exit:-1

    h_pass 'true' -s exit
    h_pass 'false' -s exit
    if ${Atf_Check} -s exit -x 'kill $$'; then
        atf_fail "Signal detected as clean exit"
    fi
}

atf_test_case sflag_ignore
sflag_ignore_head()
{
    atf_set "descr" "Tests for the -s option using the 'ignore' qualifier"
}
sflag_ignore_body()
{
    h_pass 'true' -s ignore
    h_pass 'false' -s ignore
    if ${Atf_Check} -s ignored -x 'kill $$'; then
        atf_fail "Signal not ignored"
    fi
}

atf_test_case sflag_signal
sflag_signal_head()
{
    atf_set "descr" "Tests for the -s option using the 'signal' qualifier"
}
sflag_signal_body()
{
    ${Atf_Check} -s signal:hup -x 'kill -1 $$' || atf_fail "Signal not detected"
    ${Atf_Check} -s signal:sighup -x 'kill -1 $$' || atf_fail "Signal not" \
        "detected"
    ${Atf_Check} -s signal:1 -x 'kill -1 $$' || atf_fail "Signal not detected"
    ${Atf_Check} -s signal -x 'kill -1 $$' || atf_fail "Signal not detected"

    ${Atf_Check} -s not-signal:kill -x 'kill -9 $$' && \
        atf_fail "not-signal:kill matched kill -9"
    ${Atf_Check} -s not-signal:kill -x 'kill -1 $$' || \
        atf_fail "not-signal:kill did not match kill -1"

    h_fail 'true' -s signal
    h_fail 'false' -s signal
}

atf_test_case xflag
xflag_head()
{
    atf_set "descr" "Tests for the -x option"
}
xflag_body()
{
    ${Atf_Check} -s ne:0 -o ignore -e ignore "echo foo 2>&1" || \
        atf_fail "Shell command succeeded without -x"

    ${Atf_Check} -e inline:"foo\n" -x "echo foo 1>&2" || \
        atf_fail "Cannot run command with -x"

    ${Atf_Check} -o inline:"foo\n" -x echo foo || \
        atf_fail "Using -x does not respect all provided arguments"
}

atf_test_case vflag
vflag_head()
{
    atf_set "descr" "The -v option shows both streams on successful checks"
}
vflag_body()
{
    cmd='echo output; echo error >&2'
    expected='stdout:\noutput\n\nstderr:\nerror\n\n'

    # Successful checks are silent on stderr without -v.
    atf_check -o ignore -e empty ${Atf_Check} \
        -o ignore -e ignore -x "${cmd}"

    # With -v, both streams are shown even when their checks pass.
    atf_check -o ignore -e inline:"${expected}" ${Atf_Check} \
        -v -o inline:"output\n" -e inline:"error\n" -x "${cmd}"

    # An expected nonzero exit status also allows verbose output.
    atf_check -o ignore -e inline:"${expected}" ${Atf_Check} \
        -v -s exit:7 -o inline:"output\n" -e inline:"error\n" \
        -x "${cmd}; exit 7"
}

atf_test_case vflag_save
vflag_save_head()
{
    atf_set "descr" "The -v option shows streams without changing saved output"
}
vflag_save_body()
{
    expected='stdout:\noutput\n\nstderr:\nerror\n\n'

    # Both streams are shown while being saved to files.
    atf_check -o ignore -e inline:"${expected}" ${Atf_Check} \
        -v -o save:out -e save:err -x 'echo output; echo error >&2'

    # Verbose output does not change the saved streams.
    atf_check -o inline:"output\n" cat out
    atf_check -o inline:"error\n" cat err
}

atf_test_case vflag_empty
vflag_empty_head()
{
    atf_set "descr" "The -v option shows headers for empty streams"
}
vflag_empty_body()
{
    expected='stdout:\n\nstderr:\n\n'

    # Empty streams still have headers and separators.
    atf_check -o ignore -e inline:"${expected}" \
        ${Atf_Check} -v true
}

atf_test_case vflag_no_newline
vflag_no_newline_head()
{
    atf_set "descr" "The -v option separates streams without trailing newlines"
}
vflag_no_newline_body()
{
    expected='stdout:\noutput\nstderr:\nerror\n'

    # Streams without trailing newlines remain separated.
    atf_check -o ignore -e inline:"${expected}" \
        ${Atf_Check} -v -o ignore -e ignore \
        -x 'printf output; printf error >&2'
}

atf_test_case vflag_failure
vflag_failure_head()
{
    atf_set "descr" "The -v option preserves failures without duplicate dumps"
}
vflag_failure_body()
{
    cmd='echo output; echo error >&2; exit 1'
    expected='Fail: incorrect exit status: 1, expected: 0\n'
    expected="${expected}"'stdout:\noutput\n\nstderr:\nerror\n\n'

    # A status mismatch reports the failure and both streams without -v.
    atf_check -s not-exit:0 -o ignore -e inline:"${expected}" ${Atf_Check} \
        -o ignore -e ignore -x "${cmd}"

    # With -v, the same failure is reported without duplicate stream dumps.
    atf_check -s not-exit:0 -o ignore -e inline:"${expected}" ${Atf_Check} \
        -v -o ignore -e ignore -x "${cmd}"

    # An output mismatch still fails and includes both stream headers.
    atf_check -s not-exit:0 -o ignore -e match:'stdout:' \
        -e match:'stderr:' -e match:'Fail:' ${Atf_Check} \
        -v -o empty -e ignore -x 'echo output; echo error >&2'
}

atf_test_case vflag_repeat
vflag_repeat_head()
{
    atf_set "descr" "The -v option shows output from each retry"
}
vflag_repeat_body()
{
    # This script is run twice: the first time the script exits with 1
    # and the second time the script exits with 0. This ensures that
    # the script is run more than once when `-r` is specified.
    cat >script.sh <<'EOF'
echo output
echo error >&2
if [ ! -f retried ]; then
    touch retried
    exit 1
fi
EOF
    expected='Fail: incorrect exit status: 1, expected: 0\n'
    streams='stdout:\noutput\n\nstderr:\nerror\n\n'
    expected="${expected}${streams}${streams}"

    # Both attempts print their streams, and the successful retry passes.
    atf_check -o ignore -e inline:"${expected}" \
        ${Atf_Check} -v -r 10 -o ignore -e ignore ${Atf_Shell} script.sh
}

atf_test_case oflag_empty
oflag_empty_head()
{
    atf_set "descr" "Tests for the -o option using the 'empty' argument"
}
oflag_empty_body()
{
    h_pass "true" -o empty
    h_fail "echo foo" -o empty
}

atf_test_case oflag_ignore
oflag_ignore_head()
{
    atf_set "descr" "Tests for the -o option using the 'ignore' argument"
}
oflag_ignore_body()
{
    h_pass "true" -o ignore
    h_pass "echo foo" -o ignore
}

atf_test_case oflag_file
oflag_file_head()
{
    atf_set "descr" "Tests for the -o option using the 'file:' argument"
}
oflag_file_body()
{
    touch empty
    h_pass "true" -o file:empty

    echo foo >text
    h_pass "echo foo" -o file:text
    h_fail "echo bar" -o file:text

    dd if=/dev/urandom of=bin bs=1k count=10
    h_pass "cat bin" -o file:bin
}

atf_test_case oflag_inline
oflag_inline_head()
{
    atf_set "descr" "Tests for the -o option using the 'inline:' argument"
}
oflag_inline_body()
{
    h_pass "true" -o inline:
    h_pass "echo foo bar" -o inline:"foo bar\n"
    h_pass "printf 'foo bar'" -o inline:"foo bar"
    h_pass "printf '\t\n\t\n'" -o inline:"\t\n\t\n"
    h_pass "printf '\a\b\033\f\n\r\t\v'" -o inline:"\a\b\e\f\n\r\t\v"
    h_pass "printf '\011\022\033\012'" -o inline:"\011\022\033\012"

    h_fail "echo foo bar" -o inline:"foo bar"
    h_fail "echo -n foo bar" -o inline:"foo bar\n"
}

atf_test_case oflag_match
oflag_match_head()
{
    atf_set "descr" "Tests for the -o option using the 'match:' argument"
}
oflag_match_body()
{
    h_pass "printf no-newline" -o "match:^no-newline"
    h_pass "echo line1; echo foo bar" -o "match:^foo"
    h_pass "echo foo bar" -o "match:o b"
    h_fail "echo foo bar" -o "match:baz"
    h_fail "echo foo bar" -o "match:^bar"
}

atf_test_case oflag_save
oflag_save_head()
{
    atf_set "descr" "Tests for the -o option using the 'save:' argument"
}
oflag_save_body()
{
    h_pass "echo foo" -o save:out
    echo foo >exp
    cmp -s out exp || atf_fail "Saved output does not match expected results"
}

atf_test_case oflag_multiple
oflag_multiple_head()
{
    atf_set "descr" "Tests for multiple occurrences of the -o option"
}
oflag_multiple_body()
{
    h_pass "echo foo bar" -o match:foo -o match:bar
    h_pass "echo foo; echo bar" -o match:foo -o match:bar
    h_fail "echo foo baz" -o match:bar -o match:foo
    h_fail "echo foo; echo baz" -o match:bar -o match:foo
}

atf_test_case oflag_negated
oflag_negated_head()
{
    atf_set "descr" "Tests for negated occurrences of the -o option"
}
oflag_negated_body()
{
    h_fail "echo foo" -o empty
    h_pass "echo foo" -o not-empty

    h_pass "echo foo bar" -o match:foo
    h_fail "echo foo bar" -o not-match:foo
}

atf_test_case eflag_empty
eflag_empty_head()
{
    atf_set "descr" "Tests for the -e option using the 'empty' argument"
}
eflag_empty_body()
{
    h_pass "true 1>&2" -e empty
    h_fail "echo foo 1>&2" -e empty
}

atf_test_case eflag_ignore
eflag_ignore_head()
{
    atf_set "descr" "Tests for the -e option using the 'ignore' argument"
}
eflag_ignore_body()
{
    h_pass "true 1>&2" -e ignore
    h_pass "echo foo 1>&2" -e ignore
}

atf_test_case eflag_file
eflag_file_head()
{
    atf_set "descr" "Tests for the -e option using the 'file:' argument"
}
eflag_file_body()
{
    touch empty
    h_pass "true 1>&2" -e file:empty

    echo foo >text
    h_pass "echo foo 1>&2" -e file:text
    h_fail "echo bar 1>&2" -e file:text

    dd if=/dev/urandom of=bin bs=1k count=10
    h_pass "cat bin 1>&2" -e file:bin
}

atf_test_case eflag_inline
eflag_inline_head()
{
    atf_set "descr" "Tests for the -e option using the 'inline:' argument"
}
eflag_inline_body()
{
    h_pass "true 1>&2" -e inline:
    h_pass "echo foo bar 1>&2" -e inline:"foo bar\n"
    h_pass "printf 'foo bar' 1>&2" -e inline:"foo bar"
    h_pass "printf '\t\n\t\n' 1>&2" -e inline:"\t\n\t\n"
    h_pass "printf '\a\b\033\f\n\r\t\v' 1>&2" -e inline:"\a\b\e\f\n\r\t\v"
    h_pass "printf '\011\022\033\012' 1>&2" -e inline:"\011\022\033\012"

    h_fail "echo foo bar 1>&2" -e inline:"foo bar"
    h_fail "echo -n foo bar 1>&2" -e inline:"foo bar\n"
}

atf_test_case eflag_save
eflag_save_head()
{
    atf_set "descr" "Tests for the -e option using the 'save:' argument"
}
eflag_save_body()
{
    h_pass "echo foo 1>&2" -e save:out
    echo foo >exp
    cmp -s out exp || atf_fail "Saved output does not match expected results"
}

atf_test_case eflag_match
eflag_match_head()
{
    atf_set "descr" "Tests for the -e option using the 'match:' argument"
}
eflag_match_body()
{
    h_pass "printf no-newline 1>&2" -e "match:^no-newline"
    h_pass "echo line1 1>&2; echo foo bar 1>&2" -e "match:^foo"
    h_pass "echo foo bar 1>&2" -e "match:o b"
    h_fail "echo foo bar 1>&2" -e "match:baz"
    h_fail "echo foo bar 1>&2" -e "match:^bar"
}

atf_test_case eflag_multiple
eflag_multiple_head()
{
    atf_set "descr" "Tests for multiple occurrences of the -e option"
}
eflag_multiple_body()
{
    h_pass "echo foo bar 1>&2" -e match:foo -e match:bar
    h_pass "echo foo 1>&2; echo bar 1>&2" -e match:foo -e match:bar
    h_fail "echo foo baz 1>&2" -e match:bar -e match:foo
    h_fail "echo foo 1>&2; echo baz 1>&2" -e match:bar -e match:foo
}

atf_test_case eflag_negated
eflag_negated_head()
{
    atf_set "descr" "Tests for negated occurrences of the -e option"
}
eflag_negated_body()
{
    h_fail "echo foo 1>&2" -e empty
    h_pass "echo foo 1>&2" -e not-empty

    h_pass "echo foo bar 1>&2" -e match:foo
    h_fail "echo foo bar 1>&2" -e not-match:foo
}

atf_test_case stdin
stdin_head()
{
    atf_set "descr" "Tests that stdin is preserved"
}
stdin_body()
{
    echo "hello" | ${Atf_Check} -o match:"hello" cat || \
        atf_fail "atf-check does not seem to respect stdin"
}

atf_test_case unusual_umask
unusual_umask_head()
{
    atf_set "descr" "Tests that atf-check doesn't care about unusual umasks"
}
unusual_umask_body()
{
    for mask in 022 027 0222 0177 0777 ; do
        umask ${mask}
        ${Atf_Check} true || \
            atf_fail "atf-check failed with umask ${mask}"
    done
    umask 022
}

atf_init_test_cases()
{
    atf_add_test_case sflag_eq_ne
    atf_add_test_case sflag_exit
    atf_add_test_case sflag_ignore
    atf_add_test_case sflag_signal

    atf_add_test_case xflag

    atf_add_test_case vflag
    atf_add_test_case vflag_save
    atf_add_test_case vflag_empty
    atf_add_test_case vflag_no_newline
    atf_add_test_case vflag_failure
    atf_add_test_case vflag_repeat

    atf_add_test_case oflag_empty
    atf_add_test_case oflag_ignore
    atf_add_test_case oflag_file
    atf_add_test_case oflag_inline
    atf_add_test_case oflag_match
    atf_add_test_case oflag_save
    atf_add_test_case oflag_multiple
    atf_add_test_case oflag_negated

    atf_add_test_case eflag_empty
    atf_add_test_case eflag_ignore
    atf_add_test_case eflag_file
    atf_add_test_case eflag_inline
    atf_add_test_case eflag_match
    atf_add_test_case eflag_save
    atf_add_test_case eflag_multiple
    atf_add_test_case eflag_negated

    atf_add_test_case stdin

    atf_add_test_case unusual_umask
}

# vim: syntax=sh:expandtab:shiftwidth=4:softtabstop=4
