#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 Kajetan Staszkiewicz
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

. $(atf_get_srcdir)/utils.subr

get_counters()
{
	echo " === rules ==="
	rules=$(mktemp) || exit
	(jexec router pfctl -qvvsn ; jexec router pfctl -qvvsr) | normalize_pfctl_s > $rules
	cat $rules

	echo " === tables ==="
	tables=$(mktemp) || exit 1
	jexec router pfctl -qvvsT > $tables
	cat $tables

	echo " === states ==="
	states=$(mktemp) || exit 1
	jexec router pfctl -qvvss | normalize_pfctl_s > $states
	cat $states

	echo " === nodes ==="
	nodes=$(mktemp) || exit 1
	jexec router pfctl -qvvsS | normalize_pfctl_s > $nodes
	cat $nodes
}

atf_test_case "match_pass_state" "cleanup"
match_pass_state_head()
{
	atf_set descr 'Counters on match and pass rules'
	atf_set require.user root
}

match_pass_state_body()
{
	setup_router_server_ipv6

	# Thest counters for a statefull firewall. Expose the behaviour of
	# increasing table counters if a table is used multiple times.
	# The table "tbl_in" is used both in match and pass rule. It's counters
	# are incremented twice. The tables "tbl_out_match" and "tbl_out_pass"
	# are used only once and have their countes increased only once.
	# Test source node counters for this simple scenario too.
	pft_set_rules router \
		"set state-policy if-bound" \
		"table <tbl_in>  { ${net_tester_host_tester} }" \
		"table <tbl_out_pass> { ${net_server_host_server} }" \
		"table <tbl_out_match> { ${net_server_host_server} }" \
		"block" \
		"pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
		"match in  on ${epair_tester}b inet6 proto tcp from <tbl_in>  scrub (random-id)" \
		"pass  in  on ${epair_tester}b inet6 proto tcp from <tbl_in>  keep state (max-src-states 3 source-track rule)" \
		"match out on ${epair_server}a inet6 proto tcp to   <tbl_out_match> scrub (random-id)" \
		"pass  out on ${epair_server}a inet6 proto tcp to   <tbl_out_pass> keep state"

	# Use a real TCP connection so that it will be properly closed, guaranteeing the amount of packets.
	atf_check -s exit:0 -o match:"This is a test" -x \
		"echo 'This is a test' | nc -w3 ${net_server_host_server} echo"
	# Let FINs pass through.
	sleep 1
	get_counters

	for rule_regexp in \
		"@3 match in on ${epair_tester}b .* Packets: 10 Bytes: 766 States: 1 " \
		"@4 pass in on ${epair_tester}b .* Packets: 10 Bytes: 766 States: 1 " \
		"@5 match out on ${epair_server}a .* Packets: 10 Bytes: 766 States: 1 " \
		"@6 pass out on ${epair_server}a .* Packets: 10 Bytes: 766 States: 1 " \
	; do
		grep -qE "${rule_regexp}" $rules || atf_fail "Rule regexp not found for '${rule_regexp}'"
	done

	table_counters_single="Evaluations: NoMatch: 0 Match: 1 In/Block: Packets: 0 Bytes: 0 In/Pass: Packets: 4 Bytes: 311 In/XPass: Packets: 0 Bytes: 0 Out/Block: Packets: 0 Bytes: 0 Out/Pass: Packets: 6 Bytes: 455 Out/XPass: Packets: 0 Bytes: 0"
	table_counters_double="Evaluations: NoMatch: 0 Match: 2 In/Block: Packets: 0 Bytes: 0 In/Pass: Packets: 12 Bytes: 910 In/XPass: Packets: 0 Bytes: 0 Out/Block: Packets: 0 Bytes: 0 Out/Pass: Packets: 8 Bytes: 622 Out/XPass: Packets: 0 Bytes: 0"
	for table_test in \
		"tbl_in___${table_counters_double}" \
		"tbl_out_match___${table_counters_single}" \
		"tbl_out_pass___${table_counters_single}" \
	; do
		table_name=${table_test%%___*}
		table_regexp=${table_test##*___}
		table=$(mktemp) || exit 1
		cat $tables | grep -A10 $table_name | tr '\n' ' ' | awk '{gsub("[\\[\\]]", " ", $0); gsub("[[:blank:]]+"," ",$0); print $0}' > ${table}
		grep -qE "${table_regexp}" ${table} || atf_fail "Bad counters for table ${table_name}"
	done;

	for state_regexp in \
		"${epair_tester}b tcp ${net_server_host_server}.* <- ${net_tester_host_tester}.* 6:4 pkts, 455:311 bytes, rule 4," \
		"${epair_server}a tcp ${net_server_host_tester}.* -> ${net_server_host_server}.* 6:4 pkts, 455:311 bytes, rule 6," \
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done

	for node_regexp in \
		"${net_tester_host_tester} -> :: .* 10 pkts, 766 bytes, filter rule 4, limit source-track"\
	; do
		grep -qE "${node_regexp}" $nodes || atf_fail "Source node not found for '${node_regexp}'"
	done
}

match_pass_state_cleanup()
{
	pft_cleanup
}

atf_test_case "match_pass_no_state" "cleanup"
match_pass_no_state_head()
{
	atf_set descr 'Counters on match and pass rules without keep state'
	atf_set require.user root
}

match_pass_no_state_body()
{
	setup_router_server_ipv6

	# Test counters for a stateless firewall.
	# The table "tbl_in" is used both in match and pass rule in the inbound
	# direction. The "In/Pass" counter is incremented twice. The table
	# "tbl_inout" matches the same host on inbound and outbound direction.
	# It will also be incremented twice. The tables "tbl_out_match" and
	# "tbl_out_pass" will have their counters increased only once.
	pft_set_rules router \
		"table <tbl_in>        { ${net_tester_host_tester} }" \
		"table <tbl_inout>     { ${net_tester_host_tester} }" \
		"table <tbl_out_match> { ${net_server_host_server} }" \
		"table <tbl_out_pass>  { ${net_server_host_server} }" \
		"block" \
		"pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
		"match in  on ${epair_tester}b inet6 proto tcp from <tbl_inout>" \
		"match in  on ${epair_tester}b inet6 proto tcp from <tbl_in>" \
		"pass  in  on ${epair_tester}b inet6 proto tcp from <tbl_in> no state" \
		"pass  out on ${epair_tester}b inet6 proto tcp to   <tbl_in> no state" \
		"match in  on ${epair_server}a inet6 proto tcp from <tbl_out_match>" \
		"pass  in  on ${epair_server}a inet6 proto tcp from <tbl_out_pass>  no state" \
		"match out on ${epair_server}a inet6 proto tcp from <tbl_inout> no state" \
		"pass  out on ${epair_server}a inet6 proto tcp to   <tbl_out_pass>  no state"

	# Use a real TCP connection so that it will be properly closed, guaranteeing the amount of packets.
	atf_check -s exit:0 -o match:"This is a test" -x \
		"echo 'This is a test' | nc -w3 ${net_server_host_server} echo"
	sleep 1
	get_counters

	for rule_regexp in \
		"@3 match in on ${epair_tester}b .* Packets: 6 Bytes: 455 " \
		"@4 match in on ${epair_tester}b .* Packets: 6 Bytes: 455 " \
		"@5 pass in on ${epair_tester}b .* Packets: 6 Bytes: 455 " \
		"@6 pass out on ${epair_tester}b .* Packets: 4 Bytes: 311 " \
		"@7 match in on ${epair_server}a .* Packets: 4 Bytes: 311 " \
		"@8 pass in on ${epair_server}a .* Packets: 4 Bytes: 311 " \
		"@10 pass out on ${epair_server}a .* Packets: 6 Bytes: 455 " \
	; do
		grep -qE "${rule_regexp}" $rules || atf_fail "Rule regexp not found for '${rule_regexp}'"
	done

	for table_test in \
		"tbl_in___Evaluations: NoMatch: 0 Match: 16 In/Block: Packets: 0 Bytes: 0 In/Pass: Packets: 12 Bytes: 910 In/XPass: Packets: 0 Bytes: 0 Out/Block: Packets: 0 Bytes: 0 Out/Pass: Packets: 4 Bytes: 311 Out/XPass: Packets: 0 Bytes: 0" \
		"tbl_out_match___Evaluations: NoMatch: 0 Match: 4 In/Block: Packets: 0 Bytes: 0 In/Pass: Packets: 4 Bytes: 311 In/XPass: Packets: 0 Bytes: 0 Out/Block: Packets: 0 Bytes: 0 Out/Pass: Packets: 0 Bytes: 0 Out/XPass: Packets: 0 Bytes: 0" \
		"tbl_out_pass___Evaluations: NoMatch: 0 Match: 10 In/Block: Packets: 0 Bytes: 0 In/Pass: Packets: 4 Bytes: 311 In/XPass: Packets: 0 Bytes: 0 Out/Block: Packets: 0 Bytes: 0 Out/Pass: Packets: 6 Bytes: 455 Out/XPass: Packets: 0 Bytes: 0" \
		"tbl_inout___Evaluations: NoMatch: 0 Match: 12 In/Block: Packets: 0 Bytes: 0 In/Pass: Packets: 6 Bytes: 455 In/XPass: Packets: 0 Bytes: 0 Out/Block: Packets: 0 Bytes: 0 Out/Pass: Packets: 6 Bytes: 455 Out/XPass: Packets: 0 Bytes: 0" \
	; do
		table_name=${table_test%%___*}
		table_regexp=${table_test##*___}
		table=$(mktemp) || exit 1
		cat $tables | grep -A10 $table_name | tr '\n' ' ' | awk '{gsub("[\\[\\]]", " ", $0); gsub("[[:blank:]]+"," ",$0); print $0}' > ${table}
		grep -qE "${table_regexp}" ${table} || atf_fail "Bad counters for table ${table_name}"
	done;
}

match_pass_no_state_cleanup()
{
	pft_cleanup
}

atf_test_case "match_block" "cleanup"
match_block_head()
{
	atf_set descr 'Counters on match and block rules'
	atf_set require.user root
}

match_block_body()
{
	setup_router_server_ipv6

	# Stateful firewall with a blocking rule. The rule will have its
	# counters increased because it matches and applies correctly.
	# The "match" rule before the "pass" rule will have its counters
	# increased for blocked traffic too.
	pft_set_rules router \
		"set state-policy if-bound" \
		"table <tbl_in_match> { ${net_server_host_server} }" \
		"table <tbl_in_block> { ${net_server_host_server} }" \
		"block" \
		"pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
		"match  in on ${epair_tester}b inet6 proto tcp to   <tbl_in_match> scrub (random-id)" \
		"block  in on ${epair_tester}b inet6 proto tcp to   <tbl_in_block>" \
		"pass  out on ${epair_server}a inet6 proto tcp keep state"

	# Wait 3 seconds, that will cause 2 SYNs to be sent out.
	echo 'This is a test' | nc -w3 ${net_server_host_server} echo
	sleep 1
	get_counters

	for rule_regexp in \
		"@3 match in on ${epair_tester}b .* Packets: 2 Bytes: 160 States: 0 " \
		"@4 block drop in on ${epair_tester}b .* Packets: 2 Bytes: 160 States: 0 " \
	; do
		grep -qE "${rule_regexp}" $rules || atf_fail "Rule regexp not found for '${rule_regexp}'"
	done

	# OpenBSD has (In|Out)/Match. We don't (yet) have it in FreeBSD
	# so we follow the action of the "pass" rule ("block" for this test)
	# in "match" rules.
	for table_test in \
		"tbl_in_match___Evaluations: NoMatch: 0 Match: 2 In/Block: Packets: 2 Bytes: 160 In/Pass: Packets: 0 Bytes: 0 In/XPass: Packets: 0 Bytes: 0 Out/Block: Packets: 0 Bytes: 0 Out/Pass: Packets: 0 Bytes: 0 Out/XPass: Packets: 0 Bytes: 0" \
		"tbl_in_block___Evaluations: NoMatch: 0 Match: 2 In/Block: Packets: 2 Bytes: 160 In/Pass: Packets: 0 Bytes: 0 In/XPass: Packets: 0 Bytes: 0 Out/Block: Packets: 0 Bytes: 0 Out/Pass: Packets: 0 Bytes: 0 Out/XPass: Packets: 0 Bytes: 0" \
	; do
		table_name=${table_test%%___*}
		table_regexp=${table_test##*___}
		table=$(mktemp) || exit 1
		cat $tables | grep -A10 $table_name | tr '\n' ' ' | awk '{gsub("[\\[\\]]", " ", $0); gsub("[[:blank:]]+"," ",$0); print $0}' > ${table}
		grep -qE "${table_regexp}" ${table} || atf_fail "Bad counters for table ${table_name}"
	done;
}

match_block_cleanup()
{
	pft_cleanup
}

atf_test_case "match_fail" "cleanup"
match_fail_head()
{
	atf_set descr 'Counters on match and failing pass rules'
	atf_set require.user root
}

match_fail_body()
{
	setup_router_server_ipv6

	# Statefull firewall with a failing "pass" rule.
	# When the rule can't apply it will not have its counters increased.
	pft_set_rules router \
		"set state-policy if-bound" \
		"table <tbl_in_match> { ${net_server_host_server} }" \
		"table <tbl_in_fail> { ${net_server_host_server} }" \
		"block" \
		"pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
		"match  in on ${epair_tester}b inet6 proto tcp to <tbl_in_match> scrub (random-id)" \
		"pass   in on ${epair_tester}b inet6 proto tcp to <tbl_in_fail> keep state (max 1)" \
		"pass  out on ${epair_server}a inet6 proto tcp keep state"

	# The first test will pass and increase the counters for all rules.
	echo 'This is a test' | nc -w3 ${net_server_host_server} echo
	# The second test will go through the "match" rules but fail
	# on the "pass" rule due to 'keep state (max 1)'.
	# Wait 3 seconds, that will cause 2 SYNs to be sent out.
	echo 'This is a test' | nc -w3 ${net_server_host_server} echo
	sleep 1
	get_counters

	for rule_regexp in \
		"@3 match in on ${epair_tester}b .* Packets: 10 Bytes: 766 States: 1 " \
		"@4 pass in on ${epair_tester}b .* Packets: 10 Bytes: 766 States: 1 " \
	; do
		grep -qE "${rule_regexp}" $rules || atf_fail "Rule regexp not found for '${rule_regexp}'"
	done

	$table_counters_single="Evaluations: NoMatch: 0 Match: 3 In/Block: Packets: 0 Bytes: 0 In/Pass: Packets: 6 Bytes: 455 In/XPass: Packets: 0 Bytes: 0 Out/Block: Packets: 0 Bytes: 0 Out/Pass: Packets: 4 Bytes: 311 Out/XPass: Packets: 0 Bytes: 0"
	for table_test in \
		"tbl_in_match___${table_counters_single}" \
		"tbl_in_fail___${table_counters_single}" \
	; do
		table_name=${table_test%%___*}
		table_regexp=${table_test##*___}
		table=$(mktemp) || exit 1
		cat $tables | grep -A10 $table_name | tr '\n' ' ' | awk '{gsub("[\\[\\]]", " ", $0); gsub("[[:blank:]]+"," ",$0); print $0}' > ${table}
		grep -qE "${table_regexp}" ${table} || atf_fail "Bad counters for table ${table_name}"
	done;
}

match_fail_cleanup()
{
	pft_cleanup
}

atf_test_case "nat_natonly" "cleanup"
nat_natonly_head()
{
	atf_set descr 'Counters on only a NAT rule creating state'
	atf_set require.user root
}

nat_natonly_body()
{
	setup_router_server_ipv6

	# NAT is applied on the "nat" rule.
	# The "nat" rule matches on pre-NAT addresses. There is no separate
	# "pass" rule so the "nat" rule creates the state.
	pft_set_rules router \
		"set state-policy if-bound" \
		"table <tbl_src_nat> { ${net_tester_host_tester} }" \
		"table <tbl_dst_nat> { ${net_server_host_server} }" \
		"nat on ${epair_server}a inet6 proto tcp from <tbl_src_nat> to <tbl_dst_nat> -> ${net_server_host_router}"

	# Use a real TCP connection so that it will be properly closed, guaranteeing the amount of packets.
	atf_check -s exit:0 -o match:"This is a test" -x \
		"echo 'This is a test' | nc -w3 ${net_server_host_server} echo"
	sleep 1
	get_counters

	for rule_regexp in \
		"@0 nat on ${epair_server}a .* Packets: 10 Bytes: 766 States: 1 " \
	; do
		grep -qE "${rule_regexp}" $rules || atf_fail "Rule regexp not found for '${rule_regexp}'"
	done

	# All tables have counters increased for In/Pass and Out/Pass, not XPass.
	table_counters="Evaluations: NoMatch: 0 Match: 1 In/Block: Packets: 0 Bytes: 0 In/Pass: Packets: 4 Bytes: 311 In/XPass: Packets: 0 Bytes: 0 Out/Block: Packets: 0 Bytes: 0 Out/Pass: Packets: 6 Bytes: 455 Out/XPass: Packets: 0 Bytes: 0"
	for table_test in \
		"tbl_src_nat___${table_counters}" \
		"tbl_dst_nat___${table_counters}" \
	; do
		table_name=${table_test%%___*}
		table_regexp=${table_test##*___}
		table=$(mktemp) || exit 1
		cat $tables | grep -A10 $table_name | tr '\n' ' ' | awk '{gsub("[\\[\\]]", " ", $0); gsub("[[:blank:]]+"," ",$0); print $0}' > ${table}
		grep -qE "${table_regexp}" ${table} || atf_fail "Bad counters for table ${table_name}"
	done;

	for state_regexp in \
		"all tcp ${net_server_host_router}.* -> ${net_server_host_server}.* 6:4 pkts, 455:311 bytes" \
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done
}

nat_natonly_cleanup()
{
	pft_cleanup
}

atf_test_case "nat_nat" "cleanup"
nat_nat_head()
{
	atf_set descr 'Counters on NAT, match and pass rules with keep state'
	atf_set require.user root
}

nat_nat_body()
{
	setup_router_server_ipv6

	# NAT is applied in the NAT ruleset.
	# The "nat" rule matches on pre-NAT addresses.
	# The "match" rule matches on post-NAT addresses.
	# The "pass" rule matches on post-NAT addresses and creates the state.
	pft_set_rules router \
		"set state-policy if-bound" \
		"table <tbl_src_nat> { ${net_tester_host_tester} }" \
		"table <tbl_dst_nat> { ${net_server_host_server} }" \
		"table <tbl_src_match> { ${net_server_host_router} }" \
		"table <tbl_dst_match> { ${net_server_host_server} }" \
		"table <tbl_src_pass> { ${net_server_host_router} }" \
		"table <tbl_dst_pass> { ${net_server_host_server} }" \
		"nat on ${epair_server}a inet6 proto tcp from <tbl_src_nat> to <tbl_dst_nat> -> ${net_server_host_router}" \
		"block" \
		"pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
		"pass  in  on ${epair_tester}b inet6 proto tcp keep state" \
		"match out on ${epair_server}a inet6 proto tcp from <tbl_src_match> to <tbl_dst_match> scrub (random-id)" \
		"pass  out on ${epair_server}a inet6 proto tcp from <tbl_src_pass>  to <tbl_dst_pass>  keep state"

	# Use a real TCP connection so that it will be properly closed, guaranteeing the amount of packets.
	atf_check -s exit:0 -o match:"This is a test" -x \
		"echo 'This is a test' | nc -w3 ${net_server_host_server} echo"
	sleep 1
	get_counters

	for rule_regexp in \
		"@0 nat on ${epair_server}a .* Packets: 10 Bytes: 766 States: 1 " \
		"@4 match out on ${epair_server}a .* Packets: 10 Bytes: 766 States: 1 " \
		"@5 pass out on ${epair_server}a .* Packets: 10 Bytes: 766 States: 1 " \
	; do
		grep -qE "${rule_regexp}" $rules || atf_fail "Rule regexp not found for '${rule_regexp}'"
	done

	# All tables have counters increased for In/Pass and Out/Pass, not XPass nor Block.
	table_counters="Evaluations: NoMatch: 0 Match: 1 In/Block: Packets: 0 Bytes: 0 In/Pass: Packets: 4 Bytes: 311 In/XPass: Packets: 0 Bytes: 0 Out/Block: Packets: 0 Bytes: 0 Out/Pass: Packets: 6 Bytes: 455 Out/XPass: Packets: 0 Bytes: 0"
	for table_test in \
		"tbl_src_nat___${table_counters}" \
		"tbl_dst_nat___${table_counters}" \
		"tbl_src_match___${table_counters}" \
		"tbl_dst_match___${table_counters}" \
		"tbl_src_pass___${table_counters}" \
		"tbl_dst_pass___${table_counters}" \
	; do
		table_name=${table_test%%___*}
		table_regexp=${table_test##*___}
		table=$(mktemp) || exit 1
		cat $tables | grep -A10 $table_name | tr '\n' ' ' | awk '{gsub("[\\[\\]]", " ", $0); gsub("[[:blank:]]+"," ",$0); print $0}' > ${table}
		grep -qE "${table_regexp}" ${table} || atf_fail "Bad counters for table ${table_name}"
	done;

	for state_regexp in \
		"${epair_server}a tcp ${net_server_host_router}.* -> ${net_server_host_server}.* 6:4 pkts, 455:311 bytes, rule 5," \
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done
}

nat_nat_cleanup()
{
	pft_cleanup
}

atf_test_case "nat_match" "cleanup"
nat_match_head()
{
	atf_set descr 'Counters on match with NAT and pass rules'
	atf_set require.user root
}

nat_match_body()
{
	setup_router_server_ipv6

	# NAT is applied on the "match" rule.
	# The "match" rule up to and including the NAT rule match on pre-NAT addresses.
	# The "match" rule after NAT matches on post-NAT addresses.
	# The "pass" rule matches on post-NAT addresses and creates the state.
	pft_set_rules router \
		"set state-policy if-bound" \
		"table <tbl_src_match1> { ${net_tester_host_tester} }" \
		"table <tbl_dst_match1> { ${net_server_host_server} }" \
		"table <tbl_src_match2> { ${net_tester_host_tester} }" \
		"table <tbl_dst_match2> { ${net_server_host_server} }" \
		"table <tbl_src_match3> { ${net_server_host_router} }" \
		"table <tbl_dst_match3> { ${net_server_host_server} }" \
		"table <tbl_src_pass> { ${net_server_host_router} }" \
		"table <tbl_dst_pass> { ${net_server_host_server} }" \
		"block" \
		"pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
		"pass  in  on ${epair_tester}b inet6 proto tcp keep state" \
		"match out on ${epair_server}a inet6 proto tcp from <tbl_src_match1> to <tbl_dst_match1> scrub (random-id)" \
		"match out on ${epair_server}a inet6 proto tcp from <tbl_src_match2> to <tbl_dst_match2> nat-to ${net_server_host_router}" \
		"match out on ${epair_server}a inet6 proto tcp from <tbl_src_match3> to <tbl_dst_match3> scrub (random-id)" \
		"pass  out on ${epair_server}a inet6 proto tcp from <tbl_src_pass>  to <tbl_dst_pass>  keep state"

	# Use a real TCP connection so that it will be properly closed, guaranteeing the amount of packets.
	atf_check -s exit:0 -o match:"This is a test" -x \
		"echo 'This is a test' | nc -w3 ${net_server_host_server} echo"
	sleep 1
	get_counters

	for rule_regexp in \
		"@4 match out on ${epair_server}a .* Packets: 10 Bytes: 766 States: 1 " \
		"@5 match out on ${epair_server}a .* Packets: 10 Bytes: 766 States: 1 " \
		"@6 match out on ${epair_server}a .* Packets: 10 Bytes: 766 States: 1 " \
		"@7 pass out on ${epair_server}a .* Packets: 10 Bytes: 766 States: 1 " \
	; do
		grep -qE "${rule_regexp}" $rules || atf_fail "Rule regexp not found for '${rule_regexp}'"
	done

	# All tables have counters increased for In/Pass and Out/Pass, not XPass nor Block.
	table_counters="Evaluations: NoMatch: 0 Match: 1 In/Block: Packets: 0 Bytes: 0 In/Pass: Packets: 4 Bytes: 311 In/XPass: Packets: 0 Bytes: 0 Out/Block: Packets: 0 Bytes: 0 Out/Pass: Packets: 6 Bytes: 455 Out/XPass: Packets: 0 Bytes: 0"
	for table_test in \
		"tbl_src_match1___${table_counters}" \
		"tbl_dst_match1___${table_counters}" \
		"tbl_src_match2___${table_counters}" \
		"tbl_dst_match2___${table_counters}" \
		"tbl_src_match3___${table_counters}" \
		"tbl_dst_match3___${table_counters}" \
		"tbl_src_pass___${table_counters}" \
		"tbl_dst_pass___${table_counters}" \
	; do
		table_name=${table_test%%___*}
		table_regexp=${table_test##*___}
		table=$(mktemp) || exit 1
		cat $tables | grep -A10 $table_name | tr '\n' ' ' | awk '{gsub("[\\[\\]]", " ", $0); gsub("[[:blank:]]+"," ",$0); print $0}' > ${table}
		grep -qE "${table_regexp}" ${table} || atf_fail "Bad counters for table ${table_name}"
	done;

	for state_regexp in \
		"${epair_server}a tcp ${net_server_host_tester}.* -> ${net_server_host_server}.* 6:4 pkts, 455:311 bytes, rule 7, " \
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done
}

nat_match_cleanup()
{
	pft_cleanup
}

atf_test_case "nat_pass" "cleanup"
nat_pass_head()
{
	atf_set descr 'Counters on match, and pass with NAT rules'
	atf_set require.user root
}

nat_pass_body()
{
	setup_router_server_ipv6

	# NAT is applied on the "pass" rule which also creates the state.
	# All rules match on pre-NAT addresses.
	pft_set_rules router \
		"set state-policy if-bound" \
		"table <tbl_src_match> { ${net_tester_host_tester} }" \
		"table <tbl_dst_match> { ${net_server_host_server} }" \
		"table <tbl_src_pass> { ${net_tester_host_tester} }" \
		"table <tbl_dst_pass> { ${net_server_host_server} }" \
		"block" \
		"pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
		"pass  in  on ${epair_tester}b inet6 proto tcp keep state" \
		"match out on ${epair_server}a inet6 proto tcp from <tbl_src_match> to <tbl_dst_match> scrub (random-id)" \
		"pass  out on ${epair_server}a inet6 proto tcp from <tbl_src_pass>  to <tbl_dst_pass>  nat-to ${net_server_host_router} keep state"

	# Use a real TCP connection so that it will be properly closed, guaranteeing the amount of packets.
	atf_check -s exit:0 -o match:"This is a test" -x \
		"echo 'This is a test' | nc -w3 ${net_server_host_server} echo"
	sleep 1
	get_counters

	for rule_regexp in \
		"@4 match out on ${epair_server}a .* Packets: 10 Bytes: 766 States: 1 " \
		"@5 pass out on ${epair_server}a .* Packets: 10 Bytes: 766 States: 1 " \
	; do
		grep -qE "${rule_regexp}" $rules || atf_fail "Rule regexp not found for '${rule_regexp}'"
	done

	table_counters="Evaluations: NoMatch: 0 Match: 1 In/Block: Packets: 0 Bytes: 0 In/Pass: Packets: 4 Bytes: 311 In/XPass: Packets: 0 Bytes: 0 Out/Block: Packets: 0 Bytes: 0 Out/Pass: Packets: 6 Bytes: 455 Out/XPass: Packets: 0 Bytes: 0"
	for table_test in \
		"tbl_src_match___${table_counters}" \
		"tbl_dst_match___${table_counters}" \
		"tbl_src_pass___${table_counters}" \
		"tbl_dst_pass___${table_counters}" \
	; do
		table_name=${table_test%%___*}
		table_regexp=${table_test##*___}
		table=$(mktemp) || exit 1
		cat $tables | grep -A10 $table_name | tr '\n' ' ' | awk '{gsub("[\\[\\]]", " ", $0); gsub("[[:blank:]]+"," ",$0); print $0}' > ${table}
		grep -qE "${table_regexp}" ${table} || atf_fail "Bad counters for table ${table_name}"
	done;

	for state_regexp in \
		"${epair_server}a tcp ${net_server_host_router}.* -> ${net_server_host_server}.* 6:4 pkts, 455:311 bytes, rule 5," \
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done
}

nat_pass_cleanup()
{
	pft_cleanup
}

atf_test_case "rdr_match" "cleanup"
rdr_match_head()
{
	atf_set descr 'Counters on match with RDR and pass rules'
	atf_set require.user root
}

rdr_match_body()
{
	setup_router_server_ipv6

	# Similar to the nat_match test but for the RDR action.
	# Hopefully we don't need all other tests duplicated for RDR.
	# Send traffic to a non-existing host, RDR it to the server.
	#
	# The "match" rule up to and including the RDR rule match on pre-RDR dst address.
	# The "match" rule after NAT matches on post-RDR dst address.
	# The "pass" rule matches on post-RDR dst address.
	net_server_host_notserver=${net_server_host_server%%::*}::3
	pft_set_rules router \
		"set state-policy if-bound" \
		"table <tbl_src_match1> { ${net_tester_host_tester} }" \
		"table <tbl_dst_match1> { ${net_server_host_notserver} }" \
		"table <tbl_src_match2> { ${net_tester_host_tester} }" \
		"table <tbl_dst_match2> { ${net_server_host_notserver} }" \
		"table <tbl_src_match3> { ${net_tester_host_tester} }" \
		"table <tbl_dst_match3> { ${net_server_host_server} }" \
		"table <tbl_src_pass> { ${net_tester_host_tester} }" \
		"table <tbl_dst_pass> { ${net_server_host_server} }" \
		"block" \
		"pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
		"pass  out on ${epair_server}a inet6 proto tcp keep state" \
		"match  in on ${epair_tester}b inet6 proto tcp from <tbl_src_match1> to <tbl_dst_match1> scrub (random-id)" \
		"match  in on ${epair_tester}b inet6 proto tcp from <tbl_src_match2> to <tbl_dst_match2> rdr-to ${net_server_host_server}" \
		"match  in on ${epair_tester}b inet6 proto tcp from <tbl_src_match3> to <tbl_dst_match3> scrub (random-id)" \
		"pass   in on ${epair_tester}b inet6 proto tcp from <tbl_src_pass>  to <tbl_dst_pass>  keep state"

	# Use a real TCP connection so that it will be properly closed, guaranteeing the amount of packets.
	atf_check -s exit:0 -o match:"This is a test" -x \
		"echo 'This is a test' | nc -w3 ${net_server_host_notserver} echo"
	sleep 1
	get_counters

	for rule_regexp in \
		"@4 match in on ${epair_tester}b .* Packets: 10 Bytes: 766 States: 1 " \
		"@5 match in on ${epair_tester}b .* Packets: 10 Bytes: 766 States: 1 " \
		"@6 match in on ${epair_tester}b .* Packets: 10 Bytes: 766 States: 1 " \
		"@7 pass in on ${epair_tester}b .* Packets: 10 Bytes: 766 States: 1 " \
	; do
		grep -qE "${rule_regexp}" $rules || atf_fail "Rule regexp not found for '${rule_regexp}'"
	done

	# All tables have counters increased for In/Pass and Out/Pass, not XPass nor Block.
	table_counters="Evaluations: NoMatch: 0 Match: 1 In/Block: Packets: 0 Bytes: 0 In/Pass: Packets: 6 Bytes: 455 In/XPass: Packets: 0 Bytes: 0 Out/Block: Packets: 0 Bytes: 0 Out/Pass: Packets: 4 Bytes: 311 Out/XPass: Packets: 0 Bytes: 0"
	for table_test in \
		"tbl_src_match1___${table_counters}" \
		"tbl_dst_match1___${table_counters}" \
		"tbl_src_match2___${table_counters}" \
		"tbl_dst_match2___${table_counters}" \
		"tbl_src_match3___${table_counters}" \
		"tbl_dst_match3___${table_counters}" \
		"tbl_src_pass___${table_counters}" \
		"tbl_dst_pass___${table_counters}" \
	; do
		table_name=${table_test%%___*}
		table_regexp=${table_test##*___}
		table=$(mktemp) || exit 1
		cat $tables | grep -A10 $table_name | tr '\n' ' ' | awk '{gsub("[\\[\\]]", " ", $0); gsub("[[:blank:]]+"," ",$0); print $0}' > ${table}
		grep -qE "${table_regexp}" ${table} || atf_fail "Bad counters for table ${table_name}"
	done;

	for state_regexp in \
		"${epair_tester}b tcp ${net_server_host_server}.* 6:4 pkts, 455:311 bytes, rule 7, " \
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done
}

rdr_match_cleanup()
{
	pft_cleanup
}

atf_test_case "nat64_in" "cleanup"
nat64_in_head()
{
	atf_set descr 'Counters on match and inbound af-to rules'
	atf_set require.user root
}

nat64_in_body()
{
	setup_router_server_nat64

	pft_set_rules router \
		"set state-policy if-bound" \
		"table <tbl_src_match> { ${net_tester_6_host_tester} }" \
		"table <tbl_dst_match> { 64:ff9b::${net_server1_4_host_server} }" \
		"table <tbl_src_pass>  { ${net_tester_6_host_tester} }" \
		"table <tbl_dst_pass>  { 64:ff9b::${net_server1_4_host_server} }" \
		"block log" \
		"pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
		"match  in on ${epair_tester}b inet6 proto tcp from <tbl_src_match> to <tbl_dst_match> scrub (random-id)" \
		"pass   in on ${epair_tester}b inet6 proto tcp from <tbl_src_pass>  to <tbl_dst_pass> \
			af-to inet from (${epair_server1}a) \
			keep state"

	# Use a real TCP connection so that it will be properly closed, guaranteeing the amount of packets.
	atf_check -s exit:0 -o match:"This is a test" -x \
		"echo 'This is a test' | nc -w3 64:ff9b::${net_server1_4_host_server} echo"
	sleep 1
	get_counters

	# The amount of packets is counted properly but sizes are not because
	# pd->tot_len is always post-nat, even when updating pre-nat counters.
	for rule_regexp in \
		"@3 match in on ${epair_tester}b .* Packets: 10 Bytes: 686 States: 1 " \
		"@4 pass in on ${epair_tester}b .* Packets: 10 Bytes: 686 States: 1 " \
	; do
		grep -qE "${rule_regexp}" $rules || atf_fail "Rule regexp not found for '${rule_regexp}'"
	done

	# All tables have counters increased for In/Pass and Out/Pass, not XPass nor Block.
	table_counters="Evaluations: NoMatch: 0 Match: 1 In/Block: Packets: 0 Bytes: 0 In/Pass: Packets: 4 Bytes: 231 In/XPass: Packets: 0 Bytes: 0 Out/Block: Packets: 0 Bytes: 0 Out/Pass: Packets: 6 Bytes: 455 Out/XPass: Packets: 0 Bytes: 0"
	for table_test in \
		"tbl_src_match___${table_counters}" \
		"tbl_dst_match___${table_counters}" \
		"tbl_src_pass___${table_counters}" \
		"tbl_dst_pass___${table_counters}" \
	; do
		table_name=${table_test%%___*}
		table_regexp=${table_test##*___}
		table=$(mktemp) || exit 1
		cat $tables | grep -A10 $table_name | tr '\n' ' ' | awk '{gsub("[\\[\\]]", " ", $0); gsub("[[:blank:]]+"," ",$0); print $0}' > ${table}
		grep -qE "${table_regexp}" ${table} || atf_fail "Bad counters for table ${table_name}"
	done;

	for state_regexp in \
		"${epair_server1}a tcp ${net_server_host_tester}.* 6:4 pkts, 455:231 bytes, rule 4, " \
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done

	echo " === interfaces === "
	echo " === tester === "
	jexec router pfctl -qvvsI -i ${epair_tester}b
	echo " === server === "
	jexec router pfctl -qvvsI -i ${epair_server1}a
	echo " === "
}

nat64_in_cleanup()
{
	pft_cleanup
}

atf_test_case "nat64_out" "cleanup"
nat64_out_head()
{
	atf_set descr 'Counters on match and outbound af-to rules'
	atf_set require.user root
}

nat64_out_body()
{
	setup_router_server_nat64

	# af-to in outbound path requires routes for the pre-af-to traffic.
	jexec router route add -inet6 64:ff9b::/96 -iface ${epair_server1}a

	pft_set_rules router \
		"set state-policy if-bound" \
		"table <tbl_src_match> { ${net_tester_6_host_tester} }" \
		"table <tbl_dst_match> { 64:ff9b::${net_server1_4_host_server} }" \
		"table <tbl_src_pass>  { ${net_tester_6_host_tester} }" \
		"table <tbl_dst_pass>  { 64:ff9b::${net_server1_4_host_server} }" \
		"block log " \
		"pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
		"pass  in  on ${epair_tester}b inet6 proto tcp keep state" \
		"match out on ${epair_server1}a inet6 proto tcp from <tbl_src_match> to <tbl_dst_match> scrub (random-id)" \
		"pass  out on ${epair_server1}a inet6 proto tcp from <tbl_src_pass>  to <tbl_dst_pass> \
			af-to inet from (${epair_server1}a) \
			keep state"

	# Use a real TCP connection so that it will be properly closed, guaranteeing the amount of packets.
	atf_check -s exit:0 -o match:"This is a test" -x \
		"echo 'This is a test' | nc -w3 64:ff9b::${net_server1_4_host_server} echo"
	sleep 1
	get_counters

	for rule_regexp in \
		"@4 match out on ${epair_server1}a .* Packets: 10 Bytes: 686 States: 1 " \
		"@5 pass out on ${epair_server1}a .* Packets: 10 Bytes: 686 States: 1 " \
	; do
		grep -qE "${rule_regexp}" $rules || atf_fail "Rule regexp not found for '${rule_regexp}'"
	done

	# All tables have counters increased for In/Pass and Out/Pass, not XPass nor Block.
	table_counters="Evaluations: NoMatch: 0 Match: 1 In/Block: Packets: 0 Bytes: 0 In/Pass: Packets: 4 Bytes: 231 In/XPass: Packets: 0 Bytes: 0 Out/Block: Packets: 0 Bytes: 0 Out/Pass: Packets: 6 Bytes: 455 Out/XPass: Packets: 0 Bytes: 0"
	for table_test in \
		"tbl_src_match___${table_counters}" \
		"tbl_dst_match___${table_counters}" \
		"tbl_src_pass___${table_counters}" \
		"tbl_dst_pass___${table_counters}" \
	; do
		table_name=${table_test%%___*}
		table_regexp=${table_test##*___}
		table=$(mktemp) || exit 1
		cat $tables | grep -A10 $table_name | tr '\n' ' ' | awk '{gsub("[\\[\\]]", " ", $0); gsub("[[:blank:]]+"," ",$0); print $0}' > ${table}
		grep -qE "${table_regexp}" ${table} || atf_fail "Bad counters for table ${table_name}"
	done;

	for state_regexp in \
		"${epair_server1}a tcp 198.51.100.17:[0-9]+ \(64:ff9b::c633:6412\[7\]\) -> 198.51.100.18:7 \(2001:db8:4200::2\[[0-9]+\]\) .* 6:4 pkts, 455:231 bytes, rule 5," \
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done

	echo " === interfaces === "
	echo " === tester === "
	jexec router pfctl -qvvsI -i ${epair_tester}b
	echo " === server === "
	jexec router pfctl -qvvsI -i ${epair_server1}a
	echo " === "
}

nat64_out_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "match_pass_state"
	atf_add_test_case "match_pass_no_state"
	atf_add_test_case "match_block"
	atf_add_test_case "match_fail"
	atf_add_test_case "nat_natonly"
	atf_add_test_case "nat_nat"
	atf_add_test_case "nat_match"
	atf_add_test_case "nat_pass"
	atf_add_test_case "rdr_match"
	atf_add_test_case "nat64_in"
	atf_add_test_case "nat64_out"
}
