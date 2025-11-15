#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018 Kristof Provost <kp@FreeBSD.org>
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

common_dir=$(atf_get_srcdir)/../common

# We need to somehow test if the random algorithm of pf_map_addr() is working.
# The table or prefix contains multiple IP next-hop addresses, for each one try
# to establish up to 10 connections. Fail the test if with this many attempts
# the "good" target has not been chosen. However this choice is random,
# the check might still ocasionally fail.
check_random() {
	if [ "$1" = "IPv4" ]; then
		ping_from="${net_clients_4}.1"
		ping_to="${host_server_4}"
	else
		ping_from="${net_clients_6}::1"
		ping_to="${host_server_6}"
	fi
	good_targets="$2"
	bad_targets="$3"

	port=42000
	states=$(mktemp) || exit 1
	for good_target in $good_targets; do
		found="no"
		for attempt in $(seq 1 10); do
			port=$(( port + 1 ))
			jexec router pfctl -Fs
			atf_check -s exit:0 ${common_dir}/pft_ping.py \
				--sendif ${epair_tester}a --replyif ${epair_tester}a \
				--fromaddr ${ping_from} --to ${ping_to} \
				--ping-type=tcp3way --send-sport=${port}
			jexec router pfctl -qvvss | normalize_pfctl_s > $states
			cat $states
			if [ -n "${bad_targets}" ]; then
				for bad_target in $bad_targets; do
					if grep -qE "route-to: ${bad_target}@" $states; then
						atf_fail "Bad target ${bad_target} selected!"
					fi
				done
			fi;
			if grep -qE "route-to: ${good_target}@" $states; then
				found=yes
				break
			fi
		done
		if [ "${found}" = "no" ]; then
			atf_fail "Target ${good_target} not selected after ${attempt} attempts!"
		fi
	done
}

pf_map_addr_common()
{
	setup_router_server_nat64

	# Clients will connect from another network behind the router.
	# This allows for using multiple source addresses.
	jexec router route add -6 ${net_clients_6}::/${net_clients_6_mask} ${net_tester_6_host_tester}
	jexec router route add    ${net_clients_4}.0/${net_clients_4_mask} ${net_tester_4_host_tester}

	# The servers are reachable over additional IP addresses for
	# testing of tables and subnets. The addresses are noncontinougnus
	# for pf_map_addr() counter tests.
	for i in 0 1 4 5; do
		a1=$((24 + i))
		jexec server1 ifconfig ${epair_server1}b inet  ${net_server1_4}.${a1}/32 alias
		jexec server1 ifconfig ${epair_server1}b inet6 ${net_server1_6}::42:${i}/128 alias
		a2=$((40 + i))
		jexec server2 ifconfig ${epair_server2}b inet  ${net_server2_4}.${a2}/32 alias
		jexec server2 ifconfig ${epair_server2}b inet6 ${net_server2_6}::42:${i}/128 alias
	done
}

atf_test_case "v4" "cleanup"
v4_head()
{
	atf_set descr 'Basic route-to test'
	atf_set require.user root
}

v4_body()
{
	pft_init

	epair_send=$(vnet_mkepair)
	ifconfig ${epair_send}a 192.0.2.1/24 up
	epair_route=$(vnet_mkepair)
	ifconfig ${epair_route}a 203.0.113.1/24 up

	vnet_mkjail alcatraz ${epair_send}b ${epair_route}b
	jexec alcatraz ifconfig ${epair_send}b 192.0.2.2/24 up
	jexec alcatraz ifconfig ${epair_route}b 203.0.113.2/24 up
	jexec alcatraz route add -net 198.51.100.0/24 192.0.2.1
	jexec alcatraz pfctl -e

	# Attempt to provoke PR 228782
	pft_set_rules alcatraz "block all" "pass user 2" \
		"pass out route-to (${epair_route}b 203.0.113.1) from 192.0.2.2 to 198.51.100.1 no state"
	jexec alcatraz nc -w 3 -s 192.0.2.2 198.51.100.1 22

	# atf wants us to not return an error, but our netcat will fail
	true
}

v4_cleanup()
{
	pft_cleanup
}

atf_test_case "v6" "cleanup"
v6_head()
{
	atf_set descr 'Basic route-to test (IPv6)'
	atf_set require.user root
}

v6_body()
{
	pft_init

	epair_send=$(vnet_mkepair)
	ifconfig ${epair_send}a inet6 2001:db8:42::1/64 up no_dad -ifdisabled
	epair_route=$(vnet_mkepair)
	ifconfig ${epair_route}a inet6 2001:db8:43::1/64 up no_dad -ifdisabled

	vnet_mkjail alcatraz ${epair_send}b ${epair_route}b
	jexec alcatraz ifconfig ${epair_send}b inet6 2001:db8:42::2/64 up no_dad
	jexec alcatraz ifconfig ${epair_route}b inet6 2001:db8:43::2/64 up no_dad
	jexec alcatraz route add -6 2001:db8:666::/64 2001:db8:42::2
	jexec alcatraz pfctl -e

	# Attempt to provoke PR 228782
	pft_set_rules alcatraz "block all" "pass user 2" \
		"pass out route-to (${epair_route}b 2001:db8:43::1) from 2001:db8:42::2 to 2001:db8:666::1 no state"
	jexec alcatraz nc -6 -w 3 -s 2001:db8:42::2 2001:db8:666::1 22

	# atf wants us to not return an error, but our netcat will fail
	true
}

v6_cleanup()
{
	pft_cleanup
}

atf_test_case "multiwan" "cleanup"
multiwan_head()
{
	atf_set descr 'Multi-WAN redirection / reply-to test'
	atf_set require.user root
}

multiwan_body()
{
	pft_init

	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)
	epair_cl_one=$(vnet_mkepair)
	epair_cl_two=$(vnet_mkepair)

	vnet_mkjail srv ${epair_one}b ${epair_two}b
	vnet_mkjail wan_one ${epair_one}a ${epair_cl_one}b
	vnet_mkjail wan_two ${epair_two}a ${epair_cl_two}b
	vnet_mkjail client ${epair_cl_one}a ${epair_cl_two}a

	jexec client ifconfig ${epair_cl_one}a 203.0.113.1/25
	jexec wan_one ifconfig ${epair_cl_one}b 203.0.113.2/25
	jexec wan_one ifconfig ${epair_one}a 192.0.2.1/24 up
	jexec wan_one sysctl net.inet.ip.forwarding=1
	jexec srv ifconfig ${epair_one}b 192.0.2.2/24 up
	jexec client route add 192.0.2.0/24 203.0.113.2

	jexec client ifconfig ${epair_cl_two}a 203.0.113.128/25
	jexec wan_two ifconfig ${epair_cl_two}b 203.0.113.129/25
	jexec wan_two ifconfig ${epair_two}a 198.51.100.1/24 up
	jexec wan_two sysctl net.inet.ip.forwarding=1
	jexec srv ifconfig ${epair_two}b 198.51.100.2/24 up
	jexec client route add 198.51.100.0/24 203.0.113.129

	jexec srv ifconfig lo0 127.0.0.1/8 up
	jexec srv route add default 192.0.2.1
	jexec srv sysctl net.inet.ip.forwarding=1

	# Run echo server in srv jail
	jexec srv /usr/sbin/inetd -p ${PWD}/multiwan.pid $(atf_get_srcdir)/echo_inetd.conf

	jexec srv pfctl -e
	pft_set_rules srv \
		"nat on ${epair_one}b inet from 127.0.0.0/8 to any -> (${epair_one}b)" \
		"nat on ${epair_two}b inet from 127.0.0.0/8 to any -> (${epair_two}b)" \
		"rdr on ${epair_one}b inet proto tcp from any to 192.0.2.2 port 7 -> 127.0.0.1 port 7" \
		"rdr on ${epair_two}b inet proto tcp from any to 198.51.100.2 port 7 -> 127.0.0.1 port 7" \
		"block in"	\
		"block out"	\
		"pass in quick on ${epair_one}b reply-to (${epair_one}b 192.0.2.1) inet proto tcp from any to 127.0.0.1 port 7" \
		"pass in quick on ${epair_two}b reply-to (${epair_two}b 198.51.100.1) inet proto tcp from any to 127.0.0.1 port 7"

	# These will always succeed, because we don't change interface to route
	# correctly here.
	result=$(echo "one" | jexec wan_one nc -N -w 3 192.0.2.2 7)
	if [ "${result}" != "one" ]; then
		atf_fail "Redirect on one failed"
	fi
	result=$(echo "two" | jexec wan_two nc -N -w 3 198.51.100.2 7)
	if [ "${result}" != "two" ]; then
		atf_fail "Redirect on two failed"
	fi

	result=$(echo "one" | jexec client nc -N -w 3 192.0.2.2 7)
	if [ "${result}" != "one" ]; then
		atf_fail "Redirect from client on one failed"
	fi

	# This should trigger the issue fixed in 829a69db855b48ff7e8242b95e193a0783c489d9
	result=$(echo "two" | jexec client nc -N -w 3 198.51.100.2 7)
	if [ "${result}" != "two" ]; then
		atf_fail "Redirect from client on two failed"
	fi
}

