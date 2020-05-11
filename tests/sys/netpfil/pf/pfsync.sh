# $FreeBSD$
#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2018 Orange Business Services
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

atf_test_case "basic" "cleanup"
basic_head()
{
	atf_set descr 'Basic pfsync test'
	atf_set require.user root
}

basic_body()
{
	common_body
}

common_body()
{
	defer=$1
	pfsynct_init

	epair_sync=$(vnet_mkepair)
	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)

	vnet_mkjail one ${epair_one}a ${epair_sync}a
	vnet_mkjail two ${epair_two}a ${epair_sync}b

	# pfsync interface
	jexec one ifconfig ${epair_sync}a 192.0.2.1/24 up
	jexec one ifconfig ${epair_one}a 198.51.100.1/24 up
	jexec one ifconfig pfsync0 \
		syncdev ${epair_sync}a \
		maxupd 1 \
		$defer \
		up
	jexec two ifconfig ${epair_two}a 198.51.100.2/24 up
	jexec two ifconfig ${epair_sync}b 192.0.2.2/24 up
	jexec two ifconfig pfsync0 \
		syncdev ${epair_sync}b \
		maxupd 1 \
		$defer \
		up

	# Enable pf!
	jexec one pfctl -e
	pft_set_rules one \
		"set skip on ${epair_sync}a" \
		"pass keep state"
	jexec two pfctl -e
	pft_set_rules two \
		"set skip on ${epair_sync}b" \
		"pass keep state"

	ifconfig ${epair_one}b 198.51.100.254/24 up

	ping -c 1 -S 198.51.100.254 198.51.100.1

	# Give pfsync time to do its thing
	sleep 2

	if ! jexec two pfctl -s states | grep icmp | grep 198.51.100.1 | \
	    grep 198.51.100.2 ; then
		atf_fail "state not found on synced host"
	fi
}

basic_cleanup()
{
	pfsynct_cleanup
}

atf_test_case "defer" "cleanup"
defer_head()
{
	atf_set descr 'Defer mode pfsync test'
	atf_set require.user root
}

defer_body()
{
	common_body defer
}

defer_cleanup()
{
	pfsynct_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "basic"
	atf_add_test_case "defer"
}
