#
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright © 2023 Tom Jones <thj@freebsd.org>
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

atf_test_case "tcp_v6" "cleanup"
tcp_v6_head()
{
	atf_set descr 'TCP rdr with IPv6'
	atf_set require.user root
	atf_set require.progs scapy python3
}

#
# Test that rdr works for TCP with IPv6.
#
#	a <-----> b <-----> c
#
# Test configures three jails (a, b and c) and connects them together with b as
# a router between a and c.
#
# TCP traffic to b on port 80 is redirected to c on port 8000
#
# Test for incorrect checksums after the rewrite by looking at a packet capture (see bug 210860)
#
tcp_v6_body()
{
	pft_init

	j="rdr:tcp_v6"
	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)

	echo $epair_one
	echo $epair_two

	vnet_mkjail ${j}a ${epair_one}b
	vnet_mkjail ${j}b ${epair_one}a ${epair_two}a
	vnet_mkjail ${j}c ${epair_two}b

	# configure addresses for b
	jexec ${j}b ifconfig lo0 up
	jexec ${j}b ifconfig ${epair_one}a inet6 2001:db8:a::1/64 up no_dad
	jexec ${j}b ifconfig ${epair_two}a inet6 2001:db8:b::1/64 up no_dad

	# configure addresses for a
	jexec ${j}a ifconfig lo0 up
	jexec ${j}a ifconfig ${epair_one}b inet6 2001:db8:a::2/64 up no_dad

	# configure addresses for c
	jexec ${j}c ifconfig lo0 up
	jexec ${j}c ifconfig ${epair_two}b inet6 2001:db8:b::2/64 up no_dad

	# enable forwarding in the b jail
	jexec ${j}b sysctl net.inet6.ip6.forwarding=1

	# add routes so a and c can find each other
	jexec ${j}a route add -inet6 2001:db8:b::0/64 2001:db8:a::1
	jexec ${j}c route add -inet6 2001:db8:a::0/64 2001:db8:b::1

	jexec ${j}b pfctl -e

	pft_set_rules ${j}b \
		"rdr on ${epair_one}a proto tcp from any to any port 80 -> 2001:db8:b::2 port 8000"

	# Check that a can reach c over the router
	atf_check -s exit:0 -o ignore \
	    jexec ${j}a ping -6 -c 1 2001:db8:b::2

	# capture packets on c so we can look for incorrect checksums
	jexec ${j}c tcpdump --immediate-mode -w ${j}.pcap tcp and port 8000 &
	tcpdumppid=$!

	# start a web server and give it a second to start
	jexec ${j}c python3 -m http.server &
	sleep 1

	# http directly from a to c -> a ---> b
	atf_check -s exit:0 -o ignore \
		jexec ${j}a fetch -T 1 -o /dev/null -q "http://[2001:db8:b::2]:8000"

	# http from a to b with a redirect  -> a ---> b
	atf_check -s exit:0 -o ignore \
		jexec ${j}a fetch -T 1 -o /dev/null -q "http://[2001:db8:a::1]:80"

	# ask tcpdump to stop so we can check the packet capture
	jexec ${j}c kill -s SIGINT $tcpdumppid

	# Check for 'incorrect' in packet capture, this should tell us if
	# checksums are bad with rdr rules
	count=$(jexec ${j}c tcpdump -vvvv -r ${j}.pcap | grep incorrect | wc -l)
	atf_check_equal "       0" "$count"
}

tcp_v6_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "tcp_v6"
}
