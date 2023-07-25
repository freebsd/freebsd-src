#-
# Copyright (c) 2019, 2023 Shivank Garg <shivank@FreeBSD.org>
#
# This code was developed as a Google Summer of Code 2019 project
# under the guidance of Bjoern A. Zeeb.
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
# THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$

. $(atf_get_srcdir)/utils.subr

atf_test_case "ipacl_v4" "cleanup"

ipacl_v4_head()
{
	atf_set descr 'basic test for ipacl on IPv4 addresses'
	atf_set require.user root
}

ipacl_v4_body()
{
	ipacl_test_init

	epairA=$(vnet_mkepair)
	epairB=$(vnet_mkepair)
	epairC=$(vnet_mkepair)

	vnet_mkjail A ${epairA}b
	vnet_mkjail B ${epairB}b ${epairC}b

	jidA=$(jls -j A -s jid | grep -o -E '[0-9]+')
	jidB=$(jls -j B -s jid | grep -o -E '[0-9]+')

	# The ipacl policy module is not enforced for IPv4.
	sysctl security.mac.ipacl.ipv4=0

	atf_check -s exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b 192.0.2.2/24 up
	atf_check -s exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b 203.0.113.254/24 up

	# The ipacl policy module is enforced for IPv4 and prevent all
	# jails from setting their IPv4 address.
	sysctl security.mac.ipacl.ipv4=1
	sysctl security.mac.ipacl.rules=

	atf_check -s not-exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b 192.0.2.2/24 up
	atf_check -s not-exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b 203.0.113.254/24 up

	rule="${jidA},1,${epairA}b,AF_INET,192.0.2.42/-1@"
	rule="${rule}${jidB},1,${epairB}b,AF_INET,198.51.100.12/-1@"
	rule="${rule}${jidB},1,,AF_INET,203.0.113.1/24@"
	rule="${rule}${jidB},0,,AF_INET,203.0.113.9/-1"
	sysctl security.mac.ipacl.rules="${rule}"

	# Verify if it allows jail to set only certain IPv4 address.
	atf_check -s exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b 192.0.2.42/24 up
	atf_check -s not-exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b 192.0.2.43/24 up
	atf_check -s exit:0 -e ignore \
	    jexec B ifconfig ${epairB}b 198.51.100.12/24 up
	atf_check -s not-exit:0 -e ignore \
	    jexec B ifconfig ${epairC}b 198.51.100.12/24 up

	# Verify if the module allow jail to set any address in subnet.
	atf_check -s exit:0 -e ignore \
	    jexec B ifconfig ${epairB}b 203.0.113.19/24 up
	atf_check -s exit:0 -e ignore \
	    jexec B ifconfig ${epairB}b 203.0.113.241/24 up
	atf_check -s not-exit:0 -e ignore \
	    jexec B ifconfig ${epairB}b 198.18.0.1/24 up
	atf_check -s not-exit:0 -e ignore \
	    jexec B ifconfig ${epairB}b 203.0.113.9/24 up

	# Check wildcard for interfaces.
	atf_check -s exit:0 -e ignore \
	    jexec B ifconfig ${epairC}b 203.0.113.20/24 up
	atf_check -s exit:0 -e ignore \
	    jexec B ifconfig ${epairC}b 203.0.113.242/24 up
	atf_check -s not-exit:0 -e ignore \
	    jexec B ifconfig ${epairC}b 198.18.0.1/24 up
	atf_check -s not-exit:0 -e ignore \
	    jexec B ifconfig ${epairC}b 203.0.113.9/24 up

	rule="${jidA},1,,AF_INET,198.18.0.0/15@"
	rule="${rule}${jidA},0,,AF_INET,198.18.23.0/24@"
	rule="${rule}${jidA},1,,AF_INET,198.18.23.1/-1@"
	rule="${rule}${jidA},1,,AF_INET,198.51.100.0/24@"
	rule="${rule}${jidA},0,,AF_INET,198.51.100.100/-1"
	sysctl security.mac.ipacl.rules="${rule}"

	# Tests from Benchamarking and Documentation(TEST-NET-3).
	atf_check -s exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b 198.18.0.1/24 up
	atf_check -s not-exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b 198.18.23.2/24 up
	atf_check -s exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b 198.18.23.1/22 up
	atf_check -s not-exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b 198.18.23.3/24 up

	atf_check -s exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b 198.51.100.001/24 up
	atf_check -s exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b 198.51.100.254/24 up
	atf_check -s not-exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b 198.51.100.100/24 up
	atf_check -s not-exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b 203.0.113.1/24 up

	# Reset rules OID.
	sysctl security.mac.ipacl.rules=
}

ipacl_v4_cleanup()
{
	ipacl_test_cleanup
}

atf_test_case "ipacl_v6" "cleanup"

ipacl_v6_head()
{
	atf_set descr 'basic test for ipacl on IPv6 addresses'
	atf_set require.user root
}

