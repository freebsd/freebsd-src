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

create_atffile()
{
    ATF_CONFDIR="$(pwd)"; export ATF_CONFDIR

    cat >Atffile <<EOF
Content-Type: application/X-atf-atffile; version="1"

prop: test-suite = atf

EOF
    for f in "${@}"; do
        echo "tp: ${f}" >>Atffile
    done
}

create_helper()
{
    cp $(atf_get_srcdir)/misc_helpers helper
    create_atffile helper
    TESTCASE=${1}; export TESTCASE
}

create_helper_stdin()
{
    # TODO: This really, really, really must use real test programs.
    cat >${1} <<EOF
#! $(atf-config -t atf_shell)
while [ \${#} -gt 0 ]; do
    case \${1} in
        -l)
            echo 'Content-Type: application/X-atf-tp; version="1"'
            echo
EOF
    cnt=1
    while [ ${cnt} -le ${2} ]; do
        echo "echo 'ident: tc${cnt}'" >>${1}
        [ ${cnt} -lt ${2} ] && echo "echo" >>${1}
        cnt=$((${cnt} + 1))
    done
cat >>${1} <<EOF
            exit 0
            ;;
        -r*)
            resfile=\$(echo \${1} | cut -d r -f 2-)
            ;;
    esac
    testcase=\$(echo \${1} | cut -d : -f 1)
    shift
done
EOF
    cat >>${1}
}

create_mount_helper()
{
    cat >${1} <<EOF
#! /usr/bin/env atf-sh

do_mount() {
    platform=\$(uname)
    case \${platform} in
    Linux|NetBSD)
        mount -t tmpfs tmpfs \${1} || atf_fail "Mount failed"
        ;;
    FreeBSD)
        mdmfs -s 16m md \${1} || atf_fail "Mount failed"
        ;;
    SunOS)
        mount -F tmpfs tmpfs \$(pwd)/\${1} || atf_fail "Mount failed"
        ;;
    *)
        atf_fail "create_mount_helper called for an unsupported platform."
        ;;
    esac
}

atf_test_case main
main_head() {
    atf_set "require.user" "root"
}
main_body() {
EOF
    cat >>${1}
    cat >>${1} <<EOF
}

atf_init_test_cases()
{
    atf_add_test_case main
}
EOF
}

atf_test_case no_warnings
no_warnings_head()
{
    atf_set "descr" "Tests that atf-run suppresses warnings about not running" \
                    "within atf-run"
}
no_warnings_body()
{
    create_helper pass
    atf_check -s eq:0 -o ignore -e not-match:'WARNING.*atf-run' atf-run helper
}

atf_test_case config
config_head()
{
    atf_set "descr" "Tests that the config files are read in the correct" \
                    "order"
}
config_body()
{
    create_helper config

    mkdir etc
    mkdir .atf

    echo "First: read system-wide common.conf."
    cat >etc/common.conf <<EOF
Content-Type: application/X-atf-config; version="1"

1st = "sw common"
2nd = "sw common"
3rd = "sw common"
4th = "sw common"
EOF
    atf_check -s eq:0 \
        -o match:'1st: sw common' \
        -o match:'2nd: sw common' \
        -o match:'3rd: sw common' \
        -o match:'4th: sw common' \
        -e ignore -x \
        "ATF_CONFDIR=$(pwd)/etc HOME=$(pwd) atf-run helper"

    echo "Second: read system-wide <test-suite>.conf."
    cat >etc/atf.conf <<EOF
Content-Type: application/X-atf-config; version="1"

1st = "sw atf"
EOF
    atf_check -s eq:0 \
        -o match:'1st: sw atf' \
        -o match:'2nd: sw common' \
        -o match:'3rd: sw common' \
        -o match:'4th: sw common' \
        -e ignore -x \
        "ATF_CONFDIR=$(pwd)/etc HOME=$(pwd) atf-run helper"

    echo "Third: read user-specific common.conf."
    cat >.atf/common.conf <<EOF
Content-Type: application/X-atf-config; version="1"

2nd = "us common"
EOF
    atf_check -s eq:0 \
        -o match:'1st: sw atf' \
        -o match:'2nd: us common' \
        -o match:'3rd: sw common' \
        -o match:'4th: sw common' \
        -e ignore -x \
        "ATF_CONFDIR=$(pwd)/etc HOME=$(pwd) atf-run helper"

    echo "Fourth: read user-specific <test-suite>.conf."
    cat >.atf/atf.conf <<EOF
Content-Type: application/X-atf-config; version="1"

3rd = "us atf"
EOF
    atf_check -s eq:0 \
        -o match:'1st: sw atf' \
        -o match:'2nd: us common' \
        -o match:'3rd: us atf' \
        -o match:'4th: sw common' \
        -e ignore -x \
        "ATF_CONFDIR=$(pwd)/etc HOME=$(pwd) atf-run helper"
}

