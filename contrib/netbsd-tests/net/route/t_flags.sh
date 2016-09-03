#	$NetBSD: t_flags.sh,v 1.11 2016/08/10 23:00:39 roy Exp $
#
# Copyright (c) 2015 The NetBSD Foundation, Inc.
# All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

netserver=\
"rump_server -lrumpdev -lrumpnet -lrumpnet_net -lrumpnet_netinet \
	-lrumpnet_shmif"
SOCK_LOCAL=unix://commsock1
SOCK_PEER=unix://commsock2
SOCK_GW=unix://commsock3
BUS=bus1
BUS2=bus2

DEBUG=false

setup_local()
{

	atf_check -s exit:0 ${netserver} ${SOCK_LOCAL}

	export RUMP_SERVER=$SOCK_LOCAL
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 create
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 linkstr ${BUS}
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 10.0.0.2/24
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 up

	$DEBUG && rump.ifconfig
	$DEBUG && rump.netstat -rn -f inet
}

setup_peer()
{

	atf_check -s exit:0 ${netserver} ${SOCK_PEER}

	export RUMP_SERVER=$SOCK_PEER
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 create
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 linkstr ${BUS}
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 10.0.0.1/24
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 up

	$DEBUG && rump.ifconfig
	$DEBUG && rump.netstat -rn -f inet
}

setup_gw()
{

	atf_check -s exit:0 ${netserver} ${SOCK_GW}

	export RUMP_SERVER=$SOCK_GW
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 create
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 linkstr ${BUS}
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 10.0.0.254/24
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 up

	atf_check -s exit:0 -o ignore rump.ifconfig shmif1 create
	atf_check -s exit:0 -o ignore rump.ifconfig shmif1 linkstr ${BUS2}
	atf_check -s exit:0 -o ignore rump.ifconfig shmif1 10.0.2.1/24
	atf_check -s exit:0 -o ignore rump.ifconfig shmif1 alias 10.0.2.2/24
	atf_check -s exit:0 -o ignore rump.ifconfig shmif1 up

	# Wait until DAD completes (10 sec at most)
	atf_check -s exit:0 -o ignore rump.ifconfig -w 10
	atf_check -s not-exit:0 -x "rump.ifconfig shmif1 |grep -q tentative"

	$DEBUG && rump.ifconfig
	$DEBUG && rump.netstat -rn -f inet
}

teardown_gw()
{

	env RUMP_SERVER=$SOCK_GW rump.halt
}

check_entry_flags()
{
	local ip=$(echo $1 |sed 's/\./\\./g')
	local flags=$2

	atf_check -s exit:0 -o match:" $flags " -e ignore -x \
	    "rump.netstat -rn -f inet | grep ^'$ip'"
}

check_entry_gw()
{
	local ip=$(echo $1 |sed 's/\./\\./g')
	local gw=$2

	atf_check -s exit:0 -o match:" $gw " -e ignore -x \
	    "rump.netstat -rn -f inet | grep ^'$ip'"
}

check_entry_fail()
{
	ip=$(echo $1 |sed 's/\./\\./g')
	flags=$2  # Not used currently

	atf_check -s not-exit:0 -e ignore -x \
	    "rump.netstat -rn -f inet | grep ^'$ip'"
}

test_lo()
{

	export RUMP_SERVER=$SOCK_LOCAL

	# Up, Host, local
	check_entry_flags 127.0.0.1 UHl
}

test_connected()
{

	export RUMP_SERVER=$SOCK_LOCAL

	# Up, Host, LLINFO, local
	check_entry_flags 10.0.0.2 UHl

	# Up, Cloning
	check_entry_flags 10.0.0/24 UC
}

test_default_gateway()
{

	export RUMP_SERVER=$SOCK_LOCAL

	atf_check -s exit:0 -o ignore rump.route add default 10.0.0.1
	$DEBUG && rump.netstat -rn -f inet

	# Up, Gateway, Static
	check_entry_flags default UGS
}

