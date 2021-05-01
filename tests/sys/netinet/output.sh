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

atf_test_case "output_tcp_setup_success" "cleanup"
output_tcp_setup_success_head()
{

	atf_set descr 'Test valid IPv4 TCP output'
	atf_set require.user root
}

output_tcp_setup_success_body()
{

	vnet_init

	net_src="192.0.2."
	net_dst="192.0.2."
	ip_src="${net_src}1"
	ip_dst="${net_dst}2"
	plen=24
	text="testtesttst"
	port=4242

	script_name=`dirname $0`/../common/net_receiver.py
	script_name=`realpath ${script_name}`
	jname="v4t-output_tcp_setup_success"

	epair=$(vnet_mkepair)

	vnet_mkjail ${jname}a ${epair}a
	jexec ${jname}a ifconfig ${epair}a up
	jexec ${jname}a ifconfig ${epair}a inet ${ip_src}/${plen}

	vnet_mkjail ${jname}b ${epair}b
	jexec ${jname}b ifconfig ${epair}b up
	
	jexec ${jname}b ifconfig ${epair}b inet ${ip_dst}/${plen}

	# run listener
	args="--family inet --ports ${port} --match_str ${text}"
	echo jexec ${jname}b ${script_name} ${args}
	jexec ${jname}b ${script_name} --test_name "test_listen_tcp" ${args} &
	cmd_pid=$!

	# wait for the script init
	counter=0
	while [ `jexec ${jname}b sockstat -4qlp ${port} | wc -l` != "1" ]; do
		sleep 0.01
		counter=$((counter+1))
		if [ ${counter} -ge 50 ]; then break; fi
	done
	if [ `jexec ${jname}b sockstat -4qlp ${port} | wc -l` != "1" ]; then
		echo "App setup failed"
		exit 1
	fi

	# run sender
	echo -n "${text}" | jexec ${jname}a nc -N ${ip_dst} ${port}
	exit_code=$?
	if [ $exit_code -ne 0 ]; then atf_fail "sender exit code $exit_code" ; fi

	wait ${cmd_pid}
	exit_code=$?
	if [ $exit_code -ne 0 ]; then atf_fail "receiver exit code $exit_code" ; fi
}

output_tcp_setup_success_cleanup()
{
	vnet_cleanup
}


atf_test_case "output_udp_setup_success" "cleanup"
output_udp_setup_success_head()
{

	atf_set descr 'Test valid IPv4 UDP output'
	atf_set require.user root
}

output_udp_setup_success_body()
{

	vnet_init

	net_src="192.0.2."
	net_dst="192.0.2."
	ip_src="${net_src}1"
	ip_dst="${net_dst}2"
	plen=24
	text="testtesttst"
	port=4242

	script_name=`dirname $0`/../common/net_receiver.py
	script_name=`realpath ${script_name}`
	jname="v4t-output_udp_setup_success"

	epair=$(vnet_mkepair)

	vnet_mkjail ${jname}a ${epair}a
	jexec ${jname}a ifconfig ${epair}a up
	jexec ${jname}a ifconfig ${epair}a inet ${ip_src}/${plen}

	vnet_mkjail ${jname}b ${epair}b
	jexec ${jname}b ifconfig ${epair}b up
	jexec ${jname}b ifconfig ${epair}b inet ${ip_dst}/${plen}

	# run listener
	args="--family inet --ports ${port} --match_str ${text}"
	echo jexec ${jname}b ${script_name} ${args}
	jexec ${jname}b ${script_name} --test_name "test_listen_udp" ${args} &
	cmd_pid=$!

	# wait for the script init
	counter=0
	while [ `jexec ${jname}b sockstat -4qlp ${port} | wc -l` != "1" ]; do
		sleep 0.1
		counterc=$((counter+1))
		if [ ${counter} -ge 50 ]; then break; fi
	done
	if [ `jexec ${jname}b sockstat -4qlp ${port} | wc -l` != "1" ]; then
		echo "App setup failed"
		exit 1
	fi

	# run sender
	# TODO: switch from nc to some alternative to avoid 1-second delay
	echo -n "${text}" | jexec ${jname}a nc -uNw1 ${ip_dst} ${port}
	exit_code=$?
	if [ $exit_code -ne 0 ]; then atf_fail "sender exit code $exit_code" ; fi

	wait ${cmd_pid}
	exit_code=$?
	if [ $exit_code -ne 0 ]; then atf_fail "receiver exit code $exit_code" ; fi
}

