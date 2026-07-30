#
# Copyright (c) 2026 Pouria Mousavizadeh Tehrani <pouria@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

. $(atf_get_srcdir)/../common/vnet.subr

atf_test_case "gre4_basic" "cleanup"
gre4_basic_head()
{
	atf_set descr 'Create a gre(4) tunnel over ipv4 underlay'
	atf_set require.user root
	atf_set require.kmods if_gre
}
gre4_basic_body()
{
	local epair endpoint1 endpoint2
        local v4tunnel1 v4tunnel2 v6tunnel1 v6tunnel2

	endpoint1=192.168.2.1
	endpoint2=192.168.2.2
	v4tunnel1=169.254.0.1
	v4tunnel2=169.254.0.2
	v6tunnel1=2001:db8::1
	v6tunnel2=2001:db8::2

	vnet_init
	epair=$(vnet_mkepair)
	vnet_mkjail gretest1 ${epair}a
	vnet_mkjail gretest2 ${epair}b

	ifconfig -j gretest1 ${epair}a inet ${endpoint1}/30 up
	ifconfig -j gretest2 ${epair}b inet ${endpoint2}/30 up

	# Sanity check
	atf_check -o ignore \
	    jexec gretest1 ping -c 1 -t 3 ${endpoint2}
	atf_check -o ignore \
	    jexec gretest2 ping -c 1 -t 3 ${endpoint1}

	atf_check ifconfig -j gretest1 gre1 create \
	    inet tunnel ${endpoint1} ${endpoint2} up
	atf_check ifconfig -j gretest2 gre1 create \
	    inet tunnel ${endpoint2} ${endpoint1} up

	atf_check ifconfig -j gretest1 gre1 inet ${v4tunnel1} ${v4tunnel2}
	atf_check ifconfig -j gretest1 gre1 inet6 ${v6tunnel1}
	atf_check ifconfig -j gretest2 gre1 inet ${v4tunnel2} ${v4tunnel1}
	atf_check ifconfig -j gretest2 gre1 inet6 ${v6tunnel2}

	# Tunnel test
	atf_check -o ignore jexec gretest1 ping -nc 1 -t 1 $v4tunnel2
	atf_check -o ignore jexec gretest2 ping -nc 1 -t 1 $v4tunnel1
	atf_check -o ignore jexec gretest1 ping -nc 1 -t 1 $v6tunnel2
	atf_check -o ignore jexec gretest2 ping -nc 1 -t 1 $v6tunnel1
}
gre4_basic_cleanup()
{
	vnet_cleanup
}

atf_test_case "gre6_basic" "cleanup"
gre6_basic_head()
{
	atf_set descr 'Create a gre(4) tunnel over ipv6 underlay'
	atf_set require.user root
	atf_set require.kmods if_gre
}
gre6_basic_body()
{
	local epair endpoint1 endpoint2
        local v4tunnel1 v4tunnel2 v6tunnel1 v6tunnel2

	endpoint1=3fff::1
	endpoint2=3fff::2
	v4tunnel1=169.254.0.1
	v4tunnel2=169.254.0.2
	v6tunnel1=2001:db8::1
	v6tunnel2=2001:db8::2

	vnet_init
	epair=$(vnet_mkepair)
	vnet_mkjail gretest1 ${epair}a
	vnet_mkjail gretest2 ${epair}b

	ifconfig -j gretest1 ${epair}a inet6 ${endpoint1} up
	ifconfig -j gretest2 ${epair}b inet6 ${endpoint2} up

	# Sanity check
	atf_check -o ignore \
	    jexec gretest1 ping -c 1 -t 3 ${endpoint2}
	atf_check -o ignore \
	    jexec gretest2 ping -c 1 -t 3 ${endpoint1}

	atf_check ifconfig -j gretest1 gre1 create \
	    inet6 tunnel ${endpoint1} ${endpoint2} up
	atf_check ifconfig -j gretest2 gre1 create \
	    inet6 tunnel ${endpoint2} ${endpoint1} up

	atf_check ifconfig -j gretest1 gre1 inet ${v4tunnel1} ${v4tunnel2}
	atf_check ifconfig -j gretest1 gre1 inet6 ${v6tunnel1}
	atf_check ifconfig -j gretest2 gre1 inet ${v4tunnel2} ${v4tunnel1}
	atf_check ifconfig -j gretest2 gre1 inet6 ${v6tunnel2}

	# Tunnel test
	atf_check -o ignore jexec gretest1 ping -nc 1 -t 1 $v4tunnel2
	atf_check -o ignore jexec gretest2 ping -nc 1 -t 1 $v4tunnel1
	atf_check -o ignore jexec gretest1 ping -nc 1 -t 1 $v6tunnel2
	atf_check -o ignore jexec gretest2 ping -nc 1 -t 1 $v6tunnel1
}
gre6_basic_cleanup()
{
	vnet_cleanup
}

