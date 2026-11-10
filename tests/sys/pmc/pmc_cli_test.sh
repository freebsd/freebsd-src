#
# Copyright (c) 2026 Advanced Micro Devices, Inc.
#
# Author: Osvaldo Janeri Filho <ojanerif@amd.com>
#
# SPDX-License-Identifier: BSD-2-Clause
#

#
# Smoke tests for the pmc(8) front end: error handling for a missing log,
# list-events, and a record/info round trip with the "instructions" alias.
# pmc(8) initializes libpmc before running any subcommand, so every case
# needs hwpmc(4).
#

# Skip unless hwpmc(4) is loaded and libpmc has a pmu-events table for
# this CPU.
pmc_cli_setup()
{
	sysctl -n kern.hwpmc.cpuid > /dev/null 2>&1 || \
	    atf_skip "hwpmc(4) is not loaded"
	[ -n "$(pmc list-events 2>/dev/null | head -1)" ] || \
	    atf_skip "libpmc has no pmu-events table for this CPU"
}

# Run pmc record, saving its output in record.out.  pmc record returns
# kevent()'s result (1) when the -t timer ends the capture and 0 when it is
# interrupted; any other status means the capture failed.
pmc_record()
{
	pmc record "$@" > record.out 2>&1
	case $? in
	0|1) return 0 ;;
	*) return 1 ;;
	esac
}

atf_test_case pmc_info_missing_file
pmc_info_missing_file_head()
{
	atf_set "descr" \
	    "pmc info on a missing file exits with EX_OSERR (71) and a " \
	    "'Cannot open' diagnostic"
	atf_set "require.progs" "pmc"
}
pmc_info_missing_file_body()
{
	pmc_cli_setup
	atf_check -s exit:71 -o ignore -e match:"Cannot open" \
	    pmc info ./nonexistent.pmc
}

atf_test_case pmc_list_events_present
pmc_list_events_present_head()
{
	atf_set "descr" \
	    "pmc list-events prints the pmu-events table for this CPU"
	atf_set "require.user" "root"
	atf_set "require.progs" "pmc"
}
pmc_list_events_present_body()
{
	pmc_cli_setup
	atf_check -s exit:0 -o save:events.txt -e ignore pmc list-events
	atf_check test "$(wc -l < events.txt)" -gt 10
}

atf_test_case pmc_record_then_info
pmc_record_then_info_head()
{
	atf_set "descr" \
	    "pmc record samples the instructions event and pmc info " \
	    "reports a non-zero sample count for it"
	atf_set "require.user" "root"
	atf_set "require.progs" "pmc"
	atf_set "timeout" "30"
}
pmc_record_then_info_body()
{
	pmc_cli_setup

	if ! pmc_record -c instructions -t 2 record.pmc; then
		grep -q "Cannot allocate event" record.out && \
		    atf_skip "no instructions event on this CPU"
		atf_fail "pmc record failed: $(cat record.out)"
	fi

	atf_check -s exit:0 -o save:info.txt -e ignore pmc info record.pmc
	# The "Counter | Counts" table follows its header line; the alias is
	# listed under the name libpmc resolved it to.
	awk '/^Counter/ { t = 1; next }
	    t && $NF ~ /^[0-9]+$/ && $NF > 0 { found = 1 }
	    END { exit(found ? 0 : 1) }' info.txt || \
	    atf_fail "pmc info shows no samples: $(tail -5 info.txt)"
}

atf_init_test_cases()
{
	atf_add_test_case pmc_info_missing_file
	atf_add_test_case pmc_list_events_present
	atf_add_test_case pmc_record_then_info
}
