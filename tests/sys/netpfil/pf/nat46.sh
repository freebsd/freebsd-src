#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Rubicon Communications, LLC (Netgate)
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

nat46_setup_base()
{
	pft_init

	epair_link4=$(vnet_mkepair)
	epair_link6=$(vnet_mkepair)
	epair=$(vnet_mkepair)

	ifconfig ${epair}a 192.0.2.2/24 up
	route add default 192.0.2.1

	vnet_mkjail rtr46 ${epair}b ${epair_link6}a
	jexec rtr46 ifconfig ${epair}b 192.0.2.1/24 up
	jexec rtr46 ifconfig ${epair_link6}a inet6 2001:db8::1/64 up no_dad
	jexec rtr46 route -6 add default 2001:db8::2

	vnet_mkjail rtr64 ${epair_link6}b ${epair_link4}a
	jexec rtr64 ifconfig ${epair_link6}b inet6 2001:db8::2/64 up no_dad
	jexec rtr64 ifconfig ${epair_link4}a 198.51.100.1/24 up
	jexec rtr64 route -6 add default 2001:db8::1

	vnet_mkjail dst ${epair_link4}b
	jexec dst ifconfig ${epair_link4}b 198.51.100.2/24 up
	jexec dst route add default 198.51.100.1

	# Sanity checks
	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1
	atf_check -s exit:0 -o ignore \
	    jexec rtr46 ping6 -c 1 2001:db8::2
	atf_check -s exit:0 -o ignore \
	    jexec dst ping -c 1 198.51.100.1

	jexec rtr46 pfctl -e
	pft_set_rules rtr46 \
	    "set reassemble yes" \
	    "set state-policy if-bound" \
	    "pass in on ${epair}b inet from any to any af-to inet6 from (${epair_link6}a) to 64:ff9b::/96"

	jexec rtr64 pfctl -e
	pft_set_rules rtr64 \
	    "set reassemble yes" \
	    "set state-policy if-bound" \
	    "pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
	    "pass in on ${epair_link6}b inet6 from any to 64:ff9b::/96 af-to inet from (${epair_link4}a)"
}

atf_test_case "icmp_echo" "cleanup"
icmp_echo_head()
{
	atf_set descr 'Basic NAT46 ICMP echo test'
	atf_set require.user root
}

icmp_echo_body()
{
	nat46_setup_base

	atf_check -s exit:0 -o ignore \
	    ping -c 3 198.51.100.1

	atf_check -s exit:2 -o ignore \
	    ping -c 3 198.51.100.3
}

icmp_echo_cleanup()
{
	pft_cleanup
}

atf_test_case "tcp" "cleanup"
tcp_head()
{
	atf_set descr 'Basic NAT46 TCP test'
	atf_set require.user root
}

tcp_body()
{
	nat46_setup_base

	echo "foo" | jexec dst nc -l 1234 &
	sleep 1

	rcv=$(nc -w 3 198.51.100.2 1234)
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
	atf_set descr 'Basic NAT46 UDP test'
	atf_set require.user root
}

udp_body()
{
	nat46_setup_base

	echo "foo" | jexec dst nc -u -l 1234 &
	sleep 1

	rcv=$(echo bar | nc -w 3 -u 198.51.100.2 1234)
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
	atf_set descr 'Basic NAT46 SCTP test'
	atf_set require.user root
}

sctp_body()
{
	if ! kldstat -q -m sctp; then
		atf_skip "This test requires SCTP"
	fi

	nat46_setup_base

	echo "foo" | jexec dst nc --sctp -N -l 1234 &
	sleep 1

	rcv=$(echo bar | nc --sctp -w 3 198.51.100.2 1234)
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
	atf_add_test_case "tcp"
	atf_add_test_case "udp"
	atf_add_test_case "sctp"
}
