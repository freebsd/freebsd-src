# $FreeBSD$
#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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

atf_test_case "set_skip_group" "cleanup"
set_skip_group_head()
{
	atf_set descr 'Basic set skip test'
	atf_set require.user root
}

set_skip_group_body()
{
	# See PR 229241
	pft_init

	vnet_mkjail alcatraz
	jexec alcatraz ifconfig lo0 127.0.0.1/8 up
	jexec alcatraz ifconfig lo0 group foo
	jexec alcatraz pfctl -e
	pft_set_rules alcatraz "set skip on foo" \
		"block in proto icmp"

	jexec alcatraz ifconfig
	atf_check -s exit:0 -o ignore jexec alcatraz ping -c 1 127.0.0.1
}

set_skip_group_cleanup()
{
	pft_cleanup
}

atf_test_case "set_skip_group_lo" "cleanup"
set_skip_group_lo_head()
{
	atf_set descr 'Basic set skip test, lo'
	atf_set require.user root
}

set_skip_group_lo_body()
{
	# See PR 229241
	pft_init

	vnet_mkjail alcatraz
	jexec alcatraz ifconfig lo0 127.0.0.1/8 up
	jexec alcatraz pfctl -e
	pft_set_rules alcatraz "set skip on lo" \
		"block on lo0"

	atf_check -s exit:0 -o ignore jexec alcatraz ping -c 1 127.0.0.1
	pft_set_rules noflush alcatraz "set skip on lo" \
		"block on lo0"
	atf_check -s exit:0 -o ignore jexec alcatraz ping -c 1 127.0.0.1
	jexec alcatraz pfctl -s rules
}

set_skip_group_lo_cleanup()
{
	pft_cleanup
}

atf_test_case "set_skip_dynamic" "cleanup"
set_skip_dynamic_head()
{
	atf_set descr "Cope with group changes"
	atf_set require.user root
}

set_skip_dynamic_body()
{
	pft_init

	set -x

	vnet_mkjail alcatraz
	jexec alcatraz pfctl -e
	pft_set_rules alcatraz "set skip on epair" \
		"block"

	epair=$(vnet_mkepair)
	ifconfig ${epair}a 192.0.2.2/24 up
	ifconfig ${epair}b vnet alcatraz

	jexec alcatraz ifconfig ${epair}b 192.0.2.1/24 up

	atf_check -s exit:0 -o ignore jexec alcatraz ping -c 1 192.0.2.2
}

set_skip_dynamic_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "set_skip_group"
	atf_add_test_case "set_skip_group_lo"
	atf_add_test_case "set_skip_dynamic"
}
