# $FreeBSD$
#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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

common_dir=$(atf_get_srcdir)/../common

find_state()
{
	jexec alcatraz pfctl -ss | grep icmp | grep 192.0.2.2
}

find_state_v6()
{
	jexec alcatraz pfctl -ss | grep icmp | grep 2001:db8::2
}


atf_test_case "v4" "cleanup"
v4_head()
{
	atf_set descr 'Test killing states by IPv4 address'
	atf_set require.user root
	atf_set require.progs scapy
}

v4_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a 192.0.2.1/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up
	jexec alcatraz pfctl -e

	pft_set_rules alcatraz "block all" \
		"pass in proto icmp"

	# Sanity check & establish state
	atf_check -s exit:0 -o ignore ${common_dir}/pft_ping.py \
		--sendif ${epair}a \
		--to 192.0.2.2 \
		--replyif ${epair}a

	# Change rules to now deny the ICMP traffic
	pft_set_rules noflush alcatraz "block all"
	if ! find_state;
	then
		atf_fail "Setting new rules removed the state."
	fi

	# Killing with the wrong IP doesn't affect our state
	jexec alcatraz pfctl -k 192.0.2.3
	if ! find_state;
	then
		atf_fail "Killing with the wrong IP removed our state."
	fi

	# Killing with one correct address and one incorrect doesn't kill the state
	jexec alcatraz pfctl -k 192.0.2.1 -k 192.0.2.3
	if ! find_state;
	then
		atf_fail "Killing with one wrong IP removed our state."
	fi

	# Killing with correct address does remove the state
	jexec alcatraz pfctl -k 192.0.2.1
	if find_state;
	then
		atf_fail "Killing with the correct IP did not remove our state."
	fi
}

v4_cleanup()
{
	pft_cleanup
}

atf_test_case "v6" "cleanup"
v6_head()
{
	atf_set descr 'Test killing states by IPv6 address'
	atf_set require.user root
	atf_set require.progs scapy
}

v6_body()
{
	pft_init

	if [ "$(atf_config_get ci false)" = "true" ]; then
		atf_skip "https://bugs.freebsd.org/260458"
	fi

	epair=$(vnet_mkepair)
	ifconfig ${epair}a inet6 2001:db8::1/64 up no_dad

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b inet6 2001:db8::2/64 up no_dad
	jexec alcatraz pfctl -e

	pft_set_rules alcatraz "block all" \
		"pass in proto icmp6"

	# Sanity check & establish state
	atf_check -s exit:0 -o ignore ${common_dir}/pft_ping.py \
		--ip6 \
		--sendif ${epair}a \
		--to 2001:db8::2 \
		--replyif ${epair}a

	# Change rules to now deny the ICMP traffic
	pft_set_rules noflush alcatraz "block all"
	if ! find_state_v6;
	then
		atf_fail "Setting new rules removed the state."
	fi

	# Killing with the wrong IP doesn't affect our state
	jexec alcatraz pfctl -k 2001:db8::3
	if ! find_state_v6;
	then
		atf_fail "Killing with the wrong IP removed our state."
	fi

	# Killing with one correct address and one incorrect doesn't kill the state
	jexec alcatraz pfctl -k 2001:db8::1 -k 2001:db8::3
	if ! find_state_v6;
	then
		atf_fail "Killing with one wrong IP removed our state."
	fi

	# Killing with correct address does remove the state
	jexec alcatraz pfctl -k 2001:db8::1
	if find_state_v6;
	then
		atf_fail "Killing with the correct IP did not remove our state."
	fi
}

v6_cleanup()
{
	pft_cleanup
}

atf_test_case "label" "cleanup"
label_head()
{
	atf_set descr 'Test killing states by label'
	atf_set require.user root
	atf_set require.progs scapy
}

label_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a 192.0.2.1/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up
	jexec alcatraz pfctl -e

	pft_set_rules alcatraz "block all" \
		"pass in proto tcp label bar" \
		"pass in proto icmp label foo"

	# Sanity check & establish state
	atf_check -s exit:0 -o ignore ${common_dir}/pft_ping.py \
		--sendif ${epair}a \
		--to 192.0.2.2 \
		--replyif ${epair}a

	# Change rules to now deny the ICMP traffic
	pft_set_rules noflush alcatraz "block all"
	if ! find_state;
	then
		atf_fail "Setting new rules removed the state."
	fi

	# Killing a label on a different rules keeps the state
	jexec alcatraz pfctl -k label -k bar
	if ! find_state;
	then
		atf_fail "Killing a different label removed the state."
	fi

	# Killing a non-existing label keeps the state
	jexec alcatraz pfctl -k label -k baz
	if ! find_state;
	then
		atf_fail "Killing a non-existing label removed the state."
	fi

	# Killing the correct label kills the state
	jexec alcatraz pfctl -k label -k foo
	if find_state;
	then
		atf_fail "Killing the state did not remove it."
	fi
}

