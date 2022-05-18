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

atf_test_case "6in6" "cleanup"
6in6_head()
{
	atf_set descr 'IPv6 in IPv6 tunnel'
	atf_set require.user root
	atf_set require.progs openvpn
}

6in6_body()
{
	ovpn_init

	l=$(vnet_mkepair)

	vnet_mkjail a ${l}a
	jexec a ifconfig ${l}a inet6 2001:db8::1/64 up no_dad
	vnet_mkjail b ${l}b
	jexec b ifconfig ${l}b inet6 2001:db8::2/64 up no_dad

	# Sanity check
	atf_check -s exit:0 -o ignore jexec a ping6 -c 1 2001:db8::2

	ovpn_start a "
		dev ovpn0
		dev-type tun
		proto udp6

		cipher AES-256-GCM
		auth SHA256

		local 2001:db8::1
		server-ipv6 2001:db8:1::/64

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

		remote 2001:db8::1
		auth-user-pass $(atf_get_srcdir)/user.pass

		ca $(atf_get_srcdir)/ca.crt
		cert $(atf_get_srcdir)/client.crt
		key $(atf_get_srcdir)/client.key
		dh $(atf_get_srcdir)/dh.pem

		keepalive 100 600
	"

	# Give the tunnel time to come up
	sleep 10

	atf_check -s exit:0 -o ignore jexec b ping6 -c 3 2001:db8:1::1
}

6in6_cleanup()
{
	ovpn_cleanup
}

atf_test_case "timeout_client" "cleanup"
timeout_client_head()
{
	atf_set descr 'IPv4 in IPv4 tunnel'
	atf_set require.user root
	atf_set require.progs openvpn
}

timeout_client_body()
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

		keepalive 2 10
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

		ping 2
		ping-exit 10
	"

	# Give the tunnel time to come up
	sleep 10

	atf_check -s exit:0 -o ignore jexec b ping -c 3 198.51.100.1

	# Kill the server
	jexec a killall openvpn

	# Now wait for the client to notice
	sleep 20

	if [ jexec b pgrep openvpn ]; then
		jexec b ps auxf
		atf_fail "OpenVPN client still running?"
	fi
}

timeout_client_cleanup()
{
	ovpn_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "4in4"
	atf_add_test_case "6in6"
	atf_add_test_case "timeout_client"
}
