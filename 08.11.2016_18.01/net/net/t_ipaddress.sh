#	$NetBSD: t_ipaddress.sh,v 1.3 2016/08/10 21:33:52 kre Exp $
#
# Copyright (c) 2015 Internet Initiative Japan Inc.
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

SERVER="rump_server -lrumpnet -lrumpnet_net -lrumpnet_netinet -lrumpnet_shmif -lrumpdev"
SERVER6="$SERVER -lrumpnet_netinet6"
SOCK_LOCAL=unix://commsock1
BUS=bus

DEBUG=false

check_entry()
{
	local ip=$(echo $1 |sed 's/\./\\./g')
	local word=$2

	atf_check -s exit:0 -o match:"$word" -e ignore -x \
	    "rump.netstat -rn | grep ^'$ip'"
}

check_entry_fail()
{
	local ip=$(echo $1 |sed 's/\./\\./g')
	local flags=$2  # Not used currently

	atf_check -s not-exit:0 -e ignore -x \
	    "rump.netstat -rn | grep ^'$ip'"
}

test_same_address()
{
	local ip=10.0.0.1
	local net=10.0.0/24

	atf_check -s exit:0 ${SERVER} ${SOCK_LOCAL}
	export RUMP_SERVER=$SOCK_LOCAL

	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 create
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 linkstr ${BUS}
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 $ip/24
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 up
	atf_check -s exit:0 -o ignore rump.ifconfig -w 10

	$DEBUG && rump.netstat -nr -f inet

	check_entry $ip UHl
	check_entry $ip lo0
	check_entry $ip 'link#2'
	check_entry $net U
	check_entry $net shmif0
	check_entry $net 'link#2'

	# Delete the address
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 $ip delete

	$DEBUG && rump.netstat -nr -f inet

	check_entry_fail $ip
	check_entry_fail $net

	# Assign the same address again
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 $ip/24
	atf_check -s exit:0 -o ignore rump.ifconfig -w 10

	$DEBUG && rump.netstat -nr -f inet

	check_entry $ip UHl
	check_entry $ip lo0
	check_entry $ip 'link#2'
	check_entry $net U
	check_entry $net shmif0
	check_entry $net 'link#2'

	# Delete the address again
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 $ip delete

	$DEBUG && rump.netstat -nr -f inet

	check_entry_fail $ip
	check_entry_fail $net
}

test_same_address6()
{
	local ip=fc00::1
	local net=fc00::/64

	atf_check -s exit:0 ${SERVER6} ${SOCK_LOCAL}
	export RUMP_SERVER=$SOCK_LOCAL

	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 create
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 linkstr ${BUS}
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 inet6 $ip
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 up
	atf_check -s exit:0 -o ignore rump.ifconfig -w 10

	$DEBUG && rump.netstat -nr -f inet6

	check_entry $ip UHl
	check_entry $ip lo0
	check_entry $ip 'link#2'
	check_entry $net U
	check_entry $net shmif0
	check_entry $net 'link#2'

	# Delete the address
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 inet6 $ip delete

	$DEBUG && rump.netstat -nr -f inet6

	check_entry_fail $ip
	check_entry_fail $net

	# Assign the same address again
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 inet6 $ip
	atf_check -s exit:0 -o ignore rump.ifconfig -w 10

	$DEBUG && rump.netstat -nr -f inet6

	check_entry $ip UHl
	check_entry $ip lo0
	check_entry $ip 'link#2'
	check_entry $net U
	check_entry $net shmif0
	check_entry $net 'link#2'

	# Delete the address again
	atf_check -s exit:0 -o ignore rump.ifconfig shmif0 inet6 $ip delete

	$DEBUG && rump.netstat -nr -f inet6

	check_entry_fail $ip
	check_entry_fail $net
}

cleanup()
{

	$DEBUG && shmif_dumpbus -p - $BUS 2>/dev/null | tcpdump -n -e -r -
	env RUMP_SERVER=$SOCK_LOCAL rump.halt
}

add_test()
{
	local name=$1
	local desc="$2"

	atf_test_case "ipaddr_${name}" cleanup
	eval "ipaddr_${name}_head() { \
			atf_set \"descr\" \"${desc}\"; \
			atf_set \"require.progs\" \"rump_server\"; \
		}; \
	    ipaddr_${name}_body() { \
			test_${name}; \
		}; \
	    ipaddr_${name}_cleanup() { \
			cleanup; \
		}"
	atf_add_test_case "ipaddr_${name}"
}

atf_init_test_cases()
{

	add_test same_address	"Assigning/deleting an IP address twice"
	add_test same_address6	"Assigning/deleting an IPv6 address twice"
}
