#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Rubicon Communications, LLC (Netgate)
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

atf_test_case "4in4" "cleanup"
4in4_head()
{
	atf_set descr 'IPv4 in IPv4 tunnel'
	atf_set require.user root
}

4in4_body()
{
	vnet_init
	if ! kldstat -q -m if_gif; then
		atf_skip "This test requires if_gif"
	fi

	epair=$(vnet_mkepair)

	vnet_mkjail one ${epair}a
	jexec one ifconfig ${epair}a 192.0.2.1/24 up
	gone=$(jexec one ifconfig gif create)
	jexec one ifconfig $gone tunnel 192.0.2.1 192.0.2.2
	jexec one ifconfig $gone inet 198.51.100.1/24 198.51.100.2 up

	vnet_mkjail two ${epair}b
	jexec two ifconfig ${epair}b 192.0.2.2/24 up
	gtwo=$(jexec two ifconfig gif create)
	jexec two ifconfig $gtwo tunnel 192.0.2.2 192.0.2.1
	jexec two ifconfig $gtwo inet 198.51.100.2/24 198.51.100.1 up

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    jexec one ping -c 1 192.0.2.2

	# Tunnel test
	atf_check -s exit:0 -o ignore \
	    jexec one ping -c 1 198.51.100.2
	atf_check -s exit:0 -o ignore \
	    jexec two ping -c 1 198.51.100.1
}

4in4_cleanup()
{
	vnet_cleanup
}

atf_test_case "6in4" "cleanup"
6in4_head()
{
	atf_set descr 'IPv6 in IPv4 tunnel'
	atf_set require.user root
}

6in4_body()
{
	vnet_init
	if ! kldstat -q -m if_gif; then
		atf_skip "This test requires if_gif"
	fi

	epair=$(vnet_mkepair)

	vnet_mkjail one ${epair}a
	jexec one ifconfig ${epair}a 192.0.2.1/24 up
	gone=$(jexec one ifconfig gif create)
	jexec one ifconfig $gone tunnel 192.0.2.1 192.0.2.2
	jexec one ifconfig $gone inet6 no_dad 2001:db8:1::1/64 up

	vnet_mkjail two ${epair}b
	jexec two ifconfig ${epair}b 192.0.2.2/24 up
	gtwo=$(jexec two ifconfig gif create)
	jexec two ifconfig $gtwo tunnel 192.0.2.2 192.0.2.1
	jexec two ifconfig $gtwo inet6 no_dad 2001:db8:1::2/64 up

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    jexec one ping -c 1 192.0.2.2

	# Tunnel test
	atf_check -s exit:0 -o ignore \
	    jexec one ping -6 -c 1 2001:db8:1::2
	atf_check -s exit:0 -o ignore \
	    jexec two ping -6 -c 1 2001:db8:1::1
}

6in4_cleanup()
{
	vnet_cleanup
}

atf_test_case "4in6" "cleanup"
4in6_head()
{
	atf_set descr 'IPv4 in IPv6 tunnel'
	atf_set require.user root
}

4in6_body()
{
	vnet_init
	if ! kldstat -q -m if_gif; then
		atf_skip "This test requires if_gif"
	fi

	epair=$(vnet_mkepair)

	vnet_mkjail one ${epair}a
	jexec one ifconfig ${epair}a inet6 no_dad 2001:db8::1/64 up
	gone=$(jexec one ifconfig gif create)
	jexec one ifconfig $gone inet6 tunnel 2001:db8::1 2001:db8::2
	jexec one ifconfig $gone inet 198.51.100.1/24 198.51.100.2 up

	vnet_mkjail two ${epair}b
	jexec two ifconfig ${epair}b inet6 no_dad 2001:db8::2/64 up
	gtwo=$(jexec two ifconfig gif create)
	jexec two ifconfig $gtwo inet6 tunnel 2001:db8::2 2001:db8::1
	jexec two ifconfig $gtwo inet 198.51.100.2/24 198.51.100.1 up

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    jexec one ping -6 -c 1 2001:db8::2

	# Tunnel test
	atf_check -s exit:0 -o ignore \
	    jexec one ping -c 1 198.51.100.2
	atf_check -s exit:0 -o ignore \
	    jexec two ping -c 1 198.51.100.1
}

