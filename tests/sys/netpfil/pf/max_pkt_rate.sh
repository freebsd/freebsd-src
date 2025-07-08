#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 Rubicon Communications, LLC (Netgate)
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

common_setup()
{
	epair=$(vnet_mkepair)

	ifconfig ${epair}a inet 192.0.2.2/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b inet 192.0.2.1/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1

	jexec alcatraz pfctl -e
}

common_test()
{
	# One ping will pass
	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1

	# As will a second
	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1

	# But the third should fail
	atf_check -s exit:2 -o ignore \
	    ping -c 1 192.0.2.1

	# But three seconds later we can ping again
	sleep 3
	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1
}

atf_test_case "basic" "cleanup"
basic_head()
{
	atf_set descr 'Basic maximum packet rate test'
	atf_set require.user root
}

basic_body()
{
	pft_init

	common_setup

	pft_set_rules alcatraz \
	    "block" \
	    "pass in proto icmp max-pkt-rate 2/2"

	common_test
}

basic_cleanup()
{
	pft_cleanup
}

atf_test_case "anchor" "cleanup"
anchor_head()
{
	atf_set descr 'maximum packet rate on anchor'
	atf_set require.user root
}

anchor_body()
{
	pft_init

	common_setup

	pft_set_rules alcatraz \
	    "block" \
	    "anchor \"foo\" proto icmp max-pkt-rate 2/2 {\n \
	    	pass \n \
	    }"

	common_test
}

anchor_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "basic"
	atf_add_test_case "anchor"
}
