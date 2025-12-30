#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018 Kristof Provost <kp@FreeBSD.org>
# Copyright (c) 2025 Kajetan Staszkiewicz <ks@FreeBSD.org>
# Copyright (c) 2021 KUROSAWA Takahiro <takahiro.kurosawa@gmail.com>
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

atf_test_case "exhaust" "cleanup"
exhaust_head()
{
	atf_set descr 'Test exhausting the NAT pool'
	atf_set require.user root
}

exhaust_body()
{
	pft_init

	epair_nat=$(vnet_mkepair)
	epair_echo=$(vnet_mkepair)

	vnet_mkjail nat ${epair_nat}b ${epair_echo}a
	vnet_mkjail echo ${epair_echo}b

	ifconfig ${epair_nat}a 192.0.2.2/24 up
	route add -net 198.51.100.0/24 192.0.2.1

	jexec nat ifconfig ${epair_nat}b 192.0.2.1/24 up
	jexec nat ifconfig ${epair_echo}a 198.51.100.1/24 up
	jexec nat sysctl net.inet.ip.forwarding=1

	jexec echo ifconfig ${epair_echo}b 198.51.100.2/24 up
	jexec echo /usr/sbin/inetd -p ${PWD}/inetd-echo.pid $(atf_get_srcdir)/echo_inetd.conf

	# Disable checksum offload on one of the interfaces to ensure pf handles that
	jexec nat ifconfig ${epair_nat}a -txcsum

	# Enable pf!
	jexec nat pfctl -e
	pft_set_rules nat \
		"nat pass on ${epair_echo}a inet from 192.0.2.0/24 to any -> (${epair_echo}a) port 30000:30001 sticky-address"

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 3 198.51.100.2

	atf_check -s exit:0 -o match:foo* echo "foo" | nc -N 198.51.100.2 7
	atf_check -s exit:0 -o match:foo* echo "foo" | nc -N 198.51.100.2 7

	# This one will fail, but that's expected
	echo "foo" | nc -N 198.51.100.2 7 &

	sleep 1

	# If the kernel is stuck in pf_get_sport() this will not succeed either.
	timeout 2 jexec nat pfctl -sa
	if [ $? -eq 124 ]; then
		# Timed out
		atf_fail "pfctl timeout"
	fi
}

exhaust_cleanup()
{
	pft_cleanup
}

atf_test_case "nested_anchor" "cleanup"
nested_anchor_head()
{
	atf_set descr 'Test setting and retrieving nested nat anchors'
	atf_set require.user root
}

nested_anchor_body()
{
	pft_init

	epair=$(vnet_mkepair)

	vnet_mkjail nat ${epair}a

	pft_set_rules nat \
		"nat-anchor \"foo\""

	echo "nat-anchor \"bar\"" | jexec nat pfctl -g -a foo -f -
	echo "nat on ${epair}a from any to any -> (${epair}a)" | jexec nat pfctl -g -a "foo/bar" -f -

	atf_check -s exit:0 -o inline:"nat-anchor \"foo\" all {
  nat-anchor \"bar\" all {
    nat on ${epair}a all -> (${epair}a) round-robin
  }
}
" jexec nat pfctl -sn -a "*"

}

endpoint_independent_setup()
{
	pft_init
	filter="udp and dst port 1234"	# only capture udp pings

	epair_client=$(vnet_mkepair)
	epair_nat=$(vnet_mkepair)
	epair_server1=$(vnet_mkepair)
	epair_server2=$(vnet_mkepair)
	bridge=$(vnet_mkbridge)

	vnet_mkjail nat ${epair_client}b ${epair_nat}a
	vnet_mkjail client ${epair_client}a
	vnet_mkjail server1 ${epair_server1}a
	vnet_mkjail server2 ${epair_server2}a

	ifconfig ${epair_server1}b up
	ifconfig ${epair_server2}b up
	ifconfig ${epair_nat}b up
	ifconfig ${bridge} \
		addm ${epair_server1}b \
		addm ${epair_server2}b \
		addm ${epair_nat}b \
		up

	jexec nat ifconfig ${epair_client}b 192.0.2.1/24 up
	jexec nat ifconfig ${epair_nat}a 198.51.100.42/24 up
	jexec nat sysctl net.inet.ip.forwarding=1

	jexec client ifconfig ${epair_client}a 192.0.2.2/24 up
	jexec client route add default 192.0.2.1

	jexec server1 ifconfig ${epair_server1}a 198.51.100.32/24 up
	jexec server2 ifconfig ${epair_server2}a 198.51.100.22/24 up
}

