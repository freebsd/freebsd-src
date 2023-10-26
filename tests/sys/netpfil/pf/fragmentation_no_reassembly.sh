#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2017 Kristof Provost <kp@FreeBSD.org>
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

atf_test_case "match_full_v4" "cleanup"
match_full_v4_head()
{
    atf_set descr 'Matching non-fragmented IPv4 packets'
    atf_set require.user root
    atf_set require.progs scapy
}

match_full_v4_body()
{
    setup_router_dummy_ipv4

    # Sanity check.
    ping_dummy_check_request exit:0 --ping-type=icmp

    # Only non-fragmented packets are passed
    jexec router pfctl -e
    pft_set_rules router \
        "pass out" \
        "block in" \
        "pass in inet proto icmp all icmp-type echoreq"
    ping_dummy_check_request exit:0 --ping-type=icmp
    ping_dummy_check_request exit:1 --ping-type=icmp --send-length=2000 --send-frag-length 1000
}

match_full_v4_cleanup()
{
    pft_cleanup
}


atf_test_case "match_fragment_v4" "cleanup"
match_fragment_v4_head()
{
    atf_set descr 'Matching fragmented IPv4 packets'
    atf_set require.user root
    atf_set require.progs scapy
}

match_fragment_v4_body()
{
    setup_router_dummy_ipv4

    # Sanity check.
    ping_dummy_check_request exit:0 --ping-type=icmp

    # Only fragmented packets are passed
    pft_set_rules router \
        "pass out" \
        "block in" \
        "pass in inet proto icmp fragment"
    ping_dummy_check_request exit:1 --ping-type=icmp
    ping_dummy_check_request exit:0 --ping-type=icmp --send-length=2000 --send-frag-length 1000
}

match_fragment_v4_cleanup()
{
    pft_cleanup
}


atf_test_case "compat_override_v4" "cleanup"
compat_override_v4_head()
{
    atf_set descr 'Scrub rules override "set reassemble" for IPv4'
    atf_set require.user root
    atf_set require.progs scapy
}

compat_override_v4_body()
{
    setup_router_dummy_ipv4

    # Sanity check.
    ping_dummy_check_request exit:0 --ping-type=icmp

    # The same as match_fragment_v4 but with "set reassemble yes" which
    # is ignored because of presence of scrub rules.
    # Only fragmented packets are passed.
    pft_set_rules router \
        "set reassemble yes" \
        "no scrub" \
        "pass out" \
        "block in" \
        "pass in inet proto icmp fragment"
    ping_dummy_check_request exit:1 --ping-type=icmp
    ping_dummy_check_request exit:0 --ping-type=icmp --send-length=2000 --send-frag-length 1000
}

compat_override_v4_cleanup()
{
    pft_cleanup
}


atf_init_test_cases()
{
    atf_add_test_case "match_full_v4"
    atf_add_test_case "match_fragment_v4"
    atf_add_test_case "compat_override_v4"
}
