#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Kristof Provost <kp@FreeBSD.org>
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

is_master()
{
	jail=$1
	itf=$2

	jexec ${jail} ifconfig ${itf} | grep -E '(carp|vrrp)' | grep MASTER
}

wait_for_carp()
{
	jail1=$1
	itf1=$2
	jail2=$3
	itf2=$4

	while [ -z "$(is_master ${jail1} ${itf1})" ] &&
	    [ -z "$(is_master ${jail2} ${itf2})" ]; do
		sleep 1
	done

	if [ -n "$(is_master ${jail1} ${itf1})" ] &&
	    [ -n "$(is_master ${jail2} ${itf2})" ]; then
		atf_fail "Both jails are master"
	fi
}

carp_init()
{
	if ! kldstat -q -m carp; then
		atf_skip "This test requires carp"
	fi

	vnet_init
}

atf_test_case "basic_v4" "cleanup"
basic_v4_head()
{
	atf_set descr 'Basic CARP test (IPv4)'
	atf_set require.user root
}

basic_v4_body()
{
	carp_init
	vnet_init_bridge

	bridge=$(vnet_mkbridge)
	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)

	vnet_mkjail carp_basic_v4_one ${bridge} ${epair_one}a ${epair_two}a
	vnet_mkjail carp_basic_v4_two ${epair_one}b
	vnet_mkjail carp_basic_v4_three ${epair_two}b

	jexec carp_basic_v4_one ifconfig ${bridge} 192.0.2.4/29 up
	jexec carp_basic_v4_one ifconfig ${bridge} addm ${epair_one}a \
	    addm ${epair_two}a
	jexec carp_basic_v4_one ifconfig ${epair_one}a up
	jexec carp_basic_v4_one ifconfig ${epair_two}a up

	jexec carp_basic_v4_two ifconfig ${epair_one}b 192.0.2.202/29 up
	jexec carp_basic_v4_two ifconfig ${epair_one}b add vhid 1 192.0.2.1/29

	jexec carp_basic_v4_three ifconfig ${epair_two}b 192.0.2.203/29 up
	jexec carp_basic_v4_three ifconfig ${epair_two}b add vhid 1 \
	    192.0.2.1/29

	wait_for_carp carp_basic_v4_two ${epair_one}b \
	    carp_basic_v4_three ${epair_two}b

	atf_check -s exit:0 -o ignore jexec carp_basic_v4_one \
	    ping -c 3 192.0.2.1
}

basic_v4_cleanup()
{
	vnet_cleanup
}

atf_test_case "vrrp_v4" "cleanup"
vrrp_v4_head()
{
	atf_set descr 'Basic VRRP test (IPv4)'
	atf_set require.user root
}

vrrp_v4_body()
{
	carp_init
	vnet_init_bridge

	j=vrrp_basic_v4

	bridge=$(vnet_mkbridge)
	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)

	vnet_mkjail ${j}_one ${bridge} ${epair_one}a ${epair_two}a
	vnet_mkjail ${j}_two ${epair_one}b
	vnet_mkjail ${j}_three ${epair_two}b

	jexec ${j}_one ifconfig ${bridge} 192.0.2.4/29 up
	jexec ${j}_one ifconfig ${bridge} addm ${epair_one}a \
	    addm ${epair_two}a
	jexec ${j}_one ifconfig ${epair_one}a up
	jexec ${j}_one ifconfig ${epair_two}a up

	jexec ${j}_two ifconfig ${epair_one}b 192.0.2.202/29 up
	jexec ${j}_two ifconfig ${epair_one}b add vhid 1 carpver 3 192.0.2.1/29

	jexec ${j}_three ifconfig ${epair_two}b 192.0.2.203/29 up
	jexec ${j}_three ifconfig ${epair_two}b add vhid 1 carpver 3 \
	    192.0.2.1/29

	wait_for_carp ${j}_two ${epair_one}b \
	    ${j}_three ${epair_two}b

	atf_check -s exit:0 -o ignore jexec ${j}_one \
	    ping -c 3 192.0.2.1
}

