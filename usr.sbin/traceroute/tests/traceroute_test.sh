# SPDX-License-Identifier: ISC
#
# Copyright (c) 2025 Lexi Winter
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# We are missing tests for the following flags:
#
# -a (turn on ASN lookups)
# -A (specify ASN lookup server)
# -d (enable SO_DEBUG)
# -D (print the diff between our packet and the quote in the ICMP error)
# -E (detect ECN bleaching)
# -n (or rather, we enable -n by default and don't test without it)
# -S (print per-hop packet loss)
# -v (verbose output)
# -w (how long to wait for an error response)
# -x (toggle IP checksums)
# -z (how long to wait between each probe)

. $(atf_get_srcdir)/../../sys/common/vnet.subr

# These are the default flags we use for most test cases:
# - only send a single probe packet to reduce the risk of kernel ICMP
#   rate-limiting breaking the test.
# - only trace up to 5 hops and only wait 1 second for a response so the test
#   fails quicker if something goes wrong.
# - disable DNS resolution as we don't usually care about this.
TR_FLAGS="-w 1 -q 1 -m 5 -n"

# The prefix our test networks are in.
TEST_PREFIX="192.0.2.0/24"

# The IPv4 addresses of the first link net between trsrc and trrtr.
LINK_TRSRC_TRSRC="192.0.2.5"
LINK_TRSRC_TRRTR="192.0.2.6"
LINK_TRSRC_PREFIXLEN="30"

# The IPv4 addresses of the second link net between trsrc and trrtr.
LINK_TRSRC2_TRSRC="192.0.2.13"
LINK_TRSRC2_TRRTR="192.0.2.14"
LINK_TRSRC2_PREFIXLEN="30"

# The IPv4 addresses of the link net between trdst and trrtr.
LINK_TRDST_TRDST="192.0.2.9"
LINK_TRDST_TRRTR="192.0.2.10"
LINK_TRDST_PREFIXLEN="30"

# This is an address inside $TEST_PREFIX which is not routed anywhere.
UNREACHABLE_ADDR="192.0.2.255"

setup_network()
{
	# Create 3 jails: one to be the source host, one to be the router,
	# and one to be the destination host.

	vnet_init

	# src jail
	epsrc=$(vnet_mkepair)
	epsrc2=$(vnet_mkepair)
	vnet_mkjail trsrc ${epsrc}a ${epsrc2}a

	# dst jail
	epdst=$(vnet_mkepair)
	vnet_mkjail trdst ${epdst}a

	# router jail
	vnet_mkjail trrtr ${epsrc}b ${epsrc2}b ${epdst}b

	# Configure IPv4 addresses and routes on each jail.

	# trsrc
	jexec trsrc ifconfig ${epsrc}a inet \
	    ${LINK_TRSRC_TRSRC}/${LINK_TRSRC_PREFIXLEN}
	jexec trrtr ifconfig ${epsrc}b inet \
	    ${LINK_TRSRC_TRRTR}/${LINK_TRSRC_PREFIXLEN}
	jexec trsrc route add -inet ${TEST_PREFIX} ${LINK_TRSRC_TRRTR}

	# trsrc2
	jexec trsrc ifconfig ${epsrc2}a inet \
	    ${LINK_TRSRC2_TRSRC}/${LINK_TRSRC2_PREFIXLEN}
	jexec trrtr ifconfig ${epsrc2}b inet \
	    ${LINK_TRSRC2_TRRTR}/${LINK_TRSRC2_PREFIXLEN}

	# trdst
	jexec trdst ifconfig ${epdst}a inet \
	    ${LINK_TRDST_TRDST}/${LINK_TRDST_PREFIXLEN}
	jexec trrtr ifconfig ${epdst}b inet \
	    ${LINK_TRDST_TRRTR}/${LINK_TRDST_PREFIXLEN}
	jexec trdst route add -inet ${TEST_PREFIX} ${LINK_TRDST_TRRTR}

	# The router jail (only) needs IP forwarding enabled.
	jexec trrtr sysctl net.inet.ip.forwarding=1
}

##
#
# start_tcpdump, stop_tcpdump: used to capture packets during the test so we
# can verify we actually sent the expected packets.