endpoint_independent_common()
{
	# Enable pf!
	jexec nat pfctl -e

	# validate non-endpoint independent nat rule behaviour
	pft_set_rules nat "${1}"

	jexec server1 tcpdump -i ${epair_server1}a -w ${PWD}/server1.pcap \
		--immediate-mode $filter &
	server1tcppid="$!"
	jexec server2 tcpdump -i ${epair_server2}a -w ${PWD}/server2.pcap \
		--immediate-mode $filter &
	server2tcppid="$!"

	# send out multiple packets
	for i in $(seq 1 10); do
		echo "ping" | jexec client nc -u 198.51.100.32 1234 -p 4242 -w 0
		echo "ping" | jexec client nc -u 198.51.100.22 1234 -p 4242 -w 0
	done

	kill $server1tcppid
	kill $server2tcppid

	tuple_server1=$(tcpdump -r ${PWD}/server1.pcap | awk '{addr=$3} END {print addr}')
	tuple_server2=$(tcpdump -r ${PWD}/server2.pcap | awk '{addr=$3} END {print addr}')

	if [ -z $tuple_server1 ]
	then
		atf_fail "server1 did not receive connection from client (default)"
	fi

	if [ -z $tuple_server2 ]
	then
		atf_fail "server2 did not receive connection from client (default)"
	fi

	if [ "$tuple_server1" = "$tuple_server2" ]
	then
		echo "server1 tcpdump: $tuple_server1"
		echo "server2 tcpdump: $tuple_server2"
		atf_fail "Received same IP:port on server1 and server2 (default)"
	fi

	# validate endpoint independent nat rule behaviour
	pft_set_rules nat "${2}"

	jexec server1 tcpdump -i ${epair_server1}a -w ${PWD}/server1.pcap \
		--immediate-mode $filter &
	server1tcppid="$!"
	jexec server2 tcpdump -i ${epair_server2}a -w ${PWD}/server2.pcap \
		--immediate-mode $filter &
	server2tcppid="$!"

	# send out multiple packets,  sometimes one fails to go through
	for i in $(seq 1 10); do
		echo "ping" | jexec client nc -u 198.51.100.32 1234 -p 4242 -w 0
		echo "ping" | jexec client nc -u 198.51.100.22 1234 -p 4242 -w 0
	done

	kill $server1tcppid
	kill $server2tcppid

	tuple_server1=$(tcpdump -r ${PWD}/server1.pcap | awk '{addr=$3} END {print addr}')
	tuple_server2=$(tcpdump -r ${PWD}/server2.pcap | awk '{addr=$3} END {print addr}')

	if [ -z $tuple_server1 ]
	then
		atf_fail "server1 did not receive connection from client (endpoint-independent)"
	fi

	if [ -z $tuple_server2 ]
	then
		atf_fail "server2 did not receive connection from client (endpoint-independent)"
	fi

	if [ ! "$tuple_server1" = "$tuple_server2" ]
	then
		echo "server1 tcpdump: $tuple_server1"
		echo "server2 tcpdump: $tuple_server2"
		atf_fail "Received different IP:port on server1 than server2 (endpoint-independent)"
	fi
}

atf_test_case "endpoint_independent_compat" "cleanup"
endpoint_independent_compat_head()
{
	atf_set descr 'Test that a client behind NAT gets the same external IP:port for different servers'
	atf_set require.user root
}

