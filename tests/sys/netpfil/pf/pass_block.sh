# $FreeBSD$
#
# SPDX-License-Identifier: BSD-2-Clause
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

common_dir=$(atf_get_srcdir)/../common

atf_test_case "v4" "cleanup"
v4_head()
{
	atf_set descr 'Basic pass/block test for IPv4'
	atf_set require.user root
}

v4_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a 192.0.2.1/24 up

	# Set up a simple jail with one interface
	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up

	# Trivial ping to the jail, without pf
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 192.0.2.2

	# pf without policy will let us ping
	jexec alcatraz pfctl -e
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 192.0.2.2

	# Block everything
	pft_set_rules alcatraz "block in"
	atf_check -s exit:2 -o ignore ping -c 1 -t 1 192.0.2.2

	# Block everything but ICMP
	pft_set_rules alcatraz "block in" "pass in proto icmp"
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 192.0.2.2
}

v4_cleanup()
{
	pft_cleanup
}

atf_test_case "v6" "cleanup"
v6_head()
{
	atf_set descr 'Basic pass/block test for IPv6'
	atf_set require.user root
}

v6_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a inet6 2001:db8:42::1/64 up no_dad

	# Set up a simple jail with one interface
	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b inet6 2001:db8:42::2/64 up no_dad

	# Trivial ping to the jail, without pf
	atf_check -s exit:0 -o ignore ping -6 -c 1 -W 1 2001:db8:42::2

	# pf without policy will let us ping
	jexec alcatraz pfctl -e
	atf_check -s exit:0 -o ignore ping -6 -c 1 -W 1 2001:db8:42::2

	# Block everything
	pft_set_rules alcatraz "block in"
	atf_check -s exit:2 -o ignore ping -6 -c 1 -W 1 2001:db8:42::2

	# Block everything but ICMP
	pft_set_rules alcatraz "block in" "pass in proto icmp6"
	atf_check -s exit:0 -o ignore ping -6 -c 1 -W 1 2001:db8:42::2

	# Allowing ICMPv4 does not allow ICMPv6
	pft_set_rules alcatraz "block in" "pass in proto icmp"
	atf_check -s exit:2 -o ignore ping -6 -c 1 -W 1 2001:db8:42::2
}

v6_cleanup()
{
	pft_cleanup
}

atf_test_case "noalias" "cleanup"
noalias_head()
{
	atf_set descr 'Test the :0 noalias option'
	atf_set require.user root
}

noalias_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a inet6 2001:db8:42::1/64 up no_dad

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b inet6 2001:db8:42::2/64 up no_dad

	linklocaladdr=$(jexec alcatraz ifconfig ${epair}b inet6 \
		| grep %${epair}b \
		| awk '{ print $2; }' \
		| cut -d % -f 1)

	# Sanity check
	atf_check -s exit:0 -o ignore ping -6 -c 3 -W 1 2001:db8:42::2
	atf_check -s exit:0 -o ignore ping -6 -c 3 -W 1 ${linklocaladdr}%${epair}a

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz "block out inet6 from (${epair}b:0) to any"

	atf_check -s exit:2 -o ignore ping -6 -c 3 -W 1 2001:db8:42::2

	# We should still be able to ping the link-local address
	atf_check -s exit:0 -o ignore ping -6 -c 3 -W 1 ${linklocaladdr}%${epair}a

	pft_set_rules alcatraz "block out inet6 from (${epair}b) to any"

	# We cannot ping to the link-local address
	atf_check -s exit:2 -o ignore ping -6 -c 3 -W 1 ${linklocaladdr}%${epair}a
}

noalias_cleanup()
{
	pft_cleanup
}

atf_test_case "nested_inline" "cleanup"
nested_inline_head()
{
	atf_set descr "Test nested inline anchors, PR196314"
	atf_set require.user root
}

nested_inline_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a inet 192.0.2.1/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"block in" \
		"anchor \"an1\" {" \
			"pass in quick proto tcp to port time" \
			"anchor \"an2\" {" \
				"pass in quick proto icmp" \
			"}" \
		"}"

	atf_check -s exit:0 -o ignore ping -c 1 -t 1 192.0.2.2
}

nested_inline_cleanup()
{
	pft_cleanup
}

atf_test_case "urpf" "cleanup"
urpf_head()
{
	atf_set descr "Test unicast reverse path forwarding check"
	atf_set require.user root
	atf_set require.progs scapy
}

urpf_body()
{
	pft_init

	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair_one}b ${epair_two}b

	ifconfig ${epair_one}a 192.0.2.2/24 up
	ifconfig ${epair_two}a 198.51.100.2/24 up

	jexec alcatraz ifconfig ${epair_one}b 192.0.2.1/24 up
	jexec alcatraz ifconfig ${epair_two}b 198.51.100.1/24 up
	jexec alcatraz sysctl net.inet.ip.forwarding=1

	# Sanity checks
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 192.0.2.1
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 198.51.100.1
	atf_check -s exit:0 ${common_dir}/pft_ping.py \
		--sendif ${epair_one}a \
		--to 192.0.2.1 \
		--fromaddr 198.51.100.2 \
		--replyif ${epair_two}a
	atf_check -s exit:0 ${common_dir}/pft_ping.py \
		--sendif ${epair_two}a \
		--to 198.51.100.1 \
		--fromaddr 192.0.2.2 \
		--replyif ${epair_one}a

	pft_set_rules alcatraz \
		"block quick from urpf-failed"
	jexec alcatraz pfctl -e

	# Correct source still works
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 192.0.2.1
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 198.51.100.1

	# Unexpected source interface is blocked
	atf_check -s exit:1 ${common_dir}/pft_ping.py \
		--sendif ${epair_one}a \
		--to 192.0.2.1 \
		--fromaddr 198.51.100.2 \
		--replyif ${epair_two}a
	atf_check -s exit:1 ${common_dir}/pft_ping.py \
		--sendif ${epair_two}a \
		--to 198.51.100.1 \
		--fromaddr 192.0.2.2 \
		--replyif ${epair_one}a
}

urpf_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "v4"
	atf_add_test_case "v6"
	atf_add_test_case "noalias"
	atf_add_test_case "nested_inline"
	atf_add_test_case "urpf"
}
