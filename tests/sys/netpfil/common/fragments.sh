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

fragments_head()
{
	atf_set descr 'Too many fragments test'
	atf_set require.user root
}

fragments_body()
{
	firewall=$1
	firewall_init $firewall

	epair=$(vnet_mkepair)
	ifconfig ${epair}b inet 192.0.2.1/24 up

	vnet_mkjail iron ${epair}a
	jexec iron ifconfig ${epair}a 192.0.2.2/24 up

	ifconfig ${epair}b mtu 200
	jexec iron ifconfig ${epair}a mtu 200

	firewall_config "iron" ${firewall} \
		"pf" \
			"scrub all fragment reassemble" \
		"ipfw" \
			"ipfw -q add 100 reass all from any to any in" \
		"ipf" \
			"pass in all with frags"

	jexec iron sysctl net.inet.ip.maxfragsperpacket=1024

	atf_check -s exit:0 -o ignore ping -c 1 -t 1 192.0.2.2
	atf_check -s exit:0 -o ignore ping -c 1 -s 800 192.0.2.2

	# Too many fragments should fail
	atf_check -s exit:2 -o ignore ping -c 1 -s 20000 192.0.2.2
}

fragments_cleanup()
{
	firewall=$1
	firewall_cleanup $firewall
}

setup_tests \
		"fragments" \
			"pf" \
			"ipfw" \
			"ipf"
