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
	atf_set require.progs scapy
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
	jexec alcatraz ifconfig ${epair}a 192.0.2.1/24 up

	ifconfig ${epair}b 192.0.2.2/24 up

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
	atf_check -o match:".*rule 0/0\(match\): block in on ${epair}a: 192.0.2.2 > 192.0.2.1: ICMP echo request.*" \
	    cat pflog.txt

	# At most three lines should be written: one for the first ping, and
	# two for the second: one for the initial pass through the ruleset, and
	# then a drop because of the state limit.  Ideally only the drop would
	# be logged; if this is fixed, the count will be 2 instead of 3.
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

atf_init_test_cases()
{
	atf_add_test_case "malformed"
	atf_add_test_case "matches"
	atf_add_test_case "state_max"
	atf_add_test_case "unspecified_v4"
	atf_add_test_case "unspecified_v6"
}
