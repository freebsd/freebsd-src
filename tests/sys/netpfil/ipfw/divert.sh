#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2023 Igor Ostapenko <pm@igoro.pro>
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

#
# ipfw divert action test cases
#
#  -----------|    |--     |----|     ----|    |-----------
# ( ) inbound |ipfw|  ) -> |host| -> ( )  |ipfw| outbound  )
#  -----------| |  |--     |----|     ----|  | |-----------
#               |                            |
#              \|/                          \|/
#           |------|                      |------|
#           |divapp|                      |divapp|
#           |------|                      |------|
#
# The basic cases:
#   - inbound > diverted               | divapp terminated
#   - inbound > diverted > inbound     | host terminated
#   - inbound > diverted > outbound    | network terminated
#   - outbound > diverted              | divapp terminated
#   - outbound > diverted > outbound   | network terminated
#   - outbound > diverted > inbound    | e.g. host terminated
#
# When a packet is diverted, forwarded, and possibly diverted again:
#   - inbound > diverted > inbound > forwarded
#         > outbound                       | network terminated
#   - inbound > diverted > inbound > forwarded
#         > outbound > diverted > outbound | network terminated
#
# Test case naming legend:
# in - inbound
# div - diverted
# out - outbound
# fwd - forwarded
#

. $(atf_get_srcdir)/../common/utils.subr

divert_init()
{
	if ! kldstat -q -m ipdivert; then
		atf_skip "This test requires ipdivert"
	fi
}

atf_test_case "in_div" "cleanup"
in_div_head()
{
	atf_set descr 'Test inbound > diverted | divapp terminated'
	atf_set require.user root
}
in_div_body()
{
	firewall_init "ipfw"
	divert_init

	epair=$(vnet_mkepair)
	vnet_mkjail div ${epair}b
	ifconfig ${epair}a 192.0.2.1/24 up
	jexec div ifconfig ${epair}b 192.0.2.2/24 up
	jexec div ipfw add 65534 allow all from any to any

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c3 192.0.2.2

	jexec div ipfw add 100 divert 2000 icmp from any to any in icmptypes 8

	jexec div $(atf_get_srcdir)/../common/divapp 2000 &
	divapp_pid=$!
	# Wait for the divapp to be ready
	sleep 1

	# divapp is expected to "eat" the packet
	atf_check -s not-exit:0 -o ignore ping -c1 -t1 192.0.2.2

	wait $divapp_pid
}
in_div_cleanup()
{
	firewall_cleanup "ipfw"
}

atf_test_case "in_div_in" "cleanup"
in_div_in_head()
{
	atf_set descr 'Test inbound > diverted > inbound | host terminated'
	atf_set require.user root
}
in_div_in_body()
{
	firewall_init "ipfw"
	divert_init

	epair=$(vnet_mkepair)
	vnet_mkjail div ${epair}b
	ifconfig ${epair}a 192.0.2.1/24 up
	jexec div ifconfig ${epair}b 192.0.2.2/24 up
	jexec div ipfw add 65534 allow all from any to any

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c3 192.0.2.2

	jexec div ipfw add 100 divert 2000 icmp from any to any in icmptypes 8

	jexec div $(atf_get_srcdir)/../common/divapp 2000 divert-back &
	divapp_pid=$!
	# Wait for the divapp to be ready
	sleep 1

	# divapp is NOT expected to "eat" the packet
	atf_check -s exit:0 -o ignore ping -c1 192.0.2.2

	wait $divapp_pid
}
in_div_in_cleanup()
{
	firewall_cleanup "ipfw"
}

