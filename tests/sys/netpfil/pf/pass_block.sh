#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018 Kristof Provost <kp@FreeBSD.org>
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

atf_test_case "enable_disable" "cleanup"
enable_disable_head()
{
	atf_set descr 'Test enable/disable'
	atf_set require.user root
}

enable_disable_body()
{
	pft_init

	j="pass_block:enable_disable"

	vnet_mkjail ${j}

	# Disable when disabled fails
	atf_check -s exit:1 -e ignore \
	    jexec ${j} pfctl -d

	# Enable succeeds
	atf_check -s exit:0 -e ignore \
	    jexec ${j} pfctl -e

	# Enable when enabled fails
	atf_check -s exit:1 -e ignore \
	    jexec ${j} pfctl -e

	# Disable succeeds
	atf_check -s exit:0 -e ignore \
	    jexec ${j} pfctl -d
}

enable_disable_cleanup()
{
	pft_cleanup
}

atf_test_case "v4" "cleanup"
v4_head()
{
	atf_set descr 'Basic pass/block test for IPv4'
	atf_set require.user root
}

v4_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a 192.0.2.1/24 up

	# Set up a simple jail with one interface
	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up

	# Trivial ping to the jail, without pf
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 192.0.2.2

	# pf without policy will let us ping
	jexec alcatraz pfctl -e
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 192.0.2.2

	# Block everything
	pft_set_rules alcatraz "block in"
	atf_check -s exit:2 -o ignore ping -c 1 -t 1 192.0.2.2

	# Block everything but ICMP
	pft_set_rules alcatraz "block in" "pass in proto icmp"
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 192.0.2.2
}

v4_cleanup()
{
	pft_cleanup
}

atf_test_case "v6" "cleanup"
v6_head()
{
	atf_set descr 'Basic pass/block test for IPv6'
	atf_set require.user root
}

v6_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a inet6 2001:db8:42::1/64 up no_dad

	# Set up a simple jail with one interface
	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b inet6 2001:db8:42::2/64 up no_dad

	# Trivial ping to the jail, without pf
	atf_check -s exit:0 -o ignore ping -6 -c 1 -W 1 2001:db8:42::2

	# pf without policy will let us ping
	jexec alcatraz pfctl -e
	atf_check -s exit:0 -o ignore ping -6 -c 1 -W 1 2001:db8:42::2

	# Block everything
	pft_set_rules alcatraz "block in"
	atf_check -s exit:2 -o ignore ping -6 -c 1 -W 1 2001:db8:42::2

	# Block everything but ICMP
	pft_set_rules alcatraz "block in" "pass in proto icmp6"
	atf_check -s exit:0 -o ignore ping -6 -c 1 -W 1 2001:db8:42::2

	# Allowing ICMPv4 does not allow ICMPv6
	pft_set_rules alcatraz "block in" "pass in proto icmp"
	atf_check -s exit:2 -o ignore ping -6 -c 1 -W 1 2001:db8:42::2
}

v6_cleanup()
{
	pft_cleanup
}

atf_test_case "noalias" "cleanup"
noalias_head()
{
	atf_set descr 'Test the :0 noalias option'
	atf_set require.user root
}

noalias_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a inet6 2001:db8:42::1/64 up no_dad

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b inet6 2001:db8:42::2/64 up no_dad

	linklocaladdr=$(jexec alcatraz ifconfig ${epair}b inet6 \
		| grep %${epair}b \
		| awk '{ print $2; }' \
		| cut -d % -f 1)

	# Sanity check
	atf_check -s exit:0 -o ignore ping -6 -c 3 -W 1 2001:db8:42::2
	atf_check -s exit:0 -o ignore ping -6 -c 3 -W 1 ${linklocaladdr}%${epair}a

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz "block out inet6 from (${epair}b:0) to any"

	atf_check -s exit:2 -o ignore ping -6 -c 3 -W 1 2001:db8:42::2

	# We should still be able to ping the link-local address
	atf_check -s exit:0 -o ignore ping -6 -c 3 -W 1 ${linklocaladdr}%${epair}a

	pft_set_rules alcatraz "block out inet6 from (${epair}b) to any"

	# We cannot ping to the link-local address
	atf_check -s exit:2 -o ignore ping -6 -c 3 -W 1 ${linklocaladdr}%${epair}a
}

noalias_cleanup()
{
	pft_cleanup
}

atf_test_case "nested_inline" "cleanup"
nested_inline_head()
{
	atf_set descr "Test nested inline anchors, PR196314"
	atf_set require.user root
}

nested_inline_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a inet 192.0.2.1/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"block in" \
		"anchor \"an1\" {" \
			"pass in quick proto tcp to port time" \
			"anchor \"an2\" {" \
				"pass in quick proto icmp" \
			"}" \
		"}"

	atf_check -s exit:0 -o ignore ping -c 1 -t 1 192.0.2.2
}

nested_inline_cleanup()
{
	pft_cleanup
}

atf_test_case "urpf" "cleanup"
urpf_head()
{
	atf_set descr "Test unicast reverse path forwarding check"
	atf_set require.user root
	atf_set require.progs scapy
}

