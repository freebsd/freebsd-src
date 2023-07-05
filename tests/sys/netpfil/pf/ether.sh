# $FreeBSD$
#
# SPDX-License-Identifier: BSD-2-Clause
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

common_dir=$(atf_get_srcdir)/../common

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

	# Should still fail for 'to', even if it's in a list
	pft_set_rules alcatraz \
		"ether block to { ${epair_a_mac}, 00:01:02:0:04:05 }"
	atf_check -s exit:2 -o ignore ping -c 1 -t 1 192.0.2.2

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

	# Block everything not us
	pft_set_rules alcatraz \
		"ether block out on ${epair}b to { ! ${epair_a_mac} }"
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 192.0.2.2

	# Block us now
	pft_set_rules alcatraz \
		"ether block out on ${epair}b to { ! 00:01:02:03:04:05 }"
	atf_check -s exit:2 -o ignore ping -c 1 -t 1 192.0.2.2

	# Block with a masked address
	pft_set_rules alcatraz \
		"ether block out on ${epair}b to { ! 00:01:02:03:00:00/32 }"
	jexec alcatraz pfctl -se
	atf_check -s exit:2 -o ignore ping -c 1 -t 1 192.0.2.2

	epair_prefix=$(echo $epair_a_mac | cut -c-8)
	pft_set_rules alcatraz \
		"ether block out on ${epair}b to { ${epair_prefix}:00:00:00/24 }"
	atf_check -s exit:2 -o ignore ping -c 1 -t 1 192.0.2.2

	pft_set_rules alcatraz \
		"ether block out on ${epair}b to { ${epair_prefix}:00:00:00&ff:ff:ff:00:00:00 }"
	atf_check -s exit:2 -o ignore ping -c 1 -t 1 192.0.2.2

	# Check '-F ethernet' works
	jexec alcatraz pfctl -F ethernet
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

atf_test_case "captive_long" "cleanup"
captive_long_head()
{
	atf_set descr 'More complex captive portal setup'
	atf_set require.user root
}

captive_long_body()
{
	# Host is client, jail 'gw' is the captive portal gateway, jail 'srv'
	# is a random (web)server. We use the echo protocol rather than http
	# for the test, because that's easier.
	dummynet_init

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

	jexec gw dnctl pipe 1 config bw 300KByte/s

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 198.51.100.2

	pft_set_rules gw \
		"ether anchor \"captiveportal\" on { ${epair_gw}b } {" \
			"ether pass quick proto { 0x0806, 0x8035, 0x888e, 0x88c7, 0x8863, 0x8864 }" \
			"ether pass tag \"captive\"" \
		"}" \
		"rdr on ${epair_gw}b proto tcp to port daytime tagged captive -> 127.0.0.1 port echo"
	jexec gw pfctl -e

	# ICMP should still work, because we don't redirect it.
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 198.51.100.2

	jexec gw /usr/sbin/inetd -p gw.pid $(atf_get_srcdir)/echo_inetd.conf
	jexec srv /usr/sbin/inetd -p srv.pid $(atf_get_srcdir)/daytime_inetd.conf

	echo foo | nc -N 198.51.100.2 13

	# Confirm that we're getting redirected
	atf_check -s exit:0 -o match:"^foo$" -x "echo foo | nc -N 198.51.100.2 13"

	# Now update the rules to allow our client to pass without redirect
	pft_set_rules gw \
		"ether anchor \"captiveportal\" on { ${epair_gw}b } {" \
			"ether pass quick proto { 0x0806, 0x8035, 0x888e, 0x88c7, 0x8863, 0x8864 }" \
			"ether pass quick from { ${epair_gw_a_mac} } dnpipe 1" \
			"ether pass tag \"captive\"" \
		"}" \
		"rdr on ${epair_gw}b proto tcp to port daytime tagged captive -> 127.0.0.1 port echo"

	# We're not being redirected and get datime information now
	atf_check -s exit:0 -o match:"^(Mon|Tue|Wed|Thu|Fri|Sat|Sun)" -x "echo foo | nc -N 198.51.100.2 13"

	jexec gw killall inetd
	jexec srv killall inetd
}

