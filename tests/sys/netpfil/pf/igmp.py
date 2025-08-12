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

class TestIGMP(VnetTestTemplate):
    REQUIRED_MODULES = [ "pf" ]
    TOPOLOGY = {
        "vnet1": {"ifaces": ["if1"]},
        "vnet2": {"ifaces": ["if1"]},
        "if1": {"prefixes4": [("192.0.2.2/24", "192.0.2.1/24")]},
    }

    def vnet2_handler(self, vnet):
        ifname = vnet.iface_alias_map["if1"].name
        ToolsHelper.print_output("/sbin/pfctl -e")
        ToolsHelper.pf_rules([
            "pass",
            ])
        ToolsHelper.print_output("/sbin/pfctl -x loud")
        ToolsHelper.print_output("echo \"j 230.0.0.1 %s\ns 3600\nq\" | /usr/sbin/mtest" % ifname)

    def find_igmp_reply(self, pkt, ifname):
        pkt.show()
        s = DelayedSend(pkt)

        found = False
        packets = self.sp.sniff(iface=ifname, timeout=5)
        for r in packets:
            r.show()
            igmp = r.getlayer(self.sc.igmp.IGMP)
            if not igmp:
                continue
            igmp.show()
            if not igmp.gaddr == "230.0.0.1":
                continue
            found = True
        return found

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_ip_opts(self):
        """Verify that we allow IGMP packets with IP options"""
        ifname = self.vnet.iface_alias_map["if1"].name

        # Import in the correct vnet, so at to not confuse Scapy
        import scapy.all as sp
        import scapy.contrib as sc
        import scapy.contrib.igmp
        self.sp = sp
        self.sc = sc

        # We allow IGMP packets with the router alert option
        pkt = sp.IP(dst="224.0.0.1%%%s" % ifname, ttl=1,
            options=[sp.IPOption_Router_Alert()]) \
            / sc.igmp.IGMP(type=0x11, mrcode=1)
        assert self.find_igmp_reply(pkt, ifname)

        # But not with other options
        pkt = sp.IP(dst="224.0.0.1%%%s" % ifname, ttl=1,
            options=[sp.IPOption_NOP()]) \
            / sc.igmp.IGMP(type=0x11, mrcode=1)
        assert not self.find_igmp_reply(pkt, ifname)

        # Or with the wrong TTL
        pkt = sp.IP(dst="224.0.0.1%%%s" % ifname, ttl=2,
            options=[sp.IPOption_Router_Alert()]) \
            / sc.igmp.IGMP(type=0x11, mrcode=1)
        assert not self.find_igmp_reply(pkt, ifname)

        # Or with the wrong destination address
        pkt = sp.IP(dst="224.0.0.2%%%s" % ifname, ttl=2,
            options=[sp.IPOption_Router_Alert()]) \
            / sc.igmp.IGMP(type=0x11, mrcode=1)
        assert not self.find_igmp_reply(pkt, ifname)
