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

	atf_set descr 'Test valid IPv4 redirect'
	atf_set require.user root
	atf_set require.progs scapy
}

valid_redirect_body() {

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

	ip4a="192.0.2.1"
	ip4b="192.0.2.2"

	net4="198.51.100.0/24"
	dst_addr4="198.51.100.42"

	# remote_rtr
	remote_rtr_ip="192.0.2.3"
	remote_rtr_mac="00:00:5E:00:53:42"

	new_rtr_ip="192.0.2.4"

	script_name="redirect.py"

	epair=$(vnet_mkepair)
	ifconfig ${epair}a up
	ifconfig ${epair}a inet ${ip4a}/24

	jname="v4t-${id}-${yl}-${xl}"
	vnet_mkjail ${jname} ${epair}b
	jexec ${jname} ifconfig ${epair}b up
	jexec ${jname} ifconfig ${epair}b inet ${ip4b}/24

	# Setup static entry for the remote router
	jexec ${jname} arp -s ${remote_rtr_ip} ${remote_rtr_mac}
	# setup prefix reachable via router
	jexec ${jname} route add -4 -net ${net4} ${remote_rtr_ip}
	
	local_ip=${ip4b}
	local_mac=`jexec ${jname} ifconfig ${epair}b ether | awk '$1~/ether/{print$2}'`

	# echo "LOCAL: ${local_ip} ${local_mac}"
	# echo "REMOTE: ${remote_rtr_ip} ${remote_rtr_mac}"

	atf_check -s exit:0 $(atf_get_srcdir)/${script_name} \
		--smac ${remote_rtr_mac} --dmac ${local_mac} \
		--sip ${remote_rtr_ip} --dip ${local_ip} \
		--route ${dst_addr4} --gw ${new_rtr_ip}  \
		--iface ${epair}a 
	
	atf_check -o match:"destination: ${dst_addr4}\$" jexec ${jname} route -n get -4 ${dst_addr4}
	atf_check -o match:'flags: <UP,GATEWAY,HOST,DYNAMIC,DONE>' jexec ${jname} route -n get -4 ${dst_addr4}
}

valid_redirect_cleanup() {

	vnet_cleanup
}

atf_init_test_cases()
{

	atf_add_test_case "valid_redirect"
}

# end