endpoint_independent_compat_body()
{
	endpoint_independent_setup # Sets ${epair_…} variables

	endpoint_independent_common \
		"nat on ${epair_nat}a inet from ! (${epair_nat}a) to any -> (${epair_nat}a)" \
		"nat on ${epair_nat}a inet from ! (${epair_nat}a) to any -> (${epair_nat}a) endpoint-independent"
}

endpoint_independent_compat_cleanup()
{
	pft_cleanup
	rm -f server1.out
	rm -f server2.out
}

atf_test_case "endpoint_independent_exhaust" "cleanup"
endpoint_independent_exhaust_head()
{
	atf_set descr 'Test that a client behind NAT gets the same external IP:port for different servers'
	atf_set require.user root
}

endpoint_independent_exhaust_body()
{
	endpoint_independent_setup # Sets ${epair_…} variables

	endpoint_independent_common \
		"nat on ${epair_nat}a inet from ! (${epair_nat}a) to any -> (${epair_nat}a)" \
		"nat on ${epair_nat}a inet from ! (${epair_nat}a) to any -> (${epair_nat}a) port 3000:3001 sticky-address endpoint-independent"

	# Exhaust the available nat ports
	for i in $(seq 1 10); do
		echo "ping" | jexec client nc -u 198.51.100.32 1234 -w 0
		echo "ping" | jexec client nc -u 198.51.100.22 1234 -w 0
	done
}

endpoint_independent_exhaust_cleanup()
{
	pft_cleanup
	rm -f server1.out
	rm -f server2.out
}

atf_test_case "endpoint_independent_static_port" "cleanup"
endpoint_independent_static_port_head()
{
	atf_set descr 'Test that a client behind NAT gets the same external IP:port for different servers, with static-port'
	atf_set require.user root
}

endpoint_independent_static_port_body()
{
	endpoint_independent_setup # Sets ${epair_…} variables

	endpoint_independent_common \
		"nat on ${epair_nat}a inet from ! (${epair_nat}a) to any -> (${epair_nat}a)" \
		"nat on ${epair_nat}a inet from ! (${epair_nat}a) to any -> (${epair_nat}a) static-port sticky-address endpoint-independent"

	# Exhaust the available nat ports
	for i in $(seq 1 10); do
		echo "ping" | jexec client nc -u 198.51.100.32 1234 -w 0
		echo "ping" | jexec client nc -u 198.51.100.22 1234 -w 0
	done
}

endpoint_independent_static_port_cleanup()
{
	pft_cleanup
	rm -f server1.out
	rm -f server2.out
}

atf_test_case "endpoint_independent_pass" "cleanup"
endpoint_independent_pass_head()
{
	atf_set descr 'Test that a client behind NAT gets the same external IP:port for different servers'
	atf_set require.user root
}

endpoint_independent_pass_body()
{
	endpoint_independent_setup # Sets ${epair_…} variables

	endpoint_independent_common \
		"pass out on ${epair_nat}a inet from ! (${epair_nat}a) to any nat-to (${epair_nat}a) keep state" \
		"pass out on ${epair_nat}a inet from ! (${epair_nat}a) to any nat-to (${epair_nat}a) endpoint-independent keep state"

}

endpoint_independent_pass_cleanup()
{
	pft_cleanup
	rm -f server1.out
	rm -f server2.out
}

nested_anchor_cleanup()
{
	pft_cleanup
}

atf_test_case "nat6_nolinklocal" "cleanup"
nat6_nolinklocal_head()
{
	atf_set descr 'Ensure we do not use link-local addresses'
	atf_set require.user root
}

