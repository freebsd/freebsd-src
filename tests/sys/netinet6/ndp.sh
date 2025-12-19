#!/usr/bin/env atf-sh
#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Alexander V. Chernikov
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
#

. $(atf_get_srcdir)/../common/vnet.subr

atf_test_case "ndp_add_gu_success" "cleanup"
ndp_add_gu_success_head() {
	atf_set descr 'Test static ndp record addition'
	atf_set require.user root
}

ndp_add_gu_success_body() {
	local epair0 jname

	vnet_init

	jname="v6t-ndp_add_success"

	epair0=$(vnet_mkepair)

	vnet_mkjail ${jname} ${epair0}a
	jexec ${jname} ndp -i ${epair0}a -- -disabled
	jexec ${jname} ifconfig ${epair0}a up

	jexec ${jname} ifconfig ${epair0}a inet6 2001:db8::1/64

	# wait for DAD to complete
	while [ `jexec ${jname} ifconfig | grep inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done

	atf_check jexec ${jname} ndp -s 2001:db8::2 90:10:00:01:02:03

	t=`jexec ${jname} ndp -an | grep 2001:db8::2 | awk '{print $1, $2, $3, $4}'`
	if [ "${t}" != "2001:db8::2 90:10:00:01:02:03 ${epair0}a permanent" ]; then
		atf_fail "Wrong output: ${t}"
	fi
	echo "T='${t}'"
}

ndp_add_gu_success_cleanup() {
	vnet_cleanup
}

atf_test_case "ndp_del_gu_success" "cleanup"
ndp_del_gu_success_head() {
	atf_set descr 'Test ndp record deletion'
	atf_set require.user root
}

ndp_del_gu_success_body() {
	local epair0 jname

	vnet_init

	jname="v6t-ndp_del_gu_success"

	epair0=$(vnet_mkepair)

	vnet_mkjail ${jname} ${epair0}a

	jexec ${jname} ndp -i ${epair0}a -- -disabled
	jexec ${jname} ifconfig ${epair0}a up

	jexec ${jname} ifconfig ${epair0}a inet6 2001:db8::1/64

	# wait for DAD to complete
	while [ `jexec ${jname} ifconfig | grep inet6 | grep -c tentative` != "0" ]; do
		sleep 0.1
	done

	jexec ${jname} ping -c1 -t1 2001:db8::2

	atf_check -o match:"2001:db8::2 \(2001:db8::2\) deleted" jexec ${jname} ndp -nd 2001:db8::2
}

ndp_del_gu_success_cleanup() {
	vnet_cleanup
}

ndp_if_up()
{
	local ifname=$1
	local jname=$2

	if [ -n "$jname" ]; then
		jname="jexec ${jname}"
	fi
	atf_check ${jname} ifconfig ${ifname} up
	atf_check ${jname} ifconfig ${ifname} inet6 -ifdisabled
	while ${jname} ifconfig ${ifname} inet6 | grep tentative; do
		sleep 0.1
	done
}

ndp_if_lladdr()
{
	local ifname=$1
	local jname=$2

	if [ -n "$jname" ]; then
		jname="jexec ${jname}"
	fi
	${jname} ifconfig ${ifname} inet6 | \
	    awk '/inet6 fe80:/{split($2, addr, "%"); print addr[1]}'
}

atf_test_case "ndp_slaac_default_route" "cleanup"
ndp_slaac_default_route_head() {
	atf_set descr 'Test default route installation via SLAAC'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

ndp_slaac_default_route_body() {
	local epair0 jname lladdr

	vnet_init

	jname="v6t-ndp_slaac_default_route"

	epair0=$(vnet_mkepair)

	vnet_mkjail ${jname} ${epair0}a

	ndp_if_up ${epair0}a ${jname}
	ndp_if_up ${epair0}b
	atf_check jexec ${jname} ifconfig ${epair0}a inet6 accept_rtadv

        # Send an RA advertising a prefix.
	atf_check -e ignore python3 $(atf_get_srcdir)/ra.py \
	    --sendif ${epair0}b \
	    --dst $(ndp_if_lladdr ${epair0}a ${jname}) \
	    --src $(ndp_if_lladdr ${epair0}b) \
	    --prefix "2001:db8:ffff:1000::" --prefixlen 64

	# Wait for a default router to appear.
	while [ -z "$(jexec ${jname} ndp -r)" ]; do
		sleep 0.1
	done
	atf_check -o match:"^default[[:space:]]+fe80:" \
	    jexec ${jname} netstat -rn -6

	# Get rid of the default route.
	jexec ${jname} route -6 flush
	atf_check -o not-match:"^default[[:space:]]+fe80:" \
	    jexec ${jname} netstat -rn -6

	# Send another RA, make sure that the default route is installed again.
	atf_check -e ignore python3 $(atf_get_srcdir)/ra.py \
	    --sendif ${epair0}b \
	    --dst $(ndp_if_lladdr ${epair0}a ${jname}) \
	    --src $(ndp_if_lladdr ${epair0}b) \
	    --prefix "2001:db8:ffff:1000::" --prefixlen 64
	while [ -z "$(jexec ${jname} ndp -r)" ]; do
		sleep 0.1
	done
	atf_check -o match:"^default[[:space:]]+fe80:" \
	    jexec ${jname} netstat -rn -6
}

ndp_slaac_default_route_cleanup() {
	vnet_cleanup
}

atf_test_case "ndp_prefix_len_mismatch" "cleanup"
ndp_prefix_len_mismatch_head() {
	atf_set descr 'Test RAs on an interface without a /64 lladdr'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

ndp_prefix_len_mismatch_body() {
	vnet_init

	epair=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair}a

	jexec alcatraz ifconfig ${epair}a inet6 -auto_linklocal
	jexec alcatraz ifconfig ${epair}a inet6 -ifdisabled
	jexec alcatraz ifconfig ${epair}a inet6 accept_rtadv
	jexec alcatraz ifconfig ${epair}a inet6 fe80::5a9c:fcff:fe10:5d07/127
	jexec alcatraz ifconfig ${epair}a up

	ifconfig ${epair}b inet6 -ifdisabled
	ifconfig ${epair}b up

	atf_check -e ignore python3 $(atf_get_srcdir)/ra.py \
	    --sendif ${epair}b \
	    --dst $(ndp_if_lladdr ${epair}a alcatraz) \
	    --src $(ndp_if_lladdr ${epair}b) \
	    --prefix "2001:db8:ffff:1000::" --prefixlen 64

	atf_check \
	    -o match:"inet6 2001:db8:ffff:1000:.* prefixlen 64.*autoconf.*" \
	    jexec alcatraz ifconfig ${epair}a
}

ndp_prefix_len_mismatch_cleanup() {
	vnet_cleanup
}

atf_test_case "ndp_prefix_lifetime" "cleanup"
ndp_prefix_lifetime_head() {
	atf_set descr 'Test ndp slaac address lifetime handling'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

ndp_prefix_lifetime_body() {
	local epair0 jname prefix

	vnet_init

	jname="v6t-ndp_prefix_lifetime"

	epair0=$(vnet_mkepair)

	vnet_mkjail ${jname} ${epair0}a

	ndp_if_up ${epair0}a ${jname}
	ndp_if_up ${epair0}b
	atf_check jexec ${jname} ifconfig ${epair0}a inet6 accept_rtadv no_dad

	prefix="2001:db8:ffff:1000:"

        # Send an RA advertising a prefix.
	atf_check -e ignore python3 $(atf_get_srcdir)/ra.py \
	    --sendif ${epair0}b \
	    --dst $(ndp_if_lladdr ${epair0}a ${jname}) \
	    --src $(ndp_if_lladdr ${epair0}b) \
	    --prefix "2001:db8:ffff:1000::" --prefixlen 64 \
	    --validlifetime 10 --preferredlifetime 5

	# Wait for a default router to appear.
	while [ -z "$(jexec ${jname} ndp -r)" ]; do
		sleep 0.1
	done
	atf_check \
	    -o match:"^default[[:space:]]+fe80:" \
	    jexec ${jname} netstat -rn -6

	atf_check \
	    -o match:"inet6 ${prefix}.* prefixlen 64 autoconf pltime 5 vltime 10" \
	    jexec ${jname} ifconfig ${epair0}a

	# Wait for the address to become deprecated.
	sleep 6
	atf_check \
	    -o match:"inet6 ${prefix}.* prefixlen 64 deprecated autoconf pltime 0 vltime [1-9]+" \
	    jexec ${jname} ifconfig -L ${epair0}a

	# Wait for the address to expire.
	sleep 6
	atf_check \
	    -o not-match:"inet6 ${prefix}.*" \
	    jexec ${jname} ifconfig ${epair0}a
}

ndp_prefix_lifetime_cleanup() {
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "ndp_add_gu_success"
	atf_add_test_case "ndp_del_gu_success"
	atf_add_test_case "ndp_slaac_default_route"
	atf_add_test_case "ndp_prefix_len_mismatch"
	atf_add_test_case "ndp_prefix_lifetime"
}
