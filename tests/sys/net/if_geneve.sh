#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025-2026 Pouria Mousavizadeh Tehrani <pouria@FreeBSD.org>
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

atf_test_case "ether_ipv4" "cleanup"
ether_ipv4_head()
{
	atf_set descr 'Create a geneve(4) l2 tunnel over an ipv4 underlay using epair and pass traffic between jails'
	atf_set require.user root
}

ether_ipv4_body()
{
	local epair geneve1 geneve2 vni1 endpoint1 endpoint2
        local v4tunnel1 v4tunnel2 v6tunnel1 v6tunnel2

	endpoint1=192.168.2.1
	endpoint2=192.168.2.2
	v4tunnel1=169.254.0.1
	v4tunnel2=169.254.0.2
	v6tunnel1=2001:db8::1
	v6tunnel2=2001:db8::2
	vni1=1

	if ! kldstat -q -m if_geneve; then
		atf_skip "This test requires if_geneve"
	fi

	vnet_init
	epair=$(vnet_mkepair)
	vnet_mkjail genevetest1 ${epair}a
	vnet_mkjail genevetest2 ${epair}b

	ifconfig -j genevetest1 ${epair}a inet ${endpoint1}/24 up
	ifconfig -j genevetest2 ${epair}b inet ${endpoint2}/24 up

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 create genevemode l2 debug \
	    geneveid $vni1 geneveremote ${endpoint2} genevelocal ${endpoint1} up
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 create genevemode l2 debug \
	    geneveid $vni1 geneveremote ${endpoint1} genevelocal ${endpoint2} up

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet ${v4tunnel1}/24
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet6 ${v6tunnel1}
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet ${v4tunnel2}/24
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet6 ${v6tunnel2}

	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v4tunnel2
	atf_check -s exit:0 -o ignore jexec genevetest2 ping -nc 1 -t 1 $v4tunnel1
	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v6tunnel2
	atf_check -s exit:0 -o ignore jexec genevetest2 ping -nc 1 -t 1 $v6tunnel1
}

ether_ipv4_cleanup()
{
	vnet_cleanup
}

atf_test_case "ether_ipv6" "cleanup"
ether_ipv6_head()
{
	atf_set descr 'Create a geneve(4) l2 tunnel over an ipv6 underlay using epair and pass traffic between jails'
	atf_set require.user root
}

ether_ipv6_body()
{
	local epair geneve1 geneve2 vni1 endpoint1 endpoint2
        local v4tunnel1 v4tunnel2 v6tunnel1 v6tunnel2

	endpoint1=3fff::1
	endpoint2=3fff::2
	v4tunnel1=169.254.0.1
	v4tunnel2=169.254.0.2
	v6tunnel1=2001:db8::1
	v6tunnel2=2001:db8::2
	vni1=1

	if ! kldstat -q -m if_geneve; then
		atf_skip "This test requires if_geneve"
	fi

	vnet_init
	epair=$(vnet_mkepair)
	vnet_mkjail genevetest1 ${epair}a
	vnet_mkjail genevetest2 ${epair}b

	ifconfig -j genevetest1 ${epair}a inet6 ${endpoint1} up
	ifconfig -j genevetest2 ${epair}b inet6 ${endpoint2} up

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 create genevemode l2 debug \
	    geneveid $vni1 geneveremote ${endpoint2} genevelocal ${endpoint1} up
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 create genevemode l2 debug \
	    geneveid $vni1 geneveremote ${endpoint1} genevelocal ${endpoint2} up

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet ${v4tunnel1}/24
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet6 ${v6tunnel1}
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet ${v4tunnel2}/24
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet6 ${v6tunnel2}

	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v4tunnel2
	atf_check -s exit:0 -o ignore jexec genevetest2 ping -nc 1 -t 1 $v4tunnel1
	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v6tunnel2
	atf_check -s exit:0 -o ignore jexec genevetest2 ping -nc 1 -t 1 $v6tunnel1
}

ether_ipv6_cleanup()
{
	vnet_cleanup
}

atf_test_case "inherit_ipv4" "cleanup"
inherit_ipv4_head()
{
	atf_set descr 'Create a geneve(4) l3 tunnel over an ipv4 underlay using epair and pass traffic between jails'
	atf_set require.user root
}

