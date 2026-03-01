#! /usr/libexec/atf-sh
#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Boris Lytochkin
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
#

common_dir="$(atf_get_srcdir)/../common"
. ${common_dir}/utils.subr

NC="nc -w 1 -dnN"

setup_network_v4()
{
	epair="$1"

	ifconfig ${epair}a 192.0.2.0/31 up
	ifconfig ${epair_recv}a up

	vnet_mkjail alcatraz ${epair}b

	jexec alcatraz ifconfig ${epair}b 192.0.2.1/31 up

	jexec alcatraz /usr/sbin/inetd -p /dev/null $(atf_get_srcdir)/lookup_inetd.conf

	# Sanity checks
	atf_check -s exit:0 -o ignore ping -i .1 -c 3 -s 1200 192.0.2.1
	atf_check -o "inline:GOOD 82\n" ${NC} 192.0.2.1 82

}

atf_test_case "ip4" "cleanup"
ip4_head()
{
	atf_set descr 'IPv4 lookup test'
	atf_set require.user root
	atf_set require.progs scapy
}

ip4_body()
{
	firewall_init "ipfw"

	epair=$(vnet_mkepair)

	setup_network_v4 ${epair}

	# Source address match
	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type addr" \
		"ipfw -q table 1 add 192.0.2.0/32" \
		"ipfw -q add 100 unreach port tcp from any to any lookup src-ip 1 in"
	atf_check -s exit:1 ${NC} 192.0.2.1 82

	# Destination address match
	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type addr" \
		"ipfw -q table 1 add 192.0.2.1/32" \
		"ipfw -q add 100 unreach port tcp from any to any lookup dst-ip 1 in"
	atf_check -s exit:1 ${NC} 192.0.2.1 82

	# Masked part

	# Masked source address match
	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type addr" \
		"ipfw -q table 1 add 192.0.0.0/32" \
		"ipfw -q add 100 allow tcp from any to any lookup src-ip4:255.255.253.255 1 in" \
		"ipfw -q add 200 unreach port ip from any to any in"
	atf_check -o "inline:GOOD 82\n" ${NC} 192.0.2.1 82

	# Masked destination address match
	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type addr" \
		"ipfw -q table 1 add 192.0.0.1/32" \
		"ipfw -q add 100 allow tcp from any to any lookup dst-ip4:255.255.253.255 1 in" \
		"ipfw -q add 200 unreach port ip from any to any in"
	atf_check -o "inline:GOOD 82\n" ${NC} 192.0.2.1 82

	# Masked source address !match
	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type addr" \
		"ipfw -q table 1 add 192.0.0.0/32" \
		"ipfw -q add 100 allow tcp from any to any lookup src-ip4:128.255.253.255 1 in" \
		"ipfw -q add 200 unreach port ip from any to any in"
	atf_check -s exit:1 ${NC} 192.0.2.1 82

	# Masked destination address !match
	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type addr" \
		"ipfw -q table 1 add 192.0.2.1/32" \
		"ipfw -q add 100 allow tcp from any to any lookup dst-ip4:128.255.255.255 1 in" \
		"ipfw -q add 200 unreach port ip from any to any in"
	atf_check -s exit:1 ${NC} 192.0.2.1 82

}


setup_network_v6()
{
	epair="$1"

	ifconfig ${epair}a inet6 2001:db8:42::1/64 up no_dad -ifdisabled

	vnet_mkjail alcatraz ${epair}b

	jexec alcatraz ifconfig ${epair}b inet6 2001:db8:42::2/64 up no_dad

	jexec alcatraz /usr/sbin/inetd -p /dev/null $(atf_get_srcdir)/lookup_inetd.conf

	# Sanity checks
	atf_check -s exit:0 -o ignore ping6 -i .1 -c 3 -s 1200 2001:db8:42::2
	atf_check -o "inline:GOOD 82\n" ${NC} 2001:db8:42::2 82

}

atf_test_case "ip6" "cleanup"
ip6_head()
{
	atf_set descr 'IPv6 lookup test'
	atf_set require.user root
	atf_set require.progs scapy
}

