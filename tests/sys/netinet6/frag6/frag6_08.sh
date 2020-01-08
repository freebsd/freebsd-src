# $FreeBSD$
#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Netflix, Inc.
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

. $(atf_get_srcdir)/frag6.subr

frag6_08_check_stats() {

	local jname ifname
	jname=$1
	ifname=$2

	case "${jname}" in
	"")	echo "ERROR: jname is empty"; return ;;
	esac
	case "${ifname}" in
	"")	echo "ERROR: ifname is empty"; return ;;
	esac

	# Defaults are: IPV6_FRAGTTL  120 slowtimo ticks.
	# pfslowtimo() is run at hz/2.  So this takes 60s.
	# This is awefully long for a test case.
	# The Python script has to wait for this already to get the ICMPv6
	# hence we do not sleep here anymore.

	nf=`jexec ${jname} sysctl -n net.inet6.ip6.frag6_nfragpackets`
	case ${nf} in
	0)	break ;;
	*)	atf_fail "VNET frag6_nfragpackets not 0 but: ${nf}" ;;
	esac
	nf=`sysctl -n net.inet6.ip6.frag6_nfrags`
	case ${nf} in
	0)	break ;;
	*)	atf_fail "Global frag6_nfrags not 0 but: ${nf}" ;;
	esac

	#
	# Check selection of global UDP stats.
	#
	cat <<EOF > ${HOME}/filter-${jname}.txt
    <received-datagrams>0</received-datagrams>
    <dropped-incomplete-headers>0</dropped-incomplete-headers>
    <dropped-bad-data-length>0</dropped-bad-data-length>
    <dropped-bad-checksum>0</dropped-bad-checksum>
    <dropped-no-checksum>0</dropped-no-checksum>
    <dropped-no-socket>0</dropped-no-socket>
    <dropped-broadcast-multicast>0</dropped-broadcast-multicast>
    <dropped-full-socket-buffer>0</dropped-full-socket-buffer>
    <not-for-hashed-pcb>0</not-for-hashed-pcb>
EOF
	count=`jexec ${jname} netstat -s -p udp --libxo xml,pretty | grep -E -x -c -f ${HOME}/filter-${jname}.txt`
	rm -f ${HOME}/filter-${jname}.txt
	case ${count} in
	9)	;;
	*)	jexec ${jname} netstat -s -p udp --libxo xml,pretty
		atf_fail "Global UDP statistics do not match: ${count} != 9" ;;
	esac


	#
	# Check selection of global IPv6 stats.
	# XXX-BZ Only ICMPv6 errors and no proper stats!
	#
	cat <<EOF > ${HOME}/filter-${jname}.txt
    <dropped-below-minimum-size>0</dropped-below-minimum-size>
    <dropped-short-packets>0</dropped-short-packets>
    <dropped-bad-options>0</dropped-bad-options>
    <dropped-bad-version>0</dropped-bad-version>
    <received-fragments>2</received-fragments>
    <dropped-fragment>0</dropped-fragment>
    <dropped-fragment-after-timeout>1</dropped-fragment-after-timeout>
    <dropped-fragments-overflow>0</dropped-fragments-overflow>
    <atomic-fragments>0</atomic-fragments>
    <reassembled-packets>0</reassembled-packets>
    <forwarded-packets>0</forwarded-packets>
    <packets-not-forwardable>0</packets-not-forwardable>
    <sent-redirects>0</sent-redirects>
    <send-packets-fabricated-header>0</send-packets-fabricated-header>
    <discard-no-mbufs>0</discard-no-mbufs>
    <discard-no-route>0</discard-no-route>
    <sent-fragments>0</sent-fragments>
    <fragments-created>0</fragments-created>
    <discard-cannot-fragment>0</discard-cannot-fragment>
    <discard-scope-violations>0</discard-scope-violations>
EOF
	count=`jexec ${jname} netstat -s -p ip6 --libxo xml,pretty | grep -E -x -c -f ${HOME}/filter-${jname}.txt`
	rm -f ${HOME}/filter-${jname}.txt
	case ${count} in
	20)	;;
	*)	jexec ${jname} netstat -s -p ip6 --libxo xml,pretty
		atf_fail "Global IPv6 statistics do not match: ${count} != 20" ;;
	esac

	#
	# Check selection of global ICMPv6 stats.
	# XXX-TODO check output histogram (just too hard to parse [no multi-line-grep])
	#
	cat <<EOF > ${HOME}/filter-${jname}.txt
    <icmp6-calls>2</icmp6-calls>
      <no-route>0</no-route>
      <admin-prohibited>0</admin-prohibited>
      <beyond-scope>0</beyond-scope>
      <address-unreachable>0</address-unreachable>
      <port-unreachable>0</port-unreachable>
      <packet-too-big>0</packet-too-big>
      <time-exceed-transmit>0</time-exceed-transmit>
      <time-exceed-reassembly>1</time-exceed-reassembly>
      <bad-header>1</bad-header>
      <bad-next-header>0</bad-next-header>
      <bad-option>0</bad-option>
      <redirects>0</redirects>
      <unknown>0</unknown>
      <reflect>0</reflect>
      <too-many-nd-options>0</too-many-nd-options>
      <bad-nd-options>0</bad-nd-options>
      <bad-neighbor-solicitation>0</bad-neighbor-solicitation>
      <bad-neighbor-advertisement>0</bad-neighbor-advertisement>
      <bad-router-solicitation>0</bad-router-solicitation>
      <bad-router-advertisement>0</bad-router-advertisement>
      <bad-redirect>0</bad-redirect>
