#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Ahsan Barkati
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
#
#

. $(atf_get_srcdir)/utils.subr
. $(atf_get_srcdir)/runner.subr

basic_head()
{
	atf_set descr 'Basic IPv4 NAT test'
	atf_set require.user root
}

basic_body()
{
	firewall=$1
	firewall_init $firewall
	nat_init $firewall

	epair_host_nat=$(vnet_mkepair)
	epair_client1_nat=$(vnet_mkepair)
	epair_client2_nat=$(vnet_mkepair)

	vnet_mkjail nat ${epair_host_nat}b ${epair_client1_nat}a ${epair_client2_nat}a
	vnet_mkjail client1 ${epair_client1_nat}b
	vnet_mkjail client2 ${epair_client2_nat}b

	ifconfig ${epair_host_nat}a 198.51.100.2/24 up
	jexec nat ifconfig ${epair_host_nat}b 198.51.100.1/24 up

	jexec nat ifconfig ${epair_client1_nat}a 192.0.2.1/24 up
	jexec client1 ifconfig ${epair_client1_nat}b 192.0.2.2/24 up

	jexec nat ifconfig ${epair_client2_nat}a 192.0.3.1/24 up
	jexec client2 ifconfig ${epair_client2_nat}b 192.0.3.2/24 up

	jexec nat sysctl net.inet.ip.forwarding=1

	jexec client1 route add -net 198.51.100.0/24 192.0.2.1
	jexec client2 route add -net 198.51.100.0/24 192.0.3.1

	# ping fails without NAT configuration
	atf_check -s exit:2 -o ignore jexec client1 ping -t 1 -c 1 198.51.100.2
	atf_check -s exit:2 -o ignore jexec client2 ping -t 1 -c 1 198.51.100.2

	firewall_config nat ${firewall} \
		"pf" \
			"nat pass on ${epair_host_nat}b inet from any to any -> (${epair_host_nat}b)" \
		"ipfw" \
			"ipfw -q nat 123 config if ${epair_host_nat}b" \
			"ipfw -q add 1000 nat 123 all from any to any" \
		"ipfnat" \
			"map ${epair_host_nat}b 192.0.3.0/24 -> 0/32" \
			"map ${epair_host_nat}b 192.0.2.0/24 -> 0/32" \


	# ping is successful now
	atf_check -s exit:0 -o ignore jexec client1 ping -t 1 -c 1 198.51.100.2
	atf_check -s exit:0 -o ignore jexec client2 ping -t 1 -c 1 198.51.100.2

}

basic_cleanup()
{
	firewall=$1
	firewall_cleanup $firewall
}

userspace_nat_head()
{
	atf_set descr 'Nat test for ipfw using userspace natd'
	atf_set require.user root
}
userspace_nat_body()
{
	firewall=$1
	firewall_init $firewall

	if ! kldstat -q -m ipdivert; then
		atf_skip "This test requires ipdivert module loaded"
	fi

	epair_host_nat=$(vnet_mkepair)
	epair_client1_nat=$(vnet_mkepair)
	epair_client2_nat=$(vnet_mkepair)

	vnet_mkjail nat ${epair_host_nat}b ${epair_client1_nat}a ${epair_client2_nat}a
	vnet_mkjail client1 ${epair_client1_nat}b
	vnet_mkjail client2 ${epair_client2_nat}b

	ifconfig ${epair_host_nat}a 198.51.100.2/24 up
	jexec nat ifconfig ${epair_host_nat}b 198.51.100.1/24 up

	jexec nat ifconfig ${epair_client1_nat}a 192.0.2.1/24 up
	jexec client1 ifconfig ${epair_client1_nat}b 192.0.2.2/24 up

	jexec nat ifconfig ${epair_client2_nat}a 192.0.3.1/24 up
	jexec client2 ifconfig ${epair_client2_nat}b 192.0.3.2/24 up

	jexec nat sysctl net.inet.ip.forwarding=1

	jexec client1 route add -net 198.51.100.0/24 192.0.2.1
	jexec client2 route add -net 198.51.100.0/24 192.0.3.1
	# Test the userspace NAT of ipfw
	# ping fails without NAT configuration
	atf_check -s exit:2 -o ignore jexec client1 ping -t 1 -c 1 198.51.100.2
	atf_check -s exit:2 -o ignore jexec client2 ping -t 1 -c 1 198.51.100.2

	firewall_config nat ${firewall} \
		"ipfw" \
			"natd -interface ${epair_host_nat}b" \
			"ipfw -q add divert natd all from any to any via ${epair_host_nat}b" \

	# ping is successful now
	atf_check -s exit:0 -o ignore jexec client1 ping -t 1 -c 1 198.51.100.2
	atf_check -s exit:0 -o ignore jexec client2 ping -t 1 -c 1 198.51.100.2
}

