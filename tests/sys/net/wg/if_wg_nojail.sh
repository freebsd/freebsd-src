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

. $(atf_get_srcdir)/../../common/vnet.subr

atf_test_case "wg_bad_decrypt" "cleanup"
wg_bad_decrypt_head()
{
	atf_set descr 'Create a wg(4) tunnel over an epair and inject a decryption error'
	atf_set require.user root
	atf_set require.kmods if_wg
}

wg_bad_decrypt_body()
{
	local epair pri1 pri2 pub1 pub2 wg1 wg2
        local endpoint1 endpoint2 tunnel1 tunnel2

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

	# No receive errors before injection
	ierrs=$(netstat -j wgtest2 -I $wg2 --libxo json,pretty | \
			awk '/received-errors/ { print $2 }')
	atf_check_equal "0," "$ierrs"

	# Trigger a decryption error
	atf_check -s exit:0 -o ignore \
	    sysctl debug.fail_point.crypto.inject_badmsg="1*return"

	atf_check -s exit:2 -o ignore jexec wgtest1 ping -c 1 -t 5 $tunnel2

	ierrs=$(netstat -j wgtest2 -I $wg2 --libxo json,pretty | \
			awk '/received-errors/ { print $2 }')
	atf_check_equal "1," "$ierrs"
}

wg_bad_decrypt_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "wg_bad_decrypt"
}