multiwan_cleanup()
{
	pft_cleanup
}

atf_test_case "multiwanlocal" "cleanup"
multiwanlocal_head()
{
	atf_set descr 'Multi-WAN local origin source-based redirection / route-to test'
	atf_set require.user root
}

multiwanlocal_body()
{
	pft_init

	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)
	epair_cl_one=$(vnet_mkepair)
	epair_cl_two=$(vnet_mkepair)

	vnet_mkjail srv1 ${epair_one}b
	vnet_mkjail srv2 ${epair_two}b
	vnet_mkjail wan_one ${epair_one}a ${epair_cl_one}b
	vnet_mkjail wan_two ${epair_two}a ${epair_cl_two}b
	vnet_mkjail client ${epair_cl_one}a ${epair_cl_two}a

	jexec client ifconfig ${epair_cl_one}a 203.0.113.1/25
	jexec wan_one ifconfig ${epair_cl_one}b 203.0.113.2/25
	jexec wan_one ifconfig ${epair_one}a 192.0.2.1/24 up
	jexec wan_one sysctl net.inet.ip.forwarding=1
	jexec srv1 ifconfig ${epair_one}b 192.0.2.2/24 up

	jexec client ifconfig ${epair_cl_two}a 203.0.113.128/25
	jexec wan_two ifconfig ${epair_cl_two}b 203.0.113.129/25
	jexec wan_two ifconfig ${epair_two}a 198.51.100.1/24 up
	jexec wan_two sysctl net.inet.ip.forwarding=1
	jexec srv2 ifconfig ${epair_two}b 198.51.100.2/24 up

	jexec client route add default 203.0.113.2
	jexec srv1 route add default 192.0.2.1
	jexec srv2 route add default 198.51.100.1

	# Run data source in srv1 and srv2
	jexec srv1 sh -c 'dd if=/dev/zero bs=1024 count=100 | nc -l 7 -w 2 -N &'
	jexec srv2 sh -c 'dd if=/dev/zero bs=1024 count=100 | nc -l 7 -w 2 -N &'

	jexec client pfctl -e
	pft_set_rules client \
		"block in"	\
		"block out"	\
		"pass out quick route-to (${epair_cl_two}a 203.0.113.129) inet proto tcp from 203.0.113.128 to any port 7" \
		"pass out on ${epair_cl_one}a inet proto tcp from any to any port 7" \
		"set skip on lo"

	# This should work
	result=$(jexec client nc -N -w 1 192.0.2.2 7 | wc -c)
	if [ ${result} -ne 102400 ]; then
		jexec client pfctl -ss
		atf_fail "Redirect from client on one failed: ${result}"
	fi

	# This should trigger the issue
	result=$(jexec client nc -N -w 1 -s 203.0.113.128 198.51.100.2 7 | wc -c)
	jexec client pfctl -ss
	if [ ${result} -ne 102400 ]; then
		atf_fail "Redirect from client on two failed: ${result}"
	fi
}

multiwanlocal_cleanup()
{
	pft_cleanup
}

atf_test_case "icmp_nat" "cleanup"
icmp_nat_head()
{
	atf_set descr 'Test that ICMP packets are correct for route-to + NAT'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

icmp_nat_body()
{
	pft_init

	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)
	epair_three=$(vnet_mkepair)

	vnet_mkjail gw ${epair_one}b ${epair_two}a ${epair_three}a
	vnet_mkjail srv ${epair_two}b
	vnet_mkjail srv2 ${epair_three}b

	ifconfig ${epair_one}a 192.0.2.2/24 up
	route add -net 198.51.100.0/24 192.0.2.1
	jexec gw sysctl net.inet.ip.forwarding=1
	jexec gw ifconfig ${epair_one}b 192.0.2.1/24 up
	jexec gw ifconfig ${epair_two}a 198.51.100.1/24 up
	jexec gw ifconfig ${epair_three}a 203.0.113.1/24 up mtu 500
	jexec srv ifconfig ${epair_two}b 198.51.100.2/24 up
	jexec srv route add default 198.51.100.1
	jexec srv2 ifconfig ${epair_three}b 203.0.113.2/24 up mtu 500
	jexec srv2 route add default 203.0.113.1

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 198.51.100.2

	jexec gw pfctl -e
	pft_set_rules gw \
		"nat on ${epair_two}a inet from 192.0.2.0/24 to any -> (${epair_two}a)" \
		"nat on ${epair_three}a inet from 192.0.2.0/24 to any -> (${epair_three}a)" \
		"pass out route-to (${epair_three}a 203.0.113.2) proto icmp icmp-type echoreq"

	# Now ensure that we get an ICMP error with the correct IP addresses in it.
	atf_check -s exit:0 ${common_dir}/pft_icmp_check.py \
		--to 198.51.100.2 \
		--fromaddr 192.0.2.2 \
		--recvif ${epair_one}a \
		--sendif ${epair_one}a

	# ping reports the ICMP error, so check of that too.
	atf_check -s exit:2 -o match:'frag needed and DF set' \
		ping -D -c 1 -s 1000 198.51.100.2
}

icmp_nat_cleanup()
{
	pft_cleanup
}

atf_test_case "dummynet" "cleanup"
dummynet_head()
{
	atf_set descr 'Test that dummynet applies to route-to packets'
	atf_set require.user root
}

dummynet_body()
{
	dummynet_init

	epair_srv=$(vnet_mkepair)
	epair_gw=$(vnet_mkepair)

	vnet_mkjail srv ${epair_srv}a
	jexec srv ifconfig ${epair_srv}a 192.0.2.1/24 up
	jexec srv route add default 192.0.2.2

	vnet_mkjail gw ${epair_srv}b ${epair_gw}a
	jexec gw ifconfig ${epair_srv}b 192.0.2.2/24 up
	jexec gw ifconfig ${epair_gw}a 198.51.100.1/24 up
	jexec gw sysctl net.inet.ip.forwarding=1

	ifconfig ${epair_gw}b 198.51.100.2/24 up
	route add -net 192.0.2.0/24 198.51.100.1

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 192.0.2.1

	jexec gw dnctl pipe 1 config delay 1200
	pft_set_rules gw \
		"pass out route-to (${epair_srv}b 192.0.2.1) to 192.0.2.1 dnpipe 1"
	jexec gw pfctl -e

	# The ping request will pass, but take 1.2 seconds
	# So this works:
	atf_check -s exit:0 -o ignore ping -c 1 -t 2 192.0.2.1
	# But this times out:
	atf_check -s exit:2 -o ignore ping -c 1 -t 1 192.0.2.1

	# return path dummynet
	pft_set_rules gw \
		"pass out route-to (${epair_srv}b 192.0.2.1) to 192.0.2.1 dnpipe (0, 1)"

	# The ping request will pass, but take 1.2 seconds
	# So this works:
	atf_check -s exit:0 -o ignore ping -c 1 -t 2 192.0.2.1
	# But this times out:
	atf_check -s exit:2 -o ignore ping -c 1 -t 1 192.0.2.1
}

