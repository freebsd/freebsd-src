#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 Rubicon Communications, LLC (Netgate)
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

nat64_setup_base()
{
	pft_init

	epair_link=$(vnet_mkepair)
	epair=$(vnet_mkepair)

	ifconfig ${epair}a inet6 2001:db8::2/64 up no_dad
	route -6 add default 2001:db8::1

	vnet_mkjail rtr ${epair}b ${epair_link}a
	jexec rtr ifconfig ${epair}b inet6 2001:db8::1/64 up no_dad
	jexec rtr ifconfig ${epair_link}a 192.0.2.1/24 up

	vnet_mkjail dst ${epair_link}b
	jexec dst ifconfig ${epair_link}b 192.0.2.2/24 up
	jexec dst route add default 192.0.2.1

	# Sanity checks
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 2001:db8::1
	atf_check -s exit:0 -o ignore \
	    jexec dst ping -c 1 192.0.2.1

	jexec rtr pfctl -e
}

nat64_setup_in()
{
	state_policy="${1:-if-bound}"
	nat64_setup_base
	pft_set_rules rtr \
	    "set reassemble yes" \
	    "set state-policy ${state_policy}" \
	    "block" \
	    "pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
	    "pass in on ${epair}b inet6 from any to 64:ff9b::/96 af-to inet from (${epair_link}a)"
}

nat64_setup_out()
{
	state_policy="${1:-if-bound}"
	nat64_setup_base
	jexec rtr sysctl net.inet6.ip6.forwarding=1
	# AF translation happens post-routing, traffic must be directed
	# towards the outbound interface using routes for the original AF.
	# jexec rtr ifconfig ${epair_link}a inet6 2001:db8:2::1/64 up no_dad
	jexec rtr route add -inet6 64:ff9b::/96 -iface ${epair_link}a;
	pft_set_rules rtr \
	    "set reassemble yes" \
	    "set state-policy ${state_policy}" \
	    "block" \
	    "pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
	    "pass in  on ${epair}b from any to 64:ff9b::/96" \
	    "pass out on ${epair_link}a from any to 64:ff9b::/96 af-to inet from (${epair_link}a)"
}

atf_test_case "icmp_echo_in" "cleanup"
icmp_echo_in_head()
{
	atf_set descr 'Basic NAT64 ICMP echo test on inbound interface'
	atf_set require.user root
}

icmp_echo_in_body()
{
	nat64_setup_in

	# One ping
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 64:ff9b::192.0.2.2

	# Make sure packets make it even when state is established
	atf_check -s exit:0 \
	    -o match:'5 packets transmitted, 5 packets received, 0.0% packet loss' \
	    ping6 -c 5 64:ff9b::192.0.2.2
}

icmp_echo_in_cleanup()
{
	pft_cleanup
}

atf_test_case "icmp_echo_out" "cleanup"
icmp_echo_out_head()
{
	atf_set descr 'Basic NAT64 ICMP echo test on outbound interface'
	atf_set require.user root
}

icmp_echo_out_body()
{
	nat64_setup_out

	# One ping
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 64:ff9b::192.0.2.2

	# Make sure packets make it even when state is established
	atf_check -s exit:0 \
	    -o match:'5 packets transmitted, 5 packets received, 0.0% packet loss' \
	    ping6 -c 5 64:ff9b::192.0.2.2
}

icmp_echo_out_cleanup()
{
	pft_cleanup
}

atf_test_case "fragmentation_in" "cleanup"
fragmentation_in_head()
{
	atf_set descr 'Test fragmented packets on inbound interface'
	atf_set require.user root
}

fragmentation_in_body()
{
	nat64_setup_in

	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 -s 1280 64:ff9b::192.0.2.2

	atf_check -s exit:0 \
	    -o match:'3 packets transmitted, 3 packets received, 0.0% packet loss' \
	    ping6 -c 3 -s 2000 64:ff9b::192.0.2.2
	atf_check -s exit:0 \
	    -o match:'3 packets transmitted, 3 packets received, 0.0% packet loss' \
	    ping6 -c 3 -s 10000 -b 20000 64:ff9b::192.0.2.2
}

fragmentation_in_cleanup()
{
	pft_cleanup
}

atf_test_case "fragmentation_out" "cleanup"
fragmentation_out_head()
{
	atf_set descr 'Test fragmented packets on outbound interface'
	atf_set require.user root
}

fragmentation_out_body()
{
	nat64_setup_out

	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 -s 1280 64:ff9b::192.0.2.2

	atf_check -s exit:0 \
	    -o match:'3 packets transmitted, 3 packets received, 0.0% packet loss' \
	    ping6 -c 3 -s 2000 64:ff9b::192.0.2.2
	atf_check -s exit:0 \
	    -o match:'3 packets transmitted, 3 packets received, 0.0% packet loss' \
	    ping6 -c 3 -s 10000 -b 20000 64:ff9b::192.0.2.2
}

fragmentation_out_cleanup()
{
	pft_cleanup
}

