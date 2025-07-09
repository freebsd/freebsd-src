#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 Rubicon Communications, LLC (Netgate)
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

import pytest
import re
from utils import DelayedSend
from atf_python.sys.net.tools import ToolsHelper
from atf_python.sys.net.vnet import VnetTestTemplate

class TestHeader(VnetTestTemplate):
    REQUIRED_MODULES = [ "pf" ]
    TOPOLOGY = {
        "vnet1": {"ifaces": ["if1", "if2"]},
        "vnet2": {"ifaces": ["if1", "if2"]},
        "if1": {"prefixes4": [("192.0.2.2/24", "192.0.2.1/24")]},
        "if2": {"prefixes4": [("198.51.100.1/24", "198.51.100.2/24")]},
    }

    def vnet2_handler(self, vnet):
        ToolsHelper.print_output("/sbin/sysctl net.inet.ip.forwarding=1")
        ToolsHelper.print_output("/usr/sbin/arp -s 198.51.100.3 00:01:02:03:04:05")
        ToolsHelper.print_output("/sbin/pfctl -e")
        ToolsHelper.print_output("/sbin/pfctl -x loud")
        ToolsHelper.pf_rules([
            "pass",
            ])

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_too_many(self):
        "Verify that we drop packets with silly numbers of headers."

        sendif = self.vnet.iface_alias_map["if1"]
        recvif = self.vnet.iface_alias_map["if2"].name
        gw_mac = sendif.epairb.ether

        ToolsHelper.print_output("/sbin/route add default 192.0.2.1")

        # Import in the correct vnet, so at to not confuse Scapy
        import scapy.all as sp

        # Sanity check, ensure we get replies to normal ping
        pkt = sp.Ether(dst=gw_mac) \
            / sp.IP(dst="198.51.100.3") \
            / sp.ICMP(type='echo-request')
        s = DelayedSend(pkt, sendif.name)
        reply = sp.sniff(iface=recvif, timeout=3)
        print(reply)

        found = False
        for r in reply:
            r.show()

            icmp = r.getlayer(sp.ICMP)
            if not icmp:
                continue
            assert icmp.type == 8 # 'echo-request'
            found = True
        assert found

        # Up to 19 AH headers will pass
        pkt = sp.Ether(dst=gw_mac) \
            / sp.IP(dst="198.51.100.3")
        for i in range(0, 18):
            pkt = pkt / sp.AH(nh=51, payloadlen=1)
        pkt = pkt / sp.AH(nh=1, payloadlen=1) / sp.ICMP(type='echo-request')

        s = DelayedSend(pkt, sendif.name)
        reply = sp.sniff(iface=recvif, timeout=3)
        print(reply)
        found = False
        for r in reply:
            r.show()

            ah = r.getlayer(sp.AH)
            if not ah:
                continue
            found = True
        assert found

        # But more will get dropped
        pkt = sp.Ether(dst=gw_mac) \
            / sp.IP(dst="198.51.100.3")
        for i in range(0, 19):
            pkt = pkt / sp.AH(nh=51, payloadlen=1)
        pkt = pkt / sp.AH(nh=1, payloadlen=1) / sp.ICMP(type='echo-request')

        s = DelayedSend(pkt, sendif.name)
        reply = sp.sniff(iface=recvif, timeout=3)
        print(reply)

        found = False
        for r in reply:
            r.show()

            ah = r.getlayer(sp.AH)
            if not ah:
                continue
            found = True
        assert not found

class TestHeader6(VnetTestTemplate):
    REQUIRED_MODULES = [ "pf" ]
    SKIP_MODULES = [ "ipfilter" ]
    TOPOLOGY = {
        "vnet1": {"ifaces": ["if1", "if2"]},
        "vnet2": {"ifaces": ["if1", "if2"]},
        "if1": {"prefixes6": [("2001:db8::2/64", "2001:db8::1/64")]},
        "if2": {"prefixes6": [("2001:db8:1::2/64", "2001:db8:1::1/64")]},
    }

    def vnet2_handler(self, vnet):
        ToolsHelper.print_output("/sbin/sysctl net.inet6.ip6.forwarding=1")
        ToolsHelper.print_output("/usr/sbin/ndp -s 2001:db8:1::3 00:01:02:03:04:05")
        ToolsHelper.print_output("/sbin/pfctl -e")
        ToolsHelper.print_output("/sbin/pfctl -x loud")
        ToolsHelper.pf_rules([
            "pass",
            ])

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_too_many(self):
        "Verify that we drop packets with silly numbers of headers."
        ToolsHelper.print_output("/sbin/ifconfig")

        sendif = self.vnet.iface_alias_map["if1"]
        recvif = self.vnet.iface_alias_map["if2"].name
        our_mac = sendif.ether
        gw_mac = sendif.epairb.ether

        ToolsHelper.print_output("/sbin/route -6 add default 2001:db8::1")

        # Import in the correct vnet, so at to not confuse Scapy
        import scapy.all as sp

        # Sanity check, ensure we get replies to normal ping
        pkt = sp.Ether(src=our_mac, dst=gw_mac) \
            / sp.IPv6(src="2001:db8::2", dst="2001:db8:1::3") \
            / sp.ICMPv6EchoRequest()
        s = DelayedSend(pkt, sendif.name)
        reply = sp.sniff(iface=recvif, timeout=3)
        print(reply)

        found = False
        for r in reply:
            r.show()

            icmp = r.getlayer(sp.ICMPv6EchoRequest)
            if not icmp:
                continue
            found = True
        assert found

        # Up to 19 AH headers will pass
        pkt = sp.Ether(src=our_mac, dst=gw_mac) \
            / sp.IPv6(src="2001:db8::2", dst="2001:db8:1::3")
        for i in range(0, 18):
            pkt = pkt / sp.AH(nh=51, payloadlen=1)
        pkt = pkt / sp.AH(nh=58, payloadlen=1) / sp.ICMPv6EchoRequest()
        s = DelayedSend(pkt, sendif.name)
        reply = sp.sniff(iface=recvif, timeout=3)
        print(reply)

        found = False
        for r in reply:
            r.show()

            ah = r.getlayer(sp.AH)
            if not ah:
                continue
            found = True
        assert found

        # But more will get dropped
        pkt = sp.Ether(src=our_mac, dst=gw_mac) \
            / sp.IPv6(src="2001:db8::2", dst="2001:db8:1::3")
        for i in range(0, 19):
            pkt = pkt / sp.AH(nh=51, payloadlen=1)
        pkt = pkt / sp.AH(nh=58, payloadlen=1) / sp.ICMPv6EchoRequest()
        s = DelayedSend(pkt, sendif.name)
        reply = sp.sniff(iface=recvif, timeout=3)
        print(reply)

        found = False
        for r in reply:
            r.show()

            ah = r.getlayer(sp.AH)
            if not ah:
                continue
            found = True
        assert not found
