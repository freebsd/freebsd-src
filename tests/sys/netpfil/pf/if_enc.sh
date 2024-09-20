#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 Igor Ostapenko <pm@igoro.pro>
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

#
# The following network is used as a base for testing.
#
#
#                      ${awan}b |----------| ${bwan}b
#                       2.0.0.1 | host wan | 3.0.0.1
#                         .---->| Internet |<----.
#                   A WAN |     |----------|     | B WAN
#                         |                      |
#  Office A side          |                      |            Office B side
#                         | ${awan}a    ${bwan}a |
#                         v 2.0.0.22    3.0.0.33 v
#           ${alan}b |----------|           |----------| ${blan}b
#            1.0.0.1 | host agw |           | host bgw | 4.0.0.1
#       .----------->| gateway  | < IPsec > | gateway  |<-----------.
#       | A LAN      |----------|   tunnel  |----------|      B LAN |
#       |                                                           |
#       |                                                           |
#       | ${alan}a                                         ${blan}a |
#       v 1.0.0.11                                         4.0.0.44 v
#  |----------|                                                |----------|
#  |  host a  |                                                |  host b  |
#  |  client  |                                                |  client  |
#  |----------|                                                |----------|
#
#
# There is routing between office A clients and office B ones. The traffic is
# encrypted, i.e. host wan should see IPsec flow (ESP packets).
#

ipsec_init()
{
	if ! sysctl -q kern.features.ipsec >/dev/null ; then
		atf_skip "This test requires ipsec"
	fi
}

if_enc_init()
{
	ipsec_init
	if ! kldstat -q -m if_enc; then
		atf_skip "This test requires if_enc"
	fi
}

ipfw_init()
{
	if ! kldstat -q -m ipfw; then
		atf_skip "This test requires ipfw"
	fi
}

assert_ipfw_is_off()
{
	if kldstat -q -m ipfw; then
		atf_skip "This test is for the case when ipfw is not loaded"
	fi
}

build_test_network()
{
	local ipfwon=$1

	alan=$(vnet_mkepair)
	awan=$(vnet_mkepair)
	bwan=$(vnet_mkepair)
	blan=$(vnet_mkepair)

	# host a
	vnet_mkjail a ${alan}a
	jexec a ifconfig ${alan}a 1.0.0.11/24 up
	jexec a route add default 1.0.0.1
	test $ipfwon && jexec a ipfw add 65534 allow all from any to any

	# host agw
	vnet_mkjail agw ${alan}b ${awan}a
	jexec agw ifconfig ${alan}b 1.0.0.1/24 up
	jexec agw ifconfig ${awan}a 2.0.0.22/24 up
	jexec agw route add default 2.0.0.1
	jexec agw sysctl net.inet.ip.forwarding=1
	test $ipfwon && jexec agw ipfw add 65534 allow all from any to any

	# host wan
	vnet_mkjail wan ${awan}b ${bwan}b
	jexec wan ifconfig ${awan}b 2.0.0.1/24 up
	jexec wan ifconfig ${bwan}b 3.0.0.1/24 up
	jexec wan sysctl net.inet.ip.forwarding=1
	test $ipfwon && jexec wan ipfw add 65534 allow all from any to any

	# host bgw
	vnet_mkjail bgw ${bwan}a ${blan}b
	jexec bgw ifconfig ${bwan}a 3.0.0.33/24 up
	jexec bgw ifconfig ${blan}b 4.0.0.1/24 up
	jexec bgw route add default 3.0.0.1
	jexec bgw sysctl net.inet.ip.forwarding=1
	test $ipfwon && jexec bgw ipfw add 65534 allow all from any to any

	# host b
	vnet_mkjail b ${blan}a
	jexec b ifconfig ${blan}a 4.0.0.44/24 up
	jexec b route add default 4.0.0.1
	test $ipfwon && jexec b ipfw add 65534 allow all from any to any

	# Office A VPN setup
	echo '
		spdadd 1.0.0.0/24 4.0.0.0/24 any -P out ipsec esp/tunnel/2.0.0.22-3.0.0.33/require;
		spdadd 4.0.0.0/24 1.0.0.0/24 any -P in  ipsec esp/tunnel/3.0.0.33-2.0.0.22/require;
		add 2.0.0.22 3.0.0.33 esp 0x203 -E aes-gcm-16 "123456789012345678901234567890123456";
		add 3.0.0.33 2.0.0.22 esp 0x302 -E aes-gcm-16 "123456789012345678901234567890123456";
	' | jexec agw setkey -c

	# Office B VPN setup
	echo '
		spdadd 4.0.0.0/24 1.0.0.0/24 any -P out ipsec esp/tunnel/3.0.0.33-2.0.0.22/require;
		spdadd 1.0.0.0/24 4.0.0.0/24 any -P in  ipsec esp/tunnel/2.0.0.22-3.0.0.33/require;
		add 2.0.0.22 3.0.0.33 esp 0x203 -E aes-gcm-16 "123456789012345678901234567890123456";
		add 3.0.0.33 2.0.0.22 esp 0x302 -E aes-gcm-16 "123456789012345678901234567890123456";
	' | jexec bgw setkey -c
}