userspace_nat_cleanup()
{
	firewall=$1
	firewall_cleanup $firewall
}

common_cgn() {
	firewall=$1
	portalias=$2
	firewall_init $firewall
	nat_init $firewall

	epair_host_nat=$(vnet_mkepair)
	epair_client1_nat=$(vnet_mkepair)
	epair_client2_nat=$(vnet_mkepair)

	vnet_mkjail nat ${epair_host_nat}b ${epair_client1_nat}a ${epair_client2_nat}a
	vnet_mkjail client1 ${epair_client1_nat}b
	vnet_mkjail client2 ${epair_client2_nat}b

	ifconfig ${epair_host_nat}a 198.51.100.2/24 up
	jexec nat ifconfig ${epair_host_nat}b 198.51.100.1/24 up

	jexec nat ifconfig ${epair_client1_nat}a 100.64.0.1/24 up
	jexec client1 ifconfig ${epair_client1_nat}b 100.64.0.2/24 up

	jexec nat ifconfig ${epair_client2_nat}a 100.64.1.1/24 up
	jexec client2 ifconfig ${epair_client2_nat}b 100.64.1.2/24 up

	jexec nat sysctl net.inet.ip.forwarding=1

	jexec client1 route add -net 198.51.100.0/24 100.64.0.1
	jexec client2 route add -net 198.51.100.0/24 100.64.1.1

	# ping fails without NAT configuration
	atf_check -s exit:2 -o ignore jexec client1 ping -t 1 -c 1 198.51.100.2
	atf_check -s exit:2 -o ignore jexec client2 ping -t 1 -c 1 198.51.100.2

	if [[ $portalias ]]; then
		firewall_config nat $firewall \
			"ipfw" \
				"ipfw -q nat 123 config if ${epair_host_nat}b unreg_cgn port_alias 2000-2999" \
				"ipfw -q nat 456 config if ${epair_host_nat}b unreg_cgn port_alias 3000-3999" \
				"ipfw -q add 1000 nat 123 all from any to 198.51.100.2 2000-2999 in via ${epair_host_nat}b" \
				"ipfw -q add 2000 nat 456 all from any to 198.51.100.2 3000-3999 in via ${epair_host_nat}b" \
				"ipfw -q add 3000 nat 123 all from 100.64.0.2 to any out via ${epair_host_nat}b" \
				"ipfw -q add 4000 nat 456 all from 100.64.1.2 to any out via ${epair_host_nat}b"
	else
		firewall_config nat $firewall \
			"ipfw" \
				"ipfw -q nat 123 config if ${epair_host_nat}b unreg_cgn" \
				"ipfw -q add 1000 nat 123 all from any to any"
	fi

	# ping is successful now
	atf_check -s exit:0 -o ignore jexec client1 ping -t 1 -c 1 198.51.100.2
	atf_check -s exit:0 -o ignore jexec client2 ping -t 1 -c 1 198.51.100.2

	# if portalias, test a tcp server/client with nc
	if [[ $portalias ]]; then
		for inst in 1 2; do
			daemon nc -p 198.51.100.2 7
			atf_check -s exit:0 -o ignore jexec client$inst sh -c "echo | nc -N 198.51.100.2 7"
		done
	fi
}

cgn_head()
{
	atf_set descr 'IPv4 CGN (RFC 6598) test'
	atf_set require.user root
}

cgn_body()
{
	common_cgn $1 false
}

cgn_cleanup()
{
	firewall_cleanup ipfw
}

portalias_head()
{
	atf_set descr 'IPv4 CGN (RFC 6598) port aliasing test'
	atf_set require.user root
}

portalias_body()
{
	common_cgn $1 true
}

portalias_cleanup()
{
	firewall_cleanup ipfw
}

setup_tests \
		basic \
			pf \
			ipfw \
			ipfnat \
		userspace_nat \
			ipfw \
		cgn \
			ipfw \
		portalias \
			ipfw
