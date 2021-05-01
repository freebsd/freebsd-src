#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (C) 2019 Jan Sucan <jansucan@FreeBSD.org>
# All rights reserved.
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

atf_test_case ping_c1_s56_t1
ping_c1_s56_t1_head() {
    atf_set "descr" "Stop after receiving 1 ECHO_RESPONSE packet"
}
ping_c1_s56_t1_body() {
    if ! getaddrinfo -f inet localhost 1>/dev/null 2>&1; then
	atf_skip "IPv4 is not configured"
    fi
    atf_check -s exit:0 -o save:std.out -e empty \
	      ping -4 -c 1 -s 56 -t 1 localhost
    check_ping_statistics std.out $(atf_get_srcdir)/ping_c1_s56_t1.out
}

atf_test_case ping_6_c1_s8_t1
ping_6_c1_s8_t1_head() {
    atf_set "descr" "Stop after receiving 1 ECHO_RESPONSE packet"
}
ping_6_c1_s8_t1_body() {
    if ! getaddrinfo -f inet6 localhost 1>/dev/null 2>&1; then
	atf_skip "IPv6 is not configured"
    fi
    atf_check -s exit:0 -o save:std.out -e empty \
	      ping -6 -c 1 -s 8 -t 1 localhost
    check_ping_statistics std.out $(atf_get_srcdir)/ping_6_c1_s8_t1.out
}

atf_test_case ping6_c1_s8_t1
ping6_c1_s8_t1_head() {
    atf_set "descr" "Use IPv6 when invoked as ping6"
}
ping6_c1_s8_t1_body() {
    if ! getaddrinfo -f inet6 localhost 1>/dev/null 2>&1; then
	atf_skip "IPv6 is not configured"
    fi
    atf_check -s exit:0 -o save:std.out -e empty \
	      ping6 -c 1 -s 8 -t 1 localhost
    check_ping_statistics std.out $(atf_get_srcdir)/ping_6_c1_s8_t1.out
}

atf_init_test_cases() {
    atf_add_test_case ping_c1_s56_t1
    atf_add_test_case ping_6_c1_s8_t1
    atf_add_test_case ping6_c1_s8_t1
}

check_ping_statistics() {
    sed -e 's/0.[0-9]\{3\}//g' \
	-e 's/[1-9][0-9]*.[0-9]\{3\}//g' \
	-e 's/localhost ([0-9]\{1,3\}\(\.[0-9]\{1,3\}\)\{1,3\})/localhost/' \
	-e 's/from [0-9]\{1,3\}\(\.[0-9]\{1,3\}\)\{1,3\}/from/' \
	-e 's/ttl=[0-9][0-9]*/ttl=/' \
	-e 's/hlim=[0-9][0-9]*/hlim=/' \
	"$1" >"$1".filtered
    atf_check -s exit:0 diff -u "$1".filtered "$2"
}
