#
# Automated Testing Framework (atf)
#
# Copyright (c) 2007 The NetBSD Foundation, Inc.
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

create_helpers()
{
    mkdir dir1
    cp $(atf_get_srcdir)/pass_helper dir1/tp1
    cp $(atf_get_srcdir)/fail_helper dir1/tp2
    cp $(atf_get_srcdir)/pass_helper tp3
    cp $(atf_get_srcdir)/fail_helper tp4

    cat >tp5 <<EOF
#! $(atf-config -t atf_shell)
echo foo
EOF
    chmod +x tp5

    cat >Atffile <<EOF
Content-Type: application/X-atf-atffile; version="1"

prop: test-suite = atf

tp: dir1
tp: tp3
tp: tp4
tp: tp5
EOF

    cat >dir1/Atffile <<EOF
Content-Type: application/X-atf-atffile; version="1"

prop: test-suite = atf

tp: tp1
tp: tp2
EOF
}

run_helpers()
{
    mkdir etc
    cat >etc/atf-run.hooks <<EOF
#! $(atf-config -t atf_shell)

info_start_hook()
{
    atf_tps_writer_info "startinfo" "A value"
}

info_end_hook()
{
    atf_tps_writer_info "endinfo" "Another value"
}
EOF
    echo "Using atf-run to run helpers"
    ATF_CONFDIR=$(pwd)/etc atf-run >tps.out 2>/dev/null
    rm -rf etc
}

atf_test_case default
default_head()
{
    atf_set "descr" "Checks that the default output uses the ticker" \
                    "format"
}
default_body()
{
    create_helpers
    run_helpers

    # Check that the default output uses the ticker format.
    atf_check -s eq:0 -o match:'test cases' -o match:'Failed test cases' \
        -o match:'Summary for' -e empty -x 'atf-report <tps.out'
}

