#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2023 Rubicon Communications, LLC (Netgate)
# Copyright (c) 2024 Deciso B.V.
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

atf_test_case "malformed" "cleanup"
malformed_head()
{
	atf_set descr 'Test that we do not log malformed packets as passing'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

malformed_body()
{
	pflog_init

	epair=$(vnet_mkepair)

	vnet_mkjail srv ${epair}b
	jexec srv ifconfig ${epair}b 192.0.2.1/24 up

	vnet_mkjail cl ${epair}a
	jexec cl ifconfig ${epair}a 192.0.2.2/24 up

	jexec cl pfctl -e
	jexec cl ifconfig pflog0 up
	pft_set_rules cl \
		"pass log keep state"

	# Not required, but the 'pf: dropping packet with ip options' kernel log can
	# help when debugging the test.
	jexec cl pfctl -x loud

	jexec cl tcpdump -n -e -ttt --immediate-mode -l -U -i pflog0 >> pflog.txt &
	sleep 1 # Wait for tcpdump to start

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    jexec srv ping -c 1 192.0.2.2

	jexec srv ${common_dir}/pft_ping.py  \
	    --sendif ${epair}b \
	    --to 192.0.2.2 \
	    --send-nop \
	    --recvif ${epair}b

	atf_check -o match:".*rule 0/8\(ip-option\): block in on ${epair}a: 192.0.2.1 > 192.0.2.2: ICMP echo request.*" \
	    cat pflog.txt
}

malformed_cleanup()
{
	pft_cleanup
}

atf_test_case "matches" "cleanup"
matches_head()
{
	atf_set descr 'Test the pflog matches keyword'
	atf_set require.user root
}

matches_body()
{
	pflog_init

	epair=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair}a
	jexec alcatraz ifconfig ${epair}a 192.0.2.1/24 up

	ifconfig ${epair}b 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1

	jexec alcatraz pfctl -e
	jexec alcatraz ifconfig pflog0 up
	pft_set_rules alcatraz \
		"match log(matches) inet proto icmp" \
		"match log(matches) inet from 192.0.2.2" \
		"pass"

	jexec alcatraz tcpdump -n -e -ttt --immediate-mode -l -U -i pflog0 >> ${PWD}/pflog.txt &
	sleep 1 # Wait for tcpdump to start

	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1

	echo "Rules"
	jexec alcatraz pfctl -sr -vv
	echo "States"
	jexec alcatraz pfctl -ss -vv
	echo "Log"
	cat ${PWD}/pflog.txt

	atf_check -o match:".*rule 0/0\(match\): match in on ${epair}a: 192.0.2.2 > 192.0.2.1: ICMP echo request.*" \
	    cat pflog.txt
	atf_check -o match:".*rule 1/0\(match\): match in on ${epair}a: 192.0.2.2 > 192.0.2.1: ICMP echo request.*" \
	    cat pflog.txt
}

matches_cleanup()
{
	pft_cleanup
}

atf_test_case "matches_logif" "cleanup"
matches_logif_head()
{
	atf_set descr 'Test log(matches, to pflogX)'
	atf_set require.user root
}

matches_logif_body()
{
	pflog_init

	epair=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair}a
	jexec alcatraz ifconfig ${epair}a 192.0.2.1/24 up

	ifconfig ${epair}b 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1

	jexec alcatraz pfctl -e
	jexec alcatraz ifconfig pflog0 up
	jexec alcatraz ifconfig pflog1 create
	jexec alcatraz ifconfig pflog1 up
	pft_set_rules alcatraz \
		"match log(matches, to pflog1) inet proto icmp" \
		"match log inet from 192.0.2.2" \
		"pass log(to pflog0)"

	jexec alcatraz tcpdump -n -e -ttt --immediate-mode -l -U -i pflog1 >> ${PWD}/pflog1.txt &
	jexec alcatraz tcpdump -n -e -ttt --immediate-mode -l -U -i pflog0 >> ${PWD}/pflog0.txt &
	sleep 1 # Wait for tcpdump to start

	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1

	echo "Rules"
	jexec alcatraz pfctl -sr -vv
	echo "States"
	jexec alcatraz pfctl -ss -vv
	echo "Log 0"
	cat ${PWD}/pflog0.txt
	echo "Log 1"
	cat ${PWD}/pflog1.txt

	atf_check -o match:".*rule 0/0\(match\): match in on ${epair}a: 192.0.2.2 > 192.0.2.1: ICMP echo request.*" \
	    cat pflog1.txt
	atf_check -o match:".*rule 1/0\(match\): match in on ${epair}a: 192.0.2.2 > 192.0.2.1: ICMP echo request.*" \
	    cat pflog1.txt
}

matches_logif_cleanup()
{
	pft_cleanup
}