label_cleanup()
{
	pft_cleanup
}

atf_test_case "multilabel" "cleanup"
multilabel_head()
{
	atf_set descr 'Test killing states with multiple labels by label'
	atf_set require.user root
	atf_set require.progs scapy
}

multilabel_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a 192.0.2.1/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up
	jexec alcatraz pfctl -e

	pft_set_rules alcatraz "block all" \
		"pass in proto icmp label foo label bar"

	# Sanity check & establish state
	atf_check -s exit:0 -o ignore ${common_dir}/pft_ping.py \
		--sendif ${epair}a \
		--to 192.0.2.2 \
		--replyif ${epair}a

	# Change rules to now deny the ICMP traffic
	pft_set_rules noflush alcatraz "block all"
	if ! find_state;
	then
		atf_fail "Setting new rules removed the state."
	fi

	# Killing a label on a different rules keeps the state
	jexec alcatraz pfctl -k label -k baz
	if ! find_state;
	then
		atf_fail "Killing a different label removed the state."
	fi

	# Killing the state with the last label works
	jexec alcatraz pfctl -k label -k bar
	if find_state;
	then
		atf_fail "Killing with the last label did not remove the state."
	fi

	pft_set_rules alcatraz "block all" \
		"pass in proto icmp label foo label bar"

	# Reestablish state
	atf_check -s exit:0 -o ignore ${common_dir}/pft_ping.py \
		--sendif ${epair}a \
		--to 192.0.2.2 \
		--replyif ${epair}a

	# Change rules to now deny the ICMP traffic
	pft_set_rules noflush alcatraz "block all"
	if ! find_state;
	then
		atf_fail "Setting new rules removed the state."
	fi

	# Killing with the first label works too
	jexec alcatraz pfctl -k label -k foo
	if find_state;
	then
		atf_fail "Killing with the first label did not remove the state."
	fi
}

multilabel_cleanup()
{
	pft_cleanup
}

atf_test_case "gateway" "cleanup"
gateway_head()
{
	atf_set descr 'Test killing states by route-to/reply-to address'
	atf_set require.user root
	atf_set require.progs scapy
}

gateway_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a 192.0.2.1/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up
	jexec alcatraz pfctl -e

	pft_set_rules alcatraz "block all" \
		"pass in reply-to (${epair}b 192.0.2.1) proto icmp"

	# Sanity check & establish state
	# Note: use pft_ping so we always use the same ID, so pf considers all
	# echo requests part of the same flow.
	atf_check -s exit:0 -o ignore ${common_dir}/pft_ping.py \
		--sendif ${epair}a \
		--to 192.0.2.2 \
		--replyif ${epair}a

	# Change rules to now deny the ICMP traffic
	pft_set_rules noflush alcatraz "block all"
	if ! find_state;
	then
		atf_fail "Setting new rules removed the state."
	fi

	# Killing with a different gateway does not affect our state
	jexec alcatraz pfctl -k gateway -k 192.0.2.2
	if ! find_state;
	then
		atf_fail "Killing with a different gateway removed the state."
	fi

	# Killing states with the relevant gateway does terminate our state
	jexec alcatraz pfctl -k gateway -k 192.0.2.1
	if find_state;
	then
		atf_fail "Killing with the gateway did not remove the state."
	fi
}

gateway_cleanup()
{
	pft_cleanup
}

atf_test_case "match" "cleanup"
match_head()
{
	atf_set descr 'Test killing matching states'
	atf_set require.user root
}

wait_for_state()
{
	jail=$1
	addr=$2

	while ! jexec $jail pfctl -s s | grep $addr >/dev/null;
	do
		sleep .1
	done
}

