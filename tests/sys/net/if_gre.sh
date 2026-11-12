#
# Copyright (c) 2026 Pouria Mousavizadeh Tehrani <pouria@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

. $(atf_get_srcdir)/../common/vnet.subr

gre_setup_endpoints()
{
	local af endpoint1 endpoint2 epair plen

	af=$1
	endpoint1=$2
	endpoint2=$3
	if [ "${af}" = "inet" ]; then
		plen=/30
	else
		plen=
	fi

	vnet_init
	epair=$(vnet_mkepair)
	vnet_mkjail gretest1 ${epair}a
	vnet_mkjail gretest2 ${epair}b

	ifconfig -j gretest1 ${epair}a ${af} ${endpoint1}${plen} up
	ifconfig -j gretest2 ${epair}b ${af} ${endpoint2}${plen} up

	# Sanity check
	atf_check -o ignore \
	    jexec gretest1 ping -c 1 -t 3 ${endpoint2}
	atf_check -o ignore \
	    jexec gretest2 ping -c 1 -t 3 ${endpoint1}
}

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
	atf_check -o match:"greokey: 0xa \(10\)" -o match:"greikey: 0xa \(10\)" \
	    ifconfig -j gretest1 gre1
	atf_check -o match:"greokey: 0xa \(10\)" -o match:"greikey: 0xa \(10\)" \
	    ifconfig -j gretest2 gre1

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
	atf_check -o match:"greokey: 0xa \(10\)" ifconfig -j gretest $gre
	atf_check -o match:"ENABLE_CSUM" ifconfig -j gretest $gre
	atf_check -o match:"ENABLE_SEQ" ifconfig -j gretest $gre
	atf_check -o match:"UDPENCAP" ifconfig -j gretest $gre
	atf_check -o match:"udpport: 55555" ifconfig -j gretest $gre
}
gre_blind_options_cleanup()
{
	vnet_cleanup
}

