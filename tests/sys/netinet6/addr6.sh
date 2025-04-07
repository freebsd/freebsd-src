#!/usr/bin/env atf-sh
#-
# SPDX-License-Identifier: ISC
#
# Copyright (c) 2025 Lexi Winter.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

. $(atf_get_srcdir)/../common/vnet.subr

atf_test_case "addr6_invalid_addr" "cleanup"
addr6_invalid_addr_head()
{
	atf_set descr "adding an invalid IPv6 address returns an error"
	atf_set require.user root
}

addr6_invalid_addr_body()
{
	vnet_init

	ep=$(vnet_mkepair)
	atf_check -s exit:0 ifconfig ${ep}a inet6 2001:db8::1/128
	atf_check -s exit:1 -e ignore ifconfig ${ep}a inet6 2001:db8::1/127 alias
}

addr6_invalid_addr_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "addr6_invalid_addr"
}