match_body()
{
	pft_init

	epair_one=$(vnet_mkepair)
	ifconfig ${epair_one}a 192.0.2.1/24 up

	epair_two=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair_one}b ${epair_two}a
	jexec alcatraz ifconfig ${epair_one}b 192.0.2.2/24 up
	jexec alcatraz ifconfig ${epair_two}a 198.51.100.1/24 up
	jexec alcatraz sysctl net.inet.ip.forwarding=1
	jexec alcatraz pfctl -e

	vnet_mkjail singsing ${epair_two}b
	jexec singsing ifconfig ${epair_two}b 198.51.100.2/24 up
	jexec singsing route add default 198.51.100.1
	jexec singsing /usr/sbin/inetd -p inetd-echo.pid \
	    $(atf_get_srcdir)/echo_inetd.conf

	route add 198.51.100.0/24 192.0.2.2

	pft_set_rules alcatraz \
		"nat on ${epair_two}a from 192.0.2.0/24 -> (${epair_two}a)" \
		"pass all"

	nc 198.51.100.2 7 &
	wait_for_state alcatraz 192.0.2.1

	# Expect two states
	states=$(jexec alcatraz pfctl -s s | grep 192.0.2.1 | wc -l)
	if [ $states -ne 2 ] ;
	then
		atf_fail "Expected two states, found $states"
	fi

	# If we don't kill the matching NAT state one should be left
	jexec alcatraz pfctl -k 192.0.2.1
	states=$(jexec alcatraz pfctl -s s | grep 192.0.2.1 | wc -l)
	if [ $states -ne 1 ] ;
	then
		atf_fail "Expected one states, found $states"
	fi

	# Flush
	jexec alcatraz pfctl -F states

	nc 198.51.100.2 7 &
	wait_for_state alcatraz 192.0.2.1

	# Kill matching states, expect all of them to be gone
	jexec alcatraz pfctl -M -k 192.0.2.1
	states=$(jexec alcatraz pfctl -s s | grep 192.0.2.1 | wc -l)
	if [ $states -ne 0 ] ;
	then
		atf_fail "Expected zero states, found $states"
	fi
}

match_cleanup()
{
	pft_cleanup
}

atf_test_case "interface" "cleanup"
interface_head()
{
	atf_set descr 'Test killing states based on interface'
	atf_set require.user root
	atf_set require.progs scapy
}

interface_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a 192.0.2.1/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up
	jexec alcatraz pfctl -e

	pft_set_rules alcatraz "block all" \
		"pass in proto icmp"

	# Sanity check & establish state
	atf_check -s exit:0 -o ignore ${common_dir}/pft_ping.py \
		--sendif ${epair}a \
		--to 192.0.2.2 \
		--replyif ${epair}a

	# Change rules to now deny the ICMP traffic
	pft_set_rules noflush alcatraz "block all"
	if ! find_state;
	then
		atf_fail "Setting new rules removed the state."
	fi

	# Flushing states on a different interface doesn't affect our state
	jexec alcatraz pfctl -i ${epair}a -Fs
	if ! find_state;
	then
		atf_fail "Flushing on a different interface removed the state."
	fi

	# Flushing on the correct interface does (even with floating states)
	jexec alcatraz pfctl -i ${epair}b -Fs
	if find_state;
	then
		atf_fail "Flushing on a the interface did not remove the state."
	fi
}

interface_cleanup()
{
	pft_cleanup
}

atf_test_case "id" "cleanup"
id_head()
{
	atf_set descr 'Test killing states by id'
	atf_set require.user root
	atf_set require.progs scapy
}

id_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a 192.0.2.1/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up
	jexec alcatraz pfctl -e

	pft_set_rules alcatraz "block all" \
		"pass in proto tcp" \
		"pass in proto icmp"

	# Sanity check & establish state
	atf_check -s exit:0 -o ignore ${common_dir}/pft_ping.py \
		--sendif ${epair}a \
		--to 192.0.2.2 \
		--replyif ${epair}a

	# Change rules to now deny the ICMP traffic
	pft_set_rules noflush alcatraz "block all"
	if ! find_state;
	then
		atf_fail "Setting new rules removed the state."
	fi

	# Get the state ID
	id=$(jexec alcatraz pfctl -ss -vvv | grep -A 3 icmp |
	    grep -A 3 192.0.2.2 | awk '/id:/ { printf("%s/%s", $2, $4); }')

	# Kill the wrong ID
	jexec alcatraz pfctl -k id -k 1
	if ! find_state;
	then
		atf_fail "Killing a different ID removed the state."
	fi

	# Kill the correct ID
	jexec alcatraz pfctl -k id -k ${id}
	if find_state;
	then
		atf_fail "Killing the state did not remove it."
	fi
}

id_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "v4"
	atf_add_test_case "v6"
	atf_add_test_case "label"
	atf_add_test_case "multilabel"
	atf_add_test_case "gateway"
	atf_add_test_case "match"
	atf_add_test_case "interface"
	atf_add_test_case "id"
}