atf_test_case vflag
vflag_head()
{
    atf_set "descr" "Tests that the -v flag works and that it properly" \
                    "overrides the values in configuration files"
}
vflag_body()
{
    create_helper testvar

    echo "Checking that 'testvar' is not defined."
    atf_check -s eq:1 -o ignore -e ignore -x \
        "ATF_CONFDIR=$(pwd)/etc atf-run helper"

    echo "Checking that defining 'testvar' trough '-v' works."
    atf_check -s eq:0 -o match:'testvar: a value' -e ignore -x \
        "ATF_CONFDIR=$(pwd)/etc atf-run -v testvar='a value' helper"

    echo "Checking that defining 'testvar' trough the configuration" \
         "file works."
    mkdir etc
    cat >etc/common.conf <<EOF
Content-Type: application/X-atf-config; version="1"

testvar = "value in conf file"
EOF
    atf_check -s eq:0 -o match:'testvar: value in conf file' -e ignore -x \
              "ATF_CONFDIR=$(pwd)/etc atf-run helper"

    echo "Checking that defining 'testvar' trough -v overrides the" \
         "configuration file."
    atf_check -s eq:0 -o match:'testvar: a value' -e ignore -x \
        "ATF_CONFDIR=$(pwd)/etc atf-run -v testvar='a value' helper"
}

atf_test_case atffile
atffile_head()
{
    atf_set "descr" "Tests that the variables defined by the Atffile" \
                    "are recognized and that they take the lowest priority"
}
atffile_body()
{
    create_helper testvar

    echo "Checking that 'testvar' is not defined."
    atf_check -s eq:1 -o ignore -e ignore -x \
              "ATF_CONFDIR=$(pwd)/etc atf-run helper"

    echo "Checking that defining 'testvar' trough the Atffile works."
    echo 'conf: testvar = "a value"' >>Atffile
    atf_check -s eq:0 -o match:'testvar: a value' -e ignore -x \
              "ATF_CONFDIR=$(pwd)/etc atf-run helper"

    echo "Checking that defining 'testvar' trough the configuration" \
         "file overrides the one in the Atffile."
    mkdir etc
    cat >etc/common.conf <<EOF
Content-Type: application/X-atf-config; version="1"

testvar = "value in conf file"
EOF
    atf_check -s eq:0 -o match:'testvar: value in conf file' -e ignore -x \
              "ATF_CONFDIR=$(pwd)/etc atf-run helper"
    rm -rf etc

    echo "Checking that defining 'testvar' trough -v overrides the" \
         "one in the Atffile."
    atf_check -s eq:0 -o match:'testvar: new value' -e ignore -x \
        "ATF_CONFDIR=$(pwd)/etc atf-run -v testvar='new value' helper"
}

atf_test_case atffile_recursive
atffile_recursive_head()
{
    atf_set "descr" "Tests that variables defined by an Atffile are not" \
                    "inherited by other Atffiles."
}
atffile_recursive_body()
{
    create_helper testvar

    mkdir dir
    mv Atffile helper dir

    echo "Checking that 'testvar' is not inherited."
    create_atffile dir
    echo 'conf: testvar = "a value"' >> Atffile
    atf_check -s eq:1 -o ignore -e ignore -x "ATF_CONFDIR=$(pwd)/etc atf-run"

    echo "Checking that defining 'testvar' in the correct Atffile works."
    echo 'conf: testvar = "a value"' >>dir/Atffile
    atf_check -s eq:0 -o match:'testvar: a value' -e ignore -x \
              "ATF_CONFDIR=$(pwd)/etc atf-run"
}

atf_test_case fds
fds_head()
{
    atf_set "descr" "Tests that all streams are properly captured"
}
fds_body()
{
    create_helper fds

    atf_check -s eq:0 \
        -o match:'^tc-so:msg1 to stdout$' \
        -o match:'^tc-so:msg2 to stdout$' \
        -o match:'^tc-se:msg1 to stderr$' \
        -o match:'^tc-se:msg2 to stderr$' \
        -e empty atf-run
}

