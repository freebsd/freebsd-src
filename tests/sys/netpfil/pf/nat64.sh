#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 Rubicon Communications, LLC (Netgate)
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

nat64_setup()
{
	pft_init

	epair_link=$(vnet_mkepair)
	epair=$(vnet_mkepair)

	ifconfig ${epair}a inet6 2001:db8::2/64 up no_dad
	route -6 add default 2001:db8::1

	vnet_mkjail rtr ${epair}b ${epair_link}a
	jexec rtr ifconfig ${epair}b inet6 2001:db8::1/64 up no_dad
	jexec rtr ifconfig ${epair_link}a 192.0.2.1/24 up

	vnet_mkjail dst ${epair_link}b
	jexec dst ifconfig ${epair_link}b 192.0.2.2/24 up
	jexec dst route add default 192.0.2.1

	# Sanity checks
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 2001:db8::1
	atf_check -s exit:0 -o ignore \
	    jexec dst ping -c 1 192.0.2.1

	jexec rtr pfctl -e
	pft_set_rules rtr \
	    "set reassemble yes" \
	    "set state-policy if-bound" \
	    "pass in on ${epair}b inet6 from any to 64:ff9b::/96 af-to inet from (${epair_link}a)"
}

atf_test_case "icmp_echo" "cleanup"
icmp_echo_head()
{
	atf_set descr 'Basic NAT64 ICMP echo test'
	atf_set require.user root
}

icmp_echo_body()
{
	nat64_setup

	# One ping
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 64:ff9b::192.0.2.2

	# Make sure packets make it even when state is established
	atf_check -s exit:0 \
	    -o match:'5 packets transmitted, 5 packets received, 0.0% packet loss' \
	    ping6 -c 5 64:ff9b::192.0.2.2
}

icmp_echo_cleanup()
{
	pft_cleanup
}

atf_test_case "fragmentation" "cleanup"
fragmentation_head()
{
	atf_set descr 'Test fragmented packets'
	atf_set require.user root
}

fragmentation_body()
{
	nat64_setup

	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 -s 1280 64:ff9b::192.0.2.2

	atf_check -s exit:0 \
	    -o match:'3 packets transmitted, 3 packets received, 0.0% packet loss' \
	    ping6 -c 3 -s 2000 64:ff9b::192.0.2.2
	atf_check -s exit:0 \
	    -o match:'3 packets transmitted, 3 packets received, 0.0% packet loss' \
	    ping6 -c 3 -s 10000 -b 20000 64:ff9b::192.0.2.2
}

fragmentation_cleanup()
{
	pft_cleanup
}

atf_test_case "tcp" "cleanup"
tcp_head()
{
	atf_set descr 'TCP NAT64 test'
	atf_set require.user root
}

tcp_body()
{
	nat64_setup

	echo "foo" | jexec dst nc -l 1234 &

	# Sanity check & delay for nc startup
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 64:ff9b::192.0.2.2

	rcv=$(nc -w 3 -6 64:ff9b::c000:202 1234)
	if [ "${rcv}" != "foo" ];
	then
		echo "rcv=${rcv}"
		atf_fail "Failed to connect to TCP server"
	fi
}

tcp_cleanup()
{
	pft_cleanup
}

atf_test_case "udp" "cleanup"
udp_head()
{
	atf_set descr 'UDP NAT64 test'
	atf_set require.user root
}

udp_body()
{
	nat64_setup

	echo "foo" | jexec dst nc -u -l 1234 &

	# Sanity check & delay for nc startup
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 64:ff9b::192.0.2.2

	rcv=$(echo bar | nc -w 3 -6 -u 64:ff9b::c000:202 1234)
	if [ "${rcv}" != "foo" ];
	then
		echo "rcv=${rcv}"
		atf_fail "Failed to connect to UDP server"
	fi
}

udp_cleanup()
{
	pft_cleanup
}

atf_test_case "sctp" "cleanup"
sctp_head()
{
	atf_set descr 'SCTP NAT64 test'
	atf_set require.user root
}

sctp_body()
{
	nat64_setup
	if ! kldstat -q -m sctp; then
		atf_skip "This test requires SCTP"
	fi

	echo "foo" | jexec dst nc --sctp -N -l 1234 &

	# Sanity check & delay for nc startup
	atf_check -s exit:0 -o ignore \
	    ping6 -c 1 64:ff9b::192.0.2.2

	rcv=$(echo bar | nc --sctp -w 3 -6 64:ff9b::c000:202 1234)
	if [ "${rcv}" != "foo" ];
	then
		echo "rcv=${rcv}"
		atf_fail "Failed to connect to SCTP server"
	fi
}

sctp_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "icmp_echo"
	atf_add_test_case "fragmentation"
	atf_add_test_case "tcp"
	atf_add_test_case "udp"
	atf_add_test_case "sctp"
}
