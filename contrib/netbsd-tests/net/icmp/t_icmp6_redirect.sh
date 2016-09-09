#	$NetBSD: t_icmp6_redirect.sh,v 1.2 2016/08/10 22:17:44 kre Exp $
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
netserver="rump_server -lrumpnet -lrumpnet_net -lrumpnet_shmif"
netserver="${netserver} -lrumpnet_netinet -lrumpnet_netinet6 -lrumpdev"

SOCK_LOCAL=unix://commsock1
SOCK_PEER=unix://commsock2
SOCK_GW1=unix://commsock3
SOCK_GW2=unix://commsock4

BUS1=bus1
BUS2=bus2
IP6BUS1=fc00:1::/64
IP6BUS2=fc00:2::/64
IP6IF0_LOCAL=fc00:1::2
IP6IF0_PEER=fc00:2::2
IP6IF0_GW1=fc00:1::1
IP6IF1_GW1=fc00:2::1
IP6IF0_GW2=fc00:1::3

REDIRECT_TIMEOUT=5

DEBUG=true

get_lladdr()
{

	export RUMP_SERVER=${1}
	rump.ifconfig ${2} inet6 | awk "/fe80/ {sub(/%$2/, \"\"); print \$2;}"
	unset RUMP_SERVER

	return 0
}

atf_test_case basic cleanup

basic_head()
{

	atf_set "descr" "Test for the basically function of the ICMP6 redirect"
	atf_set "require.progs" "rump_server rump.route rump.ping rump.ifconfig"
}

basic_body()
{
	local gw1_lladdr0=
	local gw1_lladdr1=
	local gw2_lladdr0=

	atf_check -s exit:0 ${netserver} ${SOCK_LOCAL}
	atf_check -s exit:0 ${netserver} ${SOCK_PEER}
	atf_check -s exit:0 ${netserver} ${SOCK_GW1}
	atf_check -s exit:0 ${netserver} ${SOCK_GW2}

	#
	# Setup
	#
	# Setup gateway #1 (real gateway)
	export RUMP_SERVER=${SOCK_GW1}
	atf_check -s exit:0 rump.ifconfig shmif0 create
	atf_check -s exit:0 rump.ifconfig shmif0 linkstr ${BUS1}
	atf_check -s exit:0 rump.ifconfig shmif0 inet6 ${IP6IF0_GW1}
	atf_check -s exit:0 rump.ifconfig shmif0 up

	atf_check -s exit:0 rump.ifconfig shmif1 create
	atf_check -s exit:0 rump.ifconfig shmif1 linkstr ${BUS2}
	atf_check -s exit:0 rump.ifconfig shmif1 inet6 ${IP6IF1_GW1}
	atf_check -s exit:0 rump.ifconfig shmif1 up

	atf_check -s exit:0 -o match:'0.->.1' rump.sysctl -w \
	    net.inet6.ip6.forwarding=1
	unset RUMP_SERVER

	gw1_lladdr0=`get_lladdr ${SOCK_GW1} shmif0`
	gw1_lladdr1=`get_lladdr ${SOCK_GW1} shmif1`

	# Setup a peer behind gateway #1
	export RUMP_SERVER=${SOCK_PEER}
	atf_check -s exit:0 rump.ifconfig shmif0 create
	atf_check -s exit:0 rump.ifconfig shmif0 linkstr ${BUS2}
	atf_check -s exit:0 rump.ifconfig shmif0 inet6 ${IP6IF0_PEER}
	atf_check -s exit:0 rump.ifconfig shmif0 up
	atf_check -s exit:0 -o ignore rump.route add \
	    -inet6 default ${gw1_lladdr1}%shmif0
	unset RUMP_SERVER

	# Setup gateway #2 (fake gateway)
	export RUMP_SERVER=${SOCK_GW2}
	atf_check -s exit:0 rump.ifconfig shmif0 create
	atf_check -s exit:0 rump.ifconfig shmif0 linkstr ${BUS1}
	atf_check -s exit:0 rump.ifconfig shmif0 inet6 ${IP6IF0_GW2}
	atf_check -s exit:0 rump.ifconfig shmif0 up

	atf_check -s exit:0 -o ignore rump.route add \
	     -inet6 ${IP6BUS2} ${gw1_lladdr0}%shmif0
	atf_check -s exit:0 -o match:'0.->.1' rump.sysctl -w \
	    net.inet6.ip6.forwarding=1
	unset RUMP_SERVER

	gw2_lladdr0=`get_lladdr ${SOCK_GW2} shmif0`

	export RUMP_SERVER=${SOCK_LOCAL}
	atf_check -s exit:0 rump.ifconfig shmif0 create
	atf_check -s exit:0 rump.ifconfig shmif0 linkstr ${BUS1}
	atf_check -s exit:0 rump.ifconfig shmif0 inet6 ${IP6IF0_LOCAL}
	atf_check -s exit:0 rump.ifconfig shmif0 up

	# Teach the fake gateway as the default gateway
	atf_check -s exit:0 -o ignore rump.route add \
	    -inet6 default ${gw2_lladdr0}%shmif0
	$DEBUG && rump.route get -inet6 ${IP6IF0_PEER}

	atf_check -s exit:0 -o ignore rump.sysctl -w \
	    net.inet6.icmp6.redirtimeout=$REDIRECT_TIMEOUT

	#
	# Tests
	#
	atf_check -s exit:0 -o ignore rump.ping6 -c 1 -n ${IP6IF0_PEER}
	$DEBUG && rump.route show -inet6
	# Check if a created route is correctly redirected to gateway #1
	atf_check -s exit:0 -o match:"gateway: ${gw1_lladdr0}" rump.route get \
	    -inet6 ${IP6IF0_PEER}

	atf_check -s exit:0 sleep $((REDIRECT_TIMEOUT + 2))
	$DEBUG && rump.route show -inet6
	# Check if the created route is expired
	atf_check -s exit:0 -o not-match:"gateway: ${gw1_lladdr0}" rump.route get \
	    -inet6 ${IP6IF0_PEER}

	unset RUMP_SERVER
}

basic_cleanup()
{

	env RUMP_SERVER=$SOCK_LOCAL rump.halt
	env RUMP_SERVER=$SOCK_PEER rump.halt
	env RUMP_SERVER=$SOCK_GW1 rump.halt
	env RUMP_SERVER=$SOCK_GW2 rump.halt
}

atf_init_test_cases()
{

	atf_add_test_case basic
}
