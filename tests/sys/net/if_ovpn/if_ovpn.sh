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
. $(atf_get_srcdir)/../../netpfil/pf/utils.subr

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

	echo 'foo' | jexec b nc -u -w 2 192.0.2.1 1194
	atf_check -s exit:0 -o ignore jexec b ping -c 3 198.51.100.1
}

4in4_cleanup()
{
	ovpn_cleanup
}

atf_test_case "4mapped" "cleanup"
4mapped_head()
{
	atf_set descr 'IPv4 mapped addresses'
	atf_set require.user root
	atf_set require.progs openvpn
}

4mapped_body()
{
	ovpn_init

	l=$(vnet_mkepair)

	vnet_mkjail a ${l}a
	jexec a ifconfig ${l}a 192.0.2.1/24 up
	vnet_mkjail b ${l}b
	jexec b ifconfig ${l}b 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore jexec a ping -c 1 192.0.2.2

	#jexec a ifconfig ${l}a

	ovpn_start a "
		dev ovpn0
		dev-type tun

		cipher AES-256-GCM
		auth SHA256

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

4mapped_cleanup()
{
	ovpn_cleanup
}

atf_test_case "6in4" "cleanup"
6in4_head()
{
	atf_set descr 'IPv6 in IPv4 tunnel'
	atf_set require.user root
	atf_set require.progs openvpn
}

6in4_body()
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
		proto udp

		cipher AES-256-GCM
		auth SHA256

		local 192.0.2.1
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

	atf_check -s exit:0 -o ignore jexec b ping6 -c 3 2001:db8:1::1
}

6in4_cleanup()
{
	ovpn_cleanup
}

atf_test_case "4in6" "cleanup"
4in6_head()
{
	atf_set descr 'IPv4 in IPv6 tunnel'
	atf_set require.user root
	atf_set require.progs openvpn
}

4in6_body()
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

	atf_check -s exit:0 -o ignore jexec b ping -c 3 198.51.100.1
}

4in6_cleanup()
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
	atf_check -s exit:0 -o ignore jexec b ping6 -c 3 -z 16 2001:db8:1::1
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
	jexec a ifconfig lo0 127.0.0.1/8 up
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

		management 192.0.2.1 1234
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

		keepalive 2 10
	"

	# Give the tunnel time to come up
	sleep 10

	atf_check -s exit:0 -o ignore jexec b ping -c 3 198.51.100.1

	# Kill the client
	jexec b killall openvpn

	# Now wait for the server to notice
	sleep 15

	while echo "status" | jexec a nc -N 192.0.2.1 1234 | grep 192.0.2.2; do
		echo "Client disconnect not discovered"
		sleep 1
	done
}

timeout_client_cleanup()
{
	ovpn_cleanup
}

atf_test_case "explicit_exit" "cleanup"
explicit_exit_head()
{
	atf_set descr 'Text explicit exit notification'
	atf_set require.user root
	atf_set require.progs openvpn
}

explicit_exit_body()
{
	ovpn_init

	l=$(vnet_mkepair)

	vnet_mkjail a ${l}a
	jexec a ifconfig ${l}a 192.0.2.1/24 up
	jexec a ifconfig lo0 127.0.0.1/8 up
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

		management 192.0.2.1 1234
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

		explicit-exit-notify
	"

	# Give the tunnel time to come up
	sleep 10

	atf_check -s exit:0 -o ignore jexec b ping -c 3 198.51.100.1

	if ! echo "status" | jexec a nc -N 192.0.2.1 1234 | grep 192.0.2.2; then
		atf_fail "Client not found in status list!"
	fi

	# Kill the client
	jexec b killall openvpn

	while echo "status" | jexec a nc -N 192.0.2.1 1234 | grep 192.0.2.2; do
		jexec a ps auxf
		echo "Client disconnect not discovered"
		sleep 1
	done
}

explicit_exit_cleanup()
{
	ovpn_cleanup
}

atf_test_case "multi_client" "cleanup"
multi_client_head()
{
	atf_set descr 'Multiple simultaneous clients'
	atf_set require.user root
	atf_set require.progs openvpn
}

