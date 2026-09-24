#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Alexander Leidinger <netchild@FreeBSD.org>
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

. $(atf_get_srcdir)/../common/utils.subr

helper=$(atf_get_srcdir)/../../common/sendfile_helper

# ipfw's layer-2 hook (net.link.ether.ipfw) runs ipfw_chk() on the raw
# Ethernet frame.  A sender that advertises IFCAP_MEXTPG hands it an unmapped
# (M_EXTPG) sendfile(2)/KTLS chain; ipfw_chk() pulls the frame up to inspect
# it.  Two preconditions decide whether this exercises anything:
#
# vnet_mkepair() clears IFCAP_MEXTPG, so it is restored on both ends.
#
# Without TCP options the mapped ether + IPv4 + TCP head is 54 bytes; a pull
# that reaches past it crosses into the unmapped payload.  rfc1323 is disabled
# below so the head stays at 54.
ipfw_l2_setup()
{
	firewall_init ipfw

	epair=$(vnet_mkepair)
	for iface in ${epair}a ${epair}b; do
		ifconfig ${iface} mextpg
		ifconfig ${iface} | grep -q MEXTPG ||
		    atf_fail "MEXTPG unavailable on ${iface}"
	done

	vnet_mkjail snd ${epair}a
	vnet_mkjail rcv ${epair}b

	jexec snd ifconfig ${epair}a 192.0.2.1/24 up
	jexec rcv ifconfig ${epair}b 192.0.2.2/24 up
	jexec snd sysctl -q net.inet.tcp.rfc1323=0
	jexec rcv sysctl -q net.inet.tcp.rfc1323=0

	dd if=/dev/random of=payload bs=1m count=8 status=none
	payload=$(pwd)/payload
	size=$(stat -f %z ${payload})
}

# Load a ruleset in the sender vnet and hook ipfw into its layer 2.  The
# ruleset's first rule counts every layer-2 frame ipfw sees, which is what
# distinguishes "filtered and survived" from "never filtered".
ipfw_l2_enable()
{
	firewall_config snd ipfw ipfw "$@"
	atf_check -s exit:0 -o ignore \
	    jexec snd sysctl net.link.ether.ipfw=1
}

# Start the receiver, writing to $1, and the sender in the background.
ipfw_l2_sendfile_start()
{
	jexec rcv nc -l 5555 > $1 &
	receiver=$!
	sleep 1
	timeout 60 jexec snd ${helper} -c 192.0.2.2 -p 5555 ${payload} 0 ${size} 0 &
	sender=$!
}

# Wait for the sender to finish, then for the receiver to drain.
ipfw_l2_sendfile_wait()
{
	wait ${sender}
	status=$?

	i=0
	while [ ${i} -lt 10 ] && kill -0 ${receiver} 2>/dev/null; do
		sleep 1
		i=$((i + 1))
	done
	kill ${receiver} 2>/dev/null
	wait ${receiver} 2>/dev/null
	return ${status}
}

# Send the payload and leave what arrived in $1.
ipfw_l2_sendfile()
{
	ipfw_l2_sendfile_start $1
	ipfw_l2_sendfile_wait
}

# Wait up to 10 s for rule $1 to have counted a packet.
ipfw_l2_wait_count()
{
	i=0
	while [ ${i} -lt 100 ]; do
		count=$(jexec snd ipfw show $1 2>/dev/null | awk 'NR == 1 {print $2}')
		[ "${count:-0}" -gt 0 ] && return 0
		sleep 0.1
		i=$((i + 1))
	done
	return 1
}

# rule 100 counts layer-2 frames; without the hook it stays zero.
ipfw_l2_assert_filtered()
{
	ipfw_l2_wait_count 100 ||
	    atf_fail "ipfw saw no frames on layer 2"
}

atf_test_case "unmapped" "cleanup"
unmapped_head()
{
	atf_set descr 'Unmapped mbufs survive ipfw layer-2 filtering'
	atf_set require.user root
}

unmapped_body()
{
	ipfw_l2_setup
	ipfw_l2_enable \
	    "ipfw add 100 count ip from any to any layer2" \
	    "ipfw add 200 allow ip from any to any"

	ipfw_l2_sendfile received ||
	    atf_fail "sendfile through the ipfw layer-2 hook failed"
	atf_check -s exit:0 cmp ${payload} received
	ipfw_l2_assert_filtered
}

unmapped_cleanup()
{
	firewall_cleanup ipfw
}

# Let the handshake through and deny the data segments, so the deny path
# runs on the unmapped chain; rule 110 counts the first drop at once.
atf_test_case "unmapped_block" "cleanup"
unmapped_block_head()
{
	atf_set descr 'ipfw layer 2 drops unmapped data segments when told to'
	atf_set require.user root
}

unmapped_block_body()
{
	ipfw_l2_setup
	ipfw_l2_enable \
	    "ipfw add 100 count ip from any to any layer2" \
	    "ipfw add 110 deny tcp from any to any dst-port 5555 tcpdatalen 1000-65535 layer2" \
	    "ipfw add 200 allow ip from any to any"

	ipfw_l2_sendfile_start blocked
	ipfw_l2_wait_count 110 ||
	    atf_fail "the deny rule matched no data segment"
	kill ${sender} ${receiver} 2>/dev/null
	wait ${sender} ${receiver} 2>/dev/null
	[ "$(stat -f %z blocked)" -lt ${size} ] ||
	    atf_fail "the payload arrived although ipfw drops its data segments"
	ipfw_l2_assert_filtered
}

unmapped_block_cleanup()
{
	firewall_cleanup ipfw
}

atf_init_test_cases()
{
	atf_add_test_case "unmapped"
	atf_add_test_case "unmapped_block"
}