inherit_ipv4_body()
{
	local epair geneve1 geneve2 vni1 endpoint1 endpoint2
        local v4tunnel1 v4tunnel2 v6tunnel1 v6tunnel2

	endpoint1=192.168.2.1
	endpoint2=192.168.2.2
	v4tunnel1=169.254.0.1
	v4tunnel2=169.254.0.2
	v6tunnel1=2001:db8::1
	v6tunnel2=2001:db8::2
	vni1=2

	if ! kldstat -q -m if_geneve; then
		atf_skip "This test requires if_geneve"
	fi

	vnet_init
	epair=$(vnet_mkepair)
	vnet_mkjail genevetest1 ${epair}a
	vnet_mkjail genevetest2 ${epair}b

	ifconfig -j genevetest1 ${epair}a inet ${endpoint1}/24 up
	ifconfig -j genevetest2 ${epair}b inet ${endpoint2}/24 up

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 create genevemode l3 debug \
	    geneveid $vni1 geneveremote ${endpoint2} genevelocal ${endpoint1} up
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 create genevemode l3 debug \
	    geneveid $vni1 geneveremote ${endpoint1} genevelocal ${endpoint2} up

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet ${v4tunnel1}/24
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet6 ${v6tunnel1}
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet ${v4tunnel2}/24
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet6 ${v6tunnel2}

	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v4tunnel2
	atf_check -s exit:0 -o ignore jexec genevetest2 ping -nc 1 -t 1 $v4tunnel1
	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v6tunnel2
	atf_check -s exit:0 -o ignore jexec genevetest2 ping -nc 1 -t 1 $v6tunnel1
}

inherit_ipv4_cleanup()
{
	vnet_cleanup
}

atf_test_case "inherit_ipv6" "cleanup"
inherit_ipv6_head()
{
	atf_set descr 'Create a geneve(4) l3 tunnel over an ipv6 underlay using epair and pass traffic between jails'
	atf_set require.user root
}

inherit_ipv6_body()
{
	local epair geneve1 geneve2 vni1 endpoint1 endpoint2
        local v4tunnel1 v4tunnel2 v6tunnel1 v6tunnel2

	endpoint1=3fff::1
	endpoint2=3fff::2
	v4tunnel1=169.254.0.1
	v4tunnel2=169.254.0.2
	v6tunnel1=2001:db8::1
	v6tunnel2=2001:db8::2
	vni1=1

	if ! kldstat -q -m if_geneve; then
		atf_skip "This test requires if_geneve"
	fi

	vnet_init
	epair=$(vnet_mkepair)
	vnet_mkjail genevetest1 ${epair}a
	vnet_mkjail genevetest2 ${epair}b

	ifconfig -j genevetest1 ${epair}a inet6 ${endpoint1} up
	ifconfig -j genevetest2 ${epair}b inet6 ${endpoint2} up

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 create genevemode l3 debug \
	    geneveid $vni1 geneveremote ${endpoint2} genevelocal ${endpoint1} up
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 create genevemode l3 debug \
	    geneveid $vni1 geneveremote ${endpoint1} genevelocal ${endpoint2} up

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet ${v4tunnel1}/24
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet6 ${v6tunnel1}
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet ${v4tunnel2}/24
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet6 ${v6tunnel2}

	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v4tunnel2
	atf_check -s exit:0 -o ignore jexec genevetest2 ping -nc 1 -t 1 $v4tunnel1
	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v6tunnel2
	atf_check -s exit:0 -o ignore jexec genevetest2 ping -nc 1 -t 1 $v6tunnel1
}

inherit_ipv6_cleanup()
{
	vnet_cleanup
}

atf_test_case "ether_ipv6_blind_options" "cleanup"
ether_ipv6_blind_options_head()
{
	atf_set descr 'Create a geneve(4) l2 ipv6 tunnel and test geneve options'
	atf_set require.user root
}

