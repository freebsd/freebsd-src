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

setup_networking()
{
	jname="$1"
	lo_dst="$2"
	epair0="$3"
	epair1="$4"

	vnet_mkjail ${jname}a ${epair0}a ${epair1}a
	# Setup transit IPv4 networks
	jexec ${jname}a ifconfig ${epair0}a up
	jexec ${jname}a ifconfig ${epair0}a inet 203.0.113.1/30
	jexec ${jname}a ifconfig ${epair1}a up
	jexec ${jname}a ifconfig ${epair1}a inet 203.0.113.5/30

	vnet_mkjail ${jname}b ${epair0}b ${epair1}b ${lo_dst}
	jexec ${jname}b ifconfig ${epair0}b up
	jexec ${jname}b ifconfig ${epair0}b inet 203.0.113.2/30
	jexec ${jname}b ifconfig ${epair1}b up
	jexec ${jname}b ifconfig ${epair1}b inet 203.0.113.6/30
	jexec ${jname}b ifconfig ${lo_dst} up

}

atf_test_case "lpm_test1_success" "cleanup"
lpm_test1_success_head()
{

	atf_set descr 'Test IPv4 LPM for /30 and /31'
	atf_set require.user root
}

lpm_test1_success_body()
{

	vnet_init

	jname="v4t-lpm_test1_success"

	lo_dst=$(vnet_mkloopback)
	epair0=$(vnet_mkepair)
	epair1=$(vnet_mkepair)

	setup_networking ${jname} ${lo_dst} ${epair0} ${epair1}

	jexec ${jname}b ifconfig ${lo_dst} inet 198.51.100.0/32
	jexec ${jname}b ifconfig ${lo_dst} alias 198.51.100.2/32

	# Add routes
	# A -> towards B via epair0a 
	jexec ${jname}a route add -4 -net 198.51.100.0/30 203.0.113.2
	# A -> towards B via epair1a
	jexec ${jname}a route add -4 -net 198.51.100.0/31 203.0.113.6

	count=20
	valid_message="${count} packets transmitted, ${count} packets received"
	
	# Check that 198.51.100.0 goes via epair1
	atf_check -o match:"${valid_message}" jexec ${jname}a ping -f -nc${count} 198.51.100.0
	pkt_0=`jexec ${jname}a netstat -Wf link -I ${epair0}a | head | awk '$1!~/^Name/{print$8}'`
	pkt_1=`jexec ${jname}a netstat -Wf link -I ${epair1}a | head | awk '$1!~/^Name/{print$8}'`
	if [ ${pkt_1} -le ${count} ]; then
		echo "LPM failure: 1: ${pkt_0} 2: ${pkt_1} (should be ${count})"
		exit 1
	fi

	# Check that 198.51.100.2 goes via epair0
	atf_check -o match:"${valid_message}" jexec ${jname}a ping -f -nc${count} 198.51.100.2
	pkt_0=`jexec ${jname}a netstat -Wf link -I ${epair0}a | head | awk '$1!~/^Name/{print$8}'`
	pkt_1=`jexec ${jname}a netstat -Wf link -I ${epair1}a | head | awk '$1!~/^Name/{print$8}'`
	if [ ${pkt_0} -le ${count} ]; then
		echo "LPM failure: 1: ${pkt_0} (should be ${count})  2: ${pkt_1}"
		exit 1
	fi

	echo "RAW BALANCING: 1: ${pkt_0} 2: ${pkt_1}"
}

lpm_test1_success_cleanup()
{
	vnet_cleanup
}

atf_test_case "lpm_test2_success" "cleanup"
lpm_test2_success_head()
{

	atf_set descr 'Test IPv4 LPM for the host routes'
	atf_set require.user root
}

lpm_test2_success_body()
{

	vnet_init

	jname="v4t-lpm_test2_success"

	lo_dst=$(vnet_mkloopback)
	epair0=$(vnet_mkepair)
	epair1=$(vnet_mkepair)

	setup_networking ${jname} ${lo_dst} ${epair0} ${epair1}

	jexec ${jname}b ifconfig ${lo_dst} inet 198.51.100.0/32
	jexec ${jname}b ifconfig ${lo_dst} alias 198.51.100.1/32

	# Add routes
	# A -> towards B via epair0a 
	jexec ${jname}a route add -4 -host 198.51.100.0 203.0.113.2
	# A -> towards B via epair1a
	jexec ${jname}a route add -4 -host 198.51.100.1 203.0.113.6

	count=20
	valid_message="${count} packets transmitted, ${count} packets received"
	
	# Check that 198.51.100.0 goes via epair0
	atf_check -o match:"${valid_message}" jexec ${jname}a ping -f -nc${count} 198.51.100.0
	pkt_0=`jexec ${jname}a netstat -Wf link -I ${epair0}a | head | awk '$1!~/^Name/{print$8}'`
	pkt_1=`jexec ${jname}a netstat -Wf link -I ${epair1}a | head | awk '$1!~/^Name/{print$8}'`
	if [ ${pkt_0} -le ${count} ]; then
		echo "LPM failure: 1: ${pkt_0} (should be ${count})  2: ${pkt_1}"
		exit 1
	fi

	# Check that 198.51.100.1 goes via epair1
	atf_check -o match:"${valid_message}" jexec ${jname}a ping -f -nc${count} 198.51.100.1
	pkt_0=`jexec ${jname}a netstat -Wf link -I ${epair0}a | head | awk '$1!~/^Name/{print$8}'`
	pkt_1=`jexec ${jname}a netstat -Wf link -I ${epair1}a | head | awk '$1!~/^Name/{print$8}'`
	if [ ${pkt_1} -le ${count} ]; then
		echo "LPM failure: 1: ${pkt_0} 2: ${pkt_1} (should be ${count})"
		exit 1
	fi

	echo "RAW BALANCING: 1: ${pkt_0} 2: ${pkt_1}"
}

lpm_test2_success_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "lpm_test1_success"
	atf_add_test_case "lpm_test2_success"
}

