#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 Rubicon Communications, LLC (Netgate)
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

. $(atf_get_srcdir)/utils.subr

common_dir=$(atf_get_srcdir)/../common

atf_test_case "zero_id" "cleanup"
zero_id_head()
{
	atf_set descr 'Test ICMPv6 echo with ID 0 keep being blocked'
	atf_set require.user root
	atf_set require.progs python3 scapy
}

zero_id_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a inet6 2001:db8::2/64 up no_dad

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b inet6 2001:db8::1/64 up no_dad

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    ping -c 1 2001:db8::1

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"set block-policy drop" \
		"antispoof quick for { egress ${epair}b }" \
		"block all" \
		"pass out" \
		"pass in quick inet6 proto IPV6-ICMP icmp6-type 135" \
		"pass in quick inet6 proto IPV6-ICMP icmp6-type 136" \
		"pass out quick inet6 proto IPV6 from self to any"

	# Now we can't ping
	atf_check -s exit:2 -o ignore \
	    ping -c 1 2001:db8::1

	# Force neighbour discovery
	ndp -d 2001:db8::1

	# Verify that we don't confuse echo request with ID 0 for neighbour discovery
	atf_check -s exit:1 -o ignore \
	     ${common_dir}/pft_ping.py \
	         --sendif ${epair}a \
	         --to 2001:db8::1 \
	         --replyif ${epair}a

	jexec alcatraz pfctl -ss -vv
	jexec alcatraz pfctl -sr -vv
}

zero_id_cleanup()
{
	pft_cleanup
}

atf_test_case "ttl_exceeded" "cleanup"
ttl_exceeded_head()
{
	atf_set descr 'Test that we correctly translate TTL exceeded back'
	atf_set require.user root
}

ttl_exceeded_body()
{
	pft_init

	epair_srv=$(vnet_mkepair)
	epair_int=$(vnet_mkepair)
	epair_cl=$(vnet_mkepair)

	vnet_mkjail srv ${epair_srv}a
	jexec srv ifconfig ${epair_srv}a inet6 2001:db8:1::1/64 no_dad up
	jexec srv route add -6 default 2001:db8:1::2

	vnet_mkjail int ${epair_srv}b ${epair_int}a
	jexec int sysctl net.inet6.ip6.forwarding=1
	jexec int ifconfig ${epair_srv}b inet6 2001:db8:1::2/64 no_dad up
	jexec int ifconfig ${epair_int}a inet6 2001:db8:2::2/64 no_dad up

	vnet_mkjail nat ${epair_int}b ${epair_cl}b
	jexec nat ifconfig ${epair_int}b inet6 2001:db8:2::1 no_dad up
	jexec nat ifconfig ${epair_cl}b inet6 2001:db8:3::2/64 no_dad up
	jexec nat sysctl net.inet6.ip6.forwarding=1
	jexec nat route add -6 default 2001:db8:2::2

	vnet_mkjail cl ${epair_cl}a
	jexec cl ifconfig ${epair_cl}a inet6 2001:db8:3::1/64 no_dad up
	jexec cl route add -6 default 2001:db8:3::2

	jexec nat pfctl -e
	pft_set_rules nat \
	    "nat on ${epair_int}b from 2001:db8:3::/64 -> (${epair_int}b:0)" \
	    "block" \
	    "pass inet6 proto udp" \
	    "pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv, echoreq }"

	# Sanity checks
	atf_check -s exit:0 -o ignore \
	    jexec cl ping -c 1 2001:db8:3::2
	atf_check -s exit:0 -o ignore \
	    jexec cl ping -c 1 2001:db8:2::1
	atf_check -s exit:0 -o ignore \
	    jexec cl ping -c 1 2001:db8:2::2
	atf_check -s exit:0 -o ignore \
	    jexec cl ping -c 1 2001:db8:1::1

	echo "UDP"
	atf_check -s exit:0 -e ignore -o match:".*2001:db8:2::2.*" \
	    jexec cl traceroute6 2001:db8:1::1
	jexec nat pfctl -Fs

	echo "ICMP"
	atf_check -s exit:0 -e ignore -o match:".*2001:db8:2::2.*" \
	    jexec cl traceroute6 -I 2001:db8:1::1
}

