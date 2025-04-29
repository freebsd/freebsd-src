#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Modirum MDPay
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

common_dir=$(atf_get_srcdir)/../common

syncookie_state()
{
	jail=$1

	jexec $jail pfctl -si -v | grep -A 2 '^Syncookies' | grep active \
	    | awk '{ print($2); }'
}

atf_test_case "basic" "cleanup"
basic_head()
{
	atf_set descr 'Basic syncookie test'
	atf_set require.user root
}

basic_body()
{
	pft_init

	epair=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.1/24 up
	jexec alcatraz /usr/sbin/inetd -p ${PWD}/inetd-alcatraz.pid \
	    $(atf_get_srcdir)/echo_inetd.conf

	ifconfig ${epair}a 192.0.2.2/24 up

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"set syncookies always" \
		"pass in" \
		"pass out"

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.1

	reply=$(echo foo | nc -N -w 5 192.0.2.1 7)
	if [ "${reply}" != "foo" ];
	then
		atf_fail "Failed to connect to syncookie protected echo daemon"
	fi

	# Check that status shows syncookies as being active
	active=$(syncookie_state alcatraz)
	if [ "$active" != "active" ];
	then
		atf_fail "syncookies not active"
	fi
}

basic_cleanup()
{
	rm -f ${PWD}/inetd-alcatraz.pid
	pft_cleanup
}

atf_test_case "basic_v6" "cleanup"
basic_v6_head()
{
	atf_set descr 'Basic syncookie IPv6 test'
	atf_set require.user root
}

basic_v6_body()
{
	pft_init

	epair=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b inet6 2001:db8::1/64 up no_dad
	jexec alcatraz /usr/sbin/inetd -p ${PWD}/inetd-alcatraz.pid \
	    $(atf_get_srcdir)/echo_inetd.conf

	ifconfig ${epair}a inet6 2001:db8::2/64 up no_dad

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"set syncookies always" \
		"pass in" \
		"pass out"

	# Sanity check
	atf_check -s exit:0 -o ignore ping6 -c 1 2001:db8::1

	reply=$(echo foo | nc -N -w 5 2001:db8::1 7)
	if [ "${reply}" != "foo" ];
	then
		atf_fail "Failed to connect to syncookie protected echo daemon"
	fi

	# Check that status shows syncookies as being active
	active=$(syncookie_state alcatraz)
	if [ "$active" != "active" ];
	then
		atf_fail "syncookies not active"
	fi
}

basic_v6_cleanup()
{
	pft_cleanup
}

atf_test_case "forward" "cleanup"
forward_head()
{
	atf_set descr 'Syncookies for forwarded hosts'
	atf_set require.user root
}

forward_body()
{
	pft_init

	epair_in=$(vnet_mkepair)
	epair_out=$(vnet_mkepair)

	vnet_mkjail fwd ${epair_in}b ${epair_out}a
	vnet_mkjail srv ${epair_out}b

	jexec fwd ifconfig ${epair_in}b 192.0.2.1/24 up
	jexec fwd ifconfig ${epair_out}a 198.51.100.1/24 up
	jexec fwd sysctl net.inet.ip.forwarding=1

	jexec srv ifconfig ${epair_out}b 198.51.100.2/24 up
	jexec srv route add default 198.51.100.1
	jexec srv /usr/sbin/inetd -p ${PWD}/inetd-alcatraz.pid \
	    $(atf_get_srcdir)/echo_inetd.conf

	ifconfig ${epair_in}a 192.0.2.2/24 up
	route add -net 198.51.100.0/24 192.0.2.1

	jexec fwd pfctl -e
	pft_set_rules fwd \
		"set syncookies always" \
		"pass in" \
		"pass out"

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 198.51.100.2

	reply=$(echo foo | nc -N -w 5 198.51.100.2 7)
	if [ "${reply}" != "foo" ];
	then
		atf_fail "Failed to connect to syncookie protected echo daemon"
	fi
}

forward_cleanup()
{
	pft_cleanup
}

atf_test_case "forward_v6" "cleanup"
forward_v6_head()
{
	atf_set descr 'Syncookies for forwarded hosts'
	atf_set require.user root
}

