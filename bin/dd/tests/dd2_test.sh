#
# Copyright (c) 2017 Spectra Logic Corporation
# Copyright (c) 2023 Klara, Inc.
#
# SPDX-License-Identifier: BSD-2-Clause
#

atf_test_case max_seek
max_seek_head()
{
	atf_set "descr" "dd(1) can seek by the maximum amount"
}
max_seek_body()
{
	case $(df -T . | tail -n 1 | cut -wf 2) in
	"ufs")
		atf_skip "UFS's maximum file size is too small"
		;;
	"zfs")
		# ZFS is fine
		;;
	"tmpfs")
		atf_skip "tmpfs can't create arbitrarily large sparse files"
		;;
	*)
		atf_skip "Unknown file system"
		;;
	esac

	touch f.in
	seek=$(bc -e "2^63 / 4096 - 1")
	atf_check -s exit:0 -e ignore dd if=f.in of=f.out bs=4096 seek=$seek
}

atf_test_case seek_overflow
seek_overflow_head()
{
	atf_set "descr" "dd(1) should reject too-large seek values"
}
seek_overflow_body()
{
	touch f.in
	seek=$(bc -e "2^63 / 4096")
	atf_check -s not-exit:0 -e match:"seek offsets cannot be larger than" \
		dd if=f.in of=f.out bs=4096 seek=$seek
	atf_check -s not-exit:0 -e match:"seek offsets cannot be larger than" \
		dd if=f.in of=f.out bs=4096 seek=-1
}

atf_test_case sigint
sigint_open_head()
{
	atf_set "descr" "SIGINT while opening destination"
}
sigint_open_body()
{
	atf_check mkfifo fifo
	set -m
	dd if=fifo of=/dev/null 2>stderr &
	pid=$!
	sleep 3
	kill -INT $pid
	wait $pid
	rv=$?
	atf_check test "$rv" -gt 128
	atf_check -o inline:"INT\n" kill -l $((rv-128))
	atf_check test -s stderr
}

atf_test_case sigint
sigint_read_head()
{
	atf_set "descr" "SIGINT while reading source"
}
sigint_read_body()
{
	atf_check mkfifo fifo
	(sleep 30 >fifo &) # ensures that dd does not block on open
	set -m
	dd if=fifo of=/dev/null 2>stderr &
	pid=$!
	sleep 3
	kill -INT $pid
	wait $pid
	rv=$?
	atf_check test "$rv" -gt 128
	atf_check -o inline:"INT\n" kill -l $((rv-128))
	atf_check test -s stderr
}

atf_init_test_cases()
{
	atf_add_test_case max_seek
	atf_add_test_case seek_overflow
	atf_add_test_case sigint_open
	atf_add_test_case sigint_read
}