gre_multikey_test()
{
	local af endpoint1 endpoint2 encap
	local tunnel1a tunnel1b tunnel2a tunnel2b

	af=$1
	endpoint1=$2
	endpoint2=$3
	encap=$4
	tunnel1a=192.0.2.1
	tunnel1b=192.0.2.2
	tunnel2a=198.51.100.1
	tunnel2b=198.51.100.2

	gre_setup_endpoints ${af} ${endpoint1} ${endpoint2}

	atf_check ifconfig -j gretest1 gre1 create ${af} \
	    tunnel ${endpoint1} ${endpoint2} greikey 10 greokey 10 ${encap} up
	atf_check ifconfig -j gretest1 gre2 create ${af} \
	    tunnel ${endpoint1} ${endpoint2} greikey 20 greokey 20 ${encap} up
	atf_check ifconfig -j gretest2 gre1 create ${af} \
	    tunnel ${endpoint2} ${endpoint1} greikey 10 greokey 10 ${encap} up
	atf_check ifconfig -j gretest2 gre2 create ${af} \
	    tunnel ${endpoint2} ${endpoint1} greikey 20 greokey 20 ${encap} up
	atf_check -o match:"greikey: 0xa \(10\)" -o match:"greokey: 0xa \(10\)" \
	    ifconfig -j gretest1 gre1
	atf_check -o match:"greikey: 0x14 \(20\)" -o match:"greokey: 0x14 \(20\)" \
	    ifconfig -j gretest1 gre2
	if [ -n "${encap}" ]; then
		atf_check -o match:"UDPENCAP" ifconfig -j gretest1 gre2
		atf_check -o match:"UDPENCAP" ifconfig -j gretest2 gre2
	fi

	# The duplicate is detected when the tunnel addresses are set,
	# ifconfig only warns when it fails to set them.
	atf_check -s ignore -e ignore ifconfig -j gretest1 gre3 create ${af} \
	    greikey 10 ${encap} tunnel ${endpoint1} ${endpoint2}
	atf_check -o not-match:"tunnel inet" ifconfig -j gretest1 gre3
	atf_check ifconfig -j gretest1 gre3 destroy

	# The duplicate is detected when the incoming key is set
	atf_check -s not-exit:0 -e ignore ifconfig -j gretest1 gre3 create \
	    ${af} tunnel ${endpoint1} ${endpoint2} ${encap} greikey 10
	atf_check ifconfig -j gretest1 gre3 destroy
	atf_check -s not-exit:0 -e ignore ifconfig -j gretest1 gre2 greikey 10
	atf_check -o match:"greikey: 0x14 \(20\)" ifconfig -j gretest1 gre2

	# The outgoing key is not a part of the duplicate check
	atf_check ifconfig -j gretest1 gre2 greokey 10 up
	atf_check -o match:"greokey: 0xa \(10\)" ifconfig -j gretest1 gre2
	atf_check ifconfig -j gretest1 gre2 greokey 20 up

	atf_check ifconfig -j gretest1 gre1 inet ${tunnel1a} ${tunnel1b}
	atf_check ifconfig -j gretest1 gre2 inet ${tunnel2a} ${tunnel2b}
	atf_check ifconfig -j gretest2 gre1 inet ${tunnel1b} ${tunnel1a}
	atf_check ifconfig -j gretest2 gre2 inet ${tunnel2b} ${tunnel2a}

	atf_check -o ignore jexec gretest1 ping -nc 1 -t 1 ${tunnel1b}
	atf_check -o ignore jexec gretest1 ping -nc 1 -t 1 ${tunnel2b}
	atf_check -o ignore jexec gretest2 ping -nc 1 -t 1 ${tunnel1a}
	atf_check -o ignore jexec gretest2 ping -nc 1 -t 1 ${tunnel2a}

	# Replies with an unknown key are not accepted by the other tunnel
	atf_check ifconfig -j gretest2 gre2 greokey 30 up
	atf_check -s exit:2 -o ignore \
	    jexec gretest1 ping -nc 1 -t 1 ${tunnel2b}
	atf_check -o ignore jexec gretest1 ping -nc 1 -t 1 ${tunnel1b}
	atf_check ifconfig -j gretest2 gre2 greokey 20 up
	atf_check -o ignore jexec gretest1 ping -nc 1 -t 1 ${tunnel2b}

	# Requests are not accepted when the incoming key does not match
	atf_check ifconfig -j gretest2 gre2 greikey 30 up
	atf_check -s exit:2 -o ignore \
	    jexec gretest1 ping -nc 1 -t 1 ${tunnel2b}
	atf_check -o ignore jexec gretest1 ping -nc 1 -t 1 ${tunnel1b}
	atf_check ifconfig -j gretest2 gre2 greikey 20 up
	atf_check -o ignore jexec gretest1 ping -nc 1 -t 1 ${tunnel2b}
}

atf_test_case "gre4_multikey" "cleanup"
gre4_multikey_head()
{
	atf_set descr 'Create two gre(4) tunnels between the same ipv4 endpoints with different gre keys'
	atf_set require.user root
	atf_set require.kmods if_gre
}
gre4_multikey_body()
{
	gre_multikey_test inet 203.0.113.1 203.0.113.2
}
gre4_multikey_cleanup()
{
	vnet_cleanup
}

atf_test_case "gre6_multikey" "cleanup"
gre6_multikey_head()
{
	atf_set descr 'Create two gre(4) tunnels between the same ipv6 endpoints with different gre keys'
	atf_set require.user root
	atf_set require.kmods if_gre
}
gre6_multikey_body()
{
	gre_multikey_test inet6 3fff::1 3fff::2
}
gre6_multikey_cleanup()
{
	vnet_cleanup
}

atf_test_case "gre4_multikey_udpencap" "cleanup"
gre4_multikey_udpencap_head()
{
	atf_set descr 'Create two gre(4) udp encapsulated tunnels between the same ipv4 endpoints with different gre keys'
	atf_set require.user root
	atf_set require.kmods if_gre
}
gre4_multikey_udpencap_body()
{
	gre_multikey_test inet 203.0.113.1 203.0.113.2 udpencap
}
gre4_multikey_udpencap_cleanup()
{
	vnet_cleanup
}

