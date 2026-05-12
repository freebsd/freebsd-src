#
# Copyright (c) 2026 Boris Lytochkin
#
# SPDX-License-Identifier: BSD-2-Clause
#

common_dir="$(atf_get_srcdir)/../common"
. ${common_dir}/utils.subr

NC="nc -w 1 -dnN"

setup_network_v6()
{
	epair="$1"

	ifconfig ${epair}a inet6 2001:db8:42::1/64 up no_dad -ifdisabled

	vnet_mkjail alcatraz ${epair}b

	ifconfig -j alcatraz ${epair}b inet6 2001:db8:42::2/64 up no_dad -ifdisabled

	jexec alcatraz /usr/sbin/inetd -p /dev/null $(atf_get_srcdir)/lookup_inetd.conf

	# Sanity checks
	atf_check -s exit:0 -o ignore ping6 -i .1 -c 3 -s 1200 2001:db8:42::2
	atf_check -o "inline:GOOD 82\n" ${NC} 2001:db8:42::2 82
}

atf_test_case "ipv6fl" "cleanup"

ipv6fl_head()
{
	atf_set descr 'flow-id test'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

ipv6fl_body()
{

        firewall_init "ipfw"

        epair=$(vnet_mkepair)

        setup_network_v6 ${epair}

	# Check if the firewall is able to match exact IPv6 flow label
	firewall_config "alcatraz" ipfw ipfw \
			"ipfw -q add 100 allow ip6 from any to any flow-id 0xbaad" \
			"ipfw -q add 200 deny ipv6-icmp from any to any icmp6types 128 in"

	# Check Flow Label matches
	atf_check -s exit:0 ${common_dir}/pft_ping.py \
		--sendif ${epair}a \
		--fromaddr 2001:db8:42::1 \
		--to 2001:db8:42::2 \
		--send-fl $((0xbaad)) \
		--replyif ${epair}a

	# Check Flow Label mismatch
	atf_check -s exit:1 ${common_dir}/pft_ping.py \
		--sendif ${epair}a \
		--fromaddr 2001:db8:42::1 \
		--to 2001:db8:42::2 \
		--send-fl $((0xf001)) \
		--replyif ${epair}a

}

ipv6fl_cleanup()
{
	firewall_cleanup $1
}

atf_init_test_cases()
{
	atf_add_test_case "ipv6fl"
}
