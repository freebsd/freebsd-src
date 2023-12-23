#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright © 2023 Orange Business Services
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

. $(atf_get_srcdir)/utils.subr

sctp_init()
{
	pft_init
	if ! kldstat -q -m sctp; then
		atf_skip "This test requires SCTP"
	fi
}

atf_test_case "basic_v4" "cleanup"
basic_v4_head()
{
	atf_set descr 'Basic SCTP connection over IPv4 passthrough'
	atf_set require.user root
}

basic_v4_body()
{
	sctp_init

	j="sctp:basic_v4"
	epair=$(vnet_mkepair)

	vnet_mkjail ${j}a ${epair}a
	vnet_mkjail ${j}b ${epair}b

	jexec ${j}a ifconfig ${epair}a 192.0.2.1/24 up
	jexec ${j}b ifconfig ${epair}b 192.0.2.2/24 up
	# Sanity check
	atf_check -s exit:0 -o ignore \
	    jexec ${j}a ping -c 1 192.0.2.2

	jexec ${j}a pfctl -e
	pft_set_rules ${j}a \
		"block" \
		"pass in proto sctp to port 1234"

	echo "foo" | jexec ${j}a nc --sctp -N -l 1234 &

	# Wait for the server to start
	sleep 1

	out=$(jexec ${j}b nc --sctp -N -w 3 192.0.2.1 1234)
	if [ "$out" != "foo" ]; then
		atf_fail "SCTP connection failed"
	fi

	# Now with scrub rules present, so normalization is done
	pft_set_rules ${j}a \
		"scrub on ${j}a" \
		"block" \
		"pass in proto sctp to port 1234"

	echo "foo" | jexec ${j}a nc --sctp -N -l 1234 &
	sleep 1

	out=$(jexec ${j}b nc --sctp -N -w 3 192.0.2.1 1234)
	if [ "$out" != "foo" ]; then
		atf_fail "SCTP connection failed"
	fi

	# Now fail with a blocked port
	echo "foo" | jexec ${j}a nc --sctp -N -l 1235 &
	sleep 1

	out=$(jexec ${j}b nc --sctp -N -w 3 192.0.2.1 1235)
	if [ "$out" == "foo" ]; then
		atf_fail "SCTP port block failed"
	fi

	# Now fail with a blocked port but passing source port
	out=$(jexec ${j}b nc --sctp -N -w 3 -p 1234 192.0.2.1 1235)
	if [ "$out" == "foo" ]; then
		atf_fail "SCTP port block failed"
	fi
}

basic_v4_cleanup()
{
	pft_cleanup
}

atf_test_case "basic_v6" "cleanup"
basic_v6_head()
{
	atf_set descr 'Basic SCTP connection over IPv6'
	atf_set require.user root
}

basic_v6_body()
{
	sctp_init

	j="sctp:basic_v6"
	epair=$(vnet_mkepair)

	vnet_mkjail ${j}a ${epair}a
	vnet_mkjail ${j}b ${epair}b

	jexec ${j}a ifconfig ${epair}a inet6 2001:db8::a/64 up no_dad
	jexec ${j}b ifconfig ${epair}b inet6 2001:db8::b/64 up no_dad

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    jexec ${j}a ping -6 -c 1 2001:db8::b

	jexec ${j}a pfctl -e
	pft_set_rules ${j}a \
		"block proto sctp" \
		"pass in proto sctp to port 1234"

	echo "foo" | jexec ${j}a nc -6 --sctp -N -l 1234 &

	# Wait for the server to start
	sleep 1

	out=$(jexec ${j}b nc --sctp -N -w 3 2001:db8::a 1234)
	if [ "$out" != "foo" ]; then
		atf_fail "SCTP connection failed"
	fi

	# Now with scrub rules present, so normalization is done
	pft_set_rules ${j}a \
		"scrub on ${j}a" \
		"block proto sctp" \
		"pass in proto sctp to port 1234"

	echo "foo" | jexec ${j}a nc -6 --sctp -N -l 1234 &
	sleep 1

	out=$(jexec ${j}b nc --sctp -N -w 3 2001:db8::a 1234)
	if [ "$out" != "foo" ]; then
		atf_fail "SCTP connection failed"
	fi

	# Now fail with a blocked port
	echo "foo" | jexec ${j}a nc -6 --sctp -N -l 1235 &
	sleep 1

	out=$(jexec ${j}b nc --sctp -N -w 3 2001:db8::a 1235)
	if [ "$out" == "foo" ]; then
		atf_fail "SCTP port block failed"
	fi

	# Now fail with a blocked port but passing source port
	out=$(jexec ${j}b nc --sctp -N -w 3 -p 1234 2001:db8::a 1235)
	if [ "$out" == "foo" ]; then
		atf_fail "SCTP port block failed"
	fi
}

