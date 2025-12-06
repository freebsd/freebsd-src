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
from atf_python.sys.net.tools import ToolsHelper
from atf_python.sys.net.vnet import VnetTestTemplate

class TestNAT44(VnetTestTemplate):
    REQUIRED_MODULES = [ "pf" ]
    TOPOLOGY = {
        "vnet1": {"ifaces": ["if1"]},
        "vnet2": {"ifaces": ["if1", "if2"]},
        "vnet3": {"ifaces": ["if2"]},
        "if1": {"prefixes4": [("192.0.2.2/24", "192.0.2.1/24")]},
        "if2": {"prefixes4": [("198.51.100.1/24", "198.51.100.2")]},
    }

    def vnet2_handler(self, vnet):
        outifname = vnet.iface_alias_map["if2"].name
        ToolsHelper.print_output("/sbin/sysctl net.inet.ip.forwarding=1")

        ToolsHelper.print_output("/sbin/pfctl -e")
        ToolsHelper.print_output("/sbin/pfctl -x loud")
        ToolsHelper.pf_rules([
            "set reassemble yes",
            "nat on {} inet from 192.0.2.0/24 -> ({})".format(outifname, outifname),
            "pass"])

    def vnet3_handler(self, vnet):
        pass

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_nat_igmp(self):
        "Verify that NAT translation of !(TCP|UDP|SCTP|ICMP) doesn't panic"
        ToolsHelper.print_output("/sbin/route add default 192.0.2.1")
        ToolsHelper.print_output("ping -c 3 198.51.100.2")

        # Import in the correct vnet, so at to not confuse Scapy
        import scapy.all as sp
        import scapy.contrib as sc
        import scapy.contrib.igmp

        pkt = sp.IP(dst="198.51.100.2", ttl=64) \
            / sc.igmp.IGMP(type=0x11, mrcode=1)
        sp.send(pkt)

        # This time we'll hit an existing state
        pkt = sp.IP(dst="198.51.100.2", ttl=64) \
            / sc.igmp.IGMP(type=0x11, mrcode=1)
        reply = sp.sr1(pkt, timeout=3)
        if reply:
            reply.show()
