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
	atf_set require.progs scapy
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

atf_init_test_cases()
{
	atf_add_test_case "cve_2019_5598"
}
