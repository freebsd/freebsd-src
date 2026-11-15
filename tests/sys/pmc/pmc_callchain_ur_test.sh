#
# Copyright (c) 2026 Advanced Micro Devices, Inc.
#
# Author: Osvaldo Janeri Filho <ojanerif@amd.com>
#
# SPDX-License-Identifier: BSD-2-Clause
#

#
# Regression test for two KASSERTs in pmc_capture_user_callchain() that
# fired on valid samples: the TDP_CALLCHAIN check did not allow for the
# PMC_UR ring, and the pm_runcount check ran before verifying that the
# sample had not been dropped in the meantime.  Both are reachable only
# through the PMC_UR ring, which hwpmc uses for samples of
# PMC_F_USERCALLCHAIN PMCs (pmcstat -U) taken while the thread is in the
# kernel.  An INVARIANTS kernel without the fixes panics on the first such
# sample; on a kernel without INVARIANTS the test passes without proving
# anything.
#
# A panic cannot be reported from inside the test, so the test asserts that
# pmcstat -U exits cleanly and that kern.hwpmc.stats.merges, which counts
# user callchains merged from the PMC_UR ring, increased during the run.
#

# Candidate sampling events, tried in order.
UR_EVENTS="instructions unhalted-core-cycles ex_ret_instr ls_not_halted_cyc"

# Sampling rate: small enough to collect many samples in a short workload.
UR_RATE=100000

# Write a CPU-bound workload that runs for the given number of seconds.
ur_write_load()
{
	cat > ur_load.sh <<'EOF'
#!/bin/sh
end=$(($(date +%s) + ${1:-4}))
x=0
while [ $(date +%s) -lt $end ]; do
	i=0
	while [ $i -lt 100000 ]; do
		x=$((x + i * 3 + 7))
		i=$((i + 1))
	done
done
EOF
	chmod +x ur_load.sh
}

# Print the first event in UR_EVENTS that pmcstat can sample in process mode.
# The probe runs without -U so that a kernel lacking the fixes is not
# crashed before the real test starts.
ur_pick_event()
{
	local ev

	for ev in $UR_EVENTS; do
		if pmcstat -P "$ev" -n "$UR_RATE" -O ur_probe.pmc \
		    -- /usr/bin/true >/dev/null 2>&1; then
			printf '%s\n' "$ev"
			return 0
		fi
	done
	return 1
}

atf_test_case pmcstat_U_survives_and_samples
pmcstat_U_survives_and_samples_head()
{
	atf_set "descr" \
	    "pmcstat -U collects user callchains through the PMC_UR ring " \
	    "without tripping the pmc_capture_user_callchain() KASSERTs"
	atf_set "require.user" "root"
	atf_set "require.progs" "pmcstat"
	atf_set "timeout" "60"
}
pmcstat_U_survives_and_samples_body()
{
	local after before ev

	before=$(sysctl -n kern.hwpmc.stats.merges 2>/dev/null) || \
	    atf_skip "hwpmc(4) is not loaded"
	ur_write_load
	ev=$(ur_pick_event) || \
	    atf_skip "no process-mode sampling event allocatable" \
	    "(tried: $UR_EVENTS)"

	atf_check -s exit:0 -o ignore -e ignore \
	    pmcstat -U -P "$ev" -n "$UR_RATE" -O ur_capture.pmc \
	    -- ./ur_load.sh 4

	after=$(sysctl -n kern.hwpmc.stats.merges)
	[ "$after" -gt "$before" ] || \
	    atf_fail "no user callchain went through the PMC_UR ring" \
	    "(kern.hwpmc.stats.merges stayed at $before)"
}

atf_init_test_cases()
{
	atf_add_test_case pmcstat_U_survives_and_samples
}
