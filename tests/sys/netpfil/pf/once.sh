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

atf_test_case "basic" "cleanup"
basic_head()
{
	atf_set descr 'Basic one shot rule test'
	atf_set require.user root
}

basic_body()
{
	pft_init

	epair=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair}a
	jexec alcatraz ifconfig ${epair}a 192.0.2.1/24 up

	ifconfig ${epair}b 192.0.2.2/24 up

	# Sanity checks
	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
	    "block" \
	    "pass in from 192.0.2.2 once"

	# First once succeeds
	atf_check -s exit:0 -o ignore \
	    ping -c 3 192.0.2.1

	# Check for '# expired'
	atf_check -s exit:0 -e ignore \
	    -o match:'pass in inet from 192.0.2.2 to any flags S/SA keep state once # expired' \
	    jexec alcatraz pfctl -sr -vv

	# The second one does not
	atf_check -s exit:2 -o ignore \
	    ping -c 3 192.0.2.1

	# Flush states, still shouldn't work
	jexec alcatraz pfctl -Fs
	atf_check -s exit:2 -o ignore \
	    ping -c 3 192.0.2.1
}

basic_cleanup()
{
	pft_cleanup
}

atf_test_case "anchor" "cleanup"
anchor_head()
{
	atf_set descr 'Test one shot rule in anchors'
	atf_set require.user root
}

anchor_body()
{
	pft_init
	epair=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair}a
	jexec alcatraz ifconfig ${epair}a 192.0.2.1/24 up

	ifconfig ${epair}b 192.0.2.2/24 up

	# Sanity checks
	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
	    "block" \
	    "anchor \"once\" {\n
	        pass in from 192.0.2.2 once\n
	    }"

	# First once succeeds
	atf_check -s exit:0 -o ignore \
	    ping -c 3 192.0.2.1

	# Check for '# expired'
	jexec alcatraz pfctl -sr -vv -a "*"
	atf_check -s exit:0 -e ignore \
	    -o match:'pass in inet from 192.0.2.2 to any flags S/SA keep state once # expired' \
	    jexec alcatraz pfctl -sr -vv -a "*"

	# The second one does not
	atf_check -s exit:2 -o ignore \
	    ping -c 3 192.0.2.1
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
