# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Stormshield

. $(atf_get_srcdir)/../../sys/common/vnet.subr

atf_test_case "badfib" "cleanup"
badfib_head()
{
	atf_set descr "Test adding an interface to a non-existent FIB"
	atf_set require.user root
}
badfib_body()
{
	local epair

	vnet_init

	epair=$(vnet_mkepair)
	atf_check -s exit:0 ifconfig ${epair}a fib 0
	atf_check -s not-exit:0 -e not-empty \
	    ifconfig ${epair}a fib $(sysctl -n net.fibs)
}
badfib_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case badfib
}
