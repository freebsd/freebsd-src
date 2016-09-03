#	$NetBSD: t_ipv6_lifetime.sh,v 1.2 2016/08/10 21:33:52 kre Exp $
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

INET6SERVER="rump_server -lrumpnet -lrumpnet_net -lrumpnet_netinet -lrumpdev"
INET6SERVER="$INET6SERVER -lrumpnet_netinet6 -lrumpnet_shmif"

SOCK=unix://sock
BUS=./bus

DEBUG=false

atf_test_case basic cleanup

basic_head()
{
	atf_set "descr" "Tests for IPv6 address lifetime"
	atf_set "require.progs" "rump_server"
}

basic_body()
{
	local time=5
	local bonus=2
	local ip="fc00::1"

	atf_check -s exit:0 ${INET6SERVER} $SOCK
	export RUMP_SERVER=$SOCK

	atf_check -s exit:0 rump.ifconfig shmif0 create
	atf_check -s exit:0 rump.ifconfig shmif0 linkstr $BUS
	atf_check -s exit:0 rump.ifconfig shmif0 up

	# A normal IP address doesn't contain preferred/valid lifetime
	atf_check -s exit:0 rump.ifconfig shmif0 inet6 $ip
	$DEBUG && rump.ifconfig -L shmif0
	atf_check -s exit:0 -o not-match:'pltime' rump.ifconfig -L shmif0
	atf_check -s exit:0 -o not-match:'vltime' rump.ifconfig -L shmif0
	atf_check -s exit:0 rump.ifconfig shmif0 inet6 $ip delete

	# Setting only a preferred lifetime
	atf_check -s exit:0 rump.ifconfig shmif0 inet6 $ip pltime $time
	$DEBUG && rump.ifconfig -L shmif0
	atf_check -s exit:0 -o match:'pltime' rump.ifconfig -L shmif0
	atf_check -s exit:0 -o match:'vltime infty' rump.ifconfig -L shmif0
	atf_check -s exit:0 sleep $(($time + $bonus))
	$DEBUG && rump.ifconfig -L shmif0
	# Should remain but marked as deprecated
	atf_check -s exit:0 -o match:'deprecated' rump.ifconfig -L shmif0
	atf_check -s exit:0 rump.ifconfig shmif0 inet6 $ip delete

	# Setting only a valid lifetime (invalid)
	atf_check -s not-exit:0 -e match:'Invalid argument' \
	    rump.ifconfig shmif0 inet6 $ip vltime $time

	# Setting both preferred and valid lifetimes (same value)
	atf_check -s exit:0 rump.ifconfig shmif0 inet6 $ip \
	    pltime $time vltime $time
	$DEBUG && rump.ifconfig -L shmif0
	atf_check -s exit:0 -o match:'pltime' rump.ifconfig -L shmif0
	atf_check -s exit:0 -o match:'vltime' rump.ifconfig -L shmif0
	atf_check -s exit:0 sleep $(($time + $bonus))
	$DEBUG && rump.ifconfig -L shmif0
	# Shouldn't remain anymore
	atf_check -s exit:0 -o not-match:"$ip" rump.ifconfig -L shmif0

	# Setting both preferred and valid lifetimes (pltime < vltime)
	atf_check -s exit:0 rump.ifconfig shmif0 inet6 $ip \
	    pltime $time vltime $((time * 2))
	$DEBUG && rump.ifconfig -L shmif0
	atf_check -s exit:0 -o match:'pltime' rump.ifconfig -L shmif0
	atf_check -s exit:0 -o match:'vltime' rump.ifconfig -L shmif0
	atf_check -s exit:0 sleep $(($time + $bonus))
	$DEBUG && rump.ifconfig -L shmif0
	# Should remain but marked as deprecated
	atf_check -s exit:0 -o match:'deprecated' rump.ifconfig -L shmif0
	atf_check -s exit:0 sleep $(($time + $bonus))
	$DEBUG && rump.ifconfig -L shmif0
	# Shouldn't remain anymore
	atf_check -s exit:0 -o not-match:"$ip" rump.ifconfig -L shmif0

	# Setting both preferred and valid lifetimes (pltime > vltime)
	atf_check -s not-exit:0 -e match:'Invalid argument' rump.ifconfig \
	    shmif0 inet6 $ip pltime $(($time * 2)) vltime $time

	return 0
}

cleanup()
{
	env RUMP_SERVER=$SOCK rump.halt
}

dump()
{
	env RUMP_SERVER=$SOCK rump.ifconfig
	env RUMP_SERVER=$SOCK rump.netstat -nr
	shmif_dumpbus -p - $BUS 2>/dev/null| tcpdump -n -e -r -
}

basic_cleanup()
{
	$DEBUG && dump
	cleanup
}

atf_init_test_cases()
{
	atf_add_test_case basic
}