captive_long_cleanup()
{
	pft_cleanup
}

atf_test_case "dummynet" "cleanup"
dummynet_head()
{
	atf_set descr 'Test dummynet for L2 traffic'
	atf_set require.user root
}

dummynet_body()
{
	pft_init

	if ! kldstat -q -m dummynet; then
		atf_skip "This test requires dummynet"
	fi

	epair=$(vnet_mkepair)
	vnet_mkjail alcatraz ${epair}b

	ifconfig ${epair}a 192.0.2.1/24 up
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore ping -i .1 -c 3 -s 1200 192.0.2.2

	jexec alcatraz dnctl pipe 1 config bw 30Byte/s
	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"ether pass in dnpipe 1"

	# Ensure things don't break if non-IP(v4/v6) traffic hits dummynet
	arp -d 192.0.2.2

	# single ping succeeds just fine
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	# Saturate the link
	ping -i .1 -c 5 -s 1200 192.0.2.2

	# We should now be hitting the limits and get this packet dropped.
	atf_check -s exit:2 -o ignore ping -c 1 -s 1200 192.0.2.2

	# We can now also dummynet outbound traffic!
	pft_set_rules alcatraz \
		"ether pass out dnpipe 1"

	# We should still be hitting the limits and get this packet dropped.
	atf_check -s exit:2 -o ignore ping -c 1 -s 1200 192.0.2.2
}

dummynet_cleanup()
{
	pft_cleanup
}

atf_test_case "anchor" "cleanup"
anchor_head()
{
	atf_set descr 'Test ether anchors'
	atf_set require.user root
}

anchor_body()
{
	pft_init

	epair=$(vnet_mkepair)
	epair_a_mac=$(ifconfig ${epair}a ether | awk '/ether/ { print $2; }')

	vnet_mkjail alcatraz ${epair}b

	ifconfig ${epair}a 192.0.2.1/24 up
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"ether anchor \"foo\" in on lo0 {" \
			"ether block" \
		"}"

	# That only filters on lo0, so we should still be able to pass traffic
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	pft_set_rules alcatraz \
		"ether block in" \
		"ether anchor \"foo\" in on ${epair}b {" \
			"ether pass" \
		"}"
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	pft_set_rules alcatraz \
		"ether pass" \
		"ether anchor \"bar\" in on ${epair}b {" \
			"ether block" \
		"}"
	atf_check -s exit:2 -o ignore ping -c 1 -t 2 192.0.2.2

	pft_set_rules alcatraz \
		"ether block in" \
		"ether anchor \"baz\" on ${epair}b {" \
			"ether pass in from 01:02:03:04:05:06" \
		"}" \
		"ether pass in from ${epair_a_mac}"
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	atf_check -s exit:0 -o match:'baz' jexec alcatraz pfctl -sA
}

anchor_cleanup()
{
	pft_cleanup
}

atf_test_case "ip" "cleanup"
ip_head()
{
	atf_set descr 'Test filtering based on IP source/destination'
	atf_set require.user root
}

ip_body()
{
	pft_init

	epair=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair}b

	ifconfig ${epair}a 192.0.2.1/24 up
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"ether pass" \
		"ether block in l3 from 192.0.2.1"

	atf_check -s exit:2 -o ignore ping -c 1 192.0.2.2

	# Change IP address and we can ping again
	ifconfig ${epair}a 192.0.2.3/24 up
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	# Test the 'to' keyword too
	pft_set_rules alcatraz \
		"ether pass" \
		"ether block out l3 to 192.0.2.3"
	atf_check -s exit:2 -o ignore ping -c 1 192.0.2.2

	# Test table
	pft_set_rules alcatraz \
		"table <tbl> { 192.0.2.3 }" \
		"ether pass" \
		"ether block out l3 to <tbl>"
	atf_check -s exit:2 -o ignore ping -c 1 192.0.2.2
}

