# $FreeBSD$
#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2018 Kristof Provost <kp@FreeBSD.org>
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

atf_test_case "v4" "cleanup"
v4_head()
{
	atf_set descr 'Basic route-to test'
	atf_set require.user root
}

v4_body()
{
	pft_init

	epair_send=$(vnet_mkepair)
	ifconfig ${epair_send}a 192.0.2.1/24 up
	epair_route=$(vnet_mkepair)
	ifconfig ${epair_route}a 203.0.113.1/24 up

	vnet_mkjail alcatraz ${epair_send}b ${epair_route}b
	jexec alcatraz ifconfig ${epair_send}b 192.0.2.2/24 up
	jexec alcatraz ifconfig ${epair_route}b 203.0.113.2/24 up
	jexec alcatraz route add -net 198.51.100.0/24 192.0.2.1
	jexec alcatraz pfctl -e

	# Attempt to provoke PR 228782
	pft_set_rules alcatraz "block all" "pass user 2" \
		"pass out route-to (${epair_route}b 203.0.113.1) from 192.0.2.2 to 198.51.100.1 no state"
	jexec alcatraz nc -w 3 -s 192.0.2.2 198.51.100.1 22

	# atf wants us to not return an error, but our netcat will fail
	true
}

v4_cleanup()
{
	pft_cleanup
}

atf_test_case "v6" "cleanup"
v6_head()
{
	atf_set descr 'Basic route-to test (IPv6)'
	atf_set require.user root
}

v6_body()
{
	pft_init

	epair_send=$(vnet_mkepair)
	ifconfig ${epair_send}a inet6 2001:db8:42::1/64 up no_dad -ifdisabled
	epair_route=$(vnet_mkepair)
	ifconfig ${epair_route}a inet6 2001:db8:43::1/64 up no_dad -ifdisabled

	vnet_mkjail alcatraz ${epair_send}b ${epair_route}b
	jexec alcatraz ifconfig ${epair_send}b inet6 2001:db8:42::2/64 up no_dad
	jexec alcatraz ifconfig ${epair_route}b inet6 2001:db8:43::2/64 up no_dad
	jexec alcatraz route add -6 2001:db8:666::/64 2001:db8:42::2
	jexec alcatraz pfctl -e

	# Attempt to provoke PR 228782
	pft_set_rules alcatraz "block all" "pass user 2" \
		"pass out route-to (${epair_route}b 2001:db8:43::1) from 2001:db8:42::2 to 2001:db8:666::1 no state"
	jexec alcatraz nc -6 -w 3 -s 2001:db8:42::2 2001:db8:666::1 22

	# atf wants us to not return an error, but our netcat will fail
	true
}

v6_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "v4"
	atf_add_test_case "v6"
}
