#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 KUROSAWA Takahiro <takahiro.kurosawa@gmail.com>
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

HELPER=$(atf_get_srcdir)/../../common/sendfile_helper

# pf must call mb_unmapped_to_ext() before passing an output mbuf to a network
# interface if it does not accept unmapped mbufs as ip_output() does. These
# tests make sure that unmapped mbufs are correctly converted to mapped ones
# before pf passes mbufs to interfaces.

atf_test_case "v4" "cleanup"
v4_head() {
	atf_set descr 'unmapped mbuf IPv4 test'
	atf_set require.user root
}

v4_body() {
	pft_init

	l=$(vnet_mkepair)
	vnet_mkjail a ${l}a
	jexec a ifconfig ${l}a 192.0.2.1/24 up
	vnet_mkjail b ${l}b
	jexec b ifconfig ${l}b 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore jexec a ping -c 1 192.0.2.2

	jexec a pfctl -e
	pft_set_rules a "pass out on ${l}a route-to (${l}a 192.0.2.2) all"
	jexec b timeout 30s nc -4 -d -l 2345 > out &
	sleep 1
	atf_check -s exit:0 -o ignore \
	    jexec a ${HELPER} -c 192.0.2.2 -p 2345 ${HELPER} 0 512 0
	wait
	atf_check_equal $(dd bs=512 count=1 if=${HELPER} | sha256 -q) \
	    $(sha256 -q out)
}

v4_cleanup() {
	pft_cleanup
	rm -f out
}

atf_test_case "v6" "cleanup"
v6_head() {
	atf_set descr 'unmapped mbuf IPv6 test'
	atf_set require.user root
}

v6_body() {
	pft_init

	l=$(vnet_mkepair)
	vnet_mkjail a ${l}a
	jexec a ifconfig ${l}a inet6 2001:db8::1/64 up
	vnet_mkjail b ${l}b
	jexec b ifconfig ${l}b inet6 2001:db8::2/64 up

	# Sanity check
	atf_check -s exit:0 -o ignore jexec a ping -c 1 2001:db8::2

	jexec a pfctl -e
	pft_set_rules a "pass out on ${l}a route-to (${l}a 2001:db8::2) all"
	jexec b timeout 30s nc -6 -d -l 2345 > out &
	sleep 1
	atf_check -s exit:0 -o ignore \
	    jexec a ${HELPER} -c 2001:db8::2 -p 2345 ${HELPER} 0 512 0
	wait
	atf_check_equal $(dd bs=512 count=1 if=${HELPER} | sha256 -q) \
	    $(sha256 -q out)
}

v6_cleanup() {
	pft_cleanup
	rm -f out
}

atf_test_case "dummynet" "cleanup"
dummynet_head() {
	atf_set descr 'unmapped mbuf dummynet test'
	atf_set require.user root
}

dummynet_body() {
	pft_init

	l=$(vnet_mkepair)
	vnet_mkjail a ${l}a
	jexec a ifconfig ${l}a 192.0.2.1/24 up
	vnet_mkjail b ${l}b
	jexec b ifconfig ${l}b 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore jexec a ping -c 1 192.0.2.2

	atf_check -s exit:0 -o ignore \
	    jexec a dnctl pipe 1 config delay 100
	jexec a pfctl -e
	pft_set_rules a "pass out on ${l}a route-to (${l}a 192.0.2.2) all dnpipe 1"
	jexec b timeout 30s nc -4 -d -l 2345 > out &
	sleep 1
	atf_check -s exit:0 -o ignore \
	    jexec a ${HELPER} -c 192.0.2.2 -p 2345 ${HELPER} 0 512 0
	wait
	atf_check_equal $(dd bs=512 count=1 if=${HELPER} | sha256 -q) \
	    $(sha256 -q out)
}

dummynet_cleanup() {
	pft_cleanup
	rm -f out
}

atf_init_test_cases()
{
	atf_add_test_case "v4"
	atf_add_test_case "v6"
	atf_add_test_case "dummynet"
}
