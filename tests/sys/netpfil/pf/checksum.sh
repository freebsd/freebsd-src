# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2020 Kristof Provost <kp@FreeBSD.org>
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

atf_test_case "unaligned" "cleanup"
unaligned_head()
{
	atf_set descr 'Test unaligned checksum updates'
	atf_set require.user root
}

unaligned_body()
{
	pft_init

	epair_in=$(vnet_mkepair)
	epair_out=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair_in}b ${epair_out}a

	ifconfig ${epair_in}a 192.0.2.2/24 up
	route add -net 198.51.100.0/24 192.0.2.1

	jexec alcatraz ifconfig ${epair_in}b 192.0.2.1/24 up
	jexec alcatraz sysctl net.inet.ip.forwarding=1

	jexec alcatraz ifconfig ${epair_out}a 198.51.100.1/24 up
	jexec alcatraz arp -s 198.51.100.2 00:01:02:03:04:05

	ifconfig ${epair_out}b up

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"scrub on ${epair_in}b reassemble tcp max-mss 1200"

	# Check aligned
	atf_check -s exit:0 ${common_dir}/pft_ping.py \
		--sendif ${epair_in}a \
		--to 198.51.100.2 \
		--recvif ${epair_out}b \
		--tcpsyn

	# And unaligned
	atf_check -s exit:0 ${common_dir}/pft_ping.py \
		--sendif ${epair_in}a \
		--to 198.51.100.2 \
		--recvif ${epair_out}b \
		--tcpsyn \
		--tcpopt_unaligned
}

unaligned_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "unaligned"
}
