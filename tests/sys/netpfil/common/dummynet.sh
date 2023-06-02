# $FreeBSD$
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Rubicon Communications, LLC (Netgate)
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
. $(atf_get_srcdir)/runner.subr

interface_removal_head()
{
	atf_set descr 'Test removing interfaces with dummynet delayed traffic'
	atf_set require.user root
}

interface_removal_body()
{
	fw=$1
	firewall_init $fw
	dummynet_init $fw

	epair=$(vnet_mkepair)
	vnet_mkjail alcatraz ${epair}b

	ifconfig ${epair}a 192.0.2.1/24 up
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore ping -i .1 -c 3 -s 1200 192.0.2.2

	jexec alcatraz dnctl pipe 1 config delay 1500

	firewall_config alcatraz ${fw} \
		"ipfw"	\
			"ipfw add 1000 pipe 1 ip from any to any" \
		"pf"	\
			"pass on ${epair}b dnpipe 1"

	# single ping succeeds just fine
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	# Send traffic that'll still be pending when we remove the interface
	ping -c 5 -s 1200 192.0.2.2 &
	sleep 1 # Give ping the chance to start.

	# Remove the interface, but keep the jail around for a bit
	ifconfig ${epair}a destroy

	sleep 3
}

interface_removal_cleanup()
{
	firewall_cleanup $1
}

pipe_head()
{
	atf_set descr 'Basic pipe test'
	atf_set require.user root
}

pipe_body()
{
	fw=$1
	firewall_init $fw
	dummynet_init $fw

	epair=$(vnet_mkepair)
	vnet_mkjail alcatraz ${epair}b

	ifconfig ${epair}a 192.0.2.1/24 up
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore ping -i .1 -c 3 -s 1200 192.0.2.2

	jexec alcatraz dnctl pipe 1 config bw 30Byte/s

	firewall_config alcatraz ${fw} \
		"ipfw"	\
			"ipfw add 1000 pipe 1 ip from any to any" \
		"pf"	\
			"pass on ${epair}b dnpipe 1"

	# single ping succeeds just fine
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	# Saturate the link
	ping -i .1 -c 5 -s 1200 192.0.2.2

	# We should now be hitting the limits and get this packet dropped.
	atf_check -s exit:2 -o ignore ping -c 1 -s 1200 192.0.2.2
}

pipe_cleanup()
{
	firewall_cleanup $1
}

pipe_v6_head()
{
	atf_set descr 'Basic IPv6 pipe test'
	atf_set require.user root
}

pipe_v6_body()
{
	fw=$1
	firewall_init $fw
	dummynet_init $fw

	epair=$(vnet_mkepair)
	vnet_mkjail alcatraz ${epair}b

	ifconfig ${epair}a inet6 2001:db8:42::1/64 up no_dad
	jexec alcatraz ifconfig ${epair}b inet6 2001:db8:42::2/64 up no_dad

	# Sanity check
	atf_check -s exit:0 -o ignore ping6 -i .1 -c 3 -s 1200 2001:db8:42::2

	jexec alcatraz dnctl pipe 1 config bw 100Byte/s

	firewall_config alcatraz ${fw} \
		"ipfw"	\
			"ipfw add 1000 pipe 1 ip6 from any to any" \
		"pf"	\
			"pass on ${epair}b dnpipe 1"

	# Single ping succeeds
	atf_check -s exit:0 -o ignore ping6 -c 1 2001:db8:42::2

	# Saturate the link
	ping6 -i .1 -c 5 -s 1200 2001:db8:42::2

	# We should now be hitting the limit and get this packet dropped.
	atf_check -s exit:2 -o ignore ping6 -c 1 -s 1200 2001:db8:42::2
}

pipe_v6_cleanup()
{
	firewall_cleanup $1
}

codel_head()
{
	atf_set descr 'FQ_CODEL basic test'
	atf_set require.user root
}

codel_body()
{
	fw=$1
	firewall_init $fw
	dummynet_init $fw

	epair=$(vnet_mkepair)
	vnet_mkjail alcatraz ${epair}b

	ifconfig ${epair}a 192.0.2.1/24 up
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore ping -i .1 -c 3 -s 1200 192.0.2.2

	jexec alcatraz dnctl pipe 1 config  bw 10Mb queue 100 droptail
	jexec alcatraz dnctl sched 1 config pipe 1 type fq_codel target 0ms interval 0ms quantum 1514 limit 10240 flows 1024 ecn
	jexec alcatraz dnctl queue 1 config pipe 1 droptail

	firewall_config alcatraz ${fw} \
		"ipfw"	\
			"ipfw add 1000 queue 1 ip from any to any" \
		"pf"	\
			"pass dnqueue 1"

	# single ping succeeds just fine
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2
}

