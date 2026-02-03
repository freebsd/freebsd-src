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

common_dir=$(atf_get_srcdir)/../common

atf_test_case "dummynet" "cleanup"
dummynet_head()
{
	atf_set descr 'Test dummynet with match keyword'
	atf_set require.user root
}

dummynet_body()
{
	dummynet_init

	epair=$(vnet_mkepair)
	vnet_mkjail alcatraz ${epair}b

	ifconfig ${epair}a 192.0.2.1/24 up
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore ping -i .1 -c 3 -s 1200 192.0.2.2

	jexec alcatraz dnctl pipe 1 config bw 30Byte/s
	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"match in dnpipe 1" \
		"pass"

	# single ping succeeds just fine
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	# Saturate the link
	ping -i .1 -c 5 -s 1200 192.0.2.2

	# We should now be hitting the limits and get this packet dropped.
	atf_check -s exit:2 -o ignore ping -c 1 -s 1200 192.0.2.2
}

dummynet_cleanup()
{
	pft_cleanup
}

atf_test_case "quick" "cleanup"
quick_head()
{
	atf_set descr 'Test quick on match rules'
	atf_set require.user root
}

quick_body()
{
	pft_init

	epair=$(vnet_mkepair)
	vnet_mkjail alcatraz ${epair}b

	ifconfig ${epair}a 192.0.2.1/24 up
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.2

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"pass" \
		"match in quick proto icmp" \
		"block"

	# 'match quick' should retain the previous pass/block state
	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.2

	pft_set_rules alcatraz \
		"block" \
		"match in quick proto icmp" \
		"pass"

	atf_check -s exit:2 -o ignore \
	    ping -c 1 192.0.2.2
}

quick_cleanup()
{
	pft_cleanup
}

atf_test_case "allow_opts" "cleanup"
allow_opts_head()
{
	atf_set descr 'Test allowing IP options via match'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

allow_opts_body()
{
	pft_init

	epair=$(vnet_mkepair)

	ifconfig ${epair}b 192.0.2.2/24 up

	vnet_mkjail alcatraz ${epair}a
	jexec alcatraz ifconfig ${epair}a 192.0.2.1/24 up

	jexec alcatraz pfctl -e
	jexec alcatraz pfctl -x loud
	pft_set_rules alcatraz \
	    "match proto icmp allow-opts" \
	    "pass"

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1

	atf_check -s exit:0 -o ignore \
	    ${common_dir}/pft_ping.py  \
	    --sendif ${epair}b \
	    --to 192.0.2.1 \
	    --send-nop \
	    --replyif ${epair}b

	# This doesn't work without 'allow-opts'
	pft_set_rules alcatraz \
	    "match proto icmp" \
	    "pass"
	atf_check -s exit:1 -o ignore \
	    ${common_dir}/pft_ping.py  \
	    --sendif ${epair}b \
	    --to 192.0.2.1 \
	    --send-nop \
	    --replyif ${epair}b

	# Setting it on a pass rule still works.
	pft_set_rules alcatraz \
	    "pass allow-opts"
	atf_check -s exit:0 -o ignore \
	    ${common_dir}/pft_ping.py  \
	    --sendif ${epair}b \
	    --to 192.0.2.1 \
	    --send-nop \
	    --replyif ${epair}b
}

allow_opts_cleanup()
{
	pft_cleanup
}

atf_test_case "double_match" "cleanup"
double_match_head()
{
	atf_set descr 'Test two match statements in separate anchors'
	atf_set require.user root
}

double_match_body()
{
	pft_init

	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)
	vnet_mkjail rtr ${epair_one}a ${epair_two}a
	vnet_mkjail srv ${epair_two}b

	ifconfig ${epair_one}b 192.0.2.2/24 up

	jexec rtr ifconfig ${epair_one}a 192.0.2.1/24 up
	jexec rtr ifconfig ${epair_two}a 198.51.100.1/24 up
	jexec rtr sysctl net.inet.ip.forwarding=1

	jexec srv ifconfig ${epair_two}b 198.51.100.2/24 up

	route add default 192.0.2.1

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1

	jexec rtr pfctl -e
	pft_set_rules rtr \
		"nat on ${epair_two}a from 192.0.2.0/24 to any -> (${epair_two}a)" \
		"block all" \
		"anchor \"userrules\" all {\n \
		anchor \"one\" all { \n\
		    match in tag \"allow\"\n\
		}\n\
		anchor \"two\" all { \n\
		    match tag \"allow\"\n\
		}\n\
		}\n" \
		"pass quick tagged \"allow\""

	atf_check -s exit:0 -o ignore \
	    ping -c 1 198.51.100.2

	jexec rtr pfctl -ss -vv
	jexec rtr pfctl -sr -vv -a "*"
	jexec rtr pfctl -sr -a "*"
}

double_match_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "dummynet"
	atf_add_test_case "quick"
	atf_add_test_case "allow_opts"
	atf_add_test_case "double_match"
}
