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

	ifconfig ${epair}b 192.0.2.2/24 up

	vnet_mkjail alcatraz ${epair}a
	jexec alcatraz ifconfig ${epair}a 192.0.2.1/24 up

	jexec alcatraz pfctl -e
}

common_test()
{
	# Small packets pass
	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1
	atf_check -s exit:0 -o ignore \
	    ping -c 1 -s 100 192.0.2.1

	# Larger packets do not
	atf_check -s exit:2 -o ignore \
	    ping -c 3 -s 101 192.0.2.1
	atf_check -s exit:2 -o ignore \
	    ping -c 3 -s 128 192.0.2.1
}

atf_test_case "basic" "cleanup"
basic_head()
{
	atf_set descr 'Basic max-pkt-size test'
	atf_set require.user root
}

basic_body()
{
	pft_init

	common_setup

	pft_set_rules alcatraz \
	    "pass max-pkt-size 128"

	common_test

	# We can enforce this on fragmented packets too
	pft_set_rules alcatraz \
	    "pass max-pkt-size 2000"

	atf_check -s exit:0 -o ignore \
	    ping -c 1 -s 1400 192.0.2.1
	atf_check -s exit:0 -o ignore \
	    ping -c 1 -s 1972 192.0.2.1
	atf_check -s exit:2 -o ignore \
	    ping -c 1 -s 1973 192.0.2.1
	atf_check -s exit:2 -o ignore \
	    ping -c 3 -s 3000 192.0.2.1
}

basic_cleanup()
{
	pft_cleanup
}

atf_test_case "match" "cleanup"
match_head()
{
	atf_set descr 'max-pkt-size on match rules'
	atf_set require.user root
}

match_body()
{
	pft_init

	common_setup

	pft_set_rules alcatraz \
	    "match in max-pkt-size 128" \
	    "pass"

	common_test
}

match_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "basic"
	atf_add_test_case "match"
}