nat6_nolinklocal_body()
{
	pft_init

	epair_nat=$(vnet_mkepair)
	epair_echo=$(vnet_mkepair)

	vnet_mkjail nat ${epair_nat}b ${epair_echo}a
	vnet_mkjail echo ${epair_echo}b

	ifconfig ${epair_nat}a inet6 2001:db8::2/64 no_dad up
	route add -6 -net 2001:db8:1::/64 2001:db8::1

	jexec nat ifconfig ${epair_nat}b inet6 2001:db8::1/64 no_dad up
	jexec nat ifconfig ${epair_echo}a inet6 2001:db8:1::1/64 no_dad up
	jexec nat sysctl net.inet6.ip6.forwarding=1

	jexec echo ifconfig ${epair_echo}b inet6 2001:db8:1::2/64 no_dad up
	# Ensure we can't reply to link-local pings
	jexec echo pfctl -e
	pft_set_rules echo \
	    "pass" \
	    "block in inet6 proto icmp6 from fe80::/10 to any icmp6-type echoreq"

	jexec nat pfctl -e
	pft_set_rules nat \
	    "nat pass on ${epair_echo}a inet6 from 2001:db8::/64 to any -> (${epair_echo}a)" \
	    "pass"

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    ping -6 -c 1 2001:db8::1
	for i in `seq 0 10`
	do
		atf_check -s exit:0 -o ignore \
		    ping -6 -c 1 2001:db8:1::2
	done
}

nat6_nolinklocal_cleanup()
{
	pft_cleanup
}

empty_table_common()
{
	option=$1

	pft_init

	epair_wan=$(vnet_mkepair)
	epair_lan=$(vnet_mkepair)

	vnet_mkjail srv ${epair_wan}a
	jexec srv ifconfig ${epair_wan}a 192.0.2.2/24 up

	vnet_mkjail rtr ${epair_wan}b ${epair_lan}a
	jexec rtr ifconfig ${epair_wan}b 192.0.2.1/24 up
	jexec rtr ifconfig ${epair_lan}a 198.51.100.1/24 up
	jexec rtr sysctl net.inet.ip.forwarding=1

	ifconfig ${epair_lan}b 198.51.100.2/24 up
	route add default 198.51.100.1

	jexec rtr pfctl -e
	pft_set_rules rtr \
	    "table <empty>" \
	    "nat on ${epair_wan}b inet from 198.51.100.0/24 -> <empty> ${option}" \
	    "pass"

	# Sanity checks
	atf_check -s exit:0 -o ignore \
	    jexec rtr ping -c 1 192.0.2.2
	atf_check -s exit:0 -o ignore \
	    ping -c 1 198.51.100.1
	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1

	# Provoke divide by zero
	ping -c 1 192.0.2.2
	true
}

atf_test_case "empty_table_source_hash" "cleanup"
empty_table_source_hash_head()
{
	atf_set descr 'Test source-hash on an emtpy table'
	atf_set require.user root
}

empty_table_source_hash_body()
{
	empty_table_common "source-hash"
}

empty_table_source_hash_cleanup()
{
	pft_cleanup
}

atf_test_case "empty_table_random" "cleanup"
empty_table_random_head()
{
	atf_set descr 'Test random on an emtpy table'
	atf_set require.user root
}

empty_table_random_body()
{
	empty_table_common "random"
}

empty_table_random_cleanup()
{
	pft_cleanup
}

no_addrs_common()
{
	option=$1

	pft_init

	epair_wan=$(vnet_mkepair)
	epair_lan=$(vnet_mkepair)

	vnet_mkjail srv ${epair_wan}a
	jexec srv ifconfig ${epair_wan}a 192.0.2.2/24 up

	vnet_mkjail rtr ${epair_wan}b ${epair_lan}a
	jexec rtr route add -net 192.0.2.0/24 -iface ${epair_wan}b
	jexec rtr ifconfig ${epair_lan}a 198.51.100.1/24 up
	jexec rtr sysctl net.inet.ip.forwarding=1

	ifconfig ${epair_lan}b 198.51.100.2/24 up
	route add default 198.51.100.1

	jexec rtr pfctl -e
	pft_set_rules rtr \
	    "nat on ${epair_wan}b inet from 198.51.100.0/24 -> (${epair_wan}b) ${option}" \
	    "pass"

	# Provoke divide by zero
	ping -c 1 192.0.2.2
	true
}

atf_test_case "no_addrs_source_hash" "cleanup"
no_addrs_source_hash_head()
{
	atf_set descr 'Test source-hash on an interface with no addresses'
	atf_set require.user root
}

