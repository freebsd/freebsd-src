# $FreeBSD$

. $(atf_get_srcdir)/utils.subr

atf_test_case "v4" "cleanup"
v4_head()
{
	atf_set descr 'Basic route-to test'
	atf_set require.user root
}

v4_body()
{
	pft_init

	epair_send=$(vnet_mkepair)
	ifconfig ${epair_send}a 192.0.2.1/24 up
	epair_route=$(vnet_mkepair)
	ifconfig ${epair_route}a 203.0.113.1/24 up

	vnet_mkjail alcatraz ${epair_send}b ${epair_route}b
	jexec alcatraz ifconfig ${epair_send}b 192.0.2.2/24 up
	jexec alcatraz ifconfig ${epair_route}b 203.0.113.2/24 up
	jexec alcatraz route add -net 198.51.100.0/24 192.0.2.1
	jexec alcatraz pfctl -e

	# Attempt to provoke PR 228782
	pft_set_rules alcatraz "block all" "pass user 2" \
		"pass out route-to (${epair_route}b 203.0.113.1) from 192.0.2.2 to 198.51.100.1 no state"
	jexec alcatraz nc -w 3 -s 192.0.2.2 198.51.100.1 22

	# atf wants us to not return an error, but our netcat will fail
	true
}

v4_cleanup()
{
	pft_cleanup
}

atf_test_case "v6" "cleanup"
v6_head()
{
	atf_set descr 'Basic route-to test (IPv6)'
	atf_set require.user root
}

v6_body()
{
	pft_init

	epair_send=$(vnet_mkepair)
	ifconfig ${epair_send}a inet6 2001:db8:42::1/64 up no_dad -ifdisabled
	epair_route=$(vnet_mkepair)
	ifconfig ${epair_route}a inet6 2001:db8:43::1/64 up no_dad -ifdisabled

	vnet_mkjail alcatraz ${epair_send}b ${epair_route}b
	jexec alcatraz ifconfig ${epair_send}b inet6 2001:db8:42::2/64 up no_dad
	jexec alcatraz ifconfig ${epair_route}b inet6 2001:db8:43::2/64 up no_dad
	jexec alcatraz route add -6 2001:db8:666::/64 2001:db8:42::2
	jexec alcatraz pfctl -e

	# Attempt to provoke PR 228782
	pft_set_rules alcatraz "block all" "pass user 2" \
		"pass out route-to (${epair_route}b 2001:db8:43::1) from 2001:db8:42::2 to 2001:db8:666::1 no state"
	jexec alcatraz nc -6 -w 3 -s 2001:db8:42::2 2001:db8:666::1 22

	# atf wants us to not return an error, but our netcat will fail
	true
}

v6_cleanup()
{
	pft_cleanup
}

atf_test_case "multiwan" "cleanup"
multiwan_head()
{
	atf_set descr 'Multi-WAN redirection / reply-to test'
	atf_set require.user root
}

multiwan_body()
{
	pft_init

	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)
	epair_cl_one=$(vnet_mkepair)
	epair_cl_two=$(vnet_mkepair)

	vnet_mkjail srv ${epair_one}b ${epair_two}b
	vnet_mkjail wan_one ${epair_one}a ${epair_cl_one}b
	vnet_mkjail wan_two ${epair_two}a ${epair_cl_two}b
	vnet_mkjail client ${epair_cl_one}a ${epair_cl_two}a

	jexec client ifconfig ${epair_cl_one}a 203.0.113.1/25
	jexec wan_one ifconfig ${epair_cl_one}b 203.0.113.2/25
	jexec wan_one ifconfig ${epair_one}a 192.0.2.1/24 up
	jexec wan_one sysctl net.inet.ip.forwarding=1
	jexec srv ifconfig ${epair_one}b 192.0.2.2/24 up
	jexec client route add 192.0.2.0/24 203.0.113.2

	jexec client ifconfig ${epair_cl_two}a 203.0.113.128/25
	jexec wan_two ifconfig ${epair_cl_two}b 203.0.113.129/25
	jexec wan_two ifconfig ${epair_two}a 198.51.100.1/24 up
	jexec wan_two sysctl net.inet.ip.forwarding=1
	jexec srv ifconfig ${epair_two}b 198.51.100.2/24 up
	jexec client route add 198.51.100.0/24 203.0.113.129

	jexec srv ifconfig lo0 127.0.0.1/8 up
	jexec srv route add default 192.0.2.1
	jexec srv sysctl net.inet.ip.forwarding=1

	# Run echo server in srv jail
	jexec srv /usr/sbin/inetd -p multiwan.pid $(atf_get_srcdir)/echo_inetd.conf

	jexec srv pfctl -e
	pft_set_rules srv \
		"nat on ${epair_one}b inet from 127.0.0.0/8 to any -> (${epair_one}b)" \
		"nat on ${epair_two}b inet from 127.0.0.0/8 to any -> (${epair_two}b)" \
		"rdr on ${epair_one}b inet proto tcp from any to 192.0.2.2 port 7 -> 127.0.0.1 port 7" \
		"rdr on ${epair_two}b inet proto tcp from any to 198.51.100.2 port 7 -> 127.0.0.1 port 7" \
		"block in"	\
		"block out"	\
		"pass in quick on ${epair_one}b reply-to (${epair_one}b 192.0.2.1) inet proto tcp from any to 127.0.0.1 port 7" \
		"pass in quick on ${epair_two}b reply-to (${epair_two}b 198.51.100.1) inet proto tcp from any to 127.0.0.1 port 7"

	# These will always succeed, because we don't change interface to route
	# correctly here.
	result=$(echo "one" | jexec wan_one nc -N -w 3 192.0.2.2 7)
	if [ "${result}" != "one" ]; then
		atf_fail "Redirect on one failed"
	fi
	result=$(echo "two" | jexec wan_two nc -N -w 3 198.51.100.2 7)
	if [ "${result}" != "two" ]; then
		atf_fail "Redirect on two failed"
	fi

	result=$(echo "one" | jexec client nc -N -w 3 192.0.2.2 7)
	if [ "${result}" != "one" ]; then
		atf_fail "Redirect from client on one failed"
	fi

	# This should trigger the issue fixed in 829a69db855b48ff7e8242b95e193a0783c489d9
	result=$(echo "two" | jexec client nc -N -w 3 198.51.100.2 7)
	if [ "${result}" != "two" ]; then
		atf_fail "Redirect from client on two failed"
	fi
}

multiwan_cleanup()
{
	rm -f multiwan.pid
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "v4"
	atf_add_test_case "v6"
	atf_add_test_case "multiwan"
}