atf_test_case mux_streams
mux_streams_head()
{
    atf_set "descr" "Tests for a race condition in stream multiplexing"
}
mux_streams_body()
{
    create_helper mux_streams

    for i in 1 2 3 4 5; do
        echo "Attempt ${i}"
        atf_check -s eq:0 -o match:'stdout 9999' -o match:'stderr 9999' atf-run
    done
}

atf_test_case expect
expect_head()
{
    atf_set "descr" "Tests the processing of test case results and the" \
        "expect features"
}
expect_body()
{
    ln -s "$(atf_get_srcdir)/expect_helpers" .
    create_atffile expect_helpers

    atf_check -s eq:1 \
        -o match:'death_and_exit, expected_death' \
        -o match:'death_and_signal, expected_death' \
        -o match:'death_but_pass, failed' \
        -o match:'exit_any_and_exit, expected_exit' \
        -o match:'exit_but_pass, failed' \
        -o match:'exit_code_and_exit, expected_exit' \
        -o match:'fail_and_fail_check, expected_failure' \
        -o match:'fail_and_fail_requirement, expected_failure' \
        -o match:'fail_but_pass, failed' \
        -o match:'pass_and_pass, passed' \
        -o match:'pass_but_fail_check, failed' \
        -o match:'pass_but_fail_requirement, failed' \
        -o match:'signal_any_and_signal, expected_signal' \
        -o match:'signal_but_pass, failed' \
        -o match:'signal_no_and_signal, expected_signal' \
        -o match:'timeout_and_hang, expected_timeout' \
        -o match:'timeout_but_pass, failed' \
        -e empty atf-run
}

atf_test_case missing_results
missing_results_head()
{
    atf_set "descr" "Ensures that atf-run correctly handles test cases that " \
                    "do not create the results file"
}
missing_results_body()
{
    create_helper_stdin helper 1 <<EOF
test -f \${resfile} && echo "resfile found"
exit 0
EOF
    chmod +x helper

    create_atffile helper

    re='^tc-end: [0-9][0-9]*\.[0-9]*, tc1,'
    atf_check -s eq:1 \
        -o match:"${re} failed,.*failed to create" \
        -o not-match:'resfile found' \
        -e empty atf-run
}

atf_test_case broken_results
broken_results_head()
{
    atf_set "descr" "Ensures that atf-run reports test programs that" \
                    "provide a bogus results output as broken programs"
}
broken_results_body()
{
    # We produce two errors from the header to ensure that the parse
    # errors are printed on a single line on the output file.  Printing
    # them on separate lines would be incorrect.
    create_helper_stdin helper 1 <<EOF
echo 'line 1' >\${resfile}
echo 'line 2' >>\${resfile}
exit 0
EOF
    chmod +x helper

    create_atffile helper

    re='^tc-end: [0-9][0-9]*\.[0-9]*, tc1,'
    atf_check -s eq:1 -o match:"${re} .*line 1.*line 2" -e empty atf-run
}

atf_test_case broken_tp_list
broken_tp_list_head()
{
    atf_set "descr" "Ensures that atf-run reports test programs that" \
                    "provide a bogus test case list"
}
broken_tp_list_body()
{
    cat >helper <<EOF
#! $(atf-config -t atf_shell)
while [ \${#} -gt 0 ]; do
    if [ \${1} = -l ]; then
        echo 'Content-Type: application/X-atf-tp; version="1"'
        echo
        echo 'foo: bar'
        exit 0
    else
        shift
    fi
done
exit 0
EOF
    chmod +x helper

    create_atffile helper

    re='^tp-end: [0-9][0-9]*\.[0-9]*, helper,'
    re="${re} Invalid format for test case list:.*First property.*ident"
    atf_check -s eq:1 -o match:"${re}" -e empty atf-run
}

atf_test_case zero_tcs
zero_tcs_head()
{
    atf_set "descr" "Ensures that atf-run reports test programs without" \
                    "test cases as errors"
}
zero_tcs_body()
{
    create_helper_stdin helper 0 <<EOF
echo 'Content-Type: application/X-atf-tp; version="1"'
echo
exit 1
EOF
    chmod +x helper

    create_atffile helper

    re='^tp-end: [0-9][0-9]*\.[0-9]*, helper,'
    atf_check -s eq:1 \
        -o match:"${re} .*Invalid format for test case list" \
        -e empty atf-run
}

atf_test_case exit_codes
exit_codes_head()
{
    atf_set "descr" "Ensures that atf-run reports bogus exit codes for" \
                    "programs correctly"
}
exit_codes_body()
{
    create_helper_stdin helper 1 <<EOF
echo "failed: Yes, it failed" >\${resfile}
exit 0
EOF
    chmod +x helper

    create_atffile helper

    re='^tc-end: [0-9][0-9]*\.[0-9]*, tc1,'
    atf_check -s eq:1 \
        -o match:"${re} .*exited successfully.*reported failure" \
        -e empty atf-run
}

atf_test_case signaled
signaled_head()
{
    atf_set "descr" "Ensures that atf-run reports test program's crashes" \
                    "correctly regardless of their actual results"
}
signaled_body()
{
    create_helper_stdin helper 2 <<EOF
echo "passed" >\${resfile}
case \${testcase} in
    tc1) ;;
    tc2) echo "Killing myself!" ; kill -9 \$\$ ;;
esac
EOF
    chmod +x helper

    create_atffile helper

    re='^tc-end: [0-9][0-9]*\.[0-9]*, tc2,'
    atf_check -s eq:1 -o match:"${re} .*received signal 9" \
        -e empty atf-run
}

