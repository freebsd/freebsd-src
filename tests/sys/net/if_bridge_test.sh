#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 The FreeBSD Foundation
#
# This software was developed by Kristof Provost under sponsorship
# from the FreeBSD Foundation.
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

. $(atf_get_srcdir)/../common/vnet.subr

atf_test_case "bridge_transmit_ipv4_unicast" "cleanup"
bridge_transmit_ipv4_unicast_head()
{
	atf_set descr 'bridge_transmit_ipv4_unicast bridging test'
	atf_set require.user root
}

bridge_transmit_ipv4_unicast_body()
{
	vnet_init
	vnet_init_bridge

	epair_alcatraz=$(vnet_mkepair)
	epair_singsing=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair_alcatraz}b
	vnet_mkjail singsing ${epair_singsing}b

	jexec alcatraz ifconfig ${epair_alcatraz}b 192.0.2.1/24 up
	jexec singsing ifconfig ${epair_singsing}b 192.0.2.2/24 up

	bridge=$(vnet_mkbridge)

	ifconfig ${bridge} up
	ifconfig ${epair_alcatraz}a up
	ifconfig ${epair_singsing}a up
	ifconfig ${bridge} addm ${epair_alcatraz}a
	ifconfig ${bridge} addm ${epair_singsing}a

	atf_check -s exit:0 -o ignore jexec alcatraz ping -c 3 -t 1 192.0.2.2
	atf_check -s exit:0 -o ignore jexec singsing ping -c 3 -t 1 192.0.2.1
}

bridge_transmit_ipv4_unicast_cleanup()
{
	vnet_cleanup
}

atf_test_case "stp" "cleanup"
stp_head()
{
	atf_set descr 'Spanning tree test'
	atf_set require.user root
}

stp_body()
{
	vnet_init
	vnet_init_bridge

	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)
	bridge_a=$(vnet_mkbridge)
	bridge_b=$(vnet_mkbridge)

	vnet_mkjail a ${bridge_a} ${epair_one}a ${epair_two}a
	vnet_mkjail b ${bridge_b} ${epair_one}b ${epair_two}b

	jexec a ifconfig ${epair_one}a up
	jexec a ifconfig ${epair_two}a up
	jexec a ifconfig ${bridge_a} addm ${epair_one}a
	jexec a ifconfig ${bridge_a} addm ${epair_two}a

	jexec b ifconfig ${epair_one}b up
	jexec b ifconfig ${epair_two}b up
	jexec b ifconfig ${bridge_b} addm ${epair_one}b
	jexec b ifconfig ${bridge_b} addm ${epair_two}b

	jexec a ifconfig ${bridge_a} 192.0.2.1/24

	# Enable spanning tree
	jexec a ifconfig ${bridge_a} stp ${epair_one}a
	jexec a ifconfig ${bridge_a} stp ${epair_two}a
	jexec b ifconfig ${bridge_b} stp ${epair_one}b
	jexec b ifconfig ${bridge_b} stp ${epair_two}b

	jexec b ifconfig ${bridge_b} up
	jexec a ifconfig ${bridge_a} up

	# Give STP time to do its thing
	sleep 5

	a_discard=$(jexec a ifconfig ${bridge_a} | grep discarding)
	b_discard=$(jexec b ifconfig ${bridge_b} | grep discarding)

	if [ -z "${a_discard}" ] && [ -z "${b_discard}" ]
	then
		atf_fail "STP failed to detect bridging loop"
	fi

	# We must also have at least some forwarding interfaces
	a_forwarding=$(jexec a ifconfig ${bridge_a} | grep forwarding)
	b_forwarding=$(jexec b ifconfig ${bridge_b} | grep forwarding)

	if [ -z "${a_forwarding}" ] && [ -z "${b_forwarding}" ]
	then
		atf_fail "STP failed to detect bridging loop"
	fi
}

stp_cleanup()
{
	vnet_cleanup
}

atf_test_case "stp_vlan" "cleanup"
stp_vlan_head()
{
	atf_set descr 'Spanning tree on VLAN test'
	atf_set require.user root
}

stp_vlan_body()
{
	vnet_init
	vnet_init_bridge
	_vnet_check_req vlan

	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)
	bridge_a=$(vnet_mkbridge)
	bridge_b=$(vnet_mkbridge)

	vnet_mkjail a ${bridge_a} ${epair_one}a ${epair_two}a
	vnet_mkjail b ${bridge_b} ${epair_one}b ${epair_two}b

	jexec a ifconfig ${epair_one}a up
	jexec a ifconfig ${epair_two}a up
	vlan_a_one=$(jexec a ifconfig vlan create vlandev ${epair_one}a vlan 42)
	vlan_a_two=$(jexec a ifconfig vlan create vlandev ${epair_two}a vlan 42)
	jexec a ifconfig ${vlan_a_one} up
	jexec a ifconfig ${vlan_a_two} up
	jexec a ifconfig ${bridge_a} addm ${vlan_a_one}
	jexec a ifconfig ${bridge_a} addm ${vlan_a_two}

	jexec b ifconfig ${epair_one}b up
	jexec b ifconfig ${epair_two}b up
	vlan_b_one=$(jexec b ifconfig vlan create vlandev ${epair_one}b vlan 42)
	vlan_b_two=$(jexec b ifconfig vlan create vlandev ${epair_two}b vlan 42)
	jexec b ifconfig ${vlan_b_one} up
	jexec b ifconfig ${vlan_b_two} up
	jexec b ifconfig ${bridge_b} addm ${vlan_b_one}
	jexec b ifconfig ${bridge_b} addm ${vlan_b_two}

	jexec a ifconfig ${bridge_a} 192.0.2.1/24

	# Enable spanning tree
	jexec a ifconfig ${bridge_a} stp ${vlan_a_one}
	jexec a ifconfig ${bridge_a} stp ${vlan_a_two}
	jexec b ifconfig ${bridge_b} stp ${vlan_b_one}
	jexec b ifconfig ${bridge_b} stp ${vlan_b_two}

	jexec b ifconfig ${bridge_b} up
	jexec a ifconfig ${bridge_a} up

	# Give STP time to do its thing
	sleep 5

	a_discard=$(jexec a ifconfig ${bridge_a} | grep discarding)
	b_discard=$(jexec b ifconfig ${bridge_b} | grep discarding)

	if [ -z "${a_discard}" ] && [ -z "${b_discard}" ]
	then
		atf_fail "STP failed to detect bridging loop"
	fi

	# We must also have at least some forwarding interfaces
	a_forwarding=$(jexec a ifconfig ${bridge_a} | grep forwarding)
	b_forwarding=$(jexec b ifconfig ${bridge_b} | grep forwarding)

	if [ -z "${a_forwarding}" ] && [ -z "${b_forwarding}" ]
	then
		atf_fail "STP failed to detect bridging loop"
	fi
}