ether_ipv6_blind_options_body()
{
	local epair geneve1 geneve2 vni1 endpoint1 endpoint2
        local v6tunnel1 v6tunnel2

	endpoint1=3fff::1
	endpoint2=3fff::2
	v6tunnel1=2001:db8::1
	v6tunnel2=2001:db8::2
	vni1=1

	if ! kldstat -q -m if_geneve; then
		atf_skip "This test requires if_geneve"
	fi

	vnet_init
	epair=$(vnet_mkepair)
	vnet_mkjail genevetest1 ${epair}a
	vnet_mkjail genevetest2 ${epair}b

	ifconfig -j genevetest1 ${epair}a inet6 ${endpoint1} up
	ifconfig -j genevetest2 ${epair}b inet6 ${endpoint2} up

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 create genevemode l2 debug \
	    geneveid $vni1 geneveremote ${endpoint2} genevelocal ${endpoint1} up
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 create genevemode l2 debug \
	    geneveid $vni1 geneveremote ${endpoint1} genevelocal ${endpoint2} up

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet6 ${v6tunnel1}
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet6 ${v6tunnel2}

	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v6tunnel2

	atf_check -s exit:0 -o ignore ifconfig -j genevetest1 geneve1 genevemaxaddr 1000
	atf_check -s exit:0 -o match:"max: 1000" ifconfig -j genevetest1 -v geneve1
	atf_check -s exit:0 -o ignore ifconfig -j genevetest1 geneve1 genevetimeout 1000
	atf_check -s exit:0 -o match:"timeout: 1000" ifconfig -j genevetest1 -v geneve1
	atf_check -s exit:0 -o ignore ifconfig -j genevetest1 geneve1 -genevelearn
	atf_check -s exit:0 -o match:"mode: nolearning" ifconfig -j genevetest1 -v geneve1
	atf_check -s exit:0 -o ignore ifconfig -j genevetest1 geneve1 genevelearn
	atf_check -s exit:0 -o match:" learning" ifconfig -j genevetest1 -v geneve1
	atf_check -s exit:0 -o match:"count: 1" ifconfig -j genevetest1 -v geneve1
	atf_check -s exit:0 -o ignore ifconfig -j genevetest1 geneve1 geneveflush
	atf_check -s exit:0 -o match:"count: 0" ifconfig -j genevetest1 -v geneve1
	atf_check -s exit:0 -o ignore ifconfig -j genevetest1 geneve1 geneveflushall
	atf_check -s exit:0 -o ignore ifconfig -j genevetest1 geneve1 genevettl inherit
	atf_check -s exit:0 -o match:"ttl: inherit" ifconfig -j genevetest1 -v geneve1
	atf_check -s exit:0 -o ignore ifconfig -j genevetest1 geneve1 genevettl 1
	atf_check -s exit:0 -o match:"ttl: 1" ifconfig -j genevetest1 -v geneve1
	atf_check -s exit:0 -o ignore ifconfig -j genevetest1 geneve1 down genevedf set up
	atf_check -s exit:0 -o match:"df: set" ifconfig -j genevetest1 -v geneve1
	atf_check -s exit:0 -o ignore ifconfig -j genevetest1 geneve1 down genevedf inherit up
	atf_check -s exit:0 -o match:"df: inherit" ifconfig -j genevetest1 -v geneve1
	atf_check -s exit:0 -o ignore ifconfig -j genevetest1 geneve1 down genevedf unset up
	atf_check -s exit:0 -o match:"df: unset" ifconfig -j genevetest1 -v geneve1
	atf_check -s exit:0 -o ignore ifconfig -j genevetest1 geneve1 genevedscpinherit
	atf_check -s exit:0 -o match:"dscp: inherit" ifconfig -j genevetest1 -v geneve1
	atf_check -s exit:0 -o ignore ifconfig -j genevetest1 geneve1 -genevedscpinherit
	atf_check -s exit:0 -o ignore ifconfig -j genevetest1 geneve1 geneveexternal
	atf_check -s exit:0 -o match:" external" ifconfig -j genevetest1 -v geneve1
	atf_check -s exit:0 -o ignore ifconfig -j genevetest1 geneve1 -geneveexternal
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 down geneveportrange 11000 62000 up
	atf_check -s exit:0 -o match:"portrange: 11000-62000" ifconfig -j genevetest1 -v geneve1

	atf_check -s exit:0 -o ignore jexec genevetest2 ping -nc 1 -t 1 $v6tunnel1
}

ether_ipv6_blind_options_cleanup()
{
	vnet_cleanup
}

atf_test_case "ether_ipv6_external" "cleanup"
ether_ipv6_external_head()
{
	atf_set descr 'Create a geneve(4) l2 ipv6 tunnel and test geneve collect metadata'
	atf_set require.user root
}

ether_ipv6_external_body()
{
	local epair geneve1 geneve2 vni1 endpoint1 endpoint2
        local v6tunnel1 v6tunnel2

	endpoint1=3fff::1
	endpoint2=3fff::2
	v6tunnel1=2001:db8::1
	v6tunnel2=2001:db8::2
	vni1=1

	if ! kldstat -q -m if_geneve; then
		atf_skip "This test requires if_geneve"
	fi

	vnet_init
	epair=$(vnet_mkepair)
	vnet_mkjail genevetest1 ${epair}a
	vnet_mkjail genevetest2 ${epair}b

	ifconfig -j genevetest1 ${epair}a inet6 ${endpoint1} up
	ifconfig -j genevetest2 ${epair}b inet6 ${endpoint2} up

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 create genevemode l2 debug \
	    geneveid $vni1 geneveremote ${endpoint2} genevelocal ${endpoint1} up
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 create genevemode l2 debug \
	    geneveid $vni1 geneveremote ${endpoint1} genevelocal ${endpoint2} up

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet6 ${v6tunnel1}
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet6 ${v6tunnel2}

	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v6tunnel2

	atf_check -s exit:0 -o ignore ifconfig -j genevetest1 geneve1 geneveexternal
	atf_check -s exit:16 -e ignore ifconfig -j genevetest1 geneve1 down geneveid 10 up
	atf_check -s exit:0 -o ignore ifconfig -j genevetest1 geneve1 -geneveexternal
	atf_check -s exit:0 -o ignore ifconfig -j genevetest1 geneve1 down geneveid 10 up

}

