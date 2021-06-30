# $FreeBSD$
#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2021 Modirum MDPay
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

atf_test_case "forward" "cleanup"
forward_head()
{
	atf_set descr 'Syncookies for forwarded hosts'
	atf_set require.user root
}

forward_body()
{
	pft_init

	epair_in=$(vnet_mkepair)
	epair_out=$(vnet_mkepair)

	vnet_mkjail fwd ${epair_in}b ${epair_out}a
	vnet_mkjail srv ${epair_out}b

	jexec fwd ifconfig ${epair_in}b 192.0.2.1/24 up
	jexec fwd ifconfig ${epair_out}a 198.51.100.1/24 up
	jexec fwd sysctl net.inet.ip.forwarding=1

	jexec srv ifconfig ${epair_out}b 198.51.100.2/24 up
	jexec srv route add default 198.51.100.1
	jexec srv /usr/sbin/inetd -p inetd-alcatraz.pid \
	    $(atf_get_srcdir)/echo_inetd.conf

	ifconfig ${epair_in}a 192.0.2.2/24 up
	route add -net 198.51.100.0/24 192.0.2.1

	jexec fwd pfctl -e
	pft_set_rules fwd \
		"set syncookies always" \
		"pass in" \
		"pass out"

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 198.51.100.2

	reply=$(echo foo | nc -N -w 5 198.51.100.2 7)
	if [ "${reply}" != "foo" ];
	then
		atf_fail "Failed to connect to syncookie protected echo daemon"
	fi
}

forward_cleanup()
{
	rm -f inetd-alcatraz.pid
	pft_cleanup
}

atf_test_case "nostate" "cleanup"
nostate_head()
{
	atf_set descr 'Ensure that we do not create until SYN|ACK'
	atf_set require.user root
	atf_set require.progs scapy
}

nostate_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a 192.0.2.2/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.1/24 up

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"set syncookies always" \
		"pass in" \
		"pass out"

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.1

	# Now syn flood to create many states
	${common_dir}/pft_synflood.py \
		--sendif ${epair}a \
		--to 192.0.2.2 \
		--count 20

	states=$(jexec alcatraz pfctl -ss | grep tcp)
	if [ -n "$states" ];
	then
		echo "$states"
		atf_fail "Found unexpected state"
	fi
}

nostate_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "forward"
	atf_add_test_case "nostate"
}