dummynet_cleanup()
{
	pft_cleanup
}

atf_test_case "dummynet_in" "cleanup"
dummynet_in_head()
{
	atf_set descr 'Thest that dummynet works as expected on pass in route-to packets'
	atf_set require.user root
}

dummynet_in_body()
{
	dummynet_init

	epair_srv=$(vnet_mkepair)
	epair_gw=$(vnet_mkepair)

	vnet_mkjail srv ${epair_srv}a
	jexec srv ifconfig ${epair_srv}a 192.0.2.1/24 up
	jexec srv route add default 192.0.2.2

	vnet_mkjail gw ${epair_srv}b ${epair_gw}a
	jexec gw ifconfig ${epair_srv}b 192.0.2.2/24 up
	jexec gw ifconfig ${epair_gw}a 198.51.100.1/24 up
	jexec gw sysctl net.inet.ip.forwarding=1

	ifconfig ${epair_gw}b 198.51.100.2/24 up
	route add -net 192.0.2.0/24 198.51.100.1

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 192.0.2.1

	jexec gw dnctl pipe 1 config delay 1200
	pft_set_rules gw \
		"pass in route-to (${epair_srv}b 192.0.2.1) to 192.0.2.1 dnpipe 1"
	jexec gw pfctl -e

	# The ping request will pass, but take 1.2 seconds
	# So this works:
	echo "Expect 1.2 s"
	ping -c 1 192.0.2.1
	atf_check -s exit:0 -o ignore ping -c 1 -t 2 192.0.2.1
	# But this times out:
	atf_check -s exit:2 -o ignore ping -c 1 -t 1 192.0.2.1

	# return path dummynet
	pft_set_rules gw \
		"pass in route-to (${epair_srv}b 192.0.2.1) to 192.0.2.1 dnpipe (0, 1)"

	# The ping request will pass, but take 1.2 seconds
	# So this works:
	echo "Expect 1.2 s"
	ping -c 1 192.0.2.1
	atf_check -s exit:0 -o ignore ping -c 1 -t 2 192.0.2.1
	# But this times out:
	atf_check -s exit:2 -o ignore ping -c 1 -t 1 192.0.2.1
}

dummynet_in_cleanup()
{
	pft_cleanup
}

atf_test_case "ifbound" "cleanup"
ifbound_head()
{
	atf_set descr 'Test that route-to states bind the expected interface'
	atf_set require.user root
}

ifbound_body()
{
	pft_init

	j="route_to:ifbound"

	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)
	ifconfig ${epair_one}b up

	vnet_mkjail ${j}2 ${epair_two}b
	jexec ${j}2 ifconfig ${epair_two}b inet 198.51.100.2/24 up
	jexec ${j}2 ifconfig ${epair_two}b inet alias 203.0.113.1/24
	jexec ${j}2 route add default 198.51.100.1

	vnet_mkjail $j ${epair_one}a ${epair_two}a
	jexec $j ifconfig ${epair_one}a 192.0.2.1/24 up
	jexec $j ifconfig ${epair_two}a 198.51.100.1/24 up
	jexec $j route add default 192.0.2.2

	jexec $j pfctl -e
	pft_set_rules $j \
		"set state-policy if-bound" \
		"block" \
		"pass out route-to (${epair_two}a 198.51.100.2)"

	atf_check -s exit:0 -o ignore \
	    jexec $j ping -c 3 203.0.113.1
}

ifbound_cleanup()
{
	pft_cleanup
}

atf_test_case "ifbound_v6" "cleanup"
ifbound_v6_head()
{
	atf_set descr 'Test that route-to states for IPv6 bind the expected interface'
	atf_set require.user root
}

ifbound_v6_body()
{
	pft_init

	j="route_to:ifbound_v6"

	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)
	ifconfig ${epair_one}b up

	vnet_mkjail ${j}2 ${epair_two}b
	jexec ${j}2 ifconfig ${epair_two}b inet6 2001:db8:1::2/64 up no_dad
	jexec ${j}2 ifconfig ${epair_two}b inet6 alias 2001:db8:2::1/64 no_dad
	jexec ${j}2 route -6 add default 2001:db8:1::1

	vnet_mkjail $j ${epair_one}a ${epair_two}a
	jexec $j ifconfig ${epair_one}a inet6 2001:db8::1/64 up no_dad
	jexec $j ifconfig ${epair_two}a inet6 2001:db8:1::1/64 up no_dad
	jexec $j route -6 add default 2001:db8::2

	jexec $j ping6 -c 3 2001:db8:1::2

	jexec $j pfctl -e
	pft_set_rules $j \
		"set state-policy if-bound" \
		"block" \
		"pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
		"pass out route-to (${epair_two}a 2001:db8:1::2)"

	atf_check -s exit:0 -o ignore \
	    jexec $j ping6 -c 3 2001:db8:2::1
}

ifbound_v6_cleanup()
{
	pft_cleanup
}

atf_test_case "ifbound_reply_to" "cleanup"
ifbound_reply_to_head()
{
	atf_set descr 'Test that reply-to states bind to the expected interface'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

ifbound_reply_to_body()
{
	pft_init

	j="route_to:ifbound_reply_to"

	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)
	ifconfig ${epair_one}b inet 192.0.2.2/24 up
	ifconfig ${epair_two}b up

	vnet_mkjail $j ${epair_one}a ${epair_two}a
	jexec $j ifconfig ${epair_one}a 192.0.2.1/24 up
	jexec $j ifconfig ${epair_two}a 198.51.100.1/24 up
	jexec $j route add default 198.51.100.254

	jexec $j pfctl -e
	pft_set_rules $j \
		"set state-policy if-bound" \
		"block" \
		"pass in on ${epair_one}a reply-to (${epair_one}a 192.0.2.2) inet from any to 192.0.2.0/24 keep state"

	atf_check -s exit:0 -o ignore \
	    ping -c 3 192.0.2.1

	atf_check -s exit:0 \
	    ${common_dir}/pft_ping.py \
	    --to 192.0.2.1 \
	    --from 203.0.113.2 \
	    --sendif ${epair_one}b \
	    --replyif ${epair_one}b

	# pft_ping uses the same ID every time, so this will look like more traffic in the same state
	atf_check -s exit:0 \
	    ${common_dir}/pft_ping.py \
	    --to 192.0.2.1 \
	    --from 203.0.113.2 \
	    --sendif ${epair_one}b \
	    --replyif ${epair_one}b

	jexec $j pfctl -ss -vv
}

ifbound_reply_to_cleanup()
{
	pft_cleanup
}

