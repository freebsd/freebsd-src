#-
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2019 Ahsan Barkati
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
#
# $FreeBSD$
#

. $(atf_get_srcdir)/utils.subr
. $(atf_get_srcdir)/runner.subr

v4_head()
{
	atf_set descr 'Basic forwarding test'
	atf_set require.user root
	atf_set require.progs scapy
}

v4_body()
{
	firewall=$1
	firewall_init $firewall

	epair_send=$(vnet_mkepair)
	ifconfig ${epair_send}a 192.0.2.1/24 up

	epair_recv=$(vnet_mkepair)
	ifconfig ${epair_recv}a up

	vnet_mkjail iron ${epair_send}b ${epair_recv}b
	jexec iron ifconfig ${epair_send}b 192.0.2.2/24 up
	jexec iron ifconfig ${epair_recv}b 198.51.100.2/24 up
	jexec iron sysctl net.inet.ip.forwarding=1
	jexec iron arp -s 198.51.100.3 00:01:02:03:04:05
	route add -net 198.51.100.0/24 192.0.2.2


	atf_check -s exit:0 $(atf_get_srcdir)/pft_ping.py \
		--sendif ${epair_send}a \
		--to 198.51.100.3 \
		--recvif ${epair_recv}a

	firewall_config "iron" ${firewall} \
		"pf" \
			"block in" \
		"ipfw" \
			"ipfw -q add 100 deny all from any to any in" \
		"ipf" \
			"block in all" \

	atf_check -s exit:1 $(atf_get_srcdir)/pft_ping.py \
		--sendif ${epair_send}a \
		--to 198.51.100.3 \
		--recvif ${epair_recv}a

	firewall_config "iron" ${firewall} \
		"pf" \
			"block out" \
		"ipfw" \
			"ipfw -q add 100 deny all from any to any out" \
		"ipf" \
			"block out all" \

	atf_check -s exit:1 $(atf_get_srcdir)/pft_ping.py \
		--sendif ${epair_send}a \
		--to 198.51.100.3 \
		--recv ${epair_recv}a
}

v4_cleanup()
{
	firewall=$1
	firewall_cleanup $firewall
}

setup_tests \
		v4 \
			pf \
			ipfw \
			ipf
