#!/usr/bin/env atf-sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright 2022 John-Mark Gurney
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#
# Run the tests:
# make WITH_TESTS=yes -j 4 all install && kyua test -k /usr/tests/Kyuafile  sbin/dhclient/pcp
#
# Output last run:
# kyua report --verbose -r $(ls -tr ~/.kyua/store/results.*.db | tail -n 1)

. $(atf_get_srcdir)/../../sys/common/vnet.subr

generic_dhcp_cleanup()
{

	# clean up programs
	kill $(cat dhclient.test.pid) $(cat dhcpd.pid)

	# clean up files
	rm -f dhclient.dhcpd.conf lease.dhclient.test dhclient.test.pid

	vnet_cleanup
}

atf_test_case normal cleanup
normal_head()
{
	atf_set descr 'test dhclient against a server'
	atf_set require.user root
}

normal_body()
{
	dhcpd=$(which dhcpd)

	if ! [ -x "$dhcpd" ]; then
		atf_skip "ISC dhcp server (isc-dhcp44-server) not installed"
	fi

	vnet_init

	epair=$(vnet_mkepair)

	vnet_mkjail dhclient_normal_test ${epair}b

	# Set IP on server iface
	ifconfig ${epair}a 192.0.2.2/24 up

	# Create dhcp server config
	cat > dhclient.dhcpd.conf << EOF
default-lease-time 36000;
max-lease-time 86400;
authoritative;
subnet 192.0.2.0 netmask 255.255.255.0 {
	range 192.0.2.10 192.0.2.10;
	option routers 192.0.2.2;
	option domain-name-servers 192.0.2.2;
}
EOF

	# Start dhcp server
	touch dhcpd.leases.conf
	atf_check -e ignore ${dhcpd} -cf ./dhclient.dhcpd.conf -lf ./dhcpd.leases.conf -pf ./dhcpd.pid ${epair}a

	# Expect that we get an IP assigned
	atf_check -e match:'DHCPACK from 192.0.2.2' jexec dhclient_normal_test dhclient -c /dev/null -l ./lease.dhclient.test -p ./dhclient.test.pid ${epair}b

	# And it's the correct one
	atf_check -o match:'inet 192.0.2.10' jexec dhclient_normal_test ifconfig ${epair}b

}

normal_cleanup()
{

	generic_dhcp_cleanup
}

atf_test_case pcp cleanup
pcp_head()
{
	atf_set descr 'test dhclient on pcp interface'
	atf_set require.user root
}

pcp_body()
{
	dhcpd=$(which dhcpd)

	if ! [ -x "$dhcpd" ]; then
		atf_skip "ISC dhcp server (isc-dhcp44-server) not installed"
	fi

	vnet_init

	epair=$(vnet_mkepair)

	# Server side needs to be up to pass packets
	ifconfig ${epair}a up

	# Make sure necessary netgraph modules are loaded
	kldstat -q -n ng_ether || kldload ng_ether
	kldstat -q -n ng_iface || kldload ng_iface
	kldstat -q -n ng_vlan || kldload ng_vlan

	# create vlan, and attach epair to it (has incoming/outgoing vlan
	# 0 tagged frames)
	ngctl mkpeer ${epair}a: vlan lower downstream

	# create new interface on other side of vlan (untagged/pcp)
	ngctl mkpeer ${epair}a:lower. eiface vlan0 ether

	# get the interface created
	ngiface=$(ngctl show ${epair}a:lower.vlan0 | head -n 1 | awk '{ print $2}')

	# schedule it for clean up
	echo ${ngiface} >> ngctl.shutdown

	# set the filter on it
	ngctl msg ${epair}a:lower. 'addfilter { vlan=0 hook="vlan0" }'

	vnet_mkjail dhclient_pcp_test ${epair}b

	# Set IP on server iface
	ifconfig ${ngiface} up 192.0.2.2/24

	# Set pcp in jail
	jexec dhclient_pcp_test ifconfig ${epair}b pcp 0 up

	# Create dhcp server config
	cat > dhclient.dhcpd.conf << EOF
default-lease-time 36000;
max-lease-time 86400;
authoritative;
subnet 192.0.2.0 netmask 255.255.255.0 {
	range 192.0.2.10 192.0.2.10;
	option routers 192.0.2.2;
	option domain-name-servers 192.0.2.2;
}
EOF

	# Start dhcp server
	touch dhcpd.leases.conf
	atf_check -e ignore ${dhcpd} -cf ./dhclient.dhcpd.conf -lf ./dhcpd.leases.conf -pf ./dhcpd.pid ${ngiface}

	# Expect that we get an IP assigned
	atf_check -e match:'DHCPACK from 192.0.2.2' jexec dhclient_pcp_test dhclient -c /dev/null -l ./lease.dhclient.test -p ./dhclient.test.pid ${epair}b

	# And it's the correct one
	atf_check -o match:'inet 192.0.2.10' jexec dhclient_pcp_test ifconfig ${epair}b
}

pcp_cleanup()
{

	generic_dhcp_cleanup

	# Clean up netgraph nodes
	for i in $(cat ngctl.shutdown); do
		ngctl shutdown ${i}:
	done
	rm -f ngctl.shutdown
}

atf_init_test_cases()
{
	atf_add_test_case normal
	atf_add_test_case pcp
}