basic_v6_cleanup()
{
	pft_cleanup
}

atf_test_case "abort_v4" "cleanup"
abort_v4_head()
{
	atf_set descr 'Test sending ABORT messages'
	atf_set require.user root
}

abort_v4_body()
{
	sctp_init

	j="sctp:abort_v4"
	epair=$(vnet_mkepair)

	vnet_mkjail ${j}a ${epair}a
	vnet_mkjail ${j}b ${epair}b

	jexec ${j}a ifconfig ${epair}a 192.0.2.1/24 up
	jexec ${j}b ifconfig ${epair}b 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    jexec ${j}a ping -c 1 192.0.2.2

	jexec ${j}a pfctl -e
	pft_set_rules ${j}a \
		"block return in proto sctp to port 1234"

	echo "foo" | jexec ${j}a nc --sctp -N -l 1234 &

	# Wait for the server to start
	sleep 1

	# If we get the abort we'll exit immediately, if we don't timeout will
	# stop nc.
	out=$(jexec ${j}b timeout 3 nc --sctp -N 192.0.2.1 1234)
	if [ $? -eq 124 ]; then
		atf_fail 'Abort not received'
	fi
	if [ "$out" == "foo" ]; then
		atf_fail "block failed entirely"
	fi

	# Without 'return' we will time out.
	pft_set_rules ${j}a \
		"block in proto sctp to port 1234"

	out=$(jexec ${j}b timeout 3 nc --sctp -N 192.0.2.1 1234)
	if [ $? -ne 124 ]; then
		atf_fail 'Abort sent anyway?'
	fi
}

abort_v4_cleanup()
{
	pft_cleanup
}

atf_test_case "abort_v6" "cleanup"
abort_v6_head()
{
	atf_set descr 'Test sending ABORT messages over IPv6'
	atf_set require.user root
}

abort_v6_body()
{
	sctp_init

	j="sctp:abort_v6"
	epair=$(vnet_mkepair)

	vnet_mkjail ${j}a ${epair}a
	vnet_mkjail ${j}b ${epair}b

	jexec ${j}a ifconfig ${epair}a inet6 2001:db8::a/64 no_dad
	jexec ${j}b ifconfig ${epair}b inet6 2001:db8::b/64 no_dad

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    jexec ${j}a ping -6 -c 1 2001:db8::b

	jexec ${j}a pfctl -e
	pft_set_rules ${j}a \
		"block return in proto sctp to port 1234"

	echo "foo" | jexec ${j}a nc -6 --sctp -N -l 1234 &

	# Wait for the server to start
	sleep 1

	# If we get the abort we'll exit immediately, if we don't timeout will
	# stop nc.
	out=$(jexec ${j}b timeout 3 nc --sctp -N 2001:db8::a 1234)
	if [ $? -eq 124 ]; then
		atf_fail 'Abort not received'
	fi
	if [ "$out" == "foo" ]; then
		atf_fail "block failed entirely"
	fi

	# Without 'return' we will time out.
	pft_set_rules ${j}a \
		"block in proto sctp to port 1234"

	out=$(jexec ${j}b timeout 3 nc --sctp -N 2001:db8::a 1234)
	if [ $? -ne 124 ]; then
		atf_fail 'Abort sent anyway?'
	fi
}

abort_v6_cleanup()
{
	pft_cleanup
}

atf_test_case "nat_v4" "cleanup"
nat_v4_head()
{
	atf_set descr 'Test NAT-ing SCTP over IPv4'
	atf_set require.user root
}

