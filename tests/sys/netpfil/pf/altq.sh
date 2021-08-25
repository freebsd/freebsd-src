. $(atf_get_srcdir)/utils.subr

atf_test_case "hfsc" "cleanup"
hfsc_head()
{
	atf_set descr 'Basic HFSC test'
	atf_set require.user root
}

hfsc_body()
{
	altq_init
	is_altq_supported hfsc

	epair=$(vnet_mkepair)
	vnet_mkjail altq_hfsc ${epair}b

	ifconfig ${epair}a 192.0.2.1/24 up
	jexec altq_hfsc ifconfig ${epair}b 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore ping -i .1 -c 3 -s 1200 192.0.2.2

	jexec altq_hfsc pfctl -e
	pft_set_rules altq_hfsc \
	    "altq on ${epair}b bandwidth 100b hfsc queue { default }" \
	    "queue default hfsc(default linkshare 80b)" \
	    "pass proto icmp "

	# single ping succeeds just fine
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	# "Saturate the link"
	ping -i .1 -c 5 -s 1200 192.0.2.2

	# We should now be hitting the limits and get this packet dropped.
	atf_check -s exit:2 -o ignore ping -c 1 -s 1200 192.0.2.2
}

hfsc_cleanup()
{
	altq_cleanup
}

atf_test_case "match" "cleanup"
match_head()
{
	atf_set descr 'Basic match keyword test'
	atf_set require.user root
}

match_body()
{
	altq_init
	is_altq_supported hfsc

	epair=$(vnet_mkepair)
	vnet_mkjail altq_match ${epair}b

	ifconfig ${epair}a 192.0.2.1/24 up
	jexec altq_match ifconfig ${epair}b 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore ping -i .1 -c 3 -s 1200 192.0.2.2

	jexec altq_match pfctl -e
	pft_set_rules altq_match \
	    "altq on ${epair}b bandwidth 100000000b hfsc queue { default, slow }" \
	    "queue default hfsc(default linkshare 80000000b)" \
	    "queue slow hfsc(linkshare 80b upperlimit 80b)" \
	    "match proto icmp queue slow" \
	    "pass"

	# single ping succeeds just fine
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	# "Saturate the link"
	ping -i .1 -c 5 -s 1200 192.0.2.2

	# We should now be hitting the limits and get this packet dropped.
	atf_check -s exit:2 -o ignore ping -c 1 -s 1200 192.0.2.2
}

match_cleanup()
{
	altq_cleanup
}

atf_test_case "cbq_vlan" "cleanup"
cbq_vlan_head()
{
	atf_set descr 'CBQ over VLAN test'
	atf_set require.user root
}

cbq_vlan_body()
{
	altq_init
	is_altq_supported cbq

	epair=$(vnet_mkepair)
	vnet_mkjail altq_cbq_vlan ${epair}b

	vlan=$(vnet_mkvlan)
	ifconfig ${vlan} vlan 42 vlandev ${epair}a
	ifconfig ${vlan} 192.0.2.1/24 up
	ifconfig ${epair}a up

	vlanj=$(jexec altq_cbq_vlan ifconfig vlan create)
	echo ${vlanj} >> created_interfaces.lst

	jexec altq_cbq_vlan ifconfig ${epair}b up
	jexec altq_cbq_vlan ifconfig ${vlanj} vlan 42 vlandev ${epair}b
	jexec altq_cbq_vlan ifconfig ${vlanj} 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore ping -i .1 -c 3 -s 1200 192.0.2.2

	jexec altq_cbq_vlan pfctl -e
	pft_set_rules altq_cbq_vlan \
		"altq on ${vlanj} bandwidth 14000b cbq queue { default }" \
		"queue default bandwidth 14000b cbq(default) { slow } " \
		"queue slow bandwidth 6000b cbq(borrow)" \
		"match proto icmp queue slow" \
		"match proto tcp queue default" \
		"pass"

	# single ping succeeds just fine
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	# "Saturate the link"
	ping -i .01 -c 50 -s 1200 192.0.2.2

	# We should now be hitting the limits and get these packet dropped.
	rcv=$(ping -i .1 -c 5 -s 1200 192.0.2.2 | tr "," "\n" | awk '/packets received/ { print $1; }')
	echo "Received $rcv packets"
	if [ ${rcv} -gt 1 ]
	then
		atf_fail "Received ${rcv} packets in a saturated link"
	fi
}

cbq_vlan_cleanup()
{
	altq_cleanup
}

