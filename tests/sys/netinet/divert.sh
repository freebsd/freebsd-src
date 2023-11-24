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

load_divert_module() {
	kldstat -q -m ipdivert
	if [ $? -ne  0 ]; then
		atf_skip "ipdivert module is not loaded"
	fi
}

atf_test_case "ipdivert_ip_output_remote_success" "cleanup"
ipdivert_ip_output_remote_success_head() {

	atf_set descr 'Test diverting IPv4 packet to remote destination'
	atf_set require.user root
	atf_set require.progs scapy
}

ipdivert_ip_output_remote_success_body() {

	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "i386" ]; then
		atf_skip "https://bugs.freebsd.org/245764"
	fi

	ids=65530
	id=`printf "%x" ${ids}`
	if [ $$ -gt 65535 ]; then
		xl=`printf "%x" $(($$ - 65535))`
		yl="1"
	else
		xl=`printf "%x" $$`
		yl=""
	fi

	vnet_init
	load_divert_module

	ip4a="192.0.2.5"
	ip4b="192.0.2.6"

	script_name="../common/divert.py"

	epair=$(vnet_mkepair)
	ifconfig ${epair}a up
	ifconfig ${epair}a inet ${ip4a}/30

	jname="v4t-${id}-${yl}-${xl}"
	vnet_mkjail ${jname} ${epair}b
	jexec ${jname} ifconfig ${epair}b up
	jexec ${jname} ifconfig ${epair}b inet ${ip4b}/30

	atf_check -s exit:0 $(atf_get_srcdir)/${script_name} \
		--dip ${ip4b} --test_name ipdivert_ip_output_remote_success
	
	count=`jexec ${jname} netstat -s -p icmp  | grep 'Input histogram:' -A8 | grep -c 'echo: '`
	# Verify redirect got installed
	atf_check_equal "1" "${count}"
}

ipdivert_ip_output_remote_success_cleanup() {

	vnet_cleanup
}

atf_test_case "ipdivert_ip_input_local_success" "cleanup"
ipdivert_ip_input_local_success_head() {

	atf_set descr 'Test diverting IPv4 packet to remote destination'
	atf_set require.user root
	atf_set require.progs scapy
}

ipdivert_ip_input_local_success_body() {

	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "i386" ]; then
		atf_skip "https://bugs.freebsd.org/245764"
	fi

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
	load_divert_module

	ip4a="192.0.2.5"
	ip4b="192.0.2.6"

	script_name="../common/divert.py"

	epair=$(vnet_mkepair)
	ifconfig ${epair}a up
	ifconfig ${epair}a inet ${ip4a}/30

	jname="v4t-${id}-${yl}-${xl}"
	vnet_mkjail ${jname} ${epair}b
	jexec ${jname} ifconfig ${epair}b up
	jexec ${jname} ifconfig ${epair}b inet ${ip4b}/30

	atf_check -s exit:0 jexec ${jname} $(atf_get_srcdir)/${script_name} \
	    --sip ${ip4a} --dip ${ip4b} \
	    --test_name ipdivert_ip_input_local_success
	
	count=`jexec ${jname} netstat -s -p icmp  | grep 'Input histogram:' -A8 | grep -c 'echo: '`
	# Verify redirect got installed
	atf_check_equal "1" "${count}"
}

ipdivert_ip_input_local_success_cleanup() {

	vnet_cleanup
}

atf_init_test_cases()
{

	atf_add_test_case "ipdivert_ip_output_remote_success"
	atf_add_test_case "ipdivert_ip_input_local_success"
}

# end