nat_v4_body()
{
	sctp_init

	j="sctp:nat_v4"
	epair_c=$(vnet_mkepair)
	epair_srv=$(vnet_mkepair)

	vnet_mkjail ${j}srv ${epair_srv}a
	vnet_mkjail ${j}gw ${epair_srv}b ${epair_c}a
	vnet_mkjail ${j}c ${epair_c}b

	jexec ${j}srv ifconfig ${epair_srv}a 198.51.100.1/24 up
	# No default route in srv jail, to ensure we're NAT-ing
	jexec ${j}gw ifconfig ${epair_srv}b 198.51.100.2/24 up
	jexec ${j}gw ifconfig ${epair_c}a 192.0.2.1/24 up
	jexec ${j}gw sysctl net.inet.ip.forwarding=1
	jexec ${j}c ifconfig ${epair_c}b 192.0.2.2/24 up
	jexec ${j}c route add default 192.0.2.1

	jexec ${j}gw pfctl -e
	pft_set_rules ${j}gw \
		"nat on ${epair_srv}b from 192.0.2.0/24 -> (${epair_srv}b)" \
		"pass"

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    jexec ${j}c ping -c 1 198.51.100.1

	echo "foo" | jexec ${j}srv nc --sctp -N -l 1234 &

	# Wait for the server to start
	sleep 1

	out=$(jexec ${j}c nc --sctp -N -w 3 198.51.100.1 1234)
	if [ "$out" != "foo" ]; then
		atf_fail "SCTP connection failed"
	fi
}

nat_v4_cleanup()
{
	pft_cleanup
}

atf_test_case "nat_v6" "cleanup"
nat_v6_head()
{
	atf_set descr 'Test NAT-ing SCTP over IPv6'
	atf_set require.user root
}

nat_v6_body()
{
	sctp_init

	j="sctp:nat_v6"
	epair_c=$(vnet_mkepair)
	epair_srv=$(vnet_mkepair)

	vnet_mkjail ${j}srv ${epair_srv}a
	vnet_mkjail ${j}gw ${epair_srv}b ${epair_c}a
	vnet_mkjail ${j}c ${epair_c}b

	jexec ${j}srv ifconfig ${epair_srv}a inet6 2001:db8::1/64 up no_dad
	# No default route in srv jail, to ensure we're NAT-ing
	jexec ${j}gw ifconfig ${epair_srv}b inet6 2001:db8::2/64 up no_dad
	jexec ${j}gw ifconfig ${epair_c}a inet6 2001:db8:1::1/64 up no_dad
	jexec ${j}gw sysctl net.inet6.ip6.forwarding=1
	jexec ${j}c ifconfig ${epair_c}b inet6 2001:db8:1::2/64 up no_dad
	jexec ${j}c route add -6 default 2001:db8:1::1

	jexec ${j}gw pfctl -e
	pft_set_rules ${j}gw \
		"nat on ${epair_srv}b from 2001:db8:1::/64 -> (${epair_srv}b)" \
		"pass"

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    jexec ${j}c ping -6 -c 1 2001:db8::1

	echo "foo" | jexec ${j}srv nc -6 --sctp -N -l 1234 &

	# Wait for the server to start
	sleep 1

	out=$(jexec ${j}c nc --sctp -N -w 3 2001:db8::1 1234)
	if [ "$out" != "foo" ]; then
		atf_fail "SCTP connection failed"
	fi
}

nat_v6_cleanup()
{
	pft_cleanup
}

atf_test_case "rdr_v4" "cleanup"
rdr_v4_head()
{
	atf_set descr 'Test rdr SCTP over IPv4'
	atf_set require.user root
}