atf_test_case "tcp_in_if_bound" "cleanup"
tcp_in_if_bound_head()
{
	atf_set descr 'TCP NAT64 test on inbound interface, if-bound states'
	atf_set require.user root
}

tcp_in_if_bound_body()
{
	nat64_setup_in

	echo "foo" | jexec dst nc -l 1234 &

	# Sanity check & delay for nc startup
	atf_check -s exit:0 -o ignore \
	    ping6 -c 3 64:ff9b::192.0.2.2

	rcv=$(nc -w 3 -6 64:ff9b::c000:202 1234)
	if [ "${rcv}" != "foo" ];
	then
		echo "rcv=${rcv}"
		atf_fail "Failed to connect to TCP server"
	fi

	sleep 1

	# Interfaces of the state are reversed when doing inbound NAT64!
	# FIXME: Packets from both directions are counted only on the inbound direction!
	states=$(mktemp) || exit 1
	jexec rtr pfctl -qvvss | normalize_pfctl_s > $states
	for state_regexp in \
		"${epair_link}a tcp 192.0.2.1:[0-9]+ \(2001:db8::2\[[0-9]+\]\) -> 192.0.2.2:1234 \(64:ff9b::c000:202\[1234\]\) .* 5:4 pkts.* rule 3 .* origif: ${epair}b" \
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done
	[ $(cat $states | grep tcp | wc -l) -eq 1 ] || atf_fail "Not exactly 1 state found!"
}

tcp_in_if_bound_cleanup()
{
	pft_cleanup
}

atf_test_case "tcp_out_if_bound" "cleanup"
tcp_out_if_bound_head()
{
	atf_set descr 'TCP NAT64 test on outbound interface, if-bound states'
	atf_set require.user root
}

tcp_out_if_bound_body()
{
	nat64_setup_out

	echo "foo" | jexec dst nc -l 1234 &

	# Sanity check & delay for nc startup
	atf_check -s exit:0 -o ignore \
	    ping6 -c 3 64:ff9b::192.0.2.2

	rcv=$(nc -w 3 -6 64:ff9b::c000:202 1234)
	if [ "${rcv}" != "foo" ];
	then
		echo "rcv=${rcv}"
		atf_fail "Failed to connect to TCP server"
	fi

	sleep 1

	# Origif is not printed when identical as if.
	states=$(mktemp) || exit 1
	jexec rtr pfctl -qvvss | normalize_pfctl_s > $states
	for state_regexp in \
		"${epair}b tcp 64:ff9b::c000:202\[1234\] <- 2001:db8::2\[[0-9]+\] .* 5:4 pkts.* rule 3 .*creatorid" \
		"${epair_link}a tcp 192.0.2.1:[0-9]+ \(64:ff9b::c000:202\[1234\]\) -> 192.0.2.2:1234 \(2001:db8::2\[[0-9]+\]\).* 5:4 pkts.* rule 4 .*creatorid" \
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done
	[ $(cat $states | grep tcp | wc -l) -eq 2 ] || atf_fail "Not exactly 2 states found!"
}

tcp_out_if_bound_cleanup()
{
	pft_cleanup
}

atf_test_case "tcp_in_floating" "cleanup"
tcp_in_floating_head()
{
	atf_set descr 'TCP NAT64 test on inbound interface, floating states'
	atf_set require.user root
}

tcp_in_floating_body()
{
	nat64_setup_in "floating"

	echo "foo" | jexec dst nc -l 1234 &

	# Sanity check & delay for nc startup
	atf_check -s exit:0 -o ignore \
	    ping6 -c 3 64:ff9b::192.0.2.2

	rcv=$(nc -w 3 -6 64:ff9b::c000:202 1234)
	if [ "${rcv}" != "foo" ];
	then
		echo "rcv=${rcv}"
		atf_fail "Failed to connect to TCP server"
	fi

	sleep 1

	# Interfaces of the state are reversed when doing inbound NAT64!
	# FIXME: Packets from both directions are counted only on the inbound direction!
	states=$(mktemp) || exit 1
	jexec rtr pfctl -qvvss | normalize_pfctl_s > $states
	for state_regexp in \
		"all tcp 192.0.2.1:[0-9]+ \(2001:db8::2\[[0-9]+\]\) -> 192.0.2.2:1234 \(64:ff9b::c000:202\[1234\]\).* 5:4 pkts.* rule 3 .* origif: ${epair}b" \
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done
	[ $(cat $states | grep tcp | wc -l) -eq 1 ] || atf_fail "Not exactly 1 state found!"
}

tcp_in_floating_cleanup()
{
	pft_cleanup
}

atf_test_case "tcp_out_floating" "cleanup"
tcp_out_floating_head()
{
	atf_set descr 'TCP NAT64 test on outbound interface, floating states'
	atf_set require.user root
}

