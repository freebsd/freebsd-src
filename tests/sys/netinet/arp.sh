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
#

. $(atf_get_srcdir)/../common/vnet.subr

atf_test_case "arp_add_success" "cleanup"
arp_add_success_head() {
	atf_set descr 'Test static arp record addition'
	atf_set require.user root
}

arp_add_success_body() {

	vnet_init

	jname="v4t-arp_add_success"

	epair0=$(vnet_mkepair)

	vnet_mkjail ${jname} ${epair0}a

	jexec ${jname} ifconfig ${epair0}a inet 198.51.100.1/24

	atf_check jexec ${jname} arp -s 198.51.100.2 90:10:00:01:02:03

	atf_check -o match:"\? \(198.51.100.2\) at 90:10:00:01:02:03 on ${epair0}a permanent" jexec ${jname} arp -ni ${epair0}a 198.51.100.2
}

arp_add_success_cleanup() {
	vnet_cleanup
}


atf_test_case "arp_del_success" "cleanup"
arp_del_success_head() {
	atf_set descr 'Test arp record deletion'
	atf_set require.user root
}

arp_del_success_body() {

	vnet_init

	jname="v4t-arp_del_success"

	epair0=$(vnet_mkepair)

	vnet_mkjail ${jname} ${epair0}a

	jexec ${jname} ifconfig ${epair0}a inet 198.51.100.1/24

	jexec ${jname} ping -c1 -t1 198.51.100.2

	atf_check -o match:"198.51.100.2 \(198.51.100.2\) deleted" jexec ${jname} arp -nd 198.51.100.2
	atf_check -s exit:1 -o match:"198.51.100.2 \(198.51.100.2\) -- no entry" jexec ${jname} arp -n 198.51.100.2
}

arp_del_success_cleanup() {
	vnet_cleanup
}

atf_test_case "pending_delete_if" "cleanup"
pending_delete_if_head() {
	atf_set descr 'Test having pending link layer lookups on interface delete'
	atf_set require.user root
}

pending_delete_if_body() {
	vnet_init

	jname="arp_pending_delete_if"
	epair=$(vnet_mkepair)

	ifconfig ${epair}b up

	vnet_mkjail ${jname} ${epair}a
	jexec ${jname} ifconfig ${epair}a 198.51.100.1/24
	for i in `seq 2 200`
	do
		jexec ${jname} ping 198.51.100.${i} &
	done

	# Give the ping processes time to send their ARP requests
	sleep 1

	jexec ${jname} arp -an
	jexec ${jname} killall ping

	# Delete the interface. Test failure panics the machine.
	ifconfig ${epair}b destroy
}

pending_delete_if_cleanup() {
	vnet_cleanup
}


atf_test_case "arp_lookup_host" "cleanup"
arp_lookup_host_head() {
	atf_set descr 'Test looking up a specific host'
	atf_set require.user root
}

arp_lookup_host_body() {

	vnet_init

	jname="v4t-arp_lookup_host"

	epair0=$(vnet_mkepair)

	vnet_mkjail ${jname}a ${epair0}a
	vnet_mkjail ${jname}b ${epair0}b

	ipa=198.51.100.1
	ipb=198.51.100.2

	atf_check -o ignore \
		  ifconfig -j ${jname}a ${epair0}a inet ${ipa}/24
	atf_check -o ignore \
		  ifconfig -j ${jname}b ${epair0}b inet ${ipb}/24

	# get jail b's MAC address
	eth="$(ifconfig -j ${jname}b ${epair0}b |
		sed -nE "s/^\tether ([0-9a-f:]*)$/\1/p")"

	# no entry yet
	atf_check -s exit:1 -o match:"\(${ipb}\) -- no entry" \
		  jexec ${jname}a arp -n ${ipb}

	# now ping jail b from jail a
	atf_check -o ignore \
		  jexec ${jname}a ping -c1 ${ipb}

	# should be populated
	atf_check -o match:"\(${ipb}\) at $eth on ${epair0}a" \
		  jexec ${jname}a arp -n ${ipb}

}