ip_cleanup()
{
	pft_cleanup
}

atf_test_case "tag" "cleanup"
tag_head()
{
	atf_set descr 'Test setting tags'
	atf_set require.user root
}

tag_body()
{
	pft_init

	epair=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair}b
	ifconfig ${epair}a 192.0.2.1/24 up
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"ether pass in tag foo" \
		"block in tagged foo"

	atf_check -s exit:2 -o ignore ping -c 1 192.0.2.2

	pft_set_rules alcatraz \
		"ether pass in tag bar" \
		"block in tagged foo"

	# Still passes when tagged differently
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2
}

tag_cleanup()
{
	pft_cleanup
}

atf_test_case "match_tag" "cleanup"
match_tag_head()
{
	atf_set descr 'Test matching tags'
	atf_set require.user root
}

match_tag_body()
{
	pft_init

	epair=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair}b

	ifconfig ${epair}a 192.0.2.1/24 up
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"ether block out tagged foo" \
		"pass in proto icmp tag foo"

	atf_check -s exit:2 -o ignore ping -c 1 192.0.2.2

	pft_set_rules alcatraz \
		"ether block out tagged bar" \
		"pass in proto icmp tag foo"

	# Still passes when tagged differently
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2
}

match_tag_cleanup()
{
	pft_cleanup
}

atf_test_case "short_pkt" "cleanup"
short_pkt_head()
{
	atf_set descr 'Test overly short Ethernet packets'
	atf_set require.user root
	atf_set require.progs scapy
}

short_pkt_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a 192.0.2.1/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"ether pass in" \
		"ether pass out" \
		"ether pass in l3 from 192.0.2.1"

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	jexec alcatraz pfctl -se -v

	# Try sending ever shorter ping requests
	# BPF won't let us send anything shorter than an Ethernet header, but
	# that's good enough for this test
	$(atf_get_srcdir)/pft_ether.py \
	    --sendif ${epair}a \
	    --to 192.0.2.2 \
	    --len 14-64
}

short_pkt_cleanup()
{
	pft_cleanup
}

atf_test_case "bridge_to" "cleanup"
bridge_to_head()
{
	atf_set descr 'Test bridge-to keyword'
	atf_set require.user root
	atf_set require.progs scapy
}

bridge_to_body()
{
	pft_init

	epair_in=$(vnet_mkepair)
	epair_out=$(vnet_mkepair)

	ifconfig ${epair_in}a 192.0.2.1/24 up
	ifconfig ${epair_out}a up

	vnet_mkjail alcatraz ${epair_in}b ${epair_out}b
	jexec alcatraz ifconfig ${epair_in}b 192.0.2.2/24 up
	jexec alcatraz ifconfig ${epair_out}b up

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2
	atf_check -s exit:1 -o ignore \
		${common_dir}/pft_ping.py \
		--sendif ${epair_in}a \
		--to 192.0.2.2 \
		--recvif ${epair_out}a

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"ether pass in on ${epair_in}b bridge-to ${epair_out}b"

	# Now the packets go out epair_out rather than be processed locally
	atf_check -s exit:2 -o ignore ping -c 1 192.0.2.2
	atf_check -s exit:0 -o ignore \
		${common_dir}/pft_ping.py \
		--sendif ${epair_in}a \
		--to 192.0.2.2 \
		--recvif ${epair_out}a
}

bridge_to_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "mac"
	atf_add_test_case "proto"
	atf_add_test_case "direction"
	atf_add_test_case "captive"
	atf_add_test_case "captive_long"
	atf_add_test_case "dummynet"
	atf_add_test_case "anchor"
	atf_add_test_case "ip"
	atf_add_test_case "tag"
	atf_add_test_case "match_tag"
	atf_add_test_case "short_pkt"
	atf_add_test_case "bridge_to"
}