ether_ipv6_external_cleanup()
{
	vnet_cleanup
}

atf_test_case "ether_ipv4_multicast" "cleanup"
ether_ipv4_multicast_head()
{
	atf_set descr 'Create a geneve(4) l2 ipv4 multicast tunnel using epair and pass traffic between jails'
	atf_set require.user root
}

ether_ipv4_multicast_body()
{
	local epair geneve1 geneve2 vni1 endpoint1 endpoint2 mc_group
        local v4tunnel1 v4tunnel2 v6tunnel1 v6tunnel2

	endpoint1=192.168.2.1
	endpoint2=192.168.2.2
	mc_group=239.0.0.1
	v4tunnel1=169.254.0.1
	v4tunnel2=169.254.0.2
	v6tunnel1=2001:db8::1
	v6tunnel2=2001:db8::2
	vni1=1

	if ! kldstat -q -m if_geneve; then
		atf_skip "This test requires if_geneve"
	fi
	if ! kldstat -q -m ip_mroute; then
		atf_skip "This test requires ip_mroute"
	fi

	vnet_init
	epair=$(vnet_mkepair)
	vnet_mkjail genevetest1 ${epair}a
	vnet_mkjail genevetest2 ${epair}b

	ifconfig -j genevetest1 ${epair}a inet ${endpoint1}/24 up
	ifconfig -j genevetest2 ${epair}b inet ${endpoint2}/24 up

	# manually add the multicast routes to epairs
	route -j genevetest1 add -net 239.0.0.0/8 -interface ${epair}a
	route -j genevetest2 add -net 239.0.0.0/8 -interface ${epair}b

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 create genevemode l2 debug \
	    geneveid $vni1 genevelocal ${endpoint1} \
	    genevegroup ${mc_group} genevedev ${epair}a up
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 create genevemode l2 debug \
	    geneveid $vni1 genevelocal ${endpoint2} \
	    genevegroup ${mc_group} genevedev ${epair}b up

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet ${v4tunnel1}/24
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet6 ${v6tunnel1}
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet ${v4tunnel2}/24
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet6 ${v6tunnel2}

	atf_check -s exit:0 -o match:"group 239.0.0.1" jexec genevetest1 ifmcstat -i ${epair}a -f inet
	atf_check -s exit:0 -o match:"group 239.0.0.1" jexec genevetest2 ifmcstat -i ${epair}b -f inet

	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v4tunnel2
	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v6tunnel2
	atf_check -s exit:0 -o ignore jexec genevetest2 ping -nc 1 -t 1 $v4tunnel1
	atf_check -s exit:0 -o ignore jexec genevetest2 ping -nc 1 -t 1 $v6tunnel1

}

ether_ipv4_multicast_cleanup()
{
	vnet_cleanup
}

atf_test_case "ether_ipv6_multicast" "cleanup"
ether_ipv6_multicast_head()
{
	atf_set descr 'Create a geneve(4) l2 ipv6 multicast tunnel using epair and pass traffic between jails'
	atf_set require.user root
}

