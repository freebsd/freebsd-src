#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 Kristof Provost <kp@FreeBSD.org>
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
	atf_set descr 'Test setting and retrieving limits'
	atf_set require.user root
}

basic_body()
{
	pft_init

	vnet_mkjail alcatraz

	pft_set_rules alcatraz \
		"set limit states 200" \
		"set limit frags 100" \
		"set limit src-nodes 50" \
		"set limit table-entries 25"

	atf_check -s exit:0 -o match:'states.*200' \
	    jexec alcatraz pfctl -sm
	atf_check -s exit:0 -o match:'frags.*100' \
	    jexec alcatraz pfctl -sm
	atf_check -s exit:0 -o match:'src-nodes.*50' \
	    jexec alcatraz pfctl -sm
	atf_check -s exit:0 -o match:'table-entries.*25' \
	    jexec alcatraz pfctl -sm
}

basic_cleanup()
{
	pft_cleanup
}

atf_test_case "zero" "cleanup"
zero_head()
{
	atf_set descr 'Test changing a limit from zero on an in-use zone'
	atf_set require.user root
}

zero_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}b 192.0.2.2/24 up

	vnet_mkjail alcatraz ${epair}a
	jexec alcatraz ifconfig ${epair}a 192.0.2.1/24 up

	atf_check -s exit:0 -o ignore \
	    ping -c 3 192.0.2.1

	jexec alcatraz pfctl -e
	# Set no limit
	pft_set_rules noflush alcatraz \
		"set limit states 0" \
		"pass"

	# Check that we really report no limit
	atf_check -s exit:0 -o 'match:states        hard limit        0' \
	    jexec alcatraz pfctl -sa

	# Create a state
	atf_check -s exit:0 -o ignore \
	    ping -c 3 192.0.2.1

	# Limit states
	pft_set_rules noflush alcatraz \
		"set limit states 1000" \
		"pass"

	# And create a new state
	atf_check -s exit:0 -o ignore \
	    ping -c 3 192.0.2.1

	atf_check -s exit:0 -o 'match:states        hard limit     1000' \
	    jexec alcatraz pfctl -sa
}

zero_cleanup()
{
	pft_cleanup
}

atf_test_case "anchors" "cleanup"
anchors_head()
{
	atf_set descr 'Test increasing maximum number of anchors'
	atf_set require.user root
}

anchors_body()
{
	pft_init

	vnet_mkjail alcatraz

	jexec alcatraz pfctl -e

	pft_set_rules alcatraz \
	    "set limit anchors 1"

	pft_set_rules alcatraz \
	    "set limit anchors 2" \
	    "pass" \
	    "anchor \"foo\" {\n
	        pass in\n
	    }" \
	    "anchor \"bar\" {\n
	        pass out\n
	    }"
}

anchors_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "basic"
	atf_add_test_case "zero"
	atf_add_test_case "anchors"
}