4in6_cleanup()
{
	vnet_cleanup
}

atf_test_case "6in6" "cleanup"
6in6_head()
{
	atf_set descr 'IPv6 in IPv6 tunnel'
	atf_set require.user root
}

6in6_body()
{
	vnet_init
	if ! kldstat -q -m if_gif; then
		atf_skip "This test requires if_gif"
	fi

	epair=$(vnet_mkepair)

	vnet_mkjail one ${epair}a
	jexec one ifconfig ${epair}a inet6 no_dad 2001:db8::1/64 up
	gone=$(jexec one ifconfig gif create)
	jexec one ifconfig $gone inet6 tunnel 2001:db8::1 2001:db8::2
	jexec one ifconfig $gone inet6 no_dad 2001:db8:1::1/64 up

	vnet_mkjail two ${epair}b
	jexec two ifconfig ${epair}b inet6 no_dad 2001:db8::2/64 up
	gtwo=$(jexec two ifconfig gif create)
	jexec two ifconfig $gtwo inet6 tunnel 2001:db8::2 2001:db8::1
	jexec two ifconfig $gtwo inet6 no_dad 2001:db8:1::2/64 up

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    jexec one ping -6 -c 1 2001:db8::2

	# Tunnel test
	atf_check -s exit:0 -o ignore \
	    jexec one ping -6 -c 1 2001:db8:1::2
	atf_check -s exit:0 -o ignore \
	    jexec two ping -6 -c 1 2001:db8:1::1
}

6in6_cleanup()
{
	vnet_cleanup
}

atf_test_case "etherip" "cleanup"
etherip_head()
{
	atf_set descr 'EtherIP regression'
	atf_set require.user root
}

etherip_body()
{
	vnet_init
	vnet_init_bridge

	if ! kldstat -q -m if_gif; then
		atf_skip "This test requires if_gif"
	fi

	epair=$(vnet_mkepair)

	vnet_mkjail one ${epair}a
	gone=$(jexec one ifconfig gif create)
	jexec one ifconfig ${epair}a 192.0.2.1/24 up
	jexec one ifconfig $gone tunnel 192.0.2.1 192.0.2.2
	jexec one ifconfig $gone 198.51.100.1/24 198.51.100.2 up
	jexec one ifconfig $gone inet6 no_dad 2001:db8:1::1/64

	bone=$(jexec one ifconfig bridge create)
	jexec one ifconfig $bone addm $gone
	jexec one ifconfig $bone 192.168.169.253/24 up
	jexec one ifconfig $bone inet6 no_dad 2001:db8:2::1/64

	vnet_mkjail two ${epair}b
	gtwo=$(jexec two ifconfig gif create)
	jexec two ifconfig ${epair}b 192.0.2.2/24 up
	jexec two ifconfig $gtwo tunnel 192.0.2.2 192.0.2.1
	jexec two ifconfig $gtwo 198.51.100.2/24 198.51.100.1 up
	jexec two ifconfig $gtwo inet6 no_dad 2001:db8:1::2/64

	btwo=$(jexec two ifconfig bridge create)
	jexec two ifconfig $btwo addm $gtwo
	jexec two ifconfig $btwo 192.168.169.254/24 up
	jexec two ifconfig $btwo inet6 no_dad 2001:db8:2::2/64

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    jexec one ping -c 1 192.0.2.2

	# EtherIP tunnel test
	atf_check -s exit:0 -o ignore \
	    jexec one ping -c 1 192.168.169.254
	atf_check -s exit:0 -o ignore \
	    jexec one ping -6 -c 1 2001:db8:2::2
	atf_check -s exit:0 -o ignore \
	    jexec two ping -c 1 192.168.169.253
	atf_check -s exit:0 -o ignore \
	    jexec two ping -6 -c 1 2001:db8:2::1

	# EtherIP should not affect normal IPv[46] over IPv4 tunnel
	# See bugzilla PR 227450
	# IPv4 in IPv4 Tunnel test
	atf_check -s exit:0 -o ignore \
	    jexec one ping -c 1 198.51.100.2
	atf_check -s exit:0 -o ignore \
	    jexec two ping -c 1 198.51.100.1

	# IPv6 in IPv4 tunnel test
	atf_check -s exit:0 -o ignore \
	    jexec one ping -6 -c 1 2001:db8:1::2
	atf_check -s exit:0 -o ignore \
	    jexec two ping -6 -c 1 2001:db8:1::1
}