atf_test_case "gre6_csum" "cleanup"
gre6_csum_head()
{
	atf_set descr 'Create a gre(4) tunnel over ipv6 underlay with checksum option'
	atf_set require.user root
	atf_set require.kmods if_gre
}
gre6_csum_body()
{
	local epair endpoint1 endpoint2
	local v4tunnel1 v4tunnel2 v6tunnel1 v6tunnel2

	endpoint1=3fff::1
	endpoint2=3fff::2
	v4tunnel1=169.254.0.1
	v4tunnel2=169.254.0.2
	v6tunnel1=2001:db8::1
	v6tunnel2=2001:db8::2

	vnet_init
	epair=$(vnet_mkepair)
	vnet_mkjail gretest1 ${epair}a
	vnet_mkjail gretest2 ${epair}b

	ifconfig -j gretest1 ${epair}a inet6 ${endpoint1} up
	ifconfig -j gretest2 ${epair}b inet6 ${endpoint2} up

	# Sanity check
	atf_check -o ignore \
	    jexec gretest1 ping -c 1 -t 3 ${endpoint2}
	atf_check -o ignore \
	    jexec gretest2 ping -c 1 -t 3 ${endpoint1}

	atf_check ifconfig -j gretest1 gre1 create \
	    inet6 tunnel ${endpoint1} ${endpoint2} enable_csum up
	atf_check ifconfig -j gretest2 gre1 create \
	    inet6 tunnel ${endpoint2} ${endpoint1} enable_csum up

	atf_check ifconfig -j gretest1 gre1 inet ${v4tunnel1} ${v4tunnel2}
	atf_check ifconfig -j gretest1 gre1 inet6 ${v6tunnel1}
	atf_check ifconfig -j gretest2 gre1 inet ${v4tunnel2} ${v4tunnel1}
	atf_check ifconfig -j gretest2 gre1 inet6 ${v6tunnel2}

	# ifconfig test
	atf_check -o match:"ENABLE_CSUM" ifconfig -j gretest1 gre1
	atf_check -o match:"ENABLE_CSUM" ifconfig -j gretest2 gre1

	# Tunnel test
	atf_check -o ignore jexec gretest1 ping -nc 1 -t 1 $v4tunnel2
	atf_check -o ignore jexec gretest2 ping -nc 1 -t 1 $v4tunnel1
	atf_check -o ignore jexec gretest1 ping -nc 1 -t 1 $v6tunnel2
	atf_check -o ignore jexec gretest2 ping -nc 1 -t 1 $v6tunnel1
}
gre6_csum_cleanup()
{
	vnet_cleanup
}

