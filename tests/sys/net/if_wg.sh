#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 The FreeBSD Foundation
#
# This software was developed by Mark Johnston under sponsorship
# from the FreeBSD Foundation.
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

atf_test_case "wg_basic" "cleanup"
wg_basic_head()
{
	atf_set descr 'Create a wg(4) tunnel over an epair and pass traffic between jails'
	atf_set require.user root
}

wg_basic_body()
{
	local epair pri1 pri2 pub1 pub2 wg1 wg2
        local endpoint1 endpoint2 tunnel1 tunnel2

	kldload -n if_wg || atf_skip "This test requires if_wg and could not load it"

	pri1=$(wg genkey)
	pri2=$(wg genkey)

	endpoint1=192.168.2.1
	endpoint2=192.168.2.2
	tunnel1=169.254.0.1
	tunnel2=169.254.0.2

	epair=$(vnet_mkepair)

	vnet_init

	vnet_mkjail wgtest1 ${epair}a
	vnet_mkjail wgtest2 ${epair}b

	jexec wgtest1 ifconfig ${epair}a ${endpoint1}/24 up
	jexec wgtest2 ifconfig ${epair}b ${endpoint2}/24 up

	wg1=$(jexec wgtest1 ifconfig wg create)
	echo "$pri1" | jexec wgtest1 wg set $wg1 listen-port 12345 \
	    private-key /dev/stdin
	pub1=$(jexec wgtest1 wg show $wg1 public-key)
	wg2=$(jexec wgtest2 ifconfig wg create)
	echo "$pri2" | jexec wgtest2 wg set $wg2 listen-port 12345 \
	    private-key /dev/stdin
	pub2=$(jexec wgtest2 wg show $wg2 public-key)

	atf_check -s exit:0 -o ignore \
	    jexec wgtest1 wg set $wg1 peer "$pub2" \
	    endpoint ${endpoint2}:12345 allowed-ips ${tunnel2}/32
	atf_check -s exit:0 \
	    jexec wgtest1 ifconfig $wg1 inet ${tunnel1}/24 up

	atf_check -s exit:0 -o ignore \
	    jexec wgtest2 wg set $wg2 peer "$pub1" \
	    endpoint ${endpoint1}:12345 allowed-ips ${tunnel1}/32
	atf_check -s exit:0 \
	    jexec wgtest2 ifconfig $wg2 inet ${tunnel2}/24 up

	# Generous timeout since the handshake takes some time.
	atf_check -s exit:0 -o ignore jexec wgtest1 ping -c 1 -t 5 $tunnel2
	atf_check -s exit:0 -o ignore jexec wgtest2 ping -c 1 $tunnel1
}

wg_basic_cleanup()
{
	vnet_cleanup
}

atf_test_case "wg_basic_crossaf" "cleanup"
wg_basic_crossaf_head()
{
	atf_set descr 'Create a wg(4) tunnel and pass IPv4 traffic over an IPv6 nexthop'
	atf_set require.user root
}

