##
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2022 Rubicon Communications, LLC ("Netgate")
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

atf_test_case "4in4" "cleanup"
4in4_head()
{
	atf_set descr 'IPv4 in IPv4 tunnel'
	atf_set require.user root
	atf_set require.progs openvpn
}

4in4_body()
{
	ovpn_init

	l=$(vnet_mkepair)

	vnet_mkjail a ${l}a
	jexec a ifconfig ${l}a 192.0.2.1/24 up
	vnet_mkjail b ${l}b
	jexec b ifconfig ${l}b 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore jexec a ping -c 1 192.0.2.2

	ovpn_start a "
		dev ovpn0
		dev-type tun
		proto udp4

		cipher AES-256-GCM
		auth SHA256

		local 192.0.2.1
		server 198.51.100.0 255.255.255.0
		ca $(atf_get_srcdir)/ca.crt
		cert $(atf_get_srcdir)/server.crt
		key $(atf_get_srcdir)/server.key
		dh $(atf_get_srcdir)/dh.pem

		mode server
		script-security 2
		auth-user-pass-verify /usr/bin/true via-env
		topology subnet

		keepalive 100 600
	"
	ovpn_start b "
		dev tun0
		dev-type tun

		client

		remote 192.0.2.1
		auth-user-pass $(atf_get_srcdir)/user.pass

		ca $(atf_get_srcdir)/ca.crt
		cert $(atf_get_srcdir)/client.crt
		key $(atf_get_srcdir)/client.key
		dh $(atf_get_srcdir)/dh.pem

		keepalive 100 600
	"

	# Give the tunnel time to come up
	sleep 10

	atf_check -s exit:0 -o ignore jexec b ping -c 3 198.51.100.1
}

4in4_cleanup()
{
	ovpn_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "4in4"
}
