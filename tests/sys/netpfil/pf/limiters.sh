#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Rubicon Communications, LLC (Netgate)
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

atf_test_case "state_basic" "cleanup"
state_basic_head()
{
	atf_set descr 'Basic state limiter test'
	atf_set require.user root
}

state_basic_body()
{
	pft_init

	epair=$(vnet_mkepair)

	ifconfig ${epair}a 192.0.2.2/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.1/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1

	jexec alcatraz pfctl -e
	# Allow up to one ICMP state.
	pft_set_rules alcatraz \
	    "set timeout icmp.error 120" \
	    "state limiter \"server\" id 1 limit 1" \
	    "block in proto icmp" \
	    "pass in proto icmp state limiter \"server\" (no-match)"

	atf_check -s exit:0 -o ignore \
	    ping -c 2 192.0.2.1

	# This should now fail
	atf_check -s exit:2 -o ignore \
	    ping -c 2 192.0.2.1

	jexec alcatraz pfctl -sLimiterStates
	hardlim=$(jexec alcatraz pfctl -sLimiterStates | awk 'NR>1 { print $5; }')
	if [ $hardlim -eq 0 ]; then
		atf_fail "Hard limit not incremented"
	fi
}

state_basic_cleanup()
{
	pft_cleanup
}

atf_test_case "state_rate" "cleanup"
state_rate_head()
{
	atf_set descr 'State rate limiting test'
	atf_set require.user root
}

state_rate_body()
{
	pft_init

	epair=$(vnet_mkepair)

	ifconfig ${epair}a 192.0.2.2/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.1/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1

	jexec alcatraz pfctl -e
	# Allow one ICMP state per 5 seconds
	pft_set_rules alcatraz \
	    "set timeout icmp.error 120" \
	    "state limiter \"server\" id 1 limit 1000 rate 1/5" \
	    "block in proto icmp" \
	    "pass in proto icmp state limiter \"server\" (no-match)"

	atf_check -s exit:0 -o ignore \
	    ping -c 2 192.0.2.1

	# This should now fail
	atf_check -s exit:2 -o ignore \
	    ping -c 2 192.0.2.1

	jexec alcatraz pfctl -sLimiterStates
	ratelim=$(jexec alcatraz pfctl -sLimiterStates | awk 'NR>1 { print $6; }')
	if [ $ratelim -eq 0 ]; then
		atf_fail "Rate limit not incremented"
	fi

	sleep 6

	# We can now create another state
	atf_check -s exit:0 -o ignore \
	    ping -c 2 192.0.2.1
}

state_rate_cleanup()
{
	pft_cleanup
}

atf_test_case "state_block" "cleanup"
state_block_head()
{
	atf_set descr 'Test block mode state limiter'
	atf_set require.user root
}

state_block_body()
{
	pft_init

	epair=$(vnet_mkepair)

	ifconfig ${epair}a 192.0.2.2/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.1/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1

	jexec alcatraz pfctl -e
	# Allow one ICMP state per 5 seconds
	pft_set_rules alcatraz \
	    "set timeout icmp.error 120" \
	    "state limiter \"server\" id 1 limit 1000 rate 1/5" \
	    "pass" \
	    "pass in proto icmp state limiter \"server\" (block)"

	atf_check -s exit:0 -o ignore \
	    ping -c 2 192.0.2.1

	# This should now fail
	atf_check -s exit:2 -o ignore \
	    ping -c 2 192.0.2.1

	# However, if we set no-match and exceed the limit we just pass
	pft_set_rules alcatraz \
	    "set timeout icmp.error 120" \
	    "state limiter \"server\" id 1 limit 1000 rate 1/5" \
	    "pass" \
	    "pass in proto icmp state limiter \"server\" (no-match)"

	atf_check -s exit:0 -o ignore \
	    ping -c 2 192.0.2.1
	atf_check -s exit:0 -o ignore \
	    ping -c 2 192.0.2.1
}

state_block_cleanup()
{
	pft_cleanup
}

atf_test_case "source_basic" "cleanup"
source_basic_head()
{
	atf_set descr 'Basic source limiter test'
	atf_set require.user root
}

source_basic_body()
{
	pft_init

	epair=$(vnet_mkepair)

	ifconfig ${epair}a 192.0.2.2/24 up
	ifconfig ${epair}a inet alias 192.0.2.3/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.1/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    ping -S 192.0.2.2 -c 1 192.0.2.1
	atf_check -s exit:0 -o ignore \
	    ping -S 192.0.2.3 -c 1 192.0.2.1

	jexec alcatraz pfctl -e

	# Allow up to one source for ICMP.
	pft_set_rules alcatraz \
	    "set timeout icmp.error 120" \
	    "source limiter \"server\" id 1 entries 128 limit 1" \
	    "block in proto icmp" \
	    "pass in proto icmp source limiter \"server\" (no-match)"

	atf_check -s exit:0 -o ignore \
	    ping -S 192.0.2.2 -c 2 192.0.2.1

	# This should now fail
	atf_check -s exit:2 -o ignore \
	    ping -S 192.0.2.2 -c 2 192.0.2.1

	jexec alcatraz pfctl -sLimiterSrcs
	hardlim=$(jexec alcatraz pfctl -sLimiterSrcs | awk 'NR>1 { print $5; }')
	if [ $hardlim -eq 0 ]; then
		atf_fail "Hard limit not incremented"
	fi

	# However, a different source will succeed
	atf_check -s exit:0 -o ignore \
	    ping -S 192.0.2.3 -c 2 192.0.2.1

	atf_check -o match:"192.0.2.2/32 .*hardlim 2 ratelim 0" \
	    -e ignore \
	    jexec alcatraz pfctl -sLimiterSrcs -v
	atf_check -o match:"192.0.2.3/32 .*hardlim 0 ratelim 0" \
	    -e ignore \
	    jexec alcatraz pfctl -sLimiterSrcs -v

	# Kill the source entry
	atf_check -s exit:0 -e ignore \
	    jexec alcatraz pfctl -I 1 -k source -k 192.0.2.2
	# Now we can ping again from it
	atf_check -s exit:0 -o ignore \
	    ping -S 192.0.2.2 -c 2 192.0.2.1
}

source_basic_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "state_basic"
	atf_add_test_case "state_rate"
	atf_add_test_case "state_block"
	atf_add_test_case "source_basic"
}