atf_test_case "gre6_key" "cleanup"
gre6_key_head()
{
	atf_set descr 'Create a gre(4) tunnel over ipv6 underlay with gre key option'
	atf_set require.user root
	atf_set require.kmods if_gre
}
gre6_key_body()
{
	local epair endpoint1 endpoint2
        local v4tunnel1 v4tunnel2 v6tunnel1 v6tunnel2

	endpoint1=3fff::1
	endpoint2=3fff::2
	v4tunnel1=169.254.0.1
	v4tunnel2=169.254.0.2
	v6tunnel1=2001:db8::1
	v6tunnel2=2001:db8::2

	vnet_init
	epair=$(vnet_mkepair)
	vnet_mkjail gretest1 ${epair}a
	vnet_mkjail gretest2 ${epair}b

	ifconfig -j gretest1 ${epair}a inet6 ${endpoint1} up
	ifconfig -j gretest2 ${epair}b inet6 ${endpoint2} up

	# Sanity check
	atf_check -o ignore \
	    jexec gretest1 ping -c 1 -t 3 ${endpoint2}
	atf_check -o ignore \
	    jexec gretest2 ping -c 1 -t 3 ${endpoint1}

	atf_check ifconfig -j gretest1 gre1 create \
	    inet6 tunnel ${endpoint1} ${endpoint2} grekey 10 up
	atf_check ifconfig -j gretest2 gre1 create \
	    inet6 tunnel ${endpoint2} ${endpoint1} grekey 10 up

	atf_check ifconfig -j gretest1 gre1 inet ${v4tunnel1} ${v4tunnel2}
	atf_check ifconfig -j gretest1 gre1 inet6 ${v6tunnel1}
	atf_check ifconfig -j gretest2 gre1 inet ${v4tunnel2} ${v4tunnel1}
	atf_check ifconfig -j gretest2 gre1 inet6 ${v6tunnel2}

	# ifconfig test
	atf_check -o match:"grekey: 0xa \(10\)" ifconfig -j gretest1 gre1
	atf_check -o match:"grekey: 0xa \(10\)" ifconfig -j gretest2 gre1

	# Tunnel test
	atf_check -o ignore jexec gretest1 ping -nc 1 -t 1 $v4tunnel2
	atf_check -o ignore jexec gretest2 ping -nc 1 -t 1 $v4tunnel1
	atf_check -o ignore jexec gretest1 ping -nc 1 -t 1 $v6tunnel2
	atf_check -o ignore jexec gretest2 ping -nc 1 -t 1 $v6tunnel1
}
gre6_key_cleanup()
{
	vnet_cleanup
}

atf_test_case "gre6_seq" "cleanup"
gre6_seq_head()
{
	atf_set descr 'Create a sequenced gre(4) tunnel over ipv6 underlay'
	atf_set require.user root
	atf_set require.kmods if_gre
}
gre6_seq_body()
{
	local epair endpoint1 endpoint2
	local v4tunnel1 v4tunnel2 v6tunnel1 v6tunnel2

	endpoint1=3fff::1
	endpoint2=3fff::2
	v4tunnel1=169.254.0.1
	v4tunnel2=169.254.0.2
	v6tunnel1=2001:db8::1
	v6tunnel2=2001:db8::2

	vnet_init
	epair=$(vnet_mkepair)
	vnet_mkjail gretest1 ${epair}a
	vnet_mkjail gretest2 ${epair}b

	ifconfig -j gretest1 ${epair}a inet6 ${endpoint1} up
	ifconfig -j gretest2 ${epair}b inet6 ${endpoint2} up

	# Sanity check
	atf_check -o ignore \
	    jexec gretest1 ping -c 1 -t 3 ${endpoint2}
	atf_check -o ignore \
	    jexec gretest2 ping -c 1 -t 3 ${endpoint1}

	atf_check ifconfig -j gretest1 gre1 create \
	    inet6 tunnel ${endpoint1} ${endpoint2} enable_seq up
	atf_check ifconfig -j gretest2 gre1 create \
	    inet6 tunnel ${endpoint2} ${endpoint1} enable_seq up

	atf_check ifconfig -j gretest1 gre1 inet ${v4tunnel1} ${v4tunnel2}
	atf_check ifconfig -j gretest1 gre1 inet6 ${v6tunnel1}
	atf_check ifconfig -j gretest2 gre1 inet ${v4tunnel2} ${v4tunnel1}
	atf_check ifconfig -j gretest2 gre1 inet6 ${v6tunnel2}

	# ifconfig test
	atf_check -o match:"ENABLE_SEQ" ifconfig -j gretest1 gre1
	atf_check -o match:"ENABLE_SEQ" ifconfig -j gretest2 gre1

	# Tunnel test
	atf_check -o ignore jexec gretest1 ping -nc 1 -t 1 $v4tunnel2
	atf_check -o ignore jexec gretest2 ping -nc 1 -t 1 $v4tunnel1
	atf_check -o ignore jexec gretest1 ping -nc 1 -t 1 $v6tunnel2
	atf_check -o ignore jexec gretest2 ping -nc 1 -t 1 $v6tunnel1
}
gre6_seq_cleanup()
{
	vnet_cleanup
}

