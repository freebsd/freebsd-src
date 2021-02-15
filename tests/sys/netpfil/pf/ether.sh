# $FreeBSD$
#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright © 2021. Rubicon Communications, LLC (Netgate). All Rights Reserved.
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

atf_test_case "mac" "cleanup"
mac_head()
{
	atf_set descr 'Test MAC address filtering'
	atf_set require.user root
}

mac_body()
{
	pft_init

	epair=$(vnet_mkepair)
	epair_a_mac=$(ifconfig ${epair}a ether | awk '/ether/ { print $2; }')

	ifconfig ${epair}a 192.0.2.1/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up

	pft_set_rules alcatraz \
		"ether block from ${epair_a_mac}"

	atf_check -s exit:0 -o ignore ping -c 1 -t 1 192.0.2.2

	# Now enable. Ping should fail.
	jexec alcatraz pfctl -e

	atf_check -s exit:2 -o ignore ping -c 1 -t 1 192.0.2.2

	# Should still fail for 'to'
	pft_set_rules alcatraz \
		"ether block to ${epair_a_mac}"
	atf_check -s exit:2 -o ignore ping -c 1 -t 1 192.0.2.2

	# Succeeds if we block a different MAC address
	pft_set_rules alcatraz \
		"ether block to 00:01:02:03:04:05"
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 192.0.2.2

	# Now try this with an interface specified
	pft_set_rules alcatraz \
		"ether block on ${epair}b from ${epair_a_mac}"
	atf_check -s exit:2 -o ignore ping -c 1 -t 1 192.0.2.2

	# Wrong interface should not match
	pft_set_rules alcatraz \
		"ether block on ${epair}a from ${epair_a_mac}"
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 192.0.2.2

	# Test negation
	pft_set_rules alcatraz \
		"ether block in on ${epair}b from ! ${epair_a_mac}"
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 192.0.2.2

	pft_set_rules alcatraz \
		"ether block out on ${epair}b to ! ${epair_a_mac}"
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 192.0.2.2
}

mac_cleanup()
{
	pft_cleanup
}

atf_test_case "proto" "cleanup"
proto_head()
{
	atf_set descr 'Test EtherType filtering'
	atf_set require.user root
}

proto_body()
{
	pft_init

	epair=$(vnet_mkepair)
	epair_a_mac=$(ifconfig ${epair}a ether | awk '/ether/ { print $2; }')

	ifconfig ${epair}a 192.0.2.1/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up

	pft_set_rules alcatraz \
		"ether block proto 0x0810"
	jexec alcatraz pfctl -e

	atf_check -s exit:0 -o ignore ping -c 1 -t 1 192.0.2.2

	# Block IP
	pft_set_rules alcatraz \
		"ether block proto 0x0800"
	atf_check -s exit:2 -o ignore ping -c 1 -t 1 192.0.2.2

	# Block ARP
	pft_set_rules alcatraz \
		"ether block proto 0x0806"
	arp -d 192.0.2.2
	atf_check -s exit:2 -o ignore ping -c 1 -t 1 192.0.2.2
}

proto_cleanup()
{
	pft_cleanup
}

atf_test_case "direction" "cleanup"
direction_head()
{
	atf_set descr 'Test directionality of ether rules'
	atf_set require.user root
	atf_set require.progs jq
}