output_udp_setup_success_cleanup()
{
	vnet_cleanup
}

atf_test_case "output_raw_success" "cleanup"
output_raw_success_head()
{

	atf_set descr 'Test valid IPv4 raw output'
	atf_set require.user root
}

output_raw_success_body()
{

	vnet_init

	net_src="192.0.2."
	net_dst="192.0.2."
	ip_src="${net_src}1"
	ip_dst="${net_dst}2"
	plen=24

	script_name=`dirname $0`/../common/net_receiver.py
	script_name=`realpath ${script_name}`
	jname="v4t-output_raw_success"

	epair=$(vnet_mkepair)

	vnet_mkjail ${jname}a ${epair}a
	jexec ${jname}a ifconfig ${epair}a up
	jexec ${jname}a ifconfig ${epair}a inet ${ip_src}/${plen}

	vnet_mkjail ${jname}b ${epair}b
	jexec ${jname}b ifconfig ${epair}b up
	
	jexec ${jname}b ifconfig ${epair}b inet ${ip_dst}/${plen}

	atf_check -o match:'1 packets transmitted, 1 packets received' jexec ${jname}a ping -nc1 ${ip_dst}
}

output_raw_success_cleanup()
{
	vnet_cleanup
}

# Multipath tests are done the following way:
#            epair0
# jailA lo <        > lo jailB
#            epair1
# jailA has 2 routes towards /24 prefix on jailB loopback, via 2 epairs
# jailB has 1 route towards /24 prefix on jailA loopback, via epair0
#
# jailA initiates connections/sends packets towards IPs on jailB loopback.
# Script then compares amount of packets sent via epair0 and epair1

mpath_check()
{
	if [ `sysctl -iW net.route.multipath | wc -l` != "1" ]; then
		atf_skip "This test requires ROUTE_MPATH enabled"
	fi
}

mpath_enable()
{
	jexec $1 sysctl net.route.multipath=1
	if [ $? != 0 ]; then
		atf_fail "Setting multipath in jail $1 failed".
	fi
}

atf_test_case "output_tcp_flowid_mpath_success" "cleanup"
output_tcp_flowid_mpath_success_head()
{

	atf_set descr 'Test valid IPv4 TCP output flowid generation'
	atf_set require.user root
}

