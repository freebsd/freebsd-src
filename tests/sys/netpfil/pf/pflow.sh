#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2023 Rubicon Communications, LLC (Netgate)
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

atf_test_case "basic" "cleanup"
basic_head()
{
	atf_set descr 'Basic pflow test'
	atf_set require.user root
}

basic_body()
{
	pflow_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a 192.0.2.2/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.1/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.1

	pflow=$(jexec alcatraz pflowctl -c)

	# Reject invalid flow destinations
	atf_check -s exit:1 -e ignore \
	    jexec alcatraz pflowctl -s ${pflow} dst 256.0.0.1:4000
	atf_check -s exit:1 -e ignore \
	    jexec alcatraz pflowctl -s ${pflow} dst 192.0.0.2:400000

	# A valid destination is accepted
	atf_check -s exit:0 \
	    jexec alcatraz pflowctl -s ${pflow} dst 192.0.2.2:4000

	# Reject invalid version numbers
	atf_check -s exit:1 -e ignore \
	    jexec alcatraz pflowctl -s ${pflow} proto 9

	# Valid version passes
	atf_check -s exit:0 \
	    jexec alcatraz pflowctl -s ${pflow} proto 5
	atf_check -s exit:0 \
	    jexec alcatraz pflowctl -s ${pflow} proto 10

	# We can change the observation domain
	atf_check -s exit:0 \
	    jexec alcatraz pflowctl -s ${pflow} domain 13
	atf_check -s exit:0 -o match:".*domain 13.*" \
	    jexec alcatraz pflowctl -l
}

basic_cleanup()
{
	pft_cleanup
}