ipacl_v6_body()
{
	ipacl_test_init

	epairA=$(vnet_mkepair)
	epairB=$(vnet_mkepair)
	epairC=$(vnet_mkepair)

	vnet_mkjail A ${epairA}b
	vnet_mkjail B ${epairB}b ${epairC}b

	jidA=$(jls -j A -s jid | grep -o -E '[0-9]+')
	jidB=$(jls -j B -s jid | grep -o -E '[0-9]+')

	# The ipacl policy module is not enforced for IPv6.
	sysctl security.mac.ipacl.ipv6=0

	atf_check -s exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b inet6 2001:2::abcd/48 up
	atf_check -s exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b inet6 2001:2::5ea:11/48 up

	# The ipacl policy module is enforced for IPv6 and prevent all
	# jails from setting their IPv6 address.
	sysctl security.mac.ipacl.ipv6=1
	sysctl security.mac.ipacl.rules=

	atf_check -s not-exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b inet6 2001:2::abcd/48 up
	atf_check -s not-exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b inet6 2001:2::5ea:11/48 up

	rule="${jidA},1,${epairA}b,AF_INET6,2001:db8::1111/-1@"
	rule="${rule}${jidB},1,${epairB}b,AF_INET6,2001:2::1234:1234/-1@"
	rule="${rule}${jidB},1,,AF_INET6,fe80::/32@"
	rule="${rule}${jidB},0,,AF_INET6,fe80::abcd/-1"
	sysctl security.mac.ipacl.rules="${rule}"

	# Verify if it allows jail to set only certain IPv6 address.
	atf_check -s exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b inet6 2001:db8::1111/64 up
	atf_check -s not-exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b inet6 2001:db8::1112/64 up
	atf_check -s exit:0 -e ignore \
	    jexec B ifconfig ${epairB}b inet6 2001:2::1234:1234/48 up
	atf_check -s not-exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b inet6 2001:2::1234:1234/48 up

	# Verify if the module allow jail set any address in subnet.
	atf_check -s exit:0 -e ignore \
	    jexec B ifconfig ${epairB}b inet6 FE80::1101:1221/15 up
	atf_check -s exit:0 -e ignore \
	    jexec B ifconfig ${epairB}b inet6 FE80::abab/15 up
	atf_check -s exit:0 -e ignore \
	    jexec B ifconfig ${epairB}b inet6 FE80::1/64 up
	atf_check -s not-exit:0 -e ignore \
	    jexec B ifconfig ${epairB}b inet6 FE80::abcd/15 up

	# Check wildcard for interfaces.
	atf_check -s exit:0 -e ignore \
	    jexec B ifconfig ${epairC}b inet6 FE80::1101:1221/15 up
	atf_check -s exit:0 -e ignore \
	    jexec B ifconfig ${epairC}b inet6 FE80::abab/32 up
	atf_check -s not-exit:0 -e ignore \
	    jexec B ifconfig ${epairC}b inet6 FE81::1/64 up
	atf_check -s not-exit:0 -e ignore \
	    jexec B ifconfig ${epairC}b inet6 FE80::abcd/32 up

	rule="${jidB},1,,AF_INET6,2001:2::/48@"
	rule="${rule}${jidB},1,,AF_INET6,2001:3::/32"
	sysctl security.mac.ipacl.rules="${rule}"

	# Tests when subnet is allowed.
	atf_check -s not-exit:0 -e ignore \
	    jexec B ifconfig ${epairC}b inet6 2001:2:0001::1/64 up
	atf_check -s not-exit:0 -e ignore \
	    jexec B ifconfig ${epairC}b inet6 2001:2:1000::1/32 up
	atf_check -s exit:0 -e ignore \
	    jexec B ifconfig ${epairC}b inet6 2001:3:0001::1/64 up
	atf_check -s not-exit:0 -e ignore \
	    jexec B ifconfig ${epairC}b inet6 2001:4::1/64 up

	# More tests of ULA address space.
	rule="${jidA},1,,AF_INET6,fc00::/7@"
	rule="${rule}${jidA},0,,AF_INET6,fc00::1111:2200/120@"
	rule="${rule}${jidA},1,,AF_INET6,fc00::1111:2299/-1@"
	rule="${rule}${jidA},1,,AF_INET6,2001:db8::/32@"
	rule="${rule}${jidA},0,,AF_INET6,2001:db8::abcd/-1"
	sysctl security.mac.ipacl.rules="${rule}"

	atf_check -s exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b inet6 fc00::0000:1234/48 up
	atf_check -s exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b inet6 fc00::0000:1234/48 up
	atf_check -s not-exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b inet6 f800::2222:2200/48 up
	atf_check -s not-exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b inet6 f800::2222:22ff/48 up

	atf_check -s exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b inet6 fc00::1111:2111/64 up
	atf_check -s not-exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b inet6 fc00::1111:2211/64 up
	atf_check -s not-exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b inet6 fc00::1111:22aa/48 up
	atf_check -s exit:0 -e ignore \
	    jexec A ifconfig ${epairA}b inet6 fc00::1111:2299/48 up

	# More tests from IPv6 documentation range.
	atf_check -s exit:0 -e ignore jexec A ifconfig \
	    ${epairA}b inet6 2001:db8:abcd:bcde:cdef:def1:ef12:f123/32 up
	atf_check -s exit:0 -e ignore jexec A ifconfig \
	    ${epairA}b inet6 2001:db8:1111:2222:3333:4444:5555:6666/32 up
	atf_check -s not-exit:0 -e ignore jexec A ifconfig \
	    ${epairA}b inet6 2001:ab9:1111:2222:3333:4444:5555:6666/32 up
	atf_check -s not-exit:0 -e ignore jexec A ifconfig \
	    ${epairA}b inet6 2001:db8::abcd/32 up

	# Reset rules OID.
	sysctl security.mac.ipacl.rules=
}

ipacl_v6_cleanup()
{
	ipacl_test_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "ipacl_v4"
	atf_add_test_case "ipacl_v6"
}