atf_test_case "ifbound_reply_to_v6" "cleanup"
ifbound_reply_to_v6_head()
{
	atf_set descr 'Test that reply-to states bind to the expected interface for IPv6'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

ifbound_reply_to_v6_body()
{
	pft_init

	j="route_to:ifbound_reply_to_v6"

	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)

	vnet_mkjail ${j}s ${epair_one}b ${epair_two}b
	jexec ${j}s ifconfig ${epair_one}b inet6 2001:db8::2/64 up no_dad
	jexec ${j}s ifconfig ${epair_two}b up
	#jexec ${j}s route -6 add default 2001:db8::1

	vnet_mkjail $j ${epair_one}a ${epair_two}a
	jexec $j ifconfig ${epair_one}a inet6 2001:db8::1/64 up no_dad
	jexec $j ifconfig ${epair_two}a inet6 2001:db8:1::1/64 up no_dad
	jexec $j route -6 add default 2001:db8:1::254

	jexec $j pfctl -e
	pft_set_rules $j \
		"set state-policy if-bound" \
		"block" \
		"pass quick inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
		"pass in on ${epair_one}a reply-to (${epair_one}a 2001:db8::2) inet6 from any to 2001:db8::/64 keep state"

	atf_check -s exit:0 -o ignore \
	    jexec ${j}s ping6 -c 3 2001:db8::1

	atf_check -s exit:0 \
	    jexec ${j}s ${common_dir}/pft_ping.py \
	    --to 2001:db8::1 \
	    --from 2001:db8:2::2 \
	    --sendif ${epair_one}b \
	    --replyif ${epair_one}b

	# pft_ping uses the same ID every time, so this will look like more traffic in the same state
	atf_check -s exit:0 \
	    jexec ${j}s ${common_dir}/pft_ping.py \
	    --to 2001:db8::1 \
	    --from 2001:db8:2::2 \
	    --sendif ${epair_one}b \
	    --replyif ${epair_one}b

	jexec $j pfctl -ss -vv
}

ifbound_reply_to_v6_cleanup()
{
	pft_cleanup
}

atf_test_case "ifbound_reply_to_rdr_dummynet" "cleanup"
ifbound_reply_to_rdr_dummynet_head()
{
	atf_set descr 'Test that reply-to states bind to the expected non-default-route interface after rdr and dummynet'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

ifbound_reply_to_rdr_dummynet_body()
{
	dummynet_init

	j="route_to:ifbound_reply_to_rdr_dummynet"

	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)
	ifconfig ${epair_one}b inet 192.0.2.2/24 up
	ifconfig ${epair_two}b up

	vnet_mkjail $j ${epair_one}a ${epair_two}a
	jexec $j ifconfig lo0 inet 127.0.0.1/8 up
	jexec $j ifconfig ${epair_one}a 192.0.2.1/24 up
	jexec $j ifconfig ${epair_two}a 198.51.100.1/24 up
	jexec $j route add default 198.51.100.254

	jexec $j pfctl -e
	jexec $j dnctl pipe 1 config delay 1
	pft_set_rules $j \
		"set state-policy if-bound" \
		"rdr on ${epair_one}a proto icmp from any to 192.0.2.1 -> 127.0.0.1" \
		"rdr on ${epair_two}a proto icmp from any to 198.51.100.1 -> 127.0.0.1" \
		"match in on ${epair_one}a inet all dnpipe (1, 1)" \
		"pass in on ${epair_one}a reply-to (${epair_one}a 192.0.2.2) inet from any to 127.0.0.1 keep state"

	atf_check -s exit:0 -o ignore \
	    ping -c 3 192.0.2.1

	atf_check -s exit:0 \
	    ${common_dir}/pft_ping.py \
	    --to 192.0.2.1 \
	    --from 203.0.113.2 \
	    --sendif ${epair_one}b \
	    --replyif ${epair_one}b

	# pft_ping uses the same ID every time, so this will look like more traffic in the same state
	atf_check -s exit:0 \
	    ${common_dir}/pft_ping.py \
	    --to 192.0.2.1 \
	    --from 203.0.113.2 \
	    --sendif ${epair_one}b \
	    --replyif ${epair_one}b

	jexec $j pfctl -sr -vv
	jexec $j pfctl -ss -vv
}

ifbound_reply_to_rdr_dummynet_cleanup()
{
	pft_cleanup
}

atf_test_case "dummynet_frag" "cleanup"
dummynet_frag_head()
{
	atf_set descr 'Test fragmentation with route-to and dummynet'
	atf_set require.user root
}

dummynet_frag_body()
{
	pft_init
	dummynet_init

	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)

	ifconfig ${epair_one}a 192.0.2.1/24 up

	vnet_mkjail alcatraz ${epair_one}b ${epair_two}a
	jexec alcatraz ifconfig ${epair_one}b 192.0.2.2/24 up
	jexec alcatraz ifconfig ${epair_two}a 198.51.100.1/24 up
	jexec alcatraz sysctl net.inet.ip.forwarding=1

	vnet_mkjail singsing ${epair_two}b
	jexec singsing ifconfig ${epair_two}b 198.51.100.2/24 up
	jexec singsing route add default 198.51.100.1

	route add 198.51.100.0/24 192.0.2.2

	jexec alcatraz dnctl pipe 1 config bw 1000Byte/s burst 4500
	jexec alcatraz dnctl pipe 2 config
	# This second pipe ensures that the pf_test(PF_OUT) call in pf_route() doesn't
	# delay packets in dummynet (by inheriting pipe 1 from the input rule).

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"set reassemble yes" \
		"pass in route-to (${epair_two}a 198.51.100.2) inet proto icmp all icmp-type echoreq dnpipe 1" \
		"pass out dnpipe 2"


	atf_check -s exit:0 -o ignore ping -c 1 198.51.100.2
	atf_check -s exit:0 -o ignore ping -c 1 -s 4000 198.51.100.2
}

dummynet_frag_cleanup()
{
	pft_cleanup
}

atf_test_case "dummynet_double" "cleanup"
dummynet_double_head()
{
	atf_set descr 'Ensure dummynet is not applied multiple times'
	atf_set require.user root
}

dummynet_double_body()
{
	pft_init
	dummynet_init

	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)

	ifconfig ${epair_one}a 192.0.2.1/24 up

	vnet_mkjail alcatraz ${epair_one}b ${epair_two}a
	jexec alcatraz ifconfig ${epair_one}b 192.0.2.2/24 up
	jexec alcatraz ifconfig ${epair_two}a 198.51.100.1/24 up
	jexec alcatraz sysctl net.inet.ip.forwarding=1

	vnet_mkjail singsing ${epair_two}b
	jexec singsing ifconfig ${epair_two}b 198.51.100.2/24 up
	jexec singsing route add default 198.51.100.1

	route add 198.51.100.0/24 192.0.2.2

	jexec alcatraz dnctl pipe 1 config delay 800

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"set reassemble yes" \
		"nat on ${epair_two}a from 192.0.2.0/24 -> (${epair_two}a)" \
		"pass in route-to (${epair_two}a 198.51.100.2) inet proto icmp all icmp-type echoreq dnpipe (1, 1)" \
		"pass out route-to (${epair_two}a 198.51.100.2) inet proto icmp all icmp-type echoreq"

	ping -c 1 198.51.100.2
	jexec alcatraz pfctl -sr -vv
	jexec alcatraz pfctl -ss -vv

	# We expect to be delayed 1.6 seconds, so timeout of two seconds passes, but
	# timeout of 1 does not.
	atf_check -s exit:0 -o ignore ping -t 2 -c 1 198.51.100.2
	atf_check -s exit:2 -o ignore ping -t 1 -c 1 198.51.100.2
}

dummynet_double_cleanup()
{
	pft_cleanup
}

atf_test_case "sticky" "cleanup"
sticky_head()
{
	atf_set descr 'Set and retrieve a rule with sticky-address'
	atf_set require.user root
}

sticky_body()
{
	pft_init

	vnet_mkjail alcatraz

	pft_set_rules alcatraz \
	    "pass in quick log on n_test_h_rtr route-to (n_srv_h_rtr <change_dst>) sticky-address from any to <dst> keep state"

	jexec alcatraz pfctl -qvvsr
}

sticky_cleanup()
{
	pft_cleanup
}

atf_test_case "ttl" "cleanup"
ttl_head()
{
	atf_set descr 'Ensure we decrement TTL on route-to'
	atf_set require.user root
}