atf_test_case "state_defaults" "cleanup"
state_defaults_head()
{
	atf_set descr 'Test set state-defaults pflow'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

state_defaults_body()
{
	pflow_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a 192.0.2.2/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.1/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.1

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"pass"

	pflow=$(jexec alcatraz pflowctl -c)
	jexec alcatraz pflowctl -s ${pflow} dst 192.0.2.2:2055

	# No flow data is generated because no states are marked for it.
	ping -c 1 192.0.2.1
	# Flush states to force pflow creation
	jexec alcatraz pfctl -Fstates

	atf_check -o match:"No data" \
	    $(atf_get_srcdir)/pft_read_ipfix.py --recvif ${epair}a --port 2055

	# Expect pflow output with state-defaults pflow
	pft_set_rules alcatraz \
		"set state-defaults pflow" \
		"pass"

	ping -c 1 192.0.2.1

	# We default to version 5
	atf_check -o match:".*v=5.*" \
	    $(atf_get_srcdir)/pft_read_ipfix.py --recvif ${epair}a --port 2055

	# Switch to version 10
	jexec alcatraz pflowctl -s ${pflow} proto 10

	ping -c 1 192.0.2.1

	atf_check -o match:".*v=10.*" \
	    $(atf_get_srcdir)/pft_read_ipfix.py --recvif ${epair}a --port 2055
}

state_defaults_cleanup()
{
	pft_cleanup
}

atf_test_case "v6" "cleanup"
v6_head()
{
	atf_set descr 'Test pflow over IPv6'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

v6_body()
{
	pflow_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a inet6 2001:db8::2/64 up no_dad

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b inet6 2001:db8::1/64 up no_dad

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    ping -6 -c 1 2001:db8::1

	pflow=$(jexec alcatraz pflowctl -c )
	# Note proto 10, because there's no IPv6 information in v5
	jexec alcatraz pflowctl -s ${pflow} dst [2001:db8::2]:2055 proto 10

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"set state-defaults pflow" \
		"pass"

	ping -6 -c 1 2001:db8::1

	atf_check -o match:".*v=10,IPv6,proto=58,src=2001:db8::2,dst=2001:db8::1.*" \
	    $(atf_get_srcdir)/pft_read_ipfix.py --recvif ${epair}a --port 2055
}

v6_cleanup()
{
	pft_cleanup
}

atf_test_case "nat" "cleanup"
nat_head()
{
	atf_set descr 'Test pflow export for NAT44'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

nat_body()
{
	pflow_init

	epair=$(vnet_mkepair)
	epair_srv=$(vnet_mkepair)

	vnet_mkjail srv ${epair_srv}a
	vnet_mkjail rtr ${epair_srv}b ${epair}a

	ifconfig ${epair}b 192.0.2.2/24 up

	jexec srv ifconfig ${epair_srv}a 198.51.100.2/24 up
	jexec srv route add default 198.51.100.1

	jexec rtr ifconfig ${epair_srv}b 198.51.100.1/24 up
	jexec rtr ifconfig ${epair}a 192.0.2.1/24 up
	jexec rtr sysctl net.inet.ip.forwarding=1

	route add -net 198.51.100.0/24 192.0.2.1

	jexec rtr pfctl -e
	pft_set_rules rtr \
		"set state-defaults pflow" \
		"nat pass on ${epair_srv}b inet from 192.0.2.0/24 to any -> (${epair_srv}b)" \
		"pass"

	pflow=$(jexec rtr pflowctl -c)
	jexec rtr pflowctl -s ${pflow} dst 192.0.2.2:2055 proto 10

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    ping -c 1 198.51.100.2

	atf_check -o match:".*v=10,NAT=4,proto=1,src=192.0.2.2-198.51.100.1.*" \
	    $(atf_get_srcdir)/pft_read_ipfix.py --recvif ${epair}b --port 2055
}

nat_cleanup()
{
	pft_cleanup
}

atf_test_case "rule" "cleanup"
rule_head()
{
	atf_set descr 'Test per-rule pflow option'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

rule_body()
{
	pflow_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a 192.0.2.2/24 up
	ifconfig ${epair}a alias 192.0.2.3/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.1/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.1

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"pass in from 192.0.2.2 keep state (pflow)" \
		"pass in from 192.0.2.3 keep state"

	pflow=$(jexec alcatraz pflowctl -c)
	jexec alcatraz pflowctl -s ${pflow} dst 192.0.2.2:2055

	# No flow is generated if we ping from 192.0.2.3
	ping -c 1 -S 192.0.2.3 192.0.2.1

	atf_check -o match:"No data" \
	    $(atf_get_srcdir)/pft_read_ipfix.py --recvif ${epair}a --port 2055

	# But there is one if we ping from 192.0.2.2
	ping -c 1 -S 192.0.2.2 192.0.2.1

	atf_check -o match:".*v=5.*" \
	    $(atf_get_srcdir)/pft_read_ipfix.py --recvif ${epair}a --port 2055
}

rule_cleanup()
{
	pft_cleanup
}

atf_test_case "max_entries" "cleanup"
max_entries_head()
{
	atf_set descr 'Test that we can only create X pflow senders'
	atf_set require.user root
}

max_entries_body()
{
	pflow_init

	vnet_mkjail alcatraz

	for i in `seq 1 128`
	do
		atf_check -s exit:0 -o ignore \
		    jexec alcatraz pflowctl -c
	done

	# We cannot create the 129th pflow sender
	atf_check -s exit:1 -o ignore -e ignore \
	    jexec alcatraz pflowctl -c

	jexec alcatraz pflowctl -l
}

max_entries_cleanup()
{
	pft_cleanup
}

atf_test_case "obs_dom" "cleanup"
obs_dom_head()
{
	atf_set descr 'Test configuring observation domain values'
	atf_set require.user root
}

obs_dom_body()
{
	pflow_init

	vnet_mkjail alcatraz

	pflow=$(jexec alcatraz pflowctl -c)
	jexec alcatraz pflowctl -s ${pflow} domain 2300000000
	atf_check -o match:".*domain 2300000000.*" -s exit:0 \
	    jexec alcatraz pflowctl -l
}

obs_dom_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "basic"
	atf_add_test_case "state_defaults"
	atf_add_test_case "v6"
	atf_add_test_case "nat"
	atf_add_test_case "rule"
	atf_add_test_case "max_entries"
	atf_add_test_case "obs_dom"
}
