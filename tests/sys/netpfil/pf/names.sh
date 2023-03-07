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

atf_test_case "names" "cleanup"
names_head()
{
	atf_set descr 'Test overlapping names'
	atf_set require.user root
}

names_body()
{
	pft_init

	epair=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair}b
	ifconfig ${epair}a name foo
	jexec alcatraz ifconfig ${epair}b name foo

	jail -r alcatraz
	ifconfig foo destroy
}

names_cleanup()
{
	pft_cleanup
}

atf_test_case "group" "cleanup"
group_head()
{
	atf_set descr 'Test group cleanup, PR257218'
	atf_set require.user root
}

group_body()
{
	pft_init

	vnet_mkjail alcatraz

	if [ -n "$(jexec alcatraz pfctl -sI | grep '^epair$')" ];
	then
		atf_fail "Unexpected epair group"
	fi

	epair=$(vnet_mkepair)
	if [ -n "$(jexec alcatraz pfctl -sI | grep '^epair$')" ];
	then
		atf_fail "Unexpected epair group"
	fi

	ifconfig ${epair}b vnet alcatraz
	if [ -z "$(jexec alcatraz pfctl -sI | grep '^epair$')" ];
	then
		atf_fail "Failed to find epair group"
	fi

	ifconfig ${epair}a destroy

	if [ -n "$(jexec alcatraz pfctl -sI | grep '^epair$')" ];
	then
		atf_fail "Unexpected epair group"
	fi
}

group_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "names"
	atf_add_test_case "group"
}