test_static()
{

	export RUMP_SERVER=$SOCK_LOCAL

	# Static route to host
	atf_check -s exit:0 -o ignore rump.route add 10.0.1.1 10.0.0.1
	$DEBUG && rump.netstat -rn -f inet

	# Up, Gateway, Host, Static
	check_entry_flags 10.0.1.1 UGHS

	# Static route to network
	atf_check -s exit:0 -o ignore rump.route add -net 10.0.2.0/24 10.0.0.1
	$DEBUG && rump.netstat -rn -f inet

	# Up, Gateway, Static
	check_entry_flags 10.0.2/24 UGS
}

test_blackhole()
{

	export RUMP_SERVER=$SOCK_LOCAL

	atf_check -s exit:0 -o ignore rump.ping -n -w 1 -c 1 10.0.0.1

	# Delete an existing route first
	atf_check -s exit:0 -o ignore rump.route delete -net 10.0.0.0/24

	# Gateway must be lo0
	atf_check -s exit:0 -o ignore \
	    rump.route add -net 10.0.0.0/24 127.0.0.1 -blackhole
	$DEBUG && rump.netstat -rn -f inet

	# Up, Gateway, Blackhole, Static
	check_entry_flags 10.0.0/24 UGBS

	atf_check -s not-exit:0 -o match:'100.0% packet loss' \
	    rump.ping -n -w 1 -c 1 10.0.0.1
	$DEBUG && rump.netstat -rn -f inet

	# Shouldn't be created
	check_entry_fail 10.0.0.1 UH
}

test_reject()
{

	export RUMP_SERVER=$SOCK_LOCAL

	# Delete an existing route first
	atf_check -s exit:0 -o ignore rump.route delete -net 10.0.0.0/24

	atf_check -s exit:0 -o ignore rump.route add -net 10.0.0.0/24 10.0.0.1 -reject
	$DEBUG && rump.netstat -rn -f inet

	# Up, Gateway, Reject, Static
	check_entry_flags 10.0.0/24 UGRS

	atf_check -s not-exit:0 -o ignore -e match:'No route to host' \
	    rump.ping -n -w 1 -c 1 10.0.0.1
	$DEBUG && rump.netstat -rn -f inet

	# Shouldn't be created
	check_entry_fail 10.0.0.1 UH

	# Gateway is lo0 (RTF_GATEWAY)

	# Delete an existing route first
	atf_check -s exit:0 -o ignore rump.route delete -net 10.0.0.0/24

	atf_check -s exit:0 -o ignore \
	    rump.route add -net 10.0.0.0/24 127.0.0.1 -reject
	$DEBUG && rump.netstat -rn -f inet

	# Up, Gateway, Reject, Static
	check_entry_flags 10.0.0/24 UGRS

	atf_check -s not-exit:0 -o ignore -e match:'Network is unreachable' \
	    rump.ping -n -w 1 -c 1 10.0.0.1
	$DEBUG && rump.netstat -rn -f inet

	# Shouldn't be created
	check_entry_fail 10.0.0.1 UH

	# Gateway is lo0 (RTF_HOST)

	# Delete an existing route first
	atf_check -s exit:0 -o ignore rump.route delete -net 10.0.0.0/24

	atf_check -s exit:0 -o ignore \
	    rump.route add -host 10.0.0.1/24 127.0.0.1 -iface -reject
	$DEBUG && rump.netstat -rn -f inet

	# Up, Host, Reject, Static
	check_entry_flags 10.0.0.1 UHRS

	atf_check -s not-exit:0 -o ignore -e match:'No route to host' \
	    rump.ping -n -w 1 -c 1 10.0.0.1
	$DEBUG && rump.netstat -rn -f inet

	return 0
}