stp_vlan_cleanup()
{
	vnet_cleanup
}

atf_test_case "static" "cleanup"
static_head()
{
	atf_set descr 'Bridge static address test'
	atf_set require.user root
}

static_body()
{
	vnet_init
	vnet_init_bridge

	epair=$(vnet_mkepair)
	bridge=$(vnet_mkbridge)

	vnet_mkjail one ${bridge} ${epair}a

	ifconfig ${epair}b up

	jexec one ifconfig ${bridge} up
	jexec one ifconfig ${epair}a up
	jexec one ifconfig ${bridge} addm ${epair}a

	# Wrong interface
	atf_check -s exit:1 -o ignore -e ignore \
	    jexec one ifconfig ${bridge} static ${epair}b 00:01:02:03:04:05

	# Bad address format
	atf_check -s exit:1 -o ignore -e ignore \
	    jexec one ifconfig ${bridge} static ${epair}a 00:01:02:03:04

	# Correct add
	atf_check -s exit:0 -o ignore \
	    jexec one ifconfig ${bridge} static ${epair}a 00:01:02:03:04:05

	# List addresses
	atf_check -s exit:0 \
	    -o match:"00:01:02:03:04:05 Vlan0 ${epair}a 0 flags=1<STATIC>" \
	    jexec one ifconfig ${bridge} addr

	# Delete with bad address format
	atf_check -s exit:1 -o ignore -e ignore \
	    jexec one ifconfig ${bridge} deladdr 00:01:02:03:04

	# Delete with unlisted address
	atf_check -s exit:1 -o ignore -e ignore \
	    jexec one ifconfig ${bridge} deladdr 00:01:02:03:04:06

	# Correct delete
	atf_check -s exit:0 -o ignore \
	    jexec one ifconfig ${bridge} deladdr 00:01:02:03:04:05
}

static_cleanup()
{
	vnet_cleanup
}

atf_test_case "vstatic" "cleanup"
vstatic_head()
{
	atf_set descr 'Bridge VLAN static address test'
	atf_set require.user root
}

vstatic_body()
{
	vnet_init
	vnet_init_bridge

	epair=$(vnet_mkepair)
	bridge=$(vnet_mkbridge)

	vnet_mkjail one ${bridge} ${epair}a

	ifconfig ${epair}b up

	jexec one ifconfig ${bridge} up
	jexec one ifconfig ${epair}a up
	jexec one ifconfig ${bridge} addm ${epair}a

	# Wrong interface
	atf_check -s exit:1 -o ignore -e ignore jexec one \
	    ifconfig ${bridge} static ${epair}b 00:01:02:03:04:05 vlan 10

	# Bad address format
	atf_check -s exit:1 -o ignore -e ignore jexec one \
	    ifconfig ${bridge} static ${epair}a 00:01:02:03:04 vlan 10

	# Invalid VLAN ID
	atf_check -s exit:1 -o ignore -e ignore jexec one \
	    ifconfig ${bridge} static ${epair}a 00:01:02:03:04:05 vlan 5000

	# Correct add
	atf_check -s exit:0 -o ignore jexec one \
	    ifconfig ${bridge} static ${epair}a 00:01:02:03:04:05 vlan 10

	# List addresses
	atf_check -s exit:0 \
	    -o match:"00:01:02:03:04:05 Vlan10 ${epair}a 0 flags=1<STATIC>" \
	    jexec one ifconfig ${bridge} addr

	# Delete with bad address format
	atf_check -s exit:1 -o ignore -e ignore jexec one \
	    ifconfig ${bridge} deladdr 00:01:02:03:04 vlan 10

	# Delete with unlisted address
	atf_check -s exit:1 -o ignore -e ignore jexec one \
	    ifconfig ${bridge} deladdr 00:01:02:03:04:06 vlan 10

	# Delete with wrong vlan id
	atf_check -s exit:1 -o ignore -e ignore jexec one \
	    ifconfig ${bridge} deladdr 00:01:02:03:04:05 vlan 20

	# Correct delete
	atf_check -s exit:0 -o ignore jexec one \
	    ifconfig ${bridge} deladdr 00:01:02:03:04:05 vlan 10
}

vstatic_cleanup()
{
	vnet_cleanup
}