no_addrs_source_hash_body()
{
	no_addrs_common "source-hash"
}

no_addrs_source_hash_cleanup()
{
	pft_cleanup
}

atf_test_case "no_addrs_random" "cleanup"
no_addrs_random_head()
{
	atf_set descr 'Test random on an interface with no addresses'
	atf_set require.user root
}

no_addrs_random_body()
{
	no_addrs_common "random"
}

no_addrs_random_cleanup()
{
	pft_cleanup
}

atf_test_case "nat_pass_in" "cleanup"
nat_pass_in_head()
{
	atf_set descr 'IPv4 NAT on inbound pass rule'
	atf_set require.user root
	atf_set require.progs scapy
}

nat_pass_in_body()
{
	setup_router_server_ipv4
	# Delete the route back to make sure that the traffic has been NAT-ed
	jexec server route del -net ${net_tester} ${net_server_host_router}
	# Provide routing back to the NAT address
	jexec server route add 203.0.113.0/24 ${net_server_host_router}
	jexec router route add 203.0.113.0/24 -iface ${epair_tester}b

	pft_set_rules router \
		"block" \
		"pass in  on ${epair_tester}b inet proto tcp nat-to 203.0.113.0 keep state" \
		"pass out on ${epair_server}a inet proto tcp keep state"

	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4201

	jexec router pfctl -qvvsr
	jexec router pfctl -qvvss
	jexec router ifconfig
	jexec router netstat -rn
}

nat_pass_in_cleanup()
{
	pft_cleanup
}

nat_pass_out_head()
{
	atf_set descr 'IPv4 NAT on outbound pass rule'
	atf_set require.user root
	atf_set require.progs scapy
}

nat_pass_out_body()
{
	setup_router_server_ipv4
	# Delete the route back to make sure that the traffic has been NAT-ed
	jexec server route del -net ${net_tester} ${net_server_host_router}

	pft_set_rules router \
		"block" \
		"pass in  on ${epair_tester}b inet proto tcp keep state" \
		"pass out on ${epair_server}a inet proto tcp nat-to ${epair_server}a keep state"

	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4201

	jexec router pfctl -qvvsr
	jexec router pfctl -qvvss
	jexec router ifconfig
	jexec router netstat -rn
}

nat_pass_out_cleanup()
{
	pft_cleanup
}

atf_test_case "nat_match" "cleanup"
nat_match_head()
{
	atf_set descr 'IPv4 NAT on match rule'
	atf_set require.user root
	atf_set require.progs scapy
}

nat_match_body()
{
	setup_router_server_ipv4
	# Delete the route back to make sure that the traffic has been NAT-ed
	jexec server route del -net ${net_tester} ${net_server_host_router}

	# NAT is applied during ruleset evaluation:
	# rules after "match" match on NAT-ed address
	pft_set_rules router \
		"block" \
		"pass in  on ${epair_tester}b inet proto tcp keep state" \
		"match out on ${epair_server}a inet proto tcp nat-to ${epair_server}a" \
		"pass out on ${epair_server}a inet proto tcp from ${epair_server}a keep state"

	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4201

	jexec router pfctl -qvvsr
	jexec router pfctl -qvvss
	jexec router ifconfig
	jexec router netstat -rn
}

nat_match_cleanup()
{
	pft_cleanup
}

map_e_common()
{
	NC_TRY_COUNT=12

	pft_init

	epair_map_e=$(vnet_mkepair)
	epair_echo=$(vnet_mkepair)

	vnet_mkjail map_e ${epair_map_e}b ${epair_echo}a
	vnet_mkjail echo ${epair_echo}b

	ifconfig ${epair_map_e}a 192.0.2.2/24 up
	route add -net 198.51.100.0/24 192.0.2.1

	jexec map_e ifconfig ${epair_map_e}b 192.0.2.1/24 up
	jexec map_e ifconfig ${epair_echo}a 198.51.100.1/24 up
	jexec map_e sysctl net.inet.ip.forwarding=1

	jexec echo ifconfig ${epair_echo}b 198.51.100.2/24 up
	jexec echo /usr/sbin/inetd -p ${PWD}/inetd-echo.pid $(atf_get_srcdir)/echo_inetd.conf

	# Enable pf!
	jexec map_e pfctl -e
}