atf_test_case "state_max" "cleanup"
state_max_head()
{
	atf_set descr 'Ensure that drops due to state limits are logged'
	atf_set require.user root
}

state_max_body()
{
	pflog_init

	epair=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair}a
	jexec alcatraz ifconfig ${epair}a inet6 ifdisabled
	jexec alcatraz ifconfig ${epair}a 192.0.2.1/24 up

	ifconfig ${epair}b 192.0.2.2/24 up
	ifconfig ${epair}b inet6 ifdisabled

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1

	jexec alcatraz pfctl -e
	jexec alcatraz ifconfig pflog0 up
	pft_set_rules alcatraz "pass log inet keep state (max 1)"

	jexec alcatraz tcpdump -n -e -ttt --immediate-mode -l -U -i pflog0 >> ${PWD}/pflog.txt &
	sleep 1 # Wait for tcpdump to start

	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1

	atf_check -s exit:2 -o ignore \
	    ping -c 1 192.0.2.1

	echo "Rules"
	jexec alcatraz pfctl -sr -vv
	echo "States"
	jexec alcatraz pfctl -ss -vv
	echo "Log"
	cat ${PWD}/pflog.txt

	# First ping passes.
	atf_check -o match:".*rule 0/0\(match\): pass in on ${epair}a: 192.0.2.2 > 192.0.2.1: ICMP echo request.*" \
	    cat pflog.txt

	# Second ping is blocked due to the state limit.
	atf_check -o match:".*rule 0/12\(state-limit\): block in on ${epair}a: 192.0.2.2 > 192.0.2.1: ICMP echo request.*" \
	    cat pflog.txt

	# At most three lines should be written: one for the first ping, and
	# two for the second: one for the initial pass through the ruleset, and
	# then a drop because of the state limit.  Ideally only the drop would
	# be logged; if this is fixed, the count will be 2 instead of 3.
	atf_check -o match:3 grep -c . pflog.txt

	# If the rule doesn't specify logging, we shouldn't log drops
	# due to state limits.
	pft_set_rules alcatraz "pass inet keep state (max 1)"

	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1

	atf_check -s exit:2 -o ignore \
	    ping -c 1 192.0.2.1

	atf_check -o match:3 grep -c . pflog.txt
}

state_max_cleanup()
{
	pft_cleanup
}

atf_test_case "unspecified_v4" "cleanup"
unspecified_v4_head()
{
	atf_set descr 'Ensure that packets to the unspecified address are visible to pfil hooks'
	atf_set require.user root
}

unspecified_v4_body()
{
	pflog_init

	vnet_mkjail alcatraz
	jexec alcatraz ifconfig lo0 inet 127.0.0.1
	jexec alcatraz route add default 127.0.0.1

	jexec alcatraz pfctl -e
	jexec alcatraz ifconfig pflog0 up
	pft_set_rules alcatraz "block log on lo0 to 0.0.0.0"

	jexec alcatraz tcpdump -n -e -ttt --immediate-mode -l -U -i pflog0 >> pflog.txt &
	sleep 1 # Wait for tcpdump to start

	atf_check -s not-exit:0 -o ignore -e ignore \
	    jexec alcatraz ping -S 127.0.0.1 -c 1 0.0.0.0

	atf_check -o match:".*: block out on lo0: 127.0.0.1 > 0.0.0.0: ICMP echo request,.*" \
	    cat pflog.txt
}

unspecified_v4_cleanup()
{
	pft_cleanup
}

atf_test_case "unspecified_v6" "cleanup"
unspecified_v6_head()
{
	atf_set descr 'Ensure that packets to the unspecified address are visible to pfil hooks'
	atf_set require.user root
}

unspecified_v6_body()
{
	pflog_init

	vnet_mkjail alcatraz
	jexec alcatraz ifconfig lo0 up
	jexec alcatraz route -6 add ::0 ::1

	jexec alcatraz pfctl -e
	jexec alcatraz ifconfig pflog0 up
	pft_set_rules alcatraz "block log on lo0 to ::0"

	jexec alcatraz tcpdump -n -e -ttt --immediate-mode -l -U -i pflog0 >> pflog.txt &
	sleep 1 # Wait for tcpdump to start

	atf_check -s not-exit:0 -o ignore -e ignore \
	    jexec alcatraz ping -6 -S ::1 -c 1 ::0

	cat pflog.txt
	atf_check -o match:".*: block out on lo0: ::1 > ::: ICMP6, echo request,.*" \
	    cat pflog.txt
}

unspecified_v6_cleanup()
{
	pft_cleanup
}

atf_test_case "rdr_action" "cleanup"
rdr_head()
{
	atf_set descr 'Ensure that NAT rule actions are logged correctly'
	atf_set require.user root
}