start_tcpdump()
{
	# Run tcpdump on trrtr, either on the given interface or on
	# ${epsrc}b, which is trsrc's default route interface.

	interface="$1"
	if [ -z "$interface" ]; then
		interface="${epsrc}b"
	fi

	rm -f "${PWD}/traceroute.pcap"

	jexec trrtr daemon -p "${PWD}/tcpdump.pid" \
	    tcpdump --immediate-mode -w "${PWD}/traceroute.pcap" -nv \
	    -i $interface

	# Give tcpdump time to start
	sleep 1
}

stop_tcpdump()
{
	# Sleep to give tcpdump a chance to finish flushing
	jexec trrtr kill -USR2 $(cat "${PWD}/tcpdump.pid")
	sleep 1
	jexec trrtr kill $(cat "${PWD}/tcpdump.pid")

	# Format the packet capture and merge continued lines (starting with
	# whitespace) into a single line; this makes it easier to match in
	# atf_check.  Append a blank line since the N command exits on EOF.
	(tcpdump -nv -r "${PWD}/traceroute.pcap"; echo) | \
	    sed -E -e :a -e N -e 's/\n +/ /' -e ta -e P -e D \
	    > tcpdump.output
}

##
# test: ipv4_basic
#

atf_test_case "ipv4_basic" "cleanup"
ipv4_basic_head()
{
	atf_set descr "Basic IPv4 traceroute across a router"
	atf_set require.user root
}

ipv4_basic_body()
{
	setup_network

	# Use a more detailed set of regexp here than the rest of the tests to
	# make sure the basic output format is correct.
	atf_check -s exit:0					\
	    -e match:"^traceroute to ${LINK_TRDST_TRDST} \\(${LINK_TRDST_TRDST}\\), 5 hops max, 40 byte packets$" \
	    -o match:"^ 1  ${LINK_TRSRC_TRRTR}  [0-9.]+ ms$"	\
	    -o match:"^ 2  ${LINK_TRDST_TRDST}  [0-9.]+ ms$"	\
	    -o not-match:"^ 3"					\
	    jexec trsrc traceroute $TR_FLAGS ${LINK_TRDST_TRDST}
}

ipv4_basic_cleanup()
{
	vnet_cleanup
}

##
# test: ipv4_icmp
#

atf_test_case "ipv4_icmp" "cleanup"
ipv4_icmp_head()
{
	atf_set descr "Basic IPv4 ICMP traceroute across a router"
	atf_set require.user root
}

ipv4_icmp_body()
{
	setup_network

	# -I and -Picmp should mean the same thing, so test both.

	for icmp_flag in -Picmp -I; do
		start_tcpdump

		atf_check -s exit:0					\
		    -e match:"^traceroute to ${LINK_TRDST_TRDST}"	\
		    -o match:"^ 1  ${LINK_TRSRC_TRRTR}"			\
		    -o match:"^ 2  ${LINK_TRDST_TRDST}"			\
		    -o not-match:"^ 3"					\
		    jexec trsrc traceroute $TR_FLAGS $icmp_flag		\
		    ${LINK_TRDST_TRDST}

		stop_tcpdump

		atf_check -s exit:0 -e ignore 				\
		    -o match:"IP \\(tos 0x0, ttl 1, .*, proto ICMP.*\\).* ${LINK_TRSRC_TRSRC} > ${LINK_TRDST_TRDST}: ICMP echo request" \
		    -o match:"IP \\(tos 0x0, ttl 2, .*, proto ICMP.*\\).* ${LINK_TRSRC_TRSRC} > ${LINK_TRDST_TRDST}: ICMP echo request" \
		    cat tcpdump.output
	done
}

ipv4_icmp_cleanup()
{
	vnet_cleanup
}

##
# test: ipv4_udp
#

atf_test_case "ipv4_udp" "cleanup"
ipv4_udp_head()
{
	atf_set descr "IPv4 UDP traceroute"
	atf_set require.user root
}

