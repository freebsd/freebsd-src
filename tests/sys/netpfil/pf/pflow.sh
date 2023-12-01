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
}

basic_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "basic"
}
