#!/usr/bin/env atf-sh
#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2022 KUROSAWA Takahiro <takahiro.kurosawa@gmail.com>
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

. $(atf_get_srcdir)/../common/vnet.subr

atf_test_case "pndp_add_gu_success" "cleanup"
pndp_add_gu_success_head() {
	atf_set descr 'Test proxy ndp record addition'
	atf_set require.user root
}

pndp_add_gu_success_body() {

	vnet_init

	jname="v6t-pndp_add_success"

	epair0=$(vnet_mkepair)

	vnet_mkjail ${jname} ${epair0}a
	jexec ${jname} ndp -i ${epair0}a -- -disabled
	jexec ${jname} ifconfig ${epair0}a up

	jexec ${jname} ifconfig ${epair0}a inet6 2001:db8::1/64
	proxy_mac=`jexec ${jname} ifconfig ${epair0}a ether | awk '$1~/ether/{print$2}'`

	# wait for DAD to complete
	while [ `jexec ${jname} ifconfig | grep inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done

	atf_check jexec ${jname} ndp -s 2001:db8::2 ${proxy_mac} proxy
	while [ `jexec ${jname} ifmcstat | grep -c undefined` != "0" ]; do
		sleep 0.1
	done

	# checking the output of ndp -an is covered by ndp.sh.
	# we check the output of ifmcstat output here.
	t=`jexec ${jname} ifmcstat -i ${epair0}a -f inet6 | grep -A1 'group ff02::1:ff00:2'`
	atf_check -o match:'mcast-macaddr 33:33:ff:00:00:02' echo $t
}

pndp_add_gu_success_cleanup() {
	vnet_cleanup
}

atf_test_case "pndp_del_gu_success" "cleanup"
pndp_del_gu_success_head() {
	atf_set descr 'Test proxy ndp record deletion'
	atf_set require.user root
}

pndp_del_gu_success_body() {

	vnet_init

	jname="v6t-pndp_del_gu_success"

	epair0=$(vnet_mkepair)

	vnet_mkjail ${jname} ${epair0}a

	jexec ${jname} ndp -i ${epair0}a -- -disabled
	jexec ${jname} ifconfig ${epair0}a up

	jexec ${jname} ifconfig ${epair0}a inet6 2001:db8::1/64
	proxy_mac=`jexec ${jname} ifconfig ${epair0}a ether | awk '$1~/ether/{print$2}'`

	# wait for DAD to complete
	while [ `jexec ${jname} ifconfig | grep inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done

	atf_check jexec ${jname} ndp -s 2001:db8::2 ${proxy_mac} proxy
	while [ `jexec ${jname} ifmcstat | grep -c undefined` != "0" ]; do
		sleep 0.1
	done
	jexec ${jname} ping -c1 -t1 2001:db8::2

	atf_check -o match:"2001:db8::2 \(2001:db8::2\) deleted" jexec ${jname} ndp -nd 2001:db8::2
	while [ `jexec ${jname} ifmcstat | grep -c undefined` != "0" ]; do
		sleep 0.1
	done
	atf_check \
	    -o not-match:'group ff02::1:ff00:2' \
	    -o not-match:'mcast-macaddr 33:33:ff:00:00:02' \
	    jexec ${jname} ifmcstat -i ${epair0}a -f inet6
}

pndp_del_gu_success_cleanup() {
	vnet_cleanup
}

atf_test_case "pndp_ifdestroy_success" "cleanup"
pndp_ifdetroy_success_head() {
	atf_set descr 'Test interface destruction with proxy ndp'
	atf_set require.user root
}

pndp_ifdestroy_success_body() {

	vnet_init

	jname="v6t-pndp_ifdestroy_success"

	epair0=$(vnet_mkepair)

	vnet_mkjail ${jname} ${epair0}a

	jexec ${jname} ndp -i ${epair0}a -- -disabled
	jexec ${jname} ifconfig ${epair0}a up

	jexec ${jname} ifconfig ${epair0}a inet6 2001:db8::1/64
	proxy_mac=`jexec ${jname} ifconfig ${epair0}a ether | awk '$1~/ether/{print$2}'`

	# wait for DAD to complete
	while [ `jexec ${jname} ifconfig | grep inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done

	atf_check jexec ${jname} ndp -s 2001:db8::2 ${proxy_mac} proxy
	while [ `jexec ${jname} ifmcstat | grep -c undefined` != "0" ]; do
		sleep 0.1
	done

	atf_check jexec ${jname} ifconfig ${epair0}a destroy
}

pndp_ifdestroy_success_cleanup() {
	vnet_cleanup
}

atf_test_case "pndp_neighbor_advert" "cleanup"
pndp_neighbor_advert_head() {
	atf_set descr 'Test Neighbor Advertisement for proxy ndp'
	atf_set require.user root
}

pndp_neighbor_advert_body() {

	vnet_init

	jname_a="v6t-pndp_neighbor_advert_a"	# NA sender (w/proxy ndp entry)
	jname_b="v6t-pndp_neighbor_advert_b"	# NA receiver (checker)
	proxy_addr="2001:db8::aaaa"

	epair0=$(vnet_mkepair)

	vnet_mkjail ${jname_a} ${epair0}a
	jexec ${jname_a} ndp -i ${epair0}a -- -disabled
	jexec ${jname_a} ifconfig ${epair0}a up
	jexec ${jname_a} ifconfig ${epair0}a inet6 2001:db8::1/64
	proxy_mac=`jexec ${jname_a} ifconfig ${epair0}a ether | awk '$1~/ether/{print$2}'`
	# wait for DAD to complete
	while [ `jexec ${jname_a} ifconfig | grep inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done
	atf_check jexec ${jname_a} ndp -s ${proxy_addr} ${proxy_mac} proxy
	while [ `jexec ${jname_a} ifmcstat | grep -c undefined` != "0" ]; do
		sleep 0.1
	done

	vnet_mkjail ${jname_b} ${epair0}b
	jexec ${jname_b} ndp -i ${epair0}b -- -disabled
	jexec ${jname_b} ifconfig ${epair0}b up
	jexec ${jname_b} ifconfig ${epair0}b inet6 2001:db8::2/64
	# wait for DAD to complete
	while [ `jexec ${jname_b} ifconfig | grep inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done

	jexec ${jname_b} ndp -nc
	# jname_b sends a NS before ICMPv6 Echo Request for the proxy address.
	# jname_a responds with a NA resolving the proxy address.
	# Then there must be a NDP entry of the proxy address in jname_b.
	jexec ${jname_b} ping -c1 -t1 ${proxy_addr}
	atf_check -o match:"${proxy_addr} +${proxy_mac} +${epair0}b" \
	    jexec ${jname_b} ndp -an
}

pndp_neighbor_advert_cleanup() {
	vnet_cleanup
}

atf_init_test_cases()
{

	atf_add_test_case "pndp_add_gu_success"
	atf_add_test_case "pndp_del_gu_success"
	atf_add_test_case "pndp_ifdestroy_success"
	atf_add_test_case "pndp_neighbor_advert"
}

# end