atf_test_case "map_e_compat" "cleanup"
map_e_compat_head()
{
	atf_set descr 'map-e-portset test'
	atf_set require.user root
}

map_e_compat_body()
{
	map_e_common

	pft_set_rules map_e \
		"nat pass on ${epair_echo}a inet from 192.0.2.0/24 to any -> (${epair_echo}a) map-e-portset 2/12/0x342"

	# Only allow specified ports.
	jexec echo pfctl -e
	pft_set_rules echo "block return all" \
		"pass in on ${epair_echo}b inet proto tcp from 198.51.100.1 port 19720:19723 to (${epair_echo}b) port 7" \
		"pass in on ${epair_echo}b inet proto tcp from 198.51.100.1 port 36104:36107 to (${epair_echo}b) port 7" \
		"pass in on ${epair_echo}b inet proto tcp from 198.51.100.1 port 52488:52491 to (${epair_echo}b) port 7" \
		"set skip on lo"

	i=0
	while [ ${i} -lt ${NC_TRY_COUNT} ]
	do
		echo "foo ${i}" | timeout 2 nc -N 198.51.100.2 7
		if [ $? -ne 0 ]; then
			atf_fail "nc failed (${i})"
		fi
		i=$((${i}+1))
	done
}

map_e_compat_cleanup()
{
	pft_cleanup
}


atf_test_case "map_e_pass" "cleanup"
map_e_pass_head()
{
	atf_set descr 'map-e-portset test'
	atf_set require.user root
}

map_e_pass_body()
{
	map_e_common

	pft_set_rules map_e \
		"pass out on ${epair_echo}a inet from 192.0.2.0/24 to any nat-to (${epair_echo}a) map-e-portset 2/12/0x342 keep state"

	jexec map_e pfctl -qvvsr

	# Only allow specified ports.
	jexec echo pfctl -e
	pft_set_rules echo "block return all" \
		"pass in on ${epair_echo}b inet proto tcp from 198.51.100.1 port 19720:19723 to (${epair_echo}b) port 7" \
		"pass in on ${epair_echo}b inet proto tcp from 198.51.100.1 port 36104:36107 to (${epair_echo}b) port 7" \
		"pass in on ${epair_echo}b inet proto tcp from 198.51.100.1 port 52488:52491 to (${epair_echo}b) port 7" \
		"set skip on lo"

	i=0
	while [ ${i} -lt ${NC_TRY_COUNT} ]
	do
		echo "foo ${i}" | timeout 2 nc -N 198.51.100.2 7
		if [ $? -ne 0 ]; then
			atf_fail "nc failed (${i})"
		fi
		i=$((${i}+1))
	done
}

map_e_pass_cleanup()
{
	pft_cleanup
}

atf_test_case "binat_compat" "cleanup"
binat_compat_head()
{
	atf_set descr 'IPv4 BINAT with nat ruleset'
	atf_set require.user root
	atf_set require.progs scapy
}