ether_ipv6_multicast_body()
{
	local epair geneve1 geneve2 vni1 endpoint1 endpoint2 mc_group
        local v4tunnel1 v4tunnel2 v6tunnel1 v6tunnel2

	endpoint1=3fff::1
	endpoint2=3fff::2
	mc_group=ff08::db8:0:1
	v4tunnel1=169.254.0.1
	v4tunnel2=169.254.0.2
	v6tunnel1=2001:db8::1
	v6tunnel2=2001:db8::2
	vni1=1

	if ! kldstat -q -m if_geneve; then
		atf_skip "This test requires if_geneve"
	fi
	if ! kldstat -q -m ip6_mroute; then
		atf_skip "This test requires ip6_mroute"
	fi

	vnet_init
	epair=$(vnet_mkepair)
	vnet_mkjail genevetest1 ${epair}a
	vnet_mkjail genevetest2 ${epair}b

	ifconfig -j genevetest1 ${epair}a inet6 ${endpoint1} up
	ifconfig -j genevetest2 ${epair}b inet6 ${endpoint2} up

	# manually add the multicast routes to epairs
	route -j genevetest1 -n6 add -net ff08::db8:0:1/96 -interface ${epair}a
	route -j genevetest2 -n6 add -net ff08::db8:0:1/96 -interface ${epair}b

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 create genevemode l2 debug \
	    geneveid $vni1 genevelocal ${endpoint1} \
	    genevegroup ${mc_group} genevedev ${epair}a up
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 create genevemode l2 debug \
	    geneveid $vni1 genevelocal ${endpoint2} \
	    genevegroup ${mc_group} genevedev ${epair}b up

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet ${v4tunnel1}/24
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet6 ${v6tunnel1}
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet ${v4tunnel2}/24
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet6 ${v6tunnel2}

	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v4tunnel2
	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v6tunnel2
	atf_check -s exit:0 -o ignore jexec genevetest2 ping -nc 1 -t 1 $v4tunnel1
	atf_check -s exit:0 -o ignore jexec genevetest2 ping -nc 1 -t 1 $v6tunnel1

}

ether_ipv6_multicast_cleanup()
{
	vnet_cleanup
}

atf_test_case "ether_ipv4_multicast_without_dev" "cleanup"
ether_ipv4_multicast_without_dev_head()
{
	atf_set descr 'Create a geneve(4) l2 ipv4 multicast tunnel without specifying genevedev using epair and pass traffic between jails'
	atf_set require.user root
}

ether_ipv4_multicast_without_dev_body()
{
	local epair geneve1 geneve2 vni1 endpoint1 endpoint2 mc_group
        local v4tunnel1 v4tunnel2 v6tunnel1 v6tunnel2

	endpoint1=192.168.2.1
	endpoint2=192.168.2.2
	mc_group=239.0.0.1
	v4tunnel1=169.254.0.1
	v4tunnel2=169.254.0.2
	v6tunnel1=2001:db8::1
	v6tunnel2=2001:db8::2
	vni1=1

	if ! kldstat -q -m if_geneve; then
		atf_skip "This test requires if_geneve"
	fi
	if ! kldstat -q -m ip_mroute; then
		atf_skip "This test requires ip_mroute"
	fi

	vnet_init
	epair=$(vnet_mkepair)
	vnet_mkjail genevetest1 ${epair}a
	vnet_mkjail genevetest2 ${epair}b

	ifconfig -j genevetest1 ${epair}a inet ${endpoint1}/24 up
	ifconfig -j genevetest2 ${epair}b inet ${endpoint2}/24 up

	# manually add the multicast routes to epairs
	route -j genevetest1 add -net 239.0.0.0/8 -interface ${epair}a
	route -j genevetest2 add -net 239.0.0.0/8 -interface ${epair}b

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 create genevemode l2 debug \
	    geneveid $vni1 genevelocal ${endpoint1} genevegroup ${mc_group} up
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 create genevemode l2 debug \
	    geneveid $vni1 genevelocal ${endpoint2} genevegroup ${mc_group} up

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet ${v4tunnel1}/24
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet6 ${v6tunnel1}
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet ${v4tunnel2}/24
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet6 ${v6tunnel2}

	atf_check -s exit:0 -o match:"group 239.0.0.1" jexec genevetest1 ifmcstat -i ${epair}a -f inet
	atf_check -s exit:0 -o match:"group 239.0.0.1" jexec genevetest2 ifmcstat -i ${epair}b -f inet

	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v4tunnel2
	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v6tunnel2
	atf_check -s exit:0 -o ignore jexec genevetest2 ping -nc 1 -t 1 $v4tunnel1
	atf_check -s exit:0 -o ignore jexec genevetest2 ping -nc 1 -t 1 $v6tunnel1

}

ether_ipv4_multicast_without_dev_cleanup()
{
	vnet_cleanup
}


atf_test_case "ether_ipv6_multicast_without_dev" "cleanup"
ether_ipv6_multicast_without_dev_head()
{
	atf_set descr 'Create a geneve(4) l2 ipv6 multicast tunnel without specifying genevedev using epair and pass traffic between jails'
	atf_set require.user root
}

