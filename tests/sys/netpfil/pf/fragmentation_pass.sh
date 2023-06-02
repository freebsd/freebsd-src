# $FreeBSD$
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2017 Kristof Provost <kp@FreeBSD.org>
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

atf_test_case "too_many_fragments" "cleanup"

too_many_fragments_head()
{
	atf_set descr 'IPv4 fragment limitation test'
	atf_set require.user root
}

too_many_fragments_body()
{
	pft_init

	epair=$(vnet_mkepair)
	vnet_mkjail alcatraz ${epair}a

	ifconfig ${epair}b inet 192.0.2.1/24 up
	jexec alcatraz ifconfig ${epair}a 192.0.2.2/24 up

	ifconfig ${epair}b mtu 200
	jexec alcatraz ifconfig ${epair}a mtu 200

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"set reassemble yes" \
		"pass keep state"

	# So we know pf is limiting things
	jexec alcatraz sysctl net.inet.ip.maxfragsperpacket=1024

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	# We can ping with < 64 fragments
	atf_check -s exit:0 -o ignore ping -c 1 -s 800 192.0.2.2

	# Too many fragments should fail
	atf_check -s exit:2 -o ignore ping -c 1 -s 20000 192.0.2.2
}

too_many_fragments_cleanup()
{
	pft_cleanup
}

atf_test_case "v6" "cleanup"
v6_head()
{
	atf_set descr 'IPv6 fragmentation test'
	atf_set require.user root
	atf_set require.progs scapy
}

v6_body()
{
	pft_init

	epair_send=$(vnet_mkepair)
	epair_link=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair_send}b ${epair_link}a
	vnet_mkjail singsing ${epair_link}b

	ifconfig ${epair_send}a inet6 2001:db8:42::1/64 no_dad up

	jexec alcatraz ifconfig ${epair_send}b inet6 2001:db8:42::2/64 no_dad up
	jexec alcatraz ifconfig ${epair_link}a inet6 2001:db8:43::2/64 no_dad up
	jexec alcatraz sysctl net.inet6.ip6.forwarding=1

	jexec singsing ifconfig ${epair_link}b inet6 2001:db8:43::3/64 no_dad up
	jexec singsing route add -6 2001:db8:42::/64 2001:db8:43::2
	route add -6 2001:db8:43::/64 2001:db8:42::2

	jexec alcatraz ifconfig ${epair_send}b inet6 -ifdisabled
	jexec alcatraz ifconfig ${epair_link}a inet6 -ifdisabled
	jexec singsing ifconfig ${epair_link}b inet6 -ifdisabled
	ifconfig ${epair_send}a inet6 -ifdisabled

	ifconfig ${epair_send}a
	jexec alcatraz ifconfig ${epair_send}b
	lladdr=$(jexec alcatraz ifconfig ${epair_send}b | awk '/ scopeid / { print($2); }' | cut -f 1 -d %)

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"set reassemble yes" \
		"pass keep state" \
		"block in" \
		"pass in inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
		"pass in inet6 proto icmp6 icmp6-type { echoreq, echorep }"

	# Host test
	atf_check -s exit:0 -o ignore \
		ping -6 -c 1 2001:db8:42::2

	atf_check -s exit:0 -o ignore \
		ping -6 -c 1 -s 4500 2001:db8:42::2

	atf_check -s exit:0 -o ignore\
		ping -6 -c 1 -b 70000 -s 65000 2001:db8:42::2

	# Force an NDP lookup
	ping -6 -c 1 ${lladdr}%${epair_send}a

	atf_check -s exit:0 -o ignore\
		ping -6 -c 1 -b 70000 -s 65000 ${lladdr}%${epair_send}a

	# Forwarding test
	atf_check -s exit:0 -o ignore \
		ping -6 -c 1 2001:db8:43::3

	atf_check -s exit:0 -o ignore \
		ping -6 -c 1 -s 4500 2001:db8:43::3

	atf_check -s exit:0 -o ignore\
		ping -6 -c 1 -b 70000 -s 65000 2001:db8:43::3

	$(atf_get_srcdir)/CVE-2019-5597.py \
		${epair_send}a \
		2001:db8:42::1 \
		2001:db8:43::3
}

v6_cleanup()
{
	pft_cleanup
}

