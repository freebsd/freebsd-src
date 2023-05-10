#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 The FreeBSD Foundation
#
# This software was developed by Li-Wen Hsu <lwhsu@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
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
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$

CAPSICUM_TEST_BIN=capsicum-test

atf_test_case "test_root"
test_root_head() {

	atf_set descr 'Run capsicum-test as root'
	atf_set require.user root
}

test_root_body() {
	atf_check -s exit:0 -o match:PASSED -e ignore \
		"$(atf_get_srcdir)/${CAPSICUM_TEST_BIN}" -u "$(id -u tests)"
}

atf_test_case "test_unprivileged"
test_unprivileged_head() {

	atf_set descr 'Run capsicum-test as an unprivileged user'
	atf_set require.user unprivileged
}

test_unprivileged_body() {
	atf_check -s exit:0 -o match:PASSED -e ignore \
		"$(atf_get_srcdir)/${CAPSICUM_TEST_BIN}" -u "$(id -u)"
}

atf_init_test_cases() {
	atf_add_test_case test_root
	atf_add_test_case test_unprivileged
}
