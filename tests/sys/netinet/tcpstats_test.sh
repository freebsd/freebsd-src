#!/usr/libexec/atf-sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 The FreeBSD Foundation
#
# ATF shell tests for the tcpstats kernel module lifecycle.
#


atf_test_case kmod_load_unload cleanup
kmod_load_unload_head()
{
	atf_set "descr" "Test that tcpstats kmod loads and unloads cleanly"
	atf_set "require.user" "root"
}

kmod_load_unload_body()
{
	# Skip if already loaded
	if kldstat -q -m tcpstats 2>/dev/null; then
		atf_skip "tcpstats module already loaded"
	fi

	atf_check -s exit:0 kldload tcpstats

	# Verify module is loaded
	atf_check -s exit:0 kldstat -q -m tcpstats

	# Verify device node exists
	atf_check -s exit:0 test -c /dev/tcpstats

	# Unload
	atf_check -s exit:0 kldunload tcpstats
}

kmod_load_unload_cleanup()
{
	kldunload tcpstats 2>/dev/null || true
}

atf_test_case dev_readable cleanup
dev_readable_head()
{
	atf_set "descr" "Test that /dev/tcpstats is readable after loading"
	atf_set "require.user" "root"
}

dev_readable_body()
{
	if kldstat -q -m tcpstats 2>/dev/null; then
		atf_skip "tcpstats module already loaded"
	fi

	atf_check -s exit:0 kldload tcpstats

	# Reading should succeed (may produce 0 bytes if no TCP connections)
	atf_check -s exit:0 -e ignore dd if=/dev/tcpstats of=/dev/null bs=320 count=1
}

dev_readable_cleanup()
{
	kldunload tcpstats 2>/dev/null || true
}

atf_test_case sysctl_exists cleanup
sysctl_exists_head()
{
	atf_set "descr" "Test that tcpstats sysctls are registered"
	atf_set "require.user" "root"
}

sysctl_exists_body()
{
	if kldstat -q -m tcpstats 2>/dev/null; then
		atf_skip "tcpstats module already loaded"
	fi

	atf_check -s exit:0 kldload tcpstats

	# Check that key sysctls exist
	atf_check -s exit:0 -o not-empty sysctl -n dev.tcpstats.max_open_fds
	atf_check -s exit:0 -o not-empty sysctl -n dev.tcpstats.max_concurrent_readers
	atf_check -s exit:0 -o not-empty sysctl -n dev.tcpstats.max_read_duration_ms
	atf_check -s exit:0 -o ignore sysctl -n dev.tcpstats.reads_total
	atf_check -s exit:0 -o ignore sysctl -n dev.tcpstats.active_fds
}

sysctl_exists_cleanup()
{
	kldunload tcpstats 2>/dev/null || true
}

atf_init_test_cases()
{
	atf_add_test_case kmod_load_unload
	atf_add_test_case dev_readable
	atf_add_test_case sysctl_exists
}