ip6_body()
{
	firewall_init "ipfw"

	epair=$(vnet_mkepair)

	setup_network_v6 ${epair}

	# Source address match
	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type addr" \
		"ipfw -q table 1 add 2001:db8:42::1/128" \
		"ipfw -q add 100 unreach port tcp from any to any lookup src-ip 1 in"
	atf_check -s exit:1 ${NC} 2001:db8:42::2 82

	# Destination address match
	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type addr" \
		"ipfw -q table 1 add 2001:db8:42::2/128" \
		"ipfw -q add 100 unreach port tcp from any to any lookup dst-ip 1 in"
	atf_check -s exit:1 ${NC} 2001:db8:42::2 82

	# Source address match
	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type addr" \
		"ipfw -q table 1 add 2000::/64" \
		"ipfw -q add 100 unreach port tcp from any to any lookup src-ip 1 in"
	atf_check -o "inline:GOOD 82\n" ${NC} 2001:db8:42::2 82

	# Masked part

	# Masked source address match
	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type addr" \
		"ipfw -q table 1 add 2000:db8:42::1/128" \
		"ipfw -q add 100 allow tcp from any to any lookup src-ip6:fff0:ffff:ffff:ffff:ffff:ffff:ffff:ffff 1 in" \
		"ipfw -q add 200 unreach port tcp from any to any in"
	atf_check -o "inline:GOOD 82\n" ${NC} 2001:db8:42::2 82

	# Destination address match
	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type addr" \
		"ipfw -q table 1 add 2001:0:42::2/128" \
		"ipfw -q add 100 allow tcp from any to any lookup dst-ip6:ffff:0:ffff:ffff:ffff:ffff:ffff:ffff 1 in" \
		"ipfw -q add 200 count tcp from any to any in"
	atf_check -o "inline:GOOD 82\n" ${NC} 2001:db8:42::2 82

	# Masked source address !match
	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type addr" \
		"ipfw -q table 1 add 2001:db8:42::1/128" \
		"ipfw -q add 100 allow tcp from any to any lookup src-ip6:fff0:ffff:ffff:ffff:ffff:ffff:ffff:ffff 1 in" \
		"ipfw -q add 200 unreach port tcp from any to any in"
	atf_check -s exit:1 ${NC} 2001:db8:42::2 82

	# Masked destination address !match
	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type addr" \
		"ipfw -q table 1 add 2000:0:42::2/128" \
		"ipfw -q add 100 allow tcp from any to any lookup dst-ip6:ffff:0:ffff:ffff:ffff:ffff:ffff:ffff 1 in" \
		"ipfw -q add 200 unreach port tcp from any to any in"
	atf_check -s exit:1 ${NC} 2001:db8:42::2 82

}

atf_test_case "rulenum" "cleanup"
rulenum_head()
{
	atf_set descr 'Rule number lookup test'
	atf_set require.user root
}

rulenum_body()
{
	firewall_init "ipfw"

	epair=$(vnet_mkepair)

	setup_network_v6 ${epair}

	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type number" \
		"ipfw -q table 1 add 100" \
		"ipfw -q add 100 unreach port tcp from any to any lookup rulenum 1 in"
	atf_check -s exit:1 ${NC} 2001:db8:42::2 82

	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type number" \
		"ipfw -q table 1 add 101" \
		"ipfw -q add 100 unreach port tcp from any to any lookup rulenum 1 in"
	atf_check -o "inline:GOOD 82\n" ${NC} 2001:db8:42::2 82

	# Masked part

	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type number" \
		"ipfw -q table 1 add 96" \
		"ipfw -q add 100 unreach port tcp from any to any lookup rulenum:0x60 1 in"
	atf_check -s exit:1 ${NC} 2001:db8:42::2 82

	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type number" \
		"ipfw -q table 1 add 101" \
		"ipfw -q add 100 unreach port tcp from any to any lookup rulenum:32 1 in"
	atf_check -o "inline:GOOD 82\n" ${NC} 2001:db8:42::2 82

}

atf_test_case "port" "cleanup"
port_head()
{
	atf_set descr 'Lookup src-/dst-port works'
	atf_set require.user root
}

port_body()
{
	firewall_init "ipfw"

	epair=$(vnet_mkepair)

	setup_network_v4 ${epair}

	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type number" \
		"ipfw -q table 1 add 82" \
		"ipfw add 10 allow tcp from any to any lookup dst-port 1 in" \
		"ipfw add 20 unreach port tcp from any to any dst-port 82"
	atf_check -o "inline:GOOD 82\n" ${NC} 192.0.2.1 82

	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type number" \
		"ipfw -q table 1 add 80" \
		"ipfw add 10 allow tcp from any to any lookup dst-port 1 in" \
		"ipfw add 20 unreach port tcp from any to any dst-port 82"
	atf_check -s exit:1 ${NC} 192.0.2.1 82

	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type number" \
		"ipfw -q table 1 add 82" \
		"ipfw add 10 allow tcp from any to any lookup src-port 1 out" \
		"ipfw add 20 unreach port tcp from any to any src-port 82 out"
	atf_check -o "inline:GOOD 82\n" ${NC} 192.0.2.1 82

	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type number" \
		"ipfw -q table 1 add 80" \
		"ipfw add 10 allow tcp from any to any lookup src-port 1 in" \
		"ipfw add 20 unreach port tcp from any to any src-port 22222 in"
	atf_check -s exit:1 ${NC} -p 22222 192.0.2.1 82

	# Masked part

	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type number" \
		"ipfw -q table 1 add 18" \
		"ipfw add 10 allow tcp from any to any lookup dst-port:0x1F 1 in" \
		"ipfw add 20 unreach port tcp from any to any dst-port 82"
	atf_check -o "inline:GOOD 82\n" ${NC} 192.0.2.1 82

	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type number" \
		"ipfw -q table 1 add 18" \
		"ipfw add 10 allow tcp from any to any lookup dst-port:255 1 in" \
		"ipfw add 20 unreach port tcp from any to any dst-port 82"
	atf_check -s exit:1 ${NC} 192.0.2.1 82
}

