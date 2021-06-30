# $FreeBSD$
#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2018 Kristof Provost <kp@FreeBSD.org>
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

atf_test_case "synproxy" "cleanup"
synproxy_head()
{
	atf_set descr 'Basic synproxy test'
	atf_set require.user root
}

synproxy_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a 192.0.2.1/24 up
	route add -net 198.51.100.0/24 192.0.2.2

	link=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair}b ${link}a
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up
	jexec alcatraz ifconfig ${link}a 198.51.100.1/24 up
	jexec alcatraz sysctl net.inet.ip.forwarding=1

	vnet_mkjail singsing ${link}b
	jexec singsing ifconfig ${link}b 198.51.100.2/24 up
	jexec singsing route add default 198.51.100.1

	jexec singsing /usr/sbin/inetd -p inetd-singsing.pid $(atf_get_srcdir)/echo_inetd.conf

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz "set fail-policy return" \
		"scrub in all fragment reassemble" \
		"pass out quick on ${epair}b all no state allow-opts" \
		"pass in quick on ${epair}b proto tcp from any to any port 7 synproxy state" \
		"pass in quick on ${epair}b all no state"

	# Sanity check, can we ping singing
	atf_check -s exit:0 -o ignore ping -c 1 198.51.100.2

	# Check that we can talk to the singsing jail, after synproxying
	reply=$(echo ping | nc -N 198.51.100.2 7)
	if [ "${reply}" != "ping" ];
	then
		atf_fail "echo failed"
	fi
}

synproxy_cleanup()
{
	rm -f inetd-singsing.pid
	pft_cleanup
}

atf_test_case "local" "cleanup"
local_head()
{
	atf_set descr 'Synproxy a locally terminated connection'
	atf_set require.user root
}

local_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a 192.0.2.2/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.1/24 up
	jexec alcatraz /usr/sbin/inetd -p inetd-alcatraz.pid \
		$(atf_get_srcdir)/echo_inetd.conf

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz "set fail-policy return" \
		"scrub in all fragment reassemble" \
		"pass in quick on ${epair}b proto tcp from any to any port 7 synproxy state"

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.1

	# Check that we can talk to the jail, after synproxying
	reply=$(echo ping | nc -N -w 5 192.0.2.1 7)
	if [ "${reply}" != "ping" ];
	then
		atf_fail "echo failed"
	fi
}

local_cleanup()
{
	rm -f inetd-alcatraz.pid
	pft_cleanup
}

atf_test_case "local_v6" "cleanup"
local_v6_head()
{
	atf_set descr 'Synproxy (v6) a locally terminated connection'
	atf_set require.user root
}

local_v6_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a inet6 2001:db8:42::1/64 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b inet6 2001:db8:42::2/64 up
	jexec alcatraz /usr/sbin/inetd -p inetd-alcatraz.pid \
		$(atf_get_srcdir)/echo_inetd.conf

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz "set fail-policy return" \
		"scrub in all fragment reassemble" \
		"pass in quick on ${epair}b proto tcp from any to any port 7 synproxy state"

	# Sanity check
	atf_check -s exit:0 -o ignore ping6 -c 1 2001:db8:42::2

	reply=$(echo ping | nc -N -w 5 2001:db8:42::2 7)
	if [ "${reply}" != "ping" ];
	then
		atf_fail "echo failed"
	fi
}

local_v6_cleanup()
{
	rm -f inetd-alcatraz.pid
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "synproxy"
	atf_add_test_case "local"
	atf_add_test_case "local_v6"
}
