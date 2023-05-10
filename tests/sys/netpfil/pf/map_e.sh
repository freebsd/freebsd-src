# $FreeBSD$
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 KUROSAWA Takahiro <takahiro.kurosawa@gmail.com>
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

atf_test_case "map_e" "cleanup"
map_e_head()
{
	atf_set descr 'map-e-portset test'
	atf_set require.user root
}

map_e_body()
{
	NC_TRY_COUNT=12

	pft_init

	epair_map_e=$(vnet_mkepair)
	epair_echo=$(vnet_mkepair)

	vnet_mkjail map_e ${epair_map_e}b ${epair_echo}a
	vnet_mkjail echo ${epair_echo}b

	ifconfig ${epair_map_e}a 192.0.2.2/24 up
	route add -net 198.51.100.0/24 192.0.2.1

	jexec map_e ifconfig ${epair_map_e}b 192.0.2.1/24 up
	jexec map_e ifconfig ${epair_echo}a 198.51.100.1/24 up
	jexec map_e sysctl net.inet.ip.forwarding=1

	jexec echo ifconfig ${epair_echo}b 198.51.100.2/24 up
	jexec echo /usr/sbin/inetd -p inetd-echo.pid $(atf_get_srcdir)/echo_inetd.conf

	# Enable pf!
	jexec map_e pfctl -e
	pft_set_rules map_e \
		"nat pass on ${epair_echo}a inet from 192.0.2.0/24 to any -> (${epair_echo}a) map-e-portset 2/12/0x342"

	# Only allow specified ports.
	jexec echo pfctl -e
	pft_set_rules echo "block return all" \
		"pass in on ${epair_echo}b inet proto tcp from 198.51.100.1 port 19720:19723 to (${epair_echo}b) port 7" \
		"pass in on ${epair_echo}b inet proto tcp from 198.51.100.1 port 36104:36107 to (${epair_echo}b) port 7" \
		"pass in on ${epair_echo}b inet proto tcp from 198.51.100.1 port 52488:52491 to (${epair_echo}b) port 7"

	i=0
	while [ ${i} -lt ${NC_TRY_COUNT} ]
	do
		echo "foo ${i}" | timeout 2 nc -N 198.51.100.2 7
		if [ $? -ne 0 ]; then
			atf_fail "nc failed (${i})"
		fi
		i=$((${i}+1))
	done
}

map_e_cleanup()
{
	rm -f inetd-echo.pid
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "map_e"
}
