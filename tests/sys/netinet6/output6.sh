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

atf_test_case "output6_tcp_setup_success" "cleanup"
output6_tcp_setup_success_head()
{

	atf_set descr 'Test valid IPv6 TCP output'
	atf_set require.user root
}

output6_tcp_setup_success_body()
{

	vnet_init

	net_src="2001:db8:0:0:1::"
	net_dst="2001:db8:0:0:1::"
	ip_src="${net_src}1"
	ip_dst=${net_dst}4242
	plen=64
	text="testtesttst"
	port=4242

	script_name=`dirname $0`/../common/net_receiver.py
	script_name=`realpath ${script_name}`
	jname="v6t-output6_tcp_setup_success"

	epair=$(vnet_mkepair)

	vnet_mkjail ${jname}a ${epair}a
	jexec ${jname}a ifconfig ${epair}a up
	jexec ${jname}a ifconfig ${epair}a inet6 ${ip_src}/${plen}

	vnet_mkjail ${jname}b ${epair}b
	jexec ${jname}b ifconfig ${epair}b up
	
	jexec ${jname}b ifconfig ${epair}b inet6 ${ip_dst}/${plen}

	# wait for DAD to complete
	while [ `jexec ${jname}b ifconfig ${epair}b inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done
	while [ `jexec ${jname}a ifconfig ${epair}a inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done

	# run listener
	args="--family inet6 --ports ${port} --match_str ${text}"
	echo jexec ${jname}b ${script_name} ${args}
	jexec ${jname}b ${script_name} --test_name "test_listen_tcp" ${args} &
	cmd_pid=$!

	# wait for the script init
	counter=0
	while [ `jexec ${jname}b sockstat -6qlp ${port} | wc -l` != "1" ]; do
		sleep 0.01
		counter=$((counter+1))
		if [ ${counter} -ge 50 ]; then break; fi
	done
	if [ `jexec ${jname}b sockstat -6qlp ${port} | wc -l` != "1" ]; then
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

output6_tcp_setup_success_cleanup()
{
	vnet_cleanup
}


atf_test_case "output6_udp_setup_success" "cleanup"
output6_udp_setup_success_head()
{

	atf_set descr 'Test valid IPv6 UDP output'
	atf_set require.user root
}

output6_udp_setup_success_body()
{

	vnet_init

	net_src="2001:db8:0:0:1::"
	net_dst="2001:db8:0:0:1::"
	ip_src="${net_src}1"
	ip_dst=${net_dst}4242
	plen=64
	text="testtesttst"
	port=4242

	script_name=`dirname $0`/../common/net_receiver.py
	script_name=`realpath ${script_name}`
	jname="v6t-output6_udp_setup_success"

	epair=$(vnet_mkepair)

	vnet_mkjail ${jname}a ${epair}a
	jexec ${jname}a ifconfig ${epair}a up
	jexec ${jname}a ifconfig ${epair}a inet6 ${ip_src}/${plen}

	vnet_mkjail ${jname}b ${epair}b
	jexec ${jname}b ifconfig ${epair}b up
	jexec ${jname}b ifconfig ${epair}b inet6 ${ip_dst}/${plen}

	# wait for DAD to complete
	while [ `jexec ${jname}b ifconfig ${epair}b inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done
	while [ `jexec ${jname}a ifconfig ${epair}a inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done

	# run listener
	args="--family inet6 --ports ${port} --match_str ${text}"
	echo jexec ${jname}b ${script_name} ${args}
	jexec ${jname}b ${script_name} --test_name "test_listen_udp" ${args} &
	cmd_pid=$!

	# wait for the script init
	counter=0
	while [ `jexec ${jname}b sockstat -6qlp ${port} | wc -l` != "1" ]; do
		sleep 0.1
		counterc=$((counter+1))
		if [ ${counter} -ge 50 ]; then break; fi
	done
	if [ `jexec ${jname}b sockstat -6qlp ${port} | wc -l` != "1" ]; then
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

output6_udp_setup_success_cleanup()
{
	vnet_cleanup
}

atf_test_case "output6_raw_success" "cleanup"
output6_raw_success_head()
{

	atf_set descr 'Test valid IPv6 raw output'
	atf_set require.user root
}

output6_raw_success_body()
{

	vnet_init

	net_src="2001:db8:0:0:1::"
	net_dst="2001:db8:0:0:1::"
	ip_src="${net_src}1"
	ip_dst=${net_dst}4242
	plen=64

	script_name=`dirname $0`/../common/net_receiver.py
	script_name=`realpath ${script_name}`
	jname="v6t-output6_raw_success"

	epair=$(vnet_mkepair)

	vnet_mkjail ${jname}a ${epair}a
	jexec ${jname}a ifconfig ${epair}a up
	jexec ${jname}a ifconfig ${epair}a inet6 ${ip_src}/${plen}

	vnet_mkjail ${jname}b ${epair}b
	jexec ${jname}b ifconfig ${epair}b up
	
	jexec ${jname}b ifconfig ${epair}b inet6 ${ip_dst}/${plen}

	# wait for DAD to complete
	while [ `jexec ${jname}b ifconfig ${epair}b inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done
	while [ `jexec ${jname}a ifconfig ${epair}a inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done

	atf_check -o match:'1 packets transmitted, 1 packets received' jexec ${jname}a ping -6 -nc1 ${ip_dst}
}

output6_raw_success_cleanup()
{
	vnet_cleanup
}

# Multipath tests are done the following way:
#               epair0/LL
# jailA lo/GU <           > lo/GU jailB
#               epair1/LL
# jailA has 2 routes towards /64 prefix on jailB loopback, via 2 epairs
# jailB has 1 route towards /64 prefix on jailA loopback, via epair0
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


atf_test_case "output6_tcp_flowid_mpath_success" "cleanup"
output6_tcp_flowid_mpath_success_head()
{

	atf_set descr 'Test valid IPv6 TCP output flowid generation'
	atf_set require.user root
}

output6_tcp_flowid_mpath_success_body()
{
	vnet_init
	mpath_check

	net_src="2001:db8:0:1"
	net_dst="2001:db8:0:2"
	ip_src="${net_src}::1"
	ip_dst="${net_dst}::1"
	plen=64
	text="testtesttst"

	script_name=`dirname $0`/../common/net_receiver.py
	script_name=`realpath ${script_name}`
	jname="v6t-output6_tcp_flowid_mpath_success"

	epair0=$(vnet_mkepair)
	epair1=$(vnet_mkepair)
	lo_src=$(vnet_mkloopback)
	lo_dst=$(vnet_mkloopback)

	vnet_mkjail ${jname}a ${epair0}a ${epair1}a ${lo_src}
	mpath_enable ${jname}a
	jls -N
	# enable link-local IPv6
	jexec ${jname}a ndp -i ${epair0}a -- -disabled
	jexec ${jname}a ifconfig ${epair0}a up
	jexec ${jname}a ndp -i ${epair1}a -- -disabled
	jexec ${jname}a ifconfig ${epair1}a up
	jexec ${jname}a ifconfig ${lo_src} up

	vnet_mkjail ${jname}b ${epair0}b ${epair1}b ${lo_dst}
	jls -N
	jexec ${jname}b ndp -i ${epair0}b -- -disabled
	jexec ${jname}b ifconfig ${epair0}b up
	jexec ${jname}b ndp -i ${epair1}b -- -disabled
	jexec ${jname}b ifconfig ${epair1}b up
	jexec ${jname}b ifconfig ${lo_dst} up

	# DST ips/ports to test
	ips="d3:c4:eb:40 2b:ff:dd:52 b1:d4:44:0e 41:2c:4d:43 66:4a:b4:be 8b:da:ac:f7 ca:d1:c4:f0 b1:31:da:d7 0c:ac:45:7a 44:9c:ce:71"
	ports="53540 49743 43067 9131 16734 5150 14379 40292 20634 51302 3387 24387 9282 14275 42103 26881 42461 29520 45714 11096"

	jexec ${jname}a ifconfig ${lo_src} inet6 ${ip_src}/128

	jexec ${jname}b ifconfig ${lo_dst} inet6 ${ip_dst}/128
	for i in ${ips}; do
		jexec ${jname}b ifconfig ${lo_dst} inet6 ${net_dst}:${i}/128
	done

	# wait for DAD to complete
	while [ `jexec ${jname}b ifconfig | grep inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done
	while [ `jexec ${jname}a ifconfig | grep inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done

	# Add routes
	# A -> towards B via epair0a LL
	ll=`jexec ${jname}b ifconfig ${epair0}b inet6 | awk '$2~/^fe80:/{print$2}' | awk -F% '{print$1}'`
	jexec ${jname}a route add -6 -net ${net_dst}::/${plen} ${ll}%${epair0}a
	# A -> towards B via epair1a LL
	ll=`jexec ${jname}b ifconfig ${epair1}b inet6 | awk '$2~/^fe80:/{print$2}' | awk -F% '{print$1}'`
	jexec ${jname}a route add -6 -net ${net_dst}::/${plen} ${ll}%${epair1}a

	# B towards A via epair0b LL
	ll=`jexec ${jname}a ifconfig ${epair1}a inet6 | awk '$2~/^fe80:/{print$2}' | awk -F% '{print$1}'`
	jexec ${jname}b route add -6 -net ${net_src}::/${plen} ${ll}%${epair1}b
	
	# Base setup verification
	atf_check -o match:'1 packets transmitted, 1 packets received' jexec ${jname}a ping -6 -c1 ${ip_dst}

	# run listener
	num_ports=`echo ${ports} | wc -w`
	num_ips=`echo ${ips} | wc -w`
	count_examples=$((num_ports*num_ips))
	listener_ports=`echo ${ports} | tr ' ' '\n' | sort -n | tr '\n' ',' | sed -e 's?,$??'`
	args="--family inet6 --ports ${listener_ports} --count ${count_examples} --match_str ${text}"
	echo jexec ${jname}b ${script_name} ${args}
	jexec ${jname}b ${script_name} --test_name "test_listen_tcp" ${args} &
	cmd_pid=$!

	# wait for the app init
	counter=0
	init=0
	while [ ${counter} -le 50 ]; do
		_ports=`jexec ${jname}b sockstat -6ql | awk "\\\$3 == ${cmd_pid} {print \\\$6}"|awk -F: "{print \\\$2}" | sort -n | tr '\n' ','`
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
		ip="${net_dst}:${_ip}"
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
	fi
	echo "TCP Balancing: 1: ${pkt_0} 2: ${pkt_1}"
}

output6_tcp_flowid_mpath_success_cleanup()
{
	vnet_cleanup
}

atf_test_case "output6_udp_flowid_mpath_success" "cleanup"
output6_udp_flowid_mpath_success_head()
{

	atf_set descr 'Test valid IPv6 UDP output flowid generation'
	atf_set require.user root
}

output6_udp_flowid_mpath_success_body()
{

	vnet_init
	mpath_check

	# Note this test will spawn around ~100 nc processes

	net_src="2001:db8:0:1"
	net_dst="2001:db8:0:2"
	ip_src="${net_src}::1"
	ip_dst="${net_dst}::1"
	plen=64
	text="testtesttst"

	script_name=`dirname $0`/../common/net_receiver.py
	script_name=`realpath ${script_name}`
	jname="v6t-output6_udp_flowid_mpath_success"

	epair0=$(vnet_mkepair)
	epair1=$(vnet_mkepair)
	lo_src=$(vnet_mkloopback)
	lo_dst=$(vnet_mkloopback)

	vnet_mkjail ${jname}a ${epair0}a ${epair1}a ${lo_src}
	mpath_enable ${jname}a
	jls -N
	# enable link-local IPv6
	jexec ${jname}a ndp -i ${epair0}a -- -disabled
	jexec ${jname}a ifconfig ${epair0}a up
	jexec ${jname}a ndp -i ${epair1}a -- -disabled
	jexec ${jname}a ifconfig ${epair1}a up
	jexec ${jname}a ifconfig ${lo_src} up

	vnet_mkjail ${jname}b ${epair0}b ${epair1}b ${lo_dst}
	jls -N
	jexec ${jname}b ndp -i ${epair0}b -- -disabled
	jexec ${jname}b ifconfig ${epair0}b up
	jexec ${jname}b ndp -i ${epair1}b -- -disabled
	jexec ${jname}b ifconfig ${epair1}b up
	jexec ${jname}b ifconfig ${lo_dst} up

	# DST ips/ports to test
	ips="d3:c4:eb:40 2b:ff:dd:52 b1:d4:44:0e 41:2c:4d:43 66:4a:b4:be 8b:da:ac:f7 ca:d1:c4:f0 b1:31:da:d7 0c:ac:45:7a 44:9c:ce:71"
	ports="53540 49743 43067 9131 16734 5150 14379 40292 20634 51302 3387 24387 9282 14275 42103 26881 42461 29520 45714 11096"

	jexec ${jname}a ifconfig ${lo_src} inet6 ${ip_src}/128

	jexec ${jname}b ifconfig ${lo_dst} inet6 ${ip_dst}/128
	for i in ${ips}; do
		jexec ${jname}b ifconfig ${lo_dst} inet6 ${net_dst}:${i}/128
	done

	
	# wait for DAD to complete
	while [ `jexec ${jname}b ifconfig | grep inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done
	while [ `jexec ${jname}a ifconfig | grep inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done

	# Add routes
	# A -> towards B via epair0a LL
	ll=`jexec ${jname}b ifconfig ${epair0}b inet6 | awk '$2~/^fe80:/{print$2}' | awk -F% '{print$1}'`
	jexec ${jname}a route add -6 -net ${net_dst}::/${plen} ${ll}%${epair0}a
	# A -> towards B via epair1a LL
	ll=`jexec ${jname}b ifconfig ${epair1}b inet6 | awk '$2~/^fe80:/{print$2}' | awk -F% '{print$1}'`
	jexec ${jname}a route add -6 -net ${net_dst}::/${plen} ${ll}%${epair1}a

	# B towards A via epair0b LL
	ll=`jexec ${jname}a ifconfig ${epair1}a inet6 | awk '$2~/^fe80:/{print$2}' | awk -F% '{print$1}'`
	jexec ${jname}b route add -6 -net ${net_src}::/${plen} ${ll}%${epair1}b
	
	# Base setup verification
	atf_check -o match:'1 packets transmitted, 1 packets received' jexec ${jname}a ping -6 -c1 ${ip_dst}

	# run listener
	num_ports=`echo ${ports} | wc -w`
	num_ips=`echo ${ips} | wc -w`
	count_examples=$((num_ports*num_ips))
	listener_ports=`echo ${ports} | tr ' ' '\n' | sort -n | tr '\n' ',' | sed -e 's?,$??'`
	args="--family inet6 --ports ${listener_ports} --count ${count_examples} --match_str ${text}"
	echo jexec ${jname}b ${script_name} ${args}
	jexec ${jname}b ${script_name} --test_name "test_listen_udp" ${args} &
	cmd_pid=$!

	# wait for the app init
	counter=0
	init=0
	while [ ${counter} -le 50 ]; do
		_ports=`jexec ${jname}b sockstat -6ql | awk "\\\$3 == ${cmd_pid} {print \\\$6}"|awk -F: "{print \\\$2}" | sort -n | tr '\n' ','`
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
		ip="${net_dst}:${_ip}"
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

output6_udp_flowid_mpath_success_cleanup()
{
	vnet_cleanup
}

atf_test_case "output6_raw_flowid_mpath_success" "cleanup"
output6_raw_flowid_mpath_success_head()
{

	atf_set descr 'Test valid IPv6 raw output flowid generation'
	atf_set require.user root
}

output6_raw_flowid_mpath_success_body()
{

	vnet_init
	mpath_check

	net_src="2001:db8:0:1"
	net_dst="2001:db8:0:2"
	ip_src="${net_src}::1"
	ip_dst="${net_dst}::1"
	plen=64
	text="testtesttst"

	jname="v6t-output6_raw_flowid_mpath_success"

	epair0=$(vnet_mkepair)
	epair1=$(vnet_mkepair)
	lo_src=$(vnet_mkloopback)
	lo_dst=$(vnet_mkloopback)

	vnet_mkjail ${jname}a ${epair0}a ${epair1}a ${lo_src}
	mpath_enable ${jname}a
	jls -N
	# enable link-local IPv6
	jexec ${jname}a ndp -i ${epair0}a -- -disabled
	jexec ${jname}a ifconfig ${epair0}a up
	jexec ${jname}a ndp -i ${epair1}a -- -disabled
	jexec ${jname}a ifconfig ${epair1}a up
	jexec ${jname}a ifconfig ${lo_src} up

	vnet_mkjail ${jname}b ${epair0}b ${epair1}b ${lo_dst}
	jls -N
	jexec ${jname}b ndp -i ${epair0}b -- -disabled
	jexec ${jname}b ifconfig ${epair0}b up
	jexec ${jname}b ndp -i ${epair1}b -- -disabled
	jexec ${jname}b ifconfig ${epair1}b up
	jexec ${jname}b ifconfig ${lo_dst} up

	# DST ips to test
	ips="9d:33:f2:7f 48:06:9a:0b cf:96:d5:78 76:da:8e:28 d4:66:38:1e ec:43:da:7c bb:f8:93:2f d3:c4:eb:40"
	ips="${ips} c7:31:0e:ae 8d:ed:35:2e c0:e0:22:31 82:1c:4e:28 c1:00:60:3e 6a:4f:3b:6c 8e:a7:e9:57 2b:ff:dd:52"
	ips="${ips} 88:44:79:5d 80:62:83:11 c8:e4:17:a6 e7:2a:45:d7 5a:92:0e:04 70:fc:6a:ee ce:24:4c:68 41:2c:4d:43"
	ips="${ips} 57:2b:5e:a7 f9:e0:69:c6 cb:b9:e6:ed 28:88:5d:fa d6:19:ac:1d dc:de:37:d8 fe:39:55:c7 b1:31:da:d7"

	jexec ${jname}a ifconfig ${lo_src} inet6 ${ip_src}/128

	jexec ${jname}b ifconfig ${lo_dst} inet6 ${ip_dst}/128
	for i in ${ips}; do
		jexec ${jname}b ifconfig ${lo_dst} inet6 ${net_dst}:${i}/128
	done

	# wait for DAD to complete
	while [ `jexec ${jname}b ifconfig | grep inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done
	while [ `jexec ${jname}a ifconfig | grep inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done

	# Add routes
	# A -> towards B via epair0a LL
	ll=`jexec ${jname}b ifconfig ${epair0}b inet6 | awk '$2~/^fe80:/{print$2}' | awk -F% '{print$1}'`
	jexec ${jname}a route add -6 -net ${net_dst}::/${plen} ${ll}%${epair0}a
	# A -> towards B via epair1a LL
	ll=`jexec ${jname}b ifconfig ${epair1}b inet6 | awk '$2~/^fe80:/{print$2}' | awk -F% '{print$1}'`
	jexec ${jname}a route add -6 -net ${net_dst}::/${plen} ${ll}%${epair1}a

	# B towards A via epair0b LL
	ll=`jexec ${jname}a ifconfig ${epair1}a inet6 | awk '$2~/^fe80:/{print$2}' | awk -F% '{print$1}'`
	jexec ${jname}b route add -6 -net ${net_src}::/${plen} ${ll}%${epair1}b
	
	# Base setup verification
	atf_check -o match:'1 packets transmitted, 1 packets received' jexec ${jname}a ping -6 -nc1 ${ip_dst}

	# run sender
	valid_message='1 packets transmitted, 1 packets received'
	for _ip in ${ips}; do
		ip="${net_dst}:${_ip}"
		atf_check -o match:"${valid_message}" jexec ${jname}a ping -6 -nc1 ${ip}
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

output6_raw_flowid_mpath_success_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "output6_tcp_setup_success"
	atf_add_test_case "output6_udp_setup_success"
	atf_add_test_case "output6_raw_success"
	atf_add_test_case "output6_tcp_flowid_mpath_success"
	atf_add_test_case "output6_udp_flowid_mpath_success"
	atf_add_test_case "output6_raw_flowid_mpath_success"
}

# end


