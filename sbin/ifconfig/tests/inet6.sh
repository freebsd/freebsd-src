# SPDX-License-Identifier: ISC
#
# Copyright (c) 2025 Lexi Winter

. $(atf_get_srcdir)/../../sys/common/vnet.subr

# Bug 286910: adding 'netmask' or 'broadcast' to an IPv6 address crashed
# ifconfig.

atf_test_case "netmask" "cleanup"
netmask_head()
{
	atf_set descr "Test invalid 'netmask' option"
	atf_set require.user root
}

netmask_body()
{
	vnet_init

	ep=$(vnet_mkepair)
	vnet_mkjail ifcjail ${ep}a

	# Add the address the wrong way
	atf_check -s exit:1 \
	    -e match:"ifconfig: netmask: invalid option for inet6" \
	    jexec ifcjail ifconfig ${ep}a inet6 2001:db8:1::1 netmask 64

	# Add the address the correct way
	atf_check -s exit:0 \
	    jexec ifcjail ifconfig ${ep}a inet6 2001:db8:1::1/64
	atf_check -s exit:0 -o match:"2001:db8:1::1 prefixlen 64" \
	    jexec ifcjail ifconfig ${ep}a

	# Remove the address the wrong way
	atf_check -s exit:1 \
	    -e match:"ifconfig: netmask: invalid option for inet6" \
	    jexec ifcjail ifconfig ${ep}a inet6 2001:db8:1::1 netmask 64 -alias
}

netmask_cleanup()
{
	vnet_cleanup
}

atf_test_case "broadcast" "cleanup"
broadcast_head()
{
	atf_set descr "Test invalid 'broadcast' option"
	atf_set require.user root
}

broadcast_body()
{
	vnet_init

	ep=$(vnet_mkepair)
	vnet_mkjail ifcjail ${ep}a

	atf_check -s exit:1 \
	    -e match:"ifconfig: broadcast: invalid option for inet6" \
	    jexec ifcjail ifconfig ${ep}a \
	        inet6 2001:db8:1::1 broadcast 2001:db8:1::ffff

	atf_check -s exit:0 \
	    jexec ifcjail ifconfig ${ep}a inet6 2001:db8:1::1/64

	atf_check -s exit:1 \
	    -e match:"ifconfig: broadcast: invalid option for inet6" \
	    jexec ifcjail ifconfig ${ep}a \
	        inet6 2001:db8:1::1 broadcast 2001:db:1::ffff -alias
}

broadcast_cleanup()
{
	vnet_cleanup
}

atf_test_case "delete6" "cleanup"
delete6_head()
{
	atf_set descr 'Test removing IPv6 addresses'
	atf_set require.user root
}

delete6_body()
{
	vnet_init

	ep=$(vnet_mkepair)

	atf_check -s exit:0 \
	    ifconfig ${ep}a inet6 fe80::42/64
	atf_check -s exit:0 -o match:"fe80::42%${ep}" \
	    ifconfig ${ep}a inet6

	atf_check -s exit:0 \
	    ifconfig ${ep}a inet6 -alias fe80::42
	atf_check -s exit:0 -o not-match:"fe80::42%${ep}" \
	    ifconfig ${ep}a inet6
}

delete6_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case netmask
	atf_add_test_case broadcast
	atf_add_test_case delete6
}
