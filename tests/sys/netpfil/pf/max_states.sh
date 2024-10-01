#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 Kajetan Staszkiewicz <vegeta@tuxpowered.net>
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


max_states_head()
{
	atf_set descr 'Max states per rule'
	atf_set require.user root
}

max_states_body()
{
	setup_router_dummy_ipv6

	pft_set_rules router \
		"block" \
		"pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }" \
		"pass in  on ${epair_tester}b inet6 proto tcp keep state (max 3)" \
		"pass out on ${epair_server}a inet6 proto tcp keep state"

	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-sport=4201
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-sport=4202
	ping_dummy_check_request exit:0 --ping-type=tcpsyn --send-sport=4203
	ping_dummy_check_request exit:1 --ping-type=tcpsyn --send-sport=4204
}

max_states_cleanup()
{
	pft_cleanup
}


atf_init_test_cases()
{
	atf_add_test_case "max_states"
}