atf_test_case "span" "cleanup"
span_head()
{
	atf_set descr 'Bridge span test'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

span_body()
{
	vnet_init
	vnet_init_bridge

	epair=$(vnet_mkepair)
	epair_span=$(vnet_mkepair)
	bridge=$(vnet_mkbridge)

	vnet_mkjail one ${bridge} ${epair}a ${epair_span}a

	ifconfig ${epair}b up
	ifconfig ${epair_span}b up

	jexec one ifconfig ${bridge} up
	jexec one ifconfig ${epair}a up
	jexec one ifconfig ${epair_span}a up
	jexec one ifconfig ${bridge} addm ${epair}a

	jexec one ifconfig ${bridge} span ${epair_span}a
	jexec one ifconfig ${bridge} 192.0.2.1/24

	# Send some traffic through the span
	jexec one ping -c 1 -t 1 192.0.2.2

	# Check that we see the traffic on the span interface
	atf_check -s exit:0 \
		$(atf_get_srcdir)/../netpfil/common/pft_ping.py \
		--sendif ${epair}b \
		--to 192.0.2.2 \
		--recvif ${epair_span}b

	jexec one ifconfig ${bridge} -span ${epair_span}a

	# And no more traffic after we remove the span
	atf_check -s exit:1 \
		$(atf_get_srcdir)/../netpfil/common/pft_ping.py \
		--sendif ${epair}b \
		--to 192.0.2.2 \
		--recvif ${epair_span}b
}

span_cleanup()
{
	vnet_cleanup
}

atf_test_case "delete_with_members" "cleanup"
delete_with_members_head()
{
	atf_set descr 'Delete a bridge which still has member interfaces'
	atf_set require.user root
}

delete_with_members_body()
{
	vnet_init
	vnet_init_bridge

	bridge=$(vnet_mkbridge)
	epair=$(vnet_mkepair)

	ifconfig ${bridge} 192.0.2.1/24 up
	ifconfig ${epair}a up
	ifconfig ${bridge} addm ${epair}a

	ifconfig ${bridge} destroy
}

delete_with_members_cleanup()
{
	vnet_cleanup
}

atf_test_case "mac_conflict" "cleanup"
mac_conflict_head()
{
	atf_set descr 'Ensure that bridges in different jails get different mac addresses'
	atf_set require.user root
}

mac_conflict_body()
{
	vnet_init
	vnet_init_bridge

	epair=$(vnet_mkepair)

	# Ensure the bridge module is loaded so jails can use it.
	tmpbridge=$(vnet_mkbridge)

	vnet_mkjail bridge_mac_conflict_one ${epair}a
	vnet_mkjail bridge_mac_conflict_two ${epair}b

	jexec bridge_mac_conflict_one ifconfig bridge create
	jexec bridge_mac_conflict_one ifconfig bridge0 192.0.2.1/24 up \
	    addm ${epair}a
	jexec bridge_mac_conflict_one ifconfig ${epair}a up

	jexec bridge_mac_conflict_two ifconfig bridge create
	jexec bridge_mac_conflict_two ifconfig bridge0 192.0.2.2/24 up \
	    addm ${epair}b
	jexec bridge_mac_conflict_two ifconfig ${epair}b up

	atf_check -s exit:0 -o ignore \
	    jexec bridge_mac_conflict_one ping -c 3 192.0.2.2
}

mac_conflict_cleanup()
{
	vnet_cleanup
}

atf_test_case "inherit_mac" "cleanup"
inherit_mac_head()
{
	atf_set descr 'Bridge inherit_mac test, #216510'
	atf_set require.user root
}

inherit_mac_body()
{
	vnet_init
	vnet_init_bridge

	bridge=$(vnet_mkbridge)
	epair=$(vnet_mkepair)
	vnet_mkjail one ${bridge} ${epair}a

	jexec one sysctl net.link.bridge.inherit_mac=1

	# Attempt to provoke the panic described in #216510
	jexec one ifconfig ${bridge} 192.0.0.1/24 up
	jexec one ifconfig ${bridge} addm ${epair}a
}

inherit_mac_cleanup()
{
	vnet_cleanup
}

atf_test_case "stp_validation" "cleanup"
stp_validation_head()
{
	atf_set descr 'Check STP validation'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

stp_validation_body()
{
	vnet_init
	vnet_init_bridge

	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)
	bridge=$(vnet_mkbridge)

	ifconfig ${bridge} up
	ifconfig ${bridge} addm ${epair_one}a addm ${epair_two}a
	ifconfig ${bridge} stp ${epair_one}a stp ${epair_two}a

	ifconfig ${epair_one}a up
	ifconfig ${epair_one}b up
	ifconfig ${epair_two}a up
	ifconfig ${epair_two}b up

	# Wait until the interfaces are no longer discarding
	while ifconfig ${bridge} | grep 'state discarding' >/dev/null
	do
		sleep 1
	done

	# Now inject invalid STP BPDUs on epair_one and see if they're repeated
	# on epair_two
	atf_check -s exit:0 \
	    $(atf_get_srcdir)/stp.py \
	    --sendif ${epair_one}b \
	    --recvif ${epair_two}b
}

stp_validation_cleanup()
{
	vnet_cleanup
}

atf_test_case "gif" "cleanup"
gif_head()
{
	atf_set descr 'gif as a bridge member'
	atf_set require.user root
}

gif_body()
{
	vnet_init
	vnet_init_bridge
	_vnet_check_req gif

	epair=$(vnet_mkepair)

	vnet_mkjail one ${epair}a
	vnet_mkjail two ${epair}b

	jexec one sysctl net.link.gif.max_nesting=2
	jexec two sysctl net.link.gif.max_nesting=2

	jexec one ifconfig ${epair}a 192.0.2.1/24 up
	jexec two ifconfig ${epair}b 192.0.2.2/24 up

	# Tunnel
	gif_one=$(jexec one ifconfig gif create)
	gif_two=$(jexec two ifconfig gif create)

	jexec one ifconfig ${gif_one} tunnel 192.0.2.1 192.0.2.2
	jexec one ifconfig ${gif_one} up
	jexec two ifconfig ${gif_two} tunnel 192.0.2.2 192.0.2.1
	jexec two ifconfig ${gif_two} up

	bridge_one=$(jexec one ifconfig bridge create)
	bridge_two=$(jexec two ifconfig bridge create)
	jexec one ifconfig ${bridge_one} 198.51.100.1/24 up
	jexec one ifconfig ${bridge_one} addm ${gif_one}
	jexec two ifconfig ${bridge_two} 198.51.100.2/24 up
	jexec two ifconfig ${bridge_two} addm ${gif_two}

	# Sanity check
	atf_check -s exit:0 -o ignore \
		jexec one ping -c 1 192.0.2.2

	# Test tunnel
	atf_check -s exit:0 -o ignore \
		jexec one ping -c 1 198.51.100.2
	atf_check -s exit:0 -o ignore \
		jexec one ping -c 1 -s 1200 198.51.100.2
	atf_check -s exit:0 -o ignore \
		jexec one ping -c 1 -s 2000 198.51.100.2

	# Higher MTU on the tunnel than on the underlying interface
	jexec one ifconfig ${epair}a mtu 1000
	jexec two ifconfig ${epair}b mtu 1000

	atf_check -s exit:0 -o ignore \
		jexec one ping -c 1 -s 1200 198.51.100.2
	atf_check -s exit:0 -o ignore \
		jexec one ping -c 1 -s 2000 198.51.100.2

	# Assigning IP addresses on the gif tunneling interfaces
	jexec one sysctl net.link.bridge.member_ifaddrs=1
	atf_check -s exit:0 -o ignore \
		jexec one ifconfig ${gif_one} 192.168.0.224/24 192.168.169.254
	atf_check -s exit:0 -o ignore \
		jexec one ifconfig ${gif_one} inet6 no_dad 2001:db8::1/64
	jexec one ifconfig ${bridge_one} deletem ${gif_one}
	atf_check -s exit:0 -o ignore \
		jexec one ifconfig ${bridge_one} addm ${gif_one}

	jexec two sysctl net.link.bridge.member_ifaddrs=0
	atf_check -s exit:0 -o ignore \
		jexec two ifconfig ${gif_two} 192.168.169.254/24 192.168.0.224
	atf_check -s exit:0 -o ignore \
		jexec two ifconfig ${gif_two} inet6 no_dad 2001:db8::2/64
	jexec two ifconfig ${bridge_two} deletem ${gif_two}
	atf_check -s exit:0 -o ignore \
		jexec two ifconfig ${bridge_two} addm ${gif_two}
}

gif_cleanup()
{
	vnet_cleanup
}

atf_test_case "mtu" "cleanup"
mtu_head()
{
	atf_set descr 'Bridge MTU changes'
	atf_set require.user root
}

get_mtu()
{
	intf=$1

	ifconfig ${intf} | awk '$5 == "mtu" { print $6 }'
}

check_mtu()
{
	intf=$1
	expected=$2

	mtu=$(get_mtu $intf)
	if [ "$mtu" -ne "$expected" ];
	then
		atf_fail "Expected MTU of $expected on $intf but found $mtu"
	fi
}

mtu_body()
{
	vnet_init
	vnet_init_bridge
	_vnet_check_req gif

	epair=$(vnet_mkepair)
	gif=$(ifconfig gif create)
	echo ${gif} >> created_interfaces.lst
	bridge=$(vnet_mkbridge)

	atf_check -s exit:0 \
		ifconfig ${bridge} addm ${epair}a

	ifconfig ${gif} mtu 1500
	atf_check -s exit:0 \
		ifconfig ${bridge} addm ${gif}

	# Changing MTU changes it for all member interfaces
	atf_check -s exit:0 \
		ifconfig ${bridge} mtu 2000

	check_mtu ${bridge} 2000
	check_mtu ${gif} 2000
	check_mtu ${epair}a 2000

	# Rejected MTUs mean none of the MTUs change
	atf_check -s exit:1 -e ignore \
		ifconfig ${bridge} mtu 9000

	check_mtu ${bridge} 2000
	check_mtu ${gif} 2000
	check_mtu ${epair}a 2000

	# We're not allowed to change the MTU of a member interface
	atf_check -s exit:1 -e ignore \
		ifconfig ${epair}a mtu 1900
	check_mtu ${epair}a 2000

	# Test adding an interface with a different MTU
	new_epair=$(vnet_mkepair)
	check_mtu ${new_epair}a 1500
	atf_check -s exit:0 -e ignore \
		ifconfig ${bridge} addm ${new_epair}a

	check_mtu ${bridge} 2000
	check_mtu ${gif} 2000
	check_mtu ${epair}a 2000
	check_mtu ${new_epair}a 2000
}

mtu_cleanup()
{
	vnet_cleanup
}

atf_test_case "vlan" "cleanup"
vlan_head()
{
	atf_set descr 'Ensure the bridge takes vlan ID into account, PR#270559'
	atf_set require.user root
}

vlan_body()
{
	vnet_init
	vnet_init_bridge
	_vnet_check_req vlan

	vid=1

	epaira=$(vnet_mkepair)
	epairb=$(vnet_mkepair)

	br=$(vnet_mkbridge)

	vnet_mkjail one ${epaira}b
	vnet_mkjail two ${epairb}b

	ifconfig ${br} up
	ifconfig ${epaira}a up
	ifconfig ${epairb}a up
	ifconfig ${br} addm ${epaira}a addm ${epairb}a

	jexec one ifconfig ${epaira}b up
	jexec one ifconfig ${epaira}b.${vid} create

	jexec two ifconfig ${epairb}b up
	jexec two ifconfig ${epairb}b.${vid} create

	# Create a MAC address conflict between an untagged and tagged interface
	jexec two ifconfig ${epairb}b.${vid} ether 02:05:6e:06:28:1a
	jexec one ifconfig ${epaira}b ether 02:05:6e:06:28:1a
	jexec one ifconfig ${epaira}b.${vid} ether 02:05:6e:06:28:1b

	# Add ip address, will also populate $br's fowarding table, by ARP announcement
	jexec one ifconfig ${epaira}b.${vid} 192.0.2.1/24 up
	jexec two ifconfig ${epairb}b.${vid} 192.0.2.2/24 up

	sleep 0.5

	ifconfig ${br}
	jexec one ifconfig
	jexec two ifconfig
	ifconfig ${br} addr

	atf_check -s exit:0 -o ignore \
	    jexec one ping -c 1 -t 1 192.0.2.2

	# This will trigger a mac flap (by ARP announcement)
	jexec one ifconfig ${epaira}b 192.0.2.1/24 up

	sleep 0.5

	ifconfig ${br} addr

	atf_check -s exit:0 -o ignore \
	    jexec one ping -c 1 -t 1 192.0.2.2
}

vlan_cleanup()
{
	vnet_cleanup
}

atf_test_case "many_bridge_members" "cleanup"
many_bridge_members_head()
{
	atf_set descr 'many_bridge_members ifconfig test'
	atf_set require.user root
}

many_bridge_members_body()
{
	vnet_init
	vnet_init_bridge

	bridge=$(vnet_mkbridge)
	ifcount=256
	for _ in $(seq 1 $ifcount); do
		epair=$(vnet_mkepair)
		ifconfig "${bridge}" addm "${epair}"a
	done

	atf_check -s exit:0 -o inline:"$ifcount\n" \
	  sh -c "ifconfig ${bridge} | grep member: | wc -l | xargs"
}

many_bridge_members_cleanup()
{
	vnet_cleanup
}

atf_test_case "member_ifaddrs_enabled" "cleanup"
member_ifaddrs_enabled_head()
{
	atf_set descr 'bridge with member_ifaddrs=1'
	atf_set require.user root
}

member_ifaddrs_enabled_body()
{
	vnet_init
	vnet_init_bridge

	ep=$(vnet_mkepair)
	ifconfig ${ep}a inet 192.0.2.1/24 up

	vnet_mkjail one ${ep}b
	jexec one sysctl net.link.bridge.member_ifaddrs=1
	jexec one ifconfig ${ep}b inet 192.0.2.2/24 up
	jexec one ifconfig bridge0 create addm ${ep}b

	atf_check -s exit:0 -o ignore ping -c3 -t1 192.0.2.2
}

member_ifaddrs_enabled_cleanup()
{
	vnet_cleanup
}

atf_test_case "member_ifaddrs_disabled" "cleanup"
member_ifaddrs_disabled_head()
{
	atf_set descr 'bridge with member_ifaddrs=0'
	atf_set require.user root
}

member_ifaddrs_disabled_body()
{
	vnet_init
	vnet_init_bridge

	vnet_mkjail one
	jexec one sysctl net.link.bridge.member_ifaddrs=0

	bridge=$(jexec one ifconfig bridge create)

	# adding an interface with an IPv4 address
	ep=$(jexec one ifconfig epair create)
	jexec one ifconfig ${ep} 192.0.2.1/32
	atf_check -s exit:1 -e ignore jexec one ifconfig ${bridge} addm ${ep}

	# adding an interface with an IPv6 address
	ep=$(jexec one ifconfig epair create)
	jexec one ifconfig ${ep} inet6 2001:db8::1/128
	atf_check -s exit:1 -e ignore jexec one ifconfig ${bridge} addm ${ep}

	# adding an interface with an IPv6 link-local address
	ep=$(jexec one ifconfig epair create)
	jexec one ifconfig ${ep} inet6 -ifdisabled auto_linklocal up
	atf_check -s exit:1 -e ignore jexec one ifconfig ${bridge} addm ${ep}

	# adding an IPv4 address to a member
	ep=$(jexec one ifconfig epair create)
	jexec one ifconfig ${bridge} addm ${ep}
	atf_check -s exit:1 -e ignore jexec one ifconfig ${ep} inet 192.0.2.2/32

	# adding an IPv6 address to a member
	ep=$(jexec one ifconfig epair create)
	jexec one ifconfig ${bridge} addm ${ep}
	atf_check -s exit:1 -e ignore jexec one ifconfig ${ep} inet6 2001:db8::1/128
}

member_ifaddrs_disabled_cleanup()
{
	vnet_cleanup
}

#
# Test kern/287150: when member_ifaddrs=0, and a physical interface which is in
# a bridge also has a vlan(4) on it, tagged packets are not correctly passed to
# vlan(4).
atf_test_case "member_ifaddrs_vlan" "cleanup"
member_ifaddrs_vlan_head()
{
	atf_set descr 'kern/287150: vlan and bridge on the same interface'
	atf_set require.user root
}

member_ifaddrs_vlan_body()
{
	vnet_init
	vnet_init_bridge
	_vnet_check_req vlan

	epone=$(vnet_mkepair)
	eptwo=$(vnet_mkepair)

	# The first jail has an epair with an IP address on vlan 20.
	vnet_mkjail one ${epone}a
	atf_check -s exit:0 jexec one ifconfig ${epone}a up
	atf_check -s exit:0 jexec one \
	    ifconfig ${epone}a.20 create inet 192.0.2.1/24 up

	# The second jail has an epair with an IP address on vlan 20,
	# which is also in a bridge.
	vnet_mkjail two ${epone}b

	jexec two ifconfig
	atf_check -s exit:0 -o save:bridge jexec two ifconfig bridge create
	bridge=$(cat bridge)
	atf_check -s exit:0 jexec two ifconfig ${bridge} addm ${epone}b up

	atf_check -s exit:0 -o ignore jexec two \
	    sysctl net.link.bridge.member_ifaddrs=0
	atf_check -s exit:0 jexec two ifconfig ${epone}b up
	atf_check -s exit:0 jexec two \
	    ifconfig ${epone}b.20 create inet 192.0.2.2/24 up

	# Make sure the two jails can communicate over the vlan.
	atf_check -s exit:0 -o ignore jexec one ping -c 3 -t 1 192.0.2.2
	atf_check -s exit:0 -o ignore jexec two ping -c 3 -t 1 192.0.2.1
}

member_ifaddrs_vlan_cleanup()
{
	vnet_cleanup
}

atf_test_case "vlan_pvid" "cleanup"
vlan_pvid_head()
{
	atf_set descr 'bridge with two ports with pvid and vlanfilter set'
	atf_set require.user root
}

vlan_pvid_body()
{
	vnet_init
	vnet_init_bridge

	epone=$(vnet_mkepair)
	eptwo=$(vnet_mkepair)

	vnet_mkjail one ${epone}b
	vnet_mkjail two ${eptwo}b

	jexec one ifconfig ${epone}b 192.0.2.1/24 up
	jexec two ifconfig ${eptwo}b 192.0.2.2/24 up

	bridge=$(vnet_mkbridge)

	ifconfig ${bridge} vlanfilter up
	ifconfig ${epone}a up
	ifconfig ${eptwo}a up
	ifconfig ${bridge} addm ${epone}a untagged 20
	ifconfig ${bridge} addm ${eptwo}a untagged 20

	# With VLAN filtering enabled, traffic should be passed.
	atf_check -s exit:0 -o ignore jexec one ping -c 3 -t 1 192.0.2.2
	atf_check -s exit:0 -o ignore jexec two ping -c 3 -t 1 192.0.2.1

	# Removed the untagged VLAN on one port; traffic should not be passed.
	ifconfig ${bridge} -ifuntagged ${epone}a
	atf_check -s exit:2 -o ignore jexec one ping -c 3 -t 1 192.0.2.2
	atf_check -s exit:2 -o ignore jexec two ping -c 3 -t 1 192.0.2.1
}

vlan_pvid_cleanup()
{
	vnet_cleanup
}

atf_test_case "vlan_pvid_filtered" "cleanup"
vlan_pvid_filtered_head()
{
	atf_set descr 'bridge with two ports with different pvids'
	atf_set require.user root
}

vlan_pvid_filtered_body()
{
	vnet_init
	vnet_init_bridge

	epone=$(vnet_mkepair)
	eptwo=$(vnet_mkepair)

	vnet_mkjail one ${epone}b
	vnet_mkjail two ${eptwo}b

	atf_check -s exit:0 jexec one ifconfig ${epone}b 192.0.2.1/24 up
	atf_check -s exit:0 jexec two ifconfig ${eptwo}b 192.0.2.2/24 up

	bridge=$(vnet_mkbridge)

	atf_check -s exit:0 ifconfig ${bridge} vlanfilter up
	atf_check -s exit:0 ifconfig ${epone}a up
	atf_check -s exit:0 ifconfig ${eptwo}a up
	atf_check -s exit:0 ifconfig ${bridge} addm ${epone}a untagged 20
	atf_check -s exit:0 ifconfig ${bridge} addm ${eptwo}a untagged 30

	atf_check -s exit:2 -o ignore jexec one ping -c 3 -t 1 192.0.2.2
	atf_check -s exit:2 -o ignore jexec two ping -c 3 -t 1 192.0.2.1
}

vlan_pvid_filtered_cleanup()
{
	vnet_cleanup
}

atf_test_case "vlan_pvid_tagged" "cleanup"
vlan_pvid_tagged_head()
{
	atf_set descr 'bridge pvid with tagged frames for pvid'
	atf_set require.user root
}

vlan_pvid_tagged_body()
{
	vnet_init
	vnet_init_bridge
	_vnet_check_req vlan

	epone=$(vnet_mkepair)
	eptwo=$(vnet_mkepair)

	vnet_mkjail one ${epone}b
	vnet_mkjail two ${eptwo}b

	# Create two tagged interfaces on the appropriate VLANs
	atf_check -s exit:0 jexec one ifconfig ${epone}b up
	atf_check -s exit:0 jexec one ifconfig ${epone}b.20 \
	    create 192.0.2.1/24 up
	atf_check -s exit:0 jexec two ifconfig ${eptwo}b up
	atf_check -s exit:0 jexec two ifconfig ${eptwo}b.20 \
	    create 192.0.2.2/24 up

	bridge=$(vnet_mkbridge)

	atf_check -s exit:0 ifconfig ${bridge} vlanfilter up
	atf_check -s exit:0 ifconfig ${epone}a up
	atf_check -s exit:0 ifconfig ${eptwo}a up
	atf_check -s exit:0 ifconfig ${bridge} addm ${epone}a untagged 20
	atf_check -s exit:0 ifconfig ${bridge} addm ${eptwo}a untagged 20

	# Tagged frames should not be passed.
	atf_check -s exit:2 -o ignore jexec one ping -c 3 -t 1 192.0.2.2
	atf_check -s exit:2 -o ignore jexec two ping -c 3 -t 1 192.0.2.1
}

vlan_pvid_tagged_cleanup()
{
	vnet_cleanup
}

atf_test_case "vlan_pvid_1q" "cleanup"
vlan_pvid_1q_head()
{
	atf_set descr '802.1q tag addition and removal'
	atf_set require.user root
}

vlan_pvid_1q_body()
{
	vnet_init
	vnet_init_bridge
	_vnet_check_req vlan

	epone=$(vnet_mkepair)
	eptwo=$(vnet_mkepair)

	vnet_mkjail one ${epone}b
	vnet_mkjail two ${eptwo}b

	# Set up one jail with an access port, and the other with a trunk port.
	# This forces the bridge to add and remove .1q tags to bridge the
	# traffic.

	atf_check -s exit:0 jexec one ifconfig ${epone}b 192.0.2.1/24 up
	atf_check -s exit:0 jexec two ifconfig ${eptwo}b up
	atf_check -s exit:0 jexec two ifconfig ${eptwo}b.20 create 192.0.2.2/24 up

	bridge=$(vnet_mkbridge)

	atf_check -s exit:0 ifconfig ${bridge} vlanfilter up
	atf_check -s exit:0 ifconfig ${bridge} addm ${epone}a untagged 20
	atf_check -s exit:0 ifconfig ${bridge} addm ${eptwo}a tagged 20

	atf_check -s exit:0 ifconfig ${epone}a up
	atf_check -s exit:0 ifconfig ${eptwo}a up

	atf_check -s exit:0 -o ignore jexec one ping -c 3 -t 1 192.0.2.2
	atf_check -s exit:0 -o ignore jexec two ping -c 3 -t 1 192.0.2.1
}

vlan_pvid_1q_cleanup()
{
       vnet_cleanup
}

#
# Test vlan filtering.
#
atf_test_case "vlan_filtering" "cleanup"
vlan_filtering_head()
{
	atf_set descr 'tagged traffic with filtering'
	atf_set require.user root
}

vlan_filtering_body()
{
	vnet_init
	vnet_init_bridge
	_vnet_check_req vlan

	epone=$(vnet_mkepair)
	eptwo=$(vnet_mkepair)

	vnet_mkjail one ${epone}b
	vnet_mkjail two ${eptwo}b

	atf_check -s exit:0 jexec one ifconfig ${epone}b up
	atf_check -s exit:0 jexec one ifconfig ${epone}b.20 \
	    create 192.0.2.1/24 up
	atf_check -s exit:0 jexec two ifconfig ${eptwo}b up
	atf_check -s exit:0 jexec two ifconfig ${eptwo}b.20 \
	    create 192.0.2.2/24 up

	bridge=$(vnet_mkbridge)

	atf_check -s exit:0 ifconfig ${bridge} vlanfilter up
	atf_check -s exit:0 ifconfig ${epone}a up
	atf_check -s exit:0 ifconfig ${eptwo}a up
	atf_check -s exit:0 ifconfig ${bridge} addm ${epone}a
	atf_check -s exit:0 ifconfig ${bridge} addm ${eptwo}a

	# Right now there are no VLANs on the access list, so everything
	# should be blocked.
	atf_check -s exit:2 -o ignore jexec one ping -c 3 -t 1 192.0.2.2
	atf_check -s exit:2 -o ignore jexec two ping -c 3 -t 1 192.0.2.1

	# Set the untagged vlan on both ports to 20 and make sure traffic is
	# still blocked.  We intentionally do not pass tagged traffic for the
	# untagged vlan.
	atf_check -s exit:0 ifconfig ${bridge} ifuntagged ${epone}a 20
	atf_check -s exit:0 ifconfig ${bridge} ifuntagged ${eptwo}a 20

	atf_check -s exit:2 -o ignore jexec one ping -c 3 -t 1 192.0.2.2
	atf_check -s exit:2 -o ignore jexec two ping -c 3 -t 1 192.0.2.1

	atf_check -s exit:0 ifconfig ${bridge} -ifuntagged ${epone}a
	atf_check -s exit:0 ifconfig ${bridge} -ifuntagged ${eptwo}a

	# Add VLANs 10-30 to the access list; now access should be allowed.
	atf_check -s exit:0 ifconfig ${bridge} +iftagged ${epone}a 10-30
	atf_check -s exit:0 ifconfig ${bridge} +iftagged ${eptwo}a 10-30
	atf_check -s exit:0 -o ignore jexec one ping -c 3 -t 1 192.0.2.2
	atf_check -s exit:0 -o ignore jexec two ping -c 3 -t 1 192.0.2.1

	# Remove vlan 20 from the access list, now access should be blocked
	# again.
	atf_check -s exit:0 ifconfig ${bridge} -iftagged ${epone}a 20
	atf_check -s exit:0 ifconfig ${bridge} -iftagged ${eptwo}a 20
	atf_check -s exit:2 -o ignore jexec one ping -c 3 -t 1 192.0.2.2
	atf_check -s exit:2 -o ignore jexec two ping -c 3 -t 1 192.0.2.1
}

vlan_filtering_cleanup()
{
	vnet_cleanup
}

#
# Test the ifconfig 'iftagged' option.
#
atf_test_case "vlan_ifconfig_iftagged" "cleanup"
vlan_ifconfig_iftagged_head()
{
	atf_set descr 'test the ifconfig iftagged option'
	atf_set require.user root
}

vlan_ifconfig_iftagged_body()
{
	vnet_init
	vnet_init_bridge

	ep=$(vnet_mkepair)
	bridge=$(vnet_mkbridge)
	atf_check -s exit:0 ifconfig ${bridge} vlanfilter up

	atf_check -s exit:0 ifconfig ${bridge} addm ${ep}a
	atf_check -s exit:0 ifconfig ${ep}a up

	# To start with, no vlans should be configured.
	atf_check -s exit:0 -o not-match:"tagged" ifconfig ${bridge}

	# Add vlans 100-149.
	atf_check -s exit:0 ifconfig ${bridge} iftagged ${ep}a 100-149
	atf_check -s exit:0 -o match:"tagged 100-149" ifconfig ${bridge}

	# Replace the vlan list with 139-199.
	atf_check -s exit:0 ifconfig ${bridge} iftagged ${ep}a 139-199
	atf_check -s exit:0 -o match:"tagged 139-199" ifconfig ${bridge}

	# Add vlans 100-170.
	atf_check -s exit:0 ifconfig ${bridge} +iftagged ${ep}a 100-170
	atf_check -s exit:0 -o match:"tagged 100-199" ifconfig ${bridge}

	# Remove vlans 104, 105, and 150-159
	atf_check -s exit:0 ifconfig ${bridge} -iftagged ${ep}a 104,105,150-159
	atf_check -s exit:0 -o match:"tagged 100-103,106-149,160-199" \
	    ifconfig ${bridge}

	# Remove the entire vlan list.
	atf_check -s exit:0 ifconfig ${bridge} iftagged ${ep}a none
	atf_check -s exit:0 -o not-match:"tagged" ifconfig ${bridge}

	# Test some invalid vlans sets.
	for bad_vlan in -1 0 4096 4097 foo 0-10 4000-5000 foo-40 40-foo; do
		atf_check -s exit:1 -e ignore \
		    ifconfig ${bridge} iftagged "$bad_vlan"
	done
}

vlan_ifconfig_iftagged_cleanup()
{
	vnet_cleanup
}

#
# Test a vlan(4) "SVI" interface on top of a bridge.
#
atf_test_case "vlan_svi" "cleanup"
vlan_svi_head()
{
	atf_set descr 'vlan bridge with an SVI'
	atf_set require.user root
}

vlan_svi_body()
{
	vnet_init
	vnet_init_bridge
	_vnet_check_req vlan

	epone=$(vnet_mkepair)

	vnet_mkjail one ${epone}b

	atf_check -s exit:0 jexec one ifconfig ${epone}b up
	atf_check -s exit:0 jexec one ifconfig ${epone}b.20 \
	    create 192.0.2.1/24 up

	bridge=$(vnet_mkbridge)

	atf_check -s exit:0 ifconfig ${bridge} vlanfilter up
	atf_check -s exit:0 ifconfig ${epone}a up
	atf_check -s exit:0 ifconfig ${bridge} addm ${epone}a tagged 20

	svi=$(vnet_mkvlan)
	atf_check -s exit:0 ifconfig ${svi} vlan 20 vlandev ${bridge}
	atf_check -s exit:0 ifconfig ${svi} inet 192.0.2.2/24 up

	atf_check -s exit:0 -o ignore ping -c 3 -t 1 192.0.2.1
}

vlan_svi_cleanup()
{
	vnet_cleanup
}

#
# Test QinQ (802.1ad).
#
atf_test_case "vlan_qinq" "cleanup"
vlan_qinq_head()
{
	atf_set descr 'vlan filtering with QinQ traffic'
	atf_set require.user root
}

vlan_qinq_body()
{
	vnet_init
	vnet_init_bridge
	_vnet_check_req vlan

	epone=$(vnet_mkepair)
	eptwo=$(vnet_mkepair)

	vnet_mkjail one ${epone}b
	vnet_mkjail two ${eptwo}b

	# Create a QinQ trunk between the two jails.  The outer (provider) tag
	# is 5, and the inner tag is 10.

	atf_check -s exit:0 jexec one ifconfig ${epone}b up
	atf_check -s exit:0 jexec one \
	    ifconfig ${epone}b.5 create vlanproto 802.1ad up
	atf_check -s exit:0 jexec one \
	    ifconfig ${epone}b.5.10 create inet 192.0.2.1/24 up

	atf_check -s exit:0 jexec two ifconfig ${eptwo}b up
	atf_check -s exit:0 jexec two ifconfig \
	    ${eptwo}b.5 create vlanproto 802.1ad up
	atf_check -s exit:0 jexec two ifconfig \
	    ${eptwo}b.5.10 create inet 192.0.2.2/24 up

	bridge=$(vnet_mkbridge)

	atf_check -s exit:0 ifconfig ${bridge} vlanfilter defqinq up
	atf_check -s exit:0 ifconfig ${epone}a up
	atf_check -s exit:0 ifconfig ${eptwo}a up
	atf_check -s exit:0 ifconfig ${bridge} addm ${epone}a
	atf_check -s exit:0 ifconfig ${bridge} addm ${eptwo}a

	# Right now there are no VLANs on the access list, so everything
	# should be blocked.
	atf_check -s exit:2 -o ignore jexec one ping -c 3 -t 1 192.0.2.2
	atf_check -s exit:2 -o ignore jexec two ping -c 3 -t 1 192.0.2.1

	# Add the provider tag to the access list; now traffic should be passed.
	atf_check -s exit:0 ifconfig ${bridge} +iftagged ${epone}a 5
	atf_check -s exit:0 ifconfig ${bridge} +iftagged ${eptwo}a 5
	atf_check -s exit:0 -o ignore jexec one ping -c 3 -t 1 192.0.2.2
	atf_check -s exit:0 -o ignore jexec two ping -c 3 -t 1 192.0.2.1

	# Remove the qinq flag from one of the interfaces; traffic should
	# be blocked again.
	atf_check -s exit:0 ifconfig ${bridge} -qinq ${epone}a
	atf_check -s exit:2 -o ignore jexec one ping -c 3 -t 1 192.0.2.2
	atf_check -s exit:2 -o ignore jexec two ping -c 3 -t 1 192.0.2.1
}

vlan_qinq_cleanup()
{
	vnet_cleanup
}

# Adding a bridge SVI to a bridge should not be allowed.
atf_test_case "bridge_svi_in_bridge" "cleanup"
bridge_svi_in_bridge_head()
{
	atf_set descr 'adding a bridge SVI to a bridge is not allowed (1)'
	atf_set require.user root
}

bridge_svi_in_bridge_body()
{
	vnet_init
	vnet_init_bridge
	_vnet_check_req vlan

	bridge=$(vnet_mkbridge)
	atf_check -s exit:0 ifconfig ${bridge}.1 create
	atf_check -s exit:1 -e ignore ifconfig ${bridge} addm ${bridge}.1
}

bridge_svi_in_bridge_cleanup()
{
	vnet_cleanup
}

atf_test_case "vlan_untagged" "cleanup"
vlan_untagged_head()
{
	atf_set descr 'bridge with two ports with untagged set'
	atf_set require.user root
}

vlan_untagged_body()
{
	vnet_init
	vnet_init_bridge

	epone=$(vnet_mkepair)
	eptwo=$(vnet_mkepair)

	vnet_mkjail one ${epone}b
	vnet_mkjail two ${eptwo}b

	jexec one ifconfig ${epone}b 192.0.2.1/24 up
	jexec two ifconfig ${eptwo}b 192.0.2.2/24 up

	bridge=$(vnet_mkbridge)

	ifconfig ${bridge} up
	ifconfig ${epone}a up
	ifconfig ${eptwo}a up
	ifconfig ${bridge} addm ${epone}a untagged 20
	ifconfig ${bridge} addm ${eptwo}a untagged 30

	# With two ports on different VLANs, traffic should not be passed.
	atf_check -s exit:2 -o ignore jexec one ping -c 3 -t 1 192.0.2.2
	atf_check -s exit:2 -o ignore jexec two ping -c 3 -t 1 192.0.2.1

	# Move the second port to VLAN 20; now traffic should be passed.
	atf_check -s exit:0 ifconfig ${bridge} ifuntagged ${eptwo}a 20
	atf_check -s exit:0 -o ignore jexec one ping -c 3 -t 1 192.0.2.2
	atf_check -s exit:0 -o ignore jexec two ping -c 3 -t 1 192.0.2.1

	# Remove the first's port untagged config, now traffic should
	# not pass again.
	atf_check -s exit:0 ifconfig ${bridge} -ifuntagged ${epone}a
	atf_check -s exit:2 -o ignore jexec one ping -c 3 -t 1 192.0.2.2
	atf_check -s exit:2 -o ignore jexec two ping -c 3 -t 1 192.0.2.1
}

vlan_untagged_cleanup()
{
	vnet_cleanup
}

atf_test_case "vlan_defuntagged" "cleanup"
vlan_defuntagged_head()
{
	atf_set descr 'defuntagged (defpvid) bridge option'
	atf_set require.user root
}

vlan_defuntagged_body()
{
	vnet_init
	vnet_init_bridge

	bridge=$(vnet_mkbridge)

	# Invalid VLAN IDs
	atf_check -s exit:1 -ematch:"invalid vlan id: 0" \
		ifconfig ${bridge} defuntagged 0
	atf_check -s exit:1 -ematch:"invalid vlan id: 4095" \
		ifconfig ${bridge} defuntagged 4095
	atf_check -s exit:1 -ematch:"invalid vlan id: 5000" \
		ifconfig ${bridge} defuntagged 5000

	# Check the bridge option is set and cleared correctly
	atf_check -s exit:0 -onot-match:"defuntagged=" \
		ifconfig ${bridge}

	atf_check -s exit:0 ifconfig ${bridge} defuntagged 10
	atf_check -s exit:0 -omatch:"defuntagged=10$" \
		ifconfig ${bridge}

	atf_check -s exit:0 ifconfig ${bridge} -defuntagged
	atf_check -s exit:0 -onot-match:"defuntagged=" \
		ifconfig ${bridge}

	# Check the untagged option is correctly set on a member
	atf_check -s exit:0 ifconfig ${bridge} defuntagged 10

	epair=$(vnet_mkepair)
	atf_check -s exit:0 ifconfig ${bridge} addm ${epair}a

	tag=$(ifconfig ${bridge} | sed -Ene \
		"/member: ${epair}a/ { N;s/.*untagged ([0-9]+).*/\\1/p;q; }")
	if [ "$tag" != "10" ]; then
		atf_fail "wrong untagged vlan: ${tag}"
	fi
}

vlan_defuntagged_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "bridge_transmit_ipv4_unicast"
	atf_add_test_case "stp"
	atf_add_test_case "stp_vlan"
	atf_add_test_case "static"
	atf_add_test_case "vstatic"
	atf_add_test_case "span"
	atf_add_test_case "inherit_mac"
	atf_add_test_case "delete_with_members"
	atf_add_test_case "mac_conflict"
	atf_add_test_case "stp_validation"
	atf_add_test_case "gif"
	atf_add_test_case "mtu"
	atf_add_test_case "vlan"
	atf_add_test_case "many_bridge_members"
	atf_add_test_case "member_ifaddrs_enabled"
	atf_add_test_case "member_ifaddrs_disabled"
	atf_add_test_case "member_ifaddrs_vlan"
	atf_add_test_case "vlan_pvid"
	atf_add_test_case "vlan_pvid_1q"
	atf_add_test_case "vlan_pvid_filtered"
	atf_add_test_case "vlan_pvid_tagged"
	atf_add_test_case "vlan_filtering"
	atf_add_test_case "vlan_ifconfig_iftagged"
	atf_add_test_case "vlan_svi"
	atf_add_test_case "vlan_qinq"
	atf_add_test_case "vlan_untagged"
	atf_add_test_case "vlan_defuntagged"
	atf_add_test_case "bridge_svi_in_bridge"
}