ttl_exceeded_cleanup()
{
	pft_cleanup
}

atf_test_case "repeat" "cleanup"
repeat_head()
{
	atf_set descr 'Ensure that repeated NDs work'
	atf_set require.user root
	atf_set require.progs ndisc6
}

repeat_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a inet6 2001:db8::2/64 up no_dad

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b inet6 2001:db8::1/64 up no_dad

	# Sanity check
	atf_check -s exit:0 -o ignore \
	    ping -c 1 2001:db8::1

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"block all" \
		"pass quick inet6 proto ipv6-icmp all icmp6-type neighbrsol keep state (if-bound) ridentifier 1000000107"

	jexec alcatraz pfctl -x loud
	  ndisc6 -m -n -r 1 2001:db8::1 ${epair}a
	jexec alcatraz pfctl -ss -vv

	atf_check -s exit:0 -o ignore \
	  ndisc6 -m -n -r 1 2001:db8::1 ${epair}a
	jexec alcatraz pfctl -ss -vv
	atf_check -s exit:0 -o ignore \
	  ndisc6 -m -n -r 1 2001:db8::1 ${epair}a
	jexec alcatraz pfctl -ss -vv
	atf_check -s exit:0 -o ignore \
	  ndisc6 -m -n -r 1 2001:db8::1 ${epair}a
	jexec alcatraz pfctl -ss -vv
}

repeat_cleanup()
{
	pft_cleanup
}

atf_test_case "nat_nud" "cleanup"
nat_nud_head()
{
	atf_set descr 'Test that NUD works when pf translates the probes'
	atf_set require.user root
}

nat_nud_body()
{
	pft_init

	epair=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair}a
	jexec alcatraz ifconfig ${epair}a inet6 no_dad
	jexec alcatraz ifconfig ${epair}a inet6 2001:db8::1/64 up
	# Probe a stale neighbour one second after using it, not five.
	jexec alcatraz sysctl net.inet6.icmp6.nd6_delay=1

	vnet_mkjail singsing ${epair}b
	jexec singsing ifconfig ${epair}b inet6 no_dad
	jexec singsing ifconfig ${epair}b inet6 2001:db8::2/64 up

	lla=$(jexec alcatraz ifconfig ${epair}a inet6 | \
	    awk '/fe80:/ { sub(/%.*/, "", $2); print $2 }')
	llb=$(jexec singsing ifconfig ${epair}b inet6 | \
	    awk '/fe80:/ { sub(/%.*/, "", $2); print $2 }')

	# NUD probes are sent from the link-local address, so this translates
	# them, and the advertisements answering them come back to 2001:db8::1.
	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
	    "nat on ${epair}a inet6 from fe80::/10 to any -> 2001:db8::1" \
	    "pass"

	# singsing resolving alcatraz leaves it stale in alcatraz's neighbour
	# cache, and alcatraz answering uses it, so alcatraz will probe it.
	atf_check -s exit:0 -o ignore \
	    jexec singsing ping -6 -c 1 ${lla}%${epair}b

	# The advertisement answering the probe makes the neighbour reachable.
	# A corrupted one is dropped: alcatraz probes twice more, then deletes
	# the entry.
	state=
	for i in $(seq 1 15); do
		state=$(jexec alcatraz ndp -n ${llb}%${epair}a 2>/dev/null | \
		    awk 'NR == 2 { print $5 }')
		if [ "${state}" = "R" ]; then
			break
		fi
		sleep 1
	done

	jexec alcatraz ndp -an
	jexec alcatraz pfctl -ss -vv

	if [ "${state}" != "R" ]; then
		atf_fail "NUD failed, neighbour state '${state:-no entry}'"
	fi
	atf_check -o match:"^[[:space:]]*0 bad checksums$" \
	    jexec alcatraz netstat -s -p icmp6

	# The probe was translated
	atf_check -o match:"2001:db8::1 \(${lla}\) -> ${llb}\[135\]" \
	    jexec alcatraz pfctl -ss
}

nat_nud_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "zero_id"
	atf_add_test_case "ttl_exceeded"
	atf_add_test_case "repeat"
	atf_add_test_case "nat_nud"
}