rdr_v4_body()
{
	sctp_init

	j="sctp:rdr_v4"
	epair_c=$(vnet_mkepair)
	epair_srv=$(vnet_mkepair)

	vnet_mkjail ${j}srv ${epair_srv}a
	vnet_mkjail ${j}gw ${epair_srv}b ${epair_c}a
	vnet_mkjail ${j}c ${epair_c}b

	jexec ${j}srv ifconfig ${epair_srv}a 198.51.100.1/24 up
	# No default route in srv jail, to ensure we're NAT-ing
	jexec ${j}gw ifconfig ${epair_srv}b 198.51.100.2/24 up
	jexec ${j}gw ifconfig ${epair_c}a 192.0.2.1/24 up
	jexec ${j}gw sysctl net.inet.ip.forwarding=1
	jexec ${j}c ifconfig ${epair_c}b 192.0.2.2/24 up
	jexec ${j}c route add default 192.0.2.1

	jexec ${j}gw pfctl -e
	pft_set_rules ${j}gw \
		"rdr pass on ${epair_srv}b proto sctp from 198.51.100.0/24 to any port 1234 -> 192.0.2.2 port 1234" \
		"pass"

	echo "foo" | jexec ${j}c nc --sctp -N -l 1234 &

	# Wait for the server to start
	sleep 1

	out=$(jexec ${j}srv nc --sctp -N -w 3 198.51.100.2 1234)
	if [ "$out" != "foo" ]; then
		atf_fail "SCTP connection failed"
	fi

	# Despite configuring port changes pf will not do so.
	echo "bar" | jexec ${j}c nc --sctp -N -l 1234 &

	pft_set_rules ${j}gw \
		"rdr pass on ${epair_srv}b proto sctp from 198.51.100.0/24 to any port 1234 -> 192.0.2.2 port 4321" \
		"pass"

	# This will fail
	out=$(jexec ${j}srv nc --sctp -N -w 3 198.51.100.2 4321)
	if [ "$out" == "bar" ]; then
		atf_fail "Port was unexpectedly changed."
	fi

	# This succeeds
	out=$(jexec ${j}srv nc --sctp -N -w 3 198.51.100.2 1234)
	if [ "$out" != "bar" ]; then
		atf_fail "Port was unexpectedly changed."
	fi
}

rdr_v4_cleanup()
{
	pft_cleanup
}

atf_test_case "pfsync" "cleanup"
pfsync_head()
{
	atf_set descr 'Test pfsync-ing SCTP connections'
	atf_set require.user root
}

