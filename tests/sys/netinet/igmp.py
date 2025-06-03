#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2023 Rubicon Communications, LLC (Netgate)
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
import pytest
from atf_python.sys.net.tools import ToolsHelper
from atf_python.sys.net.vnet import VnetTestTemplate
import os
import socket
import struct
import sys
import logging
logging.getLogger("scapy").setLevel(logging.CRITICAL)
curdir = os.path.dirname(os.path.realpath(__file__))
netpfil_common = curdir + "/../netpfil/common"
sys.path.append(netpfil_common)

sc = None
sp = None

def check_igmpv3(args, pkt):
    igmp = pkt.getlayer(sc.igmpv3.IGMPv3)
    if igmp is None:
        return False

    igmpmr = pkt.getlayer(sc.igmpv3.IGMPv3mr)
    if igmpmr is None:
        return False

    for r in igmpmr.records:
        if r.maddr != args["group"]:
            return False
        if args["type"] == "join":
            if r.rtype != 4:
                return False
        elif args["type"] == "leave":
            if r.rtype != 3:
                return False
        r.show()

    return True

def check_igmpv2(args, pkt):
    pkt.show()

    igmp = pkt.getlayer(sc.igmp.IGMP)
    if igmp is None:
        return False

    if igmp.gaddr != args["group"]:
        return False

    if args["type"] == "join":
        if igmp.type != 0x16:
            return False
    if args["type"] == "leave":
        if igmp.type != 0x17:
            return False

    return True

class TestIGMP(VnetTestTemplate):
    REQUIRED_MODULES = []
    TOPOLOGY = {
        "vnet1": { "ifaces": [ "if1" ] },
        "if1": { "prefixes4": [ ("192.0.2.1/24", "192.0.2.2/24" ) ] },
    }

    def setup_method(self, method):
        global sc
        if sc is None:
            import scapy.contrib as _sc
            import scapy.contrib.igmp
            import scapy.contrib.igmpv3
            import scapy.all as _sp
            sc = _sc
            sp = _sp
        super().setup_method(method)

    @pytest.mark.require_progs(["scapy"])
    def test_igmp3_join_leave(self):
        "Test that we send the expected join/leave IGMPv3 messages"

        if1 = self.vnet.iface_alias_map["if1"]

        # Start a background sniff
        from sniffer import Sniffer
        expected_pkt = { "type": "join", "group": "230.0.0.1" }
        sniffer = Sniffer(expected_pkt, check_igmpv3, if1.name, timeout=10)

        # Now join a multicast group, and see if we're getting the igmp packet we expect
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        mreq = struct.pack("4sl", socket.inet_aton('230.0.0.1'), socket.INADDR_ANY)
        s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

        # Wait for the sniffer to see the join packet
        sniffer.join()
        assert(sniffer.correctPackets > 0)

        # Now leave, check for the packet
        expected_pkt = { "type": "leave", "group": "230.0.0.1" }
        sniffer = Sniffer(expected_pkt, check_igmpv3, if1.name)

        s.close()
        sniffer.join()
        assert(sniffer.correctPackets > 0)

    @pytest.mark.require_progs(["scapy"])
    def test_igmp2_join_leave(self):
        "Test that we send the expected join/leave IGMPv2 messages"
        ToolsHelper.print_output("/sbin/sysctl net.inet.igmp.default_version=2")

        if1 = self.vnet.iface_alias_map["if1"]

        # Start a background sniff
        from sniffer import Sniffer
        expected_pkt = { "type": "join", "group": "230.0.0.1" }
        sniffer = Sniffer(expected_pkt, check_igmpv2, if1.name, timeout=10)

        # Now join a multicast group, and see if we're getting the igmp packet we expect
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        mreq = struct.pack("4sl", socket.inet_aton('230.0.0.1'), socket.INADDR_ANY)
        s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

        # Wait for the sniffer to see the join packet
        sniffer.join()
        assert(sniffer.correctPackets > 0)

        # Now leave, check for the packet
        expected_pkt = { "type": "leave", "group": "230.0.0.1" }
        sniffer = Sniffer(expected_pkt, check_igmpv2, if1.name)

        s.close()
        sniffer.join()
        assert(sniffer.correctPackets > 0)