wg_basic_crossaf_body()
{
	local epair pri1 pri2 pub1 pub2 wg1 wg2
	local endpoint1 endpoint2 tunnel1 tunnel2
	local testnet testlocal testremote

	kldload -n if_wg || atf_skip "This test requires if_wg and could not load it"

	pri1=$(wg genkey)
	pri2=$(wg genkey)

	endpoint1=192.168.2.1
	endpoint2=192.168.2.2
	tunnel1=2001:db8:1::1
	tunnel2=2001:db8:1::2

	testnet=192.168.3.0/24
	testlocal=192.168.3.1
	testremote=192.168.3.2

	epair=$(vnet_mkepair)

	vnet_init

	vnet_mkjail wgtest1 ${epair}a
	vnet_mkjail wgtest2 ${epair}b

	jexec wgtest1 ifconfig ${epair}a ${endpoint1}/24 up
	jexec wgtest2 ifconfig ${epair}b ${endpoint2}/24 up

	wg1=$(jexec wgtest1 ifconfig wg create)
	echo "$pri1" | jexec wgtest1 wg set $wg1 listen-port 12345 \
	    private-key /dev/stdin
	pub1=$(jexec wgtest1 wg show $wg1 public-key)
	wg2=$(jexec wgtest2 ifconfig wg create)
	echo "$pri2" | jexec wgtest2 wg set $wg2 listen-port 12345 \
	    private-key /dev/stdin
	pub2=$(jexec wgtest2 wg show $wg2 public-key)

	atf_check -s exit:0 -o ignore \
	    jexec wgtest1 wg set $wg1 peer "$pub2" \
	    endpoint ${endpoint2}:12345 allowed-ips ${tunnel2}/128,${testnet}
	atf_check -s exit:0 \
	    jexec wgtest1 ifconfig $wg1 inet6 ${tunnel1}/64 up

	atf_check -s exit:0 -o ignore \
	    jexec wgtest2 wg set $wg2 peer "$pub1" \
	    endpoint ${endpoint1}:12345 allowed-ips ${tunnel1}/128,${testnet}
	atf_check -s exit:0 \
	    jexec wgtest2 ifconfig $wg2 inet6 ${tunnel2}/64 up

	atf_check -s exit:0 jexec wgtest1 ifconfig $wg1 inet ${testlocal}/32
	atf_check -s exit:0 jexec wgtest2 ifconfig $wg2 inet ${testremote}/32

	# Generous timeout since the handshake takes some time.
	atf_check -s exit:0 -o ignore jexec wgtest1 ping -c 1 -t 5 "$tunnel2"

	# Setup our IPv6 endpoint and routing
	atf_check -s exit:0 -o ignore \
		jexec wgtest1 route add -inet ${testnet} -inet6 "$tunnel2"
	atf_check -s exit:0 -o ignore \
		jexec wgtest2 route add -inet ${testnet} -inet6 "$tunnel1"
	# Now ping an address on the other side
	atf_check -s exit:0 -o ignore jexec wgtest1 ping -c 1 -t 3 ${testremote}
}

wg_basic_crossaf_cleanup()
{
	vnet_cleanup
}

atf_test_case "wg_basic_netmap" "cleanup"
wg_basic_netmap_head()
{
	atf_set descr 'Create a wg(4) tunnel over an epair and pass traffic between jails with netmap'
	atf_set require.user root
}

wg_basic_netmap_body()
{
	local epair pri1 pri2 pub1 pub2 wg1 wg2
        local endpoint1 endpoint2 tunnel1 tunnel2 tunnel3 tunnel4
	local pid status

	kldload -n if_wg || atf_skip "This test requires if_wg and could not load it"
	kldload -n netmap || atf_skip "This test requires netmap and could not load it"

	pri1=$(wg genkey)
	pri2=$(wg genkey)

	endpoint1=192.168.2.1
	endpoint2=192.168.2.2
	tunnel1=192.168.3.1
	tunnel2=192.168.3.2
	tunnel3=192.168.3.3
	tunnel4=192.168.3.4

	epair=$(vnet_mkepair)

	vnet_init

	vnet_mkjail wgtest1 ${epair}a
	vnet_mkjail wgtest2 ${epair}b

	jexec wgtest1 ifconfig ${epair}a ${endpoint1}/24 up
	jexec wgtest2 ifconfig ${epair}b ${endpoint2}/24 up

	wg1=$(jexec wgtest1 ifconfig wg create)
	echo "$pri1" | jexec wgtest1 wg set $wg1 listen-port 12345 \
	    private-key /dev/stdin
	pub1=$(jexec wgtest1 wg show $wg1 public-key)
	wg2=$(jexec wgtest2 ifconfig wg create)
	echo "$pri2" | jexec wgtest2 wg set $wg2 listen-port 12345 \
	    private-key /dev/stdin
	pub2=$(jexec wgtest2 wg show $wg2 public-key)

	atf_check -s exit:0 -o ignore \
	    jexec wgtest1 wg set $wg1 peer "$pub2" \
	    endpoint ${endpoint2}:12345 allowed-ips ${tunnel2}/32,${tunnel4}/32
	atf_check -s exit:0 \
	    jexec wgtest1 ifconfig $wg1 inet ${tunnel1}/24 up

	atf_check -s exit:0 -o ignore \
	    jexec wgtest2 wg set $wg2 peer "$pub1" \
	    endpoint ${endpoint1}:12345 allowed-ips ${tunnel1}/32,${tunnel3}/32
	atf_check -s exit:0 \
	    jexec wgtest2 ifconfig $wg2 inet ${tunnel2}/24 up

	atf_check -s exit:0 -o ignore \
	    jexec wgtest1 sysctl net.inet.ip.forwarding=1
	atf_check -s exit:0 -o ignore \
	    jexec wgtest2 sysctl net.inet.ip.forwarding=1

	jexec wgtest1 $(atf_get_srcdir)/bridge -w 0 -i netmap:wg0 -i netmap:wg0^ &
	pid=$!

	# Generous timeout since the handshake takes some time.
	atf_check -s exit:0 -o ignore jexec wgtest1 ping -c 1 -t 5 $tunnel2
	atf_check -s exit:0 -o ignore jexec wgtest2 ping -c 1 $tunnel1

	# Verify that we cannot ping non-existent tunnel addresses.  In general
	# the remote side should respond with an ICMP message.
	atf_check -s exit:2 -o ignore jexec wgtest1 ping -c 1 -t 2 $tunnel4
	atf_check -s exit:2 -o ignore jexec wgtest2 ping -c 1 -t 2 $tunnel3

	# Make sure that the bridge is still functional.
	atf_check -s exit:0 -o ignore jexec wgtest1 ping -c 1 $tunnel2
	atf_check -s exit:0 -o ignore jexec wgtest2 ping -c 1 $tunnel1

	atf_check -s exit:0 kill -TERM $pid
	wait $pid
	status=$?

	# Make sure that SIGTERM was received and handled.
	atf_check_equal $status 143
}

