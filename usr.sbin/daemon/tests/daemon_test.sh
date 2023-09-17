#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Axcient
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

atf_test_case both_pidfile cleanup
both_pidfile_head() {
	atf_set "descr" "daemon should write pid files for itself and its child"
}
both_pidfile_body() {
	daemon -P daemon.pid -p sleep.pid sleep 300
	atf_check -s exit:0 test -f daemon.pid
	atf_check -s exit:0 -o match:"daemon: sleep" ps -p `cat daemon.pid`
	atf_check -s exit:0 test -f sleep.pid
	atf_check -s exit:0 -o match:"[0-9] sleep 300$" ps -p `cat sleep.pid`
}
both_pidfile_cleanup() {
	if [ -f daemon.pid ]; then
		daemon_pid=`cat daemon.pid`
	fi
	if [ -f sleep_pid ]; then
		sleep_pid=`cat sleep.pid`
	fi
	[ -n "$sleep_pid" ] && kill $sleep_pid
	# NB: killing the sleep should kill the daemon too, so we musn't fail
	# the test if the second kill fails with ESRCH
	[ -n "$daemon_pid" ] && kill $daemon_pid || true
}

atf_test_case chdir cleanup
chdir_head() {
	atf_set "descr" "daemon should chdir to /"
}
chdir_body() {
	# Executing sleep by relative path will only work from /
	daemon -p ${PWD}/sleep.pid -c bin/sleep 300
	atf_check -s exit:0 test -f sleep.pid
	atf_check -s exit:0 -o match:"[0-9] bin/sleep 300$" \
		ps -p `cat sleep.pid`
}
chdir_cleanup() {
	[ -f sleep.pid ] && kill `cat sleep.pid`
}

atf_test_case child_pidfile cleanup
child_pidfile_head() {
	atf_set "descr" "daemon should write its child's pid to a pidfile"
}
child_pidfile_body() {
	daemon -p sleep.pid sleep 300
	atf_check -s exit:0 test -f sleep.pid
	atf_check -s exit:0 -o match:"[0-9] sleep 300$" ps -p `cat sleep.pid`
}
child_pidfile_cleanup() {
	[ -f sleep.pid ] && kill `cat sleep.pid`
}

atf_test_case child_pidfile_lock cleanup
child_pidfile_lock_head() {
	atf_set "descr" "daemon should refuse to clobber an existing child"
}
child_pidfile_lock_body() {
	daemon -p sleep.pid sleep 300
	atf_check -s exit:0 test -f sleep.pid
	atf_check -s not-exit:0 -e match:"process already running" \
		daemon -p sleep.pid sleep 300
}
child_pidfile_lock_cleanup() {
	[ -f sleep.pid ] && kill `cat sleep.pid`
}

atf_test_case newsyslog cleanup
newsyslog_head() {
	atf_set "descr" "daemon should close and reopen the output file on SIGHUP"
}
newsyslog_body() {
	cat > child.sh <<HERE
#! /bin/sh
while true ; do
	echo "my output"
	sleep 0.1
done
HERE
	chmod +x child.sh
	daemon -P daemon.pid -H -o output_file ./child.sh
	atf_check -s exit:0 test -f daemon.pid
	sleep 0.2
	mv output_file output_file.0
	kill -HUP `cat daemon.pid`
	sleep 0.2
	atf_check -s exit:0 test -s output_file.0
	atf_check -s exit:0 test -s output_file
}
newsyslog_cleanup() {
	[ -f daemon.pid ] && kill `cat daemon.pid`
}

atf_test_case output_file
output_file_head() {
	atf_set "descr" "daemon should redirect stdout to a file"
}
output_file_body() {
	daemon -o output_file seq 1 5
	seq 1 5 > expected_file
	atf_check -s exit:0 cmp output_file expected_file
}

atf_test_case restart_child cleanup
restart_child_head() {
	atf_set "descr" "daemon should restart a dead child"
}
restart_child_body() {
	daemon -rP daemon.pid -p sleep.pid sleep 300
	atf_check -s exit:0 test -f daemon.pid
	atf_check -s exit:0 test -f sleep.pid
	orig_sleep_pid=`cat sleep.pid`
	kill $orig_sleep_pid
	# Wait up to 10s for the daemon to restart the child.
	for t in `seq 0 0.1 10`; do
		new_sleep_pid=`cat sleep.pid`
		[ "$orig_sleep_pid" -ne "$new_sleep_pid" ] && break
		sleep 0.1
	done
	[ "$orig_sleep_pid" -ne "$new_sleep_pid" ] || \
		atf_fail "child was not restarted"

}
restart_child_cleanup() {
	[ -f daemon.pid ] && kill `cat daemon.pid`
}

atf_test_case supervisor_pidfile cleanup
supervisor_pidfile_head() {
	atf_set "descr" "daemon should write its own pid to a pidfile"
}
supervisor_pidfile_body() {
	daemon -P daemon.pid sleep 300
	atf_check -s exit:0 test -f daemon.pid
	atf_check -s exit:0 -o match:"daemon: sleep" ps -p `cat daemon.pid`
}
supervisor_pidfile_cleanup() {
	[ -f daemon.pid ] && kill `cat daemon.pid`
}

atf_test_case supervisor_pidfile_lock cleanup
supervisor_pidfile_lock_head() {
	atf_set "descr" "daemon should refuse to clobber an existing instance"
}
supervisor_pidfile_lock_body() {
	daemon -P daemon.pid sleep 300
	atf_check -s exit:0 test -f daemon.pid
	atf_check -s not-exit:0 -e match:"process already running" \
		daemon -p daemon.pid sleep 300
}
supervisor_pidfile_lock_cleanup() {
	[ -f daemon.pid ] && kill `cat daemon.pid`
}

atf_test_case title cleanup
title_head() {
	atf_set "descr" "daemon should change its process title"
}
title_body() {
	daemon -P daemon.pid -t "I'm a title!" sleep 300
	atf_check -s exit:0 test -f daemon.pid
	atf_check -s exit:0 -o match:"daemon: I'm a title!" \
		ps -p `cat daemon.pid`
}
title_cleanup() {
	[ -f daemon.pid ] && kill `cat daemon.pid`
}

atf_test_case user cleanup
user_head() {
	atf_set "descr" "daemon should drop privileges"
	atf_set "require.user" "root"
}
user_body() {
	daemon -p sleep.pid -u nobody sleep 300
	atf_check -s exit:0 test -f sleep.pid
	atf_check -s exit:0 -o match:"^nobody" ps -up `cat sleep.pid`
}
user_cleanup() {
	[ -f sleep.pid ] && kill `cat sleep.pid`
}


atf_init_test_cases() {
	atf_add_test_case both_pidfile
	atf_add_test_case chdir
	atf_add_test_case child_pidfile
	atf_add_test_case child_pidfile_lock
	atf_add_test_case newsyslog
	atf_add_test_case output_file
	atf_add_test_case restart_child
	atf_add_test_case supervisor_pidfile
	atf_add_test_case supervisor_pidfile_lock
	atf_add_test_case title
	atf_add_test_case user
}
