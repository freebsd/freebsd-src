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
	# Note: use pft_ping so we always use the same ID, so pf considers all
	# echo requests part of the same flow.
	atf_check -s exit:0 -o ignore ${common_dir}/pft_ping.py \
		--sendif ${epair}a \
		--to 192.0.2.2 \
		--replyif ${epair}a

	# Change rules to now deny the ICMP traffic
	pft_set_rules noflush alcatraz "block all"

	# Established state means we can still ping alcatraz
	atf_check -s exit:0 -o ignore ${common_dir}/pft_ping.py \
		--sendif ${epair}a \
		--to 192.0.2.2 \
		--replyif ${epair}a

	# Killing with the wrong IP doesn't affect our state
	jexec alcatraz pfctl -k 192.0.2.3

	# So we can still ping
	atf_check -s exit:0 -o ignore ${common_dir}/pft_ping.py \
		--sendif ${epair}a \
		--to 192.0.2.2 \
		--replyif ${epair}a

	# Killing with one correct address and one incorrect doesn't kill the state
	jexec alcatraz pfctl -k 192.0.2.1 -k 192.0.2.3

	# So we can still ping
	atf_check -s exit:0 -o ignore ${common_dir}/pft_ping.py \
		--sendif ${epair}a \
		--to 192.0.2.2 \
		--replyif ${epair}a

	# Killing with correct address does remove the state
	jexec alcatraz pfctl -k 192.0.2.1

	# Now the ping fails
	atf_check -s exit:1 -o ignore ${common_dir}/pft_ping.py \
		--sendif ${epair}a \
		--to 192.0.2.2 \
		--replyif ${epair}a
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

	epair=$(vnet_mkepair)
	ifconfig ${epair}a inet6 2001:db8::1/64 up no_dad

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b inet6 2001:db8::2/64 up no_dad
	jexec alcatraz pfctl -e

	pft_set_rules alcatraz "block all" \
		"pass in proto icmp6"

	# Sanity check & establish state
	# Note: use pft_ping so we always use the same ID, so pf considers all
	# echo requests part of the same flow.
	atf_check -s exit:0 -o ignore ${common_dir}/pft_ping.py \
		--ip6 \
		--sendif ${epair}a \
		--to 2001:db8::2 \
		--replyif ${epair}a

	# Change rules to now deny the ICMP traffic
	pft_set_rules noflush alcatraz "block all"

	# Established state means we can still ping alcatraz
	atf_check -s exit:0 -o ignore ${common_dir}/pft_ping.py \
		--ip6 \
		--sendif ${epair}a \
		--to 2001:db8::2 \
		--replyif ${epair}a

	# Killing with the wrong IP doesn't affect our state
	jexec alcatraz pfctl -k 2001:db8::3
	atf_check -s exit:0 -o ignore ${common_dir}/pft_ping.py \
		--ip6 \
		--sendif ${epair}a \
		--to 2001:db8::2 \
		--replyif ${epair}a

	# Killing with one correct address and one incorrect doesn't kill the state
	jexec alcatraz pfctl -k 2001:db8::1 -k 2001:db8::3
	atf_check -s exit:0 -o ignore ${common_dir}/pft_ping.py \
		--ip6 \
		--sendif ${epair}a \
		--to 2001:db8::2 \
		--replyif ${epair}a

	# Killing with correct address does remove the state
	jexec alcatraz pfctl -k 2001:db8::1
	atf_check -s exit:1 -o ignore ${common_dir}/pft_ping.py \
		--ip6 \
		--sendif ${epair}a \
		--to 2001:db8::2 \
		--replyif ${epair}a

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
	# Note: use pft_ping so we always use the same ID, so pf considers all
	# echo requests part of the same flow.
	atf_check -s exit:0 -o ignore ${common_dir}/pft_ping.py \
		--sendif ${epair}a \
		--to 192.0.2.2 \
		--replyif ${epair}a

	# Change rules to now deny the ICMP traffic
	pft_set_rules noflush alcatraz "block all"

	# Established state means we can still ping alcatraz
	atf_check -s exit:0 -o ignore ${common_dir}/pft_ping.py \
		--sendif ${epair}a \
		--to 192.0.2.2 \
		--replyif ${epair}a

	# Killing a label on a different rules keeps the state
	jexec alcatraz pfctl -k label -k bar
	atf_check -s exit:0 -o ignore ${common_dir}/pft_ping.py \
		--sendif ${epair}a \
		--to 192.0.2.2 \
		--replyif ${epair}a

	# Killing a non-existing label keeps the state
	jexec alcatraz pfctl -k label -k baz
	atf_check -s exit:0 -o ignore ${common_dir}/pft_ping.py \
		--sendif ${epair}a \
		--to 192.0.2.2 \
		--replyif ${epair}a

	# Killing the correct label kills the state
	jexec alcatraz pfctl -k label -k foo
	atf_check -s exit:1 -o ignore ${common_dir}/pft_ping.py \
		--sendif ${epair}a \
		--to 192.0.2.2 \
		--replyif ${epair}a
}

label_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "v4"
	atf_add_test_case "v6"
	atf_add_test_case "label"
}