atf_test_case "gre6_multikey_udpencap" "cleanup"
gre6_multikey_udpencap_head()
{
	atf_set descr 'Create two gre(4) udp encapsulated tunnels between the same ipv6 endpoints with different gre keys'
	atf_set require.user root
	atf_set require.kmods if_gre
}
gre6_multikey_udpencap_body()
{
	gre_multikey_test inet6 3fff::1 3fff::2 udpencap
}
gre6_multikey_udpencap_cleanup()
{
	vnet_cleanup
}

# gre_key_fallback_test <af> <endpoint1> <endpoint2> [udpencap]
gre_key_fallback_test()
{
	local af endpoint1 endpoint2 encap v4tunnel1 v4tunnel2

	af=$1
	endpoint1=$2
	endpoint2=$3
	encap=$4
	v4tunnel1=192.0.2.1
	v4tunnel2=192.0.2.2

	gre_setup_endpoints ${af} ${endpoint1} ${endpoint2}

	# First jail sends packets with the key using gre1.
	# Replies have no key and are received by gre2.
	atf_check ifconfig -j gretest1 gre1 create ${af} \
	    tunnel ${endpoint1} ${endpoint2} greikey 10 greokey 10 ${encap} up
	atf_check ifconfig -j gretest1 gre2 create ${af} \
	    tunnel ${endpoint1} ${endpoint2} ${encap} up
	# The second jail has only the tunnel without the key
	atf_check ifconfig -j gretest2 gre1 create ${af} \
	    tunnel ${endpoint2} ${endpoint1} ${encap} up

	atf_check ifconfig -j gretest1 gre1 inet ${v4tunnel1} ${v4tunnel2}
	atf_check ifconfig -j gretest2 gre1 inet ${v4tunnel2} ${v4tunnel1}

	# By default the tunnel without the key accepts packets with any key
	atf_check -o inline:"1\n" \
	    sysctl -j gretest2 -n net.link.gre.key_fallback
	atf_check -o ignore jexec gretest1 ping -nc 1 -t 1 ${v4tunnel2}

	# Packets with unknown key are not accepted without fallback
	atf_check -o ignore \
	    sysctl -j gretest2 net.link.gre.key_fallback=0
	atf_check -s exit:2 -o ignore \
	    jexec gretest1 ping -nc 1 -t 1 ${v4tunnel2}

	atf_check -o ignore \
	    sysctl -j gretest2 net.link.gre.key_fallback=1
	atf_check -o ignore jexec gretest1 ping -nc 1 -t 1 ${v4tunnel2}
}

atf_test_case "gre4_key_fallback" "cleanup"
gre4_key_fallback_head()
{
	atf_set descr 'Test net.link.gre.key_fallback with gre key configured only on one side'
	atf_set require.user root
	atf_set require.kmods if_gre
}
gre4_key_fallback_body()
{
	gre_key_fallback_test inet 203.0.113.1 203.0.113.2
}
gre4_key_fallback_cleanup()
{
	vnet_cleanup
}

atf_test_case "gre6_key_fallback" "cleanup"
gre6_key_fallback_head()
{
	atf_set descr 'Test net.link.gre.key_fallback over ipv6 with gre key configured only on one side'
	atf_set require.user root
	atf_set require.kmods if_gre
}
gre6_key_fallback_body()
{
	gre_key_fallback_test inet6 3fff::1 3fff::2
}
gre6_key_fallback_cleanup()
{
	vnet_cleanup
}

atf_test_case "gre4_key_fallback_udpencap" "cleanup"
gre4_key_fallback_udpencap_head()
{
	atf_set descr 'Test net.link.gre.key_fallback over udp with gre key configured only on one side'
	atf_set require.user root
	atf_set require.kmods if_gre
}
gre4_key_fallback_udpencap_body()
{
	gre_key_fallback_test inet 203.0.113.1 203.0.113.2 udpencap
}
gre4_key_fallback_udpencap_cleanup()
{
	vnet_cleanup
}