atf_test_case hooks
hooks_head()
{
    atf_set "descr" "Checks that the default hooks work and that they" \
                    "can be overriden by the user"
}
hooks_body()
{
    cp $(atf_get_srcdir)/pass_helper helper
    create_atffile helper

    mkdir atf
    mkdir .atf

    echo "Checking default hooks"
    atf_check -s eq:0 -o match:'^info: time.start, ' \
        -o match:'^info: time.end, ' -e empty -x \
        "ATF_CONFDIR=$(pwd)/atf atf-run"

    echo "Checking the system-wide info_start hook"
    cat >atf/atf-run.hooks <<EOF
info_start_hook()
{
    atf_tps_writer_info "test" "sw value"
}
EOF
    atf_check -s eq:0 \
        -o match:'^info: test, sw value' \
        -o not-match:'^info: time.start, ' \
        -o match:'^info: time.end, ' \
        -e empty -x \
        "ATF_CONFDIR=$(pwd)/atf atf-run"

    echo "Checking the user-specific info_start hook"
    cat >.atf/atf-run.hooks <<EOF
info_start_hook()
{
    atf_tps_writer_info "test" "user value"
}
EOF
    atf_check -s eq:0 \
        -o match:'^info: test, user value' \
        -o not-match:'^info: time.start, ' \
        -o match:'^info: time.end, ' \
        -e empty -x \
        "ATF_CONFDIR=$(pwd)/atf atf-run"

    rm atf/atf-run.hooks
    rm .atf/atf-run.hooks

    echo "Checking the system-wide info_end hook"
    cat >atf/atf-run.hooks <<EOF
info_end_hook()
{
    atf_tps_writer_info "test" "sw value"
}
EOF
    atf_check -s eq:0 \
        -o match:'^info: time.start, ' \
        -o not-match:'^info: time.end, ' \
        -o match:'^info: test, sw value' \
        -e empty -x \
        "ATF_CONFDIR=$(pwd)/atf atf-run"

    echo "Checking the user-specific info_end hook"
    cat >.atf/atf-run.hooks <<EOF
info_end_hook()
{
    atf_tps_writer_info "test" "user value"
}
EOF
    atf_check -s eq:0 \
        -o match:'^info: time.start, ' \
        -o not-match:'^info: time.end, ' \
        -o match:'^info: test, user value' \
        -e empty -x \
         "ATF_CONFDIR=$(pwd)/atf atf-run"
}

atf_test_case isolation_env
isolation_env_head()
{
    atf_set "descr" "Tests that atf-run sets a set of environment variables" \
                    "to known sane values"
}
isolation_env_body()
{
    undef_vars="LANG LC_ALL LC_COLLATE LC_CTYPE LC_MESSAGES LC_MONETARY \
                LC_NUMERIC LC_TIME"
    def_vars="HOME TZ"

    mangleenv="env"
    for v in ${undef_vars} ${def_vars}; do
        mangleenv="${mangleenv} ${v}=bogus-value"
    done

    create_helper env_list
    create_atffile helper

    # We must ignore stderr in this call (instead of specifying -e empty)
    # because, when atf-run invokes the shell to run the hooks, we may get
    # error messages about an invalid locale.  This happens, at least, when
    # the shell is bash 4.x.
    atf_check -s eq:0 -o save:stdout -e ignore ${mangleenv} atf-run helper

    for v in ${undef_vars}; do
        atf_check -s eq:1 -o empty -e empty grep "^tc-so:${v}=" stdout
    done

    for v in ${def_vars}; do
        atf_check -s eq:0 -o ignore -e empty grep "^tc-so:${v}=" stdout
    done

    atf_check -s eq:0 -o ignore -e empty grep "^tc-so:TZ=UTC" stdout
}

