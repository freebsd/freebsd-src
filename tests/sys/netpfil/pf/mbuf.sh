#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 Igor Ostapenko <pm@igoro.pro>
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

dummymbuf_init()
{
	if ! kldstat -q -m dummymbuf; then
		atf_skip "This test requires dummymbuf"
	fi
}

atf_test_case "inet_in_mbuf_len" "cleanup"
inet_in_mbuf_len_head()
{
	atf_set descr 'Test that pf can handle inbound with the first mbuf with m_len < sizeof(struct ip)'
	atf_set require.user root
}
inet_in_mbuf_len_body()
{
	pft_init
	dummymbuf_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a 192.0.2.1/24 up

	# Set up a simple jail with one interface
	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c1 192.0.2.2

	# Should be denied
	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"block"
	atf_check -s not-exit:0 -o ignore ping -c1 -t1 192.0.2.2

	# Should be allowed by from/to addresses
	pft_set_rules alcatraz \
		"block" \
		"pass in from 192.0.2.1 to 192.0.2.2"
	atf_check -s exit:0 -o ignore ping -c1 192.0.2.2

	# Should still work for m_len=0
	jexec alcatraz pfilctl link -i dummymbuf:inet inet
	jexec alcatraz sysctl net.dummymbuf.rules="inet in ${epair}b pull-head 0;"
	atf_check_equal "0" "$(jexec alcatraz sysctl -n net.dummymbuf.hits)"
	atf_check -s exit:0 -o ignore ping -c1 192.0.2.2
	atf_check_equal "1" "$(jexec alcatraz sysctl -n net.dummymbuf.hits)"

	# m_len=1
	jexec alcatraz sysctl net.dummymbuf.rules="inet in ${epair}b pull-head 1;"
	jexec alcatraz sysctl net.dummymbuf.hits=0
	atf_check -s exit:0 -o ignore ping -c1 192.0.2.2
	atf_check_equal "1" "$(jexec alcatraz sysctl -n net.dummymbuf.hits)"

	# m_len=19
	# provided IPv4 basic header is 20 bytes long, it should impact the dst addr
	jexec alcatraz sysctl net.dummymbuf.rules="inet in ${epair}b pull-head 19;"
	jexec alcatraz sysctl net.dummymbuf.hits=0
	atf_check -s exit:0 -o ignore ping -c1 192.0.2.2
	atf_check_equal "1" "$(jexec alcatraz sysctl -n net.dummymbuf.hits)"
}
inet_in_mbuf_len_cleanup()
{
	pft_cleanup
}

atf_test_case "inet6_in_mbuf_len" "cleanup"
inet6_in_mbuf_len_head()
{
	atf_set descr 'Test that pf can handle inbound with the first mbuf with m_len < sizeof(struct ip6_hdr)'
	atf_set require.user root
}
inet6_in_mbuf_len_body()
{
	pft_init
	dummymbuf_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a inet6 2001:db8::1/64 up no_dad

	# Set up a simple jail with one interface
	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b inet6 2001:db8::2/64 up no_dad

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c1 2001:db8::2

	# Should be denied
	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"block" \
		"pass quick inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }"
	atf_check -s not-exit:0 -o ignore ping -c1 -t1 2001:db8::2

	# Avoid redundant ICMPv6 packets to avoid false positives during
	# counting of net.dummymbuf.hits.
	ndp -i ${epair}a -- -nud
	jexec alcatraz ndp -i ${epair}b -- -nud

	# Should be allowed by from/to addresses
	pft_set_rules alcatraz \
		"block" \
		"pass quick inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
		"pass in inet6 from 2001:db8::1 to 2001:db8::2"
	atf_check -s exit:0 -o ignore ping -c1 2001:db8::2

	# Should still work for m_len=0
	jexec alcatraz pfilctl link -i dummymbuf:inet6 inet6
	jexec alcatraz sysctl net.dummymbuf.rules="inet6 in ${epair}b pull-head 0;"
	atf_check_equal "0" "$(jexec alcatraz sysctl -n net.dummymbuf.hits)"
	atf_check -s exit:0 -o ignore ping -c1 2001:db8::2
	atf_check_equal "1" "$(jexec alcatraz sysctl -n net.dummymbuf.hits)"

	# m_len=1
	jexec alcatraz sysctl net.dummymbuf.rules="inet6 in ${epair}b pull-head 1;"
	jexec alcatraz sysctl net.dummymbuf.hits=0
	atf_check -s exit:0 -o ignore ping -c1 2001:db8::2
	atf_check_equal "1" "$(jexec alcatraz sysctl -n net.dummymbuf.hits)"

	# m_len=39
	# provided IPv6 basic header is 40 bytes long, it should impact the dst addr
	jexec alcatraz sysctl net.dummymbuf.rules="inet6 in ${epair}b pull-head 39;"
	jexec alcatraz sysctl net.dummymbuf.hits=0
	atf_check -s exit:0 -o ignore ping -c1 2001:db8::2
	atf_check_equal "1" "$(jexec alcatraz sysctl -n net.dummymbuf.hits)"
}
inet6_in_mbuf_len_cleanup()
{
	pft_cleanup
}

