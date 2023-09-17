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
	jexec srv /usr/sbin/inetd -p multiwan.pid $(atf_get_srcdir)/echo_inetd.conf

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
	rm -f multiwan.pid
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
	atf_set require.progs scapy
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
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.1
	# But this times out:
	atf_check -s exit:2 -o ignore ping -c 1 -t 1 192.0.2.1

	# return path dummynet
	pft_set_rules gw \
		"pass out route-to (${epair_srv}b 192.0.2.1) to 192.0.2.1 dnpipe (0, 1)"

	# The ping request will pass, but take 1.2 seconds
	# So this works:
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.1
	# But this times out:
	atf_check -s exit:2 -o ignore ping -c 1 -t 1 192.0.2.1
}

dummynet_cleanup()
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
}