output_tcp_flowid_mpath_success_body()
{
	vnet_init
	mpath_check

	net_src="192.0.2."
	net_dst="198.51.100."
	ip_src="${net_src}1"
	ip_dst="${net_dst}1"
	plen=24
	text="testtesttst"

	script_name=`dirname $0`/../common/net_receiver.py
	script_name=`realpath ${script_name}`
	jname="v4t-output_tcp_flowid_mpath_success"

	epair0=$(vnet_mkepair)
	epair1=$(vnet_mkepair)
	lo_src=$(vnet_mkloopback)
	lo_dst=$(vnet_mkloopback)

	vnet_mkjail ${jname}a ${epair0}a ${epair1}a ${lo_src}
	mpath_enable ${jname}a
	# Setup transit IPv4 networks
	jexec ${jname}a ifconfig ${epair0}a up
	jexec ${jname}a ifconfig ${epair0}a inet 203.0.113.1/30
	jexec ${jname}a ifconfig ${epair1}a up
	jexec ${jname}a ifconfig ${epair1}a inet 203.0.113.5/30
	jexec ${jname}a ifconfig ${lo_src} up

	vnet_mkjail ${jname}b ${epair0}b ${epair1}b ${lo_dst}
	jexec ${jname}b ifconfig ${epair0}b up
	jexec ${jname}b ifconfig ${epair0}b inet 203.0.113.2/30
	jexec ${jname}b ifconfig ${epair1}b up
	jexec ${jname}b ifconfig ${epair1}b inet 203.0.113.6/30
	jexec ${jname}b ifconfig ${lo_dst} up

	# DST ips/ports to test
	ips="4 29 48 53 55 61 71 80 84 87 90 91 119 131 137 153 154 158 162 169 169 171 176 187 197 228 233 235 236 237 245 251"
	ports="53540 49743 43067 9131 16734 5150 14379 40292 20634 51302 3387 24387 9282 14275 42103 26881 42461 29520 45714 11096"

	jexec ${jname}a ifconfig ${lo_src} inet ${ip_src}/32

	jexec ${jname}b ifconfig ${lo_dst} inet ${ip_dst}/32
	for i in ${ips}; do
		jexec ${jname}b ifconfig ${lo_dst} alias ${net_dst}${i}/32
	done

	# Add routes
	# A -> towards B via epair0a 
	jexec ${jname}a route add -4 -net ${net_dst}0/${plen} 203.0.113.2
	# A -> towards B via epair1a
	jexec ${jname}a route add -4 -net ${net_dst}0/${plen} 203.0.113.6

	# B towards A via epair0b
	jexec ${jname}b route add -4 -net ${net_src}0/${plen} 203.0.113.1
	
	# Base setup verification
	atf_check -o match:'1 packets transmitted, 1 packets received' jexec ${jname}a ping -nc1 ${ip_dst}

	# run listener
	num_ports=`echo ${ports} | wc -w`
	num_ips=`echo ${ips} | wc -w`
	count_examples=$((num_ports*num_ips))
	listener_ports=`echo ${ports} | tr ' ' '\n' | sort -n | tr '\n' ',' | sed -e 's?,$??'`
	args="--family inet --ports ${listener_ports} --count ${count_examples} --match_str ${text}"
	echo jexec ${jname}b ${script_name} ${args}
	jexec ${jname}b ${script_name} --test_name "test_listen_tcp" ${args} &
	cmd_pid=$!

	# wait for the app init
	counter=0
	init=0
	while [ ${counter} -le 50 ]; do
		_ports=`jexec ${jname}b sockstat -4ql | awk "\\\$3 == ${cmd_pid} {print \\\$6}"|awk -F: "{print \\\$2}" | sort -n | tr '\n' ','`
		if [ "${_ports}" = "${listener_ports}," ]; then
			init=1
			break;
		fi
	done
	if [ ${init} -eq 0 ]; then
		jexec ${jname}b sockstat -6ql | awk "\$3 == ${cmd_pid}"
		echo "App setup failed"
		exit 1
	fi
	echo "App setup done"

	# run sender
	for _ip in ${ips}; do
		ip="${net_dst}${_ip}"
		for port in ${ports}; do
			echo -n "${text}" | jexec ${jname}a nc -nN ${ip} ${port}
			exit_code=$?
			if [ $exit_code -ne 0 ]; then atf_fail "sender exit code $exit_code" ; fi
		done
	done

	wait ${cmd_pid}
	exit_code=$?
	if [ $exit_code -ne 0 ]; then atf_fail "receiver exit code $exit_code" ; fi

	pkt_0=`jexec ${jname}a netstat -Wf link -I ${epair0}a | head | awk '$1!~/^Name/{print$8}'`
	pkt_1=`jexec ${jname}a netstat -Wf link -I ${epair1}a | head | awk '$1!~/^Name/{print$8}'`
	if [ ${pkt_0} -le 10 ]; then
		atf_fail "Balancing failure: 1: ${pkt_0} 2: ${pkt_1}"
	fi
	if [ ${pkt_1} -le 10 ]; then
		atf_fail "Balancing failure: 1: ${pkt_0} 2: ${pkt_1}"
		exit 1
	fi
	echo "TCP Balancing: 1: ${pkt_0} 2: ${pkt_1}"
}

output_tcp_flowid_mpath_success_cleanup()
{
	vnet_cleanup
}

atf_test_case "output_udp_flowid_mpath_success" "cleanup"
output_udp_flowid_mpath_success_head()
{

	atf_set descr 'Test valid IPv4 UDP output flowid generation'
	atf_set require.user root
}

