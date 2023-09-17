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

atf_test_case "valid_redirect" "cleanup"
valid_redirect_head() {

	atf_set descr 'Test valid IPv6 redirect'
	atf_set require.user root
	atf_set require.progs scapy
}

valid_redirect_body() {

	if [ "$(atf_config_get ci false)" = "true" ]; then
		atf_skip "https://bugs.freebsd.org/247729"
	fi

	ids=65533
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

	net6="2001:db8:6667::/64"
	dst_addr6=`echo ${net6} | awk -F/ '{printf"%s4242", $1}'`
	new_rtr_ll_ip="fe80::5555"

	# remote_rtr
	remote_rtr_ll_ip="fe80::4242"
	remote_rtr_mac="00:00:5E:00:53:42"

	script_name="redirect.py"

	epair=$(vnet_mkepair)
	ifconfig ${epair}a up
	ifconfig ${epair}a inet6 ${ip6a}/64

	jname="v6t-${id}-${yl}-${xl}"
	vnet_mkjail ${jname} ${epair}b
	jexec ${jname} ifconfig ${epair}b up
	jexec ${jname} ifconfig ${epair}b inet6 ${ip6b}/64

	# Setup static entry for the remote router
	jexec ${jname} ndp -s ${remote_rtr_ll_ip}%${epair}b ${remote_rtr_mac}
	# setup prefix reachable via router
	jexec ${jname} route add -6 -net ${net6} ${remote_rtr_ll_ip}%${epair}b
	
	local_ll_ip=`jexec ${jname} ifconfig ${epair}b inet6 | awk '$1 ~ /inet6/&&$2~/^fe80/ {print$2}'|awk -F% '{print$1}'`
	local_ll_mac=`jexec ${jname} ifconfig ${epair}b ether | awk '$1~/ether/{print$2}'`

	# wait for DAD to complete
	while [ `ifconfig ${epair}a inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done
	while [ `jexec ${jname}b ifconfig ${epair}b inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done

	# enable ND debugging in the target jail to ease catching errors
	jexec ${jname} sysctl net.inet6.icmp6.nd6_debug=1

	# echo "LOCAL: ${local_ll_ip} ${local_ll_mac}"
	# echo "REMOTE: ${remote_rtr_ll_ip} ${remote_rtr_mac}"

	atf_check -s exit:0 $(atf_get_srcdir)/${script_name} \
		--smac ${remote_rtr_mac} --dmac ${local_ll_mac} \
		--sip ${remote_rtr_ll_ip} --dip ${local_ll_ip} \
		--route ${dst_addr6} --gw ${new_rtr_ll_ip}  \
		--iface ${epair}a 
	
	# Verify redirect got installed
	atf_check -o match:"destination: ${dst_addr6}\$" jexec ${jname} route -n get -6 ${dst_addr6}
	atf_check -o match:'flags: <UP,GATEWAY,HOST,DYNAMIC,DONE>' jexec ${jname} route -n get -6 ${dst_addr6}
}

valid_redirect_cleanup() {

	vnet_cleanup
}

atf_init_test_cases()
{

	atf_add_test_case "valid_redirect"
}

# end