arp_lookup_host_cleanup() {
	vnet_cleanup
}


atf_test_case "static" "cleanup"
static_head() {
	atf_set descr 'Test arp -s/-S works'
	atf_set require.user root
}

static_body() {

	vnet_init

	jname="v4t-arp_static_host"

	epair0=$(vnet_mkepair)

	vnet_mkjail ${jname}a ${epair0}a
	vnet_mkjail ${jname}b ${epair0}b

	ipa=198.51.100.1
	ipb=198.51.100.2
	ipb_re=$(echo ${ipb} | sed 's/\./\\./g')
	max_age=$(sysctl -n net.link.ether.inet.max_age)
	max_age="(${max_age}|$((${max_age} - 1)))"

	atf_check ifconfig -j ${jname}a ${epair0}a inet ${ipa}/24
	eth="$(ifconfig -j ${jname}b ${epair0}b |
		sed -nE "s/^\tether ([0-9a-f:]*)$/\1/p")"

	# Expected outputs
	permanent=\
"? (${ipb}) at 00:00:00:00:00:00 on ${epair0}a permanent [ethernet]\n"
	temporary_re=\
"\? \(${ipb_re}\) at ${eth} on ${epair0}a expires in ${max_age} seconds \[ethernet\]"
	deleted=\
"${ipb} (${ipb}) deleted\n"

	# first check -s
	atf_check jexec ${jname}a arp -s ${ipb} 0:0:0:0:0:0
	# the jail B ifconfig will send gratuitous ARP that will trigger A
	atf_check ifconfig -j ${jname}b ${epair0}b inet ${ipb}/24
	atf_check -o "inline:${permanent}" jexec ${jname}a arp -n ${ipb}
	if [ $(sysctl -n net.link.ether.inet.log_arp_permanent_modify) -ne 0 ];
	then
		msg=$(dmesg | tail -n 1)
		atf_check_equal "${msg}" \
"arp: ${eth} attempts to modify permanent entry for ${ipb} on ${epair0}a"
	fi

	# then check -S
	atf_check -o "inline:${deleted}" jexec ${jname}a arp -nd ${ipb}
	atf_check -o ignore jexec ${jname}b ping -c1 ${ipa}
	atf_check -o "match:${temporary_re}" jexec ${jname}a arp -n ${ipb}
	# Note: this doesn't fail, tracked all the way down to FreeBSD 8
	# atf_check -s not-exit:0 jexec ${jname}a arp -s ${ipb} 0:0:0:0:0:0
	atf_check -o "inline:${deleted}" \
	    jexec ${jname}a arp -S ${ipb} 0:0:0:0:0:0
	atf_check -o "inline:${permanent}" jexec ${jname}a arp -n ${ipb}
}

static_cleanup() {
	vnet_cleanup
}

atf_test_case "garp" "cleanup"
garp_head() {
	atf_set descr 'Basic gratuitous arp test'
	atf_set require.user root
}

garp_body() {
	vnet_init

	j="v4t-garp"

	epair=$(vnet_mkepair)

	vnet_mkjail ${j} ${epair}a
	atf_check -s exit:0 -o ignore \
	    jexec ${j} sysctl net.link.ether.inet.garp_rexmit_count=3
	jexec ${j} ifconfig ${epair}a inet 192.0.2.1/24 up

	# Allow some time for the timer to actually fire
	sleep 5
}

garp_cleanup() {
	vnet_cleanup
}


atf_init_test_cases()
{

	atf_add_test_case "arp_add_success"
	atf_add_test_case "arp_del_success"
	atf_add_test_case "pending_delete_if"
	atf_add_test_case "arp_lookup_host"
	atf_add_test_case "static"
	atf_add_test_case "garp"
}

# end