ipv4_udp_body()
{
	setup_network

	start_tcpdump

	atf_check -s exit:0					\
	    -e match:"^traceroute to ${LINK_TRDST_TRDST}"	\
	    -o match:"^ 1  ${LINK_TRSRC_TRRTR}"			\
	    -o match:"^ 2  ${LINK_TRDST_TRDST}"			\
	    -o not-match:"^ 3"					\
	    jexec trsrc traceroute $TR_FLAGS -Pudp ${LINK_TRDST_TRDST}

	stop_tcpdump

	atf_check -s exit:0 -e ignore 				\
	    -o match:"IP \\(tos 0x0, ttl 1, .*, proto UDP .*\\).* ${LINK_TRSRC_TRSRC}.[0-9]+ > ${LINK_TRDST_TRDST}.33435: UDP" \
	    -o match:"IP \\(tos 0x0, ttl 2, .*, proto UDP .*\\).* ${LINK_TRSRC_TRSRC}.[0-9]+ > ${LINK_TRDST_TRDST}.33436: UDP" \
	    cat tcpdump.output

	# Test with -e, the destination port should not increment.

	start_tcpdump

	atf_check -s exit:0					\
	    -e match:"^traceroute to ${LINK_TRDST_TRDST}"	\
	    -o match:"^ 1  ${LINK_TRSRC_TRRTR}"			\
	    -o match:"^ 2  ${LINK_TRDST_TRDST}"			\
	    -o not-match:"^ 3"					\
	    jexec trsrc traceroute $TR_FLAGS -Pudp -e -p 40000 ${LINK_TRDST_TRDST}

	stop_tcpdump

	atf_check -s exit:0 -e ignore 				\
	    -o match:"IP \\(tos 0x0, ttl 1, .*, proto UDP .*\\).* ${LINK_TRSRC_TRSRC}.[0-9]+ > ${LINK_TRDST_TRDST}.40000: UDP" \
	    -o match:"IP \\(tos 0x0, ttl 2, .*, proto UDP .*\\).* ${LINK_TRSRC_TRSRC}.[0-9]+ > ${LINK_TRDST_TRDST}.40000: UDP" \
	    cat tcpdump.output
}

ipv4_udp_cleanup()
{
	vnet_cleanup
}

##
# test: ipv4_sctp
#

atf_test_case "ipv4_sctp" "cleanup"
ipv4_sctp_head()
{
	atf_set descr "IPv4 SCTP traceroute"
	atf_set require.user root
}

ipv4_sctp_body()
{
	setup_network

	# For the default packet size, we should sent a SHUTDOWN ACK packet.

	start_tcpdump

	atf_check -s exit:0					\
	    -e match:"^traceroute to ${LINK_TRDST_TRDST}"	\
	    -o match:"^ 1  ${LINK_TRSRC_TRRTR}"			\
	    jexec trsrc traceroute $TR_FLAGS -Psctp ${LINK_TRDST_TRDST}

	stop_tcpdump
	atf_check -s exit:0 -e ignore 				\
	    -o match:"IP \\(tos 0x0, ttl 1, .*, proto SCTP.*\\).* ${LINK_TRSRC_TRSRC}.[0-9]+ > ${LINK_TRDST_TRDST}.33435: sctp \(1\) \[SHUTDOWN ACK\]" \
	    -o match:"IP \\(tos 0x0, ttl 2, .*, proto SCTP.*\\).* ${LINK_TRSRC_TRSRC}.[0-9]+ > ${LINK_TRDST_TRDST}.33436: sctp \(1\) \[SHUTDOWN ACK\]" \
	    cat tcpdump.output

	# For a larger packet size we should send INIT packets.

	start_tcpdump

	atf_check -s exit:0					\
	    -e match:"^traceroute to ${LINK_TRDST_TRDST}"	\
	    -o match:"^ 1  ${LINK_TRSRC_TRRTR}"			\
	    jexec trsrc traceroute $TR_FLAGS -Psctp ${LINK_TRDST_TRDST} 128

	stop_tcpdump
	atf_check -s exit:0 -e ignore 				\
	    -o match:"IP \\(tos 0x0, ttl 1, .*, proto SCTP.*\\).* ${LINK_TRSRC_TRSRC}.[0-9]+ > ${LINK_TRDST_TRDST}.33435: sctp \(1\) \[INIT\]" \
	    -o match:"IP \\(tos 0x0, ttl 2, .*, proto SCTP.*\\).* ${LINK_TRSRC_TRSRC}.[0-9]+ > ${LINK_TRDST_TRDST}.33436: sctp \(1\) \[INIT\]" \
	    cat tcpdump.output

	# Test with -e, the destination port should not increment.

	start_tcpdump

	atf_check -s exit:0					\
	    -e match:"^traceroute to ${LINK_TRDST_TRDST}"	\
	    -o match:"^ 1  ${LINK_TRSRC_TRRTR}"			\
	    jexec trsrc traceroute $TR_FLAGS -Psctp -e -p 40000 ${LINK_TRDST_TRDST}

	stop_tcpdump
	atf_check -s exit:0 -e ignore 				\
	    -o match:"IP \\(tos 0x0, ttl 1, .*, proto SCTP.*\\).* ${LINK_TRSRC_TRSRC}.[0-9]+ > ${LINK_TRDST_TRDST}.40000: sctp \(1\) \[SHUTDOWN ACK\]" \
	    -o match:"IP \\(tos 0x0, ttl 2, .*, proto SCTP.*\\).* ${LINK_TRSRC_TRSRC}.[0-9]+ > ${LINK_TRDST_TRDST}.40000: sctp \(1\) \[SHUTDOWN ACK\]" \
	    cat tcpdump.output
}