ttl_body()
{
	pft_init

	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)
	ifconfig ${epair_one}b 192.0.2.2/24 up
	route add default 192.0.2.1

	vnet_mkjail alcatraz ${epair_one}a ${epair_two}a
	jexec alcatraz ifconfig ${epair_one}a 192.0.2.1/24 up
	jexec alcatraz ifconfig ${epair_two}a 198.51.100.1/24 up
	jexec alcatraz sysctl net.inet.ip.forwarding=1

	vnet_mkjail singsing ${epair_two}b
	jexec singsing ifconfig ${epair_two}b 198.51.100.2/24 up
	jexec singsing route add default 198.51.100.1

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    ping -c 3 198.51.100.2

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"pass out" \
		"pass in route-to (${epair_two}a 198.51.100.2)"

	atf_check -s exit:0 -o ignore \
	    ping -c 3 198.51.100.2

	atf_check -s exit:2 -o ignore \
	    ping -m 1 -c 3 198.51.100.2
}

ttl_cleanup()
{
	pft_cleanup
}


atf_test_case "empty_pool" "cleanup"
empty_pool_head()
{
	atf_set descr 'Route-to with empty pool'
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
		"pass in  on ${epair_tester}b route-to (${epair_server}a <nonexistent>) inet6 from any to ${net_server_host_server}" \
		"pass out on ${epair_server}a"

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

atf_test_case "table_loop" "cleanup"

table_loop_head()
{
	atf_set descr 'Check that iterating over tables poperly loops'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

table_loop_body()
{
	pf_map_addr_common

	jexec router pfctl -e
	pft_set_rules router \
		"set debug loud" \
		"set reassemble yes" \
		"set state-policy if-bound" \
		"table <rt_targets_1> { ${net_server1_6}::42:4/127 ${net_server1_6}::42:0/127 }" \
		"table <rt_targets_2> { ${net_server2_6}::42:4/127 }" \
		"pass in on ${epair_tester}b \
			route-to { \
			(${epair_server1}a <rt_targets_1>) \
			(${epair_server2}a <rt_targets_2_empty>) \
			(${epair_server2}a <rt_targets_2>) \
			} \
		inet6 proto tcp \
		keep state"

	# Both hosts of the pool are tables. Each table gets iterated over once,
	# then the pool iterates to the next host, which is also iterated,
	# then the pool loops back to the 1st host. If an empty table is found,
	# it is skipped. Unless that's the only table, that is tested by
	# the "empty_pool" test.
	for port in $(seq 1 7); do
	port=$((4200 + port))
		atf_check -s exit:0 ${common_dir}/pft_ping.py \
			--sendif ${epair_tester}a --replyif ${epair_tester}a \
			--fromaddr ${net_clients_6}::1 --to ${host_server_6} \
			--ping-type=tcp3way --send-sport=${port}
	done

	states=$(mktemp) || exit 1
	jexec router pfctl -qvvss | normalize_pfctl_s > $states
	cat $states

	for state_regexp in \
		"${epair_tester}b tcp ${host_server_6}\[9\] <- ${net_clients_6}::1\[4201\] .* route-to: ${net_server1_6}::42:0@${epair_server1}a" \
		"${epair_tester}b tcp ${host_server_6}\[9\] <- ${net_clients_6}::1\[4202\] .* route-to: ${net_server1_6}::42:1@${epair_server1}a" \
		"${epair_tester}b tcp ${host_server_6}\[9\] <- ${net_clients_6}::1\[4203\] .* route-to: ${net_server1_6}::42:4@${epair_server1}a" \
		"${epair_tester}b tcp ${host_server_6}\[9\] <- ${net_clients_6}::1\[4204\] .* route-to: ${net_server1_6}::42:5@${epair_server1}a" \
		"${epair_tester}b tcp ${host_server_6}\[9\] <- ${net_clients_6}::1\[4205\] .* route-to: ${net_server2_6}::42:4@${epair_server2}a" \
		"${epair_tester}b tcp ${host_server_6}\[9\] <- ${net_clients_6}::1\[4206\] .* route-to: ${net_server2_6}::42:5@${epair_server2}a" \
		"${epair_tester}b tcp ${host_server_6}\[9\] <- ${net_clients_6}::1\[4207\] .* route-to: ${net_server1_6}::42:0@${epair_server1}a" \
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done
}

table_loop_cleanup()
{
	pft_cleanup
}


atf_test_case "roundrobin" "cleanup"

roundrobin_head()
{
	atf_set descr 'multiple gateways of mixed AF, including prefixes and tables, for IPv6 packets'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

roundrobin_body()
{
	pf_map_addr_common

	# The rule is defined as "inet6 proto tcp" so directly given IPv4 hosts
	# will be removed from the pool by pfctl. Tables will still be loaded
	# and pf_map_addr() will only use IPv6 addresses from them. It will
	# iterate over members of the pool and inside of tables and prefixes.

	jexec router pfctl -e
	pft_set_rules router \
		"set debug loud" \
		"set reassemble yes" \
		"set state-policy if-bound" \
		"table <rt_targets> { ${net_server2_4}.40/31 ${net_server2_4}.44 ${net_server2_6}::42:0/127 ${net_server2_6}::42:4 }" \
		"pass in on ${epair_tester}b \
			route-to { \
				(${epair_server1}a ${net_server1_4_host_server}) \
				(${epair_server2}a <rt_targets_empty>) \
				(${epair_server1}a ${net_server1_6}::42:0/127) \
				(${epair_server2}a <rt_targets_empty>) \
				(${epair_server2}a <rt_targets>) \
			} \
			inet6 proto tcp \
			keep state"

	for port in $(seq 1 6); do
		port=$((4200 + port))
		atf_check -s exit:0 ${common_dir}/pft_ping.py \
			--sendif ${epair_tester}a --replyif ${epair_tester}a \
			--fromaddr ${net_clients_6}::1 --to ${host_server_6} \
			--ping-type=tcp3way --send-sport=${port}
	done

	states=$(mktemp) || exit 1
	jexec router pfctl -qvvss | normalize_pfctl_s > $states

	for state_regexp in \
		"${epair_tester}b tcp ${host_server_6}\[9\] <- ${net_clients_6}::1\[4201\] .* route-to: ${net_server1_6}::42:0@${epair_server1}a" \
		"${epair_tester}b tcp ${host_server_6}\[9\] <- ${net_clients_6}::1\[4202\] .* route-to: ${net_server1_6}::42:1@${epair_server1}a" \
		"${epair_tester}b tcp ${host_server_6}\[9\] <- ${net_clients_6}::1\[4203\] .* route-to: ${net_server2_6}::42:0@${epair_server2}a" \
		"${epair_tester}b tcp ${host_server_6}\[9\] <- ${net_clients_6}::1\[4204\] .* route-to: ${net_server2_6}::42:1@${epair_server2}a" \
		"${epair_tester}b tcp ${host_server_6}\[9\] <- ${net_clients_6}::1\[4205\] .* route-to: ${net_server2_6}::42:4@${epair_server2}a" \
		"${epair_tester}b tcp ${host_server_6}\[9\] <- ${net_clients_6}::1\[4206\] .* route-to: ${net_server1_6}::42:0@${epair_server1}a" \
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done
}

roundrobin_cleanup()
{
	pft_cleanup
}

atf_test_case "random_table" "cleanup"

random_table_head()
{
	atf_set descr 'Pool with random flag and a table for IPv6'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

random_table_body()
{
	pf_map_addr_common

	# The "random" flag will pick random hosts from the table but will
	# not dive into prefixes, always choosing the 0th address.
	# Proper address family will be choosen.

	jexec router pfctl -e
	pft_set_rules router \
		"set debug loud" \
		"set reassemble yes" \
		"set state-policy if-bound" \
		"table <rt_targets> { ${net_server2_4}.40/31 ${net_server2_4}.44 ${net_server2_6}::42:0/127 ${net_server2_6}::42:4 }" \
		"pass in on ${epair_tester}b \
			route-to { (${epair_server2}a <rt_targets>) } random \
			inet6 proto tcp \
			keep state"

	good_targets="${net_server2_6}::42:0 ${net_server2_6}::42:4"
	bad_targets="${net_server2_6}::42:1"
	check_random IPv6 "${good_targets}" "${bad_targets}"
}

random_table_cleanup()
{
	pft_cleanup
}

atf_test_case "random_prefix" "cleanup"

random_prefix_head()
{
	atf_set descr 'Pool with random flag and a table for IPv4'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

random_prefix_body()
{
	pf_map_addr_common

	# The "random" flag will pick random hosts from given prefix.
	# The choice being random makes testing it non-trivial. We do 10
	# attempts to have each target chosen. Hopefully this is enough to have
	# this test pass often enough.

	jexec router pfctl -e
	pft_set_rules router \
		"set debug loud" \
		"set reassemble yes" \
		"set state-policy if-bound" \
		"pass in on ${epair_tester}b \
			route-to { (${epair_server2}a ${net_server2_6}::42:0/127) } random \
			inet6 proto tcp \
			keep state"

	good_targets="${net_server2_6}::42:0 ${net_server2_6}::42:1"
	check_random IPv6 "${good_targets}"
}

random_prefix_cleanup()
{
	pft_cleanup
}

atf_test_case "prefer_ipv6_nexthop_single_ipv4" "cleanup"

prefer_ipv6_nexthop_single_ipv4_head()
{
	atf_set descr 'prefer-ipv6-nexthop option for a single IPv4 gateway'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

prefer_ipv6_nexthop_single_ipv4_body()
{
	pf_map_addr_common

	# Basic forwarding test for prefer-ipv6-nexthop pool option.
	# A single IPv4 gateway will work only for IPv4 traffic.

	jexec router pfctl -e
	pft_set_rules router \
		"set reassemble yes" \
		"set state-policy if-bound" \
		"pass in on ${epair_tester}b \
			route-to (${epair_server1}a ${net_server1_4_host_server}) prefer-ipv6-nexthop \
			proto tcp \
			keep state"

	atf_check -s exit:0 ${common_dir}/pft_ping.py \
		--sendif ${epair_tester}a --replyif ${epair_tester}a \
		--fromaddr ${net_clients_4}.1 --to ${host_server_4} \
		--ping-type=tcp3way --send-sport=4201 \

	atf_check -s exit:1 ${common_dir}/pft_ping.py \
		--sendif ${epair_tester}a --replyif ${epair_tester}a \
		--fromaddr ${net_clients_6}::1 --to ${host_server_6} \
		--ping-type=tcp3way --send-sport=4202

	states=$(mktemp) || exit 1
	jexec router pfctl -qvvss | normalize_pfctl_s > $states

	for state_regexp in \
		"${epair_tester}b tcp ${host_server_4}:9 <- ${net_clients_4}.1:4201 .* route-to: ${net_server1_4_host_server}@${epair_server1}a" \
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done

	# The IPv6 packet could not have been routed over IPv4 gateway.
	atf_check -o "match:map-failed +1 +" -x "jexec router pfctl -qvvsi 2> /dev/null"
}

prefer_ipv6_nexthop_single_ipv4_cleanup()
{
	pft_cleanup
}

atf_test_case "prefer_ipv6_nexthop_single_ipv6" "cleanup"

prefer_ipv6_nexthop_single_ipv6_head()
{
	atf_set descr 'prefer-ipv6-nexthop option for a single IPv6 gateway'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

prefer_ipv6_nexthop_single_ipv6_body()
{
	pf_map_addr_common

	# Basic forwarding test for prefer-ipv6-nexthop pool option.
	# A single IPve gateway will work both for IPv4 and IPv6 traffic.

	jexec router pfctl -e
	pft_set_rules router \
		"set reassemble yes" \
		"set state-policy if-bound" \
		"pass in on ${epair_tester}b \
			route-to (${epair_server1}a ${net_server1_6_host_server}) prefer-ipv6-nexthop \
			proto tcp \
			keep state"

	atf_check -s exit:0 ${common_dir}/pft_ping.py \
		--sendif ${epair_tester}a --replyif ${epair_tester}a \
		--fromaddr ${net_clients_4}.1 --to ${host_server_4} \
		--ping-type=tcp3way --send-sport=4201 \

	atf_check -s exit:0 ${common_dir}/pft_ping.py \
		--sendif ${epair_tester}a --replyif ${epair_tester}a \
		--fromaddr ${net_clients_6}::1 --to ${host_server_6} \
		--ping-type=tcp3way --send-sport=4202

	states=$(mktemp) || exit 1
	jexec router pfctl -qvvss | normalize_pfctl_s > $states

	for state_regexp in \
		"${epair_tester}b tcp ${host_server_4}:9 <- ${net_clients_4}.1:4201 .* route-to: ${net_server1_6_host_server}@${epair_server1}a" \
		"${epair_tester}b tcp ${host_server_6}\[9\] <- ${net_clients_6}::1\[4202\] .* route-to: ${net_server1_6_host_server}@${epair_server1}a" \
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done
}

prefer_ipv6_nexthop_single_ipv6_cleanup()
{
	pft_cleanup
}

atf_test_case "prefer_ipv6_nexthop_mixed_af_roundrobin_ipv4" "cleanup"

prefer_ipv6_nexthop_mixed_af_roundrobin_ipv4_head()
{
	atf_set descr 'prefer-ipv6-nexthop option for multiple gateways of mixed AF with prefixes and tables, round robin selection, for IPv4 packets'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

prefer_ipv6_nexthop_mixed_af_roundrobin_ipv4_body()
{
	pf_map_addr_common

	# pf_map_addr() starts iterating over hosts of the pool from the 2nd
	# host. This behaviour was here before adding prefer-ipv6-nexthop
	# support, we test for it. Some targets are hosts and some are tables.
	# Those are iterated too. Finally the iteration loops back
	# to the beginning of the pool. Send out IPv4 probes.  They will use all
	# IPv4 and IPv6 addresses from the pool.

	jexec router pfctl -e
	pft_set_rules router \
		"set reassemble yes" \
		"set state-policy if-bound" \
		"table <rt_targets> { ${net_server2_4}.40/31 ${net_server2_4}.44 ${net_server2_6}::42:4/127 }" \
		"pass in on ${epair_tester}b \
			route-to { \
				(${epair_server1}a ${net_server1_6_host_server}) \
				(${epair_server1}a ${net_server1_6}::42:0/127) \
				(${epair_server2}a ${net_server2_4_host_server}) \
				(${epair_server2}a <rt_targets_empty>) \
				(${epair_server1}a ${net_server1_4}.24/31) \
				(${epair_server2}a <rt_targets>) \
			} prefer-ipv6-nexthop \
			proto tcp \
			keep state"

	for port in $(seq 1 12); do
		port=$((4200 + port))
		atf_check -s exit:0 ${common_dir}/pft_ping.py \
			--sendif ${epair_tester}a --replyif ${epair_tester}a \
			--fromaddr ${net_clients_4}.1 --to ${host_server_4} \
			--ping-type=tcp3way --send-sport=${port}
	done

	states=$(mktemp) || exit 1
	jexec router pfctl -qvvss | normalize_pfctl_s > $states

	for state_regexp in \
		"${epair_tester}b tcp ${host_server_4}:9 <- ${net_clients_4}.1:4201 .* route-to: ${net_server1_6}::42:0@${epair_server1}a" \
		"${epair_tester}b tcp ${host_server_4}:9 <- ${net_clients_4}.1:4202 .* route-to: ${net_server1_6}::42:1@${epair_server1}a" \
		"${epair_tester}b tcp ${host_server_4}:9 <- ${net_clients_4}.1:4203 .* route-to: ${net_server2_4_host_server}@${epair_server2}a" \
		"${epair_tester}b tcp ${host_server_4}:9 <- ${net_clients_4}.1:4204 .* route-to: ${net_server1_4}.24@${epair_server1}a" \
		"${epair_tester}b tcp ${host_server_4}:9 <- ${net_clients_4}.1:4205 .* route-to: ${net_server1_4}.25@${epair_server1}a" \
		"${epair_tester}b tcp ${host_server_4}:9 <- ${net_clients_4}.1:4206 .* route-to: ${net_server2_6}::42:4@${epair_server2}a" \
		"${epair_tester}b tcp ${host_server_4}:9 <- ${net_clients_4}.1:4207 .* route-to: ${net_server2_6}::42:5@${epair_server2}a" \
		"${epair_tester}b tcp ${host_server_4}:9 <- ${net_clients_4}.1:4208 .* route-to: ${net_server2_4}.40@${epair_server2}a" \
		"${epair_tester}b tcp ${host_server_4}:9 <- ${net_clients_4}.1:4209 .* route-to: ${net_server2_4}.41@${epair_server2}a" \
		"${epair_tester}b tcp ${host_server_4}:9 <- ${net_clients_4}.1:4210 .* route-to: ${net_server2_4}.44@${epair_server2}a" \
		"${epair_tester}b tcp ${host_server_4}:9 <- ${net_clients_4}.1:4211 .* route-to: ${net_server1_6_host_server}@${epair_server1}a" \
		"${epair_tester}b tcp ${host_server_4}:9 <- ${net_clients_4}.1:4212 .* route-to: ${net_server1_6}::42:0@${epair_server1}a" \
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done
}

prefer_ipv6_nexthop_mixed_af_roundrobin_ipv4_cleanup()
{
	pft_cleanup
}

prefer_ipv6_nexthop_mixed_af_roundrobin_ipv6_head()
{
	atf_set descr 'prefer-ipv6-nexthop option for multiple gateways of mixed AF with prefixes and tables, round-robin selection, for IPv6 packets'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

prefer_ipv6_nexthop_mixed_af_roundrobin_ipv6_body()
{
	pf_map_addr_common

	# The "random" flag will pick random hosts from the table but will
	# not dive into prefixes, always choosing the 0th address.
	# Proper address family will be choosen. The choice being random makes
	# testing it non-trivial. We do 10 attempts to have each target chosen.
	# Hopefully this is enough to have this test pass often enough.

	# pf_map_addr() starts iterating over hosts of the pool from the 2nd
	# host. This behaviour was here before adding prefer-ipv6-nexthop
	# support, we test for it. Some targets are hosts and some are tables.
	# Those are iterated too. Finally the iteration loops back
	# to the beginning of the pool. Send out IPv6 probes. They will use only
	#  IPv6 addresses from the pool.

	jexec router pfctl -e
	pft_set_rules router \
		"set reassemble yes" \
		"set state-policy if-bound" \
		"table <rt_targets> { ${net_server2_4}.40/31 ${net_server2_4}.44 ${net_server2_6}::42:4/127 }" \
		"pass in on ${epair_tester}b \
			route-to { \
				(${epair_server1}a ${net_server1_6_host_server}) \
				(${epair_server1}a ${net_server1_6}::42:0/127) \
				(${epair_server2}a ${net_server2_4_host_server}) \
				(${epair_server2}a <rt_targets_empty>) \
				(${epair_server1}a ${net_server1_4}.24/31) \
				(${epair_server2}a <rt_targets>) \
			} prefer-ipv6-nexthop \
			proto tcp \
			keep state"

	for port in $(seq 1 6); do
		port=$((4200 + port))
		atf_check -s exit:0 ${common_dir}/pft_ping.py \
			--sendif ${epair_tester}a --replyif ${epair_tester}a \
			--fromaddr ${net_clients_6}::1 --to ${host_server_6} \
			--ping-type=tcp3way --send-sport=${port}
	done

	states=$(mktemp) || exit 1
	jexec router pfctl -qvvss | normalize_pfctl_s > $states

	for state_regexp in \
		"${epair_tester}b tcp ${host_server_6}\[9\] <- ${net_clients_6}::1\[4201\] .* route-to: ${net_server1_6}::42:0@${epair_server1}a" \
		"${epair_tester}b tcp ${host_server_6}\[9\] <- ${net_clients_6}::1\[4202\] .* route-to: ${net_server1_6}::42:1@${epair_server1}a" \
		"${epair_tester}b tcp ${host_server_6}\[9\] <- ${net_clients_6}::1\[4203\] .* route-to: ${net_server2_6}::42:4@${epair_server2}a" \
		"${epair_tester}b tcp ${host_server_6}\[9\] <- ${net_clients_6}::1\[4204\] .* route-to: ${net_server2_6}::42:5@${epair_server2}a" \
		"${epair_tester}b tcp ${host_server_6}\[9\] <- ${net_clients_6}::1\[4205\] .* route-to: ${net_server1_6_host_server}@${epair_server1}a" \
		"${epair_tester}b tcp ${host_server_6}\[9\] <- ${net_clients_6}::1\[4206\] .* route-to: ${net_server1_6}::42:0@${epair_server1}a" \
	; do
		grep -qE "${state_regexp}" $states || atf_fail "State not found for '${state_regexp}'"
	done
}

prefer_ipv6_nexthop_mixed_af_roundrobin_ipv6_cleanup()
{
	pft_cleanup
}

atf_test_case "prefer_ipv6_nexthop_mixed_af_random_ipv4" "cleanup"

prefer_ipv6_nexthop_mixed_af_random_table_ipv4_head()
{
	atf_set descr 'prefer-ipv6-nexthop option for a mixed-af table with random selection for IPv4 packets'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

prefer_ipv6_nexthop_mixed_af_random_table_ipv4_body()
{
	pf_map_addr_common

	# Similarly to the test "random_table" the algorithm will choose
	# IP addresses from the table not diving into prefixes.
	# *prefer*-ipv6-nexthop means that IPv6 nexthops are preferred,
	# so IPv4 ones will not be chosen as long as there are IPv6 ones
	# available. With this tested there is no need for a test for IPv6-only
	# next-hops table.

	jexec router pfctl -e
	pft_set_rules router \
		"set reassemble yes" \
		"set state-policy if-bound" \
		"table <rt_targets> { ${net_server2_4}.40/31 ${net_server2_4}.44 ${net_server2_6}::42:0/127 ${net_server2_6}::42:4 }" \
		"pass in on ${epair_tester}b \
			route-to { (${epair_server2}a <rt_targets>) } random prefer-ipv6-nexthop \
			proto tcp \
			keep state"

	good_targets="${net_server2_6}::42:0 ${net_server2_6}::42:4"
	bad_targets="${net_server2_4}.40 ${net_server2_4}.41 ${net_server2_4}.44 ${net_server2_6}::42:1"
	check_random IPv4 "${good_targets}" "${bad_targets}"
}

prefer_ipv6_nexthop_mixed_af_random_table_ipv4_cleanup()
{
	pft_cleanup
}

prefer_ipv6_nexthop_ipv4_random_table_ipv4_head()
{
	atf_set descr 'prefer-ipv6-nexthop option for an IPv4-only table with random selection for IPv4 packets'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

prefer_ipv6_nexthop_ipv4_random_table_ipv4_body()
{
	pf_map_addr_common

	# Similarly to the test pf_map_addr:random_table the algorithm will
	# choose IP addresses from the table not diving into prefixes.
	# There are no IPv6 nexthops in the table, so the algorithm will
	# fall back to IPv4.

	jexec router pfctl -e
	pft_set_rules router \
		"set reassemble yes" \
		"set state-policy if-bound" \
		"table <rt_targets> { ${net_server2_4}.40/31 ${net_server2_4}.44 }" \
		"pass in on ${epair_tester}b \
			route-to { (${epair_server2}a <rt_targets>) } random prefer-ipv6-nexthop \
			proto tcp \
			keep state"

	good_targets="${net_server2_4}.40 ${net_server2_4}.44"
	bad_targets="${net_server2_4}.41"
	check_random IPv4 "${good_targets}" "${bad_targets}"
}

prefer_ipv6_nexthop_ipv4_random_table_ipv4_cleanup()
{
	pft_cleanup
}

prefer_ipv6_nexthop_ipv4_random_table_ipv6_head()
{
	atf_set descr 'prefer-ipv6-nexthop option for an IPv4-only table with random selection for IPv6 packets'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

prefer_ipv6_nexthop_ipv4_random_table_ipv6_body()
{
	pf_map_addr_common

	# IPv6 packets can't be forwarded over IPv4 next-hops.
	# The failure happens in pf_map_addr() and increases the respective
	# error counter.

	jexec router pfctl -e
	pft_set_rules router \
		"set reassemble yes" \
		"set state-policy if-bound" \
		"table <rt_targets> { ${net_server2_4}.40/31 ${net_server2_4}.44 }" \
		"pass in on ${epair_tester}b \
			route-to { (${epair_server2}a <rt_targets>) } random prefer-ipv6-nexthop \
			proto tcp \
			keep state"

	atf_check -s exit:1 ${common_dir}/pft_ping.py \
		--sendif ${epair_tester}a --replyif ${epair_tester}a \
		--fromaddr ${net_clients_6}::1 --to ${host_server_6} \
		--ping-type=tcp3way --send-sport=4201

	atf_check -o "match:map-failed +1 +" -x "jexec router pfctl -qvvsi 2> /dev/null"
}

prefer_ipv6_nexthop_ipv4_random_table_ipv6_cleanup()
{
	pft_cleanup
}

prefer_ipv6_nexthop_ipv6_random_prefix_ipv4_head()
{
	atf_set descr 'prefer-ipv6-nexthop option for an IPv6 prefix with random selection for IPv4 packets'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

prefer_ipv6_nexthop_ipv6_random_prefix_ipv4_body()
{
	pf_map_addr_common

	jexec router pfctl -e
	pft_set_rules router \
		"set reassemble yes" \
		"set state-policy if-bound" \
		"pass in on ${epair_tester}b \
			route-to { (${epair_server2}a ${net_server2_6}::42:0/127) } random prefer-ipv6-nexthop \
			proto tcp \
			keep state"

	good_targets="${net_server2_6}::42:0 ${net_server2_6}::42:1"
	check_random IPv4 "${good_targets}"
}

prefer_ipv6_nexthop_ipv6_random_prefix_ipv4_cleanup()
{
	pft_cleanup
}

prefer_ipv6_nexthop_ipv6_random_prefix_ipv6_head()
{
	atf_set descr 'prefer-ipv6-nexthop option for an IPv6 prefix with random selection for IPv6 packets'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

prefer_ipv6_nexthop_ipv6_random_prefix_ipv6_body()
{
	pf_map_addr_common

	jexec router pfctl -e
	pft_set_rules router \
		"set reassemble yes" \
		"set state-policy if-bound" \
		"pass in on ${epair_tester}b \
			route-to { (${epair_server2}a ${net_server2_6}::42:0/127) } random prefer-ipv6-nexthop \
			proto tcp \
			keep state"

	good_targets="${net_server2_6}::42:0 ${net_server2_6}::42:1"
	check_random IPv6 "${good_targets}"
}

prefer_ipv6_nexthop_ipv6_random_prefix_ipv6_cleanup()
{
	pft_cleanup
}

prefer_ipv6_nexthop_ipv4_random_prefix_ipv4_head()
{
	atf_set descr 'prefer-ipv6-nexthop option for an IPv4 prefix with random selection for IPv4 packets'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

prefer_ipv6_nexthop_ipv4_random_prefix_ipv4_body()
{
	pf_map_addr_common

	jexec router pfctl -e
	pft_set_rules router \
		"set reassemble yes" \
		"set state-policy if-bound" \
		"pass in on ${epair_tester}b \
			route-to { (${epair_server2}a ${net_server2_4}.40/31) } random prefer-ipv6-nexthop \
			proto tcp \
			keep state"

	good_targets="${net_server2_4}.40 ${net_server2_4}.41"
	check_random IPv4 "${good_targets}"
}

prefer_ipv6_nexthop_ipv4_random_prefix_ipv4_cleanup()
{
	pft_cleanup
}

prefer_ipv6_nexthop_ipv4_random_prefix_ipv6_head()
{
	atf_set descr 'prefer-ipv6-nexthop option for an IPv4 prefix with random selection for IPv6 packets'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

prefer_ipv6_nexthop_ipv4_random_prefix_ipv6_body()
{
	pf_map_addr_common

	# IPv6 packets can't be forwarded over IPv4 next-hops.
	# The failure happens in pf_map_addr() and increases the respective
	# error counter.

	jexec router pfctl -e
	pft_set_rules router \
		"set reassemble yes" \
		"set state-policy if-bound" \
		"pass in on ${epair_tester}b \
			route-to { (${epair_server2}a ${net_server2_4}.40/31) } random prefer-ipv6-nexthop \
			proto tcp \
			keep state"

	atf_check -s exit:1 ${common_dir}/pft_ping.py \
		--sendif ${epair_tester}a --replyif ${epair_tester}a \
		--fromaddr ${net_clients_6}::1 --to ${host_server_6} \
		--ping-type=tcp3way --send-sport=4201

	atf_check -o "match:map-failed +1 +" -x "jexec router pfctl -qvvsi 2> /dev/null"
}

prefer_ipv6_nexthop_ipv4_random_prefix_ipv6_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "v4"
	atf_add_test_case "v6"
	atf_add_test_case "multiwan"
	atf_add_test_case "multiwanlocal"
	atf_add_test_case "icmp_nat"
	atf_add_test_case "dummynet"
	atf_add_test_case "dummynet_in"
	atf_add_test_case "ifbound"
	atf_add_test_case "ifbound_v6"
	atf_add_test_case "ifbound_reply_to"
	atf_add_test_case "ifbound_reply_to_v6"
	atf_add_test_case "ifbound_reply_to_rdr_dummynet"
	atf_add_test_case "dummynet_frag"
	atf_add_test_case "dummynet_double"
	atf_add_test_case "sticky"
	atf_add_test_case "ttl"
	atf_add_test_case "empty_pool"
	# Tests for pf_map_addr() without prefer-ipv6-nexthop
	atf_add_test_case "table_loop"
	atf_add_test_case "roundrobin"
	atf_add_test_case "random_table"
	atf_add_test_case "random_prefix"
	# Tests for pf_map_addr() without prefer-ipv6-nexthop
	# Next hop is a Single IP address
	atf_add_test_case "prefer_ipv6_nexthop_single_ipv4"
	atf_add_test_case "prefer_ipv6_nexthop_single_ipv6"
	# Next hop is tables and prefixes, accessed by the round-robin algorithm
	atf_add_test_case "prefer_ipv6_nexthop_mixed_af_roundrobin_ipv4"
	atf_add_test_case "prefer_ipv6_nexthop_mixed_af_roundrobin_ipv6"
	# Next hop is a table, accessed by the random algorithm
	atf_add_test_case "prefer_ipv6_nexthop_mixed_af_random_table_ipv4"
	atf_add_test_case "prefer_ipv6_nexthop_ipv4_random_table_ipv4"
	atf_add_test_case "prefer_ipv6_nexthop_ipv4_random_table_ipv6"
	# Next hop is a prefix, accessed by the random algorithm
	atf_add_test_case "prefer_ipv6_nexthop_ipv6_random_prefix_ipv4"
	atf_add_test_case "prefer_ipv6_nexthop_ipv6_random_prefix_ipv6"
	atf_add_test_case "prefer_ipv6_nexthop_ipv4_random_prefix_ipv4"
	atf_add_test_case "prefer_ipv6_nexthop_ipv4_random_prefix_ipv6"
}