output_udp_flowid_mpath_success_body()
{

	vnet_init
	mpath_check

	# Note this test will spawn around ~100 nc processes

	net_src="192.0.2."
	net_dst="198.51.100."
	ip_src="${net_src}1"
	ip_dst="${net_dst}1"
	plen=24
	text="testtesttst"

	script_name=`dirname $0`/../common/net_receiver.py
	script_name=`realpath ${script_name}`
	jname="v4t-output_tcp_flowid_mpath_success"

	epair0=$(vnet_mkepair)
	epair1=$(vnet_mkepair)
	lo_src=$(vnet_mkloopback)
	lo_dst=$(vnet_mkloopback)

	vnet_mkjail ${jname}a ${epair0}a ${epair1}a ${lo_src}
	mpath_enable ${jname}a
	# Setup transit IPv4 networks
	jexec ${jname}a ifconfig ${epair0}a up
	jexec ${jname}a ifconfig ${epair0}a inet 203.0.113.1/30
	jexec ${jname}a ifconfig ${epair1}a up
	jexec ${jname}a ifconfig ${epair1}a inet 203.0.113.5/30
	jexec ${jname}a ifconfig ${lo_src} up

	vnet_mkjail ${jname}b ${epair0}b ${epair1}b ${lo_dst}
	jexec ${jname}b ifconfig ${epair0}b up
	jexec ${jname}b ifconfig ${epair0}b inet 203.0.113.2/30
	jexec ${jname}b ifconfig ${epair1}b up
	jexec ${jname}b ifconfig ${epair1}b inet 203.0.113.6/30
	jexec ${jname}b ifconfig ${lo_dst} up

	# DST ips/ports to test
	ips="4 29 48 53 55 61 71 80 84 87 90 91 119 131 137 153 154 158 162 169 169 171 176 187 197 228 233 235 236 237 245 251"
	ports="53540 49743 43067 9131 16734 5150 14379 40292 20634 51302 3387 24387 9282 14275 42103 26881 42461 29520 45714 11096"

	jexec ${jname}a ifconfig ${lo_src} inet ${ip_src}/32

	jexec ${jname}b ifconfig ${lo_dst} inet ${ip_dst}/32
	for i in ${ips}; do
		jexec ${jname}b ifconfig ${lo_dst} alias ${net_dst}${i}/32
	done

	# Add routes
	# A -> towards B via epair0a 
	jexec ${jname}a route add -4 -net ${net_dst}0/${plen} 203.0.113.2
	# A -> towards B via epair1a
	jexec ${jname}a route add -4 -net ${net_dst}0/${plen} 203.0.113.6

	# B towards A via epair0b
	jexec ${jname}b route add -4 -net ${net_src}0/${plen} 203.0.113.1
	
	# Base setup verification
	atf_check -o match:'1 packets transmitted, 1 packets received' jexec ${jname}a ping -nc1 ${ip_dst}

	# run listener
	num_ports=`echo ${ports} | wc -w`
	num_ips=`echo ${ips} | wc -w`
	count_examples=$((num_ports*num_ips))
	listener_ports=`echo ${ports} | tr ' ' '\n' | sort -n | tr '\n' ',' | sed -e 's?,$??'`
	args="--family inet --ports ${listener_ports} --count ${count_examples} --match_str ${text}"
	echo jexec ${jname}b ${script_name} ${args}
	jexec ${jname}b ${script_name} --test_name "test_listen_udp" ${args} &
	cmd_pid=$!

	# wait for the app init
	counter=0
	init=0
	while [ ${counter} -le 50 ]; do
		_ports=`jexec ${jname}b sockstat -4ql | awk "\\\$3 == ${cmd_pid} {print \\\$6}"|awk -F: "{print \\\$2}" | sort -n | tr '\n' ','`
		if [ "${_ports}" = "${listener_ports}," ]; then
			init=1
			break;
		fi
	done
	if [ ${init} -eq 0 ]; then
		jexec ${jname}b sockstat -4ql | awk "\$3 == ${cmd_pid}"
		echo "App setup failed"
		exit 1
	fi
	echo "App setup done"

	# run sender
	for _ip in ${ips}; do
		ip="${net_dst}${_ip}"
		for port in ${ports}; do
			# XXX: switch to something that allows immediate exit
			echo -n "${text}" | jexec ${jname}a nc -nuNw1 ${ip} ${port} &
			sleep 0.01
		done
	done

	wait ${cmd_pid}
	exit_code=$?
	if [ $exit_code -ne 0 ]; then atf_fail "receiver exit code $exit_code" ; fi

	pkt_0=`jexec ${jname}a netstat -Wf link -I ${epair0}a | head | awk '$1!~/^Name/{print$8}'`
	pkt_1=`jexec ${jname}a netstat -Wf link -I ${epair1}a | head | awk '$1!~/^Name/{print$8}'`
	if [ ${pkt_0} -le 10 ]; then
		atf_fail "Balancing failure: 1: ${pkt_0} 2: ${pkt_1}"
	fi
	if [ ${pkt_1} -le 10 ]; then
		atf_fail "Balancing failure: 1: ${pkt_0} 2: ${pkt_1}"
	fi
	echo "UDP BALANCING: 1: ${pkt_0} 2: ${pkt_1}"
}