vrrp_v4_cleanup()
{
	vnet_cleanup
}

atf_test_case "unicast_v4" "cleanup"
unicast_v4_head()
{
	atf_set descr 'Unicast CARP test (IPv4)'
	atf_set require.user root
}

unicast_v4_body()
{
	carp_init

	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)

	vnet_mkjail carp_uni_v4_one ${epair_one}a ${epair_two}a
	vnet_mkjail carp_uni_v4_two ${epair_one}b
	vnet_mkjail carp_uni_v4_three ${epair_two}b

	jexec carp_uni_v4_one sysctl net.inet.ip.forwarding=1
	jexec carp_uni_v4_one ifconfig ${epair_one}a inet 198.51.100.1/25
	jexec carp_uni_v4_one ifconfig ${epair_two}a inet 198.51.100.129/25

	jexec carp_uni_v4_two sysctl net.inet.ip.forwarding=1
	jexec carp_uni_v4_two ifconfig ${epair_one}b 198.51.100.2/25 up
	jexec carp_uni_v4_two route add 198.51.100.224 198.51.100.1
	# A peer address x.x.x.224 to catch PR 284872
	jexec carp_uni_v4_two ifconfig ${epair_one}b add vhid 1 \
	    peer 198.51.100.224 192.0.2.1/32

	jexec carp_uni_v4_three sysctl net.inet.ip.forwarding=1
	jexec carp_uni_v4_three ifconfig ${epair_two}b 198.51.100.224/25 up
	jexec carp_uni_v4_three route add 198.51.100.2 198.51.100.129
	jexec carp_uni_v4_three ifconfig ${epair_two}b add vhid 1 \
	    peer 198.51.100.2 192.0.2.1/32

	# Sanity check
	atf_check -s exit:0 -o ignore jexec carp_uni_v4_two \
	    ping -c 1 198.51.100.224

	wait_for_carp carp_uni_v4_two ${epair_one}b \
	    carp_uni_v4_three ${epair_two}b

	# Setup RIPv2 route daemon
	jexec carp_uni_v4_two routed -s -Pripv2
	jexec carp_uni_v4_three routed -s -Pripv2
	jexec carp_uni_v4_one routed -Pripv2

	# XXX Wait for route propagation
	sleep 3

	atf_check -s exit:0 -o ignore jexec carp_uni_v4_one \
	    ping -c 3 192.0.2.1

	# Check that we remain in unicast when tweaking settings
	atf_check -s exit:0 -o ignore \
	    jexec carp_uni_v4_two ifconfig ${epair_one}b vhid 1 advskew 2
	atf_check -s exit:0 -o match:"peer 198.51.100.224" \
	    jexec carp_uni_v4_two ifconfig ${epair_one}b
}

unicast_v4_cleanup()
{
	vnet_cleanup
}

atf_test_case "basic_v6" "cleanup"
basic_v6_head()
{
	atf_set descr 'Basic CARP test (IPv6)'
	atf_set require.user root
}

basic_v6_body()
{
	carp_init
	vnet_init_bridge

	bridge=$(vnet_mkbridge)
	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)

	vnet_mkjail carp_basic_v6_one ${bridge} ${epair_one}a ${epair_two}a
	vnet_mkjail carp_basic_v6_two ${epair_one}b
	vnet_mkjail carp_basic_v6_three ${epair_two}b

	jexec carp_basic_v6_one ifconfig ${bridge} inet6 2001:db8::0:4/64 up \
	    no_dad
	jexec carp_basic_v6_one ifconfig ${bridge} addm ${epair_one}a \
	    addm ${epair_two}a
	jexec carp_basic_v6_one ifconfig ${epair_one}a up
	jexec carp_basic_v6_one ifconfig ${epair_two}a up

	jexec carp_basic_v6_two ifconfig ${epair_one}b inet6 \
	    2001:db8::1:2/64 up no_dad
	jexec carp_basic_v6_two ifconfig ${epair_one}b inet6 add vhid 1 \
	    2001:db8::0:1/64

	jexec carp_basic_v6_three ifconfig ${epair_two}b inet6 2001:db8::1:3/64 up no_dad
	jexec carp_basic_v6_three ifconfig ${epair_two}b inet6 add vhid 1 \
	    2001:db8::0:1/64

	wait_for_carp carp_basic_v6_two ${epair_one}b \
	    carp_basic_v6_three ${epair_two}b

	atf_check -s exit:0 -o ignore jexec carp_basic_v6_one \
	    ping -6 -c 3 2001:db8::0:1
}