wg_basic_netmap_cleanup()
{
	vnet_cleanup
}

# The kernel is expected to silently ignore any attempt to add a peer with a
# public key identical to the host's.
atf_test_case "wg_key_peerdev_shared" "cleanup"
wg_key_peerdev_shared_head()
{
	atf_set descr 'Create a wg(4) interface with a shared pubkey between device and a peer'
	atf_set require.user root
}

wg_key_peerdev_shared_body()
{
	local epair pri1 pub1 wg1
        local endpoint1 tunnel1

	kldload -n if_wg || atf_skip "This test requires if_wg and could not load it"

	pri1=$(wg genkey)

	endpoint1=192.168.2.1
	tunnel1=169.254.0.1

	vnet_mkjail wgtest1

	wg1=$(jexec wgtest1 ifconfig wg create)
	echo "$pri1" | jexec wgtest1 wg set $wg1 listen-port 12345 \
	    private-key /dev/stdin
	pub1=$(jexec wgtest1 wg show $wg1 public-key)

	atf_check -s exit:0 \
	    jexec wgtest1 wg set ${wg1} peer "${pub1}" \
	    allowed-ips "${tunnel1}/32"

	atf_check -o empty jexec wgtest1 wg show ${wg1} peers
}

wg_key_peerdev_shared_cleanup()
{
	vnet_cleanup
}

# When a wg(8) interface has a private key reassigned that corresponds to the
# public key already on a peer, the kernel is expected to deconfigure the peer
# to resolve the conflict.
atf_test_case "wg_key_peerdev_makeshared" "cleanup"
wg_key_peerdev_makeshared_head()
{
	atf_set descr 'Create a wg(4) interface and assign peer key to device'
	atf_set require.progs wg
}

