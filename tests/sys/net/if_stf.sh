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

atf_test_case "6to4" "cleanup"
6to4_head()
{
	atf_set descr 'Test 6to4'
	atf_set require.user root
}

6to4_body()
{
	vnet_init
	if ! kldstat -q -m if_stf; then
		atf_skip "This test requires if_stf"
	fi
	if ! kldstat -q -m if_gif; then
		atf_skip "This test requires if_gif"
	fi

	epair=$(vnet_mkepair)

	vnet_mkjail relay ${epair}a
	jexec relay ifconfig lo0 inet6 2001:db8::1/64 up
	jexec relay ifconfig ${epair}a 192.0.2.1/24 up
	# Simple gif to terminate 6to4
	gif=$(jexec relay ifconfig gif create)
	jexec relay ifconfig $gif inet6 2002:c000:0201::1/64 up
	jexec relay ifconfig $gif tunnel 192.0.2.1 192.0.2.2
	jexec relay route -6 add default -interface $gif

	vnet_mkjail client ${epair}b
	jexec client ifconfig lo0 up
	jexec client ifconfig ${epair}b 192.0.2.2/24 up
	stf=$(jexec client ifconfig stf create)
	jexec client ifconfig $stf inet6 2002:c000:0202::1/32 up
	jexec client route -6 add default 2002:c000:0201::1

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    jexec client ping -c 1 192.0.2.1
	# 6to4 direct
	atf_check -s exit:0 -o ignore \
	    jexec client ping6 -c 1 2002:c000:0201::1

	# "Wider internet"
	atf_check -s exit:0 -o ignore \
	    jexec client ping6 -c 1 2001:db8::1
}

6to4_cleanup()
{
	vnet_cleanup
}

atf_test_case "6rd" "cleanup"
6rd_head()
{
	atf_set descr '6RD test'
	atf_set require.user root
}

6rd_body()
{
	vnet_init

	if ! kldstat -q -m if_stf; then
		atf_skip "This test requires if_stf"
	fi
	if ! kldstat -q -m if_gif; then
		atf_skip "This test requires if_gif"
	fi

	epair=$(vnet_mkepair)
	vnet_mkjail br ${epair}a
	jexec br ifconfig ${epair}a 192.0.2.1/24 up

	# Simple gif to terminate the 6RD tunnel
	gif=$(jexec br ifconfig gif create)
	jexec br ifconfig lo0 inet6 2001:db9::1/64 up
	jexec br ifconfig $gif inet6 2001:db8::/64 up
	jexec br ifconfig $gif tunnel 192.0.2.1 192.0.2.2
	jexec br route -6 add default -interface $gif

	vnet_mkjail client ${epair}b
	jexec client ifconfig lo0 up
	jexec client ifconfig ${epair}b 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    jexec client ping -c 1 192.0.2.1

	stf=$(jexec client ifconfig stf create)

	jexec client ifconfig $stf stfv4br 192.0.2.1
	jexec client ifconfig $stf stfv4net 192.0.2.2/32
	jexec client ifconfig $stf inet6 2001:db8:c000:0202::1/32 up
	jexec client route -6 add default -interface $stf

	atf_check -s exit:0 -o ignore \
	    jexec client ping6 -c 1 2001:db9::1
}

6rd_cleanup()
{
	vnet_cleanup
}

atf_test_case "6rd_peer" "cleanup"
6rd_peer_head()
{
	atf_set descr '6RD peer test'
	atf_set require.user root
}

6rd_peer_body()
{
	vnet_init

	if ! kldstat -q -m if_stf; then
		atf_skip "This test requires if_stf"
	fi

	epair=$(vnet_mkepair)

	vnet_mkjail one ${epair}a
	jexec one ifconfig lo0 up
	jexec one ifconfig ${epair}a 192.0.2.1/24 up
	stf_one=$(jexec one ifconfig stf create)
	jexec one ifconfig $stf_one stfv4br 192.0.2.3
	jexec one ifconfig $stf_one stfv4net 192.0.2.1/32
	jexec one ifconfig $stf_one inet6 2001:db8:c000:0201::1/32 up
	jexec one route -6 add default -interface $stf_one

	vnet_mkjail two ${epair}b
	jexec two ifconfig lo0 up
	jexec two ifconfig ${epair}b 192.0.2.2/24 up
	stf_two=$(jexec two ifconfig stf create)
	jexec two ifconfig $stf_two stfv4br 192.0.2.3
	jexec two ifconfig $stf_two stfv4net 192.0.2.2/32
	jexec two ifconfig $stf_two inet6 2001:db8:c000:0202::1/32 up
	jexec two route -6 add default -interface $stf_two

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    jexec one ping -c 1 192.0.2.2

	# Test 6rd
	atf_check -s exit:0 -o ignore \
	    jexec one ping6 -c 1 2001:db8:c000:0202::1
	atf_check -s exit:0 -o ignore \
	    jexec two ping6 -c 1 2001:db8:c000:0201::1

	# Shorter prefixes, for both v4 and v6
	jexec one ifconfig $stf_one inet6 2001:db8:c000:0201::1 delete
	jexec one ifconfig $stf_one inet6 2001:0201::1/16
	jexec one ifconfig $stf_one stfv4net 192.0.2.1/16
	jexec two ifconfig $stf_two inet6 2001:db8:c000:0202::1 delete
	jexec two ifconfig $stf_two inet6 2001:0202::1/16
	jexec two ifconfig $stf_two stfv4net 192.0.2.2/16

	atf_check -s exit:0 -o ignore \
	    jexec one ping6 -c 1 2001:0202::1
	atf_check -s exit:0 -o ignore \
	    jexec two ping6 -c 1 2001:0201::1
}

6rd_peer_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "6to4"
	atf_add_test_case "6rd"
	atf_add_test_case "6rd_peer"
}