multi_client_body()
{
	ovpn_init

	bridge=$(vnet_mkbridge)
	srv=$(vnet_mkepair)
	one=$(vnet_mkepair)
	two=$(vnet_mkepair)

	ifconfig ${bridge} up

	ifconfig ${srv}a up
	ifconfig ${bridge} addm ${srv}a
	ifconfig ${one}a up
	ifconfig ${bridge} addm ${one}a
	ifconfig ${two}a up
	ifconfig ${bridge} addm ${two}a

	vnet_mkjail srv ${srv}b
	jexec srv ifconfig ${srv}b 192.0.2.1/24 up
	vnet_mkjail one ${one}b
	jexec one ifconfig ${one}b 192.0.2.2/24 up
	vnet_mkjail two ${two}b
	jexec two ifconfig ${two}b 192.0.2.3/24 up
	jexec two ifconfig lo0 127.0.0.1/8 up
	jexec two ifconfig lo0 inet alias 203.0.113.1/24

	# Sanity checks
	atf_check -s exit:0 -o ignore jexec one ping -c 1 192.0.2.1
	atf_check -s exit:0 -o ignore jexec two ping -c 1 192.0.2.1

	jexec srv sysctl net.inet.ip.forwarding=1

	ovpn_start srv "
		dev ovpn0
		dev-type tun
		proto udp4

		cipher AES-256-GCM
		auth SHA256

		local 192.0.2.1
		server 198.51.100.0 255.255.255.0

		push \"route 203.0.113.0 255.255.255.0 198.51.100.1\"

		ca $(atf_get_srcdir)/ca.crt
		cert $(atf_get_srcdir)/server.crt
		key $(atf_get_srcdir)/server.key
		dh $(atf_get_srcdir)/dh.pem

		mode server
		duplicate-cn
		script-security 2
		auth-user-pass-verify /usr/bin/true via-env
		topology subnet

		keepalive 100 600

		client-config-dir $(atf_get_srcdir)/ccd
	"
	ovpn_start one "
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
	ovpn_start two "
		dev tun0
		dev-type tun

		client

		remote 192.0.2.1
		auth-user-pass $(atf_get_srcdir)/user.pass

		ca $(atf_get_srcdir)/ca.crt
		cert $(atf_get_srcdir)/client2.crt
		key $(atf_get_srcdir)/client2.key
		dh $(atf_get_srcdir)/dh.pem

		keepalive 100 600
	"

	# Give the tunnel time to come up
	sleep 10

	atf_check -s exit:0 -o ignore jexec one ping -c 3 198.51.100.1
	atf_check -s exit:0 -o ignore jexec two ping -c 3 198.51.100.1

	# Client-to-client communication
	atf_check -s exit:0 -o ignore jexec one ping -c 3 198.51.100.3
	atf_check -s exit:0 -o ignore jexec two ping -c 3 198.51.100.2

	# iroute test
	atf_check -s exit:0 -o ignore jexec one ping -c 3 203.0.113.1
}

multi_client_cleanup()
{
	ovpn_cleanup
}

atf_test_case "route_to" "cleanup"
route_to_head()
{
	atf_set descr "Test pf's route-to with OpenVPN tunnels"
	atf_set require.user root
	atf_set require.progs openvpn
}

route_to_body()
{
	pft_init
	ovpn_init

	l=$(vnet_mkepair)
	n=$(vnet_mkepair)

	vnet_mkjail a ${l}a
	jexec a ifconfig ${l}a 192.0.2.1/24 up
	jexec a ifconfig ${l}a inet alias 198.51.100.254/24
	vnet_mkjail b ${l}b ${n}a
	jexec b ifconfig ${l}b 192.0.2.2/24 up
	jexec b ifconfig ${n}a up

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

	# Check the tunnel
	atf_check -s exit:0 -o ignore jexec b ping -c 1 198.51.100.1
	atf_check -s exit:0 -o ignore jexec b ping -c 1 198.51.100.254

	# Break our routes so that we need a route-to to make things work.
	jexec b ifconfig ${n}a 198.51.100.3/24
	atf_check -s exit:2 -o ignore jexec b ping -c 1 -t 1 -S 198.51.100.2 198.51.100.254

	jexec b pfctl -e
	pft_set_rules b \
		"pass out route-to (tun0 198.51.100.1) proto icmp from 198.51.100.2 "
	atf_check -s exit:0 -o ignore jexec b ping -c 3 -S 198.51.100.2 198.51.100.254

	# And this keeps working even if we don't have a route to 198.51.100.0/24 via if_ovpn
	jexec b route del -net 198.51.100.0/24
	jexec b route add -net 198.51.100.0/24 -interface ${n}a
	pft_set_rules b \
		"pass out route-to (tun0 198.51.100.3) proto icmp from 198.51.100.2 "
	atf_check -s exit:0 -o ignore jexec b ping -c 3 -S 198.51.100.2 198.51.100.254
}

route_to_cleanup()
{
	ovpn_cleanup
	pft_cleanup
}

atf_test_case "ra" "cleanup"
ra_head()
{
	atf_set descr 'Remote access with multiple clients'
	atf_set require.user root
	atf_set require.progs openvpn
}

