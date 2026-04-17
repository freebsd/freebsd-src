#!/usr/bin/env atf-sh
#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Alexander V. Chernikov
# Copyright (c) 2026 Pouria Mousavizadeh Tehrani <pouria@FreeBSD.org>
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

	# Make sure that NAs from us are flagged as coming from a router.
	atf_check -o ignore sysctl net.inet6.ip6.forwarding=1

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
	atf_check -o ignore jexec ${jname} route -6 flush
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

atf_test_case "ndp_prefix_lifetime_extend"
ndp_prefix_lifetime_extend_head() {
	atf_set descr 'Test prefix lifetime updates via ifconfig'
	atf_set require.user root
	atf_set require.progs jq
}

get_prefix_attr() {
	local prefix=$1
	local attr=$2
	local jail=""

	if [ -n "$3" ]; then
		jail="jexec $3"
	fi

	${jail} ndp -p --libxo json | \
	    jq -r '.ndp.["prefix-list"][] |
	           select(.prefix == "'${prefix}'") | .["'${attr}'"]'
}

# Given a prefix, return its expiry time in seconds.
prefix_expiry() {
	get_prefix_attr $1 "expires_sec" $2
}

# Given a prefix, return its valid and preferred lifetimes.
prefix_lifetimes() {
	local p v

	v=$(get_prefix_attr $1 "valid-lifetime" $2)
	p=$(get_prefix_attr $1 "preferred-lifetime" $2)
	echo $v $p
}

ndp_prefix_lifetime_extend_body() {
	local epair ex1 ex2 ex3 prefix pltime vltime

	atf_check -o save:epair ifconfig epair create
	epair=$(cat epair)
	atf_check ifconfig ${epair} up

	prefix="2001:db8:ffff:1000::"

	atf_check ifconfig ${epair} inet6 ${prefix}1/64 pltime 5 vltime 10
	t=$(prefix_lifetimes ${prefix}/64)
	if [ "${t}" != "10 5" ]; then
		atf_fail "Unexpected lifetimes: ${t}"
	fi
	ex1=$(prefix_expiry ${prefix}/64)
	if [ "${ex1}" -gt 10 ]; then
		atf_fail "Unexpected expiry time: ${ex1}"
	fi

	# Double the address lifetime and verify that the prefix is
	# updated.
	atf_check ifconfig ${epair} inet6 ${prefix}1/64 pltime 10 vltime 20
	t=$(prefix_lifetimes ${prefix}/64)
	if [ "${t}" != "20 10" ]; then
		atf_fail "Unexpected lifetimes: ${t}"
	fi
	ex2=$(prefix_expiry ${prefix}/64)
	if [ "${ex2}" -le "${ex1}" ]; then
		atf_fail "Expiry time was not extended: ${ex1} <= ${ex2}"
	fi

	# Add a second address from the same prefix with a shorter
	# lifetime, and make sure that the prefix lifetime is not
	# shortened.
	atf_check ifconfig ${epair} inet6 ${prefix}2/64 pltime 5 vltime 10
	t=$(prefix_lifetimes ${prefix}/64)
	if [ "${t}" != "20 10" ]; then
		atf_fail "Unexpected lifetimes: ${t}"
	fi
	ex3=$(prefix_expiry ${prefix}/64)
	if [ "${ex3}" -le 10 -o "${ex3}" -gt 20 ]; then
		atf_fail "Unexpected expiry time: ${ex3}"
	fi
}

atf_test_case "ndp_grand_linklayer_event" "cleanup"
ndp_grand_linklayer_event_head() {
	atf_set descr 'Test ndp GRAND on link-layer change event'
	atf_set require.user root
}