atf_test_case "gre6_key_fallback_udpencap" "cleanup"
gre6_key_fallback_udpencap_head()
{
	atf_set descr 'Test net.link.gre.key_fallback over ipv6 udp with gre key configured only on one side'
	atf_set require.user root
	atf_set require.kmods if_gre
}
gre6_key_fallback_udpencap_body()
{
	gre_key_fallback_test inet6 3fff::1 3fff::2 udpencap
}
gre6_key_fallback_udpencap_cleanup()
{
	vnet_cleanup
}

# gre_key_mismatch_test <af> <endpoint1> <endpoint2> [udpencap]
gre_key_mismatch_test()
{
	local af endpoint1 endpoint2 encap v4tunnel1 v4tunnel2

	af=$1
	endpoint1=$2
	endpoint2=$3
	encap=$4
	v4tunnel1=192.0.2.1
	v4tunnel2=192.0.2.2

	gre_setup_endpoints ${af} ${endpoint1} ${endpoint2}

	atf_check ifconfig -j gretest1 gre1 create ${af} \
	    tunnel ${endpoint1} ${endpoint2} greikey 10 greokey 10 ${encap} up
	atf_check ifconfig -j gretest2 gre1 create ${af} \
	    tunnel ${endpoint2} ${endpoint1} greikey 20 greokey 20 ${encap} up

	atf_check ifconfig -j gretest1 gre1 inet ${v4tunnel1} ${v4tunnel2}
	atf_check ifconfig -j gretest2 gre1 inet ${v4tunnel2} ${v4tunnel1}

	# The only tunnel between the addresses accepts any key by default
	atf_check -o ignore jexec gretest1 ping -nc 1 -t 1 ${v4tunnel2}
	atf_check -o ignore jexec gretest2 ping -nc 1 -t 1 ${v4tunnel1}

	# The key must match without fallback
	atf_check -o ignore \
	    sysctl -j gretest2 net.link.gre.key_fallback=0
	atf_check -s exit:2 -o ignore \
	    jexec gretest1 ping -nc 1 -t 1 ${v4tunnel2}
	atf_check -o ignore \
	    sysctl -j gretest2 net.link.gre.key_fallback=1
	atf_check -o ignore jexec gretest1 ping -nc 1 -t 1 ${v4tunnel2}

	# The key must match when the tunnel is not the only one
	atf_check ifconfig -j gretest2 gre2 create ${af} \
	    tunnel ${endpoint2} ${endpoint1} greikey 30 greokey 30 ${encap} up
	atf_check -s exit:2 -o ignore \
	    jexec gretest1 ping -nc 1 -t 1 ${v4tunnel2}

	atf_check ifconfig -j gretest2 gre2 destroy
	atf_check -o ignore jexec gretest1 ping -nc 1 -t 1 ${v4tunnel2}
}

atf_test_case "gre4_key_mismatch" "cleanup"
gre4_key_mismatch_head()
{
	atf_set descr 'Test backward compatibility for gre mismatched key'
	atf_set require.user root
	atf_set require.kmods if_gre
}
gre4_key_mismatch_body()
{
	gre_key_mismatch_test inet 203.0.113.1 203.0.113.2
}
gre4_key_mismatch_cleanup()
{
	vnet_cleanup
}

atf_test_case "gre6_key_mismatch" "cleanup"
gre6_key_mismatch_head()
{
	atf_set descr 'Test backward compatibility for gre over ipv6 with mismatched key'
	atf_set require.user root
	atf_set require.kmods if_gre
}
gre6_key_mismatch_body()
{
	gre_key_mismatch_test inet6 3fff::1 3fff::2
}
gre6_key_mismatch_cleanup()
{
	vnet_cleanup
}