ra_body()
{
	ovpn_init

	bridge=$(vnet_mkbridge)
	srv=$(vnet_mkepair)
	lan=$(vnet_mkepair)
	one=$(vnet_mkepair)
	two=$(vnet_mkepair)

	ifconfig ${bridge} up

	ifconfig ${srv}a up
	ifconfig ${bridge} addm ${srv}a
	ifconfig ${one}a up
	ifconfig ${bridge} addm ${one}a
	ifconfig ${two}a up
	ifconfig ${bridge} addm ${two}a

	vnet_mkjail srv ${srv}b ${lan}a
	jexec srv ifconfig ${srv}b 192.0.2.1/24 up
	jexec srv ifconfig ${lan}a 203.0.113.1/24 up
	vnet_mkjail lan ${lan}b
	jexec lan ifconfig ${lan}b 203.0.113.2/24 up
	jexec lan route add default 203.0.113.1
	vnet_mkjail one ${one}b
	jexec one ifconfig ${one}b 192.0.2.2/24 up
	vnet_mkjail two ${two}b
	jexec two ifconfig ${two}b 192.0.2.3/24 up

	# Sanity checks
	atf_check -s exit:0 -o ignore jexec one ping -c 1 192.0.2.1
	atf_check -s exit:0 -o ignore jexec two ping -c 1 192.0.2.1
	atf_check -s exit:0 -o ignore jexec srv ping -c 1 203.0.113.2

	jexec srv sysctl net.inet.ip.forwarding=1

	ovpn_start srv "
		dev ovpn0
		dev-type tun
		proto udp4

		cipher AES-256-GCM
		auth SHA256

		local 192.0.2.1
		server 198.51.100.0 255.255.255.0

		push \"route 203.0.113.0 255.255.255.0\"

		ca $(atf_get_srcdir)/ca.crt
		cert $(atf_get_srcdir)/server.crt
		key $(atf_get_srcdir)/server.key
		dh $(atf_get_srcdir)/dh.pem

		mode server
		duplicate-cn
		script-security 2
		auth-user-pass-verify /usr/bin/true via-env
		topology subnet

		keepalive 100 600
	"
	ovpn_start one "
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
	sleep 2
	ovpn_start two "
		dev tun0
		dev-type tun

		client

		remote 192.0.2.1
		auth-user-pass $(atf_get_srcdir)/user.pass

		ca $(atf_get_srcdir)/ca.crt
		cert $(atf_get_srcdir)/client2.crt
		key $(atf_get_srcdir)/client2.key
		dh $(atf_get_srcdir)/dh.pem

		keepalive 100 600
	"

	# Give the tunnel time to come up
	sleep 10

	atf_check -s exit:0 -o ignore jexec one ping -c 1 198.51.100.1
	atf_check -s exit:0 -o ignore jexec two ping -c 1 198.51.100.1

	# Client-to-client communication
	atf_check -s exit:0 -o ignore jexec one ping -c 1 198.51.100.3
	atf_check -s exit:0 -o ignore jexec two ping -c 1 198.51.100.2

	# RA test
	atf_check -s exit:0 -o ignore jexec one ping -c 1 203.0.113.1
	atf_check -s exit:0 -o ignore jexec two ping -c 1 203.0.113.1

	atf_check -s exit:0 -o ignore jexec srv ping -c 1 -S 203.0.113.1 198.51.100.2
	atf_check -s exit:0 -o ignore jexec srv ping -c 1 -S 203.0.113.1 198.51.100.3

	atf_check -s exit:0 -o ignore jexec one ping -c 1 203.0.113.2
	atf_check -s exit:0 -o ignore jexec two ping -c 1 203.0.113.2

	atf_check -s exit:0 -o ignore jexec lan ping -c 1 198.51.100.1
	atf_check -s exit:0 -o ignore jexec lan ping -c 1 198.51.100.2
	atf_check -s exit:0 -o ignore jexec lan ping -c 1 198.51.100.3
	atf_check -s exit:2 -o ignore jexec lan ping -c 1 198.51.100.4
}

ra_cleanup()
{
	ovpn_cleanup
}


atf_test_case "chacha" "cleanup"
chacha_head()
{
	atf_set descr 'Test DCO with the chacha algorithm'
	atf_set require.user root
	atf_set require.progs openvpn
}

chacha_body()
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

		cipher CHACHA20-POLY1305
		data-ciphers CHACHA20-POLY1305
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

chacha_cleanup()
{
	ovpn_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "4in4"
	atf_add_test_case "4mapped"
	atf_add_test_case "6in4"
	atf_add_test_case "6in6"
	atf_add_test_case "4in6"
	atf_add_test_case "timeout_client"
	atf_add_test_case "explicit_exit"
	atf_add_test_case "multi_client"
	atf_add_test_case "route_to"
	atf_add_test_case "ra"
	atf_add_test_case "chacha"
}
