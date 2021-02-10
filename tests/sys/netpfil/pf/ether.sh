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

atf_init_test_cases()
{
	atf_add_test_case "mac"
	atf_add_test_case "proto"
}
