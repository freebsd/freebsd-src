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
# $FreeBSD$
#

. $(atf_get_srcdir)/../common/vnet.subr

setup_networking()
{
	jname="$1"
	lo_dst="$2"
	epair0="$3"
	epair1="$4"

	vnet_mkjail ${jname}a ${epair0}a ${epair1}a ${lo_src}
	# enable link-local IPv6
	jexec ${jname}a ndp -i ${epair0}a -- -disabled
	jexec ${jname}a ifconfig ${epair0}a up
	jexec ${jname}a ndp -i ${epair1}a -- -disabled
	jexec ${jname}a ifconfig ${epair1}a up
	jexec ${jname}a ifconfig ${lo_src} up

	vnet_mkjail ${jname}b ${epair0}b ${epair1}b ${lo_dst}
	jexec ${jname}b ndp -i ${epair0}b -- -disabled
	jexec ${jname}b ifconfig ${epair0}b up
	jexec ${jname}b ndp -i ${epair1}b -- -disabled
	jexec ${jname}b ifconfig ${epair1}b up
	jexec ${jname}b ifconfig ${lo_dst} up

	# wait for DAD to complete
	while [ `jexec ${jname}b ifconfig | grep inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done
	while [ `jexec ${jname}a ifconfig | grep inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done
}


atf_test_case "lpm6_test1_success" "cleanup"
lpm6_test1_success_head()
{

	atf_set descr 'Test IPv6 LPM for the host routes'
	atf_set require.user root
}

lpm6_test1_success_body()
{

	vnet_init

	net_dst="2001:db8:"

	jname="v6t-lpm6_test1_success"

	epair0=$(vnet_mkepair)
	epair1=$(vnet_mkepair)
	lo_dst=$(vnet_mkloopback)

	setup_networking ${jname} ${lo_dst} ${epair0} ${epair1}

	jexec ${jname}b ifconfig ${lo_dst} inet6 ${net_dst}:2:0/128
	jexec ${jname}b ifconfig ${lo_dst} inet6 ${net_dst}:2:1/128

	# Add routes
	# A -> towards B via epair0a LL
	ll=`jexec ${jname}b ifconfig ${epair0}b inet6 | awk '$2~/^fe80:/{print$2}' | awk -F% '{print$1}'`
	jexec ${jname}a route add -6 -host ${net_dst}:2:0  ${ll}%${epair0}a
	# A -> towards B via epair1a LL
	ll=`jexec ${jname}b ifconfig ${epair1}b inet6 | awk '$2~/^fe80:/{print$2}' | awk -F% '{print$1}'`
	jexec ${jname}a route add -6 -host ${net_dst}:2:1 ${ll}%${epair1}a

	count=20
	valid_message="${count} packets transmitted, ${count} packets received"
	
	# Check that ${net_dst}:2:0 goes via epair0
	atf_check -o match:"${valid_message}" jexec ${jname}a ping -6 -f -nc${count} ${net_dst}:2:0
	pkt_0=`jexec ${jname}a netstat -Wf link -I ${epair0}a | head | awk '$1!~/^Name/{print$8}'`
	pkt_1=`jexec ${jname}a netstat -Wf link -I ${epair1}a | head | awk '$1!~/^Name/{print$8}'`
	if [ ${pkt_0} -le ${count} ]; then
		echo "LPM failure: 1: ${pkt_0} (should be ${count})  2: ${pkt_1}"
		exit 1
	fi

	# Check that ${net_dst}:2:1 goes via epair1
	atf_check -o match:"${valid_message}" jexec ${jname}a ping -6 -f -nc${count} ${net_dst}:2:1
	pkt_0=`jexec ${jname}a netstat -Wf link -I ${epair0}a | head | awk '$1!~/^Name/{print$8}'`
	pkt_1=`jexec ${jname}a netstat -Wf link -I ${epair1}a | head | awk '$1!~/^Name/{print$8}'`
	if [ ${pkt_1} -le ${count} ]; then
		echo "LPM failure: 1: ${pkt_0} 2: ${pkt_1} (should be ${count})"
		exit 1
	fi

	echo "RAW BALANCING: 1: ${pkt_0} 2: ${pkt_1}"
}

lpm6_test1_success_cleanup()
{
	vnet_cleanup
}

atf_test_case "lpm6_test2_success" "cleanup"
lpm6_test2_success_head()
{

	atf_set descr 'Test IPv6 LPM for /126 and /127'
	atf_set require.user root
}

lpm6_test2_success_body()
{

	vnet_init

	net_dst="2001:db8:"

	jname="v6t-lpm6_test2_success"

	epair0=$(vnet_mkepair)
	epair1=$(vnet_mkepair)
	lo_dst=$(vnet_mkloopback)

	setup_networking ${jname} ${lo_dst} ${epair0} ${epair1}

	jexec ${jname}b ifconfig ${lo_dst} inet6 ${net_dst}:2:0/128
	jexec ${jname}b ifconfig ${lo_dst} inet6 ${net_dst}:2:2/128

	# Add routes
	# A -> towards B via epair0a LL
	ll=`jexec ${jname}b ifconfig ${epair0}b inet6 | awk '$2~/^fe80:/{print$2}' | awk -F% '{print$1}'`
	jexec ${jname}a route add -6 -net ${net_dst}:2:0/126  ${ll}%${epair0}a
	# A -> towards B via epair1a LL
	ll=`jexec ${jname}b ifconfig ${epair1}b inet6 | awk '$2~/^fe80:/{print$2}' | awk -F% '{print$1}'`
	jexec ${jname}a route add -6 -net ${net_dst}:2:0/127 ${ll}%${epair1}a

	count=20
	valid_message="${count} packets transmitted, ${count} packets received"
	
	# Check that ${net_dst}:2:0 goes via epair1
	atf_check -o match:"${valid_message}" jexec ${jname}a ping -6 -f -nc${count} ${net_dst}:2:0
	pkt_0=`jexec ${jname}a netstat -Wf link -I ${epair0}a | head | awk '$1!~/^Name/{print$8}'`
	pkt_1=`jexec ${jname}a netstat -Wf link -I ${epair1}a | head | awk '$1!~/^Name/{print$8}'`
	if [ ${pkt_1} -le ${count} ]; then
		echo "LPM failure: 1: ${pkt_0} 2: ${pkt_1} (should be ${count})"
		exit 1
	fi

	# Check that ${net_dst}:2:2 goes via epair0
	atf_check -o match:"${valid_message}" jexec ${jname}a ping -6 -f -nc${count} ${net_dst}:2:2
	pkt_0=`jexec ${jname}a netstat -Wf link -I ${epair0}a | head | awk '$1!~/^Name/{print$8}'`
	pkt_1=`jexec ${jname}a netstat -Wf link -I ${epair1}a | head | awk '$1!~/^Name/{print$8}'`
	if [ ${pkt_0} -le ${count} ]; then
		echo "LPM failure: 1: ${pkt_0} (should be ${count})  2: ${pkt_1}"
		exit 1
	fi

	echo "RAW BALANCING: 1: ${pkt_0} 2: ${pkt_1}"
}

lpm6_test2_success_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "lpm6_test1_success"
	atf_add_test_case "lpm6_test2_success"
}

# end