atf_test_case "gre4_key_mismatch_udpencap" "cleanup"
gre4_key_mismatch_udpencap_head()
{
	atf_set descr 'Test backward compatibility for gre over udp with mismatched key'
	atf_set require.user root
	atf_set require.kmods if_gre
}
gre4_key_mismatch_udpencap_body()
{
	gre_key_mismatch_test inet 203.0.113.1 203.0.113.2 udpencap
}
gre4_key_mismatch_udpencap_cleanup()
{
	vnet_cleanup
}

atf_test_case "gre6_key_mismatch_udpencap" "cleanup"
gre6_key_mismatch_udpencap_head()
{
	atf_set descr 'Test backward compatibility for gre over ipv6 udp with mismatched key'
	atf_set require.user root
	atf_set require.kmods if_gre
}
gre6_key_mismatch_udpencap_body()
{
	gre_key_mismatch_test inet6 3fff::1 3fff::2 udpencap
}
gre6_key_mismatch_udpencap_cleanup()
{
	vnet_cleanup
}

atf_test_case "gre4_asymmetric_key" "cleanup"
gre4_asymmetric_key_head()
{
	atf_set descr 'Test gre(4) tunnel with different incoming and outgoing keys'
	atf_set require.user root
	atf_set require.kmods if_gre
}
gre4_asymmetric_key_body()
{
	local endpoint1 endpoint2 v4tunnel1 v4tunnel2

	endpoint1=203.0.113.1
	endpoint2=203.0.113.2
	v4tunnel1=192.0.2.1
	v4tunnel2=192.0.2.2

	gre_setup_endpoints inet ${endpoint1} ${endpoint2}

	atf_check -o ignore \
	    sysctl -j gretest1 net.link.gre.key_fallback=0
	atf_check -o ignore \
	    sysctl -j gretest2 net.link.gre.key_fallback=0

	atf_check ifconfig -j gretest1 gre1 create \
	    tunnel ${endpoint1} ${endpoint2} greokey 100 greikey 200 up
	atf_check ifconfig -j gretest2 gre1 create \
	    tunnel ${endpoint2} ${endpoint1} greokey 200 greikey 100 up
	atf_check -o match:"greokey: 0x64 \(100\)" \
	    -o match:"greikey: 0xc8 \(200\)" ifconfig -j gretest1 gre1

	atf_check ifconfig -j gretest1 gre1 inet ${v4tunnel1} ${v4tunnel2}
	atf_check ifconfig -j gretest2 gre1 inet ${v4tunnel2} ${v4tunnel1}

	atf_check -o ignore jexec gretest1 ping -nc 1 -t 1 ${v4tunnel2}
	atf_check -o ignore jexec gretest2 ping -nc 1 -t 1 ${v4tunnel1}

	# grekey sets both the incoming and the outgoing key
	atf_check ifconfig -j gretest2 gre1 grekey 300 up
	atf_check -o match:"greokey: 0x12c \(300\)" \
	    -o match:"greikey: 0x12c \(300\)" ifconfig -j gretest2 gre1
	atf_check -s exit:2 -o ignore \
	    jexec gretest1 ping -nc 1 -t 1 ${v4tunnel2}

	atf_check ifconfig -j gretest2 gre1 greokey 200 greikey 100 up
	atf_check -o ignore jexec gretest1 ping -nc 1 -t 1 ${v4tunnel2}
}
gre4_asymmetric_key_cleanup()
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
	atf_add_test_case "gre4_multikey"
	atf_add_test_case "gre6_multikey"
	atf_add_test_case "gre4_multikey_udpencap"
	atf_add_test_case "gre6_multikey_udpencap"
	atf_add_test_case "gre4_key_fallback"
	atf_add_test_case "gre6_key_fallback"
	atf_add_test_case "gre4_key_fallback_udpencap"
	atf_add_test_case "gre6_key_fallback_udpencap"
	atf_add_test_case "gre4_key_mismatch"
	atf_add_test_case "gre6_key_mismatch"
	atf_add_test_case "gre4_key_mismatch_udpencap"
	atf_add_test_case "gre6_key_mismatch_udpencap"
	atf_add_test_case "gre4_asymmetric_key"
}
