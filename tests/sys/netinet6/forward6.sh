#!/usr/bin/env atf-sh
#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Alexander V. Chernikov
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
#

. $(atf_get_srcdir)/../common/vnet.subr

atf_test_case "fwd_ip6_gu_icmp_iface_fast_success" "cleanup"
fwd_ip6_gu_icmp_iface_fast_success_head() {

	atf_set descr 'Test valid IPv6 global unicast fast-forwarding to interface'
	atf_set require.user root
	atf_set require.progs scapy
}

fwd_ip6_gu_icmp_iface_fast_success_body() {

	ids=65529
	id=`printf "%x" ${ids}`
	if [ $$ -gt 65535 ]; then
		xl=`printf "%x" $(($$ - 65535))`
		yl="1"
	else
		xl=`printf "%x" $$`
		yl=""
	fi

	vnet_init

	ip6a="2001:db8:6666:0000:${yl}:${id}:1:${xl}"
	ip6b="2001:db8:6666:0000:${yl}:${id}:2:${xl}"
	plen=96

	src_ip="2001:db8:6666:0000:${yl}:${id}:3:${xl}"

	script_name="../common/sender.py"

	epair=$(vnet_mkepair)
	ifconfig ${epair}a up
	ifconfig ${epair}a inet6 ${ip6a}/${plen}

	jname="v6t-${id}-${yl}-${xl}"
	vnet_mkjail ${jname} ${epair}b
	jexec ${jname} ifconfig ${epair}b up
	jexec ${jname} ifconfig ${epair}b inet6 ${ip6b}/${plen}

	jail_mac=`jexec ${jname} ifconfig ${epair}b ether | awk '$1~/ether/{print$2}'`

	our_mac=`ifconfig ${epair}a ether | awk '$1~/ether/{print$2}'`

	# wait for DAD to complete
	while [ `jexec ${jname} ifconfig ${epair}b inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done
	while [ `ifconfig ${epair}a inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done

	jexec ${jname} sysctl net.inet6.ip6.forwarding=1
	# As we're doing router-on-the-stick, turn sending IP redirects off:
	jexec ${jname} sysctl net.inet6.ip6.redirect=0

	atf_check -s exit:0 $(atf_get_srcdir)/${script_name} \
		--test_name fwd_ip6_icmp \
		--smac ${our_mac} --dmac ${jail_mac} \
		--sip ${src_ip} --dip ${ip6a} \
		--iface ${epair}a 
	
	# check counters are valid
	atf_check -o match:'1 packet forwarded' jexec ${jname} netstat -sp ip6
}

fwd_ip6_gu_icmp_iface_fast_success_cleanup() {

	vnet_cleanup
}

atf_test_case "fwd_ip6_gu_icmp_gw_gu_fast_success" "cleanup"
fwd_ip6_gu_icmp_gw_gu_fast_success_head() {

	atf_set descr 'Test valid IPv6 global unicast fast-forwarding to GU gw'
	atf_set require.user root
	atf_set require.progs scapy
}

fwd_ip6_gu_icmp_gw_gu_fast_success_body() {

	ids=65528
	id=`printf "%x" ${ids}`
	if [ $$ -gt 65535 ]; then
		xl=`printf "%x" $(($$ - 65535))`
		yl="1"
	else
		xl=`printf "%x" $$`
		yl=""
	fi

	vnet_init

	ip6a="2001:db8:6666:0000:${yl}:${id}:1:${xl}"
	ip6b="2001:db8:6666:0000:${yl}:${id}:2:${xl}"
	plen=96

	src_ip="2001:db8:6666:0000:${yl}:${id}:3:${xl}"
	dst_ip="2001:db8:6666:0000:${yl}:${id}:4:${xl}"

	script_name="../common/sender.py"

	epair=$(vnet_mkepair)
	ifconfig ${epair}a up
	ifconfig ${epair}a inet6 ${ip6a}/${plen}

	jname="v6t-${id}-${yl}-${xl}"
	vnet_mkjail ${jname} ${epair}b
	jexec ${jname} ifconfig ${epair}b up
	jexec ${jname} ifconfig ${epair}b inet6 ${ip6b}/${plen}

	jail_mac=`jexec ${jname} ifconfig ${epair}b ether | awk '$1~/ether/{print$2}'`

	our_mac=`ifconfig ${epair}a ether | awk '$1~/ether/{print$2}'`

	# wait for DAD to complete
	while [ `jexec ${jname} ifconfig ${epair}b inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done
	while [ `ifconfig ${epair}a inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done

	# Add static route back to us
	jexec ${jname} route add -6 -host ${dst_ip} ${ip6a}

	jexec ${jname} sysctl net.inet6.ip6.forwarding=1
	# As we're doing router-on-the-stick, turn sending IP redirects off:
	jexec ${jname} sysctl net.inet6.ip6.redirect=0

	atf_check -s exit:0 $(atf_get_srcdir)/${script_name} \
		--test_name fwd_ip6_icmp \
		--smac ${our_mac} --dmac ${jail_mac} \
		--sip ${src_ip} --dip ${dst_ip} \
		--iface ${epair}a 
	
	# check counters are valid
	atf_check -o match:'1 packet forwarded' jexec ${jname} netstat -sp ip6
}

fwd_ip6_gu_icmp_gw_gu_fast_success_cleanup() {

	vnet_cleanup
}

atf_test_case "fwd_ip6_gu_icmp_gw_ll_fast_success" "cleanup"
fwd_ip6_gu_icmp_gw_ll_fast_success_head() {

	atf_set descr 'Test valid IPv6 global unicast fast-forwarding to LL gw'
	atf_set require.user root
	atf_set require.progs scapy
}

fwd_ip6_gu_icmp_gw_ll_fast_success_body() {

	ids=65527
	id=`printf "%x" ${ids}`
	if [ $$ -gt 65535 ]; then
		xl=`printf "%x" $(($$ - 65535))`
		yl="1"
	else
		xl=`printf "%x" $$`
		yl=""
	fi

	vnet_init

	ip6a="2001:db8:6666:0000:${yl}:${id}:1:${xl}"
	ip6b="2001:db8:6666:0000:${yl}:${id}:2:${xl}"
	plen=96

	src_ip="2001:db8:6666:0000:${yl}:${id}:3:${xl}"
	dst_ip="2001:db8:6666:0000:${yl}:${id}:4:${xl}"

	script_name="../common/sender.py"

	epair=$(vnet_mkepair)
	ifconfig ${epair}a up
	ifconfig ${epair}a inet6 ${ip6a}/${plen}

	jname="v6t-${id}-${yl}-${xl}"
	vnet_mkjail ${jname} ${epair}b
	jexec ${jname} ifconfig ${epair}b up
	jexec ${jname} ifconfig ${epair}b inet6 ${ip6b}/${plen}

	jail_mac=`jexec ${jname} ifconfig ${epair}b ether | awk '$1~/ether/{print$2}'`

	our_mac=`ifconfig ${epair}a ether | awk '$1~/ether/{print$2}'`
	our_ll_ip=`ifconfig ${epair}a inet6 | awk '$1~/inet6/&& $2~/^fe80:/{print$2}' | awk -F% '{print$1}'`

	# wait for DAD to complete
	while [ `jexec ${jname} ifconfig ${epair}b inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done
	while [ `ifconfig ${epair}a inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done

	# Add static route back to us
	atf_check -s exit:0 -o ignore jexec ${jname} route add -6 -host ${dst_ip} ${our_ll_ip}%${epair}b

	jexec ${jname} sysctl net.inet6.ip6.forwarding=1
	# As we're doing router-on-the-stick, turn sending IP redirects off:
	jexec ${jname} sysctl net.inet6.ip6.redirect=0

	atf_check -s exit:0 $(atf_get_srcdir)/${script_name} \
		--test_name fwd_ip6_icmp \
		--smac ${our_mac} --dmac ${jail_mac} \
		--sip ${src_ip} --dip ${dst_ip} \
		--iface ${epair}a 
	
	# check counters are valid
	atf_check -o match:'1 packet forwarded' jexec ${jname} netstat -sp ip6
}

fwd_ip6_gu_icmp_gw_ll_fast_success_cleanup() {

	vnet_cleanup
}

atf_test_case "fwd_ip6_gu_icmp_iface_slow_success" "cleanup"
fwd_ip6_gu_icmp_iface_slow_success_head() {

	atf_set descr 'Test valid IPv6 global unicast fast-forwarding to interface'
	atf_set require.user root
	atf_set require.progs scapy
}

fwd_ip6_gu_icmp_iface_slow_success_body() {

	ids=65526
	id=`printf "%x" ${ids}`
	if [ $$ -gt 65535 ]; then
		xl=`printf "%x" $(($$ - 65535))`
		yl="1"
	else
		xl=`printf "%x" $$`
		yl=""
	fi

	vnet_init

	ip6a="2001:db8:6666:0000:${yl}:${id}:1:${xl}"
	ip6b="2001:db8:6666:0000:${yl}:${id}:2:${xl}"
	plen=96

	src_ip="2001:db8:6666:0000:${yl}:${id}:3:${xl}"

	script_name="../common/sender.py"

	epair=$(vnet_mkepair)
	ifconfig ${epair}a up
	ifconfig ${epair}a inet6 ${ip6a}/${plen}

	jname="v6t-${id}-${yl}-${xl}"
	vnet_mkjail ${jname} ${epair}b
	jexec ${jname} ifconfig ${epair}b up
	jexec ${jname} ifconfig ${epair}b inet6 ${ip6b}/${plen}

	jail_mac=`jexec ${jname} ifconfig ${epair}b ether | awk '$1~/ether/{print$2}'`

	our_mac=`ifconfig ${epair}a ether | awk '$1~/ether/{print$2}'`

	# wait for DAD to complete
	while [ `jexec ${jname} ifconfig ${epair}b inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done
	while [ `ifconfig ${epair}a inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done

	jexec ${jname} sysctl net.inet6.ip6.forwarding=1
	# Do not turn off route redirects to ensure slow path is on

	atf_check -s exit:0 $(atf_get_srcdir)/${script_name} \
		--test_name fwd_ip6_icmp \
		--smac ${our_mac} --dmac ${jail_mac} \
		--sip ${src_ip} --dip ${ip6a} \
		--iface ${epair}a 
	
	# check counters are valid
	atf_check -o match:'1 packet forwarded' jexec ${jname} netstat -sp ip6
}

fwd_ip6_gu_icmp_iface_slow_success_cleanup() {

	vnet_cleanup
}

atf_test_case "fwd_ip6_gu_icmp_gw_gu_slow_success" "cleanup"
fwd_ip6_gu_icmp_gw_gu_slow_success_head() {

	atf_set descr 'Test valid IPv6 global unicast fast-forwarding to GU gw'
	atf_set require.user root
	atf_set require.progs scapy
}

fwd_ip6_gu_icmp_gw_gu_slow_success_body() {

	ids=65525
	id=`printf "%x" ${ids}`
	if [ $$ -gt 65535 ]; then
		xl=`printf "%x" $(($$ - 65535))`
		yl="1"
	else
		xl=`printf "%x" $$`
		yl=""
	fi

	vnet_init

	ip6a="2001:db8:6666:0000:${yl}:${id}:1:${xl}"
	ip6b="2001:db8:6666:0000:${yl}:${id}:2:${xl}"
	plen=96

	src_ip="2001:db8:6666:0000:${yl}:${id}:3:${xl}"
	dst_ip="2001:db8:6666:0000:${yl}:${id}:4:${xl}"

	script_name="../common/sender.py"

	epair=$(vnet_mkepair)
	ifconfig ${epair}a up
	ifconfig ${epair}a inet6 ${ip6a}/${plen}

	jname="v6t-${id}-${yl}-${xl}"
	vnet_mkjail ${jname} ${epair}b
	jexec ${jname} ifconfig ${epair}b up
	jexec ${jname} ifconfig ${epair}b inet6 ${ip6b}/${plen}

	jail_mac=`jexec ${jname} ifconfig ${epair}b ether | awk '$1~/ether/{print$2}'`

	our_mac=`ifconfig ${epair}a ether | awk '$1~/ether/{print$2}'`

	# wait for DAD to complete
	while [ `jexec ${jname} ifconfig ${epair}b inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done
	while [ `ifconfig ${epair}a inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done

	# Add static route back to us
	jexec ${jname} route add -6 -host ${dst_ip} ${ip6a}

	jexec ${jname} sysctl net.inet6.ip6.forwarding=1
	# Do not turn off route redirects to ensure slow path is on

	# atf_check -s exit:0
		$(atf_get_srcdir)/${script_name} \
		--test_name fwd_ip6_icmp \
		--smac ${our_mac} --dmac ${jail_mac} \
		--sip ${src_ip} --dip ${dst_ip} \
		--iface ${epair}a 
	jexec ${jname} netstat -sp ip6
	
	# check counters are valid
	atf_check -o match:'1 packet forwarded' jexec ${jname} netstat -sp ip6
}

fwd_ip6_gu_icmp_gw_gu_slow_success_cleanup() {

	vnet_cleanup
}

atf_test_case "fwd_ip6_gu_icmp_gw_ll_slow_success" "cleanup"
fwd_ip6_gu_icmp_gw_ll_slow_success_head() {

	atf_set descr 'Test valid IPv6 global unicast fast-forwarding to LL gw'
	atf_set require.user root
	atf_set require.progs scapy
}

fwd_ip6_gu_icmp_gw_ll_slow_success_body() {

	ids=65524
	id=`printf "%x" ${ids}`
	if [ $$ -gt 65535 ]; then
		xl=`printf "%x" $(($$ - 65535))`
		yl="1"
	else
		xl=`printf "%x" $$`
		yl=""
	fi

	vnet_init

	ip6a="2001:db8:6666:0000:${yl}:${id}:1:${xl}"
	ip6b="2001:db8:6666:0000:${yl}:${id}:2:${xl}"
	plen=96

	src_ip="2001:db8:6666:0000:${yl}:${id}:3:${xl}"
	dst_ip="2001:db8:6666:0000:${yl}:${id}:4:${xl}"

	script_name="../common/sender.py"

	epair=$(vnet_mkepair)
	ifconfig ${epair}a up
	ifconfig ${epair}a inet6 ${ip6a}/${plen}

	jname="v6t-${id}-${yl}-${xl}"
	vnet_mkjail ${jname} ${epair}b
	jexec ${jname} ifconfig ${epair}b up
	jexec ${jname} ifconfig ${epair}b inet6 ${ip6b}/${plen}

	jail_mac=`jexec ${jname} ifconfig ${epair}b ether | awk '$1~/ether/{print$2}'`

	our_mac=`ifconfig ${epair}a ether | awk '$1~/ether/{print$2}'`
	our_ll_ip=`ifconfig ${epair}a inet6 | awk '$1~/inet6/&& $2~/^fe80:/{print$2}' | awk -F% '{print$1}'`

	# wait for DAD to complete
	while [ `jexec ${jname} ifconfig ${epair}b inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done
	while [ `ifconfig ${epair}a inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done

	# Add static route back to us
	atf_check -s exit:0 -o ignore jexec ${jname} route add -6 -host ${dst_ip} ${our_ll_ip}%${epair}b

	jexec ${jname} sysctl net.inet6.ip6.forwarding=1
	# Do not turn off route redirects to ensure slow path is on

	atf_check -s exit:0 $(atf_get_srcdir)/${script_name} \
		--test_name fwd_ip6_icmp \
		--smac ${our_mac} --dmac ${jail_mac} \
		--sip ${src_ip} --dip ${dst_ip} \
		--iface ${epair}a 
	
	# check counters are valid
	atf_check -o match:'1 packet forwarded' jexec ${jname} netstat -sp ip6
}

fwd_ip6_gu_icmp_gw_ll_slow_success_cleanup() {

	vnet_cleanup
}

atf_init_test_cases()
{

	atf_add_test_case "fwd_ip6_gu_icmp_iface_fast_success"
	atf_add_test_case "fwd_ip6_gu_icmp_gw_gu_fast_success"
	atf_add_test_case "fwd_ip6_gu_icmp_gw_ll_fast_success"
	atf_add_test_case "fwd_ip6_gu_icmp_iface_slow_success"
	atf_add_test_case "fwd_ip6_gu_icmp_gw_gu_slow_success"
	atf_add_test_case "fwd_ip6_gu_icmp_gw_ll_slow_success"
}

# end

