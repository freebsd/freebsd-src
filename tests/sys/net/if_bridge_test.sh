# $FreeBSD$
#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
}

stp_cleanup()
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
	atf_check -s exit:0 -o ignore \
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

atf_test_case "span" "cleanup"
span_head()
{
	atf_set descr 'Bridge span test'
	atf_set require.user root
}

span_body()
{
	set -x
	vnet_init

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

atf_init_test_cases()
{
	atf_add_test_case "bridge_transmit_ipv4_unicast"
	atf_add_test_case "stp"
	atf_add_test_case "static"
	atf_add_test_case "span"
	atf_add_test_case "inherit_mac"
	atf_add_test_case "delete_with_members"
	atf_add_test_case "mac_conflict"
}