ipv4_sctp_cleanup()
{
	vnet_cleanup
}

##
# test: ipv4_tcp
#

atf_test_case "ipv4_tcp" "cleanup"
ipv4_tcp_head()
{
	atf_set descr "IPv4 TCP traceroute"
	atf_set require.user root
}

ipv4_tcp_body()
{
	setup_network

	start_tcpdump

	# We expect the second hop to be a failure since traceroute doesn't
	# know how to capture the RST packet.
	atf_check -s exit:0					\
	    -e match:"^traceroute to ${LINK_TRDST_TRDST}"	\
	    -o match:"^ 1  ${LINK_TRSRC_TRRTR}"			\
	    -o match:"^ 2  \\*"					\
	    jexec trsrc traceroute $TR_FLAGS -Ptcp ${LINK_TRDST_TRDST}

	stop_tcpdump
	atf_check -s exit:0 -e ignore 				\
	    -o match:"IP \\(tos 0x0, ttl 1, .*, proto TCP.*\\).* ${LINK_TRSRC_TRSRC}.[0-9]+ > ${LINK_TRDST_TRDST}.33435: Flags \[S\]" \
	    -o match:"IP \\(tos 0x0, ttl 2, .*, proto TCP.*\\).* ${LINK_TRSRC_TRSRC}.[0-9]+ > ${LINK_TRDST_TRDST}.33436: Flags \[S\]" \
	    cat tcpdump.output

	# Test with -e, the destination port should not increment.
	start_tcpdump

	atf_check -s exit:0					\
	    -e match:"^traceroute to ${LINK_TRDST_TRDST}"	\
	    -o match:"^ 1  ${LINK_TRSRC_TRRTR}"			\
	    -o match:"^ 2  \\*"					\
	    jexec trsrc traceroute $TR_FLAGS -Ptcp -e -p 40000 ${LINK_TRDST_TRDST}

	stop_tcpdump
	atf_check -s exit:0 -e ignore 				\
	    -o match:"IP \\(tos 0x0, ttl 1, .*, proto TCP.*\\).* ${LINK_TRSRC_TRSRC}.[0-9]+ > ${LINK_TRDST_TRDST}.40000: Flags \[S\]" \
	    -o match:"IP \\(tos 0x0, ttl 2, .*, proto TCP.*\\).* ${LINK_TRSRC_TRSRC}.[0-9]+ > ${LINK_TRDST_TRDST}.40000: Flags \[S\]" \
	    cat tcpdump.output
}

ipv4_tcp_cleanup()
{
	vnet_cleanup
}

##
# test: ipv4_srcaddr
#

atf_test_case "ipv4_srcaddr" "cleanup"
ipv4_srcaddr_head()
{
	atf_set descr "IPv4 traceroute with explicit source address"
	atf_set require.user root
}