atf_test_case "dscp_v6" "cleanup"
dscp_v6_head()
{
	atf_set descr 'DSCP for IPv6 lookup test'
	atf_set require.user root
}

dscp_v6_body()
{
	firewall_init "ipfw"

	epair=$(vnet_mkepair)

	setup_network_v6 ${epair}

	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type number" \
		"ipfw -q table 1 add 12" \
		"ipfw -q add 50 setdscp 12 tcp from any to any" \
		"ipfw -q add 100 unreach port tcp from any to any lookup dscp 1 in"
	atf_check -s exit:1 ${NC} 2001:db8:42::2 82

	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type number" \
		"ipfw -q table 1 add 101" \
		"ipfw -q add 50 setdscp 12 tcp from any to any" \
		"ipfw -q add 100 unreach port tcp from any to any lookup dscp 1 in"
	atf_check -o "inline:GOOD 82\n" ${NC} 2001:db8:42::2 82

	# Masked part

	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type number" \
		"ipfw -q table 1 add 12" \
		"ipfw -q add 50 setdscp 13 tcp from any to any" \
		"ipfw -q add 100 unreach port tcp from any to any lookup dscp:0xFE 1 in"
	atf_check -s exit:1 ${NC} 2001:db8:42::2 82

	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type number" \
		"ipfw -q table 1 add 13" \
		"ipfw -q add 50 setdscp 13 tcp from any to any" \
		"ipfw -q add 100 unreach port tcp from any to any lookup dscp:1 1 in"
	atf_check -o "inline:GOOD 82\n" ${NC} 2001:db8:42::2 82

}

atf_test_case "dscp_v4" "cleanup"
dscp_v4_head()
{
	atf_set descr 'Lookup DSCP for IPv4 test'
	atf_set require.user root
}

dscp_v4_body()
{
	firewall_init "ipfw"

	epair=$(vnet_mkepair)

	setup_network_v4 ${epair}

	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type number" \
		"ipfw -q table 1 add 12" \
		"ipfw -q add 50 setdscp 12 tcp from any to any dst-port 82 in" \
		"ipfw -q add 100 allow tcp from any to any lookup dscp 1 in" \
		"ipfw -q add 200 unreach port tcp from any to any port 82 in"
	atf_check -o "inline:GOOD 82\n" ${NC} 192.0.2.1 82

	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type number" \
		"ipfw -q table 1 add 13" \
		"ipfw -q add 50 setdscp 12 tcp from any to any dst-port 82 in" \
		"ipfw -q add 100 allow tcp from any to any lookup dscp 1 in" \
		"ipfw -q add 200 unreach port tcp from any to any dst-port 82 in"
	atf_check -s exit:1 ${NC} 192.0.2.1 82

	# Masked part

	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type number" \
		"ipfw -q table 1 add 48" \
		"ipfw -q add 50 setdscp 50 tcp from any to any dst-port 82 in" \
		"ipfw -q add 100 allow tcp from any to any lookup dscp:0xF0 1 in" \
		"ipfw -q add 200 unreach port tcp from any to any dst-port 82"
	atf_check -o "inline:GOOD 82\n" ${NC} 192.0.2.1 82

	firewall_config "alcatraz" ipfw ipfw \
		"ipfw -q table 1 create type number" \
		"ipfw -q table 1 add 48" \
		"ipfw -q add 50 setdscp 50 tcp from any to any dst-port 82 in" \
		"ipfw -q add 100 allow tcp from any to any lookup dscp:255 1 in" \
		"ipfw -q add 200 unreach port tcp from any to any dst-port 82"
	atf_check -s exit:1 ${NC} 192.0.2.1 82
}


lookup_cleanup()
{
	 firewall_cleanup $1
}

atf_init_test_cases()
{
	for test in "ip4" "ip6" \
	    "rulenum" "port" \
	    "dscp_v4" "dscp_v6" \
	    ; do
		atf_add_test_case "${test}"
		alias "${test}_cleanup"="lookup_cleanup"
	done
}

