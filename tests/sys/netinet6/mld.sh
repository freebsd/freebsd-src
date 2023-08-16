#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Netflix, Inc.
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

atf_test_case "mldraw01" "cleanup"
mldraw01_head() {

	atf_set descr 'Test for correct Ethernet Destination MAC address'
	atf_set require.user root
	atf_set require.progs scapy
}

mldraw01_body() {

	ids=65533
	id=`printf "%x" ${ids}`
	if [ $$ -gt 65535 ]; then
		xl=`printf "%x" $(($$ - 65535))`
		yl="1"
	else
		xl=`printf "%x" $$`
		yl=""
	fi

	vnet_init

	ip6a="2001:db8:6666:0000:${yl}:${id}:1:${xl}"
	ip6b="2001:db8:6666:0000:${yl}:${id}:2:${xl}"

	epair=$(vnet_mkepair)
	ifconfig ${epair}a up
	ifconfig ${epair}a inet6 ${ip6a}/64

	jname="v6t-${id}-${yl}-${xl}"
	vnet_mkjail ${jname} ${epair}b
	jexec ${jname} ifconfig ${epair}b up
	jexec ${jname} ifconfig ${epair}b inet6 ${ip6b}/64

	# Let IPv6 ND do its thing.
	#ping6 -q -c 1 ff02::1%${epair}a
	#ping6 -q -c 1 ${ip6b}
	sleep 3

	pyname=$(atf_get ident)

	atf_check -s exit:0 $(atf_get_srcdir)/mld.py \
		--sendif ${epair}a --recvif ${epair}a \
		--src ${ip6a} --to  ${ip6b} \
		--${pyname}
}

mldraw01_cleanup() {

	vnet_cleanup
}

atf_test_case "pr233683" "cleanup"
pr233683_head() {

	atf_set descr 'Test for PR233683'
	atf_set require.user root
}

pr233683_body() {
	j="mld:pr233683"

	vnet_init

	epair=$(vnet_mkepair)

	vnet_mkjail ${j}a ${epair}a
	jexec ${j}a ifconfig ${epair}a inet6 2001:db8::1/64 up
	sleep 5

	jexec ${j}a ifconfig ${epair}a inet6 2001:db8::1/64

	vnet_mkjail ${j}b ${epair}b
	jexec ${j}b ifconfig ${epair}b inet6 2001:db8::2/64 up

	# Allow DAD to run
	sleep 5

	# Debug output. If the bug is present we'd expect to not see a
	# membership for ff02::1:ff00:1
	jexec ${j}a ifmcstat -i ${epair}a
	jexec ${j}a ifconfig ${epair}a

	atf_check -s exit:0 -o ignore \
	    jexec ${j}b ping -6 -c 1 2001:db8::1
}

pr233683_cleanup() {

	vnet_cleanup
}
atf_init_test_cases()
{

	atf_add_test_case "mldraw01"
	atf_add_test_case "pr233683"
}

# end