ipv4_srcaddr_body()
{
	setup_network

	start_tcpdump

	atf_check -s exit:0				\
	    -e match:"^traceroute to ${LINK_TRDST_TRDST} \\($LINK_TRDST_TRDST\\) from ${LINK_TRSRC2_TRSRC}" \
	    -o match:"^ 1  ${LINK_TRSRC2_TRRTR}"	\
	    -o match:"^ 2  ${LINK_TRDST_TRDST}"		\
	    -o not-match:"^ 3"				\
	    jexec trsrc traceroute $TR_FLAGS		\
	        -s ${LINK_TRSRC2_TRSRC} ${LINK_TRDST_TRDST}

	stop_tcpdump
	atf_check -s exit:0 -e ignore 				\
	    -o match:"IP \\(tos 0x0, ttl 1, .*, proto UDP.*\\).* ${LINK_TRSRC2_TRSRC}.[0-9]+ > ${LINK_TRDST_TRDST}.33435: UDP" \
	    -o match:"IP \\(tos 0x0, ttl 2, .*, proto UDP.*\\).* ${LINK_TRSRC2_TRSRC}.[0-9]+ > ${LINK_TRDST_TRDST}.33436: UDP" \
	    cat tcpdump.output
}

ipv4_srcaddr_cleanup()
{
	vnet_cleanup
}

##
# test: ipv4_srcinterface
#

atf_test_case "ipv4_srcinterface" "cleanup"
ipv4_srcinterface_head()
{
	atf_set descr "IPv4 traceroute with explicit source interface"
	atf_set require.user root
}

ipv4_srcinterface_body()
{
	setup_network

	start_tcpdump

	# Unlike -s, traceroute doesn't print 'from ...' when using -i.
	atf_check -s exit:0					\
	    -e match:"^traceroute to ${LINK_TRDST_TRDST}"	\
	    -o match:"^ 1  ${LINK_TRSRC2_TRRTR}"		\
	    -o match:"^ 2  ${LINK_TRDST_TRDST}"			\
	    -o not-match:"^ 3"					\
	    jexec trsrc traceroute $TR_FLAGS			\
	        -i ${epsrc2}a ${LINK_TRDST_TRDST}

	stop_tcpdump
	atf_check -s exit:0 -e ignore 				\
	    -o match:"IP \\(tos 0x0, ttl 1, .*, proto UDP.*\\).* ${LINK_TRSRC2_TRSRC}.[0-9]+ > ${LINK_TRDST_TRDST}.33435: UDP" \
	    -o match:"IP \\(tos 0x0, ttl 2, .*, proto UDP.*\\).* ${LINK_TRSRC2_TRSRC}.[0-9]+ > ${LINK_TRDST_TRDST}.33436: UDP" \
	    cat tcpdump.output
}

ipv4_srcinterface_cleanup()
{
	vnet_cleanup
}

##
# test: ipv4_maxhops
#

atf_test_case "ipv4_maxhops" "cleanup"
ipv4_maxhops_head()
{
	atf_set descr "IPv4 traceroute with -m"
	atf_set require.user root
}

ipv4_maxhops_body()
{
	setup_network

	atf_check -s exit:0					\
	    -e match:"^traceroute to ${LINK_TRDST_TRDST}"	\
	    -o match:"^ 1  ${LINK_TRSRC_TRRTR}"			\
	    -o not-match:"^ 2"					\
	    jexec trsrc traceroute -w1 -q1 -m1 ${LINK_TRDST_TRDST}
}

ipv4_maxhops_cleanup()
{
	vnet_cleanup
}

##
# test: ipv4_unreachable
#

atf_test_case "ipv4_unreachable" "cleanup"
ipv4_unreachable_head()
{
	atf_set descr "IPv4 traceroute to an unreachable destination"
	atf_set require.user root
}

ipv4_unreachable_body()
{
	setup_network

	atf_check -s exit:0					\
	    -e match:"^traceroute to ${UNREACHABLE_ADDR}"	\
	    -o match:"^ 1  ${LINK_TRSRC_TRRTR}"			\
	    -o match:"^ 2  ${LINK_TRSRC_TRRTR}  [0-9.]+ ms !H"	\
	    -o not-match:"^ 3"					\
	    jexec trsrc traceroute $TR_FLAGS $UNREACHABLE_ADDR
}

ipv4_unreachable_cleanup()
{
	vnet_cleanup
}

##
# test: ipv4_hugepacket
#

atf_test_case "ipv4_hugepacket" "cleanup"
ipv4_hugepacket_head()
{
	atf_set descr "IPv4 traceroute with a huge packet"
	atf_set require.user root
}