basic_v6_cleanup()
{
	vnet_cleanup
}

atf_test_case "vrrp_v6" "cleanup"
vrrp_v6_head()
{
	atf_set descr 'Basic VRRP test (IPv6)'
	atf_set require.user root
}

vrrp_v6_body()
{
	carp_init
	vnet_init_bridge

	j=carp_basic_v6

	bridge=$(vnet_mkbridge)
	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)

	vnet_mkjail ${j}_one ${bridge} ${epair_one}a ${epair_two}a
	vnet_mkjail ${j}_two ${epair_one}b
	vnet_mkjail ${j}_three ${epair_two}b

	jexec ${j}_one ifconfig ${bridge} inet6 2001:db8::0:4/64 up \
	    no_dad
	jexec ${j}_one ifconfig ${bridge} addm ${epair_one}a \
	    addm ${epair_two}a
	jexec ${j}_one ifconfig ${epair_one}a up
	jexec ${j}_one ifconfig ${epair_two}a up

	jexec ${j}_two ifconfig ${epair_one}b inet6 \
	    2001:db8::1:2/64 up no_dad
	jexec ${j}_two ifconfig ${epair_one}b inet6 add vhid 1 carpver 3 \
	    2001:db8::0:1/64

	jexec ${j}_three ifconfig ${epair_two}b inet6 2001:db8::1:3/64 up no_dad
	jexec ${j}_three ifconfig ${epair_two}b inet6 add vhid 1 carpver 3 \
	    2001:db8::0:1/64

	wait_for_carp ${j}_two ${epair_one}b \
	    ${j}_three ${epair_two}b

	atf_check -s exit:0 -o ignore jexec ${j}_one \
	    ping -6 -c 3 2001:db8::0:1
}

vrrp_v6_cleanup()
{
	vnet_cleanup
}

atf_test_case "unicast_v6" "cleanup"
unicast_v6_head()
{
	atf_set descr 'Unicast CARP test (IPv6)'
	atf_set require.user root
}

unicast_v6_body()
{
	carp_init
	vnet_init_bridge

	bridge=$(vnet_mkbridge)
	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)

	vnet_mkjail carp_uni_v6_one ${bridge} ${epair_one}a ${epair_two}a
	vnet_mkjail carp_uni_v6_two ${epair_one}b
	vnet_mkjail carp_uni_v6_three ${epair_two}b

	jexec carp_uni_v6_one sysctl net.inet6.ip6.forwarding=1
	jexec carp_uni_v6_one ifconfig ${bridge} addm ${epair_one}a \
	    addm ${epair_two}a
	jexec carp_uni_v6_one ifconfig ${epair_one}a up
	jexec carp_uni_v6_one ifconfig ${epair_two}a up
	jexec carp_uni_v6_one ifconfig ${bridge} inet6 2001:db8::0:4/64 up \
	    no_dad
	jexec carp_uni_v6_one ifconfig ${bridge} inet6 alias 2001:db8:1::1/64 \
	    no_dad up
	jexec carp_uni_v6_one ifconfig ${bridge} inet6 alias 2001:db8:2::1/64 \
	    no_dad up

	jexec carp_uni_v6_two ifconfig ${epair_one}b inet6 2001:db8:1::2/64 \
	    no_dad up
	jexec carp_uni_v6_two route -6 add default 2001:db8:1::1
	jexec carp_uni_v6_two ifconfig ${epair_one}b inet6 add vhid 1 \
	    peer6 2001:db8:2::2 \
	    2001:db8::0:1/64

	jexec carp_uni_v6_three ifconfig ${epair_two}b inet6 2001:db8:2::2/64 \
	    no_dad up
	jexec carp_uni_v6_three route -6 add default 2001:db8:2::1
	jexec carp_uni_v6_three ifconfig ${epair_two}b inet6 add vhid 1 \
	    peer6 2001:db8:1::2 \
	    2001:db8::0:1/64

	# Sanity check
	atf_check -s exit:0 -o ignore jexec carp_uni_v6_two \
	    ping -6 -c 1 2001:db8:2::2

	wait_for_carp carp_uni_v6_two ${epair_one}b \
	    carp_uni_v6_three ${epair_two}b

	atf_check -s exit:0 -o ignore jexec carp_uni_v6_one \
	    ping -6 -c 3 2001:db8::0:1
}

