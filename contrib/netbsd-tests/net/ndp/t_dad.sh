#	$NetBSD: t_dad.sh,v 1.5 2016/08/10 23:07:57 kre Exp $
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

inetserver="rump_server -lrumpnet -lrumpnet_net -lrumpnet_netinet"
inetserver="$inetserver -lrumpnet_netinet6 -lrumpnet_shmif"
inetserver="$inetserver -lrumpdev"
HIJACKING="env LD_PRELOAD=/usr/lib/librumphijack.so RUMPHIJACK=sysctl=yes"

SOCKLOCAL=unix://commsock1
SOCKPEER=unix://commsock2

DEBUG=false

atf_test_case dad_basic cleanup
atf_test_case dad_duplicated cleanup

dad_basic_head()
{
	atf_set "descr" "Tests for IPv6 DAD basic behavior"
	atf_set "require.progs" "rump_server"
}

dad_duplicated_head()
{
	atf_set "descr" "Tests for IPv6 DAD duplicated state"
	atf_set "require.progs" "rump_server"
}

setup_server()
{
	local sock=$1
	local ip=$2

	export RUMP_SERVER=$sock

	atf_check -s exit:0 rump.ifconfig shmif0 create
	atf_check -s exit:0 rump.ifconfig shmif0 linkstr bus1
	atf_check -s exit:0 rump.ifconfig shmif0 inet6 $ip
	atf_check -s exit:0 rump.ifconfig shmif0 up
	atf_check -s exit:0 rump.ifconfig -w 10

	$DEBUG && rump.ifconfig shmif0
}

make_ns_pkt_str()
{
	local id=$1
	local target=$2
	pkt="33:33:ff:00:00:0${id}, ethertype IPv6 (0x86dd), length 78: ::"
	pkt="$pkt > ff02::1:ff00:${id}: ICMP6, neighbor solicitation,"
	pkt="$pkt who has $target, length 24"
	echo $pkt
}

extract_new_packets()
{
	local old=./old

	if [ ! -f $old ]; then
		old=/dev/null
	fi

	shmif_dumpbus -p - bus1 2>/dev/null| \
	    tcpdump -n -e -r - 2>/dev/null > ./new
	diff -u $old ./new |grep '^+' |cut -d '+' -f 2 > ./diff
	mv -f ./new ./old
	cat ./diff
}

dad_basic_body()
{
	local pkt=
	local localip1=fc00::1
	local localip2=fc00::2
	local localip3=fc00::3

	atf_check -s exit:0 ${inetserver} $SOCKLOCAL
	export RUMP_SERVER=$SOCKLOCAL

	atf_check -s exit:0 rump.ifconfig shmif0 create
	atf_check -s exit:0 rump.ifconfig shmif0 linkstr bus1
	atf_check -s exit:0 rump.ifconfig shmif0 inet6 $localip1
	atf_check -s exit:0 rump.ifconfig shmif0 inet6 $localip2
	$DEBUG && rump.ifconfig shmif0

	atf_check -s exit:0 rump.ifconfig shmif0 up
	rump.ifconfig shmif0 > ./out
	$DEBUG && cat ./out

	# The primary address doesn't start with tentative state
	atf_check -s not-exit:0 -x "cat ./out |grep $localip1 |grep -q tentative"
	# The alias address starts with tentative state
	# XXX we have no stable way to check this, so skip for now
	#atf_check -s exit:0 -x "cat ./out |grep $localip2 |grep -q tentative"

	atf_check -s exit:0 sleep 2
	extract_new_packets > ./out
	$DEBUG && cat ./out

	# Check DAD probe packets (Neighbor Solicitation Message)
	pkt=$(make_ns_pkt_str 2 $localip2)
	atf_check -s exit:0 -x "cat ./out |grep -q '$pkt'"
	# No DAD for the primary address
	pkt=$(make_ns_pkt_str 1 $localip1)
	atf_check -s not-exit:0 -x "cat ./out |grep -q '$pkt'"

	# Waiting for DAD complete
	atf_check -s exit:0 rump.ifconfig -w 10
	extract_new_packets > ./out
	$DEBUG && cat ./out

	# IPv6 DAD doesn't announce (Neighbor Advertisement Message)

	# The alias address left tentative
	atf_check -s not-exit:0 -x "rump.ifconfig shmif0 |grep $localip2 |grep -q tentative"

	#
	# Add a new address on the fly
	#
	atf_check -s exit:0 rump.ifconfig shmif0 inet6 $localip3

	# The new address starts with tentative state
	# XXX we have no stable way to check this, so skip for now
	#atf_check -s exit:0 -x "rump.ifconfig shmif0 |grep $localip3 |grep -q tentative"

	# Check DAD probe packets (Neighbor Solicitation Message)
	atf_check -s exit:0 sleep 2
	extract_new_packets > ./out
	$DEBUG && cat ./out
	pkt=$(make_ns_pkt_str 3 $localip3)
	atf_check -s exit:0 -x "cat ./out |grep -q '$pkt'"

	# Waiting for DAD complete
	atf_check -s exit:0 rump.ifconfig -w 10
	extract_new_packets > ./out
	$DEBUG && cat ./out

	# IPv6 DAD doesn't announce (Neighbor Advertisement Message)

	# The new address left tentative
	atf_check -s not-exit:0 -x "rump.ifconfig shmif0 |grep $localip3 |grep -q tentative"
}

