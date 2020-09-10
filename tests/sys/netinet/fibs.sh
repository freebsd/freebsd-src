#!/usr/bin/env atf-sh
#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Alexander V. Chernikov
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

atf_test_case "fibs_ifroutes1_success" "cleanup"
fibs_ifroutes1_success_head()
{

	atf_set descr 'Test IPv4 routes gets populated in the correct fib'
	atf_set require.user root
}

fibs_ifroutes1_success_body()
{

	vnet_init

	net_dst="192.168.0."
	jname="v6t-fibs_ifroutes1_success"

	epair=$(vnet_mkepair)
	vnet_mkjail ${jname}a ${epair}a

	jexec ${jname}a sysctl net.fibs=2
	
	jexec ${jname}a ifconfig ${epair}a fib 1
	jexec ${jname}a ifconfig ${epair}a inet ${net_dst}1/24
	jexec ${jname}a ifconfig ${epair}a up

	atf_check -s exit:0 -o ignore jexec ${jname}a setfib 1 route -4n get ${net_dst}0/24
	atf_check -o match:"interface: lo0" jexec ${jname}a setfib 1 route -4n get ${net_dst}1
	atf_check -o match:"destination: ${net_dst}1" jexec ${jname}a setfib 1 route -4n get ${net_dst}1
}

fibs_ifroutes1_success_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "fibs_ifroutes1_success"
}


