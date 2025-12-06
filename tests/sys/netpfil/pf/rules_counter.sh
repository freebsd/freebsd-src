#
# SPDX-License-Identifier: BSD-2-Clause
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

atf_test_case "get_clear" "cleanup"
get_clear_head()
{
	atf_set descr 'Test clearing rules counters on get rules'
	atf_set require.user root
}

get_clear_body()
{
	pft_init

	epair_send=$(vnet_mkepair)
	ifconfig ${epair_send}a 192.0.2.1/24 up

	vnet_mkjail alcatraz ${epair_send}b
	jexec alcatraz ifconfig ${epair_send}b 192.0.2.2/24 up
	jexec alcatraz pfctl -e

	pft_set_rules alcatraz \
		"pass all"

	# Ensure the rule matched packets, so we can verify non-zero counters
	atf_check -s exit:0 -o ignore ping -c 3 192.0.2.2

	# Expect non-zero counters
	atf_check -s exit:0 -e ignore \
	    -o match:'Evaluations: [1-9][0-9]*[[:space:]]*Packets: [1-9][0-9]*[[:space:]]*Bytes: [1-9][0-9]*[[:space:]]*' \
	    jexec alcatraz pfctl -s r -v

	# We should still see non-zero because we didn't clear on the last
	# pfctl, but are going to clear now
	atf_check -s exit:0 -e ignore \
	    -o match:'Evaluations: [1-9][0-9]*[[:space:]]*Packets: [1-9][0-9]*[[:space:]]*Bytes: [1-9][0-9]*[[:space:]]*' \
	    jexec alcatraz pfctl -s r -v -z

	# Expect zero counters
	atf_check -s exit:0 -e ignore \
	    -o match:'Evaluations: 0[[:space:]]*Packets: 0*[[:space:]]*Bytes: 0*[[:space:]]*' \
	    jexec alcatraz pfctl -s r -v
}

get_clear_cleanup()
{
	pft_cleanup
}

atf_test_case "keepcounters" "cleanup"
keepcounters_head()
{
	atf_set descr 'Test keepcounter functionality'
	atf_set require.user root
}

keepcounters_body()
{
	pft_init

	epair_send=$(vnet_mkepair)
	ifconfig ${epair_send}a 192.0.2.1/24 up

	vnet_mkjail alcatraz ${epair_send}b
	jexec alcatraz ifconfig ${epair_send}b 192.0.2.2/24 up
	jexec alcatraz pfctl -e

	pft_set_rules alcatraz \
		"pass all"

	# Expect zero counters
	atf_check -s exit:0 -e ignore \
	    -o match:'Evaluations: 0[[:space:]]*Packets: 0*[[:space:]]*Bytes: 0*[[:space:]]*' \
	    jexec alcatraz pfctl -s r -v

	# Ensure the rule matched packets, so we can verify non-zero counters
	atf_check -s exit:0 -o ignore ping -c 3 192.0.2.2

	# Expect non-zero counters
	atf_check -s exit:0 -e ignore \
	    -o match:'Evaluations: [1-9][0-9]*[[:space:]]*Packets: [1-9][0-9]*[[:space:]]*Bytes: [1-9][0-9]*[[:space:]]*' \
	    jexec alcatraz pfctl -s r -v

	# As we set the (same) rules again we'd expect the counters to return
	# to zero
	pft_set_rules noflush alcatraz \
		"pass all"

	atf_check -s exit:0 -e ignore \
	    -o match:'Evaluations: 0[[:space:]]*Packets: 0*[[:space:]]*Bytes: 0*[[:space:]]*' \
	    jexec alcatraz pfctl -s r -v

	# Increment rule counters
	atf_check -s exit:0 -o ignore ping -c 3 192.0.2.2

	# Now set new rules with 'keepcounters' set, so we'd expect nonzero
	# counters
	pft_set_rules noflush alcatraz \
		"set keepcounters" \
		"pass all"

	atf_check -s exit:0 -e ignore \
	    -o match:'Evaluations: [1-9][0-9]*[[:space:]]*Packets: [1-9][0-9]*[[:space:]]*Bytes: [1-9][0-9]*[[:space:]]*' \
	    jexec alcatraz pfctl -s r -v

	# However, if we set a different rule it should return to zero
	pft_set_rules noflush alcatraz \
		"set keepcounters" \
		"pass inet all"

	atf_check -s exit:0 -e ignore \
	    -o match:'Evaluations: 0[[:space:]]*Packets: 0*[[:space:]]*Bytes: 0*[[:space:]]*' \
	    jexec alcatraz pfctl -s r -v

	# If we generate traffic and don't set keepcounters we also see zero
	# counts when setting new rules
	atf_check -s exit:0 -o ignore ping -c 3 192.0.2.2
	pft_set_rules noflush alcatraz \
		"pass inet all"

	atf_check -s exit:0 -e ignore \
	    -o match:'Evaluations: 0[[:space:]]*Packets: 0*[[:space:]]*Bytes: 0*[[:space:]]*' \
	    jexec alcatraz pfctl -s r -v
}

atf_test_case "4G" "cleanup"
4G_head()
{
	atf_set descr 'Test keepcounter for values above 32 bits'
	atf_set require.user root
	atf_set timeout 900
}

4G_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a 192.0.2.1/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up
	jexec alcatraz nc -l 1234 >/dev/null &

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"pass all"

	# Now pass more than 4GB of data
	dd if=/dev/zero bs=1k count=4M | nc -N 192.0.2.2 1234

	bytes=$(jexec alcatraz pfctl -s r -v | awk '/Bytes:/ { print $7; }')
	if [ $bytes -lt 4000000000 ];
	then
		atf_fail "Expected to see > 4GB"
	fi

	# Set new rules, keeping counters
	pft_set_rules noflush alcatraz \
		"set keepcounters" \
		"pass all"

	bytes=$(jexec alcatraz pfctl -s r -v | awk '/Bytes:/ { print $7; }')
	if [ $bytes -lt 4000000000 ];
	then
		atf_fail "Expected to see > 4GB after rule reload"
	fi
}

4G_cleanup()
{
	pft_cleanup
}

keepcounters_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "get_clear"
	atf_add_test_case "keepcounters"
	atf_add_test_case "4G"
}
