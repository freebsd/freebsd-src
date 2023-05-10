# $FreeBSD$
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

atf_test_case "dummynet" "cleanup"
dummynet_head()
{
	atf_set descr 'Test dummynet with match keyword'
	atf_set require.user root
}

dummynet_body()
{
	dummynet_init

	epair=$(vnet_mkepair)
	vnet_mkjail alcatraz ${epair}b

	ifconfig ${epair}a 192.0.2.1/24 up
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore ping -i .1 -c 3 -s 1200 192.0.2.2

	jexec alcatraz dnctl pipe 1 config bw 30Byte/s
	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"match in dnpipe 1" \
		"pass"

	# single ping succeeds just fine
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	# Saturate the link
	ping -i .1 -c 5 -s 1200 192.0.2.2

	# We should now be hitting the limits and get this packet dropped.
	atf_check -s exit:2 -o ignore ping -c 1 -s 1200 192.0.2.2
}

dummynet_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "dummynet"
}
