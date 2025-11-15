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
from utils import DelayedSend
from atf_python.sys.net.tools import ToolsHelper
from atf_python.sys.net.vnet import VnetTestTemplate

class TestMLD(VnetTestTemplate):
    REQUIRED_MODULES = [ "pf" ]
    TOPOLOGY = {
        "vnet1": {"ifaces": ["if1"], "opts": ["allow.read_msgbuf"]},
        "vnet2": {"ifaces": ["if1"]},
        "if1": {"prefixes6": [("2001:db8::2/64", "2001:db8::1/64")]},
    }

    def vnet2_handler(self, vnet):
        ifname = vnet.iface_alias_map["if1"].name
        ToolsHelper.print_output("/sbin/pfctl -e")
        ToolsHelper.pf_rules([
            "pass",
            ])
        ToolsHelper.print_output("/sbin/pfctl -x loud")

    def find_mld_reply(self, pkt, ifname):
        pkt.show()
        s = DelayedSend(pkt, ifname)

        found = False
        packets = self.sp.sniff(iface=ifname, timeout=5)
        for r in packets:
            r.show()
            mld = r.getlayer(self.sp.ICMPv6MLReport2)
            if not mld:
                continue
            mld.show()
            found = True
        return found

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_router_alert(self):
        """Verify that we allow MLD packets with router alert extension header"""
        ifname = self.vnet.iface_alias_map["if1"].name
        ToolsHelper.print_output("/sbin/ifconfig")

        # Import in the correct vnet, so at to not confuse Scapy
        import scapy.all as sp
        import scapy.contrib as sc
        import scapy.contrib.igmp
        self.sp = sp
        self.sc = sc

        # MLD packets with an incorrect hop limit get dropped.
        pkt = sp.Ether() \
            / sp.IPv6(src="fe80::1%%%s" % ifname, dst="ff02::1", hlim=2) \
            / sp.IPv6ExtHdrHopByHop(options=[ \
                sp.RouterAlert(value=0) \
                ]) \
            / sp.ICMPv6MLQuery2()
        # We can't reliably test this by checking for a reply, because
        # the other jail may just send a spontaneous MLD reply.
        self.find_mld_reply(pkt, ifname)

        # Check if we logged dropping the MLD paacket
        dmesg = ToolsHelper.get_output("/sbin/dmesg")
        assert dmesg.find("Invalid MLD") != -1