binat_compat_body()
{
	setup_router_server_ipv4
	# Delete the route back to make sure that the traffic has been NAT-ed
	jexec server route del -net ${net_tester} ${net_server_host_router}

	pft_set_rules router \
		"set state-policy if-bound" \
		"set ruleset-optimization none" \
		"binat on ${epair_server}a inet proto tcp from ${net_tester_host_tester} to any tag sometag -> ${epair_server}a" \
		"block" \
		"pass in  on ${epair_tester}b inet proto tcp !tagged sometag keep state" \
		"pass out on ${epair_server}a inet proto tcp tagged sometag keep state" \
		"pass in  on ${epair_server}a inet proto tcp tagged sometag keep state" \
		"pass out on ${epair_tester}b inet proto tcp tagged sometag keep state"

	# Test the outbound NAT part of BINAT.
	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4201

	states=$(mktemp) || exit 1
	jexec router pfctl -qvss | normalize_pfctl_s > $states

	for state_regexp in \
		"${epair_tester}b tcp ${net_server_host_server}:9 <- ${net_tester_host_tester}:4201 .* 3:2 pkts,.* rule 1" \
		"${epair_server}a tcp ${net_server_host_router}:4201 \(${net_tester_host_tester}:4201\) -> ${net_server_host_server}:9 .* 3:2 pkts,.* rule 2" \
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done

	# Test the inbound RDR part of BINAT.
	# The "tester" becomes "server" and vice versa.
	inetd_conf=$(mktemp)
	echo "discard stream tcp nowait root internal" > $inetd_conf
	inetd -p ${PWD}/inetd_tester.pid $inetd_conf

	atf_check -s exit:0 \
	jexec server ${common_dir}/pft_ping.py \
	    --ping-type=tcp3way --send-sport=4202 \
	    --sendif ${epair_server}b \
	    --to ${net_server_host_router} \
	    --replyif ${epair_server}b

	states=$(mktemp) || exit 1
	jexec router pfctl -qvss | normalize_pfctl_s > $states

	for state_regexp in \
		"${epair_server}a tcp ${net_tester_host_tester}:9 \(${net_server_host_router}:9\) <- ${net_server_host_server}:4202 .* 3:2 pkts,.* rule 3" \
		"${epair_tester}b tcp ${net_server_host_server}:4202 -> ${net_tester_host_tester}:9 .* 3:2 pkts,.* rule 4" \
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done
}

binat_compat_cleanup()
{
	[ -f ${PWD}/inetd_tester.pid ] && kill $(cat ${PWD}/inetd_tester.pid)
	pft_cleanup
}

atf_test_case "binat_match" "cleanup"
binat_match_head()
{
	atf_set descr 'IPv4 BINAT with nat ruleset'
	atf_set require.user root
	atf_set require.progs scapy
}

binat_match_body()
{
	setup_router_server_ipv4
	# Delete the route back to make sure that the traffic has been NAT-ed
	jexec server route del -net ${net_tester} ${net_server_host_router}

	# The "binat-to" rule expands to 2 rules so the ""pass" rules start at 3!
	pft_set_rules router \
		"set state-policy if-bound" \
		"set ruleset-optimization none" \
		"block" \
		"match on ${epair_server}a inet proto tcp from ${net_tester_host_tester} to any tag sometag binat-to ${epair_server}a" \
		"pass in  on ${epair_tester}b inet proto tcp !tagged sometag keep state" \
		"pass out on ${epair_server}a inet proto tcp tagged sometag keep state" \
		"pass in  on ${epair_server}a inet proto tcp tagged sometag keep state" \
		"pass out on ${epair_tester}b inet proto tcp tagged sometag keep state"

	# Test the outbound NAT part of BINAT.
	ping_server_check_reply exit:0 --ping-type=tcp3way --send-sport=4201

	states=$(mktemp) || exit 1
	jexec router pfctl -qvss | normalize_pfctl_s > $states

	for state_regexp in \
		"${epair_tester}b tcp ${net_server_host_server}:9 <- ${net_tester_host_tester}:4201 .* 3:2 pkts,.* rule 3" \
		"${epair_server}a tcp ${net_server_host_router}:4201 \(${net_tester_host_tester}:4201\) -> ${net_server_host_server}:9 .* 3:2 pkts,.* rule 4" \
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done

	# Test the inbound RDR part of BINAT.
	# The "tester" becomes "server" and vice versa.
	inetd_conf=$(mktemp)
	echo "discard stream tcp nowait root internal" > $inetd_conf
	inetd -p ${PWD}/inetd_tester.pid $inetd_conf

	atf_check -s exit:0 \
	jexec server ${common_dir}/pft_ping.py \
	    --ping-type=tcp3way --send-sport=4202 \
	    --sendif ${epair_server}b \
	    --to ${net_server_host_router} \
	    --replyif ${epair_server}b

	states=$(mktemp) || exit 1
	jexec router pfctl -qvss | normalize_pfctl_s > $states

	for state_regexp in \
		"${epair_server}a tcp ${net_tester_host_tester}:9 \(${net_server_host_router}:9\) <- ${net_server_host_server}:4202 .* 3:2 pkts,.* rule 5" \
		"${epair_tester}b tcp ${net_server_host_server}:4202 -> ${net_tester_host_tester}:9 .* 3:2 pkts,.* rule 6" \
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done
}

