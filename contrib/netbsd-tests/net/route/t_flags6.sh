#	$NetBSD: t_flags6.sh,v 1.7 2016/08/10 23:00:39 roy Exp $
#
# Copyright (c) 2016 Internet Initiative Japan Inc.
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

RUMP_OPTS="-lrumpdev -lrumpnet -lrumpnet_net"
RUMP_OPTS="$RUMP_OPTS -lrumpnet_netinet -lrumpnet_netinet6"
RUMP_OPTS="$RUMP_OPTS -lrumpnet_shmif"
SOCK_LOCAL=unix://commsock1
SOCK_PEER=unix://commsock2
SOCK_GW=unix://commsock3
BUS=bus1
BUS2=bus2

IP6_LOCAL=fc00::2
IP6_PEER=fc00::1

DEBUG=false

setup_local()
{

	atf_check -s exit:0 rump_server ${RUMP_OPTS} ${SOCK_LOCAL}

	export RUMP_SERVER=$SOCK_LOCAL
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 create
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 linkstr ${BUS}
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 inet6 $IP6_LOCAL
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 up

	$DEBUG && rump.ifconfig
	$DEBUG && rump.netstat -rn -f inet6
}

setup_peer()
{

	atf_check -s exit:0 rump_server ${RUMP_OPTS} ${SOCK_PEER}

	export RUMP_SERVER=$SOCK_PEER
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 create
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 linkstr ${BUS}
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 inet6 $IP6_PEER
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 up

	$DEBUG && rump.ifconfig
	$DEBUG && rump.netstat -rn -f inet6
}

check_entry_flags()
{
	local ip=$(echo $1 |sed 's/\./\\./g')
	local flags=$2

	atf_check -s exit:0 -o match:" $flags " -e ignore -x \
	    "rump.netstat -rn -f inet6 | grep ^'$ip'"
}

check_entry_gw()
{
	local ip=$(echo $1 |sed 's/\./\\./g')
	local gw=$2

	atf_check -s exit:0 -o match:" $gw " -e ignore -x \
	    "rump.netstat -rn -f inet6 | grep ^'$ip'"
}

check_entry_fail()
{
	ip=$(echo $1 |sed 's/\./\\./g')
	flags=$2  # Not used currently

	atf_check -s not-exit:0 -e ignore -x \
	    "rump.netstat -rn -f inet6 | grep ^'$ip'"
}

test_lo6()
{

	export RUMP_SERVER=$SOCK_LOCAL

	# Up, Host, local
	check_entry_flags fe80::1 UHl

	# Up, Host
	check_entry_flags ::1 UH
}

test_connected6()
{

	export RUMP_SERVER=$SOCK_LOCAL

	# Up, Host, local
	check_entry_flags $IP6_LOCAL UHl

	# Up, Connected
	check_entry_flags fc00::/64 UC
}

test_default_gateway6()
{

	export RUMP_SERVER=$SOCK_LOCAL

	atf_check -s exit:0 -o ignore rump.route add -inet6 default $IP6_PEER
	$DEBUG && rump.netstat -rn -f inet6

	# Up, Gateway, Static
	check_entry_flags default UGS
}

test_static6()
{

	export RUMP_SERVER=$SOCK_LOCAL

	# Static route to host
	atf_check -s exit:0 -o ignore \
	    rump.route add -inet6 fc00::1:1 $IP6_PEER
	$DEBUG && rump.netstat -rn -f inet6

	# Up, Gateway, Host, Static
	check_entry_flags fc00::1:1 UGHS

	# Static route to network
	atf_check -s exit:0 -o ignore \
	    rump.route add -inet6 -net fc00::/24 $IP6_PEER
	$DEBUG && rump.netstat -rn -f inet6

	# Up, Gateway, Static
	check_entry_flags fc00::/24 UGS
}

