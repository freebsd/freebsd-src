#!/bin/sh
#-
# Copyright (c) 2022 Klara, Inc.
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
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# Name of user to use for -u tests when running as root.  Beware that
# processes owned by that user will be affected by the test.
TEST_USER=nobody

_renice() {
	atf_check -o empty -e ignore -s exit:0 renice "$@"
}

# Set a process's nice number to an absolute value
atf_test_case renice_abs_pid
renice_abs_pid_body() {
	local pid nice incr
	sleep 60 &
	pid=$!
	nice="$(ps -o nice= -p $pid)"
	incr=3
	_renice $((nice+incr)) $pid
	atf_check_equal $((nice+incr)) "$(ps -o nice= -p $pid)"
	kill $pid
}

# Change a process's nice number by a relative value
atf_test_case renice_rel_pid
renice_rel_pid_body() {
	local pid nice incr
	sleep 60 &
	pid=$!
	nice="$(ps -o nice= -p $pid)"
	incr=3
	_renice -n $incr $pid
	_renice -p -n $incr $pid
	_renice -n $incr -p $pid
	atf_check_equal $((nice+incr+incr+incr)) "$(ps -o nice= -p $pid)"
	kill $pid
}

# Set a process group's nice number to an absolute value
atf_test_case renice_abs_pgid
renice_abs_pgid_body() {
	local pid pgid nice incr
	# make sure target runs in a different pgrp than ours
	pid=$(sh -mc "sleep 60 >/dev/null & echo \$!")
	pgid="$(ps -o pgid= -p $pid)"
	nice="$(ps -o nice= -p $pid)"
	incr=3
	_renice $((nice+incr)) -g $pgid
	atf_check_equal $((nice+incr)) "$(ps -o nice= -p $pid)"
	kill $pid
}

# Change a process group's nice number by a relative value
atf_test_case renice_rel_pgid
renice_rel_pgid_body() {
	local pid pgid nice incr
	# make sure target runs in a different pgrp than ours
	pid=$(sh -mc "sleep 60 >/dev/null & echo \$!")
	pgid="$(ps -o pgid= -p $pid)"
	nice="$(ps -o nice= -p $pid)"
	incr=3
	_renice -g -n $incr $pgid
	_renice -n $incr -g $pgid
	atf_check_equal $((nice+incr+incr)) "$(ps -o nice= -p $pid)"
	kill $pid
}

# Set a user's processes' nice numbers to an absolute value
atf_test_case renice_abs_user
renice_abs_user_head() {
	atf_set "require.user" "root"
}
renice_abs_user_body() {
	local user pid nice incr
	pid=$(su -m $TEST_USER -c "sleep 60 >/dev/null & echo \$!")
	nice="$(ps -o nice= -p $pid)"
	incr=3
	_renice $((nice+incr)) -u $TEST_USER
	atf_check_equal $((nice+incr)) "$(ps -o nice= -p $pid)"
	kill $pid
}

# Change a user's processes' nice numbers by a relative value
atf_test_case renice_rel_user
renice_rel_user_head() {
	atf_set "require.user" "root"
}
renice_rel_user_body() {
	local user pid nice incr
	pid=$(su -m $TEST_USER -c "sleep 60 >/dev/null & echo \$!")
	nice="$(ps -o nice= -p $pid)"
	incr=3
	_renice -u -n $incr $TEST_USER
	_renice -n $incr -u $TEST_USER
	atf_check_equal $((nice+incr+incr)) "$(ps -o nice= -p $pid)"
	kill $pid
}

# Test various delimiter positions
atf_test_case renice_delim
renice_delim_body() {
	local pid nice incr
	sleep 60 &
	pid=$!
	nice="$(ps -o nice= -p $pid)"
	incr=0
	# without -p
	: $((incr=incr+1))
	_renice -- $((nice+incr)) $pid
	atf_check_equal $((nice+incr)) "$(ps -o nice= -p $pid)"
	: $((incr=incr+1))
	_renice $((nice+incr)) -- $pid
	atf_check_equal $((nice+incr)) "$(ps -o nice= -p $pid)"
	: $((incr=incr+1))
	_renice $((nice+incr)) $pid --
	atf_check_equal $((nice+incr)) "$(ps -o nice= -p $pid)"
	# with -p
	: $((incr=incr+1))
	_renice -p -- $((nice+incr)) $pid
	atf_check_equal $((nice+incr)) "$(ps -o nice= -p $pid)"
	: $((incr=incr+1))
	_renice -p $((nice+incr)) -- $pid
	atf_check_equal $((nice+incr)) "$(ps -o nice= -p $pid)"
	: $((incr=incr+1))
	_renice -p $((nice+incr)) $pid --
	atf_check_equal $((nice+incr)) "$(ps -o nice= -p $pid)"
	: $((incr=incr+1))
	_renice $((nice+incr)) -p -- $pid
	atf_check_equal $((nice+incr)) "$(ps -o nice= -p $pid)"
	: $((incr=incr+1))
	_renice $((nice+incr)) -p $pid --
	atf_check_equal $((nice+incr)) "$(ps -o nice= -p $pid)"
	kill $pid
}

atf_init_test_cases() {
	atf_add_test_case renice_abs_pid
	atf_add_test_case renice_rel_pid
	atf_add_test_case renice_abs_pgid
	atf_add_test_case renice_rel_pgid
	atf_add_test_case renice_abs_user
	atf_add_test_case renice_rel_user
	atf_add_test_case renice_delim
}
