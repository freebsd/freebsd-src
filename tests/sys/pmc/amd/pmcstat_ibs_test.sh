#
# Copyright (c) 2026 Advanced Micro Devices, Inc.
#
# Author: Osvaldo Janeri Filho <ojanerif@amd.com>
#
# SPDX-License-Identifier: BSD-2-Clause
#

pmcstat_ibs_setup()
{
	case "$(sysctl -n kern.hwpmc.cpuid 2>/dev/null)" in
	AuthenticAMD-*) ;;
	*) atf_skip "hwpmc(4) on an AMD CPU is required" ;;
	esac
	pmccontrol -l 2>/dev/null |
	    awk '$3 == "IBS" { found = 1 } END { exit !found }' ||
	    atf_skip "hwpmc does not expose the IBS class"
}

# System sampling: a short process-mode target may never be switched in.
pmcstat_ibs_record_replay()
{
	pmcstat -O ibs.log "$@" -- dd if=/dev/zero of=/dev/null bs=4k \
	    count=500000 > /dev/null 2> record.err ||
	    atf_fail "pmcstat failed: $(cat record.err)"
	pmcstat -R ibs.log -o ibs.txt > /dev/null 2> replay.err ||
	    atf_fail "pmcstat -R failed: $(cat replay.err)"
}

atf_test_case fetch_record_replay
fetch_record_replay_head()
{
	atf_set "descr" "pmcstat records IBS-FETCH samples and replays the log"
	atf_set "require.user" "root"
	atf_set "require.progs" "pmcstat pmccontrol"
}
fetch_record_replay_body()
{
	pmcstat_ibs_setup
	pmcstat_ibs_record_replay -S ibs-fetch
	grep -q "^ibs-fetch" ibs.txt || atf_fail "no IBS-FETCH sample replayed"
}

atf_test_case fetch_and_op_record_replay
fetch_and_op_record_replay_head()
{
	atf_set "descr" \
	    "pmcstat records IBS-FETCH and IBS-OP samples and replays both"
	atf_set "require.user" "root"
	atf_set "require.progs" "pmcstat pmccontrol"
}
fetch_and_op_record_replay_body()
{
	pmcstat_ibs_setup
	pmcstat_ibs_record_replay -S ibs-fetch -S ibs-op
	grep -q "^ibs-fetch" ibs.txt || atf_fail "no IBS-FETCH sample replayed"
	grep -q "^ibs-op" ibs.txt || atf_fail "no IBS-OP sample replayed"
}

atf_init_test_cases()
{
	atf_add_test_case fetch_record_replay
	atf_add_test_case fetch_and_op_record_replay
}
