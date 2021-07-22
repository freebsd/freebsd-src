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

atf_test_case "bpf_pcp" "cleanup"
bpf_pcp_head()
{
	atf_set descr 'Set VLAN PCP through BPF'
	atf_set require.config 'allow_sysctl_side_effects'
	atf_set require.user root
	atf_set require.progs scapy
}

bpf_pcp_body()
{
	vnet_init

	epair=$(vnet_mkepair)

	ifconfig ${epair}a up

	vnet_mkjail alcatraz ${epair}b
	vlan=$(jexec alcatraz ifconfig vlan create)
	jexec alcatraz ifconfig ${vlan} vlan 42 vlandev ${epair}b
	jexec alcatraz ifconfig ${vlan} up
	jexec alcatraz ifconfig ${epair}b up

	sysctl net.link.vlan.mtag_pcp=1

	jexec alcatraz dhclient ${vlan} &
	atf_check -s exit:1 -o ignore -e ignore $(atf_get_srcdir)/pcp.py \
		--expect-pcp 6 \
		--recvif ${epair}a

	jexec alcatraz killall dhclient
	sleep 1

	jexec alcatraz dhclient -c $(atf_get_srcdir)/dhclient_pcp.conf ${vlan} &
	atf_check -s exit:0 -o ignore -e ignore $(atf_get_srcdir)/pcp.py \
		--expect-pcp 6 \
		--recvif ${epair}a
}

bpf_pcp_cleanup()
{
	sysctl net.link.vlan.mtag_pcp=0
	jexec alcatraz killall dhclient
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "basic"
	atf_add_test_case "bpf_pcp"
}