ether_ipv6_multicast_without_dev_body()
{
	local epair geneve1 geneve2 vni1 endpoint1 endpoint2 mc_group
        local v4tunnel1 v4tunnel2 v6tunnel1 v6tunnel2

	endpoint1=3fff::1
	endpoint2=3fff::2
	mc_group=ff08::db8:0:1
	v4tunnel1=169.254.0.1
	v4tunnel2=169.254.0.2
	v6tunnel1=2001:db8::1
	v6tunnel2=2001:db8::2
	vni1=1

	if ! kldstat -q -m if_geneve; then
		atf_skip "This test requires if_geneve"
	fi
	if ! kldstat -q -m ip6_mroute; then
		atf_skip "This test requires ip6_mroute"
	fi

	vnet_init
	epair=$(vnet_mkepair)
	vnet_mkjail genevetest1 ${epair}a
	vnet_mkjail genevetest2 ${epair}b

	ifconfig -j genevetest1 ${epair}a inet6 ${endpoint1} up
	ifconfig -j genevetest2 ${epair}b inet6 ${endpoint2} up

	# manually add the multicast routes to epairs
	route -j genevetest1 -n6 add -net ff08::db8:0:1/96 -interface ${epair}a
	route -j genevetest2 -n6 add -net ff08::db8:0:1/96 -interface ${epair}b

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 create genevemode l2 debug \
	    geneveid $vni1 genevelocal ${endpoint1} genevegroup ${mc_group} up
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 create genevemode l2 debug \
	    geneveid $vni1 genevelocal ${endpoint2} genevegroup ${mc_group} up

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet ${v4tunnel1}/24
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet6 ${v6tunnel1}
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet ${v4tunnel2}/24
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet6 ${v6tunnel2}

	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v4tunnel2
	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v6tunnel2
	atf_check -s exit:0 -o ignore jexec genevetest2 ping -nc 1 -t 1 $v4tunnel1
	atf_check -s exit:0 -o ignore jexec genevetest2 ping -nc 1 -t 1 $v6tunnel1

}

ether_ipv6_multicast_without_dev_cleanup()
{
	vnet_cleanup
}

atf_test_case "inherit_ipv4_multicast" "cleanup"
inherit_ipv4_multicast_head()
{
	atf_set descr 'Create a geneve(4) l3 ipv4 multicast tunnel using epair and pass traffic between jails'
	atf_set require.user root
}

inherit_ipv4_multicast_body()
{
	local epair geneve1 geneve2 vni1 endpoint1 endpoint2 mc_group
        local v4tunnel1 v4tunnel2 v6tunnel1 v6tunnel2

	endpoint1=192.168.2.1
	endpoint2=192.168.2.2
	mc_group=239.0.0.1
	v4tunnel1=169.254.0.1
	v4tunnel2=169.254.0.2
	v6tunnel1=2001:db8::1
	v6tunnel2=2001:db8::2
	vni1=1

	if ! kldstat -q -m if_geneve; then
		atf_skip "This test requires if_geneve"
	fi
	if ! kldstat -q -m ip_mroute; then
		atf_skip "This test requires ip_mroute"
	fi

	vnet_init
	epair=$(vnet_mkepair)
	vnet_mkjail genevetest1 ${epair}a
	vnet_mkjail genevetest2 ${epair}b

	ifconfig -j genevetest1 ${epair}a inet ${endpoint1}/24 up
	ifconfig -j genevetest2 ${epair}b inet ${endpoint2}/24 up

	# manually add the multicast routes to epairs
	route -j genevetest1 add -net 239.0.0.0/8 -interface ${epair}a
	route -j genevetest2 add -net 239.0.0.0/8 -interface ${epair}b

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 create genevemode l3 debug \
	    geneveid $vni1 genevelocal ${endpoint1} \
	    genevegroup ${mc_group} genevedev ${epair}a up
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 create genevemode l3 debug \
	    geneveid $vni1 genevelocal ${endpoint2} \
	    genevegroup ${mc_group} genevedev ${epair}b up

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet ${v4tunnel1}/30
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet6 ${v6tunnel1}
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet ${v4tunnel2}/30
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet6 ${v6tunnel2}

	atf_check -s exit:0 -o ignore sysctl -j genevetest1 net.inet.icmp.bmcastecho=1
	atf_check -s exit:0 -o ignore sysctl -j genevetest2 net.inet.icmp.bmcastecho=1

	atf_check -s exit:0 -o match:"group 239.0.0.1" jexec genevetest1 ifmcstat -i ${epair}a -f inet
	atf_check -s exit:0 -o match:"group 239.0.0.1" jexec genevetest2 ifmcstat -i ${epair}b -f inet

	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v4tunnel2
	atf_check -s exit:0 -o ignore jexec genevetest2 ping -nc 1 -t 1 $v4tunnel1
	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v6tunnel2
	atf_check -s exit:0 -o ignore jexec genevetest2 ping -nc 1 -t 1 $v6tunnel1

}

inherit_ipv4_multicast_cleanup()
{
	vnet_cleanup
}

