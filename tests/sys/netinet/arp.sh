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
# $FreeBSD$
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


atf_init_test_cases()
{

	atf_add_test_case "arp_add_success"
	atf_add_test_case "arp_del_success"
	atf_add_test_case "pending_delete_if"
}

# end