forward_v6_body()
{
	pft_init

	epair_in=$(vnet_mkepair)
	epair_out=$(vnet_mkepair)

	vnet_mkjail fwd ${epair_in}b ${epair_out}a
	vnet_mkjail srv ${epair_out}b

	jexec fwd ifconfig ${epair_in}b inet6 2001:db8::1/64 up no_dad
	jexec fwd ifconfig ${epair_out}a inet6 2001:db8:1::1/64 up no_dad
	jexec fwd sysctl net.inet6.ip6.forwarding=1

	jexec srv ifconfig ${epair_out}b inet6 2001:db8:1::2/64 up no_dad
	jexec srv route -6 add default 2001:db8:1::1
	jexec srv /usr/sbin/inetd -p ${PWD}/inetd-alcatraz.pid \
	    $(atf_get_srcdir)/echo_inetd.conf

	ifconfig ${epair_in}a inet6 2001:db8::2/64 up no_dad
	route -6 add -net 2001:db8:1::/64 2001:db8::1

	jexec fwd pfctl -e
	pft_set_rules fwd \
		"set syncookies always" \
		"pass in" \
		"pass out"

	# Sanity check
	atf_check -s exit:0 -o ignore ping6 -c 1 2001:db8:1::2

	reply=$(echo foo | nc -N -w 5 2001:db8:1::2 7)
	if [ "${reply}" != "foo" ];
	then
		atf_fail "Failed to connect to syncookie protected echo daemon"
	fi
}

forward_v6_cleanup()
{
	pft_cleanup
}

loopback_test()
{
	local addr port

	addr=$1
	port=$2

	# syncookies don't work without state tracking enabled.
	atf_check -e ignore pfctl -e
	atf_check pfctl -f - <<__EOF__
set syncookies always
pass all keep state
__EOF__

        # Try to transmit data over a loopback connection.
	cat <<__EOF__ >in
Creativity, no.
__EOF__
	nc -l $addr $port >out &

	atf_check nc -N $addr $port < in

	atf_check -o file:in cat out

	atf_check -e ignore pfctl -d
}

atf_test_case "loopback" "cleanup"
loopback_head()
{
	atf_set descr 'Make sure that loopback v4 TCP connections work with syncookies on'
	atf_set require.user root
}

loopback_body()
{
	local epair

	pft_init

	atf_check ifconfig lo0 127.0.0.1/8
	atf_check ifconfig lo0 up

	loopback_test 127.0.0.1 8080

	epair=$(vnet_mkepair)
	atf_check ifconfig ${epair}a inet 192.0.2.1/24

	loopback_test 192.0.2.1 8081
}

loopback_cleanup()
{
	pft_cleanup
}

atf_test_case "loopback_v6" "cleanup"
loopback_v6_head()
{
	atf_set descr 'Make sure that loopback v6 TCP connections work with syncookies on'
	atf_set require.user root
}

loopback_v6_body()
{
	local epair

	pft_init

	atf_check ifconfig lo0 up

	loopback_test ::1 8080

	epair=$(vnet_mkepair)
	atf_check ifconfig ${epair}a inet6 2001:db8::1/64

	loopback_test 2001:db8::1 8081
}

loopback_v6_cleanup()
{
	pft_cleanup
}

atf_test_case "nostate" "cleanup"
nostate_head()
{
	atf_set descr 'Ensure that we do not create until SYN|ACK'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

nostate_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a 192.0.2.2/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.1/24 up

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"set syncookies always" \
		"pass in" \
		"pass out"

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.1

	# Now syn flood to create many states
	${common_dir}/pft_synflood.py \
		--sendif ${epair}a \
		--to 192.0.2.2 \
		--count 20

	states=$(jexec alcatraz pfctl -ss | grep tcp)
	if [ -n "$states" ];
	then
		echo "$states"
		atf_fail "Found unexpected state"
	fi
}

nostate_cleanup()
{
	pft_cleanup
}

atf_test_case "nostate_v6" "cleanup"
nostate_v6_head()
{
	atf_set descr 'Ensure that we do not create until SYN|ACK'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

nostate_v6_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a inet6 2001:db8::2/64 up no_dad

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b inet6 2001:db8::1/64 up no_dad

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"set syncookies always" \
		"pass in" \
		"pass out"

	# Sanity check
	atf_check -s exit:0 -o ignore ping6 -c 1 2001:db8::1

	# Now syn flood to create many states
	${common_dir}/pft_synflood.py \
        --ip6 \
		--sendif ${epair}a \
		--to 2001:db8::2 \
		--count 20

	states=$(jexec alcatraz pfctl -ss | grep tcp)
	if [ -n "$states" ];
	then
		echo "$states"
		atf_fail "Found unexpected state"
	fi
}

