#
# Copyright (c) 2026 Advanced Micro Devices, Inc.
#
# Author: Osvaldo Janeri Filho <ojanerif@amd.com>
#
# SPDX-License-Identifier: BSD-2-Clause
#

#
# pmc(8) frontend study, which records AMD IBS fetch samples.
#

atf_test_case pmc_frontend_ibs
pmc_frontend_ibs_head()
{
	atf_set "descr" \
	    "pmc record -s frontend captures IBS fetch samples and " \
	    "pmc frontend analyzes the log"
	atf_set "require.user" "root"
	atf_set "require.progs" "pmc pmccontrol"
	atf_set "timeout" "60"
}
pmc_frontend_ibs_body()
{
	local load rc

	sysctl -n kern.hwpmc.cpuid > /dev/null 2>&1 || \
	    atf_skip "hwpmc(4) is not loaded"
	pmccontrol -l | grep -q 'IBS-FETCH' || \
	    atf_skip "hwpmc registered no IBS fetch row on this host"

	# Keep a CPU busy so that samples are taken while recording.
	sh -c 'i=0; while [ $i -lt 3000000 ]; do i=$((i + 1)); done' &
	load=$!
	# pmc record returns kevent()'s result (1) when the -t timer ends
	# the capture and 0 when it is interrupted.
	pmc record -s frontend -t 3 frontend.pmc > record.out 2>&1
	rc=$?
	kill "$load" 2>/dev/null
	wait
	[ "$rc" -le 1 ] || \
	    atf_fail "pmc record -s frontend failed: $(cat record.out)"

	# Each decoded sample row of the analysis carries miss percentages.
	atf_check -s exit:0 -o save:frontend.txt -e ignore \
	    pmc frontend frontend.pmc
	grep -q '[0-9]%' frontend.txt || \
	    atf_fail "pmc frontend decoded no IBS fetch sample"
}

atf_init_test_cases()
{
	atf_add_test_case pmc_frontend_ibs
}
