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
	atf_set require.progs "hastctl hastd"
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
	atf_check lockf -st10 hastd.pid true
}
ctl_cleanup()
{
	if [ -s hastd.pid ]; then
		kill -9 "$(cat hastd.pid)" || true
	fi
}

hast_status()
{
	hastctl status "$@" | tail -1 | cut -w -f 2,3
}

atf_test_case pair cleanup
pair_head()
{
	atf_set descr "Test typical lifecycle or a pair "
	atf_set require.progs "hastctl hastd nc"
	atf_set require.user "root"
	atf_set timeout "60"
}
pair_body()
{
	local pair=p$$
	if nc -z 127.0.0.1 8451 || nc -z 127.0.0.1 8452; then
		atf_skip "TCP ports 8451 and 8452 must be free"
	fi
	if [ -c /dev/hast/$pair ]; then
		atf_skip "A hast device named '$pair' already exists"
	fi

	# Shared configuration
	cat >hast.conf <<EOF
resource $pair {
	on one {
		local $PWD/one/file
		remote tcp://127.0.0.1:8452
	}
	on two {
		local $PWD/two/file
		remote tcp://127.0.0.1:8451
	}
}
EOF

	# Create and start node one
	atf_check mkdir -p one
	atf_check cp hast.conf one/hast.conf
	cat >>one/hast.conf <<EOF
control $PWD/one/hastctl
pidfile $PWD/one/hastd.pid
listen tcp://127.0.0.1:8451
EOF
	atf_check dd if=/dev/zero of=one/file bs=1m count=1 status=none
	atf_check hastctl create -c one/hast.conf -n one $pair
	atf_check hastd -n one -c one/hast.conf
	hast_status -c one/hast.conf -n one $pair >one/status
	atf_check grep -qw init one/status

	# Create and start node two
	atf_check mkdir -p two
	atf_check cp hast.conf two/hast.conf
	cat >>two/hast.conf <<EOF
control $PWD/two/hastctl
pidfile $PWD/two/hastd.pid
listen tcp://127.0.0.1:8452
EOF
	atf_check dd if=/dev/zero of=two/file bs=1m count=1 status=none
	atf_check hastctl create -c two/hast.conf -n two $pair
	atf_check hastd -n two -c two/hast.conf
	hast_status -c two/hast.conf -n two $pair >two/status
	atf_check grep -qw init two/status

	# Switch node one to primary
	atf_check hastctl role -c one/hast.conf -n one primary $pair
	:>one/status
	while ! grep -q $'degraded\tprimary' one/status; do
		sleep 1
		hast_status -c one/hast.conf -n one $pair >one/status
	done

	# Switch node two to secondary
	atf_check hastctl role -c two/hast.conf -n two secondary $pair
	:>two/status
	while ! grep -q $'complete\tsecondary' two/status; do
		sleep 1
		hast_status -c two/hast.conf -n two $pair >two/status
	done

	# Write some data to the device
	atf_check test -c /dev/hast/$pair
	echo "The Magic Words are Squeamish Ossifrage" >d1
	cat d1 d1 d1 d1 >d4
	cat d4 d4 d4 d4 >d16
	# d16 is now more than 512 bytes
	atf_check dd if=d16 of=/dev/hast/$pair bs=512 count=1 status=none

	# Wait for the data to sync to both nodes
	while ! grep -aq "Squeamish Ossifrage" one/file; do
		sleep 1
	done
	while ! grep -aq "Squeamish Ossifrage" two/file; do
		sleep 1
	done

	# Now switch roles and check
	atf_check hastctl role -c one/hast.conf -n one secondary $pair
	atf_check hastctl role -c two/hast.conf -n two primary $pair
	:>two/status
	while ! grep -q $'complete\tprimary' two/status; do
		sleep 1
		hast_status -c two/hast.conf -n two $pair >two/status
	done

	# Finally, shut both nodes down
	atf_check hastctl role -c one/hast.conf -n one init $pair
	atf_check hastctl stop -c one/hast.conf -n one
	atf_check lockf -st10 one/hastd.pid true
	atf_check hastctl role -c two/hast.conf -n two init $pair
	atf_check hastctl stop -c two/hast.conf -n two
	atf_check lockf -st10 two/hastd.pid true

	# Just for kicks, compare the files; they should be identical
	# (but only because we switched both nodes to init before
	# stopping them, as the last known state of the node is stored
	# in the hastd metadata at the beginning of the file)
	atf_check cmp one/file two/file
}
pair_cleanup()
{
	if [ -s one/hastd.pid ]; then
		kill -9 "$(cat one/hastd.pid)" || true
	fi
	if [ -s two/hastd.pid ]; then
		kill -9 "$(cat two/hastd.pid)" || true
	fi
}

atf_init_test_cases()
{
	atf_add_test_case ctl
	atf_add_test_case pair
}