atf_test_case "ipfwoff_ip4_pfil_in_after_stripping" "cleanup"
ipfwoff_ip4_pfil_in_after_stripping_head()
{
	atf_set descr 'Test that pf pulls up mbuf if m_len==0 after stripping the outer header, with ipfw disabled'
	atf_set require.user root
	atf_set require.progs nc
}
ipfwoff_ip4_pfil_in_after_stripping_body()
{
	local ipfwon

	pft_init
	if_enc_init
	test "$1" == "ipfwon" && ipfwon="yes"
	test $ipfwon && ipfw_init || assert_ipfw_is_off

	build_test_network $ipfwon

	# Sanity check
	atf_check -s exit:0 -o ignore jexec a ping -c3 4.0.0.44

	# Configure port forwarding on host bgw
	jexec bgw ifconfig enc0 up
	jexec bgw sysctl net.inet.ipsec.filtertunnel=0
	jexec bgw sysctl net.enc.in.ipsec_filter_mask=2		# after stripping
	jexec bgw sysctl net.enc.out.ipsec_filter_mask=1	# before outer header
	jexec bgw pfctl -e
	pft_set_rules bgw \
		"rdr on enc0 proto tcp to 4.0.0.1 port 666 -> 4.0.0.44" \
		"pass"

	# Prepare the catcher on host b
	echo "unexpected" > ./receiver
	jexec b nc -n4l -N 666 > ./receiver &
	nc_pid=$!
	sleep 1

	# Poke it from host a to host bgw
	spell="Ak Ohum Oktay Weez Barsoom."
	echo $spell | jexec a nc -w3 4.0.0.1 666

	# Expect it to hit host b instead
	sleep 1				# let the catcher finish
	jexec b kill -KILL $nc_pid	# in a fail case the catcher may listen forever
	atf_check_equal "$spell" "$(cat ./receiver)"
}
ipfwoff_ip4_pfil_in_after_stripping_cleanup()
{
	pft_cleanup
}

atf_test_case "ipfwon_ip4_pfil_in_after_stripping" "cleanup"
ipfwon_ip4_pfil_in_after_stripping_head()
{
	atf_set descr 'Test that pf pulls up mbuf if m_len==0 after stripping the outer header, with ipfw enabled'
	atf_set require.user root
	atf_set require.progs nc
}
ipfwon_ip4_pfil_in_after_stripping_body()
{
	ipfwoff_ip4_pfil_in_after_stripping_body "ipfwon"
}
ipfwon_ip4_pfil_in_after_stripping_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "ipfwoff_ip4_pfil_in_after_stripping"
	atf_add_test_case "ipfwon_ip4_pfil_in_after_stripping"
}