dad_duplicated_body()
{
	local localip1=fc00::1
	local localip2=fc00::11
	local peerip=fc00::2

	atf_check -s exit:0 ${inetserver} $SOCKLOCAL
	atf_check -s exit:0 ${inetserver} $SOCKPEER

	setup_server $SOCKLOCAL $localip1
	setup_server $SOCKPEER $peerip

	export RUMP_SERVER=$SOCKLOCAL

	# The primary address isn't marked as duplicated
	atf_check -s not-exit:0 -x "rump.ifconfig shmif0 |grep $localip1 |grep -q duplicated"

	#
	# Add a new address duplicated with the peer server
	#
	atf_check -s exit:0 rump.ifconfig shmif0 inet6 $peerip
	atf_check -s exit:0 sleep 1

	# The new address is marked as duplicated
	atf_check -s exit:0 -x "rump.ifconfig shmif0 |grep $peerip |grep -q duplicated"

	# A unique address isn't marked as duplicated
	atf_check -s exit:0 rump.ifconfig shmif0 inet6 $localip2
	atf_check -s exit:0 sleep 1
	atf_check -s not-exit:0 -x "rump.ifconfig shmif0 |grep $localip2 |grep -q duplicated"
}

cleanup()
{
	gdb -ex bt /usr/bin/rump_server rump_server.core
	gdb -ex bt /usr/sbin/arp arp.core
	env RUMP_SERVER=$SOCKLOCAL rump.halt
	env RUMP_SERVER=$SOCKPEER rump.halt
}

dump_local()
{
	export RUMP_SERVER=$SOCKLOCAL
	rump.netstat -nr
	rump.arp -n -a
	rump.ifconfig
	$HIJACKING dmesg
}

dump_peer()
{
	export RUMP_SERVER=$SOCKPEER
	rump.netstat -nr
	rump.arp -n -a
	rump.ifconfig
	$HIJACKING dmesg
}

dump()
{
	dump_local
	dump_peer
	shmif_dumpbus -p - bus1 2>/dev/null| tcpdump -n -e -r -
}

dad_basic_cleanup()
{
	$DEBUG && dump_local
	$DEBUG && shmif_dumpbus -p - bus1 2>/dev/null| tcpdump -n -e -r -
	env RUMP_SERVER=$SOCKLOCAL rump.halt
}

dad_duplicated_cleanup()
{
	$DEBUG && dump
	cleanup
}

atf_init_test_cases()
{
	atf_add_test_case dad_basic
	atf_add_test_case dad_duplicated
}