codel_cleanup()
{
	firewall_cleanup $1
}

queue_head()
{
	atf_set descr 'Basic queue test'
	atf_set require.user root
}

queue_body()
{
	fw=$1

	if [ $fw = "ipfw" ] && [ "$(atf_config_get ci false)" = "true" ]; then
		atf_skip "https://bugs.freebsd.org/264805"
	fi

	firewall_init $fw
	dummynet_init $fw

	epair=$(vnet_mkepair)
	vnet_mkjail alcatraz ${epair}b

	ifconfig ${epair}a 192.0.2.1/24 up
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up
	jexec alcatraz /usr/sbin/inetd -p inetd-alcatraz.pid \
	    $(atf_get_srcdir)/../pf/echo_inetd.conf

	# Sanity check
	atf_check -s exit:0 -o ignore ping -i .1 -c 3 -s 1200 192.0.2.2
	reply=$(echo "foo" | nc -N 192.0.2.2 7)
	if [ "$reply" != "foo" ];
	then
		atf_fail "Echo sanity check failed"
	fi

	jexec alcatraz dnctl pipe 1 config bw 1MByte/s
	jexec alcatraz dnctl sched 1 config pipe 1 type wf2q+
	jexec alcatraz dnctl queue 100 config sched 1 weight 99 mask all
	jexec alcatraz dnctl queue 200 config sched 1 weight 1 mask all

	firewall_config alcatraz ${fw} \
		"ipfw"	\
			"ipfw add 1000 queue 100 tcp from 192.0.2.2 to any out" \
			"ipfw add 1001 queue 200 icmp from 192.0.2.2 to any out" \
			"ipfw add 1002 allow ip from any to any" \
		"pf"	\
			"pass in proto tcp dnqueue (0, 100)" \
			"pass in proto icmp dnqueue (0, 200)"

	# Single ping succeeds
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	# Unsaturated TCP succeeds
	reply=$(echo "foo" | nc -w 5 -N 192.0.2.2 7)
	if [ "$reply" != "foo" ];
	then
		atf_fail "Unsaturated echo failed"
	fi

	# Saturate the link
	ping -f -s 1300 192.0.2.2 &

	# Allow this to fill the queue
	sleep 1

	# TCP should still just pass
	fails=0
	for i in `seq 1 3`
	do
		result=$(dd if=/dev/zero bs=1024 count=2000 | timeout 3 nc -w 5 -N 192.0.2.2 7 | wc -c)
		if [ $result -ne 2048000 ];
		then
			echo "Failed to prioritise TCP traffic. Got only $result bytes"
			fails=$(( ${fails} + 1 ))
		fi
	done
	if [ ${fails} -gt 0 ];
	then
		atf_fail "We failed prioritisation ${fails} times"
	fi

	# This will fail if we reverse the pola^W priority
	firewall_config alcatraz ${fw} \
		"ipfw"	\
			"ipfw add 1000 queue 200 tcp from 192.0.2.2 to any out" \
			"ipfw add 1001 queue 100 icmp from 192.0.2.2 to any out" \
			"ipfw add 1002 allow ip from any to any" \
		"pf"	\
			"pass in proto tcp dnqueue (0, 200)" \
			"pass in proto icmp dnqueue (0, 100)"

	jexec alcatraz ping -f -s 1300 192.0.2.1 &
	sleep 1

	fails=0
	for i in `seq 1 3`
	do
		result=$(dd if=/dev/zero bs=1024 count=2000 | timeout 3 nc -w 5 -N 192.0.2.2 7 | wc -c)
		if [ $result -ne 2048000 ];
		then
			echo "Failed to prioritise TCP traffic. Got only $result bytes"
			fails=$(( ${fails} + 1 ))
		fi
	done
	if [ ${fails} -lt 3 ];
	then
		atf_fail "We failed reversed prioritisation only ${fails} times."
	fi
}

queue_cleanup()
{
	firewall_cleanup $1
}

queue_v6_head()
{
	atf_set descr 'Basic queue test'
	atf_set require.user root
}