pfsync_body()
{
	# + Builds bellow topology and initiate an SCTP connection
	#   from client to server.
	# + Tests that the connection remains open when we fail over from
	#   router one to router two.
	#
	#                   ┌──────┐
	#                   │client│
	#                   └───┬──┘
	#                       │
	#                   ┌───┴───┐
	#                   │bridge0│
	#                   └┬─────┬┘
	#                    │     │
	#   ┌────────────────┴─┐ ┌─┴────────────────┐
	#   │        one       ├─┤       two        │
	#   └────────────────┬─┘ └─┬────────────────┘
	#                    │     │
	#                   ┌┴─────┴┐
	#                   │bridge1│
	#                   └───┬───┘
	#                       │
	#                   ┌───┴──┐
	#                   │server│
	#                   └──────┘

	sctp_init
	pfsynct_init
	if ! kldstat -q -m carp
	then
		atf_skip "This test requires carp"
	fi

	j="sctp:pfsync"

	tmp=`pwd`

	bridge0=$(vnet_mkbridge)
	bridge1=$(vnet_mkbridge)

	epair_c=$(vnet_mkepair)
	epair_one0=$(vnet_mkepair)
	epair_two0=$(vnet_mkepair)
	epair_sync=$(vnet_mkepair)
	epair_one1=$(vnet_mkepair)
	epair_two1=$(vnet_mkepair)
	epair_srv=$(vnet_mkepair)

	ifconfig ${bridge0} addm ${epair_c}a addm ${epair_one0}a addm ${epair_two0}a
	ifconfig ${epair_one0}a up
	ifconfig ${epair_two0}a up
	ifconfig ${epair_c}a up
	ifconfig ${bridge0} up

	ifconfig ${bridge1} addm ${epair_srv}a addm ${epair_one1}a addm ${epair_two1}a
	ifconfig ${epair_one1}a up
	ifconfig ${epair_two1}a up
	ifconfig ${epair_srv}a up
	ifconfig ${bridge1} up

	vnet_mkjail ${j}c ${epair_c}b
	jexec ${j}c ifconfig ${epair_c}b 192.0.2.2/24 up
	jexec ${j}c route add default 192.0.2.1

	vnet_mkjail ${j}one ${epair_one0}b ${epair_one1}b ${epair_sync}a
	jexec ${j}one ifconfig ${epair_one0}b 192.0.2.3/24 up
	jexec ${j}one ifconfig ${epair_one0}b \
	    alias 192.0.2.1/32 vhid 1 pass 1234
	jexec ${j}one ifconfig ${epair_one1}b 198.51.100.3/24 up
	jexec ${j}one ifconfig ${epair_one1}b \
	    alias 198.51.100.2/32 vhid 2 pass 4321
	jexec ${j}one ifconfig ${epair_sync}a 203.0.113.1/24 up
	jexec ${j}one ifconfig pfsync0 \
		syncdev ${epair_sync}a \
		maxupd 1 \
		up
	jexec ${j}one sysctl net.inet.ip.forwarding=1

	vnet_mkjail ${j}two ${epair_two0}b ${epair_two1}b ${epair_sync}b
	jexec ${j}two ifconfig ${epair_two0}b 192.0.2.4/24 up
	jexec ${j}two ifconfig ${epair_two0}b \
	    alias 192.0.2.1/32 vhid 1 pass 1234
	jexec ${j}two ifconfig ${epair_two1}b 198.51.100.4/24 up
	jexec ${j}two ifconfig ${epair_two1}b \
	    alias 198.51.100.2/32 vhid 2 pass 4321
	jexec ${j}two ifconfig ${epair_sync}b 203.0.113.2/24 up
	jexec ${j}two ifconfig pfsync0 \
		syncdev ${epair_sync}b \
		maxupd 1 \
		up
	jexec ${j}two sysctl net.inet.ip.forwarding=1

	vnet_mkjail ${j}srv ${epair_srv}b
	jexec ${j}srv ifconfig ${epair_srv}b 198.51.100.1/24 up
	jexec ${j}srv route add default 198.51.100.2

	# Demote two, to avoid dealing with asymmetric routing
	jexec ${j}two sysctl net.inet.carp.demotion=50

	jexec ${j}one pfctl -e
	pft_set_rules ${j}one \
		"block all" \
		"pass proto { icmp, pfsync, carp }" \
		"pass proto sctp to port 1234" \
		"pass proto tcp to port 1234"

	jexec ${j}two pfctl -e
	pft_set_rules ${j}two \
		"block all" \
		"pass proto { icmp, pfsync, carp }" \
		"pass proto sctp to port 1234" \
		"pass proto tcp to port 1234"

	# Give carp time to get set up
	sleep 2

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    jexec ${j}c ping -c 1 198.51.100.1

	# Now start up an SCTP connection
	touch ${tmp}/input
	tail -F ${tmp}/input | jexec ${j}srv nc --sctp -l 1234 &
	sleep 1

	jexec ${j}c nc --sctp 198.51.100.1 1234 > ${tmp}/output &
	echo "1" >> ${tmp}/input

	# Give time for the traffic to arrive
	sleep 1
	line=$(tail -n -1 ${tmp}/output)
	if [ "${line}" != "1" ];
	then
		echo "Found ${line}"
		cat ${tmp}/output
		atf_fail "Initial SCTP connection failed"
	fi

	# Verify that two has the connection too
	state=$(jexec ${j}two pfctl -ss | grep sctp)
	if [ -z "${state}" ];
	then
		jexec ${j}two pfctl -ss
		atf_fail "Failed to find SCTP state on secondary pfsync host"
	fi

	# Now fail over (both carp IPs should switch here)
	jexec ${j}one sysctl net.inet.carp.demotion=100

	while ! jexec ${j}one ifconfig ${epair_one0}b | grep MASTER;
	do
		sleep 1
	done
	while ! jexec ${j}one ifconfig ${epair_one1}b | grep MASTER;
	do
		sleep 1
	done

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    jexec ${j}c ping -c 1 198.51.100.1

	# And check that the connection is still live
	echo "2" >> ${tmp}/input
	sleep 1
	line=$(tail -n -1 ${tmp}/output)
	if [ "${line}" != "2" ];
	then
		echo "Found ${line}"
		cat ${tmp}/output
		atf_fail "SCTP failover failed"
	fi
}

pfsync_cleanup()
{
	pfsynct_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "basic_v4"
	atf_add_test_case "basic_v6"
	atf_add_test_case "abort_v4"
	atf_add_test_case "abort_v6"
	atf_add_test_case "nat_v4"
	atf_add_test_case "nat_v6"
	atf_add_test_case "rdr_v4"
	atf_add_test_case "pfsync"
}