direction_body()
{
	pft_init

	epair=$(vnet_mkepair)
	epair_a_mac=$(ifconfig ${epair}a ether | awk '/ether/ { print $2; }')
	epair_b_mac=$(ifconfig ${epair}b ether | awk '/ether/ { print $2; }')

	ifconfig ${epair}a 192.0.2.1/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up

	pft_set_rules alcatraz \
		"ether block in proto 0x0806"
	jexec alcatraz pfctl -e

	arp -d 192.0.2.2
	jexec alcatraz arp -d 192.0.2.1

	# We don't allow the jail to receive ARP requests, so if we try to ping
	# from host to jail the host can't resolve the MAC address
	ping -c 1 -t 1 192.0.2.2

	mac=$(arp -an --libxo json \
	    | jq '."arp"."arp-cache"[] |
	    select(."ip-address"=="192.0.2.2")."mac-address"')
	atf_check_not_equal "$mac" "$epair_b_mac"

	# Clear ARP table again
	arp -d 192.0.2.2
	jexec alcatraz arp -d 192.0.2.1

	# However, we allow outbound ARP, so the host will learn our MAC if the
	# jail tries to ping
	jexec alcatraz ping -c 1 -t 1 192.0.2.1

	mac=$(arp -an --libxo json \
	    | jq '."arp"."arp-cache"[] |
	    select(."ip-address"=="192.0.2.2")."mac-address"')
	atf_check_equal "$mac" "$epair_b_mac"

	# Now do the same, but with outbound ARP blocking
	pft_set_rules alcatraz \
		"ether block out proto 0x0806"

	# Clear ARP table again
	arp -d 192.0.2.2
	jexec alcatraz arp -d 192.0.2.1

	# The jail can't send ARP requests to us, so we'll never learn our MAC
	# address
	jexec alcatraz ping -c 1 -t 1 192.0.2.1

	mac=$(jexec alcatraz arp -an --libxo json \
	    | jq '."arp"."arp-cache"[] |
	    select(."ip-address"=="192.0.2.1")."mac-address"')
	atf_check_not_equal "$mac" "$epair_a_mac"
}

direction_cleanup()
{
	pft_cleanup
}

atf_test_case "captive" "cleanup"
captive_head()
{
	atf_set descr 'Test a basic captive portal-like setup'
	atf_set require.user root
}

captive_body()
{
	# Host is client, jail 'gw' is the captive portal gateway, jail 'srv'
	# is a random (web)server. We use the echo protocol rather than http
	# for the test, because that's easier.
	pft_init

	epair_gw=$(vnet_mkepair)
	epair_srv=$(vnet_mkepair)
	epair_gw_a_mac=$(ifconfig ${epair_gw}a ether | awk '/ether/ { print $2; }')

	vnet_mkjail gw ${epair_gw}b ${epair_srv}a
	vnet_mkjail srv ${epair_srv}b

	ifconfig ${epair_gw}a 192.0.2.2/24 up
	route add -net 198.51.100.0/24 192.0.2.1
	jexec gw ifconfig ${epair_gw}b 192.0.2.1/24 up
	jexec gw ifconfig lo0 127.0.0.1/8 up
	jexec gw sysctl net.inet.ip.forwarding=1

	jexec gw ifconfig ${epair_srv}a 198.51.100.1/24 up
	jexec srv ifconfig ${epair_srv}b 198.51.100.2/24 up
	jexec srv route add -net 192.0.2.0/24 198.51.100.1

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 198.51.100.2

	pft_set_rules gw \
		"ether pass quick proto 0x0806" \
		"ether pass tag captive" \
		"rdr on ${epair_gw}b proto tcp to port echo tagged captive -> 127.0.0.1 port echo"
	jexec gw pfctl -e

	# ICMP should still work, because we don't redirect it.
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 198.51.100.2

	# Run the echo server only on the gw, so we know we've redirectly
	# correctly if we get an echo message.
	jexec gw /usr/sbin/inetd $(atf_get_srcdir)/echo_inetd.conf

	# Confirm that we're getting redirected
	atf_check -s exit:0 -o match:"^foo$" -x "echo foo | nc -N 198.51.100.2 7"

	jexec gw killall inetd

	# Now pretend we've authenticated, so add the client's MAC address
	pft_set_rules gw \
		"ether pass quick proto 0x0806" \
		"ether pass quick from ${epair_gw_a_mac}" \
		"ether pass tag captive" \
		"rdr on ${epair_gw}b proto tcp to port echo tagged captive -> 127.0.0.1 port echo"

	# No redirect, so failure.
	atf_check -s exit:1 -x "echo foo | nc -N 198.51.100.2 7"

	# Start a server in srv
	jexec srv /usr/sbin/inetd $(atf_get_srcdir)/echo_inetd.conf

	# And now we can talk to that one.
	atf_check -s exit:0 -o match:"^foo$" -x "echo foo | nc -N 198.51.100.2 7"
}

captive_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "mac"
	atf_add_test_case "proto"
	atf_add_test_case "direction"
	atf_add_test_case "captive"
}
