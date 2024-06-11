#
# Copyright (c) 2022-2023 Klara, Inc.
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Name of user to use for -u tests when running as root.  Beware that
# processes owned by that user will be affected by the test.
TEST_USER=nobody

_renice() {
	atf_check -o empty -e ignore -s exit:0 renice "$@"
}

atf_check_nice_value() {
	local pid=$1
	local expected=$2
	local actual="$(ps -o nice= -p $pid)"
	atf_check test "$actual" -eq "$expected"
}

atf_test_case renice_abs_pid
renice_abs_pid_head() {
	atf_set "descr" "Set a process's nice number to an absolute value"
}
renice_abs_pid_body() {
	local pid nice incr
	sleep 60 &
	pid=$!
	nice="$(ps -o nice= -p $pid)"
	incr=3
	_renice $((nice+incr)) $pid
	atf_check_nice_value $pid $((nice+incr))
	kill $pid
}

atf_test_case renice_rel_pid
renice_rel_pid_head() {
	atf_set "descr" "Change a process's nice number by a relative value"
}
renice_rel_pid_body() {
	local pid nice incr
	sleep 60 &
	pid=$!
	nice="$(ps -o nice= -p $pid)"
	incr=3
	_renice -n $incr $pid
	_renice -p -n $incr $pid
	_renice -n $incr -p $pid
	atf_check_nice_value $pid $((nice+incr+incr+incr))
	kill $pid
}

atf_test_case renice_abs_pgid
renice_abs_pgid_head() {
	atf_set "descr" "Set a process group's nice number to an absolute value"
}
renice_abs_pgid_body() {
	local pid pgid nice incr
	# make sure target runs in a different pgrp than ours
	pid="$(sh -mc "sleep 60 >/dev/null & echo \$!")"
	pgid="$(ps -o pgid= -p $pid)"
	nice="$(ps -o nice= -p $pid)"
	incr=3
	_renice $((nice+incr)) -g $pgid
	atf_check_nice_value $pid $((nice+incr))
	kill $pid
}

atf_test_case renice_rel_pgid
renice_rel_pgid_head() {
	atf_set "descr" "Change a process group's nice number by a relative value"
}
renice_rel_pgid_body() {
	local pid pgid nice incr
	# make sure target runs in a different pgrp than ours
	pid="$(sh -mc "sleep 60 >/dev/null & echo \$!")"
	pgid="$(ps -o pgid= -p $pid)"
	nice="$(ps -o nice= -p $pid)"
	incr=3
	_renice -g -n $incr $pgid
	_renice -n $incr -g $pgid
	atf_check_nice_value $pid $((nice+incr+incr))
	kill $pid
}

atf_test_case renice_abs_user
renice_abs_user_head() {
	atf_set "descr" "Set a user's processes' nice numbers to an absolute value"
	atf_set "require.user" "root"
}
renice_abs_user_body() {
	local user pid nice incr
	pid="$(su -m $TEST_USER -c "/bin/sh -c 'sleep 60 >/dev/null & echo \$!'")"
	nice="$(ps -o nice= -p $pid)"
	incr=3
	_renice $((nice+incr)) -u $TEST_USER
	atf_check_nice_value $pid $((nice+incr))
	kill $pid
}

atf_test_case renice_rel_user
renice_rel_user_head() {
	atf_set "descr" "Change a user's processes' nice numbers by a relative value"
	atf_set "require.user" "root"
}
renice_rel_user_body() {
	local user pid nice incr
	pid="$(su -m $TEST_USER -c "/bin/sh -c 'sleep 60 >/dev/null & echo \$!'")"
	nice="$(ps -o nice= -p $pid)"
	incr=3
	_renice -u -n $incr $TEST_USER
	_renice -n $incr -u $TEST_USER
	atf_check_nice_value $pid $((nice+incr+incr))
	kill $pid
}

atf_test_case renice_delim
renice_delim_head() {
	atf_set "descr" "Test various delimiter positions"
}
renice_delim_body() {
	local pid nice incr
	sleep 60 &
	pid=$!
	nice="$(ps -o nice= -p $pid)"
	incr=0
	# without -p
	: $((incr=incr+1))
	_renice -- $((nice+incr)) $pid
	atf_check_nice_value $pid $((nice+incr))
	: $((incr=incr+1))
	_renice $((nice+incr)) -- $pid
	atf_check_nice_value $pid $((nice+incr))
	: $((incr=incr+1))
	_renice $((nice+incr)) $pid --
	atf_check_nice_value $pid $((nice+incr))
	# with -p
	: $((incr=incr+1))
	_renice -p -- $((nice+incr)) $pid
	atf_check_nice_value $pid $((nice+incr))
	: $((incr=incr+1))
	_renice -p $((nice+incr)) -- $pid
	atf_check_nice_value $pid $((nice+incr))
	: $((incr=incr+1))
	_renice -p $((nice+incr)) $pid --
	atf_check_nice_value $pid $((nice+incr))
	: $((incr=incr+1))
	_renice $((nice+incr)) -p -- $pid
	atf_check_nice_value $pid $((nice+incr))
	: $((incr=incr+1))
	_renice $((nice+incr)) -p $pid --
	atf_check_nice_value $pid $((nice+incr))
	kill $pid
}

atf_test_case renice_incr_noarg
renice_incr_noarg_head() {
	atf_set "descr" "Do not segfault if -n is given without an argument"
}
renice_incr_noarg_body() {
	atf_check -o empty -e ignore -s exit:1 renice -n
}

atf_init_test_cases() {
	atf_add_test_case renice_abs_pid
	atf_add_test_case renice_rel_pid
	atf_add_test_case renice_abs_pgid
	atf_add_test_case renice_rel_pgid
	atf_add_test_case renice_abs_user
	atf_add_test_case renice_rel_user
	atf_add_test_case renice_delim
	atf_add_test_case renice_incr_noarg
}
