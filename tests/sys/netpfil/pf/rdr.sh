# $FreeBSD$
#
# SPDX-License-Identifier: BSD-2-Clause
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

atf_test_case "basic" "cleanup"
basic_head()
{
	atf_set descr 'Basic rdr test'
	atf_set require.user root
}

basic_body()
{
	pft_init

	epair=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair}b

	ifconfig ${epair}a 192.0.2.2/24 up
	route add -net 198.51.100.0/24 192.0.2.1

	jexec alcatraz ifconfig ${epair}b 192.0.2.1/24 up
	jexec alcatraz sysctl net.inet.ip.forwarding=1

	# Enable pf!
	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"rdr pass on ${epair}b proto tcp from any to 198.51.100.0/24 port 1234 -> 192.0.2.1 port 4321"

	echo "foo" | jexec alcatraz nc -N -l 4321 &
	sleep 1

	result=$(nc -N -w 3 198.51.100.2 1234)
	if [ "$result" != "foo" ]; then
		atf_fail "Redirect failed"
	fi
}

basic_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "basic"
}