atf_test_case "codel_bridge" "cleanup"
codel_bridge_head()
{
	atf_set descr 'codel over if_bridge test'
	atf_set require.user root
}

codel_bridge_body()
{
	altq_init
	is_altq_supported codel

	epair=$(vnet_mkepair)
	ifconfig ${epair}a 192.0.2.1/24 up

	vnet_mkjail altq_codel_bridge ${epair}b

	bridge=$(jexec altq_codel_bridge ifconfig bridge create)
	jexec altq_codel_bridge ifconfig ${bridge} addm ${epair}b
	jexec altq_codel_bridge ifconfig ${epair}b up
	jexec altq_codel_bridge ifconfig ${bridge} 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore ping -i .1 -c 3 -s 1200 192.0.2.2

	jexec altq_codel_bridge pfctl -e
	pft_set_rules altq_codel_bridge \
		"altq on ${bridge} bandwidth 1000b codelq queue { slow }" \
		"match queue slow" \
		"pass"

	# "Saturate the link"
	ping -i .1 -c 5 -s 1200 192.0.2.2

	# We should now be hitting the limits and get these packet dropped.
	rcv=$(ping -i .1 -c 5 -s 1200 192.0.2.2 | tr "," "\n" | awk '/packets received/ { print $1; }')
	echo "Received $rcv packets"
	if [ ${rcv} -gt 1 ]
	then
		atf_fail "Received ${rcv} packets in a saturated link"
	fi
}

codel_bridge_cleanup()
{
	altq_cleanup
}

atf_test_case "prioritise" "cleanup"
prioritise_head()
{
	atf_set descr "Test prioritising one type of traffic over the other"
	atf_set require.user root
}

prioritise_body()
{
	altq_init
	is_altq_supported cbq

	epair=$(vnet_mkepair)
	vnet_mkjail altq_prioritise ${epair}b

	ifconfig ${epair}a 192.0.2.1/24 up
	jexec altq_prioritise ifconfig ${epair}b 192.0.2.2/24 up

	jexec altq_prioritise /usr/sbin/inetd -p inetd-altq.pid \
	    $(atf_get_srcdir)/../pf/echo_inetd.conf

	# Sanity check
	atf_check -s exit:0 -o ignore ping -i .1 -c 3 -s 1200 192.0.2.2

	jexec altq_prioritise pfctl -e
	pft_set_rules altq_prioritise \
		"altq on ${epair}b bandwidth 6000b cbq queue { default, slow }" \
		"queue default priority 7 cbq(default)" \
		"queue slow priority 1 cbq" \
		"match proto icmp queue slow" \
		"match proto tcp queue default" \
		"pass"

	# single ping succeeds just fine
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	# Unsaturated TCP succeeds
	reply=$(echo "foo" | nc -w 5 -N 192.0.2.2 7)
	if [ "$reply" != "foo" ];
	then
		atf_fail "Unsaturated echo failed"
	fi

	# "Saturate the link"
	ping -i .01 -c 50 -s 1200 192.0.2.2

	# We should now be hitting the limits and get these packet dropped.
	rcv=$(ping -i .1 -c 5 -s 1200 192.0.2.2 | tr "," "\n" | awk '/packets received/ { print $1; }')
	echo "Received $rcv packets"
	if [ ${rcv} -gt 1 ]
	then
		atf_fail "Received ${rcv} packets in a saturated link"
	fi

	# TCP should still pass
	for i in `seq 1 10`
	do
		reply=$(echo "foo_${i}" | nc -w 5 -N 192.0.2.2 7)
		if [ "$reply" != "foo_${i}" ];
		then
			atf_fail "Prioritised echo failed ${i}"
		fi

	done

	# Now reverse priority
	pft_set_rules altq_prioritise \
		"altq on ${epair}b bandwidth 6000b cbq queue { default, slow }" \
		"queue default priority 7 cbq(default)" \
		"queue slow priority 1 cbq" \
		"match proto tcp queue slow" \
		"match proto icmp queue default" \
		"pass"

	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2
	ping -i .01 -c 50 -s 1200 192.0.2.2
	for i in `seq 1 10`
	do
		reply=$(echo "foo_${i}" | nc -w 5 -N 192.0.2.2 7)
		if [ "$reply" == "foo_${i}" ];
		then
			atf_fail "Unexpected echo success"
		fi

	done
}

prioritise_cleanup()
{
	altq_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "hfsc"
	atf_add_test_case "match"
	atf_add_test_case "cbq_vlan"
	atf_add_test_case "codel_bridge"
	atf_add_test_case "prioritise"
}