tcp_out_floating_body()
{
	nat64_setup_out "floating"

	echo "foo" | jexec dst nc -l 1234 &

	# Sanity check & delay for nc startup
	atf_check -s exit:0 -o ignore \
	    ping6 -c 3 64:ff9b::192.0.2.2

	rcv=$(nc -w 3 -6 64:ff9b::c000:202 1234)
	if [ "${rcv}" != "foo" ];
	then
		echo "rcv=${rcv}"
		atf_fail "Failed to connect to TCP server"
	fi

	sleep 1

	# Origif is not printed when identical as if.
	states=$(mktemp) || exit 1
	jexec rtr pfctl -qvvss | normalize_pfctl_s > $states
	for state_regexp in \
		"all tcp 64:ff9b::c000:202\[1234\] <- 2001:db8::2\[[0-9]+\] .* 5:4 pkts,.* rule 3 .*creatorid"\
		"all tcp 192.0.2.1:[0-9]+ \(64:ff9b::c000:202\[1234\]\) -> 192.0.2.2:1234 \(2001:db8::2\[[0-9]+\]\) .* 5:4 pkts,.* rule 4 .*creatorid"\
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done
	[ $(cat $states | grep tcp | wc -l) -eq 2 ] || atf_fail "Not exactly 2 states found!"
}

tcp_out_floating_cleanup()
{
	pft_cleanup
}

atf_test_case "udp_in" "cleanup"
udp_in_head()
{
	atf_set descr 'UDP NAT64 test on inbound interface'
	atf_set require.user root
}

udp_in_body()
{
	nat64_setup_in

	echo "foo" | jexec dst nc -u -l 1234 &

	# Sanity check & delay for nc startup
	atf_check -s exit:0 -o ignore \
	    ping6 -c 3 64:ff9b::192.0.2.2

	rcv=$(echo bar | nc -w 3 -6 -u 64:ff9b::c000:202 1234)
	if [ "${rcv}" != "foo" ];
	then
		echo "rcv=${rcv}"
		atf_fail "Failed to connect to UDP server"
	fi
}

udp_in_cleanup()
{
	pft_cleanup
}

atf_test_case "udp_out" "cleanup"
udp_out_head()
{
	atf_set descr 'UDP NAT64 test on outbound interface'
	atf_set require.user root
}

udp_out_body()
{
	nat64_setup_out

	echo "foo" | jexec dst nc -u -l 1234 &

	# Sanity check & delay for nc startup
	atf_check -s exit:0 -o ignore \
	    ping6 -c 3 64:ff9b::192.0.2.2

	rcv=$(echo bar | nc -w 3 -6 -u 64:ff9b::c000:202 1234)
	if [ "${rcv}" != "foo" ];
	then
		echo "rcv=${rcv}"
		atf_fail "Failed to connect to UDP server"
	fi
}

udp_out_cleanup()
{
	pft_cleanup
}

atf_test_case "sctp_in" "cleanup"
sctp_in_head()
{
	atf_set descr 'SCTP NAT64 test on inbound interface'
	atf_set require.user root
}

sctp_in_body()
{
	nat64_setup_in
	if ! kldstat -q -m sctp; then
		atf_skip "This test requires SCTP"
	fi

	echo "foo" | jexec dst nc --sctp -N -l 1234 &

	# Sanity check & delay for nc startup
	atf_check -s exit:0 -o ignore \
	    ping6 -c 3 64:ff9b::192.0.2.2

	rcv=$(echo bar | nc --sctp -w 3 -6 64:ff9b::c000:202 1234)
	if [ "${rcv}" != "foo" ];
	then
		echo "rcv=${rcv}"
		atf_fail "Failed to connect to SCTP server"
	fi
}

sctp_in_cleanup()
{
	pft_cleanup
}

atf_test_case "sctp_out" "cleanup"
sctp_out_head()
{
	atf_set descr 'SCTP NAT64 test on outbound interface'
	atf_set require.user root
}

sctp_out_body()
{
	nat64_setup_out
	if ! kldstat -q -m sctp; then
		atf_skip "This test requires SCTP"
	fi

	echo "foo" | jexec dst nc --sctp -N -l 1234 &

	# Sanity check & delay for nc startup
	atf_check -s exit:0 -o ignore \
	    ping6 -c 3 64:ff9b::192.0.2.2

	rcv=$(echo bar | nc --sctp -w 3 -6 64:ff9b::c000:202 1234)
	if [ "${rcv}" != "foo" ];
	then
		echo "rcv=${rcv}"
		atf_fail "Failed to connect to SCTP server"
	fi
}

sctp_out_cleanup()
{
	pft_cleanup
}

atf_test_case "tos" "cleanup"
tos_head()
{
	atf_set descr 'ToS translation test'
	atf_set require.user root
}

tos_body()
{
	nat64_setup_in

	# Ensure we can distinguish ToS on the destination
	jexec dst pfctl -e
	pft_set_rules dst \
	    "pass" \
	    "block in inet tos 8"

	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 -z 4 64:ff9b::192.0.2.2
	atf_check -s exit:2 -o ignore \
	    ping6 -c 1 -z 8 64:ff9b::192.0.2.2
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 -z 16 64:ff9b::192.0.2.2

	jexec dst pfctl -sr -vv
}

tos_cleanup()
{
	pft_cleanup
}

