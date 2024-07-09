#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 Rubicon Communications, LLC (Netgate)
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

atf_test_case "zero_id" "cleanup"
zero_id_head()
{
	atf_set descr 'Test ICMPv6 echo with ID 0 keep being blocked'
	atf_set require.user root
	atf_set require.progs scapy
}

zero_id_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a inet6 2001:db8::2/64 up no_dad

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b inet6 2001:db8::1/64 up no_dad

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    ping -c 1 2001:db8::1

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"set block-policy drop" \
		"antispoof quick for { egress ${epair}b }" \
		"block all" \
		"pass out" \
		"pass in quick inet6 proto IPV6-ICMP icmp6-type 135" \
		"pass in quick inet6 proto IPV6-ICMP icmp6-type 136" \
		"pass out quick inet6 proto IPV6 from self to any"

	# Now we can't ping
	atf_check -s exit:2 -o ignore \
	    ping -c 1 2001:db8::1

	# Force neighbour discovery
	ndp -d 2001:db8::1

	# Verify that we don't confuse echo request with ID 0 for neighbour discovery
	atf_check -s exit:1 -o ignore \
	     ${common_dir}/pft_ping.py \
	         --sendif ${epair}a \
	         --to 2001:db8::1 \
	         --replyif ${epair}a

	jexec alcatraz pfctl -ss -vv
	jexec alcatraz pfctl -sr -vv
}

zero_id_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "zero_id"
}
