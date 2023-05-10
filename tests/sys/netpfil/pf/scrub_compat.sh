# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Kristof Provost <kp@FreeBSD.org>
# Copyright (c) 2023 Kajetan Staszkiewicz <vegeta@tuxpowered.net>
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

atf_test_case "max_mss_v4" "cleanup"
max_mss_v4_head()
{
	atf_set descr 'Test IPv4 scrub "mss" rule'
	atf_set require.user root
	atf_set require.progs scapy
}

max_mss_v4_body()
{
	setup_router_dummy_ipv4
	pft_set_rules router "scrub on ${epair_tester}b max-mss 1300"
	# Check aligned
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-mss=1400 --expect-mss=1300
	# And unaligned
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-mss=1400 --expect-mss=1300 \
	    --send-tcpopt-unaligned
}

max_mss_v4_cleanup()
{
	pft_cleanup
}


atf_test_case "max_mss_v6" "cleanup"
max_mss_v6_head()
{
	atf_set descr 'Test IPv6 scrub "mss" rule'
	atf_set require.user root
	atf_set require.progs scapy
}

max_mss_v6_body()
{
	setup_router_dummy_ipv6
	pft_set_rules router "scrub on ${epair_tester}b max-mss 1300"
	# Check aligned
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-mss=1400 --expect-mss=1300
	# And unaligned
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-mss=1400 --expect-mss=1300 \
	    --send-tcpopt-unaligned
}

max_mss_v6_cleanup()
{
	pft_cleanup
}


atf_test_case "set_tos_v4" "cleanup"
set_tos_v4_head()
{
	atf_set descr 'Test IPv4 scub "set-tos" rule'
	atf_set require.user root
	atf_set require.progs scapy
}

set_tos_v4_body()
{
	setup_router_dummy_ipv4
	pft_set_rules router "scrub on ${epair_tester}b set-tos 0x42"
	ping_dummy_check_request exit:0 --send-tc=0 --expect-tc=66
}

set_tos_v4_cleanup()
{
	pft_cleanup
}


atf_test_case "set_tos_v6" "cleanup"
set_tos_v6_head()
{
	atf_set descr 'Test IPv6 scub "set-tos" rule'
	atf_set require.user root
	atf_set require.progs scapy
}

set_tos_v6_body()
{
	setup_router_dummy_ipv6
	pft_set_rules router "scrub on ${epair_tester}b set-tos 0x42"
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-tc=0 --expect-tc=66
}

set_tos_v6_cleanup()
{
	pft_cleanup
}


atf_test_case "min_ttl_v4" "cleanup"
min_ttl_v4_head()
{
	atf_set descr 'Test IPv4 scub "min-ttl" rule'
	atf_set require.user root
	atf_set require.progs scapy
}

min_ttl_v4_body()
{
	setup_router_dummy_ipv4
	pft_set_rules router "scrub on ${epair_tester}b min-ttl 50"
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-hlim=40 --expect-hlim=49
}

min_ttl_v4_cleanup()
{
	pft_cleanup
}


atf_test_case "min_ttl_v6" "cleanup"
min_ttl_v6_head()
{
	atf_set descr 'Test IPv6 scub "min-ttl" rule'
	atf_set require.user root
	atf_set require.progs scapy
}

min_ttl_v6_body()
{
	setup_router_dummy_ipv6
	pft_set_rules router "scrub on ${epair_tester}b min-ttl 50"
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-hlim=40 --expect-hlim=49
}

min_ttl_v6_cleanup()
{
	pft_cleanup
}


atf_test_case "no_scrub_v4" "cleanup"
no_scrub_v4_head()
{
	atf_set descr 'Test IPv4 "no scrub" rule'
	atf_set require.user root
	atf_set require.progs scapy
}

no_scrub_v4_body()
{
	setup_router_dummy_ipv4
	pft_set_rules router\
		"no scrub on ${epair_tester}b to ${net_server_host_server}"
		"scrub on ${epair_tester}b set-tos 0x42"
	ping_dummy_check_request exit:0 --send-tc=0 --expect-tc=0
}

no_scrub_v4_cleanup()
{
	pft_cleanup
}


atf_test_case "no_scrub_v6" "cleanup"
no_scrub_v6_head()
{
	atf_set descr 'Test IPv6 "no scrub" rule'
	atf_set require.user root
	atf_set require.progs scapy
}

no_scrub_v6_body()
{
	setup_router_dummy_ipv6
	pft_set_rules router \
		"no scrub on ${epair_tester}b to ${net_server_host_server}"
		"scrub on ${epair_tester}b set-tos 0x42"
	ping_dummy_check_request exit:0 --send-tc=0 --expect-tc=0
}

no_scrub_v6_cleanup()
{
	pft_cleanup
}


atf_init_test_cases()
{
	atf_add_test_case "max_mss_v4"
	atf_add_test_case "max_mss_v6"
	atf_add_test_case "set_tos_v4"
	atf_add_test_case "set_tos_v6"
	atf_add_test_case "min_ttl_v4"
	atf_add_test_case "min_ttl_v6"
	atf_add_test_case "no_scrub_v4"
	atf_add_test_case "no_scrub_v6"
}