atf_test_case "no_v4" "cleanup"
no_v4_head()
{
	atf_set descr 'Test error handling when there is no IPv4 address to translate to'
	atf_set require.user root
}

no_v4_body()
{
	pft_init

	epair_link=$(vnet_mkepair)
	epair=$(vnet_mkepair)

	ifconfig ${epair}a inet6 2001:db8::2/64 up no_dad
	route -6 add default 2001:db8::1

	vnet_mkjail rtr ${epair}b ${epair_link}a
	jexec rtr ifconfig ${epair}b inet6 2001:db8::1/64 up no_dad

	vnet_mkjail dst ${epair_link}b
	jexec dst ifconfig ${epair_link}b 192.0.2.2/24 up
	jexec dst route add default 192.0.2.1

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 2001:db8::1

	jexec rtr pfctl -e
	pft_set_rules rtr \
	    "block" \
	    "pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
	    "pass in on ${epair}b inet6 from any to 64:ff9b::/96 af-to inet from (${epair_link}a)" \

	atf_check -s exit:2 -o ignore \
	    ping6 -c 3 64:ff9b::192.0.2.2
}

no_v4_cleanup()
{
	pft_cleanup
}

atf_test_case "range" "cleanup"
range_head()
{
	atf_set descr 'Test using an address range for the IPv4 side'
	atf_set require.user root
}

range_body()
{
	pft_init

	epair_link=$(vnet_mkepair)
	epair=$(vnet_mkepair)

	ifconfig ${epair}a inet6 2001:db8::2/64 up no_dad
	route -6 add default 2001:db8::1

	vnet_mkjail rtr ${epair}b ${epair_link}a
	jexec rtr ifconfig ${epair}b inet6 2001:db8::1/64 up no_dad
	jexec rtr ifconfig ${epair_link}a 192.0.2.2/24 up
	jexec rtr ifconfig ${epair_link}a inet alias 192.0.2.3/24 up

	vnet_mkjail dst ${epair_link}b
	jexec dst ifconfig ${epair_link}b 192.0.2.254/24 up
	jexec dst route add default 192.0.2.2

	# Sanity checks
	atf_check -s exit:0 -o ignore \
	    jexec rtr ping -c 1 192.0.2.254
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 2001:db8::1
	atf_check -s exit:0 -o ignore \
	    jexec dst ping -c 1 192.0.2.2
	atf_check -s exit:0 -o ignore \
	    jexec dst ping -c 1 192.0.2.3

	jexec rtr pfctl -e
	pft_set_rules rtr \
	    "set reassemble yes" \
	    "set state-policy if-bound" \
	    "block" \
	    "pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
	    "pass in on ${epair}b inet6 from any to 64:ff9b::/96 af-to inet from 192.0.2.2/31 round-robin" \

	# Use pf to count sources
	jexec dst pfctl -e
	pft_set_rules dst \
	    "pass"

	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 64:ff9b::192.0.2.254
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 64:ff9b::192.0.2.254

	# Verify on dst that we saw different source addresses
	atf_check -s exit:0 -o match:".*192.0.2.2.*" \
	    jexec dst pfctl -ss
	atf_check -s exit:0 -o match:".*192.0.2.3.*" \
	    jexec dst pfctl -ss
}

range_cleanup()
{
	pft_cleanup
}

atf_test_case "pool" "cleanup"
pool_head()
{
	atf_set descr 'Use a pool of IPv4 addresses'
	atf_set require.user root
}

pool_body()
{
	pft_init

	epair_link=$(vnet_mkepair)
	epair=$(vnet_mkepair)

	ifconfig ${epair}a inet6 2001:db8::2/64 up no_dad
	route -6 add default 2001:db8::1

	vnet_mkjail rtr ${epair}b ${epair_link}a
	jexec rtr ifconfig ${epair}b inet6 2001:db8::1/64 up no_dad
	jexec rtr ifconfig ${epair_link}a 192.0.2.1/24 up
	jexec rtr ifconfig ${epair_link}a inet alias 192.0.2.3/24 up
	jexec rtr ifconfig ${epair_link}a inet alias 192.0.2.4/24 up

	vnet_mkjail dst ${epair_link}b
	jexec dst ifconfig ${epair_link}b 192.0.2.2/24 up
	jexec dst route add default 192.0.2.1

	# Sanity checks
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 2001:db8::1
	atf_check -s exit:0 -o ignore \
	    jexec dst ping -c 1 192.0.2.1

	jexec rtr pfctl -e
	pft_set_rules rtr \
	    "set reassemble yes" \
	    "set state-policy if-bound" \
	    "block" \
	    "pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
	    "pass in on ${epair}b inet6 from any to 64:ff9b::/96 af-to inet from { 192.0.2.1, 192.0.2.3, 192.0.2.4 } round-robin"

	# Use pf to count sources
	jexec dst pfctl -e
	pft_set_rules dst \
	    "pass"

	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 64:ff9b::192.0.2.2
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 64:ff9b::192.0.2.2
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 64:ff9b::192.0.2.2

	# Verify on dst that we saw different source addresses
	atf_check -s exit:0 -o match:".*192.0.2.1.*" \
	    jexec dst pfctl -ss
	atf_check -s exit:0 -o match:".*192.0.2.3.*" \
	    jexec dst pfctl -ss
	atf_check -s exit:0 -o match:".*192.0.2.4.*" \
	    jexec dst pfctl -ss
}