ipv4_hugepacket_body()
{
	setup_network

	# We expect this to fail since we specified -F (don't fragment) and the
	# 2000-byte packet is too large to fit through our tiny epair.  Make
	# sure traceroute reports the error.
	atf_check -s exit:0					\
	    -e match:"^traceroute to ${LINK_TRDST_TRDST} \\(${LINK_TRDST_TRDST}\\), 5 hops max, 2000 byte packets$" \
	    -o match:"^ 1 traceroute: wrote ${LINK_TRDST_TRDST} 2000 chars, ret=-1" \
	    -e match:"^traceroute: sendto: Message too long"	\
	    jexec trsrc traceroute -F $TR_FLAGS ${LINK_TRDST_TRDST} 2000
}

ipv4_hugepacket_cleanup()
{
	vnet_cleanup
}

##
# test: ipv4_firsthop
#

atf_test_case "ipv4_firsthop" "cleanup"
ipv4_firsthop_head()
{
	atf_set descr "IPv4 traceroute with one hop skipped"
	atf_set require.user root
}

ipv4_firsthop_body()
{
	setup_network

	# -f 2 means we skip the first hop.  For backward compatibility, -M is
	# the same as -f, so test that too.

	for flag in -f2 -M2; do
		start_tcpdump

		atf_check -s exit:0					\
		    -e match:"^traceroute to ${LINK_TRDST_TRDST}"	\
		    -o not-match:"^ 1"					\
		    -o match:"^ 2  ${LINK_TRDST_TRDST}"			\
		    -o not-match:"^ 3"					\
		    jexec trsrc traceroute $flag $TR_FLAGS ${LINK_TRDST_TRDST}

		stop_tcpdump
		atf_check -s exit:0 -e ignore 				\
		    -o not-match:"^..:..:..\....... IP \\(tos 0x0, ttl 1, .*, proto UDP.*\\)" \
		    -o match:"IP \\(tos 0x0, ttl 2, .*, proto UDP.*\\).* ${LINK_TRSRC_TRSRC}.[0-9]+ > ${LINK_TRDST_TRDST}.33435: UDP" \
		    cat tcpdump.output
	done
}

ipv4_firsthop_cleanup()
{
	vnet_cleanup
}

##
# test: ipv4_nprobes
#

atf_test_case "ipv4_nprobes" "cleanup"
ipv4_nprobes_head()
{
	atf_set descr "IPv4 traceroute with varying number of probes"
	atf_set require.user root
}

ipv4_nprobes_body()
{
	setup_network

	# By default we should send 3 probes.
	atf_check -s exit:0 -e ignore				\
	    -o match:"^ 1  ${LINK_TRSRC_TRRTR} \(${LINK_TRSRC_TRRTR}\)(  [0-9.]+ ms){3}$" \
	    jexec trsrc traceroute -w1 -m1 ${LINK_TRDST_TRDST}

	# Also test 1 and 2 (below the default) and 5 (above the default)
	for nprobes in 1 2 5; do
		atf_check -s exit:0 -e ignore				\
		    -o match:"^ 1  ${LINK_TRSRC_TRRTR} \(${LINK_TRSRC_TRRTR}\)(  [0-9.]+ ms){$nprobes}$" \
		    jexec trsrc traceroute -q$nprobes -w1 -m1 ${LINK_TRDST_TRDST}
	    done
}

ipv4_nprobes_cleanup()
{
	vnet_cleanup
}

##
# test: ipv4_baseport
#

atf_test_case "ipv4_baseport" "cleanup"
ipv4_baseport_head()
{
	atf_set descr "IPv4 traceroute with non-default base port"
	atf_set require.user root
}

ipv4_baseport_body()
{
	setup_network

	start_tcpdump

	atf_check -s exit:0					\
	    -e match:"^traceroute to ${LINK_TRDST_TRDST}"	\
	    -o match:"^ 1  ${LINK_TRSRC_TRRTR}"			\
	    -o match:"^ 2  ${LINK_TRDST_TRDST}"			\
	    -o not-match:"^ 3"					\
	    jexec trsrc traceroute $TR_FLAGS -p 40000		\
	    ${LINK_TRDST_TRDST}

	stop_tcpdump

	atf_check -s exit:0 -e ignore 				\
	    -o match:"IP \\(tos 0x0, ttl 1, .*, proto UDP.*\\).* ${LINK_TRSRC_TRSRC}.[0-9]+ > ${LINK_TRDST_TRDST}.40001: UDP" \
	    -o match:"IP \\(tos 0x0, ttl 2, .*, proto UDP.*\\).* ${LINK_TRSRC_TRSRC}.[0-9]+ > ${LINK_TRDST_TRDST}.40002: UDP" \
	    cat tcpdump.output
}

