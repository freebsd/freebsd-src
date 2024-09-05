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

atf_test_case "exhaust" "cleanup"
exhaust_head()
{
	atf_set descr 'Test exhausting the NAT pool'
	atf_set require.user root
}

exhaust_body()
{
	pft_init

	epair_nat=$(vnet_mkepair)
	epair_echo=$(vnet_mkepair)

	vnet_mkjail nat ${epair_nat}b ${epair_echo}a
	vnet_mkjail echo ${epair_echo}b

	ifconfig ${epair_nat}a 192.0.2.2/24 up
	route add -net 198.51.100.0/24 192.0.2.1

	jexec nat ifconfig ${epair_nat}b 192.0.2.1/24 up
	jexec nat ifconfig ${epair_echo}a 198.51.100.1/24 up
	jexec nat sysctl net.inet.ip.forwarding=1

	jexec echo ifconfig ${epair_echo}b 198.51.100.2/24 up
	jexec echo /usr/sbin/inetd -p ${PWD}/inetd-echo.pid $(atf_get_srcdir)/echo_inetd.conf

	# Enable pf!
	jexec nat pfctl -e
	pft_set_rules nat \
		"nat pass on ${epair_echo}a inet from 192.0.2.0/24 to any -> (${epair_echo}a) port 30000:30001 sticky-address"

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 3 198.51.100.2

	atf_check -s exit:0 -o match:foo* echo "foo" | nc -N 198.51.100.2 7
	atf_check -s exit:0 -o match:foo* echo "foo" | nc -N 198.51.100.2 7

	# This one will fail, but that's expected
	echo "foo" | nc -N 198.51.100.2 7 &

	sleep 1

	# If the kernel is stuck in pf_get_sport() this will not succeed either.
	timeout 2 jexec nat pfctl -sa
	if [ $? -eq 124 ]; then
		# Timed out
		atf_fail "pfctl timeout"
	fi
}

exhaust_cleanup()
{
	pft_cleanup
}

atf_test_case "nested_anchor" "cleanup"
nested_anchor_head()
{
	atf_set descr 'Test setting and retrieving nested nat anchors'
	atf_set require.user root
}

nested_anchor_body()
{
	pft_init

	epair=$(vnet_mkepair)

	vnet_mkjail nat ${epair}a

	pft_set_rules nat \
		"nat-anchor \"foo\""

	echo "nat-anchor \"bar\"" | jexec nat pfctl -g -a foo -f -
	echo "nat on ${epair}a from any to any -> (${epair}a)" | jexec nat pfctl -g -a "foo/bar" -f -

	atf_check -s exit:0 -o inline:"nat-anchor \"foo\" all {
  nat-anchor \"bar\" all {
    nat on ${epair}a all -> (${epair}a) round-robin
  }
}
" jexec nat pfctl -sn -a "*"

}

atf_test_case "endpoint_independent" "cleanup"
endpoint_independent_head()
{
	atf_set descr 'Test that a client behind NAT gets the same external IP:port for different servers'
	atf_set require.user root
}