pool_cleanup()
{
	pft_cleanup
}


atf_test_case "table"
table_head()
{
	atf_set descr 'Check table restrictions'
	atf_set require.user root
}

table_body()
{
	pft_init

	# Round-robin and random are allowed
	echo "pass in on epair inet6 from any to 64:ff9b::/96 af-to inet from <wanaddr> round-robin" | \
	    atf_check -s exit:0 \
	    pfctl -f -
	echo "pass in on epair inet6 from any to 64:ff9b::/96 af-to inet from <wanaddr> random" | \
	    atf_check -s exit:0 \
	    pfctl -f -

	# bitmask is not
	echo "pass in on epair inet6 from any to 64:ff9b::/96 af-to inet from <wanaddr> bitmask" | \
	    atf_check -s exit:1 \
	    -e match:"tables are not supported by pool type" \
	    pfctl -f -
}

table_cleanup()
{
	pft_cleanup
}

atf_test_case "table_range" "cleanup"
table_range_head()
{
	atf_set descr 'Test using an address range within a table for the IPv4 side'
	atf_set require.user root
}

table_range_body()
{
	pft_init

	epair_link=$(vnet_mkepair)
	epair=$(vnet_mkepair)

	ifconfig ${epair}a inet6 2001:db8::2/64 up no_dad
	route -6 add default 2001:db8::1

	vnet_mkjail rtr ${epair}b ${epair_link}a
	jexec rtr ifconfig ${epair}b inet6 2001:db8::1/64 up no_dad
	jexec rtr ifconfig ${epair_link}a 192.0.2.2/24 up
	jexec rtr ifconfig ${epair_link}a inet alias 192.0.2.3/24 up

	vnet_mkjail dst ${epair_link}b
	jexec dst ifconfig ${epair_link}b 192.0.2.254/24 up
	jexec dst route add default 192.0.2.2

	# Sanity checks
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 2001:db8::1
	atf_check -s exit:0 -o ignore \
	    jexec dst ping -c 1 192.0.2.2

	jexec rtr pfctl -e
	pft_set_rules rtr \
	    "set reassemble yes" \
	    "set state-policy if-bound" \
	    "table <wanaddrs> { 192.0.2.2/31 }" \
	    "block" \
	    "pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
	    "pass in on ${epair}b inet6 from any to 64:ff9b::/96 af-to inet from <wanaddrs> round-robin"

	# Use pf to count sources
	jexec dst pfctl -e
	pft_set_rules dst \
	    "pass"

	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 64:ff9b::192.0.2.254
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 64:ff9b::192.0.2.254

	# Verify on dst that we saw different source addresses
	atf_check -s exit:0 -o match:".*192.0.2.2.*" \
	    jexec dst pfctl -ss
	atf_check -s exit:0 -o match:".*192.0.2.3.*" \
	    jexec dst pfctl -ss
}

table_range_cleanup()
{
	pft_cleanup
}

table_common_body()
{
	pool_type=$1

	pft_init

	epair_link=$(vnet_mkepair)
	epair=$(vnet_mkepair)

	ifconfig ${epair}a inet6 2001:db8::2/64 up no_dad
	route -6 add default 2001:db8::1

	vnet_mkjail rtr ${epair}b ${epair_link}a
	jexec rtr ifconfig ${epair}b inet6 2001:db8::1/64 up no_dad
	jexec rtr ifconfig ${epair_link}a 192.0.2.1/24 up
	jexec rtr ifconfig ${epair_link}a inet alias 192.0.2.3/24 up
	jexec rtr ifconfig ${epair_link}a inet alias 192.0.2.4/24 up

	vnet_mkjail dst ${epair_link}b
	jexec dst ifconfig ${epair_link}b 192.0.2.2/24 up
	jexec dst route add default 192.0.2.1

	# Sanity checks
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 2001:db8::1
	atf_check -s exit:0 -o ignore \
	    jexec dst ping -c 1 192.0.2.1

	jexec rtr pfctl -e
	pft_set_rules rtr \
	    "set reassemble yes" \
	    "set state-policy if-bound" \
	    "table <wanaddrs> { 192.0.2.1, 192.0.2.3, 192.0.2.4 }" \
	    "block" \
	    "pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
	    "pass in on ${epair}b inet6 from any to 64:ff9b::/96 af-to inet from <wanaddrs> ${pool_type}"

	# Use pf to count sources
	jexec dst pfctl -e
	pft_set_rules dst \
	    "pass"

	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 64:ff9b::192.0.2.2
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 64:ff9b::192.0.2.2
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 64:ff9b::192.0.2.2

	# XXX We can't reasonably check pool type random because it's random. It may end
	# up choosing the same source IP for all three connections.
	if [ "${pool_type}" == "round-robin" ];
	then
		# Verify on dst that we saw different source addresses
		atf_check -s exit:0 -o match:".*192.0.2.1.*" \
		    jexec dst pfctl -ss
		atf_check -s exit:0 -o match:".*192.0.2.3.*" \
		    jexec dst pfctl -ss
		atf_check -s exit:0 -o match:".*192.0.2.4.*" \
		    jexec dst pfctl -ss
	fi
}