atf_test_case "inherit_ipv6_multicast" "cleanup"
inherit_ipv6_multicast_head()
{
	atf_set descr 'Create a geneve(4) l3 ipv6 multicast tunnel using epair and pass traffic between jails'
	atf_set require.user root
}

inherit_ipv6_multicast_body()
{
	local epair geneve1 geneve2 vni1 endpoint1 endpoint2 mc_group
        local v4tunnel1 v4tunnel2 v6tunnel1 v6tunnel2

	endpoint1=3fff::1
	endpoint2=3fff::2
	mc_group=ff08::db8:0:1
	v4tunnel1=169.254.0.1
	v4tunnel2=169.254.0.2
	v6tunnel1=2001:db8::1
	v6tunnel2=2001:db8::2
	vni1=1

	if ! kldstat -q -m if_geneve; then
		atf_skip "This test requires if_geneve"
	fi
	if ! kldstat -q -m ip6_mroute; then
		atf_skip "This test requires ip6_mroute"
	fi

	vnet_init
	epair=$(vnet_mkepair)
	vnet_mkjail genevetest1 ${epair}a
	vnet_mkjail genevetest2 ${epair}b

	ifconfig -j genevetest1 ${epair}a inet6 ${endpoint1} up
	ifconfig -j genevetest2 ${epair}b inet6 ${endpoint2} up

	# manually add the multicast routes to epairs
	route -j genevetest1 -n6 add -net ff08::db8:0:1/96 -interface ${epair}a
	route -j genevetest2 -n6 add -net ff08::db8:0:1/96 -interface ${epair}b

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 create genevemode l3 debug \
	    geneveid $vni1 genevelocal ${endpoint1} \
	    genevegroup ${mc_group} genevedev ${epair}a up
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 create genevemode l3 debug \
	    geneveid $vni1 genevelocal ${endpoint2} \
	    genevegroup ${mc_group} genevedev ${epair}b up

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet ${v4tunnel1}/30
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet6 ${v6tunnel1}
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet ${v4tunnel2}/30
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet6 ${v6tunnel2}

	atf_check -s exit:0 -o ignore sysctl -j genevetest1 net.inet.icmp.bmcastecho=1
	atf_check -s exit:0 -o ignore sysctl -j genevetest2 net.inet.icmp.bmcastecho=1

	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v6tunnel2
	atf_check -s exit:0 -o ignore jexec genevetest2 ping -nc 1 -t 1 $v6tunnel1
	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v4tunnel2
	atf_check -s exit:0 -o ignore jexec genevetest2 ping -nc 1 -t 1 $v4tunnel1

}

inherit_ipv6_multicast_cleanup()
{
	vnet_cleanup
}

atf_test_case "inherit_ipv4_multicast_without_dev" "cleanup"
inherit_ipv4_multicast_without_dev_head()
{
	atf_set descr 'Create a geneve(4) l3 ipv4 multicast tunnel without specifying genevedev using epair and pass traffic between jails'
	atf_set require.user root
}

inherit_ipv4_multicast_without_dev_body()
{
	local epair geneve1 geneve2 vni1 endpoint1 endpoint2 mc_group
        local v4tunnel1 v4tunnel2 v6tunnel1 v6tunnel2

	endpoint1=192.168.2.1
	endpoint2=192.168.2.2
	mc_group=239.0.0.1
	v4tunnel1=169.254.0.1
	v4tunnel2=169.254.0.2
	v6tunnel1=2001:db8::1
	v6tunnel2=2001:db8::2
	vni1=1

	if ! kldstat -q -m if_geneve; then
		atf_skip "This test requires if_geneve"
	fi
	if ! kldstat -q -m ip_mroute; then
		atf_skip "This test requires ip_mroute"
	fi

	vnet_init
	epair=$(vnet_mkepair)
	vnet_mkjail genevetest1 ${epair}a
	vnet_mkjail genevetest2 ${epair}b

	ifconfig -j genevetest1 ${epair}a inet ${endpoint1}/24 up
	ifconfig -j genevetest2 ${epair}b inet ${endpoint2}/24 up

	# manually add the multicast routes to epairs
	route -j genevetest1 add -net 239.0.0.0/8 -interface ${epair}a
	route -j genevetest2 add -net 239.0.0.0/8 -interface ${epair}b

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 create genevemode l3 debug \
	    geneveid $vni1 genevelocal ${endpoint1} genevegroup ${mc_group} up
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 create genevemode l3 debug \
	    geneveid $vni1 genevelocal ${endpoint2} genevegroup ${mc_group} up

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet ${v4tunnel1}/30
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet6 ${v6tunnel1}
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet ${v4tunnel2}/30
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet6 ${v6tunnel2}

	atf_check -s exit:0 -o ignore sysctl -j genevetest1 net.inet.icmp.bmcastecho=1
	atf_check -s exit:0 -o ignore sysctl -j genevetest2 net.inet.icmp.bmcastecho=1

	atf_check -s exit:0 -o match:"group 239.0.0.1" jexec genevetest1 ifmcstat -i ${epair}a -f inet
	atf_check -s exit:0 -o match:"group 239.0.0.1" jexec genevetest2 ifmcstat -i ${epair}b -f inet

	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v4tunnel2
	atf_check -s exit:0 -o ignore jexec genevetest2 ping -nc 1 -t 1 $v4tunnel1
	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v6tunnel2
	atf_check -s exit:0 -o ignore jexec genevetest2 ping -nc 1 -t 1 $v6tunnel1

}

