#
# Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

atf_test_case basic
basic_body()
{

	if ! dmesg >/dev/null 2>&1; then
		atf_skip "No dmesg(8) access"
	fi

	# SIGABRT carefully chosen to avoid issues when run under Kyua.  No
	# matter the value of the global kern.logsigexit, these should force
	# the messages as appropriate and we'll all be happy.
	proccontrol -m logsigexit -s enable \
	    sh -c 'echo $$ > enabled.out; kill -ABRT $$'
	proccontrol -m logsigexit -s disable \
	    sh -c 'echo $$ > disabled.out; kill -ABRT $$'

	atf_check test -s enabled.out
	atf_check test -s disabled.out

	read enpid < enabled.out
	read dispid < disabled.out

	atf_check -o save:dmesg.out dmesg
	atf_check grep -Eq "$enpid.+exited on signal" dmesg.out
	atf_check -s not-exit:0 grep -Eq "$dispid.+exited on signal" dmesg.out
}

atf_init_test_cases()
{
	atf_add_test_case basic
}