unicast_v6_cleanup()
{
	vnet_cleanup
}

atf_test_case "unicast_ll_v6" "cleanup"
unicast_ll_v6_head()
{
	atf_set descr 'Unicast CARP test (IPv6, link-local)'
	atf_set require.user root
}

unicast_ll_v6_body()
{
	carp_init
	vnet_init_bridge

	j=carp_uni_ll_v6

	bridge=$(vnet_mkbridge)
	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)

	vnet_mkjail ${j}_one ${bridge} ${epair_one}a ${epair_two}a
	vnet_mkjail ${j}_two ${epair_one}b
	vnet_mkjail ${j}_three ${epair_two}b

	jexec ${j}_one ifconfig ${bridge} addm ${epair_one}a \
	    addm ${epair_two}a
	jexec ${j}_one ifconfig ${epair_one}a up
	jexec ${j}_one ifconfig ${epair_two}a up
	jexec ${j}_one ifconfig ${bridge} inet6 2001:db8::0:4/64 up \
	    no_dad
	jexec ${j}_one ifconfig ${bridge} inet6 alias 2001:db8:1::1/64 \
	    no_dad up

	jexec ${j}_two ifconfig ${epair_one}b inet6 2001:db8:1::2/64 \
	    no_dad up
	jexec ${j}_three ifconfig ${epair_two}b inet6 2001:db8:1::3/64 \
	    no_dad up

	ll_one=$(jexec ${j}_two ifconfig ${epair_one}b | awk "/ .*%${epair_one}b.* / { print \$2 }" | cut -d % -f 1)
	ll_two=$(jexec ${j}_three ifconfig ${epair_two}b | awk "/ .*%${epair_two}b.* / { print \$2 }" | cut -d % -f 1)

	jexec ${j}_two ifconfig ${epair_one}b inet6 add vhid 1 \
	    peer6 ${ll_two} \
	    2001:db8::0:1/64
	jexec ${j}_three ifconfig ${epair_two}b inet6 add vhid 1 \
	    peer6 ${ll_one} \
	    2001:db8::0:1/64

	# Sanity check
	atf_check -s exit:0 -o ignore jexec ${j}_two \
	    ping -6 -c 1 2001:db8:1::3

	wait_for_carp ${j}_two ${epair_one}b \
	    ${j}_three ${epair_two}b

	atf_check -s exit:0 -o ignore jexec ${j}_one \
	    ping -6 -c 3 2001:db8::0:1
}

unicast_ll_v6_cleanup()
{
	vnet_cleanup
}

atf_test_case "negative_demotion" "cleanup"
negative_demotion_head()
{
	atf_set descr 'Test PR #259528'
	atf_set require.user root
}

negative_demotion_body()
{
	carp_init

	epair=$(vnet_mkepair)

	vnet_mkjail one ${epair}a
	jexec one sysctl net.inet.carp.preempt=1
	jexec one ifconfig ${epair}a 192.0.2.1/24 up
	jexec one ifconfig ${epair}a add vhid 1 192.0.2.254/24 \
	    advskew 0 pass foobar

	vnet_mkjail two ${epair}b
	jexec two sysctl net.inet.carp.preempt=1
	jexec two ifconfig ${epair}b 192.0.2.2/24 up
	jexec two ifconfig ${epair}b add vhid 1 192.0.2.254/24 \
	    advskew 100 pass foobar

	# Allow things to settle
	wait_for_carp one ${epair}a two ${epair}b

	if is_master one ${epair}a && is_master two ${epair}b
	then
		atf_fail "Two masters!"
	fi

	jexec one sysctl net.inet.carp.demotion=-1
	sleep 3

	if is_master one ${epair}a && is_master two ${epair}b
	then
		atf_fail "Two masters!"
	fi
}

