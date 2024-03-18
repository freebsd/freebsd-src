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


atf_test_case "basic" "cleanup"
basic_head()
{
	atf_set descr 'Basic loginterface test'
	atf_set require.user root
}

basic_body()
{
	pft_init

	epair=$(vnet_mkepair)

	ifconfig ${epair}a 192.0.2.2/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.1/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.1

	# No interface stats until we configure a loginterface
	atf_check -o not-match:"Interface Stats for" \
		jexec alcatraz pfctl -s info

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"set loginterface ${epair}b" \
		"pass"

	# We do get Interface Stats listed when we've configured a loginterface
	atf_check -o match:"Interface Stats for ${epair}b" \
		jexec alcatraz pfctl -s info

	# And after we've sent traffic there's non-zero counters
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.1

	atf_check -o match:"Interface Stats for ${epair}b" \
		jexec alcatraz pfctl -s info
	atf_check -o match:"Passed                               1" \
		jexec alcatraz pfctl -s info

	# And no interface stats once we remove the loginterface
	pft_set_rules alcatraz \
		"pass"
	atf_check -o not-match:"Interface Stats for ${epair}b" \
		jexec alcatraz pfctl -s info
}

basic_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "basic"
}