inherit_ipv4_multicast_without_dev_cleanup()
{
	vnet_cleanup
}


atf_test_case "inherit_ipv6_multicast_without_dev" "cleanup"
inherit_ipv6_multicast_without_dev_head()
{
	atf_set descr 'Create a geneve(4) l3 ipv6 multicast tunnel without specifying genevedev using epair and pass traffic between jails'
	atf_set require.user root
}

inherit_ipv6_multicast_without_dev_body()
{
	local epair geneve1 geneve2 vni1 endpoint1 endpoint2 mc_group
        local v4tunnel1 v4tunnel2 v6tunnel1 v6tunnel2

	endpoint1=3fff::1
	endpoint2=3fff::2
	mc_group=ff08::db8:0:1
	v4tunnel1=169.254.0.1
	v4tunnel2=169.254.0.2
	v6tunnel1=2001:db8::1
	v6tunnel2=2001:db8::2
	vni1=1

	if ! kldstat -q -m if_geneve; then
		atf_skip "This test requires if_geneve"
	fi
	if ! kldstat -q -m ip6_mroute; then
		atf_skip "This test requires ip6_mroute"
	fi

	vnet_init
	epair=$(vnet_mkepair)
	vnet_mkjail genevetest1 ${epair}a
	vnet_mkjail genevetest2 ${epair}b

	ifconfig -j genevetest1 ${epair}a inet6 ${endpoint1} up
	ifconfig -j genevetest2 ${epair}b inet6 ${endpoint2} up

	# manually add the multicast routes to epairs
	route -j genevetest1 -n6 add -net ff08::db8:0:1/96 -interface ${epair}a
	route -j genevetest2 -n6 add -net ff08::db8:0:1/96 -interface ${epair}b

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 create genevemode l3 debug \
	    geneveid $vni1 genevelocal ${endpoint1} genevegroup ${mc_group} up
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 create genevemode l3 debug \
	    geneveid $vni1 genevelocal ${endpoint2} genevegroup ${mc_group} up

	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet ${v4tunnel1}/30
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest1 geneve1 inet6 ${v6tunnel1}
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet ${v4tunnel2}/30
	atf_check -s exit:0 -o ignore \
	    ifconfig -j genevetest2 geneve1 inet6 ${v6tunnel2}

	atf_check -s exit:0 -o ignore sysctl -j genevetest1 net.inet.icmp.bmcastecho=1
	atf_check -s exit:0 -o ignore sysctl -j genevetest2 net.inet.icmp.bmcastecho=1

	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v6tunnel2
	atf_check -s exit:0 -o ignore jexec genevetest2 ping -nc 1 -t 1 $v6tunnel1
	atf_check -s exit:0 -o ignore jexec genevetest1 ping -nc 1 -t 1 $v4tunnel2
	atf_check -s exit:0 -o ignore jexec genevetest2 ping -nc 1 -t 1 $v4tunnel1

}

inherit_ipv6_multicast_without_dev_cleanup()
{
	vnet_cleanup
}


atf_init_test_cases()
{
	atf_add_test_case "ether_ipv4"
	atf_add_test_case "ether_ipv4_multicast"
	atf_add_test_case "ether_ipv4_multicast_without_dev"
	atf_add_test_case "ether_ipv6"
	atf_add_test_case "ether_ipv6_blind_options"
	atf_add_test_case "ether_ipv6_external"
	atf_add_test_case "ether_ipv6_multicast"
	atf_add_test_case "ether_ipv6_multicast_without_dev"
	atf_add_test_case "inherit_ipv4"
	atf_add_test_case "inherit_ipv4_multicast"
	atf_add_test_case "inherit_ipv4_multicast_without_dev"
	atf_add_test_case "inherit_ipv6"
	atf_add_test_case "inherit_ipv6_multicast"
	atf_add_test_case "inherit_ipv6_multicast_without_dev"
}
