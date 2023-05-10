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

atf_test_case "modulate_v4" "cleanup"
modulate_v4_head()
{
	atf_set descr 'IPv4 TCP sequence number modulation'
	atf_set require.user root
	atf_set require.progs scapy
}

modulate_v4_body()
{
	setup_router_dummy_ipv4

	pft_set_rules router \
		"pass in on ${epair_tester}b modulate state"
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-seq 42 # Sanity check
	ping_dummy_check_request exit:1 --ping-type=tcpsyn --send-seq 42 --expect-seq 42
}

modulate_v4_cleanup()
{
	pft_cleanup
}

atf_test_case "modulate_v6" "cleanup"
modulate_v6_head()
{
	atf_set descr 'IPv6 TCP sequence number modulation'
	atf_set require.user root
	atf_set require.progs scapy
}

modulate_v6_body()
{
	setup_router_dummy_ipv6

	pft_set_rules router \
		"pass in on ${epair_tester}b modulate state"
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-seq 42 # Sanity check
	ping_dummy_check_request exit:1 --ping-type=tcpsyn --send-seq 42 --expect-seq 42
}

modulate_v6_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "modulate_v4"
	atf_add_test_case "modulate_v6"
}
