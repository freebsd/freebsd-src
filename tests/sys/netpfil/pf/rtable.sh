# $FreeBSD$
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2023 Kajetan Staszkiewicz <vegetga@tuxpowered.net>
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

atf_test_case "forward_v4" "cleanup"
forward_v4_head()
{
	atf_set descr 'Test IPv4 forwarding with rtable'
	atf_set require.user root
	atf_set require.progs scapy
}

forward_v4_body()
{
	setup_router_server_ipv4

	# Sanity check
	ping_server_check_reply exit:0

	jexec router sysctl net.fibs=2
	jexec router ifconfig ${epair_server}a fib 1
	jexec router route del -net ${net_server}
	jexec router route add -fib 1 -net ${net_server} -iface ${epair_server}a

	# Sanity check
	ping_server_check_reply exit:1

	# This rule is not enough.
	# Echo requests will be properly forwarded but replies can't be routed back.
	pft_set_rules router \
		"pass in on ${epair_tester}b inet proto icmp all icmp-type echoreq rtable 1"
	ping_server_check_reply exit:1

	# Allow replies coming back to the tester properly via stateful filtering post-routing.
	pft_set_rules router \
		"pass in  on ${epair_tester}b inet proto icmp all icmp-type echoreq rtable 1" \
		"pass out on ${epair_server}a inet proto icmp all icmp-type echoreq rtable 0"
	ping_server_check_reply exit:0

	# Allow replies coming back to the tester properly via provding extra routes in rtable 1
	pft_set_rules router \
		"pass in  on ${epair_tester}b inet proto icmp all icmp-type echoreq rtable 1"
	jexec router route add -fib 1 -net ${net_tester} -iface ${epair_tester}b
	ping_server_check_reply exit:0
}

forward_v4_cleanup()
{
	pft_cleanup
}

atf_test_case "forward_v6" "cleanup"
forward_v6_head()
{
	atf_set descr 'Test IPv6 forwarding with rtable'
	atf_set require.user root
	atf_set require.progs scapy
}

forward_v6_body()
{
	setup_router_server_ipv6

	# Sanity check
	ping_server_check_reply exit:0

	jexec router sysctl net.fibs=2
	jexec router ifconfig ${epair_server}a fib 1
	jexec router route del -6 ${net_server}
	jexec router route add -fib 1 -6 ${net_server} -iface ${epair_server}a

	# Sanity check
	ping_server_check_reply exit:1

	# This rule is not enough.
	# Echo requests will be properly forwarded but replies can't be routed back.
	pft_set_rules router \
		"pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
		"pass in on ${epair_tester}b inet6 proto icmp6 icmp6-type echoreq"
	ping_server_check_reply exit:1

	# Allow replies coming back to the tester properly via stateful filtering post-routing.
	pft_set_rules router \
		"pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
		"pass in  on ${epair_tester}b inet6 proto icmp6 icmp6-type echoreq rtable 1" \
		"pass out on ${epair_server}a inet6 proto icmp6 icmp6-type echoreq rtable 0"
	ping_server_check_reply exit:0

	# Allow replies coming back to the tester properly via provding extra routes in rtable 1
	pft_set_rules router \
		"pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
		"pass in  on ${epair_tester}b inet6 proto icmp6 icmp6-type echoreq rtable 1"
	jexec router route add -fib 1 -6 ${net_tester} -iface ${epair_tester}b
	ping_server_check_reply exit:0
}

forward_v6_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "forward_v4"
	atf_add_test_case "forward_v6"
}
