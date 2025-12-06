#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Kristof Provost <kp@FreeBSD.org>
# Copyright (c) 2024 Kajetan Staszkiewicz <vegeta@tuxpowered.net>
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

atf_test_case "source_track" "cleanup"
source_track_head()
{
	atf_set descr 'Basic source tracking test'
	atf_set require.user root
}

source_track_body()
{
	pft_init

	epair=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair}b

	ifconfig ${epair}a 192.0.2.2/24 up
	jexec alcatraz ifconfig ${epair}b 192.0.2.1/24 up

	# Enable pf!
	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"pass in keep state (source-track)" \
		"pass out keep state (source-track)"

	ping -c 3 192.0.2.1
	atf_check -s exit:0 -o match:'192.0.2.2 -> 0.0.0.0 \( states 1,.*' \
	    jexec alcatraz pfctl -sS

	# Flush all source nodes
	jexec alcatraz pfctl -FS

	# We can't find the previous source node any more
	atf_check -s exit:0 -o not-match:'192.0.2.2 -> 0.0.0.0 \( states 1,.*' \
	    jexec alcatraz pfctl -sS

	# But we still have the state
	atf_check -s exit:0 -o match:'all icmp 192.0.2.1:8 <- 192.0.2.2:.*' \
	    jexec alcatraz pfctl -ss
}

source_track_cleanup()
{
	pft_cleanup
}

atf_test_case "kill" "cleanup"
kill_head()
{
	atf_set descr 'Test killing source nodes'
	atf_set require.user root
}

kill_body()
{
	pft_init

	epair=$(vnet_mkepair)
	vnet_mkjail alcatraz ${epair}b

	ifconfig ${epair}a 192.0.2.2/24 up
	ifconfig ${epair}a inet alias 192.0.2.3/24 up
	jexec alcatraz ifconfig ${epair}b 192.0.2.1/24 up

	# Enable pf!
	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"pass in keep state (source-track)" \
		"pass out keep state (source-track)"

	# Establish two sources
	atf_check -s exit:0 -o ignore \
	    ping -c 1 -S 192.0.2.2 192.0.2.1
	atf_check -s exit:0 -o ignore \
	    ping -c 1 -S 192.0.2.3 192.0.2.1

	# Check that both source nodes exist
	atf_check -s exit:0 -o match:'192.0.2.2 -> 0.0.0.0 \( states 1,.*' \
	    jexec alcatraz pfctl -sS
	atf_check -s exit:0 -o match:'192.0.2.3 -> 0.0.0.0 \( states 1,.*' \
	    jexec alcatraz pfctl -sS


jexec alcatraz pfctl -sS

	# Kill the 192.0.2.2 source
	jexec alcatraz pfctl -K 192.0.2.2

	# The other source still exists
	atf_check -s exit:0 -o match:'192.0.2.3 -> 0.0.0.0 \( states 1,.*' \
	    jexec alcatraz pfctl -sS

	# But not the one we killed
	atf_check -s exit:0 -o not-match:'192.0.2.2 -> 0.0.0.0 \( states 1,.*' \
	    jexec alcatraz pfctl -sS
}

kill_cleanup()
{
	pft_cleanup
}