atf_test_case "mtu_diff" "cleanup"
mtu_diff_head()
{
	atf_set descr 'Test reassembly across different MTUs, PR #255432'
	atf_set require.user root
}

mtu_diff_body()
{
	pft_init

	epair_small=$(vnet_mkepair)
	epair_large=$(vnet_mkepair)

	vnet_mkjail first ${epair_small}b ${epair_large}a
	vnet_mkjail second ${epair_large}b

	ifconfig ${epair_small}a 192.0.2.1/25 up
	jexec first ifconfig ${epair_small}b 192.0.2.2/25 up

	jexec first sysctl net.inet.ip.forwarding=1
	jexec first ifconfig ${epair_large}a 192.0.2.130/25 up
	jexec first ifconfig ${epair_large}a mtu 9000
	jexec second ifconfig ${epair_large}b 192.0.2.131/25 up
	jexec second ifconfig ${epair_large}b mtu 9000
	jexec second route add default 192.0.2.130

	route add 192.0.2.128/25 192.0.2.2

	jexec first pfctl -e
	pft_set_rules first \
		"set reassemble yes" \
		"pass keep state"

	# Sanity checks
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.130
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.131

	# Large packet that'll get reassembled and sent out in one on the large
	# epair
	atf_check -s exit:0 -o ignore ping -c 1 -s 8000 192.0.2.131
}

mtu_diff_cleanup()
{
	pft_cleanup
}

frag_common()
{
	name=$1

	pft_init

	epair=$(vnet_mkepair)
	vnet_mkjail alcatraz ${epair}a

	ifconfig ${epair}b inet 192.0.2.1/24 up
	jexec alcatraz ifconfig ${epair}a 192.0.2.2/24 up

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"set reassemble yes" \
		"pass keep state"

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	atf_check -s exit:0 -o ignore $(atf_get_srcdir)/frag-${1}.py \
		--to 192.0.2.2 \
		--fromaddr 192.0.2.1 \
		--sendif ${epair}b \
		--recvif ${epair}b
}

atf_test_case "overreplace" "cleanup"
overreplace_head()
{
	atf_set descr 'ping fragment that overlaps fragment at index boundary and replace it'
	atf_set require.user root
	atf_set require.progs scapy
}

overreplace_body()
{
	frag_common overreplace
}

overreplace_cleanup()
{
	pft_cleanup
}

atf_test_case "overindex" "cleanup"
overindex_head()
{
	atf_set descr 'ping fragment that overlaps the first fragment at index boundary'
	atf_set require.user root
	atf_set require.progs scapy
}

overindex_body()
{
	frag_common overindex
}

overindex_cleanup()
{
	pft_cleanup
}

atf_test_case "overlimit" "cleanup"
overlimit_head()
{
	atf_set descr 'ping fragment at index boundary that cannot be requeued'
	atf_set require.user root
	atf_set require.progs scapy
}

overlimit_body()
{
	frag_common overlimit
}

overlimit_cleanup()
{
	pft_cleanup
}

atf_test_case "reassemble" "cleanup"
reassemble_head()
{
	atf_set descr 'Test reassembly'
	atf_set require.user root
}

reassemble_body()
{
	pft_init

	epair=$(vnet_mkepair)
	vnet_mkjail alcatraz ${epair}a

	ifconfig ${epair}b inet 192.0.2.1/24 up
	jexec alcatraz ifconfig ${epair}a 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"pass out" \
		"block in" \
		"pass in inet proto icmp all icmp-type echoreq"

	# Single fragment passes
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	# But a fragmented ping does not
	atf_check -s exit:2 -o ignore ping -c 1 -s 2000 192.0.2.2

	pft_set_rules alcatraz \
		"set reassemble yes" \
		"pass out" \
		"block in" \
		"pass in inet proto icmp all icmp-type echoreq"

	# Both single packet & fragmented pass when we scrub
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2
	atf_check -s exit:0 -o ignore ping -c 1 -s 2000 192.0.2.2
}

reassemble_cleanup()
{
	pft_cleanup
}

atf_test_case "no_df" "cleanup"
no_df_head()
{
	atf_set descr 'Test removing of DF flag'
	atf_set require.user root
}