etherip_cleanup()
{
	vnet_cleanup
}

atf_test_case "etherip6" "cleanup"
etherip6_head()
{
	atf_set descr 'EtherIP over IPv6 regression'
	atf_set require.user root
}

etherip6_body()
{
	vnet_init
	vnet_init_bridge

	if ! kldstat -q -m if_gif; then
		atf_skip "This test requires if_gif"
	fi

	epair=$(vnet_mkepair)

	vnet_mkjail one ${epair}a
	gone=$(jexec one ifconfig gif create)
	jexec one ifconfig ${epair}a inet6 no_dad 2001:db8::1/64 up
	jexec one ifconfig $gone inet6 tunnel 2001:db8::1 2001:db8::2
	jexec one ifconfig $gone 198.51.100.1/24 198.51.100.2 up
	jexec one ifconfig $gone inet6 no_dad 2001:db8:1::1/64

	bone=$(jexec one ifconfig bridge create)
	jexec one ifconfig $bone addm $gone
	jexec one ifconfig $bone 192.168.169.253/24 up
	jexec one ifconfig $bone inet6 no_dad 2001:db8:2::1/64

	vnet_mkjail two ${epair}b
	gtwo=$(jexec two ifconfig gif create)
	jexec two ifconfig ${epair}b inet6 no_dad 2001:db8::2/64 up
	jexec two ifconfig $gtwo inet6 tunnel 2001:db8::2 2001:db8::1
	jexec two ifconfig $gtwo 198.51.100.2/24 198.51.100.1 up
	jexec two ifconfig $gtwo inet6 no_dad 2001:db8:1::2/64

	btwo=$(jexec two ifconfig bridge create)
	jexec two ifconfig $btwo addm $gtwo
	jexec two ifconfig $btwo 192.168.169.254/24 up
	jexec two ifconfig $btwo inet6 no_dad 2001:db8:2::2/64

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    jexec one ping -6 -c 1 2001:db8::2

	# EtherIP tunnel test
	atf_check -s exit:0 -o ignore \
	    jexec one ping -c 1 192.168.169.254
	atf_check -s exit:0 -o ignore \
	    jexec one ping -6 -c 1 2001:db8:2::2
	atf_check -s exit:0 -o ignore \
	    jexec two ping -c 1 192.168.169.253
	atf_check -s exit:0 -o ignore \
	    jexec two ping -6 -c 1 2001:db8:2::1

	# EtherIP should not affect normal IPv[46] over IPv6 tunnel
	# See bugzilla PR 227450
	# IPv4 in IPv6 Tunnel test
	atf_check -s exit:0 -o ignore \
	    jexec one ping -c 1 198.51.100.2
	atf_check -s exit:0 -o ignore \
	    jexec two ping -c 1 198.51.100.1

	# IPv6 in IPv6 tunnel test
	atf_check -s exit:0 -o ignore \
	    jexec one ping -6 -c 1 2001:db8:1::2
	atf_check -s exit:0 -o ignore \
	    jexec two ping -6 -c 1 2001:db8:1::1
}

etherip6_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "4in4"
	atf_add_test_case "6in4"
	atf_add_test_case "4in6"
	atf_add_test_case "6in6"
	atf_add_test_case "etherip"
	atf_add_test_case "etherip6"
}