max_src_conn_rule_head()
{
	atf_set descr 'Max connections per source per rule'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

max_src_conn_rule_body()
{
	setup_router_server_ipv6

	# Clients will connect from another network behind the router.
	# This allows for using multiple source addresses and for tester jail
	# to not respond with RST packets for SYN+ACKs.
	jexec router route add -6 2001:db8:44::0/64 2001:db8:42::2
	jexec server route add -6 2001:db8:44::0/64 2001:db8:43::1

	pft_set_rules router \
		"block" \
		"pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
		"pass in  on ${epair_tester}b inet6 proto tcp keep state (max-src-conn 3 source-track rule overload <bad_hosts>)" \
		"pass out on ${epair_server}a inet6 proto tcp keep state"

	# Limiting of connections is done for connections which have successfully
	# finished the 3-way handshake. Once the handshake is done, the state
	# is moved to CLOSED state. We use pft_ping.py to check that the handshake
	# was really successful and after that we check what is in pf state table.

	# 3 connections from host ::1 will be allowed.
	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4201 --fromaddr 2001:db8:44::1
	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4202 --fromaddr 2001:db8:44::1
	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4203 --fromaddr 2001:db8:44::1
	# The 4th connection from host ::1 will have its state killed.
	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4204 --fromaddr 2001:db8:44::1
	# A connection from host :2 is will be allowed.
	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4205 --fromaddr 2001:db8:44::2

	states=$(mktemp) || exit 1
	jexec router pfctl -qss | normalize_pfctl_s | grep 'tcp 2001:db8:43::2\[9\] <-' > $states

	grep -qE '2001:db8:44::1\[4201\] ESTABLISHED:ESTABLISHED' $states || atf_fail "State for port 4201 not found or not established"
	grep -qE '2001:db8:44::1\[4202\] ESTABLISHED:ESTABLISHED' $states || atf_fail "State for port 4202 not found or not established"
	grep -qE '2001:db8:44::1\[4203\] ESTABLISHED:ESTABLISHED' $states || atf_fail "State for port 4203 not found or not established"
	grep -qE '2001:db8:44::2\[4205\] ESTABLISHED:ESTABLISHED' $states || atf_fail "State for port 4205 not found or not established"

	if (
		grep -qE '2001:db8:44::1\[4204\] ' $states &&
		! grep -qE '2001:db8:44::1\[4204\] CLOSED:CLOSED' $states
	); then
		atf_fail "State for port 4204 found but not closed"
	fi

	jexec router pfctl -T test -t bad_hosts 2001:db8:44::1 || atf_fail "Host not found in overload table"
}

max_src_conn_rule_cleanup()
{
	pft_cleanup
}

max_src_states_rule_head()
{
	atf_set descr 'Max states per source per rule'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

max_src_states_rule_body()
{
	setup_router_server_ipv6

	# Clients will connect from another network behind the router.
	# This allows for using multiple source addresses and for tester jail
	# to not respond with RST packets for SYN+ACKs.
	jexec router route add -6 2001:db8:44::0/64 2001:db8:42::2
	jexec server route add -6 2001:db8:44::0/64 2001:db8:43::1

	pft_set_rules router \
		"block" \
		"pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
		"pass in  on ${epair_tester}b inet6 proto tcp from port 4210:4219 keep state (max-src-states 3 source-track rule) label rule_A" \
		"pass in  on ${epair_tester}b inet6 proto tcp from port 4220:4229 keep state (max-src-states 3 source-track rule) label rule_B" \
		"pass out on ${epair_server}a keep state"

	# The option max-src-states prevents even the initial SYN packet going
	# through. It's enough that we check ping_server_check_reply, no need to
	# bother checking created states.

	# 2 connections from host ::1 matching rule_A will be allowed, 1 will fail to create a state.
	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4211 --fromaddr 2001:db8:44::1
	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4212 --fromaddr 2001:db8:44::1
	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4213 --fromaddr 2001:db8:44::1
	ping_server_check_reply exit:1 --ping-type=tcp3way --send-sport=4214 --fromaddr 2001:db8:44::1

	# 2 connections from host ::1 matching rule_B will be allowed, 1 will fail to create a state.
	# Limits from rule_A don't interfere with rule_B.
	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4221 --fromaddr 2001:db8:44::1
	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4222 --fromaddr 2001:db8:44::1
	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4223 --fromaddr 2001:db8:44::1
	ping_server_check_reply exit:1 --ping-type=tcp3way --send-sport=4224 --fromaddr 2001:db8:44::1

	# 2 connections from host ::2 matching rule_B will be allowed, 1 will fail to create a state.
	# Limits for host ::1 will not interfere with host ::2.
	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4224 --fromaddr 2001:db8:44::2
	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4225 --fromaddr 2001:db8:44::2
	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4226 --fromaddr 2001:db8:44::2
	ping_server_check_reply exit:1 --ping-type=tcp3way --send-sport=4227 --fromaddr 2001:db8:44::2

	# We will check the resulting source nodes, though.
	# Order of source nodes in output is not guaranteed, find each one separately.
	nodes=$(mktemp) || exit 1
	jexec router pfctl -qvsS | normalize_pfctl_s > $nodes
	for node_regexp in \
		'2001:db8:44::1 -> :: \( states 3, connections 3, rate [0-9/\.]+s \) age [0-9:]+, 9 pkts, [0-9]+ bytes, filter rule 3, limit source-track$' \
		'2001:db8:44::1 -> :: \( states 3, connections 3, rate [0-9/\.]+s \) age [0-9:]+, 9 pkts, [0-9]+ bytes, filter rule 4, limit source-track$' \
		'2001:db8:44::2 -> :: \( states 3, connections 3, rate [0-9/\.]+s \) age [0-9:]+, 9 pkts, [0-9]+ bytes, filter rule 4, limit source-track$' \
	; do
		grep -qE "${node_regexp}" $nodes || atf_fail "Source node not found for '${node_regexp}'"
	done

	# Check if limit counters have been properly set.
	jexec router pfctl -qvvsi | grep -qE 'max-src-states\s+3\s+' || atf_fail "max-src-states not set to 3"
}

max_src_states_rule_cleanup()
{
	pft_cleanup
}

max_src_states_global_head()
{
	atf_set descr 'Max states per source global'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

max_src_states_global_body()
{
	setup_router_server_ipv6

	# Clients will connect from another network behind the router.
	# This allows for using multiple source addresses and for tester jail
	# to not respond with RST packets for SYN+ACKs.
	jexec router route add -6 2001:db8:44::0/64 2001:db8:42::2
	jexec server route add -6 2001:db8:44::0/64 2001:db8:43::1

	pft_set_rules router \
		"block" \
		"pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
		"pass in  on ${epair_tester}b inet6 proto tcp from port 4210:4219 keep state (max-src-states 3 source-track global) label rule_A" \
		"pass in  on ${epair_tester}b inet6 proto tcp from port 4220:4229 keep state (max-src-states 3 source-track global) label rule_B" \
		"pass out on ${epair_server}a keep state"

	# Global source tracking creates a single source node shared between all
	# rules for each connecting source IP address and counts states created
	# by all rules. Each rule has its own max-src-conn value checked against
	# that single source node.

	# 3 connections from host …::1 matching rule_A will be allowed.
	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4211 --fromaddr 2001:db8:44::1
	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4212 --fromaddr 2001:db8:44::1
	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4213 --fromaddr 2001:db8:44::1
	# The 4th connection matching rule_A from host …::1 will have its state killed.
	ping_server_check_reply exit:1 --ping-type=tcp3way --send-sport=4214 --fromaddr 2001:db8:44::1
	# A connection matching rule_B from host …::1 will have its state killed too.
	ping_server_check_reply exit:1 --ping-type=tcp3way --send-sport=4221 --fromaddr 2001:db8:44::1

	nodes=$(mktemp) || exit 1
	jexec router pfctl -qvsS | normalize_pfctl_s > $nodes
	cat $nodes
	node_regexp='2001:db8:44::1 -> :: \( states 3, connections 3, rate [0-9/\.]+s \) age [0-9:]+, 9 pkts, [0-9]+ bytes, limit source-track'
	grep -qE "$node_regexp" $nodes || atf_fail "Source nodes not matching expected output"
}

max_src_states_global_cleanup()
{
	pft_cleanup
}

sn_types_compat_head()
{
	atf_set descr 'Combination of source node types with compat NAT rules'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

sn_types_compat_body()
{
	setup_router_dummy_ipv6

	# Clients will connect from another network behind the router.
	# This allows for using multiple source addresses.
	jexec router route add -6 2001:db8:44::0/64 2001:db8:42::2

	# Additional gateways for route-to.
	rtgw=${net_server_host_server%::*}::2:1
	jexec router ndp -s ${rtgw} 00:01:02:03:04:05

	# This test will check for proper source node creation for:
	# max-src-states -> PF_SN_LIMIT
	# sticky-address -> PF_SN_NAT
	# route-to -> PF_SN_ROUTE
	# The test expands to all 8 combinations of those source nodes being
	# present or not.

	pft_set_rules router \
		"table <rtgws> { ${rtgw} }" \
		"table <rdrgws> { 2001:db8:45::1 }" \
		"rdr on ${epair_tester}b inet6 proto tcp from 2001:db8:44::10/124 to 2001:db8:45::1 -> <rdrgws> port 4242 sticky-address" \
		"block" \
		"pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
		"pass in  quick  on ${epair_tester}b route-to ( ${epair_server}a <rtgws>)                inet6 proto tcp from port 4211 keep state                                      label rule_3" \
		"pass in  quick  on ${epair_tester}b route-to ( ${epair_server}a <rtgws>) sticky-address inet6 proto tcp from port 4212 keep state                                      label rule_4" \
		"pass in  quick  on ${epair_tester}b route-to ( ${epair_server}a <rtgws>)                inet6 proto tcp from port 4213 keep state (max-src-states 3 source-track rule) label rule_5" \
		"pass in  quick  on ${epair_tester}b route-to ( ${epair_server}a <rtgws>) sticky-address inet6 proto tcp from port 4214 keep state (max-src-states 3 source-track rule) label rule_6" \
		"pass out quick  on ${epair_server}a keep state"

	# We don't check if state limits are properly enforced, this is tested
	# by other tests in this file.
	# Source address will not match the NAT rule
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-sport=4211 --fromaddr 2001:db8:44::01 --to 2001:db8:45::1
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-sport=4212 --fromaddr 2001:db8:44::02 --to 2001:db8:45::1
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-sport=4213 --fromaddr 2001:db8:44::03 --to 2001:db8:45::1
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-sport=4214 --fromaddr 2001:db8:44::04 --to 2001:db8:45::1
	# Source address will match the NAT rule
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-sport=4211 --fromaddr 2001:db8:44::11 --to 2001:db8:45::1
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-sport=4212 --fromaddr 2001:db8:44::12 --to 2001:db8:45::1
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-sport=4213 --fromaddr 2001:db8:44::13 --to 2001:db8:45::1
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-sport=4214 --fromaddr 2001:db8:44::14 --to 2001:db8:45::1

	states=$(mktemp) || exit 1
	jexec router pfctl -qvss | normalize_pfctl_s > $states
	nodes=$(mktemp) || exit 1
	jexec router pfctl -qvvsS | normalize_pfctl_s > $nodes

	# Order of states in output is not guaranteed, find each one separately.
	for state_regexp in \
		'all tcp 2001:db8:45::1\[9\] <- 2001:db8:44::1\[4211\] .* 1:0 pkts, 76:0 bytes, rule 3$' \
		'all tcp 2001:db8:45::1\[9\] <- 2001:db8:44::2\[4212\] .* 1:0 pkts, 76:0 bytes, rule 4, route sticky-address$' \
		'all tcp 2001:db8:45::1\[9\] <- 2001:db8:44::3\[4213\] .* 1:0 pkts, 76:0 bytes, rule 5, limit source-track$' \
		'all tcp 2001:db8:45::1\[9\] <- 2001:db8:44::4\[4214\] .* 1:0 pkts, 76:0 bytes, rule 6, limit source-track, route sticky-address$' \
		'all tcp 2001:db8:45::1\[4242\] \(2001:db8:45::1\[9\]\) <- 2001:db8:44::11\[4211\] .* 1:0 pkts, 76:0 bytes, rule 3, NAT/RDR sticky-address' \
		'all tcp 2001:db8:45::1\[4242\] \(2001:db8:45::1\[9\]\) <- 2001:db8:44::12\[4212\] .* 1:0 pkts, 76:0 bytes, rule 4, NAT/RDR sticky-address, route sticky-address' \
		'all tcp 2001:db8:45::1\[4242\] \(2001:db8:45::1\[9\]\) <- 2001:db8:44::13\[4213\] .* 1:0 pkts, 76:0 bytes, rule 5, limit source-track, NAT/RDR sticky-address' \
		'all tcp 2001:db8:45::1\[4242\] \(2001:db8:45::1\[9\]\) <- 2001:db8:44::14\[4214\] .* 1:0 pkts, 76:0 bytes, rule 6, limit source-track, NAT/RDR sticky-address, route sticky-address' \
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done

	# Order of source nodes in output is not guaranteed, find each one separately.
	for node_regexp in \
		'2001:db8:44::2 -> 2001:db8:43::2:1 \( states 1, connections 0, rate 0.0/0s \) age [0-9:]+, 1 pkts, 76 bytes, filter rule 4, route sticky-address' \
		'2001:db8:44::3 -> :: \( states 1, connections 0, rate 0.0/0s \) age [0-9:]+, 1 pkts, 76 bytes, filter rule 5, limit source-track' \
		'2001:db8:44::4 -> 2001:db8:43::2:1 \( states 1, connections 0, rate 0.0/0s ) age [0-9:]+, 1 pkts, 76 bytes, filter rule 6, route sticky-address' \
		'2001:db8:44::4 -> :: \( states 1, connections 0, rate 0.0/0s \) age [0-9:]+, 1 pkts, 76 bytes, filter rule 6, limit source-track' \
		'2001:db8:44::11 -> 2001:db8:45::1 \( states 1, connections 0, rate 0.0/0s \) age [0-9:]+, 1 pkts, 76 bytes, rdr rule 0, NAT/RDR sticky-address' \
		'2001:db8:44::12 -> 2001:db8:45::1 \( states 1, connections 0, rate 0.0/0s \) age [0-9:]+, 1 pkts, 76 bytes, rdr rule 0, NAT/RDR sticky-address' \
		'2001:db8:44::12 -> 2001:db8:43::2:1 \( states 1, connections 0, rate 0.0/0s \) age [0-9:]+, 1 pkts, 76 bytes, filter rule 4, route sticky-address' \
		'2001:db8:44::13 -> 2001:db8:45::1 \( states 1, connections 0, rate 0.0/0s \) age [0-9:]+, 1 pkts, 76 bytes, rdr rule 0, NAT/RDR sticky-address' \
		'2001:db8:44::13 -> :: \( states 1, connections 0, rate 0.0/0s \) age [0-9:]+, 1 pkts, 76 bytes, filter rule 5, limit source-track' \
		'2001:db8:44::14 -> 2001:db8:45::1 \( states 1, connections 0, rate 0.0/0s \) age [0-9:]+, 1 pkts, 76 bytes, rdr rule 0, NAT/RDR sticky-address' \
		'2001:db8:44::14 -> 2001:db8:43::2:1 \( states 1, connections 0, rate 0.0/0s ) age [0-9:]+, 1 pkts, 76 bytes, filter rule 6, route sticky-address' \
		'2001:db8:44::14 -> :: \( states 1, connections 0, rate 0.0/0s \) age [0-9:]+, 1 pkts, 76 bytes, filter rule 6, limit source-track' \
	; do
		grep -qE "${node_regexp}" $nodes || atf_fail "Source node not found for '${node_regexp}'"
	done

	! grep -q 'filter rule 3' $nodes || atf_fail "Source node found for rule 3"
}

sn_types_compat_cleanup()
{
	pft_cleanup
}

sn_types_pass_head()
{
	atf_set descr 'Combination of source node types with pass NAT rules'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

sn_types_pass_body()
{
	setup_router_dummy_ipv6

	# Clients will connect from another network behind the router.
	# This allows for using multiple source addresses.
	jexec router route add -6 2001:db8:44::0/64 2001:db8:42::2

	# Additional gateways for route-to.
	rtgw=${net_server_host_server%::*}::2:1
	jexec router ndp -s ${rtgw} 00:01:02:03:04:05

	# This test will check for proper source node creation for:
	# max-src-states -> PF_SN_LIMIT
	# sticky-address -> PF_SN_NAT
	# route-to -> PF_SN_ROUTE
	# The test expands to all 8 combinations of those source nodes being
	# present or not.

	pft_set_rules router \
		"table <rtgws> { ${rtgw} }" \
		"table <rdrgws> { 2001:db8:45::1 }" \
		"block" \
		"pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
		"match in       on ${epair_tester}b inet6 proto tcp from 2001:db8:44::10/124 to 2001:db8:45::1 rdr-to <rdrgws> port 4242 sticky-address                                label rule_3" \
		"pass  in quick on ${epair_tester}b route-to ( ${epair_server}a <rtgws>)                inet6 proto tcp from port 4211 keep state                                      label rule_4" \
		"pass  in quick on ${epair_tester}b route-to ( ${epair_server}a <rtgws>) sticky-address inet6 proto tcp from port 4212 keep state                                      label rule_5" \
		"pass  in quick on ${epair_tester}b route-to ( ${epair_server}a <rtgws>)                inet6 proto tcp from port 4213 keep state (max-src-states 3 source-track rule) label rule_6" \
		"pass  in quick on ${epair_tester}b route-to ( ${epair_server}a <rtgws>) sticky-address inet6 proto tcp from port 4214 keep state (max-src-states 3 source-track rule) label rule_7" \
		"pass out quick on ${epair_server}a keep state"

	# We don't check if state limits are properly enforced, this is tested
	# by other tests in this file.
	# Source address will not match the NAT rule
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-sport=4211 --fromaddr 2001:db8:44::01 --to 2001:db8:45::1
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-sport=4212 --fromaddr 2001:db8:44::02 --to 2001:db8:45::1
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-sport=4213 --fromaddr 2001:db8:44::03 --to 2001:db8:45::1
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-sport=4214 --fromaddr 2001:db8:44::04 --to 2001:db8:45::1
	# Source address will match the NAT rule
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-sport=4211 --fromaddr 2001:db8:44::11 --to 2001:db8:45::1
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-sport=4212 --fromaddr 2001:db8:44::12 --to 2001:db8:45::1
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-sport=4213 --fromaddr 2001:db8:44::13 --to 2001:db8:45::1
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-sport=4214 --fromaddr 2001:db8:44::14 --to 2001:db8:45::1

	states=$(mktemp) || exit 1
	jexec router pfctl -qvss | normalize_pfctl_s > $states
	nodes=$(mktemp) || exit 1
	jexec router pfctl -qvvsS | normalize_pfctl_s > $nodes

	echo " === states ==="
	cat $states
	echo " === nodes ==="
	cat $nodes
	echo " === end === "

	# Order of states in output is not guaranteed, find each one separately.
	for state_regexp in \
		'all tcp 2001:db8:45::1\[9\] <- 2001:db8:44::1\[4211\] .* 1:0 pkts, 76:0 bytes, rule 4$' \
		'all tcp 2001:db8:45::1\[9\] <- 2001:db8:44::2\[4212\] .* 1:0 pkts, 76:0 bytes, rule 5, route sticky-address$' \
		'all tcp 2001:db8:45::1\[9\] <- 2001:db8:44::3\[4213\] .* 1:0 pkts, 76:0 bytes, rule 6, limit source-track$' \
		'all tcp 2001:db8:45::1\[9\] <- 2001:db8:44::4\[4214\] .* 1:0 pkts, 76:0 bytes, rule 7, limit source-track, route sticky-address$' \
		'all tcp 2001:db8:45::1\[4242\] \(2001:db8:45::1\[9\]\) <- 2001:db8:44::11\[4211\] .* 1:0 pkts, 76:0 bytes, rule 4, NAT/RDR sticky-address' \
		'all tcp 2001:db8:45::1\[4242\] \(2001:db8:45::1\[9\]\) <- 2001:db8:44::12\[4212\] .* 1:0 pkts, 76:0 bytes, rule 5, NAT/RDR sticky-address, route sticky-address' \
		'all tcp 2001:db8:45::1\[4242\] \(2001:db8:45::1\[9\]\) <- 2001:db8:44::13\[4213\] .* 1:0 pkts, 76:0 bytes, rule 6, limit source-track, NAT/RDR sticky-address' \
		'all tcp 2001:db8:45::1\[4242\] \(2001:db8:45::1\[9\]\) <- 2001:db8:44::14\[4214\] .* 1:0 pkts, 76:0 bytes, rule 7, limit source-track, NAT/RDR sticky-address, route sticky-address' \
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done

	# Order of source nodes in output is not guaranteed, find each one separately.
	for node_regexp in \
		'2001:db8:44::2 -> 2001:db8:43::2:1 \( states 1, connections 0, rate 0.0/0s \) age [0-9:]+, 1 pkts, 76 bytes, filter rule 5, route sticky-address' \
		'2001:db8:44::3 -> :: \( states 1, connections 0, rate 0.0/0s \) age [0-9:]+, 1 pkts, 76 bytes, filter rule 6, limit source-track' \
		'2001:db8:44::4 -> 2001:db8:43::2:1 \( states 1, connections 0, rate 0.0/0s ) age [0-9:]+, 1 pkts, 76 bytes, filter rule 7, route sticky-address' \
		'2001:db8:44::4 -> :: \( states 1, connections 0, rate 0.0/0s \) age [0-9:]+, 1 pkts, 76 bytes, filter rule 7, limit source-track' \
		'2001:db8:44::11 -> 2001:db8:45::1 \( states 1, connections 0, rate 0.0/0s \) age [0-9:]+, 1 pkts, 76 bytes, filter rule 3, NAT/RDR sticky-address' \
		'2001:db8:44::12 -> 2001:db8:45::1 \( states 1, connections 0, rate 0.0/0s \) age [0-9:]+, 1 pkts, 76 bytes, filter rule 3, NAT/RDR sticky-address' \
		'2001:db8:44::12 -> 2001:db8:43::2:1 \( states 1, connections 0, rate 0.0/0s \) age [0-9:]+, 1 pkts, 76 bytes, filter rule 5, route sticky-address' \
		'2001:db8:44::13 -> 2001:db8:45::1 \( states 1, connections 0, rate 0.0/0s \) age [0-9:]+, 1 pkts, 76 bytes, filter rule 3, NAT/RDR sticky-address' \
		'2001:db8:44::13 -> :: \( states 1, connections 0, rate 0.0/0s \) age [0-9:]+, 1 pkts, 76 bytes, filter rule 6, limit source-track' \
		'2001:db8:44::14 -> 2001:db8:45::1 \( states 1, connections 0, rate 0.0/0s \) age [0-9:]+, 1 pkts, 76 bytes, filter rule 3, NAT/RDR sticky-address' \
		'2001:db8:44::14 -> 2001:db8:43::2:1 \( states 1, connections 0, rate 0.0/0s ) age [0-9:]+, 1 pkts, 76 bytes, filter rule 7, route sticky-address' \
		'2001:db8:44::14 -> :: \( states 1, connections 0, rate 0.0/0s \) age [0-9:]+, 1 pkts, 76 bytes, filter rule 7, limit source-track' \
	; do
		grep -qE "${node_regexp}" $nodes || atf_fail "Source node not found for '${node_regexp}'"
	done
}

sn_types_pass_cleanup()
{
	pft_cleanup
}

atf_test_case "mixed_af" "cleanup"
mixed_af_head()
{
       atf_set descr 'Test mixed address family source tracking'
       atf_set require.user root
       atf_set require.progs python3 scapy
}

mixed_af_body()
{
	setup_router_server_nat64

	# Clients will connect from another network behind the router.
	# This allows for using multiple source addresses.
	jexec router route add -6 ${net_clients_6}::/${net_clients_6_mask} ${net_tester_6_host_tester}

	jexec router pfctl -e
	pft_set_rules router \
		"set reassemble yes" \
		"set state-policy if-bound" \
		"block" \
		"pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
		"pass in on ${epair_tester}b \
			route-to { \
				(${epair_server1}a ${net_server1_4_host_server}) \
				(${epair_server2}a ${net_server2_6_host_server}) \
			} prefer-ipv6-nexthop sticky-address \
			inet6 proto tcp from any to 64:ff9b::/96 \
			af-to inet from ${net_clients_4}.0/${net_clients_4_mask} round-robin sticky-address"

	atf_check -s exit:0 ${common_dir}/pft_ping.py \
		--sendif ${epair_tester}a \
		--replyif ${epair_tester}a \
		--fromaddr 2001:db8:44::1 \
		--to 64:ff9b::192.0.2.100 \
		--ping-type=tcp3way \
		--send-sport=4201
	atf_check -s exit:0 ${common_dir}/pft_ping.py \
		--sendif ${epair_tester}a \
		--replyif ${epair_tester}a \
		--fromaddr 2001:db8:44::1 \
		--to 64:ff9b::192.0.2.100 \
		--ping-type=tcp3way \
		--send-sport=4202
	atf_check -s exit:0 ${common_dir}/pft_ping.py \
		--sendif ${epair_tester}a \
		--replyif ${epair_tester}a \
		--fromaddr 2001:db8:44::2 \
		--to 64:ff9b::192.0.2.100 \
		--ping-type=tcp3way \
		--send-sport=4203

	states=$(mktemp) || exit 1
	jexec router pfctl -qvvss | normalize_pfctl_s > $states
	nodes=$(mktemp) || exit 1
	jexec router pfctl -qvvsS | normalize_pfctl_s > $nodes

	# States are checked for proper route-to information.
	# The route-to gateway is IPv4.
	# FIXME: Sticky-address is broken for af-to pools!
	#        The SN is created but apparently not used, as seen in states.
	for state_regexp in \
		"${epair_server2}a tcp 203.0.113.0:4201 \(2001:db8:44::1\[4201\]\) -> 192.0.2.100:9 \(64:ff9b::c000:264\[9\]\) .* route-to: 2001:db8:4202::2@${epair_server2}a origif: ${epair_tester}b" \
		"${epair_server2}a tcp 203.0.113.1:4202 \(2001:db8:44::1\[4202\]\) -> 192.0.2.100:9 \(64:ff9b::c000:264\[9\]\) .* route-to: 2001:db8:4202::2@${epair_server2}a origif: ${epair_tester}b" \
		"${epair_server1}a tcp 203.0.113.2:4203 \(2001:db8:44::2\[4203\]\) -> 192.0.2.100:9 \(64:ff9b::c000:264\[9\]\) .* route-to: 198.51.100.18@${epair_server1}a origif: ${epair_tester}b" \
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done

	# Source nodes map IPv6 source address onto IPv4 gateway and IPv4 SNAT address.
	for node_regexp in \
		'2001:db8:44::2 -> 203.0.113.2 .* states 1, .* NAT/RDR sticky-address' \
		'2001:db8:44::2 -> 198.51.100.18 .* states 1, .* route sticky-address' \
		'2001:db8:44::1 -> 203.0.113.0 .* states 2, .* NAT/RDR sticky-address' \
		'2001:db8:44::1 -> 2001:db8:4202::2 .* states 2, .* route sticky-address' \
	; do
		grep -qE "${node_regexp}" $nodes || atf_fail "Source node not found for '${node_regexp}'"
	done
}

mixed_af_cleanup()
{
	pft_cleanup
}

atf_test_case "check_valid" "cleanup"
check_valid_head()
{
	atf_set descr 'Test if source node is invalidated on change in redirection pool'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

check_valid_body()
{
	setup_router_server_nat64

	# Clients will connect from another network behind the router.
	# This allows for using multiple source addresses.
	jexec router route add -6 ${net_clients_6}::/${net_clients_6_mask} ${net_tester_6_host_tester}

	jexec server1 ifconfig ${epair_server1}b inet6 ${net_server1_6}::42:1/128 alias
	jexec server1 ifconfig ${epair_server1}b inet6 ${net_server1_6}::42:2/128 alias

	jexec router pfctl -e
	pft_set_rules router \
		"set debug loud " \
		"set state-policy if-bound" \
		"table <targets> { ${net_server1_6}::42:1 }" \
		"pass in on ${epair_tester}b \
			route-to { (${epair_server1}a <targets>) } \
			sticky-address \
			proto tcp \
			keep state"

	atf_check -s exit:0 ${common_dir}/pft_ping.py \
		--sendif ${epair_tester}a --replyif ${epair_tester}a \
		--fromaddr ${net_clients_6}::1 --to ${host_server_6} \
		--ping-type=tcp3way --send-sport=4201

	# A source node is created using the original redirection target
	nodes=$(mktemp) || exit 1
	jexec router pfctl -qvvsS | normalize_pfctl_s > $nodes
	node_regexp='2001:db8:44::1 -> 2001:db8:4201::42:1 .* states 1,.* route sticky-address'
	grep -qE "${node_regexp}" $nodes || atf_fail "Source node not found for '${node_regexp}'"

	# Change contents of the redirection table
	echo ${net_server1_6}::42:2 | jexec router pfctl -Tr -t targets -f -

	atf_check -s exit:0 ${common_dir}/pft_ping.py \
		--sendif ${epair_tester}a --replyif ${epair_tester}a \
		--fromaddr ${net_clients_6}::1 --to ${host_server_6} \
		--ping-type=tcp3way --send-sport=4202

	# The original source node was deleted, a new one was created.
	# It has 1 states.
	jexec router pfctl -qvvsS | normalize_pfctl_s > $nodes
	node_regexp='2001:db8:44::1 -> 2001:db8:4201::42:2 .* states 1,.* route sticky-address'
	grep -qE "${node_regexp}" $nodes || atf_fail "Source node not found for '${node_regexp}'"

	atf_check -s exit:0 ${common_dir}/pft_ping.py \
		--sendif ${epair_tester}a --replyif ${epair_tester}a \
		--fromaddr ${net_clients_6}::1 --to ${host_server_6} \
		--ping-type=tcp3way --send-sport=4203

	# Without redirection table change the source node is reused.
	# It has 2 states.
	jexec router pfctl -qvvsS | normalize_pfctl_s > $nodes
	node_regexp='2001:db8:44::1 -> 2001:db8:4201::42:2 .* states 2,.* route sticky-address'
	grep -qE "${node_regexp}" $nodes || atf_fail "Source node not found for '${node_regexp}'"
}

check_valid_cleanup()
{
	pft_cleanup
}


atf_init_test_cases()
{
	atf_add_test_case "source_track"
	atf_add_test_case "kill"
	atf_add_test_case "max_src_conn_rule"
	atf_add_test_case "max_src_states_rule"
	atf_add_test_case "max_src_states_global"
	atf_add_test_case "sn_types_compat"
	atf_add_test_case "sn_types_pass"
	atf_add_test_case "mixed_af"
	atf_add_test_case "check_valid"
}