ipv4_baseport_cleanup()
{
	vnet_cleanup
}

##
# test: ipv4_gre
#

atf_test_case "ipv4_gre" "cleanup"
ipv4_gre_head()
{
	atf_set descr "IPv4 GRE traceroute"
	atf_set require.user root
}

ipv4_gre_body()
{
	setup_network

	start_tcpdump

	# We expect the second hop to be a failure since the remote host will
	# ignore the GRE packet.
	atf_check -s exit:0					\
	    -e match:"^traceroute to ${LINK_TRDST_TRDST}"	\
	    -o match:"^ 1  ${LINK_TRSRC_TRRTR}"			\
	    -o match:"^ 2  \\*"					\
	    jexec trsrc traceroute $TR_FLAGS -Pgre ${LINK_TRDST_TRDST}

	stop_tcpdump
	atf_check -s exit:0 -e ignore 				\
	    -o match:"IP \\(tos 0x0, ttl 1, .*, proto GRE .*\\).* ${LINK_TRSRC_TRSRC} > ${LINK_TRDST_TRDST}: GREv1" \
	    -o match:"IP \\(tos 0x0, ttl 2, .*, proto GRE .*\\).* ${LINK_TRSRC_TRSRC} > ${LINK_TRDST_TRDST}: GREv1" \
	    cat tcpdump.output
}

ipv4_gre_cleanup()
{
	vnet_cleanup
}

##
# test: ipv4_udplite
#

atf_test_case "ipv4_udplite" "cleanup"
ipv4_udplite_head()
{
	atf_set descr "IPv4 UDP-Lite traceroute"
	atf_set require.user root
}

ipv4_udplite_body()
{
	setup_network

	start_tcpdump

	atf_check -s exit:0					\
	    -e match:"^traceroute to ${LINK_TRDST_TRDST}"	\
	    -o match:"^ 1  ${LINK_TRSRC_TRRTR}"			\
	    -o match:"^ 2  ${LINK_TRDST_TRDST}"			\
	    -o not-match:"^ 3"					\
	    jexec trsrc traceroute $TR_FLAGS -Pudplite ${LINK_TRDST_TRDST}

	stop_tcpdump
	atf_check -s exit:0 -e ignore 				\
	    -o match:"IP \\(tos 0x0, ttl 1, .*, proto unknown \(136\), .*\\).* ${LINK_TRSRC_TRSRC} > ${LINK_TRDST_TRDST}:  ip-proto-136" \
	    -o match:"IP \\(tos 0x0, ttl 2, .*, proto unknown \(136\), .*\\).* ${LINK_TRSRC_TRSRC} > ${LINK_TRDST_TRDST}:  ip-proto-136" \
	    cat tcpdump.output
}

ipv4_udplite_cleanup()
{
	vnet_cleanup
}

##
# test: ipv4_iptos
#

atf_test_case "ipv4_iptos" "cleanup"
ipv4_iptos_head()
{
	atf_set descr "IPv4 traceroute with explicit ToS"
	atf_set require.user root
}

ipv4_iptos_body()
{
	setup_network

	start_tcpdump

	atf_check -s exit:0					\
	    -e match:"^traceroute to ${LINK_TRDST_TRDST}"	\
	    -o match:"^ 1  ${LINK_TRSRC_TRRTR}"			\
	    -o match:"^ 2  ${LINK_TRDST_TRDST}"			\
	    -o not-match:"^ 3"					\
	    jexec trsrc traceroute $TR_FLAGS -t 4 ${LINK_TRDST_TRDST}

	stop_tcpdump
	atf_check -s exit:0 -e ignore 				\
	    -o match:"IP \\(tos 0x4, ttl 1, .*, proto UDP .*\\).* ${LINK_TRSRC_TRSRC}.[0-9]+ > ${LINK_TRDST_TRDST}.33435: UDP" \
	    -o match:"IP \\(tos 0x4, ttl 2, .*, proto UDP .*\\).* ${LINK_TRSRC_TRSRC}.[0-9]+ > ${LINK_TRDST_TRDST}.33436: UDP" \
	    cat tcpdump.output
}