atf_test_case isolation_home
isolation_home_head()
{
    atf_set "descr" "Tests that atf-run sets HOME to a sane and valid value"
}
isolation_home_body()
{
    create_helper env_home
    create_atffile helper
    atf_check -s eq:0 -o ignore -e ignore env HOME=foo atf-run helper
}

atf_test_case isolation_stdin
isolation_stdin_head()
{
    atf_set "descr" "Tests that atf-run nullifies the stdin of test cases"
}
isolation_stdin_body()
{
    create_helper read_stdin
    create_atffile helper
    atf_check -s eq:0 -o ignore -e ignore -x 'echo hello world | atf-run helper'
}

atf_test_case isolation_umask
isolation_umask_head()
{
    atf_set "descr" "Tests that atf-run sets the umask to a known value"
}
isolation_umask_body()
{
    create_helper umask
    create_atffile helper

    atf_check -s eq:0 -o match:'umask: 0022' -e ignore -x \
        "umask 0000 && atf-run helper"
}

atf_test_case cleanup_pass
cleanup_pass_head()
{
    atf_set "descr" "Tests that atf-run calls the cleanup routine of the test" \
        "case when the test case result is passed"
}
cleanup_pass_body()
{
    create_helper cleanup_states
    create_atffile helper

    atf_check -s eq:0 -o match:'cleanup_states, passed' -e ignore atf-run \
        -v state=pass -v statedir=$(pwd) helper
    test -f to-stay || atf_fail "Test case body did not run correctly"
    if [ -f to-delete ]; then
        atf_fail "Test case cleanup did not run correctly"
    fi
}

atf_test_case cleanup_fail
cleanup_fail_head()
{
    atf_set "descr" "Tests that atf-run calls the cleanup routine of the test" \
        "case when the test case result is failed"
}
cleanup_fail_body()
{
    create_helper cleanup_states
    create_atffile helper

    atf_check -s eq:1 -o match:'cleanup_states, failed' -e ignore atf-run \
        -v state=fail -v statedir=$(pwd) helper
    test -f to-stay || atf_fail "Test case body did not run correctly"
    if [ -f to-delete ]; then
        atf_fail "Test case cleanup did not run correctly"
    fi
}

atf_test_case cleanup_skip
cleanup_skip_head()
{
    atf_set "descr" "Tests that atf-run calls the cleanup routine of the test" \
        "case when the test case result is skipped"
}
cleanup_skip_body()
{
    create_helper cleanup_states
    create_atffile helper

    atf_check -s eq:0 -o match:'cleanup_states, skipped' -e ignore atf-run \
        -v state=skip -v statedir=$(pwd) helper
    test -f to-stay || atf_fail "Test case body did not run correctly"
    if [ -f to-delete ]; then
        atf_fail "Test case cleanup did not run correctly"
    fi
}

atf_test_case cleanup_curdir
cleanup_curdir_head()
{
    atf_set "descr" "Tests that atf-run calls the cleanup routine in the same" \
        "work directory as the body so that they can share data"
}
cleanup_curdir_body()
{
    create_helper cleanup_curdir
    create_atffile helper

    atf_check -s eq:0 -o match:'cleanup_curdir, passed' \
        -o match:'Old value: 1234' -e ignore atf-run helper
}

atf_test_case cleanup_signal
cleanup_signal_head()
{
    atf_set "descr" "Tests that atf-run calls the cleanup routine if it gets" \
        "a termination signal while running the body"
}
cleanup_signal_body()
{
    : # TODO: Write this.
}