no_df_body()
{
	setup_router_server_ipv4

	ifconfig ${epair_tester}a mtu 9000
	jexec router ifconfig ${epair_tester}b mtu 9000
	jexec router ifconfig ${epair_server}a mtu 1500
	jexec server ifconfig ${epair_server}b mtu 1500

	# Sanity check.
	ping_server_check_reply exit:0 --ping-type=icmp

	pft_set_rules router \
		"set reassemble no" \
		"pass out" \
		"block in" \
		"pass in inet proto icmp all icmp-type echoreq"

	# Ping with normal, fragmentable packets.
	ping_server_check_reply exit:1 --ping-type=icmp --send-length=2000

	pft_set_rules router \
		"set reassemble yes" \
		"pass out" \
		"block in" \
		"pass in inet proto icmp all icmp-type echoreq"

	# Ping with normal, fragmentable packets.
	ping_server_check_reply exit:0 --ping-type=icmp --send-length=2000

	# Ping with non-fragmentable packets.
	ping_server_check_reply exit:1 --ping-type=icmp --send-length=2000 --send-flags DF

	pft_set_rules router \
		"set reassemble yes no-df" \
		"pass out" \
		"block in" \
		"pass in inet proto icmp all icmp-type echoreq"

	# Ping with non-fragmentable packets again.
	# This time pf will strip the DF flag.
	ping_server_check_reply exit:0 --ping-type=icmp --send-length=2000 --send-flags DF
}
no_df_cleanup()
{
	pft_cleanup
}

atf_test_case "no_df" "cleanup"
no_df_head()
{
	atf_set descr 'Test removing of DF flag'
	atf_set require.user root
}

no_df_body()
{
	setup_router_server_ipv4

	# Tester can send long packets which will get fragmented by the router.
	# Replies from server will come in fragments which might get
	# reassembled resulting in a long reply packet sent back to tester.
	ifconfig ${epair_tester}a mtu 9000
	jexec router ifconfig ${epair_tester}b mtu 9000
	jexec router ifconfig ${epair_server}a mtu 1500
	jexec server ifconfig ${epair_server}b mtu 1500

	# Sanity check.
	ping_server_check_reply exit:0 --ping-type=icmp

	# Enable packet reassembly with clearing of the no-df flag.
	pft_set_rules router \
		"scrub all fragment reassemble no-df" \
		"block" \
		"pass inet proto icmp all icmp-type echoreq"
	# Ping with non-fragmentable packets.
	# pf will strip the DF flag resulting in fragmentation and packets
	# getting properly forwarded.
	ping_server_check_reply exit:0 --ping-type=icmp --send-length=2000 --send-flags DF
}
no_df_cleanup()
{
	pft_cleanup
}

atf_test_case "reassemble_slowpath" "cleanup"
reassemble_slowpath_head()
{
	atf_set descr 'Test reassembly on the slow path'
	atf_set require.user root
}

reassemble_slowpath_body()
{
	if ! sysctl -q kern.features.ipsec >/dev/null ; then
		atf_skip "This test requires ipsec"
	fi

	setup_router_server_ipv4

	# Now define an ipsec policy so we end up taking the slow path.
	# We don't actually need the traffic to go through ipsec, we just don't
	# want to go through ip_tryforward().
	echo "flush;
	spdflush;
	spdadd 203.0.113.1/32 203.0.113.2/32 any -P out ipsec esp/transport//require;
	add 203.0.113.1 203.0.113.2 esp 0x1001 -E aes-gcm-16 \"12345678901234567890\";" \
	    | jexec router setkey -c

	# Sanity check.
	ping_server_check_reply exit:0 --ping-type=icmp

	# Enable packet reassembly with clearing of the no-df flag.
	pft_set_rules router \
		"scrub in on ${epair_tester}b fragment no reassemble" \
		"scrub on ${epair_server}a fragment reassemble" \
		"pass"

	# Ensure that the packet makes it through the slow path
	atf_check -s exit:0 -o ignore \
	    ping -c 1 -s 2000 198.51.100.2
}

reassemble_slowpath_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "too_many_fragments"
	atf_add_test_case "v6"
	atf_add_test_case "mtu_diff"
	atf_add_test_case "overreplace"
	atf_add_test_case "overindex"
	atf_add_test_case "overlimit"
	atf_add_test_case "reassemble"
	atf_add_test_case "no_df"
	atf_add_test_case "reassemble_slowpath"
}