endpoint_independent_body()
{
	pft_init
	filter="udp and dst port 1234"	# only capture udp pings

	epair_client=$(vnet_mkepair)
	epair_nat=$(vnet_mkepair)
	epair_server1=$(vnet_mkepair)
	epair_server2=$(vnet_mkepair)
	bridge=$(vnet_mkbridge)

	vnet_mkjail nat ${epair_client}b ${epair_nat}a
	vnet_mkjail client ${epair_client}a
	vnet_mkjail server1 ${epair_server1}a
	vnet_mkjail server2 ${epair_server2}a

	ifconfig ${epair_server1}b up
	ifconfig ${epair_server2}b up
	ifconfig ${epair_nat}b up
	ifconfig ${bridge} \
		addm ${epair_server1}b \
		addm ${epair_server2}b \
		addm ${epair_nat}b \
		up

	jexec nat ifconfig ${epair_client}b 192.0.2.1/24 up
	jexec nat ifconfig ${epair_nat}a 198.51.100.42/24 up
	jexec nat sysctl net.inet.ip.forwarding=1

	jexec client ifconfig ${epair_client}a 192.0.2.2/24 up
	jexec client route add default 192.0.2.1

	jexec server1 ifconfig ${epair_server1}a 198.51.100.32/24 up
	jexec server2 ifconfig ${epair_server2}a 198.51.100.22/24 up

	# Enable pf!
	jexec nat pfctl -e

	# validate non-endpoint independent nat rule behaviour
	pft_set_rules nat \
		"nat on ${epair_nat}a inet from ! (${epair_nat}a) to any -> (${epair_nat}a)"

	jexec server1 tcpdump -i ${epair_server1}a -w ${PWD}/server1.pcap \
		--immediate-mode $filter &
	server1tcppid="$!"
	jexec server2 tcpdump -i ${epair_server2}a -w ${PWD}/server2.pcap \
		--immediate-mode $filter &
	server2tcppid="$!"

	# send out multiple packets
	for i in $(seq 1 10); do
		echo "ping" | jexec client nc -u 198.51.100.32 1234 -p 4242 -w 0
		echo "ping" | jexec client nc -u 198.51.100.22 1234 -p 4242 -w 0
	done

	kill $server1tcppid
	kill $server2tcppid

	tuple_server1=$(tcpdump -r ${PWD}/server1.pcap | awk '{addr=$3} END {print addr}')
	tuple_server2=$(tcpdump -r ${PWD}/server2.pcap | awk '{addr=$3} END {print addr}')

	if [ -z $tuple_server1 ]
	then
		atf_fail "server1 did not receive connection from client (default)"
	fi

	if [ -z $tuple_server2 ]
	then
		atf_fail "server2 did not receive connection from client (default)"
	fi

	if [ "$tuple_server1" = "$tuple_server2" ]
	then
		echo "server1 tcpdump: $tuple_server1"
		echo "server2 tcpdump: $tuple_server2"
		atf_fail "Received same IP:port on server1 and server2 (default)"
	fi

	# validate endpoint independent nat rule behaviour
	pft_set_rules nat \
		"nat on ${epair_nat}a inet from ! (${epair_nat}a) to any -> (${epair_nat}a) endpoint-independent"

	jexec server1 tcpdump -i ${epair_server1}a -w ${PWD}/server1.pcap \
		--immediate-mode $filter &
	server1tcppid="$!"
	jexec server2 tcpdump -i ${epair_server2}a -w ${PWD}/server2.pcap \
		--immediate-mode $filter &
	server2tcppid="$!"

	# send out multiple packets,  sometimes one fails to go through
	for i in $(seq 1 10); do
		echo "ping" | jexec client nc -u 198.51.100.32 1234 -p 4242 -w 0
		echo "ping" | jexec client nc -u 198.51.100.22 1234 -p 4242 -w 0
	done

	kill $server1tcppid
	kill $server2tcppid

	tuple_server1=$(tcpdump -r ${PWD}/server1.pcap | awk '{addr=$3} END {print addr}')
	tuple_server2=$(tcpdump -r ${PWD}/server2.pcap | awk '{addr=$3} END {print addr}')

	if [ -z $tuple_server1 ]
	then
		atf_fail "server1 did not receive connection from client (endpoint-independent)"
	fi

	if [ -z $tuple_server2 ]
	then
		atf_fail "server2 did not receive connection from client (endpoint-independent)"
	fi

	if [ ! "$tuple_server1" = "$tuple_server2" ]
	then
		echo "server1 tcpdump: $tuple_server1"
		echo "server2 tcpdump: $tuple_server2"
		atf_fail "Received different IP:port on server1 than server2 (endpoint-independent)"
	fi
}

endpoint_independent_cleanup()
{
	pft_cleanup
	rm -f server1.out
	rm -f server2.out
}

nested_anchor_cleanup()
{
	pft_cleanup
}

atf_test_case "nat6_nolinklocal" "cleanup"
nat6_nolinklocal_head()
{
	atf_set descr 'Ensure we do not use link-local addresses'
	atf_set require.user root
}

nat6_nolinklocal_body()
{
	pft_init

	epair_nat=$(vnet_mkepair)
	epair_echo=$(vnet_mkepair)

	vnet_mkjail nat ${epair_nat}b ${epair_echo}a
	vnet_mkjail echo ${epair_echo}b

	ifconfig ${epair_nat}a inet6 2001:db8::2/64 no_dad up
	route add -6 -net 2001:db8:1::/64 2001:db8::1

	jexec nat ifconfig ${epair_nat}b inet6 2001:db8::1/64 no_dad up
	jexec nat ifconfig ${epair_echo}a inet6 2001:db8:1::1/64 no_dad up
	jexec nat sysctl net.inet6.ip6.forwarding=1

	jexec echo ifconfig ${epair_echo}b inet6 2001:db8:1::2/64 no_dad up
	# Ensure we can't reply to link-local pings
	jexec echo pfctl -e
	pft_set_rules echo \
	    "pass" \
	    "block in inet6 proto icmp6 from fe80::/10 to any icmp6-type echoreq"

	jexec nat pfctl -e
	pft_set_rules nat \
	    "nat pass on ${epair_echo}a inet6 from 2001:db8::/64 to any -> (${epair_echo}a)" \
	    "pass"

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    ping -6 -c 1 2001:db8::1
	for i in `seq 0 10`
	do
		atf_check -s exit:0 -o ignore \
		    ping -6 -c 1 2001:db8:1::2
	done
}

nat6_nolinklocal_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "exhaust"
	atf_add_test_case "nested_anchor"
	atf_add_test_case "endpoint_independent"
	atf_add_test_case "nat6_nolinklocal"
}
