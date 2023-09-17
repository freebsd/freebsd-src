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

atf_test_case "ndp_add_gu_success" "cleanup"
ndp_add_gu_success_head() {
	atf_set descr 'Test static ndp record addition'
	atf_set require.user root
}

ndp_add_gu_success_body() {

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


atf_init_test_cases()
{

	atf_add_test_case "ndp_add_gu_success"
	atf_add_test_case "ndp_del_gu_success"
}

# end