test_icmp_redirect()
{

	### Testing Dynamic flag ###

	#
	# Setup a gateway 10.0.0.254. 10.0.2.1 is behind it.
	#
	setup_gw

	#
	# Teach the peer that 10.0.2.* is behind 10.0.0.254
	#
	export RUMP_SERVER=$SOCK_PEER
	atf_check -s exit:0 -o ignore rump.route add -net 10.0.2.0/24 10.0.0.254
	# Up, Gateway, Static
	check_entry_flags 10.0.2/24 UGS

	#
	# Setup the default gateway to the peer, 10.0.0.1
	#
	export RUMP_SERVER=$SOCK_LOCAL
	atf_check -s exit:0 -o ignore rump.route add default 10.0.0.1
	# Up, Gateway, Static
	check_entry_flags default UGS

	# Try ping 10.0.2.1
	atf_check -s exit:0 -o ignore rump.ping -n -w 1 -c 1 10.0.2.1
	$DEBUG && rump.netstat -rn -f inet

	# Up, Gateway, Host, Dynamic
	check_entry_flags 10.0.2.1 UGHD
	check_entry_gw 10.0.2.1 10.0.0.254

	export RUMP_SERVER=$SOCK_PEER
	$DEBUG && rump.netstat -rn -f inet

	### Testing Modified flag ###

	#
	# Teach a wrong route to 10.0.2.2
	#
	export RUMP_SERVER=$SOCK_LOCAL
	atf_check -s exit:0 -o ignore rump.route add 10.0.2.2 10.0.0.1
	# Up, Gateway, Host, Static
	check_entry_flags 10.0.2.2 UGHS
	check_entry_gw 10.0.2.2 10.0.0.1

	# Try ping 10.0.2.2
	atf_check -s exit:0 -o ignore rump.ping -n -w 1 -c 1 10.0.2.2
	$DEBUG && rump.netstat -rn -f inet

	# Up, Gateway, Host, Modified, Static
	check_entry_flags 10.0.2.2 UGHMS
	check_entry_gw 10.0.2.2 10.0.0.254

	teardown_gw
}

test_announce()
{
	export RUMP_SERVER=$SOCK_LOCAL

	# Delete an existing route first
	atf_check -s exit:0 -o ignore rump.route delete -net 10.0.0.0/24

	atf_check -s exit:0 -o ignore rump.route add -net 10.0.0.0/24 10.0.0.1 -proxy
	$DEBUG && rump.netstat -rn -f inet

	# Up, Gateway, Static, proxy
	check_entry_flags 10.0.0/24 UGSp

	# TODO test its behavior
}

cleanup()
{
	$DEBUG && /usr/bin/shmif_dumpbus -p - $BUS 2>/dev/null | \
	    /usr/sbin/tcpdump -n -e -r -
	env RUMP_SERVER=$SOCK_LOCAL rump.halt
	env RUMP_SERVER=$SOCK_PEER rump.halt
}

add_test()
{
	local name=$1
	local desc="$2"

	atf_test_case "route_flags_${name}" cleanup
	eval "route_flags_${name}_head() { \
			atf_set \"descr\" \"${desc}\"; \
			atf_set \"require.progs\" \"rump_server\"; \
		}; \
	    route_flags_${name}_body() { \
			setup_local; \
			setup_peer; \
			test_${name}; \
		}; \
	    route_flags_${name}_cleanup() { \
			cleanup; \
		}"
	atf_add_test_case "route_flags_${name}"
}

atf_init_test_cases()
{

	add_test lo              "Tests route flags: loop back interface"
	add_test connected       "Tests route flags: connected route"
	add_test default_gateway "Tests route flags: default gateway"
	add_test static          "Tests route flags: static route"
	add_test blackhole       "Tests route flags: blackhole route"
	add_test reject          "Tests route flags: reject route"
	add_test icmp_redirect   "Tests route flags: icmp redirect"
	add_test announce        "Tests route flags: announce flag"
}