nostate_v6_cleanup()
{
	pft_cleanup
}

atf_test_case "adaptive" "cleanup"
adaptive_head()
{
	atf_set descr 'Adaptive mode test'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

adaptive_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a 192.0.2.2/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.1/24 up

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"set limit states 100" \
		"set syncookies adaptive (start 10%%, end 5%%)" \
		"pass in" \
		"pass out"

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.1

	# Check that status shows syncookies as being inactive
	active=$(syncookie_state alcatraz)
	if [ "$active" != "inactive" ];
	then
		atf_fail "syncookies active when they should not be"
	fi

	# Now syn flood to create many states
	${common_dir}/pft_synflood.py \
		--sendif ${epair}a \
		--to 192.0.2.2 \
		--count 100

	# Check that status shows syncookies as being active
	active=$(syncookie_state alcatraz)
	if [ "$active" != "active" ];
	then
		atf_fail "syncookies not active"
	fi

	# Adaptive mode should kick in and stop us from creating more than
	# about 10 states
	states=$(jexec alcatraz pfctl -ss | grep tcp | wc -l)
	if [ "$states" -gt 20 ];
	then
		echo "$states"
		atf_fail "Found unexpected states"
	fi
}

adaptive_cleanup()
{
	pft_cleanup
}

atf_test_case "limits" "cleanup"
limits_head()
{
	atf_set descr 'Ensure limit calculation works for low or high state limits'
	atf_set require.user root
}

limits_body()
{
	pft_init

	vnet_mkjail alcatraz

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"set limit states 1" \
		"set syncookies adaptive (start 10%%, end 5%%)" \
		"pass in" \
		"pass out"

	pft_set_rules alcatraz \
		"set limit states 326000000" \
		"set syncookies adaptive (start 10%%, end 5%%)" \
		"pass in" \
		"pass out"
}

limits_cleanup()
{
	pft_cleanup
}

atf_test_case "port_reuse" "cleanup"
port_reuse_head()
{
	atf_set descr 'Test rapid port re-use'
	atf_set require.user root
}

port_reuse_body()
{
	pft_init

	epair=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair}b
	vnet_mkjail singsing
	jexec alcatraz ifconfig ${epair}b 192.0.2.1/24 up
	jexec alcatraz /usr/sbin/inetd -p ${PWD}/inetd-alcatraz.pid \
	    $(atf_get_srcdir)/echo_inetd.conf

	ifconfig ${epair}a 192.0.2.2/24 up

	jexec alcatraz pfctl -e
	jexec alcatraz pfctl -x loud
	pft_set_rules alcatraz \
		"set syncookies always" \
		"pass in" \
		"pass out"

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.1

	reply=$(echo foo | nc -p 1234 -N -w 5 192.0.2.1 7)
	if [ "${reply}" != "foo" ];
	then
		atf_fail "Failed to connect to syncookie protected echo daemon"
	fi

	# We can't re-use the source IP/port combo quickly enough, so we're
	# going to play a really dirty trick, and move our interface to a new
	# jail, and do it from there.
	ifconfig ${epair}a vnet singsing
	jexec singsing ifconfig ${epair}a 192.0.2.2/24 up
	atf_check -s exit:0 -o ignore jexec singsing ping -c 1 192.0.2.1

	reply=$(echo bar | jexec singsing nc -p 1234 -N -w 5 192.0.2.1 7)
	if [ "${reply}" != "bar" ];
	then
		atf_fail "Failed to connect to syncookie protected echo daemon (2)"
	fi
}

port_reuse_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "basic"
	atf_add_test_case "basic_v6"
	atf_add_test_case "forward"
	atf_add_test_case "forward_v6"
	atf_add_test_case "loopback"
	atf_add_test_case "loopback_v6"
	atf_add_test_case "nostate"
	atf_add_test_case "nostate_v6"
	atf_add_test_case "adaptive"
	atf_add_test_case "limits"
	atf_add_test_case "port_reuse"
}