atf_test_case "table_round_robin" "cleanup"
table_round_robin_head()
{
	atf_set descr 'Use a table of IPv4 addresses in round-robin mode'
	atf_set require.user root
}

table_round_robin_body()
{
	table_common_body round-robin
}

table_round_robin_cleanup()
{
	pft_cleanup
}

atf_test_case "table_random" "cleanup"
table_random_head()
{
	atf_set descr 'Use a table of IPv4 addresses in random mode'
	atf_set require.user root
}

table_random_body()
{
	table_common_body random
}

table_random_cleanup()
{
	pft_cleanup
}

atf_test_case "dummynet" "cleanup"
dummynet_head()
{
	atf_set descr 'Test dummynet on af-to rules'
	atf_set require.user root
}

dummynet_body()
{
	pft_init
	dummynet_init

	epair_link=$(vnet_mkepair)
	epair=$(vnet_mkepair)

	ifconfig ${epair}a inet6 2001:db8::2/64 up no_dad
	route -6 add default 2001:db8::1

	vnet_mkjail rtr ${epair}b ${epair_link}a
	jexec rtr ifconfig ${epair}b inet6 2001:db8::1/64 up no_dad
	jexec rtr ifconfig ${epair_link}a 192.0.2.1/24 up

	vnet_mkjail dst ${epair_link}b
	jexec dst ifconfig ${epair_link}b 192.0.2.2/24 up
	jexec dst route add default 192.0.2.1

	# Sanity checks
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 2001:db8::1
	atf_check -s exit:0 -o ignore \
	    jexec dst ping -c 1 192.0.2.1

	jexec rtr pfctl -e
	jexec rtr dnctl pipe 1 config delay 600
	pft_set_rules rtr \
	    "set reassemble yes" \
	    "set state-policy if-bound" \
	    "block" \
	    "pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
	    "pass in on ${epair}b inet6 from any to 64:ff9b::/96 dnpipe 1 af-to inet from (${epair_link}a)"

	# The ping request will pass, but take 1.2 seconds (.6 in, .6 out)
	# So this works:
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 -t 2 64:ff9b::192.0.2.2

	# But this times out:
	atf_check -s exit:2 -o ignore \
	    ping6 -c 1 -t 1 64:ff9b::192.0.2.2
}

dummynet_cleanup()
{
	pft_cleanup
}

atf_test_case "gateway6" "cleanup"
gateway6_head()
{
	atf_set descr 'NAT64 with a routing hop on the v6 side'
	atf_set require.user root
}

gateway6_body()
{
	pft_init

	epair_lan_link=$(vnet_mkepair)
	epair_link=$(vnet_mkepair)
	epair=$(vnet_mkepair)

	ifconfig ${epair}a inet6 2001:db8:1::2/64 up no_dad
	route -6 add default 2001:db8:1::1

	vnet_mkjail lan_rtr ${epair}b ${epair_lan_link}a
	jexec lan_rtr ifconfig ${epair}b inet6 2001:db8:1::1/64 up no_dad
	jexec lan_rtr ifconfig ${epair_lan_link}a inet6 2001:db8::2/64 up no_dad
	jexec lan_rtr route -6 add default 2001:db8::1
	jexec lan_rtr sysctl net.inet6.ip6.forwarding=1

	vnet_mkjail rtr ${epair_lan_link}b ${epair_link}a
	jexec rtr ifconfig ${epair_lan_link}b inet6 2001:db8::1/64 up no_dad
	jexec rtr ifconfig ${epair_link}a 192.0.2.1/24 up
	jexec rtr route -6 add default 2001:db8::2

	vnet_mkjail dst ${epair_link}b
	jexec dst ifconfig ${epair_link}b 192.0.2.2/24 up
	jexec dst route add default 192.0.2.1

	# Sanity checks
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 2001:db8:1::1
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 2001:db8::1
	atf_check -s exit:0 -o ignore \
	    jexec dst ping -c 1 192.0.2.1

	jexec rtr pfctl -e
	pft_set_rules rtr \
	    "set reassemble yes" \
	    "set state-policy if-bound" \
	    "block" \
	    "pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
	    "pass in on ${epair_lan_link}b inet6 from any to 64:ff9b::/96 af-to inet from (${epair_link}a)"

	# One ping
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 64:ff9b::192.0.2.2

	# Make sure packets make it even when state is established
	atf_check -s exit:0 \
	    -o match:'5 packets transmitted, 5 packets received, 0.0% packet loss' \
	    ping6 -c 5 64:ff9b::192.0.2.2
}

