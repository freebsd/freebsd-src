#
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

. $(atf_get_srcdir)/utils.subr

common_dir=$(atf_get_srcdir)/../common

atf_test_case "rst" "cleanup"
rst_head()
{
	atf_set descr 'Check sequence number validation in RST packets'
	atf_set require.user root
	atf_set require.progs scapy
}

rst_body()
{
	pft_init

	epair_srv=$(vnet_mkepair)
	epair_cl=$(vnet_mkepair)
	epair_attack=$(vnet_mkepair)

	br=$(vnet_mkbridge)
	ifconfig ${br} addm ${epair_srv}a
	ifconfig ${epair_srv}a up
	ifconfig ${br} addm ${epair_cl}a
	ifconfig ${epair_cl}a up
	ifconfig ${br} addm ${epair_attack}a
	ifconfig ${epair_attack}a up
	ifconfig ${br} up

	vnet_mkjail srv ${epair_srv}b
	jexec srv ifconfig ${epair_srv}b 192.0.2.1/24 up
	jexec srv ifconfig lo0 inet 127.0.0.1/8 up

	vnet_mkjail cl ${epair_cl}b
	jexec cl ifconfig ${epair_cl}b 192.0.2.2/24 up
	jexec cl ifconfig lo0 inet 127.0.0.1/8 up

	jexec cl pfctl -e
	pft_set_rules cl \
		"pass keep state"

	# Not required, but pf should log the bad RST packet with this set.
	jexec cl pfctl -x loud

	vnet_mkjail attack ${epair_attack}b
	jexec attack ifconfig ${epair_attack}b 192.0.2.3/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    jexec cl ping -c 1 192.0.2.1

	echo "bar" | jexec srv nc -l 1234 &
	# Allow server time to start
	sleep 1

	echo "foo" | jexec cl nc -p 4321 192.0.2.1 1234 &
	# Allow connection time to set up
	sleep 1

	# Connection should be established now
	atf_check -s exit:0 -e ignore \
	    -o match:"ESTABLISHED:ESTABLISHED" \
	    jexec cl pfctl -ss -v

	# Now insert a fake RST
	atf_check -s exit:0 -o ignore \
	    jexec attack ${common_dir}/pft_rst.py 192.0.2.1 1234 192.0.2.2 4321

	# Connection should remain established
	atf_check -s exit:0 -e ignore \
	    -o match:"ESTABLISHED:ESTABLISHED" \
	    jexec cl pfctl -ss -v
}

rst_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "rst"
}