rdr_action_body()
{
	pflog_init

	j="pflog:rdr_action"
	epair_c=$(vnet_mkepair)
	epair_srv=$(vnet_mkepair)

	vnet_mkjail ${j}srv ${epair_srv}a
	vnet_mkjail ${j}gw ${epair_srv}b ${epair_c}a
	vnet_mkjail ${j}c ${epair_c}b

	jexec ${j}srv ifconfig ${epair_srv}a inet6 ifdisabled
	jexec ${j}gw ifconfig ${epair_srv}b inet6 ifdisabled
	jexec ${j}gw ifconfig ${epair_c}a inet6 ifdisabled
	jexec ${j}c ifconfig ${epair_c}b inet6 ifdisabled

	jexec ${j}srv ifconfig ${epair_srv}a 198.51.100.1/24 up
	# No default route in srv jail, to ensure we're NAT-ing
	jexec ${j}gw ifconfig ${epair_srv}b 198.51.100.2/24 up
	jexec ${j}gw ifconfig ${epair_c}a 192.0.2.1/24 up
	jexec ${j}gw sysctl net.inet.ip.forwarding=1
	jexec ${j}c ifconfig ${epair_c}b 192.0.2.2/24 up
	jexec ${j}c route add default 192.0.2.1

	jexec ${j}gw pfctl -e
	jexec ${j}gw ifconfig pflog0 up
	pft_set_rules ${j}gw \
		"rdr log on ${epair_srv}b proto tcp from 198.51.100.0/24 to any port 1234 -> 192.0.2.2 port 1234" \
		"block quick inet6" \
		"pass in log"

	jexec ${j}gw tcpdump -n -e -ttt --immediate-mode -l -U -i pflog0 >> ${PWD}/pflog.txt &
	sleep 1 # Wait for tcpdump to start

	# send a SYN to catch in the log
	jexec ${j}srv nc -N -w 0 198.51.100.2 1234

	echo "Log"
	cat ${PWD}/pflog.txt

	# log line generated for rdr hit (pre-NAT)
	atf_check -o match:".*.*rule 0/0\(match\): rdr in on ${epair_srv}b: 198.51.100.1.[0-9]* > 198.51.100.2.1234: Flags \[S\].*" \
	    cat pflog.txt

	# log line generated for pass hit (post-NAT)
	atf_check -o match:".*.*rule 1/0\(match\): pass in on ${epair_srv}b: 198.51.100.1.[0-9]* > 192.0.2.2.1234: Flags \[S\].*" \
	    cat pflog.txt

	# only two log lines shall be written
	atf_check -o match:2 grep -c . pflog.txt
}

rdr_action_cleanup()
{
	pft_cleanup
}

atf_test_case "rule_number" "cleanup"
rule_number_head()
{
	atf_set descr 'Test rule numbers with anchors'
	atf_set require.user root
}

rule_number_body()
{
	pflog_init

	epair=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.1/24 up

	ifconfig ${epair}a 192.0.2.2/24 up
	ifconfig ${epair}a inet alias 192.0.2.3/24 up
	ifconfig ${epair}a inet alias 192.0.2.4/24 up

	jexec alcatraz pfctl -e
	jexec alcatraz ifconfig pflog0 up
	pft_set_rules alcatraz \
		"pass log from 192.0.2.2" \
		"anchor \"foo\" {\n \
			pass log from 192.0.2.3\n \
		}" \
		"pass log from 192.0.2.4"

	jexec alcatraz tcpdump -n -e -ttt --immediate-mode -l -U -i pflog0 >> pflog.txt &
	sleep 1 # Wait for tcpdump to start

	atf_check -s exit:0 -o ignore \
	    ping -c 1 -S 192.0.2.2 192.0.2.1
	atf_check -s exit:0 -o ignore \
	    ping -c 1 -S 192.0.2.3 192.0.2.1
	atf_check -s exit:0 -o ignore \
	    ping -c 1 -S 192.0.2.4 192.0.2.1

	jexec alcatraz pfctl -sr -a '*' -vv

	# Give tcpdump a little time to finish writing to the file
	sleep 1
	cat pflog.txt

	atf_check -o match:"rule 0/0\(match\): pass in.*: 192.0.2.2.*ICMP echo request" \
	    cat pflog.txt
	atf_check -o match:"rule 1.foo.0/0\(match\): pass in.*: 192.0.2.3.*: ICMP echo request" \
	    cat pflog.txt
	atf_check -o match:"rule 2/0\(match\): pass in.*: 192.0.2.4.*: ICMP echo request" \
	    cat pflog.txt
}

rule_number_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "malformed"
	atf_add_test_case "matches"
	atf_add_test_case "matches_logif"
	atf_add_test_case "state_max"
	atf_add_test_case "unspecified_v4"
	atf_add_test_case "unspecified_v6"
	atf_add_test_case "rdr_action"
	atf_add_test_case "rule_number"
}
