# $FreeBSD$

. $(atf_get_srcdir)/../common/vnet.subr

atf_test_case "basic" "cleanup"
basic_head()
{
	atf_set descr 'Basic VLAN test'
	atf_set require.user root
}

basic_body()
{
	vnet_init

	epair_vlan=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair_vlan}a
	vnet_mkjail singsing ${epair_vlan}b

	vlan0=$(jexec alcatraz ifconfig vlan create vlandev ${epair_vlan}a \
		vlan 42)
	jexec alcatraz ifconfig ${epair_vlan}a up
	jexec alcatraz ifconfig ${vlan0} 10.0.0.1/24 up

	vlan1=$(jexec singsing ifconfig vlan create vlandev ${epair_vlan}b \
		vlan 42)
	jexec singsing ifconfig ${epair_vlan}b up
	jexec singsing ifconfig ${vlan1} 10.0.0.2/24 up

	atf_check -s exit:0 -o ignore jexec singsing ping -c 1 10.0.0.1
}

basic_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "basic"
}
