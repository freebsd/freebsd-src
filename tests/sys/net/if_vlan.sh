
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

	# Test changing the vlan ID
	atf_check -s exit:0 \
	    jexec singsing ifconfig ${vlan1} vlandev ${epair_vlan}b vlan 43
	atf_check -s exit:2 -o ignore jexec singsing ping -c 1 10.0.0.1

	# And change back
	# Test changing the vlan ID
	atf_check -s exit:0 \
	    jexec singsing ifconfig ${vlan1} vlandev ${epair_vlan}b vlan 42
	atf_check -s exit:0 -o ignore jexec singsing ping -c 1 10.0.0.1
}

basic_cleanup()
{
	vnet_cleanup
}

# Simple Q-in-Q (802.1Q over 802.1ad)

atf_test_case "qinq_simple" "cleanup"
qinq_simple_head()
{
	atf_set descr 'Simple Q-in-Q test (802.1Q over 802.1ad)'
	atf_set require.user root
}

qinq_simple_body()
{
	vnet_init

	epair_qinq=$(vnet_mkepair)

	vnet_mkjail jqinq0 ${epair_qinq}a
	vnet_mkjail jqinq1 ${epair_qinq}b

	vlan5a=$(jexec jqinq0 ifconfig vlan create \
		vlandev ${epair_qinq}a vlan 5 vlanproto 802.1ad)
	vlan42a=$(jexec jqinq0 ifconfig vlan create \
		vlandev ${vlan5a} vlan 42 vlanproto 802.1q)
	jexec jqinq0 ifconfig ${epair_qinq}a up
	jexec jqinq0 ifconfig ${vlan5a} up
	jexec jqinq0 ifconfig ${vlan42a} 10.5.42.1/24 up

	vlan5b=$(jexec jqinq1 ifconfig vlan create \
		vlandev ${epair_qinq}b vlan 5 vlanproto 802.1ad)
	vlan42b=$(jexec jqinq1 ifconfig vlan create \
		vlandev ${vlan5b} vlan 42 vlanproto 802.1q)
	jexec jqinq1 ifconfig ${epair_qinq}b up
	jexec jqinq1 ifconfig ${vlan5b} up
	jexec jqinq1 ifconfig ${vlan42b} 10.5.42.2/24 up

	atf_check -s exit:0 -o ignore jexec jqinq1 ping -c 1 10.5.42.1
}

qinq_simple_cleanup()
{
	vnet_cleanup
}

# Deep Q-in-Q (802.1Q over 802.1ad over 802.1ad)

atf_test_case "qinq_deep" "cleanup"
qinq_deep_head()
{
	atf_set descr 'Deep Q-in-Q test (802.1Q over 802.1ad over 802.1ad)'
	atf_set require.user root
}

qinq_deep_body()
{
	vnet_init

	epair_qinq=$(vnet_mkepair)

	vnet_mkjail jqinq2 ${epair_qinq}a
	vnet_mkjail jqinq3 ${epair_qinq}b

	vlan5a=$(jexec jqinq2 ifconfig vlan create \
		vlandev ${epair_qinq}a vlan 5 vlanproto 802.1ad)
	vlan6a=$(jexec jqinq2 ifconfig vlan create \
		vlandev ${vlan5a} vlan 6 vlanproto 802.1ad)
	vlan42a=$(jexec jqinq2 ifconfig vlan create \
		vlandev ${vlan6a} vlan 42 vlanproto 802.1q)
	jexec jqinq2 ifconfig ${epair_qinq}a up
	jexec jqinq2 ifconfig ${vlan5a} up
	jexec jqinq2 ifconfig ${vlan6a} up
	jexec jqinq2 ifconfig ${vlan42a} 10.6.42.1/24 up

	vlan5b=$(jexec jqinq3 ifconfig vlan create \
		vlandev ${epair_qinq}b vlan 5 vlanproto 802.1ad)
	vlan6b=$(jexec jqinq3 ifconfig vlan create \
		vlandev ${vlan5b} vlan 6 vlanproto 802.1ad)
	vlan42b=$(jexec jqinq3 ifconfig vlan create \
		vlandev ${vlan6b} vlan 42 vlanproto 802.1q)
	jexec jqinq3 ifconfig ${epair_qinq}b up
	jexec jqinq3 ifconfig ${vlan5b} up
	jexec jqinq3 ifconfig ${vlan6b} up
	jexec jqinq3 ifconfig ${vlan42b} 10.6.42.2/24 up

	atf_check -s exit:0 -o ignore jexec jqinq3 ping -c 1 10.6.42.1
}