output_udp_flowid_mpath_success_cleanup()
{
	vnet_cleanup
}

atf_test_case "output_raw_flowid_mpath_success" "cleanup"
output_raw_flowid_mpath_success_head()
{

	atf_set descr 'Test valid IPv4 raw output flowid generation'
	atf_set require.user root
}

output_raw_flowid_mpath_success_body()
{

	vnet_init
	mpath_check

	net_src="192.0.2."
	net_dst="198.51.100."
	ip_src="${net_src}1"
	ip_dst="${net_dst}1"
	plen=24
	text="testtesttst"

	jname="v4t-output_tcp_flowid_mpath_success"

	epair0=$(vnet_mkepair)
	epair1=$(vnet_mkepair)
	lo_src=$(vnet_mkloopback)
	lo_dst=$(vnet_mkloopback)

	vnet_mkjail ${jname}a ${epair0}a ${epair1}a ${lo_src}
	mpath_enable ${jname}a
	# Setup transit IPv4 networks
	jexec ${jname}a ifconfig ${epair0}a up
	jexec ${jname}a ifconfig ${epair0}a inet 203.0.113.1/30
	jexec ${jname}a ifconfig ${epair1}a up
	jexec ${jname}a ifconfig ${epair1}a inet 203.0.113.5/30
	jexec ${jname}a ifconfig ${lo_src} up

	vnet_mkjail ${jname}b ${epair0}b ${epair1}b ${lo_dst}
	jexec ${jname}b ifconfig ${epair0}b up
	jexec ${jname}b ifconfig ${epair0}b inet 203.0.113.2/30
	jexec ${jname}b ifconfig ${epair1}b up
	jexec ${jname}b ifconfig ${epair1}b inet 203.0.113.6/30
	jexec ${jname}b ifconfig ${lo_dst} up

	# DST ips/ports to test
	ips="4 29 48 53 55 61 71 80 84 87 90 91 119 131 137 153 154 158 162 169 169 171 176 187 197 228 233 235 236 237 245 251"

	jexec ${jname}a ifconfig ${lo_src} inet ${ip_src}/32

	jexec ${jname}b ifconfig ${lo_dst} inet ${ip_dst}/32
	for i in ${ips}; do
		jexec ${jname}b ifconfig ${lo_dst} alias ${net_dst}${i}/32
	done

	# Add routes
	# A -> towards B via epair0a 
	jexec ${jname}a route add -4 -net ${net_dst}0/${plen} 203.0.113.2
	# A -> towards B via epair1a
	jexec ${jname}a route add -4 -net ${net_dst}0/${plen} 203.0.113.6

	# B towards A via epair0b
	jexec ${jname}b route add -4 -net ${net_src}0/${plen} 203.0.113.1
	
	# Base setup verification
	atf_check -o match:'1 packets transmitted, 1 packets received' jexec ${jname}a ping -nc1 ${ip_dst}

	# run sender
	valid_message='1 packets transmitted, 1 packets received'
	for _ip in ${ips}; do
		ip="${net_dst}${_ip}"
		atf_check -o match:"${valid_message}" jexec ${jname}a ping -nc1 ${ip}
	done

	pkt_0=`jexec ${jname}a netstat -Wf link -I ${epair0}a | head | awk '$1!~/^Name/{print$8}'`
	pkt_1=`jexec ${jname}a netstat -Wf link -I ${epair1}a | head | awk '$1!~/^Name/{print$8}'`

	jexec ${jname}a netstat -bWf link -I ${epair0}a
	jexec ${jname}a netstat -bWf link -I ${epair1}a
	if [ ${pkt_0} -le 10 ]; then
		atf_fail "Balancing failure: 1: ${pkt_0} 2: ${pkt_1}"
	fi
	if [ ${pkt_1} -le 10 ]; then
		atf_fail "Balancing failure: 1: ${pkt_0} 2: ${pkt_1}"
	fi
	echo "RAW BALANCING: 1: ${pkt_0} 2: ${pkt_1}"
}

output_raw_flowid_mpath_success_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "output_tcp_setup_success"
	atf_add_test_case "output_udp_setup_success"
	atf_add_test_case "output_raw_success"
	atf_add_test_case "output_tcp_flowid_mpath_success"
	atf_add_test_case "output_udp_flowid_mpath_success"
	atf_add_test_case "output_raw_flowid_mpath_success"
}