queue_v6_body()
{
	fw=$1
	firewall_init $fw
	dummynet_init $fw

	epair=$(vnet_mkepair)
	vnet_mkjail alcatraz ${epair}b

	ifconfig ${epair}a inet6 2001:db8:42::1/64 no_dad up
	jexec alcatraz ifconfig ${epair}b inet6 2001:db8:42::2 no_dad up
	jexec alcatraz /usr/sbin/inetd -p inetd-alcatraz.pid \
	    $(atf_get_srcdir)/../pf/echo_inetd.conf

	# Sanity check
	atf_check -s exit:0 -o ignore ping6 -i .1 -c 3 -s 1200 2001:db8:42::2
	reply=$(echo "foo" | nc -N 2001:db8:42::2 7)
	if [ "$reply" != "foo" ];
	then
		atf_fail "Echo sanity check failed"
	fi

	jexec alcatraz dnctl pipe 1 config bw 1MByte/s
	jexec alcatraz dnctl sched 1 config pipe 1 type wf2q+
	jexec alcatraz dnctl queue 100 config sched 1 weight 99 mask all
	jexec alcatraz dnctl queue 200 config sched 1 weight 1 mask all

	firewall_config alcatraz ${fw} \
		"ipfw"	\
			"ipfw add 1001 queue 100 tcp from 2001:db8:42::2 to any out" \
			"ipfw add 1000 queue 200 ipv6-icmp from 2001:db8:42::2 to any out" \
			"ipfw add 1002 allow ip6 from any to any" \
		"pf" \
			"pass in proto tcp dnqueue (0, 100)"	\
			"pass in proto icmp6 dnqueue (0, 200)"

	# Single ping succeeds
	atf_check -s exit:0 -o ignore ping6 -c 1 2001:db8:42::2

	# Unsaturated TCP succeeds
	reply=$(echo "foo" | nc -w 5 -N 2001:db8:42::2 7)
	if [ "$reply" != "foo" ];
	then
		atf_fail "Unsaturated echo failed"
	fi

	# Saturate the link
	ping6 -f -s 1200 2001:db8:42::2 &

	# Allow this to fill the queue
	sleep 1

	# TCP should still just pass
	fails=0
	for i in `seq 1 3`
	do
		result=$(dd if=/dev/zero bs=1024 count=1000 | timeout 3 nc -w 5 -N 2001:db8:42::2 7 | wc -c)
		if [ $result -ne 1024000 ];
		then
			echo "Failed to prioritise TCP traffic. Got only $result bytes"
			fails=$(( ${fails} + 1 ))
		fi
	done
	if [ ${fails} -gt 0 ];
	then
		atf_fail "We failed prioritisation ${fails} times"
	fi

	# What happens if we prioritise ICMP over TCP?
	firewall_config alcatraz ${fw} \
		"ipfw"	\
			"ipfw add 1001 queue 200 tcp from 2001:db8:42::2 to any out" \
			"ipfw add 1000 queue 100 ipv6-icmp from 2001:db8:42::2 to any out" \
			"ipfw add 1002 allow ip6 from any to any" \
		"pf" \
			"pass in proto tcp dnqueue (0, 200)"	\
			"pass in proto icmp6 dnqueue (0, 100)"

	fails=0
	for i in `seq 1 3`
	do
		result=$(dd if=/dev/zero bs=1024 count=1000 | timeout 3 nc -w 5 -N 2001:db8:42::2 7 | wc -c)
		if [ $result -ne 1024000 ];
		then
			echo "Failed to prioritise TCP traffic. Got only $result bytes"
			fails=$(( ${fails} + 1 ))
		fi
	done
	if [ ${fails} -lt 3 ];
	then
		atf_fail "We failed reversed prioritisation only ${fails} times."
	fi
}

queue_v6_cleanup()
{
	firewall_cleanup $1
}

nat_head()
{
	atf_set descr 'Basic dummynet + NAT test'
	atf_set require.user root
}

nat_body()
{
	fw=$1
	firewall_init $fw
	dummynet_init $fw
	nat_init $fw

	epair=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)

	ifconfig ${epair}a 192.0.2.2/24 up
	route add -net 198.51.100.0/24 192.0.2.1

	vnet_mkjail gw ${epair}b ${epair_two}a
	jexec gw ifconfig ${epair}b 192.0.2.1/24 up
	jexec gw ifconfig ${epair_two}a 198.51.100.1/24 up
	jexec gw sysctl net.inet.ip.forwarding=1

	vnet_mkjail srv ${epair_two}b
	jexec srv ifconfig ${epair_two}b 198.51.100.2/24 up

	jexec gw dnctl pipe 1 config bw 300Byte/s

	firewall_config gw $fw \
		"pf"	\
			"nat on ${epair_two}a inet from 192.0.2.0/24 to any -> (${epair_two}a)" \
			"pass dnpipe 1"

	# We've deliberately not set a route to 192.0.2.0/24 on srv, so the
	# only way it can respond to this is if NAT is applied correctly.
	atf_check -s exit:0 -o ignore ping -c 1 198.51.100.2
}

nat_cleanup()
{
	firewall_cleanup $1
}

setup_tests		\
	interface_removal	\
		ipfw	\
		pf	\
	pipe		\
		ipfw	\
		pf	\
	pipe_v6		\
		ipfw	\
		pf	\
	codel		\
		ipfw	\
		pf	\
	queue		\
		ipfw	\
		pf	\
	queue_v6	\
		ipfw	\
		pf	\
	nat		\
		pf
