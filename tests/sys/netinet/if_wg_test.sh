# $FreeBSD$
#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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

	kldload -n if_wg

	pri1=$(openssl rand -base64 32)
	pri2=$(openssl rand -base64 32)

	endpoint1=192.168.2.1
	endpoint2=192.168.2.2
	tunnel1=169.254.0.1
	tunnel2=169.254.0.2

	epair=$(vnet_mkepair)

	vnet_init

	vnet_mkjail wgtest1 ${epair}a
	vnet_mkjail wgtest2 ${epair}b

	# Workaround for PR 254212.
	jexec wgtest1 ifconfig lo0 up
	jexec wgtest2 ifconfig lo0 up

	jexec wgtest1 ifconfig ${epair}a $endpoint1 up
	jexec wgtest2 ifconfig ${epair}b $endpoint2 up

	wg1=$(jexec wgtest1 ifconfig wg create listen-port 12345 private-key "$pri1")
	pub1=$(jexec wgtest1 ifconfig $wg1 | awk '/public-key:/ {print $2}')
	wg2=$(jexec wgtest2 ifconfig wg create listen-port 12345 private-key "$pri2")
	pub2=$(jexec wgtest2 ifconfig $wg2 | awk '/public-key:/ {print $2}')

	atf_check -s exit:0 -o ignore \
	    jexec wgtest1 ifconfig $wg1 peer public-key "$pub2" \
	    endpoint ${endpoint2}:12345 allowed-ips ${tunnel2}/32
	atf_check -s exit:0 \
	    jexec wgtest1 ifconfig $wg1 inet $tunnel1 up

	atf_check -s exit:0 -o ignore \
	    jexec wgtest2 ifconfig $wg2 peer public-key "$pub1" \
	    endpoint ${endpoint1}:12345 allowed-ips ${tunnel1}/32
	atf_check -s exit:0 \
	    jexec wgtest2 ifconfig $wg2 inet $tunnel2 up

	# Generous timeout since the handshake takes some time.
	atf_check -s exit:0 -o ignore jexec wgtest1 ping -o -t 5 -i 0.25 $tunnel2
	atf_check -s exit:0 -o ignore jexec wgtest2 ping -o -t 5 -i 0.25 $tunnel1
}

wg_basic_cleanup()
{
	vnet_cleanup
}

# The kernel is expecteld to silently ignore any attempt to add a peer with a
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

	kldload -n if_wg

	pri1=$(openssl rand -base64 32)

	endpoint1=192.168.2.1
	tunnel1=169.254.0.1

	vnet_mkjail wgtest1

	wg1=$(jexec wgtest1 ifconfig wg create listen-port 12345 private-key "$pri1")
	pub1=$(jexec wgtest1 ifconfig $wg1 | awk '/public-key:/ {print $2}')

	atf_check -s exit:0 \
	    jexec wgtest1 ifconfig ${wg1} peer public-key "${pub1}" \
	    allowed-ips "${tunnel1}/32"

	atf_check -o empty jexec wgtest1 ifconfig ${wg1} peers
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

	kldload -n if_wg

	pri1=$(openssl rand -base64 32)
	pri2=$(openssl rand -base64 32)

	endpoint1=192.168.2.1
	tunnel1=169.254.0.1

	vnet_mkjail wgtest1

	wg1=$(jexec wgtest1 ifconfig wg create listen-port 12345 private-key "$pri1")
	pub1=$(jexec wgtest1 ifconfig $wg1 | awk '/public-key:/ {print $2}')

	wg2=$(jexec wgtest1 ifconfig wg create listen-port 12345 private-key "$pri2")

	atf_check -s exit:0 -o ignore \
	    jexec wgtest1 ifconfig ${wg2} peer public-key "${pub1}" \
	    allowed-ips "${tunnel1}/32"

	atf_check -o not-empty jexec wgtest1 ifconfig ${wg2} peers

	jexec wgtest1 sh -c "echo '${pri1}' > pri1"

	atf_check -s exit:0 \
	   jexec wgtest1 wg set ${wg2} private-key pri1

	atf_check -o empty jexec wgtest1 ifconfig ${wg2} peers
}

wg_key_peerdev_makeshared_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "wg_basic"
	atf_add_test_case "wg_key_peerdev_shared"
	atf_add_test_case "wg_key_peerdev_makeshared"
}
