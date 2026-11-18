# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2022 Gleb Smirnoff <glebius@FreeBSD.org>
# Copyright (c) 2022 Pavel Polyakov <bsd@kobyla.org>
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

. $(atf_get_srcdir)/../common/utils.subr

atf_test_case "local" "cleanup"
local_head()
{
	atf_set descr 'Test that fwd 127.0.0.1,port works'
	atf_set require.user root
}

local_body()
{
	firewall_init "ipfw"

	epair=$(vnet_mkepair)
	vnet_mkjail alcatraz ${epair}b

	atf_check ifconfig ${epair}a 192.0.2.0/31 up
	atf_check -o ignore route add 192.0.2.3/32 192.0.2.1

	atf_check jexec alcatraz ifconfig lo0 127.0.0.1/8 up
	atf_check jexec alcatraz ifconfig ${epair}b 192.0.2.1/31 up
	atf_check -o ignore jexec alcatraz route add default 192.0.2.0
	atf_check jexec alcatraz inetd -p $(pwd)/inetd.pid $(atf_get_srcdir)/fwd_inetd.conf

	firewall_config alcatraz ipfw ipfw \
	    "ipfw add 10 fwd 127.0.0.1,82 tcp from any to any dst-port 80 in via ${epair}b" \
	    "ipfw add 20 allow all from any to any"

	# Sanity check
	atf_check -o ignore ping -i .1 -c 3 -s 1200 192.0.2.1

	reply=$(nc -nN 192.0.2.3 80 < /dev/null)
	atf_check [ "${reply}" = "GOOD 82" ]
}

local_cleanup()
{
	firewall_cleanup $1
}

atf_test_case "nexthop_v6" "cleanup"
nexthop_v6_head()
{
	atf_set descr 'Test that an inbound IPv6 fwd rule chooses the next hop'
	atf_set require.user root
}

nexthop_v6_body()
{
	firewall_init "ipfw"

	epair_c=$(vnet_mkepair)
	epair_1=$(vnet_mkepair)
	epair_2=$(vnet_mkepair)

	vnet_mkjail rtr ${epair_c}b ${epair_1}a ${epair_2}a
	vnet_mkjail srv1 ${epair_1}b
	vnet_mkjail srv2 ${epair_2}b

	atf_check ifconfig ${epair_c}a inet6 2001:db8:1::2/64 up no_dad \
	    -ifdisabled
	atf_check -o ignore route add -6 2001:db8:4::/64 2001:db8:1::1

	atf_check ifconfig -j rtr ${epair_c}b inet6 2001:db8:1::1/64 up no_dad
	atf_check ifconfig -j rtr ${epair_1}a inet6 2001:db8:2::1/64 up no_dad
	atf_check ifconfig -j rtr ${epair_2}a inet6 2001:db8:3::1/64 up no_dad
	atf_check -o ignore sysctl -j rtr net.inet6.ip6.forwarding=1
	atf_check -o ignore route -j rtr add -6 2001:db8:4::/64 2001:db8:2::2

	# 2001:db8:4::1 lives on srv2, but the route points to srv1.
	atf_check ifconfig -j srv1 ${epair_1}b inet6 2001:db8:2::2/64 up no_dad
	atf_check ifconfig -j srv2 ${epair_2}b inet6 2001:db8:3::2/64 up no_dad
	atf_check ifconfig -j srv2 lo0 inet6 2001:db8:4::1/128 up
	atf_check -o ignore route -j srv2 add -6 default 2001:db8:3::1

	firewall_config rtr ipfw ipfw \
	    "ipfw add 20 allow all from any to any"
	atf_check -s exit:2 -o ignore ping -6 -c 1 -W 1 2001:db8:4::1

	firewall_config rtr ipfw ipfw \
	    "ipfw add 10 fwd 2001:db8:3::2 ip6 from any to 2001:db8:4::1 in via ${epair_c}b" \
	    "ipfw add 20 allow all from any to any"

	for redirect in 0 1; do
		atf_check -o ignore \
		    sysctl -j rtr net.inet6.ip6.redirect=${redirect}
		atf_check -o ignore ping -6 -c 1 -W 1 2001:db8:4::1
	done
}

nexthop_v6_cleanup()
{
	firewall_cleanup $1
}

atf_init_test_cases()
{
	atf_add_test_case "local"
	atf_add_test_case "nexthop_v6"
}
