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

. $(atf_get_srcdir)/../common/vnet.subr

atf_test_case "basic" "cleanup"
basic_head()
{
	atf_set descr 'Basic gif(4) test'
	atf_set require.user root
}

basic_body()
{
	vnet_init
	if ! kldstat -q -m if_gif; then
		atf_skip "This test requires if_gif"
	fi

	epair=$(vnet_mkepair)

	vnet_mkjail one ${epair}a
	jexec one ifconfig ${epair}a 192.0.2.1/24 up
	gone=$(jexec one ifconfig gif create)
	jexec one ifconfig $gone tunnel 192.0.2.1 192.0.2.2
	jexec one ifconfig $gone inet 198.51.100.1/24 198.51.100.2 up

	vnet_mkjail two ${epair}b
	jexec two ifconfig ${epair}b 192.0.2.2/24 up
	gtwo=$(jexec two ifconfig gif create)
	jexec two ifconfig $gtwo tunnel 192.0.2.2 192.0.2.1
	jexec two ifconfig $gtwo inet 198.51.100.2/24 198.51.100.1 up

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    jexec one ping -c 1 192.0.2.2

	# Tunnel test
	atf_check -s exit:0 -o ignore \
	    jexec one ping -c 1 198.51.100.2
	atf_check -s exit:0 -o ignore \
	    jexec two ping -c 1 198.51.100.1
}

basic_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "basic"
}