negative_demotion_cleanup()
{
	vnet_cleanup
}



atf_test_case "nd6_ns_source_mac" "cleanup"
nd6_ns_source_mac_head()
{
        atf_set descr 'CARP ndp neighbor solicitation MAC source test (IPv6)'
        atf_set require.user root
}

nd6_ns_source_mac_body()
{
        carp_init
        vnet_init_bridge

        bridge=$(vnet_mkbridge)
        epair_one=$(vnet_mkepair)
        epair_two=$(vnet_mkepair)

        vnet_mkjail carp_ndp_v6_bridge ${bridge} ${epair_one}a ${epair_two}a
        vnet_mkjail carp_ndp_v6_master ${epair_one}b
        vnet_mkjail carp_ndp_v6_slave ${epair_two}b

        jexec carp_ndp_v6_bridge ifconfig ${bridge} inet6 2001:db8::0:4/64 up \
            no_dad
        jexec carp_ndp_v6_bridge ifconfig ${bridge} addm ${epair_one}a \
            addm ${epair_two}a
        jexec carp_ndp_v6_bridge ifconfig ${epair_one}a up
        jexec carp_ndp_v6_bridge ifconfig ${epair_two}a up

        jexec carp_ndp_v6_master ifconfig ${epair_one}b inet6 \
            2001:db8::1:2/64 up no_dad
        jexec carp_ndp_v6_master ifconfig ${epair_one}b inet6 add vhid 1 \
            advskew 0 2001:db8::0:1/64

        jexec carp_ndp_v6_slave ifconfig ${epair_two}b inet6 \
	    2001:db8::1:3/64 up no_dad
        jexec carp_ndp_v6_slave ifconfig ${epair_two}b inet6 add vhid 1 \
            advskew 100 2001:db8::0:1/64

        wait_for_carp carp_ndp_v6_master ${epair_one}b \
            carp_ndp_v6_slave ${epair_two}b

	# carp_ndp_v6_master is MASTER

	# trigger a NS from the virtual IP from the BACKUP
        atf_check -s exit:2 -o ignore jexec carp_ndp_v6_slave \
            ping -6 -c 3 -S 2001:db8::0:1 2001:db8::0:4

	# trigger a NS from the virtual IP from the MASTER,
	# this ping should work
        atf_check -s exit:0 -o ignore jexec carp_ndp_v6_master \
            ping -6 -c 3 -S 2001:db8::0:1 2001:db8::0:4

        # ndp entry should be for the virtual mac
        atf_check -o match:'2001:db8::1 +00:00:5e:00:01:01' \
	    jexec carp_ndp_v6_bridge ndp -an
}

nd6_ns_source_mac_cleanup()
{
        vnet_cleanup
}


atf_test_case "switch" "cleanup"
switch_head()
{
	atf_set descr 'Switch between master and backup'
	atf_set require.user root
}

switch_body()
{
	carp_init

	epair=$(vnet_mkepair)

	ifconfig ${epair}a up
	ifconfig ${epair}a vhid 1 advskew 100 192.0.2.1/24
	ifconfig ${epair}a vhid 1 state backup
	ifconfig ${epair}a vhid 1 state master
}

switch_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "basic_v4"
	atf_add_test_case "vrrp_v4"
	atf_add_test_case "unicast_v4"
	atf_add_test_case "basic_v6"
	atf_add_test_case "vrrp_v6"
	atf_add_test_case "unicast_v6"
	atf_add_test_case "unicast_ll_v6"
	atf_add_test_case "negative_demotion"
	atf_add_test_case "nd6_ns_source_mac"
	atf_add_test_case "switch"
}
