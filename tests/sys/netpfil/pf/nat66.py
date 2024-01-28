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

import ctypes
import ipaddress
import pytest
import re
import socket
import threading
import time
from atf_python.sys.net.tools import ToolsHelper
from atf_python.sys.net.vnet import VnetTestTemplate

class DelayedSend(threading.Thread):
    def __init__(self, packet):
        threading.Thread.__init__(self)
        self._packet = packet

        self.start()

    def run(self):
        import scapy.all as sp
        time.sleep(1)
        sp.send(self._packet)

class TestNAT66(VnetTestTemplate):
    REQUIRED_MODUES = [ "pf" ]
    TOPOLOGY = {
        "vnet1": {"ifaces": ["if1"]},
        "vnet2": {"ifaces": ["if1", "if2"]},
        "vnet3": {"ifaces": ["if2"]},
        "if1": {"prefixes6": [("2001:db8::2/64", "2001:db8::1/64")]},
        "if2": {"prefixes6": [("2001:db8:1::1/64", "2001:db8:1::2/64")]},
    }

    def vnet2_handler(self, vnet):
        ifname = vnet.iface_alias_map["if1"].name
        ToolsHelper.print_output("/sbin/ifconfig %s mtu 9000" % ifname)

        ToolsHelper.print_output("/sbin/pfctl -e")
        ToolsHelper.pf_rules([
            "set reassemble yes",
            "binat inet6 from 2001:db8::/64 to 2001:db8:1::/64 -> 2001:db8:42::/64",
            "binat inet6 from 2001:db8:1::/64 to 2001:db8:42::/64 -> 2001:db8::/64",
            "pass inet6 proto icmp6"])

        ToolsHelper.print_output("/sbin/sysctl net.inet6.ip6.forwarding=1")

    def vnet3_handler(self, vnet):
        ToolsHelper.print_output("/sbin/route add -6 2001:db8:42::/64 2001:db8:1::1")

    def check_icmp_too_big(self, sp, payload_size, frag_size=None):
        packet = sp.IPv6(src="2001:db8::2", dst="2001:db8:1::2") \
            / sp.ICMPv6EchoRequest(data=sp.raw(bytes.fromhex('f0') * payload_size))

        if frag_size is not None:
            packet = sp.fragment6(packet, frag_size)

        # Delay the send so the sniffer is running when we transmit.
        s = DelayedSend(packet)

        packets = sp.sniff(iface=self.vnet.iface_alias_map["if1"].name,
            timeout=3)
        found=False
        for p in packets:
            # We can't get a reply to this
            assert not p.getlayer(sp.ICMPv6EchoReply)

            if not p.getlayer(sp.ICMPv6PacketTooBig):
                continue

            ip6 = p.getlayer(sp.IPv6)
            icmp6 = p.getlayer(sp.ICMPv6PacketTooBig)

            # Error is from the router vnet
            assert ip6.src == "2001:db8::1"
            assert ip6.dst == "2001:db8::2"

            # And the relevant MTU is 1500
            assert icmp6.mtu == 1500

            # The icmp6 error contains our original IPv6 packet
            err = icmp6.getlayer(sp.IPerror6)
            assert err.src == "2001:db8::2"
            assert err.dst == "2001:db8:1::2"
            assert err.nh == 58

            found = True

        assert found

    def check_icmp_echo(self, sp, payload_size):
        packet = sp.IPv6(src="2001:db8::2", dst="2001:db8:1::2") \
            / sp.ICMPv6EchoRequest(data=sp.raw(bytes.fromhex('f0') * payload_size))

        # Delay the send so the sniffer is running when we transmit.
        s = DelayedSend(packet)

        packets = sp.sniff(iface=self.vnet.iface_alias_map["if1"].name,
            timeout=3)
        found=False
        for p in packets:
            if not p.getlayer(sp.ICMPv6EchoReply):
                continue

            ip6 = p.getlayer(sp.IPv6)
            icmp6 = p.getlayer(sp.ICMPv6EchoReply)

            # Error is from the router vnet
            assert ip6.src == "2001:db8:1::2"
            assert ip6.dst == "2001:db8::2"

            found = True

        assert found

    @pytest.mark.require_user("root")
    def test_npt_icmp(self):
        cl_vnet = self.vnet_map["vnet1"]
        ifname = cl_vnet.iface_alias_map["if1"].name
        ToolsHelper.print_output("/sbin/ifconfig %s mtu 9000" % ifname)

        ToolsHelper.print_output("/sbin/route add -6 2001:db8:1::/64 2001:db8::1")

        # For unclear reasons vnet3 doesn't respond to the first ping.
        # Just send two for now.
        ToolsHelper.print_output("/sbin/ping -6 -c 1 2001:db8:1::2")
        ToolsHelper.print_output("/sbin/ping -6 -c 1 2001:db8:1::2")

        # Import in the correct vnet, so at to not confuse Scapy
        import scapy.all as sp

        # A ping that easily passes without fragmentation
        self.check_icmp_echo(sp, 128)

        # Send a ping that just barely doesn't need to be fragmented
        self.check_icmp_echo(sp, 1452)

        # Send a ping that just barely needs to be fragmented
        self.check_icmp_too_big(sp, 1453)

        # A ping that arrives fragmented
        self.check_icmp_too_big(sp, 12000, 5000)