ipv4_iptos_cleanup()
{
	vnet_cleanup
}

##
# test: ipv4_srcroute
#

atf_test_case "ipv4_srcroute" "cleanup"
ipv4_srcroute_head()
{
	atf_set descr "IPv4 traceroute with explicit source routing"
	atf_set require.user root
}

ipv4_srcroute_body()
{
	setup_network
	jexec trsrc sysctl net.inet.ip.sourceroute=1
	jexec trsrc sysctl net.inet.ip.accept_sourceroute=1
	jexec trrtr sysctl net.inet.ip.sourceroute=1

	start_tcpdump

	# As we don't enable source routing on trdst, we should get an ICMP
	# source routing failed error (!S).
	atf_check -s exit:0					\
	    -e match:"^traceroute to ${LINK_TRDST_TRDST}"	\
	    -o match:"^ 1  ${LINK_TRSRC_TRRTR}"			\
	    -o match:"^ 2  ${LINK_TRDST_TRDST}  [0-9.]+ ms !S"	\
	    -o not-match:"^ 3"					\
	    jexec trsrc traceroute $TR_FLAGS			\
	        -g ${LINK_TRSRC_TRRTR} ${LINK_TRDST_TRDST}

	stop_tcpdump
	atf_check -s exit:0 -e ignore 				\
	    -o match:"IP \\(tos 0x0, ttl 1, .*, proto UDP .*, options \\(NOP,LSRR ${LINK_TRDST_TRDST}\\)\\).* ${LINK_TRSRC_TRSRC}.[0-9]+ > ${LINK_TRSRC_TRRTR}.33435: UDP" \
	    -o match:"IP \\(tos 0x0, ttl 2, .*, proto UDP .*, options \\(NOP,LSRR ${LINK_TRDST_TRDST}\\)\\).* ${LINK_TRSRC_TRSRC}.[0-9]+ > ${LINK_TRSRC_TRRTR}.33436: UDP" \
	    cat tcpdump.output
}

ipv4_srcroute_cleanup()
{
	vnet_cleanup
}

##
# test: ipv4_dontroute
#

atf_test_case "ipv4_dontroute" "cleanup"
ipv4_dontroute_head()
{
	atf_set descr "IPv4 traceroute with -r"
	atf_set require.user root
}

ipv4_dontroute_body()
{
	setup_network

	# This one should work as trrtr is directly connected.

	atf_check -s exit:0					\
	    -e match:"^traceroute to ${LINK_TRSRC_TRRTR}"	\
	    -o match:"^ 1  ${LINK_TRSRC_TRRTR}  [0-9.]+ ms$"	\
	    -o not-match:"^ 2"					\
	    jexec trsrc traceroute -r $TR_FLAGS ${LINK_TRSRC_TRRTR}

	# This one should fail.

	atf_check -s exit:0					\
	    -e match:"^traceroute to ${LINK_TRDST_TRDST}"	\
	    -o match:"^ 1 traceroute: wrote ${LINK_TRDST_TRDST} 40 chars, ret=-1" \
	    jexec trsrc traceroute -r $TR_FLAGS ${LINK_TRDST_TRDST}
}

ipv4_dontroute_cleanup()
{
	vnet_cleanup
}

##
# test case declarations

atf_init_test_cases()
{
	atf_add_test_case ipv4_basic
	atf_add_test_case ipv4_udp
	atf_add_test_case ipv4_icmp
	atf_add_test_case ipv4_tcp
	atf_add_test_case ipv4_sctp
	atf_add_test_case ipv4_gre
	atf_add_test_case ipv4_udplite
	atf_add_test_case ipv4_srcaddr
	atf_add_test_case ipv4_srcinterface
	atf_add_test_case ipv4_maxhops
	atf_add_test_case ipv4_unreachable
	atf_add_test_case ipv4_hugepacket
	atf_add_test_case ipv4_firsthop
	atf_add_test_case ipv4_nprobes
	atf_add_test_case ipv4_baseport
	atf_add_test_case ipv4_iptos
	atf_add_test_case ipv4_srcroute
	atf_add_test_case ipv4_dontroute
}
