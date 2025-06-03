#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Kristof Provost <kp@FreeBSD.org>
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

atf_test_case "cve_2019_5598" "cleanup"
cve_2019_5598_head()
{
	atf_set descr 'Test CVE-2019-5598'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

cve_2019_5598_body()
{
	pft_init

	epair_in=$(vnet_mkepair)
	epair_out=$(vnet_mkepair)
	ifconfig ${epair_in}a 192.0.2.1/24 up
	ifconfig ${epair_out}a up

	vnet_mkjail alcatraz ${epair_in}b ${epair_out}b
	jexec alcatraz ifconfig ${epair_in}b 192.0.2.2/24 up
	jexec alcatraz ifconfig ${epair_out}b 198.51.100.2/24 up
	jexec alcatraz sysctl net.inet.ip.forwarding=1
	jexec alcatraz arp -s 198.51.100.3 00:01:02:03:04:05
	jexec alcatraz route add default 198.51.100.3
	route add -net 198.51.100.0/24 192.0.2.2

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz "block all" \
		"pass in proto udp to 198.51.100.3 port 53" \
		"pass out proto udp to 198.51.100.3 port 53"

	atf_check -s exit:0 env PYTHONPATH=${common_dir} \
		$(atf_get_srcdir)/CVE-2019-5598.py \
		--sendif ${epair_in}a \
		--recvif ${epair_out}a \
		--src 192.0.2.1 \
		--to 198.51.100.3
}

cve_2019_5598_cleanup()
{
	pft_cleanup
}

atf_test_case "ttl_exceeded" "cleanup"
ttl_exceeded_head()
{
	atf_set descr 'Test that we correctly translate TTL exceeded back'
	atf_set require.user root
}

ttl_exceeded_body()
{
	pft_init

	epair_srv=$(vnet_mkepair)
	epair_int=$(vnet_mkepair)
	epair_cl=$(vnet_mkepair)

	vnet_mkjail srv ${epair_srv}a
	jexec srv ifconfig ${epair_srv}a 192.0.2.1/24 up
	jexec srv route add default 192.0.2.2

	vnet_mkjail int ${epair_srv}b ${epair_int}a
	jexec int sysctl net.inet.ip.forwarding=1
	jexec int ifconfig ${epair_srv}b 192.0.2.2/24 up
	jexec int ifconfig ${epair_int}a 203.0.113.2/24 up

	vnet_mkjail nat ${epair_int}b ${epair_cl}b
	jexec nat ifconfig ${epair_int}b 203.0.113.1/24 up
	jexec nat ifconfig ${epair_cl}b 198.51.100.2/24 up
	jexec nat sysctl net.inet.ip.forwarding=1
	jexec nat route add default 203.0.113.2

	vnet_mkjail cl ${epair_cl}a
	jexec cl ifconfig ${epair_cl}a 198.51.100.1/24 up
	jexec cl route add default 198.51.100.2

	jexec nat pfctl -e
	pft_set_rules nat \
	    "nat on ${epair_int}b from 198.51.100.0/24 -> (${epair_int}b)" \
	    "block" \
	    "pass inet proto udp" \
	    "pass inet proto icmp icmp-type { echoreq }"

	# Sanity checks
	atf_check -s exit:0 -o ignore \
	    jexec cl ping -c 1 198.51.100.2
	atf_check -s exit:0 -o ignore \
	    jexec cl ping -c 1 203.0.113.1
	atf_check -s exit:0 -o ignore \
	    jexec cl ping -c 1 203.0.113.2
	atf_check -s exit:0 -o ignore \
	    jexec cl ping -c 1 192.0.2.1

	echo "UDP"
	atf_check -s exit:0 -e ignore -o match:".*203.0.113.2.*" \
	    jexec cl traceroute 192.0.2.1
	jexec nat pfctl -Fs

	echo "ICMP"
	atf_check -s exit:0 -e ignore -o match:".*203.0.113.2.*" \
	    jexec cl traceroute -I 192.0.2.1
}

ttl_exceeded_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "cve_2019_5598"
	atf_add_test_case "ttl_exceeded"
}
