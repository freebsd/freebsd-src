#
# Copyright (c) 2026 Dag-Erling Smørgrav
#
# SPDX-License-Identifier: BSD-2-Clause
#

atf_test_case ctl cleanup
ctl_head()
{
	atf_set descr "Test that hastd can start, stop, and "\
		"communicate with hastctl"
	atf_set require.user "root"
}
ctl_body()
{
	cat >hast.conf <<EOF
control $PWD/hastctl
pidfile $PWD/hastd.pid
listen $PWD/hastd.sock
EOF
	atf_check hastd -c $PWD/hast.conf
	atf_check -o ignore hastctl status -c $PWD/hast.conf
	atf_check -o ignore hastctl stop -c $PWD/hast.conf
	rm -f hastd.pid
}
ctl_cleanup()
{
	if [ -s hastd.pid ]; then
		kill -9 "$(cat hastd.pid)" || true
	fi
}

atf_init_test_cases()
{
	atf_add_test_case ctl
}