atf_test_case "ethernet_in_mbuf_len" "cleanup"
ethernet_in_mbuf_len_head()
{
	atf_set descr 'Test that pf can handle inbound with the first mbuf with m_len < sizeof(struct ether_header)'
	atf_set require.user root
}
ethernet_in_mbuf_len_body()
{
	pft_init
	dummymbuf_init

	epair=$(vnet_mkepair)
	epair_a_mac=$(ifconfig ${epair}a ether | awk '/ether/ { print $2; }')
	ifconfig ${epair}a 192.0.2.1/24 up

	# Set up a simple jail with one interface
	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up
	epair_b_mac=$(jexec alcatraz ifconfig ${epair}b ether | awk '/ether/ { print $2; }')

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c1 192.0.2.2

	# Should be denied
	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"ether block" \
		"pass"
	atf_check -s not-exit:0 -o ignore ping -c1 -t1 192.0.2.2

	# Should be allowed by from/to addresses
	echo $epair_a_mac
	echo $epair_b_mac
	pft_set_rules alcatraz \
		"ether block" \
		"ether pass in  from ${epair_a_mac} to ${epair_b_mac}" \
		"ether pass out from ${epair_b_mac} to ${epair_a_mac}" \
		"pass"
	atf_check -s exit:0 -o ignore ping -c1 192.0.2.2

	# Should still work for m_len=0
	jexec alcatraz pfilctl link -i dummymbuf:ethernet ethernet
	jexec alcatraz sysctl net.dummymbuf.rules="ethernet in ${epair}b pull-head 0;"
	atf_check_equal "0" "$(jexec alcatraz sysctl -n net.dummymbuf.hits)"
	atf_check -s exit:0 -o ignore ping -c1 192.0.2.2
	atf_check_equal "1" "$(jexec alcatraz sysctl -n net.dummymbuf.hits)"

	# m_len=1
	jexec alcatraz sysctl net.dummymbuf.rules="ethernet in ${epair}b pull-head 1;"
	jexec alcatraz sysctl net.dummymbuf.hits=0
	atf_check -s exit:0 -o ignore ping -c1 192.0.2.2
	atf_check_equal "1" "$(jexec alcatraz sysctl -n net.dummymbuf.hits)"

	# m_len=11
	# for the simplest L2 Ethernet frame it should impact src field
	jexec alcatraz sysctl net.dummymbuf.rules="ethernet in ${epair}b pull-head 11;"
	jexec alcatraz sysctl net.dummymbuf.hits=0
	atf_check -s exit:0 -o ignore ping -c1 192.0.2.2
	atf_check_equal "1" "$(jexec alcatraz sysctl -n net.dummymbuf.hits)"

	# m_len=13
	# provided L2 Ethernet simplest header is 14 bytes long, it should impact ethertype field
	jexec alcatraz sysctl net.dummymbuf.rules="ethernet in ${epair}b pull-head 13;"
	jexec alcatraz sysctl net.dummymbuf.hits=0
	atf_check -s exit:0 -o ignore ping -c1 192.0.2.2
	atf_check_equal "1" "$(jexec alcatraz sysctl -n net.dummymbuf.hits)"
}
ethernet_in_mbuf_len_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "inet_in_mbuf_len"
	atf_add_test_case "inet6_in_mbuf_len"
	atf_add_test_case "ethernet_in_mbuf_len"
}
