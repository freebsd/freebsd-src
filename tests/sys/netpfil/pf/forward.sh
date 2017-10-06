# $FreeBSD$

. $(atf_get_srcdir)/utils.subr

atf_test_case "v4" "cleanup"
v4_head()
{
	atf_set descr 'Basic forwarding test'
	atf_set require.user root

	# We need scapy to be installed for out test scripts to work
	atf_set require.progs scapy
}

v4_body()
{
	pft_init

	epair_send=$(pft_mkepair)
	ifconfig ${epair_send}a 192.0.2.1/24 up

	epair_recv=$(pft_mkepair)
	ifconfig ${epair_recv}a up

	pft_mkjail alcatraz ${epair_send}b ${epair_recv}b
	jexec alcatraz ifconfig ${epair_send}b 192.0.2.2/24 up
	jexec alcatraz ifconfig ${epair_recv}b 198.51.100.2/24 up
	jexec alcatraz sysctl net.inet.ip.forwarding=1
	jexec alcatraz arp -s 198.51.100.3 00:01:02:03:04:05
	route add -net 198.51.100.0/24 192.0.2.2

	# Sanity check, can we forward ICMP echo requests without pf?
	atf_check -s exit:0 $(atf_get_srcdir)/pft_ping.py \
		--sendif ${epair_send}a \
		--to 198.51.100.3 \
		--recvif ${epair_recv}a

	# Forward with pf enabled
	printf "block in\n" | jexec alcatraz pfctl -ef -
	atf_check -s exit:1 $(atf_get_srcdir)/pft_ping.py \
		--sendif ${epair_send}a \
		--to 198.51.100.3 \
		--recvif ${epair_recv}a

	printf "block out\n" | jexec alcatraz pfctl -f -
	atf_check -s exit:1 $(atf_get_srcdir)/pft_ping.py \
		--sendif ${epair_send}a \
		--to 198.51.100.3 \
		--recv ${epair_recv}a

	# Allow ICMP
	printf "block in\npass in proto icmp\n" | jexec alcatraz pfctl -f -
	atf_check -s exit:0 $(atf_get_srcdir)/pft_ping.py \
		--sendif ${epair_send}a \
		--to 198.51.100.3 \
		--recvif ${epair_recv}a
}

v4_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "v4"
}