urpf_body()
{
	pft_init

	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair_one}b ${epair_two}b

	ifconfig ${epair_one}a 192.0.2.2/24 up
	ifconfig ${epair_two}a 198.51.100.2/24 up

	jexec alcatraz ifconfig ${epair_one}b 192.0.2.1/24 up
	jexec alcatraz ifconfig ${epair_two}b 198.51.100.1/24 up
	jexec alcatraz sysctl net.inet.ip.forwarding=1

	# Sanity checks
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 192.0.2.1
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 198.51.100.1
	atf_check -s exit:0 ${common_dir}/pft_ping.py \
		--sendif ${epair_one}a \
		--to 192.0.2.1 \
		--fromaddr 198.51.100.2 \
		--replyif ${epair_two}a
	atf_check -s exit:0 ${common_dir}/pft_ping.py \
		--sendif ${epair_two}a \
		--to 198.51.100.1 \
		--fromaddr 192.0.2.2 \
		--replyif ${epair_one}a

	pft_set_rules alcatraz \
		"block quick from urpf-failed" \
		"set skip on lo"
	jexec alcatraz pfctl -e

	# Correct source still works
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 192.0.2.1
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 198.51.100.1

	# Unexpected source interface is blocked
	atf_check -s exit:1 ${common_dir}/pft_ping.py \
		--sendif ${epair_one}a \
		--to 192.0.2.1 \
		--fromaddr 198.51.100.2 \
		--replyif ${epair_two}a
	atf_check -s exit:1 ${common_dir}/pft_ping.py \
		--sendif ${epair_two}a \
		--to 198.51.100.1 \
		--fromaddr 192.0.2.2 \
		--replyif ${epair_one}a
}

urpf_cleanup()
{
	pft_cleanup
}

atf_test_case "received_on" "cleanup"
received_on_head()
{
	atf_set descr 'Test received-on filtering'
	atf_set require.user root
}

received_on_body()
{
	pft_init

	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)
	epair_route=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair_one}b ${epair_two}b ${epair_route}a
	vnet_mkjail srv ${epair_route}b

	ifconfig ${epair_one}a 192.0.2.2/24 up
	ifconfig ${epair_two}a 198.51.100.2/24 up
	route add 203.0.113.2 192.0.2.1
	route add 203.0.113.3 198.51.100.1

	jexec alcatraz ifconfig ${epair_one}b 192.0.2.1/24 up
	jexec alcatraz ifconfig ${epair_two}b 198.51.100.1/24 up
	jexec alcatraz ifconfig ${epair_route}a 203.0.113.1/24 up
	jexec alcatraz sysctl net.inet.ip.forwarding=1

	jexec srv ifconfig ${epair_route}b 203.0.113.2/24 up
	jexec srv ifconfig ${epair_route}b inet alias 203.0.113.3/24 up
	jexec srv route add default 203.0.113.1

	# Sanity checks
	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1
	atf_check -s exit:0 -o ignore \
	    ping -c 1 198.51.100.1
	atf_check -s exit:0 -o ignore \
	    ping -c 1 203.0.113.2
	atf_check -s exit:0 -o ignore \
	    ping -c 1 203.0.113.3

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
	    "block in" \
	    "pass received-on ${epair_one}b"

	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1
	atf_check -s exit:2 -o ignore \
	    ping -c 1 198.51.100.1

	# And ensure we can check the received-on interface after routing
	atf_check -s exit:0 -o ignore \
	    ping -c 1 203.0.113.2
	atf_check -s exit:2 -o ignore \
	    ping -c 1 203.0.113.3

	# Now try this with a group instead
	jexec alcatraz ifconfig ${epair_one}b group test
	pft_set_rules alcatraz \
	    "block in" \
	    "pass received-on test"

	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1
	atf_check -s exit:2 -o ignore \
	    ping -c 1 198.51.100.1

	# And ensure we can check the received-on interface after routing
	atf_check -s exit:0 -o ignore \
	    ping -c 1 203.0.113.2
	atf_check -s exit:2 -o ignore \
	    ping -c 1 203.0.113.3
}

received_on_cleanup()
{
	pft_cleanup
}

atf_test_case "optimize_any" "cleanup"
optimize_any_head()
{
	atf_set descr 'Test known optimizer bug'
	atf_set require.user root
}

optimize_any_body()
{
	pft_init

	epair=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair}a

	ifconfig ${epair}b 192.0.2.2/24 up

	jexec alcatraz ifconfig ${epair}a 192.0.2.1/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 -t 1 192.0.2.1

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
	    "block" \
	    "pass in inet from { any, 192.0.2.3 }"

	atf_check -s exit:0 -o ignore ping -c 1 -t 1 192.0.2.1
}

optimize_any_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "enable_disable"
	atf_add_test_case "v4"
	atf_add_test_case "v6"
	atf_add_test_case "noalias"
	atf_add_test_case "nested_inline"
	atf_add_test_case "urpf"
	atf_add_test_case "received_on"
	atf_add_test_case "optimize_any"
}