atf_test_case "out_div" "cleanup"
out_div_head()
{
	atf_set descr 'Test outbound > diverted | divapp terminated'
	atf_set require.user root
}
out_div_body()
{
	firewall_init "ipfw"
	divert_init

	epair=$(vnet_mkepair)
	vnet_mkjail div ${epair}b
	ifconfig ${epair}a 192.0.2.1/24 up
	jexec div ifconfig ${epair}b 192.0.2.2/24 up
	jexec div ipfw add 65534 allow all from any to any

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c3 192.0.2.2

	jexec div ipfw add 100 divert 2000 icmp from any to any out icmptypes 0

	jexec div $(atf_get_srcdir)/../common/divapp 2000 &
	divapp_pid=$!
	# Wait for the divapp to be ready
	sleep 1

	# divapp is expected to "eat" the packet
	atf_check -s not-exit:0 -o ignore ping -c1 -t1 192.0.2.2

	wait $divapp_pid
}
out_div_cleanup()
{
	firewall_cleanup "ipfw"
}

atf_test_case "out_div_out" "cleanup"
out_div_out_head()
{
	atf_set descr 'Test outbound > diverted > outbound | network terminated'
	atf_set require.user root
}
out_div_out_body()
{
	firewall_init "ipfw"
	divert_init

	epair=$(vnet_mkepair)
	vnet_mkjail div ${epair}b
	ifconfig ${epair}a 192.0.2.1/24 up
	jexec div ifconfig ${epair}b 192.0.2.2/24 up
	jexec div ipfw add 65534 allow all from any to any

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c3 192.0.2.2

	jexec div ipfw add 100 divert 2000 icmp from any to any out icmptypes 0

	jexec div $(atf_get_srcdir)/../common/divapp 2000 divert-back &
	divapp_pid=$!
	# Wait for the divapp to be ready
	sleep 1

	# divapp is NOT expected to "eat" the packet
	atf_check -s exit:0 -o ignore ping -c1 192.0.2.2

	wait $divapp_pid
}
out_div_out_cleanup()
{
	firewall_cleanup "ipfw"
}

atf_test_case "in_div_in_fwd_out_div_out" "cleanup"
in_div_in_fwd_out_div_out_head()
{
	atf_set descr 'Test inbound > diverted > inbound > forwarded > outbound > diverted > outbound | network terminated'
	atf_set require.user root
}
in_div_in_fwd_out_div_out_body()
{
	firewall_init "ipfw"
	divert_init

	# host <a--epair0--b> router <a--epair1--b> site
	epair0=$(vnet_mkepair)
	epair1=$(vnet_mkepair)

	vnet_mkjail router ${epair0}b ${epair1}a
	ifconfig ${epair0}a 192.0.2.1/24 up
	jexec router sysctl net.inet.ip.forwarding=1
	jexec router ifconfig ${epair0}b 192.0.2.2/24 up
	jexec router ifconfig ${epair1}a 198.51.100.1/24 up
	jexec router ipfw add 65534 allow all from any to any

	vnet_mkjail site ${epair1}b
	jexec site ifconfig ${epair1}b 198.51.100.2/24 up
	jexec site ipfw add 65534 allow all from any to any
	jexec site route add default 198.51.100.1

	route add -net 198.51.100.0/24 192.0.2.2

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c3 192.0.2.2

	# Should be routed without diversion
	atf_check -s exit:0 -o ignore ping -c3 198.51.100.2

	jexec router ipfw add 100 divert 2001 icmp from any to any in icmptypes 8
	jexec router ipfw add 200 divert 2002 icmp from any to any out icmptypes 8

	jexec router $(atf_get_srcdir)/../common/divapp 2001 divert-back &
	indivapp_pid=$!
	jexec router $(atf_get_srcdir)/../common/divapp 2002 divert-back &
	outdivapp_pid=$!
	# Wait for the divappS to be ready
	sleep 1

	# Both divappS are NOT expected to "eat" the packet
	atf_check -s exit:0 -o ignore ping -c1 198.51.100.2

	wait $indivapp_pid && wait $outdivapp_pid
}
in_div_in_fwd_out_div_out_cleanup()
{
	firewall_cleanup "ipfw"
}

atf_init_test_cases()
{
	atf_add_test_case "in_div"
	atf_add_test_case "in_div_in"

	atf_add_test_case "out_div"
	atf_add_test_case "out_div_out"

	atf_add_test_case "in_div_in_fwd_out_div_out"
}