atf_test_case cleanup_mount
cleanup_mount_head()
{
    atf_set "descr" "Tests that the removal algorithm does not cross" \
                    "mount points"
    atf_set "require.user" "root"
}
cleanup_mount_body()
{
    ROOT="$(pwd)/root"; export ROOT

    create_mount_helper helper <<EOF
echo \$(pwd) >\${ROOT}
mkdir foo
mkdir foo/bar
mkdir foo/bar/mnt
do_mount foo/bar/mnt
mkdir foo/baz
do_mount foo/baz
mkdir foo/baz/foo
mkdir foo/baz/foo/bar
do_mount foo/baz/foo/bar
EOF
    create_atffile helper
    chmod +x helper

    platform=$(uname)
    case ${platform} in
    Linux|FreeBSD|NetBSD|SunOS)
        ;;
    *)
        # XXX Possibly specify in meta-data too.
        atf_skip "Test unimplemented in this platform (${platform})"
        ;;
    esac

    atf_check -s eq:0 -o match:"main, passed" -e ignore atf-run helper
    mount | grep $(cat root) && atf_fail "Some file systems remain mounted"
    atf_check -s eq:1 -o empty -e empty test -d $(cat root)/foo
}

atf_test_case cleanup_symlink
cleanup_symlink_head()
{
    atf_set "descr" "Tests that the removal algorithm does not follow" \
                    "symlinks, which may live in another device and thus" \
                    "be treated as mount points"
    atf_set "require.user" "root"
}
cleanup_symlink_body()
{
    ROOT="$(pwd)/root"; export ROOT

    create_mount_helper helper <<EOF
echo \$(pwd) >\${ROOT}
atf_check -s eq:0 -o empty -e empty mkdir foo
atf_check -s eq:0 -o empty -e empty mkdir foo/bar
do_mount foo/bar
atf_check -s eq:0 -o empty -e empty touch a
atf_check -s eq:0 -o empty -e empty ln -s "\$(pwd)/a" foo/bar
EOF
    create_atffile helper
    chmod +x helper

    platform=$(uname)
    case ${platform} in
    Linux|FreeBSD|NetBSD|SunOS)
        ;;
    *)
        # XXX Possibly specify in meta-data too.
        atf_skip "Test unimplemented in this platform (${platform})"
        ;;
    esac

    atf_check -s eq:0 -o match:"main, passed" -e ignore atf-run helper
    mount | grep $(cat root) && atf_fail "Some file systems remain mounted"
    atf_check -s eq:1 -o empty -e empty test -d $(cat root)/foo
}

atf_test_case require_arch
require_arch_head()
{
    atf_set "descr" "Tests that atf-run validates the require.arch property"
}
require_arch_body()
{
    create_helper require_arch
    create_atffile helper

    echo "Checking for the real architecture"
    arch=$(atf-config -t atf_arch)
    atf_check -s eq:0 -o match:"${TESTCASE}, passed" -e ignore atf-run \
        -v arch="${arch}" helper
    atf_check -s eq:0 -o match:"${TESTCASE}, passed" -e ignore atf-run \
        -v arch="foo ${arch}" helper
    atf_check -s eq:0 -o match:"${TESTCASE}, passed" -e ignore atf-run \
        -v arch="${arch} foo" helper

    echo "Checking for a fictitious architecture"
    arch=fictitious
    export ATF_ARCH=fictitious
    atf_check -s eq:0 -o match:"${TESTCASE}, passed" -e ignore atf-run \
        -v arch="${arch}" helper
    atf_check -s eq:0 -o match:"${TESTCASE}, passed" -e ignore atf-run \
        -v arch="foo ${arch}" helper
    atf_check -s eq:0 -o match:"${TESTCASE}, passed" -e ignore atf-run \
        -v arch="${arch} foo" helper

    echo "Triggering some failures"
    atf_check -s eq:0 -o match:"${TESTCASE}, skipped, .*foo.*architecture" \
        -e ignore atf-run -v arch="foo" helper
    atf_check -s eq:0 \
        -o match:"${TESTCASE}, skipped, .*foo bar.*architectures" -e ignore \
        atf-run -v arch="foo bar" helper
    atf_check -s eq:0 \
        -o match:"${TESTCASE}, skipped, .*fictitiousxxx.*architecture" \
        -e ignore atf-run -v arch="${arch}xxx" helper
}

atf_test_case require_config
require_config_head()
{
    atf_set "descr" "Tests that atf-run validates the require.config property"
}
require_config_body()
{
    create_helper require_config
    create_atffile helper

    atf_check -s eq:0 -o match:"${TESTCASE}, skipped, .*var1.*not defined" \
        -e ignore atf-run helper
    atf_check -s eq:0 -o match:"${TESTCASE}, skipped, .*var2.*not defined" \
        -e ignore atf-run -v var1=foo helper
    atf_check -s eq:0 -o match:"${TESTCASE}, passed" -e ignore atf-run \
        -v var1=a -v var2=' ' helper
}