atf_test_case "gre6_udpencap" "cleanup"
gre6_udpencap_head()
{
	atf_set descr 'Create a gre(4) over UDP tunnel with ipv6 underlay'
	atf_set require.user root
	atf_set require.kmods if_gre
}
gre6_udpencap_body()
{
	local epair endpoint1 endpoint2
        local v4tunnel1 v4tunnel2 v6tunnel1 v6tunnel2

	endpoint1=3fff::1
	endpoint2=3fff::2
	v4tunnel1=169.254.0.1
	v4tunnel2=169.254.0.2
	v6tunnel1=2001:db8::1
	v6tunnel2=2001:db8::2

	vnet_init
	epair=$(vnet_mkepair)
	vnet_mkjail gretest1 ${epair}a
	vnet_mkjail gretest2 ${epair}b

	ifconfig -j gretest1 ${epair}a inet6 ${endpoint1} up
	ifconfig -j gretest2 ${epair}b inet6 ${endpoint2} up

	# Sanity check
	atf_check -o ignore \
	    jexec gretest1 ping -c 1 -t 3 ${endpoint2}
	atf_check -o ignore \
	    jexec gretest2 ping -c 1 -t 3 ${endpoint1}

	atf_check ifconfig -j gretest1 gre1 create \
	    inet6 tunnel ${endpoint1} ${endpoint2} udpencap up
	atf_check ifconfig -j gretest2 gre1 create \
	    inet6 tunnel ${endpoint2} ${endpoint1} udpencap udpport 55555 up

	atf_check ifconfig -j gretest1 gre1 inet ${v4tunnel1} ${v4tunnel2}
	atf_check ifconfig -j gretest1 gre1 inet6 ${v6tunnel1}
	atf_check ifconfig -j gretest2 gre1 inet ${v4tunnel2} ${v4tunnel1}
	atf_check ifconfig -j gretest2 gre1 inet6 ${v6tunnel2}

	# ifconfig test
	atf_check -o match:"UDPENCAP" ifconfig -j gretest1 gre1
	atf_check -o match:"udpport: 55555" ifconfig -j gretest2 gre1

	# Tunnel test
	atf_check -o ignore jexec gretest1 ping -nc 1 -t 1 $v4tunnel2
	atf_check -o ignore jexec gretest2 ping -nc 1 -t 1 $v4tunnel1
	atf_check -o ignore jexec gretest1 ping -nc 1 -t 1 $v6tunnel2
	atf_check -o ignore jexec gretest2 ping -nc 1 -t 1 $v6tunnel1
}
gre6_udpencap_cleanup()
{
	vnet_cleanup
}

atf_test_case "gre_blind_options" "cleanup"
gre_blind_options_head()
{
	atf_set descr 'Test gre(4) tunnel options without verifying its effect on dataplane.'
	atf_set require.user root
	atf_set require.kmods if_gre
}
gre_blind_options_body()
{
	local gre

	v4tunnel1=169.254.0.1
	v4tunnel2=169.254.0.2
	v6tunnel1=2001:db8::1
	v6tunnel2=2001:db8::2

	vnet_init
	vnet_mkjail gretest

	gre=$(ifconfig -j gretest gre create \
		enable_seq grekey 10 enable_csum udpencap udpport 55555 up)

	atf_check ifconfig -j gretest $gre inet ${v4tunnel1} ${v4tunnel2}
	atf_check ifconfig -j gretest $gre inet6 ${v6tunnel1}

	# ifconfig test
	atf_check -o match:"grekey: 0xa \(10\)" ifconfig -j gretest $gre
	atf_check -o match:"ENABLE_CSUM" ifconfig -j gretest $gre
	atf_check -o match:"ENABLE_SEQ" ifconfig -j gretest $gre
	atf_check -o match:"UDPENCAP" ifconfig -j gretest $gre
	atf_check -o match:"udpport: 55555" ifconfig -j gretest $gre
}
gre_blind_options_cleanup()
{
	vnet_cleanup
}


atf_init_test_cases()
{
	atf_add_test_case "gre4_basic"
	atf_add_test_case "gre6_basic"
	atf_add_test_case "gre6_csum"
	atf_add_test_case "gre6_key"
	atf_add_test_case "gre6_seq"
	atf_add_test_case "gre6_udpencap"
	atf_add_test_case "gre_blind_options"
}
