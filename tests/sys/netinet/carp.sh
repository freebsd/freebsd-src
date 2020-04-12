# $FreeBSD$
#
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

. $(atf_get_srcdir)/../common/vnet.subr

is_master()
{
	jail=$1
	itf=$2

	jexec ${jail} ifconfig ${itf} | grep carp | grep MASTER
}

wait_for_carp()
{
	jail1=$1
	itf1=$2
	jail2=$3
	itf2=$4

	while [ -z "$(is_master ${jail1} ${itf1})" ] &&
	    [ -z "$(is_master ${jail2} ${itf2})" ]; do
		sleep 1
	done
}

atf_test_case "basic_v4" "cleanup"
basic_v4_head()
{
	atf_set descr 'Basic CARP test (IPv4)'
	atf_set require.user root
}

basic_v4_body()
{
	if ! kldstat -q -m carp; then
		atf_skip "This test requires carp"
	fi

	vnet_init
	bridge=$(vnet_mkbridge)
	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)

	vnet_mkjail carp_basic_v4_one ${bridge} ${epair_one}a ${epair_two}a
	vnet_mkjail carp_basic_v4_two ${epair_one}b
	vnet_mkjail carp_basic_v4_three ${epair_two}b

	jexec carp_basic_v4_one ifconfig ${bridge} 192.0.2.4/29 up
	jexec carp_basic_v4_one ifconfig ${bridge} addm ${epair_one}a \
	    addm ${epair_two}a
	jexec carp_basic_v4_one ifconfig ${epair_one}a up
	jexec carp_basic_v4_one ifconfig ${epair_two}a up

	jexec carp_basic_v4_two ifconfig ${epair_one}b 192.0.2.202/29 up
	jexec carp_basic_v4_two ifconfig ${epair_one}b add vhid 1 192.0.2.1/29

	jexec carp_basic_v4_three ifconfig ${epair_two}b 192.0.2.203/29 up
	jexec carp_basic_v4_three ifconfig ${epair_two}b add vhid 1 \
	    192.0.2.1/29

	wait_for_carp carp_basic_v4_two ${epair_one}b \
	    carp_basic_v4_three ${epair_two}b

	atf_check -s exit:0 -o ignore jexec carp_basic_v4_one \
	    ping -c 3 192.0.2.1
}

basic_v4_cleanup()
{
	vnet_cleanup
}

atf_test_case "basic_v6" "cleanup"
basic_v6_head()
{
	atf_set descr 'Basic CARP test (IPv6)'
	atf_set require.user root
}

basic_v6_body()
{
	if ! kldstat -q -m carp; then
		atf_skip "This test requires carp"
	fi

	vnet_init
	bridge=$(vnet_mkbridge)
	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)

	vnet_mkjail carp_basic_v6_one ${bridge} ${epair_one}a ${epair_two}a
	vnet_mkjail carp_basic_v6_two ${epair_one}b
	vnet_mkjail carp_basic_v6_three ${epair_two}b

	jexec carp_basic_v6_one ifconfig ${bridge} inet6 2001:db8::0:4/64 up \
	    no_dad
	jexec carp_basic_v6_one ifconfig ${bridge} addm ${epair_one}a \
	    addm ${epair_two}a
	jexec carp_basic_v6_one ifconfig ${epair_one}a up
	jexec carp_basic_v6_one ifconfig ${epair_two}a up

	jexec carp_basic_v6_two ifconfig ${epair_one}b inet6 \
	    2001:db8::1:2/64 up no_dad
	jexec carp_basic_v6_two ifconfig ${epair_one}b inet6 add vhid 1 \
	    2001:db8::0:1/64

	jexec carp_basic_v6_three ifconfig ${epair_two}b inet6 2001:db8::1:3/64 up no_dad
	jexec carp_basic_v6_three ifconfig ${epair_two}b inet6 add vhid 1 \
	    2001:db8::0:1/64

	wait_for_carp carp_basic_v6_two ${epair_one}b \
	    carp_basic_v6_three ${epair_two}b

	atf_check -s exit:0 -o ignore jexec carp_basic_v6_one \
	    ping6 -c 3 2001:db8::0:1
}

basic_v6_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "basic_v4"
	atf_add_test_case "basic_v6"
}