binat_match_cleanup()
{
	[ -f ${PWD}/inetd_tester.pid ] && kill $(cat ${PWD}/inetd_tester.pid)
	pft_cleanup
}

atf_test_case "empty_pool" "cleanup"
empty_pool_head()
{
	atf_set descr 'NAT with empty pool'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

empty_pool_body()
{
	pft_init
	setup_router_server_ipv6


	pft_set_rules router \
		"block" \
		"pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
		"pass in  on ${epair_tester}b" \
		"pass out on ${epair_server}a inet6 from any to ${net_server_host_server} nat-to <nonexistent>" \

	# pf_map_addr_sn() won't be able to pick a target address, because
	# the table used in redireciton pool is empty. Packet will not be
	# forwarded, error counter will be increased.
	ping_server_check_reply exit:1
	# Ignore warnings about not-loaded ALTQ
	atf_check -o "match:map-failed +1 +" -x "jexec router pfctl -qvvsi 2> /dev/null"
}

empty_pool_cleanup()
{
	pft_cleanup
}

atf_test_case "dummynet_mask" "cleanup"
dummynet_mask_head()
{
	atf_set descr 'Verify that dummynet uses the pre-nat address for masking'
	atf_set require.user root
}

dummynet_mask_body()
{
	dummynet_init

	epair_srv=$(vnet_mkepair)
	epair_cl=$(vnet_mkepair)

	ifconfig ${epair_cl}b 192.0.2.2/24 up
	route add default 192.0.2.1

	vnet_mkjail srv ${epair_srv}a
	jexec srv ifconfig ${epair_srv}a 198.51.100.2/24 up

	vnet_mkjail gw ${epair_srv}b ${epair_cl}a
	jexec gw ifconfig ${epair_srv}b 198.51.100.1/24 up
	jexec gw ifconfig ${epair_cl}a 192.0.2.1/24 up
	jexec gw sysctl net.inet.ip.forwarding=1

	jexec gw dnctl pipe 1 config delay 100 mask src-ip 0xffffff00
	jexec gw pfctl -e
	pft_set_rules gw \
	    "nat on ${epair_srv}b inet from 192.0.2.0/24 to any -> (${epair_srv}b)" \
	    "pass out dnpipe 1"

	atf_check -s exit:0 -o ignore \
	    ping -c  3 198.51.100.2

	# Now check that dummynet looked at the correct address
	atf_check -s exit:0 -o match:"ip.*192.0.2.0/0" \
	    jexec gw dnctl pipe show
}

dummynet_mask_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "exhaust"
	atf_add_test_case "nested_anchor"
	atf_add_test_case "endpoint_independent_compat"
	atf_add_test_case "endpoint_independent_exhaust"
	atf_add_test_case "endpoint_independent_static_port"
	atf_add_test_case "endpoint_independent_pass"
	atf_add_test_case "nat6_nolinklocal"
	atf_add_test_case "empty_table_source_hash"
	atf_add_test_case "no_addrs_source_hash"
	atf_add_test_case "empty_table_random"
	atf_add_test_case "no_addrs_random"
	atf_add_test_case "map_e_compat"
	atf_add_test_case "map_e_pass"
	atf_add_test_case "nat_pass_in"
	atf_add_test_case "nat_pass_out"
	atf_add_test_case "nat_match"
	atf_add_test_case "binat_compat"
	atf_add_test_case "binat_match"
	atf_add_test_case "empty_pool"
	atf_add_test_case "dummynet_mask"
}