test_blackhole6()
{

	export RUMP_SERVER=$SOCK_LOCAL

	atf_check -s exit:0 -o ignore rump.ping6 -n -X 1 -c 1 $IP6_PEER

	# Delete an existing route first
	atf_check -s exit:0 -o ignore \
	    rump.route delete -inet6 -net fc00::/64

	# Gateway must be lo0
	atf_check -s exit:0 -o ignore \
	    rump.route add -inet6 -net fc00::/64 ::1 -blackhole
	$DEBUG && rump.netstat -rn -f inet6

	# Up, Gateway, Blackhole, Static
	check_entry_flags fc00::/64 UGBS

	atf_check -s not-exit:0 -o match:'100.0% packet loss' \
	    rump.ping6 -n -X 1 -c 1 $IP6_PEER
	$DEBUG && rump.netstat -rn -f inet6

	# Shouldn't be created
	check_entry_fail $IP6_PEER UH
}

test_reject6()
{

	export RUMP_SERVER=$SOCK_LOCAL

	# Delete an existing route first
	atf_check -s exit:0 -o ignore \
	    rump.route delete -inet6 -net fc00::/64

	atf_check -s exit:0 -o ignore \
	    rump.route add -inet6 -net fc00::/64 $IP6_PEER -reject
	$DEBUG && rump.netstat -rn -f inet6

	# Up, Gateway, Reject, Static
	check_entry_flags fc00::/64 UGRS

	atf_check -s not-exit:0 -o ignore -e match:'No route to host' \
	    rump.ping6 -n -X 1 -c 1 $IP6_PEER
	$DEBUG && rump.netstat -rn -f inet6

	# Shouldn't be created
	check_entry_fail $IP6_PEER UH

	# Gateway is lo0 (RTF_GATEWAY)

	# Delete an existing route first
	atf_check -s exit:0 -o ignore \
	    rump.route delete -inet6 -net fc00::/64

	atf_check -s exit:0 -o ignore \
	    rump.route add -inet6 -net fc00::/64 ::1  -reject
	$DEBUG && rump.netstat -rn -f inet6

	# Up, Gateway, Reject, Static
	check_entry_flags fc00::/64 UGRS

	atf_check -s not-exit:0 -o ignore -e match:'Network is unreachable' \
	    rump.ping6 -n -X 1 -c 1 $IP6_PEER
	$DEBUG && rump.netstat -rn -f inet6

	# Shouldn't be created
	check_entry_fail $IP6_PEER UH

	# Gateway is lo0 (RTF_HOST)

	# Delete an existing route first
	atf_check -s exit:0 -o ignore \
	    rump.route delete -inet6 -net fc00::/64

	atf_check -s exit:0 -o ignore \
	    rump.route add -inet6 -host fc00::/64 ::1 -iface -reject
	$DEBUG && rump.netstat -rn -f inet6

	# Up, Host, Reject, Static
	check_entry_flags fc00:: UHRS

	atf_check -s not-exit:0 -o ignore -e match:'No route to host' \
	    rump.ping6 -n -X 1 -c 1 $IP6_PEER
	$DEBUG && rump.netstat -rn -f inet6

	return 0
}

test_announce6()
{
	export RUMP_SERVER=$SOCK_LOCAL

	# Delete an existing route first
	atf_check -s exit:0 -o ignore \
	    rump.route delete -inet6 -net fc00::/64

	atf_check -s exit:0 -o ignore \
	    rump.route add -inet6 -net fc00::/64 $IP6_PEER -proxy
	$DEBUG && rump.netstat -rn -f inet6

	# Up, Gateway, Static, proxy
	check_entry_flags fc00::/64 UGSp

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

	add_test lo6              "Tests route flags: loop back interface"
	add_test connected6       "Tests route flags: connected route"
	add_test default_gateway6 "Tests route flags: default gateway"
	add_test static6          "Tests route flags: static route"
	add_test blackhole6       "Tests route flags: blackhole route"
	add_test reject6          "Tests route flags: reject route"
	add_test announce6        "Tests route flags: announce flag"
}
