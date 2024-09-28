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
	jexec alcatraz pfctl -s all -v
}

source_track_cleanup()
{
	pft_cleanup
}


max_src_conn_rule_head()
{
	atf_set descr 'Max connections per source per rule'
	atf_set require.user root
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
	jexec router pfctl -qss | grep 'tcp 2001:db8:43::2\[9\] <-' > $states

	grep -qE '2001:db8:44::1\[4201\]\s+ESTABLISHED:ESTABLISHED' $states || atf_fail "State for port 4201 not found or not established"
	grep -qE '2001:db8:44::1\[4202\]\s+ESTABLISHED:ESTABLISHED' $states || atf_fail "State for port 4202 not found or not established"
	grep -qE '2001:db8:44::1\[4203\]\s+ESTABLISHED:ESTABLISHED' $states || atf_fail "State for port 4203 not found or not established"
	grep -qE '2001:db8:44::2\[4205\]\s+ESTABLISHED:ESTABLISHED' $states || atf_fail "State for port 4205 not found or not established"

	if (
		grep -qE '2001:db8:44::1\[4204\]\s+' $states &&
		! grep -qE '2001:db8:44::1\[4204\]\s+CLOSED:CLOSED' $states
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
	ping_server_check_reply exit:1 --ping-type=tcp3way --send-sport=4213 --fromaddr 2001:db8:44::1

	# 2 connections from host ::1 matching rule_B will be allowed, 1 will fail to create a state.
	# Limits from rule_A don't interfere with rule_B.
	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4221 --fromaddr 2001:db8:44::1
	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4222 --fromaddr 2001:db8:44::1
	ping_server_check_reply exit:1 --ping-type=tcp3way --send-sport=4223 --fromaddr 2001:db8:44::1

	# 2 connections from host ::2 matching rule_B will be allowed, 1 will fail to create a state.
	# Limits for host ::1 will not interfere with host ::2.
	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4224 --fromaddr 2001:db8:44::2
	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4225 --fromaddr 2001:db8:44::2
	ping_server_check_reply exit:1 --ping-type=tcp3way --send-sport=4226 --fromaddr 2001:db8:44::2

	# We will check the resulting source nodes, though.
	# Order of source nodes in output is not guaranteed, find each one separately.
	nodes=$(mktemp) || exit 1
	jexec router pfctl -qvsS  > $nodes
	for node_regexp in \
		'2001:db8:44::1 -> :: \( states 2, connections 2, rate [0-9/\.]+s \)\s+age [0-9:]+, 6 pkts, [0-9]+ bytes, filter rule 3' \
		'2001:db8:44::1 -> :: \( states 2, connections 2, rate [0-9/\.]+s \)\s+age [0-9:]+, 6 pkts, [0-9]+ bytes, filter rule 4' \
		'2001:db8:44::2 -> :: \( states 2, connections 2, rate [0-9/\.]+s \)\s+age [0-9:]+, 6 pkts, [0-9]+ bytes, filter rule 4' \
	; do
		cat $nodes | tr '\n' ' ' | grep -qE "$node_regexp" || atf_fail "Source nodes not matching expected output"
	done

	# Check if limit counters have been properly set.
	jexec router pfctl -qvvsi | grep -qE 'max-src-states\s+3\s+' || atf_fail "max-src-states not set to 3"
}

max_src_states_rule_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "source_track"
	atf_add_test_case "max_src_conn_rule"
	atf_add_test_case "max_src_states_rule"
}