# XXX The test for all expect_ values should be intermixed with the other
# tests.  However, to do that, we need to migrate to using C helpers for
# simplicity in raising signals...
atf_test_case expect
expect_body()
{
    ln -s "$(atf_get_srcdir)/expect_helpers" .
    cat >Atffile <<EOF
Content-Type: application/X-atf-atffile; version="1"

prop: test-suite = atf

tp: expect_helpers
EOF
    run_helpers

# NO_CHECK_STYLE_BEGIN
    cat >expout <<EOF
tc, #.#, expect_helpers, death_and_exit, expected_death, Exit case
tc, #.#, expect_helpers, death_and_signal, expected_death, Signal case
tc, #.#, expect_helpers, death_but_pass, failed, Test case was expected to terminate abruptly but it continued execution
tc, #.#, expect_helpers, exit_any_and_exit, expected_exit, Call will exit
tc, #.#, expect_helpers, exit_but_pass, failed, Test case was expected to exit cleanly but it continued execution
tc, #.#, expect_helpers, exit_code_and_exit, expected_exit, Call will exit
tc, #.#, expect_helpers, fail_and_fail_check, expected_failure, And fail again: 2 checks failed as expected; see output for more details
tc, #.#, expect_helpers, fail_and_fail_requirement, expected_failure, Fail reason: The failure
tc, #.#, expect_helpers, fail_but_pass, failed, Test case was expecting a failure but none were raised
tc, #.#, expect_helpers, pass_and_pass, passed
tc, #.#, expect_helpers, pass_but_fail_check, failed, 1 checks failed; see output for more details
tc, #.#, expect_helpers, pass_but_fail_requirement, failed, Some reason
tc, #.#, expect_helpers, signal_any_and_signal, expected_signal, Call will signal
tc, #.#, expect_helpers, signal_but_pass, failed, Test case was expected to receive a termination signal but it continued execution
tc, #.#, expect_helpers, signal_no_and_signal, expected_signal, Call will signal
tc, #.#, expect_helpers, timeout_and_hang, expected_timeout, Will overrun
tc, #.#, expect_helpers, timeout_but_pass, failed, Test case was expected to hang but it continued execution
tp, #.#, expect_helpers, failed
EOF
# NO_CHECK_STYLE_END
    atf_check -s eq:0 -o file:expout -e empty -x \
        "atf-report -o csv:- <tps.out | " \
        "sed -E -e 's/[0-9]+.[0-9]{6}, /#.#, /'"

# NO_CHECK_STYLE_BEGIN
    cat >expout <<EOF
expect_helpers (1/1): 17 test cases
    death_and_exit: [#.#s] Expected failure: Exit case
    death_and_signal: [#.#s] Expected failure: Signal case
    death_but_pass: [#.#s] Failed: Test case was expected to terminate abruptly but it continued execution
    exit_any_and_exit: [#.#s] Expected failure: Call will exit
    exit_but_pass: [#.#s] Failed: Test case was expected to exit cleanly but it continued execution
    exit_code_and_exit: [#.#s] Expected failure: Call will exit
    fail_and_fail_check: [#.#s] Expected failure: And fail again: 2 checks failed as expected; see output for more details
    fail_and_fail_requirement: [#.#s] Expected failure: Fail reason: The failure
    fail_but_pass: [#.#s] Failed: Test case was expecting a failure but none were raised
    pass_and_pass: [#.#s] Passed.
    pass_but_fail_check: [#.#s] Failed: 1 checks failed; see output for more details
    pass_but_fail_requirement: [#.#s] Failed: Some reason
    signal_any_and_signal: [#.#s] Expected failure: Call will signal
    signal_but_pass: [#.#s] Failed: Test case was expected to receive a termination signal but it continued execution
    signal_no_and_signal: [#.#s] Expected failure: Call will signal
    timeout_and_hang: [#.#s] Expected failure: Will overrun
    timeout_but_pass: [#.#s] Failed: Test case was expected to hang but it continued execution
[#.#s]

Test cases for known bugs:
    expect_helpers:death_and_exit: Exit case
    expect_helpers:death_and_signal: Signal case
    expect_helpers:exit_any_and_exit: Call will exit
    expect_helpers:exit_code_and_exit: Call will exit
    expect_helpers:fail_and_fail_check: And fail again: 2 checks failed as expected; see output for more details
    expect_helpers:fail_and_fail_requirement: Fail reason: The failure
    expect_helpers:signal_any_and_signal: Call will signal
    expect_helpers:signal_no_and_signal: Call will signal
    expect_helpers:timeout_and_hang: Will overrun

Failed test cases:
    expect_helpers:death_but_pass, expect_helpers:exit_but_pass, expect_helpers:fail_but_pass, expect_helpers:pass_but_fail_check, expect_helpers:pass_but_fail_requirement, expect_helpers:signal_but_pass, expect_helpers:timeout_but_pass

Summary for 1 test programs:
    1 passed test cases.
    7 failed test cases.
    9 expected failed test cases.
    0 skipped test cases.
EOF
# NO_CHECK_STYLE_END
    atf_check -s eq:0 -o file:expout -e empty -x \
        "atf-report -o ticker:- <tps.out | " \
        "sed -E -e 's/[0-9]+.[0-9]{6}/#.#/'"

    # Just ensure that this does not crash for now...
    atf_check -s eq:0 -o ignore -e empty -x "atf-report -o xml:- <tps.out"
}

atf_test_case oflag
oflag_head()
{
    atf_set "descr" "Checks that the -o flag works"
}
oflag_body()
{
    create_helpers
    run_helpers

    # Get the default output.
    atf_check -s eq:0 -o save:stdout -e empty -x 'atf-report <tps.out'
    mv stdout defout

    # Check that changing the stdout output works.
    atf_check -s eq:0 -o save:stdout -e empty -x 'atf-report -o csv:- <tps.out'
    atf_check -s eq:1 -o empty -e empty cmp -s defout stdout
    cp stdout expcsv

    # Check that sending the output to a file does not write to stdout.
    atf_check -s eq:0 -o empty -e empty -x 'atf-report -o csv:fmt.out <tps.out'
    atf_check -s eq:0 -o empty -e empty cmp -s expcsv fmt.out
    rm -f fmt.out

    # Check that defining two outputs using the same format works.
    atf_check -s eq:0 -o empty -e empty -x \
              'atf-report -o csv:fmt.out -o csv:fmt2.out <tps.out'
    atf_check -s eq:0 -o empty -e empty cmp -s expcsv fmt.out
    atf_check -s eq:0 -o empty -e empty cmp -s fmt.out fmt2.out
    rm -f fmt.out fmt2.out

    # Check that defining two outputs using different formats works.
    atf_check -s eq:0 -o empty -e empty -x \
              'atf-report -o csv:fmt.out -o ticker:fmt2.out <tps.out'
    atf_check -s eq:0 -o empty -e empty cmp -s expcsv fmt.out
    atf_check -s eq:1 -o empty -e empty cmp -s fmt.out fmt2.out
    atf_check -s eq:0 -o ignore -e empty grep "test cases" fmt2.out
    atf_check -s eq:0 -o ignore -e empty grep "Failed test cases" fmt2.out
    atf_check -s eq:0 -o ignore -e empty grep "Summary for" fmt2.out
    rm -f fmt.out fmt2.out

    # Check that defining two outputs over the same file does not work.
    atf_check -s eq:1 -o empty -e match:'more than once' -x \
              'atf-report -o csv:fmt.out -o ticker:fmt.out <tps.out'
    rm -f fmt.out

    # Check that defining two outputs over stdout (but using different
    # paths) does not work.
    atf_check -s eq:1 -o empty -e match:'more than once' -x \
              'atf-report -o csv:- -o ticker:/dev/stdout <tps.out'
    rm -f fmt.out
}

atf_test_case output_csv
output_csv_head()
{
    atf_set "descr" "Checks the CSV output format"
}
output_csv_body()
{
    create_helpers
    run_helpers

# NO_CHECK_STYLE_BEGIN
    cat >expout <<EOF
tc, #.#, dir1/tp1, main, passed
tp, #.#, dir1/tp1, passed
tc, #.#, dir1/tp2, main, failed, This always fails
tp, #.#, dir1/tp2, failed
tc, #.#, tp3, main, passed
tp, #.#, tp3, passed
tc, #.#, tp4, main, failed, This always fails
tp, #.#, tp4, failed
tp, #.#, tp5, bogus, Invalid format for test case list: 1: Unexpected token \`<<NEWLINE>>'; expected \`:'
EOF
# NO_CHECK_STYLE_END

    atf_check -s eq:0 -o file:expout -e empty -x \
        "atf-report -o csv:- <tps.out | sed -E -e 's/[0-9]+.[0-9]{6}, /#.#, /'"
}

atf_test_case output_ticker
output_ticker_head()
{
    atf_set "descr" "Checks the ticker output format"
}
output_ticker_body()
{
    create_helpers
    run_helpers

# NO_CHECK_STYLE_BEGIN
    cat >expout <<EOF
dir1/tp1 (1/5): 1 test cases
    main: [#.#s] Passed.
[#.#s]

dir1/tp2 (2/5): 1 test cases
    main: [#.#s] Failed: This always fails
[#.#s]

tp3 (3/5): 1 test cases
    main: [#.#s] Passed.
[#.#s]

tp4 (4/5): 1 test cases
    main: [#.#s] Failed: This always fails
[#.#s]

tp5 (5/5): 0 test cases
tp5: BOGUS TEST PROGRAM: Cannot trust its results because of \`Invalid format for test case list: 1: Unexpected token \`<<NEWLINE>>'; expected \`:''
[#.#s]

Failed (bogus) test programs:
    tp5

Failed test cases:
    dir1/tp2:main, tp4:main

Summary for 5 test programs:
    2 passed test cases.
    2 failed test cases.
    0 expected failed test cases.
    0 skipped test cases.
EOF

    atf_check -s eq:0 -o file:expout -e empty -x \
        "atf-report -o ticker:- <tps.out | sed -E -e 's/[0-9]+.[0-9]{6}/#.#/'"
}
# NO_CHECK_STYLE_END

atf_test_case output_xml
output_xml_head()
{
    atf_set "descr" "Checks the XML output format"
}
output_xml_body()
{
    create_helpers
    run_helpers

# NO_CHECK_STYLE_BEGIN
    cat >expout <<EOF
<?xml version="1.0" encoding="ISO-8859-1"?>
<!DOCTYPE tests-results PUBLIC "-//NetBSD//DTD ATF Tests Results 0.1//EN" "http://www.NetBSD.org/XML/atf/tests-results.dtd">

<tests-results>
<info class="startinfo">A value</info>
<tp id="dir1/tp1">
<tc id="main">
<passed />
<tc-time>#.#</tc-time></tc>
<tp-time>#.#</tp-time></tp>
<tp id="dir1/tp2">
<tc id="main">
<failed>This always fails</failed>
<tc-time>#.#</tc-time></tc>
<tp-time>#.#</tp-time></tp>
<tp id="tp3">
<tc id="main">
<passed />
<tc-time>#.#</tc-time></tc>
<tp-time>#.#</tp-time></tp>
<tp id="tp4">
<tc id="main">
<failed>This always fails</failed>
<tc-time>#.#</tc-time></tc>
<tp-time>#.#</tp-time></tp>
<tp id="tp5">
<failed>Invalid format for test case list: 1: Unexpected token \`&lt;&lt;NEWLINE&gt;&gt;'; expected \`:'</failed>
<tp-time>#.#</tp-time></tp>
<info class="endinfo">Another value</info>
</tests-results>
EOF
# NO_CHECK_STYLE_END

    atf_check -s eq:0 -o file:expout -e empty -x \
        "atf-report -o xml:- < tps.out | sed -E -e 's/>[0-9]+.[0-9]{6}</>#.#</'"
}

atf_test_case output_xml_space
output_xml_space_head()
{
    atf_set "descr" "Checks that the XML output format properly preserves" \
                    "leading and trailing whitespace in stdout and stderr" \
                    "lines"
}
output_xml_space_body()
{
    export TESTCASE=diff
    cp $(atf_get_srcdir)/misc_helpers .
    cat >Atffile <<EOF
Content-Type: application/X-atf-atffile; version="1"

prop: test-suite = atf

tp: misc_helpers
EOF

# NO_CHECK_STYLE_BEGIN
    cat >expout <<EOF
<?xml version="1.0" encoding="ISO-8859-1"?>
<!DOCTYPE tests-results PUBLIC "-//NetBSD//DTD ATF Tests Results 0.1//EN" "http://www.NetBSD.org/XML/atf/tests-results.dtd">

<tests-results>
<info class="startinfo">A value</info>
<tp id="misc_helpers">
<tc id="diff">
<so>--- a	2007-11-04 14:00:41.000000000 +0100</so>
<so>+++ b	2007-11-04 14:00:48.000000000 +0100</so>
<so>@@ -1,7 +1,7 @@</so>
<so> This test is meant to simulate a diff.</so>
<so> Blank space at beginning of context lines must be preserved.</so>
<so> </so>
<so>-First original line.</so>
<so>-Second original line.</so>
<so>+First modified line.</so>
<so>+Second modified line.</so>
<so> </so>
<so> EOF</so>
<passed />
<tc-time>#.#</tc-time></tc>
<tp-time>#.#</tp-time></tp>
<info class="endinfo">Another value</info>
</tests-results>
EOF
# NO_CHECK_STYLE_END

    run_helpers
    atf_check -s eq:0 -o file:expout -e empty -x \
        "atf-report -o xml:- <tps.out | sed -E -e 's/>[0-9]+.[0-9]{6}</>#.#</'"
}

atf_test_case too_many_args
too_many_args_body()
{
    cat >experr <<EOF
atf-report: ERROR: No arguments allowed
EOF
    atf_check -s eq:1 -o empty -e file:experr atf-report foo
}

atf_init_test_cases()
{
    atf_add_test_case default
    atf_add_test_case expect
    atf_add_test_case oflag
    atf_add_test_case output_csv
    atf_add_test_case output_ticker
    atf_add_test_case output_xml
    atf_add_test_case output_xml_space
    atf_add_test_case too_many_args
}

# vim: syntax=sh:expandtab:shiftwidth=4:softtabstop=4