wg_key_peerdev_makeshared_body()
{
	local epair pri1 pub1 pri2 wg1 wg2
        local endpoint1 tunnel1

	kldload -n if_wg || atf_skip "This test requires if_wg and could not load it"

	pri1=$(wg genkey)
	pri2=$(wg genkey)

	endpoint1=192.168.2.1
	tunnel1=169.254.0.1

	vnet_mkjail wgtest1

	wg1=$(jexec wgtest1 ifconfig wg create)
	echo "$pri1" | jexec wgtest1 wg set $wg1 listen-port 12345 \
	    private-key /dev/stdin
	pub1=$(jexec wgtest1 wg show $wg1 public-key)
	wg2=$(jexec wgtest1 ifconfig wg create)
	echo "$pri2" | jexec wgtest1 wg set $wg2 listen-port 12345 \
	    private-key /dev/stdin

	atf_check -s exit:0 -o ignore \
	    jexec wgtest1 wg set ${wg2} peer "${pub1}" \
	    allowed-ips "${tunnel1}/32"

	atf_check -o not-empty jexec wgtest1 wg show ${wg2} peers

	jexec wgtest1 sh -c "echo '${pri1}' > pri1"

	atf_check -s exit:0 \
	   jexec wgtest1 wg set ${wg2} private-key pri1

	atf_check -o empty jexec wgtest1 wg show ${wg2} peers
}

wg_key_peerdev_makeshared_cleanup()
{
	vnet_cleanup
}

# The kernel is expected to create the wg socket in the jail context that the
# wg interface was created in, even if the interface is moved to a different
# vnet.
atf_test_case "wg_vnet_parent_routing" "cleanup"
wg_vnet_parent_routing_head()
{
	atf_set descr 'Create a wg(4) tunnel without epairs and pass traffic between jails'
	atf_set require.user root
}

wg_vnet_parent_routing_body()
{
	local pri1 pri2 pub1 pub2 wg1 wg2
        local tunnel1 tunnel2

	kldload -n if_wg

	pri1=$(wg genkey)
	pri2=$(wg genkey)

	tunnel1=169.254.0.1
	tunnel2=169.254.0.2

	vnet_init

	wg1=$(ifconfig wg create)
	wg2=$(ifconfig wg create)

	vnet_mkjail wgtest1 ${wg1}
	vnet_mkjail wgtest2 ${wg2}

	echo "$pri1" | jexec wgtest1 wg set $wg1 listen-port 12345 \
	    private-key /dev/stdin
	pub1=$(jexec wgtest1 wg show $wg1 public-key)
	echo "$pri2" | jexec wgtest2 wg set $wg2 listen-port 12346 \
	    private-key /dev/stdin
	pub2=$(jexec wgtest2 wg show $wg2 public-key)

	atf_check -s exit:0 -o ignore \
	    jexec wgtest1 wg set $wg1 peer "$pub2" \
	    endpoint 127.0.0.1:12346 allowed-ips ${tunnel2}/32
	atf_check -s exit:0 \
	    jexec wgtest1 ifconfig $wg1 inet ${tunnel1}/24 up

	atf_check -s exit:0 -o ignore \
	    jexec wgtest2 wg set $wg2 peer "$pub1" \
	    endpoint 127.0.0.1:12345 allowed-ips ${tunnel1}/32
	atf_check -s exit:0 \
	    jexec wgtest2 ifconfig $wg2 inet ${tunnel2}/24 up

	# Sanity check ICMP counters; should clearly be nothing on these new
	# jails.  We'll check them as we go to ensure that the ICMP packets
	# generated really are being handled by the jails' vnets.
	atf_check -o not-match:"histogram" jexec wgtest1 netstat -s -p icmp
	atf_check -o not-match:"histogram" jexec wgtest2 netstat -s -p icmp

	# Generous timeout since the handshake takes some time.
	atf_check -s exit:0 -o ignore jexec wgtest1 ping -c 1 -t 5 $tunnel2
	atf_check -o match:"echo reply: 1" jexec wgtest1 netstat -s -p icmp
	atf_check -o match:"echo: 1" jexec wgtest2 netstat -s -p icmp

	atf_check -s exit:0 -o ignore jexec wgtest2 ping -c 1 $tunnel1
	atf_check -o match:"echo reply: 1" jexec wgtest2 netstat -s -p icmp
	atf_check -o match:"echo: 1" jexec wgtest1 netstat -s -p icmp
}

wg_vnet_parent_routing_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "wg_basic"
	atf_add_test_case "wg_basic_crossaf"
	atf_add_test_case "wg_basic_netmap"
	atf_add_test_case "wg_key_peerdev_shared"
	atf_add_test_case "wg_key_peerdev_makeshared"
	atf_add_test_case "wg_vnet_parent_routing"
}
