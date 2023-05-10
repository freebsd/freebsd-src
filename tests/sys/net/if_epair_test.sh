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

. $(atf_get_srcdir)/../common/vnet.subr

atf_test_case "pcp" "cleanup"
pcp_head()
{
	atf_set descr 'Test PCP over if_epair. PR#270736'
	atf_set require.user root
}

pcp_body()
{
	vnet_init

	j="if_epair_test_pcp"

	epair=$(vnet_mkepair)

	vnet_mkjail ${j}one ${epair}a
	vnet_mkjail ${j}two ${epair}b

	jexec ${j}one ifconfig ${epair}a 192.0.2.1/24 pcp 3 up
	jexec ${j}two ifconfig ${epair}b 192.0.2.2/24 up

	atf_check -s exit:0 -o ignore \
	    jexec ${j}one ping -c 1 192.0.2.2

	# Now set a different PCP. This used to lead to double tagging and failed pin.
	atf_check -s exit:0 -o ignore \
	    jexec ${j}one ping -C5 -c 1 192.0.2.2
}

pcp_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "pcp"
}