atf_test_case require_files
require_files_head()
{
    atf_set "descr" "Tests that atf-run validates the require.files property"
}
require_files_body()
{
    create_helper require_files
    create_atffile helper

    touch i-exist

    echo "Checking absolute paths"
    atf_check -s eq:0 -o match:"${TESTCASE}, passed" -e ignore atf-run \
        -v files='/bin/cp' helper
    atf_check -s eq:0 -o match:"${TESTCASE}, passed" -e ignore atf-run \
        -v files="$(pwd)/i-exist" helper
    atf_check -s eq:0 \
        -o match:"${TESTCASE}, skipped, .*/dont-exist" \
        -e ignore atf-run -v files="$(pwd)/i-exist $(pwd)/dont-exist" helper

    echo "Checking that relative paths are not allowed"
    atf_check -s eq:1 \
        -o match:"${TESTCASE}, failed, Relative paths.*not allowed.*hello" \
        -e ignore atf-run -v files='hello' helper
    atf_check -s eq:1 \
        -o match:"${TESTCASE}, failed, Relative paths.*not allowed.*a/b" \
        -e ignore atf-run -v files='a/b' helper
}

atf_test_case require_machine
require_machine_head()
{
    atf_set "descr" "Tests that atf-run validates the require.machine property"
}
require_machine_body()
{
    create_helper require_machine
    create_atffile helper

    echo "Checking for the real machine type"
    machine=$(atf-config -t atf_machine)
    atf_check -s eq:0 -o match:"${TESTCASE}, passed" -e ignore atf-run \
        -v machine="${machine}" helper
    atf_check -s eq:0 -o match:"${TESTCASE}, passed" -e ignore atf-run \
        -v machine="foo ${machine}" helper
    atf_check -s eq:0 -o match:"${TESTCASE}, passed" -e ignore atf-run \
        -v machine="${machine} foo" helper

    echo "Checking for a fictitious machine type"
    machine=fictitious
    export ATF_MACHINE=fictitious
    atf_check -s eq:0 -o match:"${TESTCASE}, passed" -e ignore atf-run \
        -v machine="${machine}" helper
    atf_check -s eq:0 -o match:"${TESTCASE}, passed" -e ignore atf-run \
        -v machine="foo ${machine}" helper
    atf_check -s eq:0 -o match:"${TESTCASE}, passed" -e ignore atf-run \
        -v machine="${machine} foo" helper

    echo "Triggering some failures"
    atf_check -s eq:0 -o match:"${TESTCASE}, skipped, .*foo.*machine type" \
        -e ignore atf-run -v machine="foo" helper
    atf_check -s eq:0 \
        -o match:"${TESTCASE}, skipped, .*foo bar.*machine types" -e ignore \
        atf-run -v machine="foo bar" helper
    atf_check -s eq:0 \
        -o match:"${TESTCASE}, skipped, .*fictitiousxxx.*machine type" \
        -e ignore atf-run -v machine="${machine}xxx" helper
}

atf_test_case require_progs
require_progs_head()
{
    atf_set "descr" "Tests that atf-run validates the require.progs property"
}
require_progs_body()
{
    create_helper require_progs
    create_atffile helper

    echo "Checking absolute paths"
    atf_check -s eq:0 -o match:"${TESTCASE}, passed" -e ignore atf-run \
        -v progs='/bin/cp' helper
    atf_check -s eq:0 \
        -o match:"${TESTCASE}, skipped, .*/bin/__non-existent__.*PATH" \
        -e ignore atf-run -v progs='/bin/__non-existent__' helper

    echo "Checking that relative paths are not allowed"
    atf_check -s eq:1 \
        -o match:"${TESTCASE}, failed, Relative paths.*not allowed.*bin/cp" \
        -e ignore atf-run -v progs='bin/cp' helper

    echo "Check plain file names, searching them in the PATH."
    atf_check -s eq:0 -o match:"${TESTCASE}, passed" -e ignore atf-run \
        -v progs='cp' helper
    atf_check -s eq:0 \
        -o match:"${TESTCASE}, skipped, .*__non-existent__.*PATH" -e ignore \
        atf-run -v progs='__non-existent__' helper
}