EOF
	count=`jexec ${jname} netstat -s -p icmp6 --libxo xml,pretty | grep -E -x -c -f ${HOME}/filter-${jname}.txt`
	rm -f ${HOME}/filter-${jname}.txt
	case ${count} in
	22)	;;
	*)	jexec ${jname} netstat -s -p icmp6 --libxo xml,pretty
		atf_fail "Global ICMPv6 statistics do not match: ${count} != 22" ;;
	esac

	#
	# Check selection of interface IPv6 stats.
	#
	cat <<EOF > ${HOME}/filter-${jname}.txt
    <dropped-invalid-header>0</dropped-invalid-header>
    <dropped-mtu-exceeded>0</dropped-mtu-exceeded>
    <dropped-no-route>0</dropped-no-route>
    <dropped-invalid-destination>0</dropped-invalid-destination>
    <dropped-unknown-protocol>0</dropped-unknown-protocol>
    <dropped-truncated>0</dropped-truncated>
    <sent-forwarded>0</sent-forwarded>
    <discard-packets>0</discard-packets>
    <discard-fragments>0</discard-fragments>
    <fragments-failed>0</fragments-failed>
    <fragments-created>0</fragments-created>
    <reassembly-required>2</reassembly-required>
    <reassembled-packets>0</reassembled-packets>
    <reassembly-failed>0</reassembly-failed>
EOF
	count=`jexec ${jname} netstat -s -p ip6 -I ${ifname} --libxo xml,pretty | grep -E -x -c -f ${HOME}/filter-${jname}.txt`
	rm -f ${HOME}/filter-${jname}.txt
	case ${count} in
	14)	;;
	*)	jexec ${jname} netstat -s -p ip6 -I ${ifname} --libxo xml,pretty
		atf_fail "Interface IPv6 statistics do not match: ${count} != 14" ;;
	esac

	#
	# Check selection of interface ICMPv6 stats.
	#
	cat <<EOF > ${HOME}/filter-${jname}.txt
    <received-errors>0</received-errors>
    <received-destination-unreachable>0</received-destination-unreachable>
    <received-admin-prohibited>0</received-admin-prohibited>
    <received-time-exceeded>0</received-time-exceeded>
    <received-bad-parameter>0</received-bad-parameter>
    <received-packet-too-big>0</received-packet-too-big>
    <received-echo-requests>0</received-echo-requests>
    <received-echo-replies>0</received-echo-replies>
    <received-router-solicitation>0</received-router-solicitation>
    <received-router-advertisement>0</received-router-advertisement>
    <sent-errors>2</sent-errors>
    <sent-destination-unreachable>0</sent-destination-unreachable>
    <sent-admin-prohibited>0</sent-admin-prohibited>
    <sent-time-exceeded>1</sent-time-exceeded>
    <sent-bad-parameter>1</sent-bad-parameter>
    <sent-packet-too-big>0</sent-packet-too-big>
    <sent-echo-requests>0</sent-echo-requests>
    <sent-echo-replies>0</sent-echo-replies>
    <sent-router-solicitation>0</sent-router-solicitation>
    <sent-router-advertisement>0</sent-router-advertisement>
    <sent-redirects>0</sent-redirects>
EOF
	count=`jexec ${jname} netstat -s -p icmp6 -I ${ifname} --libxo xml,pretty | grep -E -x -c -f ${HOME}/filter-${jname}.txt`
	rm -f ${HOME}/filter-${jname}.txt
	case ${count} in
	21)	;;
	*)	jexec ${jname} netstat -s -p icmp6 -I ${ifname} --libxo xml,pretty
		atf_fail "Interface ICMPv6 statistics do not match: ${count} != 21" ;;
	esac
}

atf_test_case "frag6_08" "cleanup"
frag6_08_head() {
	frag6_head 8
}

frag6_08_body() {
	frag6_body 8 frag6_08_check_stats
}

frag6_08_cleanup() {
	frag6_cleanup 8
}

atf_init_test_cases()
{
	atf_add_test_case "frag6_08"
}
