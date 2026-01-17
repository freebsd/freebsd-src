# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Gleb Smirnoff <glebius@FreeBSD.org>
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

. $(atf_get_srcdir)/../common/utils.subr

atf_test_case "fuzz" "cleanup"
fuzz_head()
{
	atf_set descr 'Create couple tables and fuzzes them'
	atf_set require.user root
}

fuzz_body()
{
	firewall_init "ipfw"

	epair=$(vnet_mkepair)
	vnet_mkjail sender ${epair}a
	jexec sender ifconfig ${epair}a 192.0.2.0/31 up
	jexec sender route add 10.0.0.0/8 192.0.2.1

	vnet_mkjail receiver ${epair}b
	jexec receiver ifconfig lo0 127.0.0.1/8 up
	jexec receiver ifconfig ${epair}b 192.0.2.1/31 up
	jexec receiver route add 10.0.0.0/8 -blackhole -iface lo0

	jexec receiver ipfw add 100 count ip from any to table\(tb0\)
	jexec receiver ipfw add 200 count ip from any to table\(tb1\)

	( jexec sender sh -c \
	    'while true; do \
		oct=$(od -An -N1 -tu1 < /dev/urandom); \
		oct=$(echo $oct); \
		ping -c 5 -i .01 -W .01 10.0.0.${oct} >/dev/null; \
	    done' ) &
	pinger=$!

	( jexec receiver sh -c \
	    'while true; do \
		set -- $(od -An -N2 -tu1 < /dev/urandom); \
		ipfw -q table tb$(($1 % 2)) add 10.0.0.$2; \
	     done' ) &
	adder=$!

	( jexec receiver sh -c \
	    'while true; do \
		set -- $(od -An -N2 -tu1 < /dev/urandom); \
		ipfw -q table tb$(($1 % 2)) del 10.0.0.$2; \
	     done' ) &
	deleter=$!

	( jexec receiver sh -c \
	    'while true; do \
		ipfw table tb0 swap tb1; \
		sleep .25; \
	    done' ) &
	swapper=$!

	sleep 30
	kill $pinger
	kill $adder
	kill $deleter
	kill $swapper
}

fuzz_cleanup()
{
	firewall_cleanup $1
}

atf_init_test_cases()
{
	atf_add_test_case "fuzz"
}