gateway6_cleanup()
{
	pft_cleanup
}

atf_test_case "route_to" "cleanup"
route_to_head()
{
	atf_set descr 'Test route-to on af-to rules'
	atf_set require.user root
}

route_to_body()
{
	pft_init

	epair_link=$(vnet_mkepair)
	epair=$(vnet_mkepair)

	ifconfig ${epair}a inet6 2001:db8::2/64 up no_dad
	route -6 add default 2001:db8::1

	vnet_mkjail rtr ${epair}b ${epair_link}a
	jexec rtr ifconfig ${epair}b inet6 2001:db8::1/64 up no_dad
	jexec rtr ifconfig ${epair_link}a 192.0.2.1/24 up

	vnet_mkjail dst ${epair_link}b
	jexec dst ifconfig ${epair_link}b 192.0.2.2/24 up
	jexec dst ifconfig lo0 203.0.113.1/32 alias
	jexec dst route add default 192.0.2.2

	# Sanity checks
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 2001:db8::1

	jexec rtr pfctl -e
	pft_set_rules rtr \
	    "set reassemble yes" \
	    "set debug loud" \
	    "set state-policy if-bound" \
	    "block log (all)" \
	    "pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
	    "pass in on ${epair}b route-to (${epair_link}a 192.0.2.2) inet6 from any to 64:ff9b::/96 af-to inet from (${epair_link}a)"

	atf_check -s exit:0 -o ignore \
	    -o match:'3 packets transmitted, 3 packets received, 0.0% packet loss' \
	    ping6 -c 3 64:ff9b::192.0.2.2

	states=$(mktemp) || exit 1
	jexec rtr pfctl -qvvss | normalize_pfctl_s > $states
	cat $states

	# Interfaces of the state are reversed when doing inbound NAT64!
	for state_regexp in \
		"${epair_link}a ipv6-icmp 192.0.2.1:.* \(2001:db8::2\[[0-9]+\]\) -> 192.0.2.2:8 \(64:ff9b::c000:202\[[0-9]+\]\).* 3:3 pkts.*route-to: 192.0.2.2@${epair_link}a origif: ${epair}b" \
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done
}

route_to_cleanup()
{
	pft_cleanup
}

atf_test_case "reply_to" "cleanup"
reply_to_head()
{
	atf_set descr 'Test reply-to on af-to rules'
	atf_set require.user root
}

reply_to_body()
{
	pft_init

	epair_link=$(vnet_mkepair)
	epair=$(vnet_mkepair)

	ifconfig ${epair}a inet6 2001:db8::2/64 up no_dad
	route -6 add default 2001:db8::1

	vnet_mkjail rtr ${epair}b ${epair_link}a
	jexec rtr ifconfig ${epair}b inet6 2001:db8::1/64 up no_dad
	jexec rtr ifconfig ${epair_link}a 192.0.2.1/24 up

	vnet_mkjail dst ${epair_link}b
	jexec dst ifconfig ${epair_link}b 192.0.2.2/24 up
	jexec dst route add default 192.0.2.1

	# Sanity checks
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 2001:db8::1

	jexec rtr pfctl -e
	pft_set_rules rtr \
	    "set reassemble yes" \
	    "set state-policy if-bound" \
	    "block" \
	    "pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
	    "pass in on ${epair}b reply-to (${epair}b 2001:db8::2) inet6 from any to 64:ff9b::/96 af-to inet from 192.0.2.1"

	atf_check -s exit:0 -o ignore \
	    -o match:'3 packets transmitted, 3 packets received, 0.0% packet loss' \
	    ping6 -c 3 64:ff9b::192.0.2.2
}

reply_to_cleanup()
{
	pft_cleanup
}

atf_test_case "v6_gateway" "cleanup"
v6_gateway_head()
{
	atf_set descr 'nat64 when the IPv4 gateway is given by an IPv6 address'
	atf_set require.user root
}

