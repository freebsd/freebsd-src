# $FreeBSD$
#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2022 Kristof Provost <kp@FreeBSD.org>
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

atf_test_case "set_prio" "cleanup"
set_prio_head()
{
	atf_set descr 'Test setting VLAN PCP'
	atf_set require.user root
}

set_prio_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a.42 create
	ifconfig ${epair}a up
	ifconfig ${epair}a.42 192.0.2.1/24 up
	ifconfig ${epair}a.42 vlandev ${epair}a vlan 42
	echo ${epair}a.42 >> created_interfaces.lst

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b.42 create
	jexec alcatraz ifconfig ${epair}b up
	jexec alcatraz ifconfig ${epair}b.42 192.0.2.2/24 up
	jexec alcatraz ifconfig ${epair}b.42 vlandev ${epair}b vlan 42

	jexec alcatraz sysctl net.link.vlan.mtag_pcp=1

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"pass out set prio 4"

	jexec alcatraz ping 192.0.2.1 &

	atf_check -e ignore -o match:'.*vlan 42, p 4.*' tcpdump -n -i ${epair}a -e -c 4
}

set_prio_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "set_prio"
}