ndp_grand_linklayer_event_body() {
	local epair0 jname prefix address mac

	vnet_init

	jname="v6t-ndp_grand_linklayer_event"
	prefix="2001:db8:ffff:1000::"
	mac="90:10:00:01:02:03"

	epair0=$(vnet_mkepair)

	vnet_mkjail ${jname}1 ${epair0}a
	vnet_mkjail ${jname}2 ${epair0}b

	ndp_if_up ${epair0}a ${jname}1
	ndp_if_up ${epair0}b ${jname}2

	# with no_dad, grand for new global address will NOT run
	atf_check ifconfig -j ${jname}1 ${epair0}a inet6 ${prefix}1 no_dad
	atf_check ifconfig -j ${jname}2 ${epair0}b inet6 ${prefix}2 no_dad
	atf_check -s exit:1 -e ignore -o ignore \
		jexec ${jname}2 ndp -n ${prefix}1

	# Create the NCE in jail 2.
	atf_check -o ignore jexec ${jname}2 ping -c1 -t1 ${prefix}1

	# Check if current mac is received
	atf_check -s exit:0 -o ignore jexec ${jname}2 ndp -n ${prefix}1

	# change mac address to trigger grand.
	atf_check ifconfig -j ${jname}1 ${epair0}a ether ${mac}

	# link-local is the first address, thus our address should
	# wait a second before sending its NA
	atf_check -o not-match:"${prefix}1.*${mac}.*" \
		jexec ${jname}2 ndp -n ${prefix}1

	sleep 1.1
	# Check if mac address automatically updated
	atf_check -o match:"${prefix}1.*${mac}.*" \
		jexec ${jname}2 ndp -n ${prefix}1
}

ndp_grand_linklayer_event_cleanup() {
	vnet_cleanup
}