v6_gateway_body()
{
	pft_init

	epair_wan_two=$(vnet_mkepair)
	epair_wan_one=$(vnet_mkepair)
	epair_lan=$(vnet_mkepair)

	ifconfig ${epair_lan}a inet6 2001:db8::2/64 up no_dad
	route -6 add default 2001:db8::1

	vnet_mkjail rtr ${epair_lan}b ${epair_wan_one}a
	jexec rtr ifconfig ${epair_lan}b inet6 2001:db8::1/64 up no_dad
	jexec rtr ifconfig ${epair_wan_one}a 192.0.2.1/24 up
	jexec rtr ifconfig ${epair_wan_one}a inet6 -ifdisabled
	jexec rtr route add default -inet6 fe80::1%${epair_wan_one}a
	#jexec rtr route add default 192.0.2.2

	vnet_mkjail wan_one ${epair_wan_one}b ${epair_wan_two}a
	jexec wan_one ifconfig ${epair_wan_one}b 192.0.2.2/24 up
	jexec wan_one ifconfig ${epair_wan_one}b inet6 fe80::1/64
	jexec wan_one ifconfig ${epair_wan_two}a 198.51.100.2/24 up
	jexec wan_one route add default 192.0.2.1
	jexec wan_one sysctl net.inet.ip.forwarding=1

	vnet_mkjail wan_two ${epair_wan_two}b
	jexec wan_two ifconfig ${epair_wan_two}b 198.51.100.1/24 up
	jexec wan_two route add default 198.51.100.2

	# Sanity checks
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 2001:db8::1
	atf_check -s exit:0 -o ignore \
	    jexec rtr ping -c 1 192.0.2.2
	atf_check -s exit:0 -o ignore \
	    jexec rtr ping -c 1 198.51.100.1

	jexec rtr pfctl -e
	pft_set_rules rtr \
	    "set reassemble yes" \
	    "set state-policy if-bound" \
	    "block" \
	    "pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
	    "pass in on ${epair_lan}b inet6 from any to 64:ff9b::/96 af-to inet from (${epair_wan_one}a)"

	atf_check -s exit:0 -o ignore \
	    -o match:'3 packets transmitted, 3 packets received, 0.0% packet loss' \
	    ping6 -c 3 64:ff9b::192.0.2.2
	atf_check -s exit:0 -o ignore \
	    -o match:'3 packets transmitted, 3 packets received, 0.0% packet loss' \
	    ping6 -c 3 64:ff9b::198.51.100.1
}

v6_gateway_cleanup()
{
	pft_cleanup
}

atf_test_case "scrub_min_ttl" "cleanup"
scrub_min_ttl_head()
{
	atf_set descr 'Ensure scrub min-ttl applies to nat64 traffic'
	atf_set require.user root
}

scrub_min_ttl_body()
{
	pft_init

	epair=$(vnet_mkepair)
	epair_link=$(vnet_mkepair)
	epair_link_two=$(vnet_mkepair)

	ifconfig ${epair}a inet6 2001:db8::2/64 up no_dad
	route -6 add default 2001:db8::1

	vnet_mkjail rtr ${epair}b ${epair_link}a
	jexec rtr ifconfig ${epair}b inet6 2001:db8::1/64 up no_dad
	jexec rtr ifconfig ${epair_link}a 192.0.2.1/24 up
	jexec rtr route add default 192.0.2.2

	vnet_mkjail rtr2 ${epair_link}b ${epair_link_two}a
	jexec rtr2 ifconfig ${epair_link}b 192.0.2.2/24 up
	jexec rtr2 ifconfig ${epair_link_two}a 198.51.100.2/24 up
	jexec rtr2 sysctl net.inet.ip.forwarding=1

	vnet_mkjail dst ${epair_link_two}b
	jexec dst ifconfig ${epair_link_two}b 198.51.100.1/24 up
	jexec dst route add default 198.51.100.2

	# Sanity checks
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 2001:db8::1
	atf_check -s exit:0 -o ignore \
	    jexec rtr ping -c 1 192.0.2.2
	atf_check -s exit:0 -o ignore \
	    jexec rtr ping -c 1 198.51.100.2
	atf_check -s exit:0 -o ignore \
	    jexec rtr ping -c 1 198.51.100.1

	jexec rtr pfctl -e
	pft_set_rules rtr \
	    "pass" \
	    "pass in on ${epair}b inet6 from any to 64:ff9b::/96 af-to inet from (${epair_link}a)"

	# Ping works with a normal TTL
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 64:ff9b::198.51.100.1

	# If we set a TTL of two the packet gets dropped
	atf_check -s exit:2 -o ignore \
	    ping6 -c 1 -m 2 64:ff9b::198.51.100.1

	# But if we have pf enforce a minimum ttl of 10 the ping does pass
	pft_set_rules rtr \
	    "pass" \
	    "pass in on ${epair}b inet6 from any to 64:ff9b::/96 af-to inet from (${epair_link}a) scrub (min-ttl 10)"
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 -m 2 64:ff9b::198.51.100.1
}

scub_min_ttl_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "icmp_echo_in"
	atf_add_test_case "icmp_echo_out"
	atf_add_test_case "fragmentation_in"
	atf_add_test_case "fragmentation_out"
	atf_add_test_case "tcp_in_if_bound"
	atf_add_test_case "tcp_out_if_bound"
	atf_add_test_case "tcp_in_floating"
	atf_add_test_case "tcp_out_floating"
	atf_add_test_case "udp_in"
	atf_add_test_case "udp_out"
	atf_add_test_case "sctp_in"
	atf_add_test_case "sctp_out"
	atf_add_test_case "tos"
	atf_add_test_case "no_v4"
	atf_add_test_case "range"
	atf_add_test_case "pool"
	atf_add_test_case "table"
	atf_add_test_case "table_range"
	atf_add_test_case "table_round_robin"
	atf_add_test_case "table_random"
	atf_add_test_case "dummynet"
	atf_add_test_case "gateway6"
	atf_add_test_case "route_to"
	atf_add_test_case "reply_to"
	atf_add_test_case "v6_gateway"
	atf_add_test_case "scrub_min_ttl"
}