atf_test_case require_user_root
require_user_root_head()
{
    atf_set "descr" "Tests that atf-run validates the require.user property" \
        "when it is set to 'root'"
}
require_user_root_body()
{
    create_helper require_user
    create_atffile helper

    if [ $(id -u) -eq 0 ]; then
        exp=passed
    else
        exp=skipped
    fi
    atf_check -s eq:0 -o match:"${TESTCASE}, ${exp}" -e ignore atf-run \
        -v user=root helper
}

atf_test_case require_user_unprivileged
require_user_unprivileged_head()
{
    atf_set "descr" "Tests that atf-run validates the require.user property" \
        "when it is set to 'root'"
}
require_user_unprivileged_body()
{
    create_helper require_user
    create_atffile helper

    if [ $(id -u) -eq 0 ]; then
        exp=skipped
    else
        exp=passed
    fi
    atf_check -s eq:0 -o match:"${TESTCASE}, ${exp}" -e ignore atf-run \
        -v user=unprivileged helper
}

atf_test_case require_user_bad
require_user_bad_head()
{
    atf_set "descr" "Tests that atf-run validates the require.user property" \
        "when it is set to 'root'"
}
require_user_bad_body()
{
    create_helper require_user
    create_atffile helper

    atf_check -s eq:1 -o match:"${TESTCASE}, failed, Invalid value.*foobar" \
        -e ignore atf-run -v user=foobar helper
}

atf_test_case timeout
timeout_head()
{
    atf_set "descr" "Tests that atf-run kills a test case that times out"
}
timeout_body()
{
    create_helper timeout
    create_atffile helper

    atf_check -s eq:1 \
        -o match:"${TESTCASE}, failed, .*timed out after 1 second" -e ignore \
        atf-run -v statedir=$(pwd) helper
    if [ -f finished ]; then
        atf_fail "Test case was not killed after time out"
    fi
}

atf_test_case timeout_forkexit
timeout_forkexit_head()
{
    atf_set "descr" "Tests that atf-run deals gracefully with a test program" \
        "that forks, exits, but the child process hangs"
}
timeout_forkexit_body()
{
    create_helper timeout_forkexit
    create_atffile helper

    atf_check -s eq:0 -o match:"${TESTCASE}, passed" -e ignore atf-run \
        -v statedir=$(pwd) helper
    test -f parent-finished || atf_fail "Parent did not exit as expected"
    test -f child-finished && atf_fail "Subprocess exited but it should have" \
        "been forcibly terminated" || true
}

atf_test_case ignore_deprecated_use_fs
ignore_deprecated_use_fs_head()
{
    atf_set "descr" "Tests that atf-run ignores the deprecated use.fs property"
}
ignore_deprecated_use_fs_body()
{
    create_helper use_fs
    create_atffile helper

    atf_check -s eq:0 -o ignore -e ignore atf-run helper
}

atf_init_test_cases()
{
    atf_add_test_case no_warnings
    atf_add_test_case config
    atf_add_test_case vflag
    atf_add_test_case atffile
    atf_add_test_case atffile_recursive
    atf_add_test_case expect
    atf_add_test_case fds
    atf_add_test_case mux_streams
    atf_add_test_case missing_results
    atf_add_test_case broken_results
    atf_add_test_case broken_tp_list
    atf_add_test_case zero_tcs
    atf_add_test_case exit_codes
    atf_add_test_case signaled
    atf_add_test_case hooks
    atf_add_test_case isolation_env
    atf_add_test_case isolation_home
    atf_add_test_case isolation_stdin
    atf_add_test_case isolation_umask
    atf_add_test_case cleanup_pass
    atf_add_test_case cleanup_fail
    atf_add_test_case cleanup_skip
    atf_add_test_case cleanup_curdir
    atf_add_test_case cleanup_signal
    atf_add_test_case cleanup_mount
    atf_add_test_case cleanup_symlink
    atf_add_test_case require_arch
    atf_add_test_case require_config
    atf_add_test_case require_files
    atf_add_test_case require_machine
    atf_add_test_case require_progs
    atf_add_test_case require_user_root
    atf_add_test_case require_user_unprivileged
    atf_add_test_case require_user_bad
    atf_add_test_case timeout
    atf_add_test_case timeout_forkexit
    atf_add_test_case ignore_deprecated_use_fs
}

# vim: syntax=sh:expandtab:shiftwidth=4:softtabstop=4