atf_test_case "ndp_input_validation_hlim" "cleanup"
ndp_input_validation_hlim_head() {
	atf_set descr 'Test RFC 4861 section 6.1.2: RA hop limit validation'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

ndp_input_validation_hlim_body() {
	local epair0 jname

	vnet_init

	jname="v6t-ndp_input_validation_hlim"

	epair0=$(vnet_mkepair)

	vnet_mkjail ${jname} ${epair0}a

	ndp_if_up ${epair0}a ${jname}
	ndp_if_up ${epair0}b
	atf_check jexec ${jname} ifconfig ${epair0}a inet6 accept_rtadv

	# Make sure that NAs from us are flagged as coming from a router.
	atf_check -o ignore sysctl net.inet6.ip6.forwarding=1

	# Send an invalid RA advertising a prefix.
	atf_check -e ignore python3 $(atf_get_srcdir)/ra.py \
	    --sendif ${epair0}b \
	    --dst $(ndp_if_lladdr ${epair0}a ${jname}) \
	    --src $(ndp_if_lladdr ${epair0}b) \
	    --hoplimit 254

	# Wait to make sure no router would appear.
	sleep 0.5
	atf_check -o empty jexec ${jname} ndp -r
}

ndp_input_validation_hlim_cleanup() {
	vnet_cleanup
}

atf_test_case "ndp_input_validation_src_linklocal" "cleanup"
ndp_input_validation_src_linklocal_head() {
	atf_set descr 'Test RFC 4861 section 6.1.2: RA source address must be link-local'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

ndp_input_validation_src_linklocal_body() {
	local epair0 jname

	vnet_init

	jname="v6t-ndp_input_validation_src_linklocal"

	epair0=$(vnet_mkepair)

	vnet_mkjail ${jname} ${epair0}a

	ndp_if_up ${epair0}a ${jname}
	ndp_if_up ${epair0}b
	atf_check jexec ${jname} ifconfig ${epair0}a inet6 accept_rtadv

	# Make sure that NAs from us are flagged as coming from a router.
	atf_check -o ignore sysctl net.inet6.ip6.forwarding=1

	# Send an invalid RA with multicast source.
	atf_check -e ignore python3 $(atf_get_srcdir)/ra.py \
	    --sendif ${epair0}b \
	    --dst $(ndp_if_lladdr ${epair0}a ${jname}) \
	    --src ff02::2

	# Send an invalid RA with global unicast source.
	atf_check -e ignore python3 $(atf_get_srcdir)/ra.py \
	    --sendif ${epair0}b \
	    --dst $(ndp_if_lladdr ${epair0}a ${jname}) \
	    --src 3fff::1

	# Wait to make sure no router would appear.
	sleep 0.5
	atf_check -o empty jexec ${jname} ndp -r
}

ndp_input_validation_src_linklocal_cleanup() {
	vnet_cleanup
}

atf_test_case "ndp_multirouter_pref" "cleanup"
ndp_multirouter_pref_head() {
	atf_set descr 'Test RFC 4861 section 6.3.4: multiple routers with different pref'
	atf_set require.user root
	atf_set require.progs jq python3 scapy
}

ndp_multirouter_pref_body() {
	local epair0 jname prefix lladdr advrtrs

	vnet_init

	jname="v6t-ndp_multirouter_pref"
	prefix="2001:db8:ffff:1000::"

	epair0=$(vnet_mkepair)

	vnet_mkjail ${jname} ${epair0}a

	ndp_if_up ${epair0}a ${jname}
	ndp_if_up ${epair0}b
	atf_check jexec ${jname} ifconfig ${epair0}a inet6 accept_rtadv

	# Make sure that NAs from us are flagged as coming from a router.
	atf_check -o ignore sysctl net.inet6.ip6.forwarding=1

	lladdr="$(ndp_if_lladdr ${epair0}b)"
	lladdr="${lladdr%?}a"
	# Send an RA with high preference.
	atf_check -e ignore python3 $(atf_get_srcdir)/ra.py \
	    --sendif ${epair0}b \
	    --dst $(ndp_if_lladdr ${epair0}a ${jname}) \
	    --src ${lladdr} \
	    --rtrpref 1 --prefix ${prefix} \
	    --validlifetime 10 --preferredlifetime 5

	lladdr="${lladdr%?}b"
	# Send an RA with medium preference.
	atf_check -e ignore python3 $(atf_get_srcdir)/ra.py \
	    --sendif ${epair0}b \
	    --dst $(ndp_if_lladdr ${epair0}a ${jname}) \
	    --src ${lladdr} \
	    --rtrpref 0 --prefix ${prefix} \
	    --validlifetime 10 --preferredlifetime 5

	lladdr="${lladdr%?}c"
	# Send an RA with low preference.
	atf_check -e ignore python3 $(atf_get_srcdir)/ra.py \
	    --sendif ${epair0}b \
	    --dst $(ndp_if_lladdr ${epair0}a ${jname}) \
	    --src ${lladdr} \
	    --rtrpref 3 --prefix ${prefix} \
	    --validlifetime 10 --preferredlifetime 5

	# Wait for a default router to appear.
	while [ "$(jexec ${jname} ndp -r | wc -l)" -ne 3 ]; do
		sleep 0.01
	done
	atf_check -s exit:0 \
		-o match:"^${lladdr%?}a%${epair0}a if=${epair0}a, flags=, pref=high,.*" \
		-o match:"^${lladdr%?}b%${epair0}a if=${epair0}a, flags=, pref=medium,.*" \
		-o match:"^${lladdr%?}c%${epair0}a if=${epair0}a, flags=, pref=low,.*" \
		jexec ${jname} ndp -r

	# Make sure a default route is being installed
	# XXX: for now, does not matter which router
	atf_check -o match:"^default[[:space:]]+${lladdr%?}" \
	    jexec ${jname} netstat -rn6

	# Make sure ndp knows about prefix advertising routers.
	advrtrs=$(get_prefix_attr ${prefix}/64 "advertising-routers" "${jname}" | \
		jq -r '. | length')
	if [ "${advrtrs}" -ne 3 ]; then
		atf_fail "Unexpected number of advertising routers: ${advrtrs}"
	fi
}

ndp_muiltirouter_pref_cleanup() {
	vnet_cleanup
}

atf_test_case "ndp_slaac_twohour_rule" "cleanup"
ndp_slaac_twohour_rule_head() {
	atf_set descr 'Test RFC 4862 section 5.5.3 (e): Two hour rule'
	atf_set require.user root
	atf_set require.progs jq python3 scapy
}

ndp_slaac_twohour_rule_body() {
	local epair0 jname prefix ex1 ex2

	vnet_init

	jname="v6t-ndp_slaac_twohour_rule"
	prefix="2001:db8:ffff:1000::"

	epair0=$(vnet_mkepair)

	vnet_mkjail ${jname} ${epair0}a

	ndp_if_up ${epair0}a ${jname}
	ndp_if_up ${epair0}b
	atf_check jexec ${jname} ifconfig ${epair0}a inet6 accept_rtadv

	# Make sure that NAs from us are flagged as coming from a router.
	atf_check -o ignore sysctl net.inet6.ip6.forwarding=1

	# Send an RA with 1 hour lifetime
	atf_check -e ignore python3 $(atf_get_srcdir)/ra.py \
	    --sendif ${epair0}b \
	    --dst $(ndp_if_lladdr ${epair0}a ${jname}) \
	    --src $(ndp_if_lladdr ${epair0}b) \
	    --prefix ${prefix} --prefixlen 64 \
	    --validlifetime 3600 --preferredlifetime 3600

	# Wait for a default router to appear.
	while [ -z "$(jexec ${jname} ndp -r)" ]; do
		sleep 0.01
	done
	ex1=$(prefix_expiry ${prefix}/64 "${jname}")

	# Set the address lifetime to 2 hours and verify that the prefix is updated.
	atf_check -e ignore python3 $(atf_get_srcdir)/ra.py \
	    --sendif ${epair0}b \
	    --dst $(ndp_if_lladdr ${epair0}a ${jname}) \
	    --src $(ndp_if_lladdr ${epair0}b) \
	    --prefix ${prefix} --prefixlen 64 \
	    --validlifetime 7200 --preferredlifetime 7200

	# Verify that ndp sets the correct value from RA.
	ex2=$(prefix_expiry ${prefix}/64 "${jname}")
	if [ "${ex2}" -le "${ex1}" ]; then
		atf_fail "Unexpected expiry time: ${ex2} <= ${ex1}"
	fi
	# Verify that address also updated the valid lifetime.
	ex2=$(ifconfig -j "${jname}" ${epair0}a inet6 | grep vltime | awk '{print $NF}' )
	if [ "${ex2}" -le 3600 ]; then
		atf_fail "Unexpected expiry time: ${ex2} <= ${ex1}"
	fi

	# Set the address lifetime to 1 Hour and verify that
	# the address of prefix is NOT updated to 1 hour.
	atf_check -e ignore python3 $(atf_get_srcdir)/ra.py \
	    --sendif ${epair0}b \
	    --dst $(ndp_if_lladdr ${epair0}a ${jname}) \
	    --src $(ndp_if_lladdr ${epair0}b) \
	    --prefix ${prefix} --prefixlen 64 \
	    --validlifetime 3600 --preferredlifetime 3600

	# Verify that ndp sets the received value from RA.
	ex2=$(prefix_expiry ${prefix}/64 "${jname}")
	if [ "${ex2}" -gt 3600 ]; then
		atf_fail "Unexpected ndp expiry time: ${ex2} > 3600"
	fi
	# Verify that address NOT updated the valid lifetime.
	ex2=$(ifconfig -j "${jname}" ${epair0}a inet6 | grep vltime | awk '{print $NF}' )
	if [ "${ex2}" -le 3600 ]; then
		atf_fail "Unexpected expiry time: ${ex2} <= 3600"
	fi
}

ndp_slaac_twohour_rule_cleanup() {
	vnet_cleanup
}

get_iface_prefix_flags() {
	local prefix=$1
	local iface=$2
	local jail=""

	if [ -n "$3" ]; then
		jail="jexec $3"
	fi

	${jail} ndp -p --libxo json | \
	    jq -r '.ndp.["prefix-list"][] |
		select((.prefix == "'${prefix}'") and .interface == "'${iface}'") |
		.flags'
}

atf_test_case "ndp_slaac_switch_onlink_prefix" "cleanup"
ndp_slaac_switch_onlink_prefix_head() {
	atf_set descr 'Test SLAAC onlink prefix switching when prefix received via multiple interfaces'
	atf_set require.user root
}

ndp_slaac_switch_onlink_prefix_body() {
	local epair0 epair1 jname prefix lladdr1 lladdr2 f1 f2

	vnet_init

	jname="v6t-ndp_slaac_switch_onlink_prefix"
	prefix="2001:db8:ffff:1000::"

	epair0=$(vnet_mkepair)
	epair1=$(vnet_mkepair)

	vnet_mkjail ${jname} ${epair0}a
	atf_check ifconfig ${epair1}a vnet ${jname}

	ndp_if_up ${epair0}a ${jname}
	ndp_if_up ${epair1}a ${jname}
	ndp_if_up ${epair0}b
	ndp_if_up ${epair1}b

	atf_check ifconfig -j ${jname} ${epair0}a inet6 accept_rtadv
	atf_check ifconfig -j ${jname} ${epair1}a inet6 accept_rtadv
	lladdr0=$(ndp_if_lladdr ${epair0}b)
	lladdr1=$(ndp_if_lladdr ${epair1}b)

	# Send an RA with high pref from epair0
	atf_check -e ignore python3 $(atf_get_srcdir)/ra.py \
	    --sendif ${epair0}b \
	    --dst $(ndp_if_lladdr ${epair0}a ${jname}) \
	    --src ${lladdr0} \
	    --rtrpref 1 --prefix ${prefix} \
	    --validlifetime 10 --preferredlifetime 5

	# Send an RA with medium pref from epair1
	atf_check -e ignore python3 $(atf_get_srcdir)/ra.py \
	    --sendif ${epair1}b \
	    --dst $(ndp_if_lladdr ${epair1}a ${jname}) \
	    --src ${lladdr1} \
	    --rtrpref 0 --prefix ${prefix} \
	    --validlifetime 10 --preferredlifetime 5

	# Wait for a default router to appear.
	while [ -z "$(jexec ${jname} ndp -r)" ]; do
		sleep 0.01
	done

	# Verify that we have a default route to epair0a
	atf_check -o match:"^default[[:space:]]+${lladdr0}" \
	    jexec ${jname} netstat -rn6

	# Verify that epair0a is_onlink and epair1a is_detached
	f1=$(get_iface_prefix_flags "${prefix}/64" "${epair0}a" "${jname}")
	f2=$(get_iface_prefix_flags "${prefix}/64" "${epair1}a" "${jname}")
	if [ "${f1}" != "LAO" ]; then
		atf_fail "Unexpected prefix flags on epair0a: ${f1}"
	fi
	if [ "${f2}" != "LAD" ]; then
		atf_fail "Unexpected prefix flags on epair1a: ${f2}"
	fi

	# Send an RA to withdraw prefix from epair0
	atf_check -e ignore python3 $(atf_get_srcdir)/ra.py \
	    --sendif ${epair0}b \
	    --dst $(ndp_if_lladdr ${epair0}a ${jname}) \
	    --src ${lladdr0} \
	    --rtrpref 1 --rtrltime 0 --prefix ${prefix} \
	    --validlifetime 0 --preferredlifetime 0

	# Verify that epair1a is_onlink and epair0a is not
	while [ "$(get_iface_prefix_flags ${prefix}/64 ${epair0}a ${jname})" == "LAO" ];
	do
		sleep 0.1
	done
	f2=$(get_iface_prefix_flags "${prefix}/64" "${epair1}a" "${jname}")
	if [ "${f2}" != "LAO" ]; then
		atf_fail "Unexpected prefix flags on epair1a: ${f2}"
	fi

	# Verify that we have a default route to epair1a
	atf_check -o match:"^default[[:space:]]+${lladdr1}" \
	    jexec ${jname} netstat -rn6
}

ndp_slaac_switch_onlink_prefix_cleanup() {
	vnet_cleanup
}

atf_test_case "ndp_routeinfo_option" "cleanup"
ndp_routeinfo_option_head() {
	atf_set descr 'Test RFC 4191: Add route based on received RTI in RA'
	atf_set require.user root
	atf_set require.progs jq python3 scapy
}

ndp_routeinfo_option_body() {
	local epair0 jname prefix lladdr route1 route2

	vnet_init

	jname="v6t-ndp_routeinfo_option"
	prefix="2001:db8:ffff:1000::"
	route1="3fff:1000::"
	route2="3fff:2000::"

	epair0=$(vnet_mkepair)

	vnet_mkjail ${jname} ${epair0}a

	ndp_if_up ${epair0}a ${jname}
	ndp_if_up ${epair0}b
	atf_check jexec ${jname} ifconfig ${epair0}a inet6 accept_rtadv

	# Make sure that NAs from us are flagged as coming from a router.
	atf_check -o ignore sysctl net.inet6.ip6.forwarding=1

	lladdr="$(ndp_if_lladdr ${epair0}b)"
	# Send an RA with high preference with 3 routes.
	# The last rti should overwrite the default route rtiltime to 100 seconds
	# and its preference to medium.
	atf_check -e ignore python3 $(atf_get_srcdir)/ra.py \
	    --sendif ${epair0}b \
	    --dst $(ndp_if_lladdr ${epair0}a ${jname}) \
	    --src ${lladdr} \
	    --rtrpref 1 --rtrltime 1800 --prefix ${prefix} \
	    --route ${route1} --routelen 32 \
	    --rtipref 1 --rtiltime 1800 \
	    --route ${route2} --routelen 48 \
	    --rtipref 4 --rtiltime 600 \
	    --route :: --routelen 0 \
	    --rtipref 4 --rtiltime 100

	# Wait for a default router to appear.
	while [ -z "$(jexec ${jname} ndp -r)" ]; do
		sleep 0.1
	done

	# Make sure routes from rti option are being installed
	atf_check -s exit:0 \
		-o match:"^${route1}/32[[:space:]]+${lladdr}.*1800" \
		-o match:"^${route2}/48[[:space:]]+${lladdr}.*600" \
		-o match:"^default[[:space:]]+${lladdr}" \
		jexec ${jname} netstat -rn6

	# Verify the default route lifetime and its preference is overwrited
	atf_check -s exit:0 \
		-o match:"^${lladdr}%${epair0}a if=${epair0}a, flags=, pref=medium, expire=1m.*" \
		jexec ${jname} ndp -r
}

ndp_routeinfo_option_cleanup() {
	vnet_cleanup
}


atf_init_test_cases()
{
	atf_add_test_case "ndp_add_gu_success"
	atf_add_test_case "ndp_del_gu_success"
	atf_add_test_case "ndp_slaac_default_route"
	atf_add_test_case "ndp_slaac_twohour_rule"
	atf_add_test_case "ndp_slaac_switch_onlink_prefix"
	atf_add_test_case "ndp_prefix_len_mismatch"
	atf_add_test_case "ndp_prefix_lifetime"
	atf_add_test_case "ndp_prefix_lifetime_extend"
	atf_add_test_case "ndp_grand_linklayer_event"
	atf_add_test_case "ndp_input_validation_hlim"
	atf_add_test_case "ndp_input_validation_src_linklocal"
	atf_add_test_case "ndp_multirouter_pref"
	atf_add_test_case "ndp_routeinfo_option"
}
