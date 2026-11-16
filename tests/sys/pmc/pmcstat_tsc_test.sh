#
# Copyright (c) 2026 Advanced Micro Devices, Inc.
#
# Author: Osvaldo Janeri Filho <ojanerif@amd.com>
#
# SPDX-License-Identifier: BSD-2-Clause
#

#
# Checks the TSC information in pmc logs: the tsc_freq field of the initlog
# record, the raw TSC column printed by pmcstat -R, the unsigned "tsc"
# values emitted by pmc filter -j, and that pmcstat -G still renders a
# callgraph from a log with callchains.
#

# Capture a 1-second system-wide "instructions" sampling log into $1.
# Only a PMC that cannot be allocated is a reason to skip.
pmc_capture()
{
	if ! pmcstat -S instructions -O "$1" sleep 1 > /dev/null \
	    2> pmcstat.err; then
		grep -q "Cannot allocate system-mode pmc" pmcstat.err && \
		    atf_skip "cannot allocate a system-mode instructions PMC"
		atf_fail "pmcstat failed: $(cat pmcstat.err)"
	fi
	[ -s "$1" ] || atf_fail "pmcstat produced an empty log"
}

atf_test_case tsc_freq_initlog
tsc_freq_initlog_head()
{
	atf_set "descr" "pmcstat -R prints tsc_freq=<hz> in the initlog record"
	atf_set "require.user" "root"
	atf_set "require.progs" "pmcstat"
	atf_set "timeout" "120"
}
tsc_freq_initlog_body()
{
	local line

	pmc_capture test.pmc

	line=$(pmcstat -R test.pmc 2>/dev/null | grep '^initlog')
	[ -n "$line" ] || atf_fail "no initlog record in pmcstat -R output"
	echo "$line" | grep -qE 'tsc_freq=[1-9][0-9]*' || \
	    atf_fail "initlog record lacks tsc_freq: $line"
}

atf_test_case tsc_column_format
tsc_column_format_head()
{
	atf_set "descr" "every pmcstat -R line ends with the raw TSC value"
	atf_set "require.user" "root"
	atf_set "require.progs" "pmcstat"
	atf_set "timeout" "120"
}
tsc_column_format_body()
{
	local bad decoded first

	pmc_capture test.pmc

	decoded=$(pmcstat -R test.pmc 2>/dev/null | grep -v '^[[:space:]]*$')
	[ -n "$decoded" ] || atf_fail "pmcstat -R produced no output"

	bad=$(echo "$decoded" | grep -cvE '[0-9]{10,}[[:space:]]*$')
	if [ "$bad" -gt 0 ]; then
		first=$(echo "$decoded" | grep -vE '[0-9]{10,}[[:space:]]*$' |
		    head -1)
		atf_fail "$bad line(s) without a TSC column; first: $first"
	fi
}

atf_test_case json_tsc_unsigned
json_tsc_unsigned_head()
{
	atf_set "descr" \
	    "every record from pmc filter -j carries a decimal tsc string"
	atf_set "require.user" "root"
	atf_set "require.progs" "pmc pmcstat"
	atf_set "timeout" "120"
}
json_tsc_unsigned_body()
{
	local bad total

	pmc_capture test.pmc

	atf_check -s exit:0 -o ignore -e ignore pmc filter -j test.pmc test.json
	[ -s test.json ] || atf_fail "pmc filter -j produced no output"
	grep -oE '"tsc": "[^"]*"' test.json > tsc.txt
	total=$(wc -l < tsc.txt)
	bad=$(grep -cvE '"tsc": "[0-9]+"' tsc.txt)
	[ "$total" -gt 0 ] || atf_fail "no \"tsc\" key in JSON output"
	[ "$bad" -eq 0 ] || \
	    atf_fail "$bad of $total \"tsc\" values are not unsigned decimals"
}

atf_test_case callgraph_render
callgraph_render_head()
{
	atf_set "descr" "pmcstat -G renders a callgraph from a sampling log"
	atf_set "require.user" "root"
	atf_set "require.progs" "pmcstat"
	atf_set "timeout" "120"
}
callgraph_render_body()
{
	pmc_capture test.pmc

	atf_check -s exit:0 -o ignore -e ignore \
	    pmcstat -R test.pmc -G callgraph.txt
	[ -s callgraph.txt ] || atf_fail "pmcstat -G wrote an empty callgraph"
}

atf_init_test_cases()
{
	atf_add_test_case tsc_freq_initlog
	atf_add_test_case tsc_column_format
	atf_add_test_case json_tsc_unsigned
	atf_add_test_case callgraph_render
}