qinq_deep_cleanup()
{
	vnet_cleanup
}

# Legacy Q-in-Q (802.1Q over 802.1Q)

atf_test_case "qinq_legacy" "cleanup"
qinq_legacy_head()
{
	atf_set descr 'Legacy Q-in-Q test (802.1Q over 802.1Q)'
	atf_set require.user root
}

qinq_legacy_body()
{
	vnet_init

	epair_qinq=$(vnet_mkepair)

	vnet_mkjail jqinq4 ${epair_qinq}a
	vnet_mkjail jqinq5 ${epair_qinq}b

	vlan5a=$(jexec jqinq4 ifconfig vlan create \
		vlandev ${epair_qinq}a vlan 5)
	vlan42a=$(jexec jqinq4 ifconfig vlan create \
		vlandev ${vlan5a} vlan 42)
	jexec jqinq4 ifconfig ${epair_qinq}a up
	jexec jqinq4 ifconfig ${vlan5a} up
	jexec jqinq4 ifconfig ${vlan42a} 10.5.42.1/24 up

	vlan5b=$(jexec jqinq5 ifconfig vlan create \
		vlandev ${epair_qinq}b vlan 5)
	vlan42b=$(jexec jqinq5 ifconfig vlan create \
		vlandev ${vlan5b} vlan 42)
	jexec jqinq5 ifconfig ${epair_qinq}b up
	jexec jqinq5 ifconfig ${vlan5b} up
	jexec jqinq5 ifconfig ${vlan42b} 10.5.42.2/24 up

	atf_check -s exit:0 -o ignore jexec jqinq5 ping -c 1 10.5.42.1
}

qinq_legacy_cleanup()
{
	vnet_cleanup
}

# Simple Q-in-Q with dot notation

atf_test_case "qinq_dot" "cleanup"
qinq_dot_head()
{
	atf_set descr 'Simple Q-in-Q test with dot notation'
	atf_set require.user root
}

qinq_dot_body()
{
	vnet_init

	epair_qinq=$(vnet_mkepair)

	vnet_mkjail jqinq6 ${epair_qinq}a
	vnet_mkjail jqinq7 ${epair_qinq}b

	jexec jqinq6 ifconfig vlan5 create \
		vlandev ${epair_qinq}a vlan 5 vlanproto 802.1ad
	jexec jqinq6 ifconfig vlan5.42 create \
		vlanproto 802.1q
	jexec jqinq6 ifconfig ${epair_qinq}a up
	jexec jqinq6 ifconfig vlan5 up
	jexec jqinq6 ifconfig vlan5.42 10.5.42.1/24 up

	vlan5b=$(jexec jqinq7 ifconfig vlan create \
		vlandev ${epair_qinq}b vlan 5 vlanproto 802.1ad)
	vlan42b=$(jexec jqinq7 ifconfig vlan create \
		vlandev ${vlan5b} vlan 42 vlanproto 802.1q)
	jexec jqinq7 ifconfig ${epair_qinq}b up
	jexec jqinq7 ifconfig ${vlan5b} up
	jexec jqinq7 ifconfig ${vlan42b} 10.5.42.2/24 up

	atf_check -s exit:0 -o ignore jexec jqinq7 ping -c 1 10.5.42.1
}

qinq_dot_cleanup()
{
	vnet_cleanup
}

atf_test_case "qinq_setflags" "cleanup"
qinq_setflags_head()
{
	atf_set descr 'Test setting flags on a QinQ device'
	atf_set require.user root
}

qinq_setflags_body()
{
	vnet_init

	epair=$(vnet_mkepair)

	ifconfig ${epair}a up
	vlan1=$(ifconfig vlan create)
	ifconfig $vlan1 vlan 1 vlandev ${epair}a
	vlan2=$(ifconfig vlan create)
	ifconfig $vlan2 vlan 2 vlandev $vlan1

	# This panics, incorrect locking
	ifconfig $vlan2 promisc
}

qinq_setflags_cleanup()
{
	vnet_cleanup
}

atf_test_case "bpf_pcp" "cleanup"
bpf_pcp_head()
{
	atf_set descr 'Set VLAN PCP through BPF'
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

	jexec alcatraz sysctl net.link.vlan.mtag_pcp=1

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
	atf_add_test_case "qinq_simple"
	atf_add_test_case "qinq_deep"
	atf_add_test_case "qinq_legacy"
	atf_add_test_case "qinq_dot"
	atf_add_test_case "qinq_setflags"
	atf_add_test_case "bpf_pcp"
}
