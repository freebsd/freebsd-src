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
# 
import pytest
import subprocess
import re
from atf_python.sys.net.tools import ToolsHelper
from atf_python.sys.net.vnet import VnetTestTemplate

def check(cmd):
    ps = subprocess.Popen(cmd, shell=True)
    ret = ps.wait()
    if ret != 0:
        raise Exception("Command %s returned %d" % (cmd, ret))

class TestReturn(VnetTestTemplate):
    REQUIRED_MODULES = [ "pf" ]
    TOPOLOGY = {
        "vnet1": {"ifaces": ["if1"]},
        "vnet2": {"ifaces": ["if1", "if2"]},
        "vnet3": {"ifaces": ["if2"]},
        "if1": {"prefixes4": [("192.0.2.2/24", "192.0.2.1/24")]},
        "if2": {"prefixes4": [("198.51.100.1/24", "198.51.100.2/24")]},
    }

    def vnet2_handler(self, vnet):
        ifname = vnet.iface_alias_map["if1"].name
        if2name = vnet.iface_alias_map["if2"].name

        ToolsHelper.print_output("/sbin/pfctl -e")
        ToolsHelper.pf_rules([
            "nat on %s inet from 192.0.2.0/24 to any -> (%s)" % (ifname, ifname),
            "block return",
            "pass inet proto icmp icmp-type echoreq",
            ])

        ToolsHelper.print_output("/sbin/sysctl net.inet.ip.forwarding=1")
        ToolsHelper.print_output("/sbin/pfctl -x loud")

    def vnet3_handler(self, vnet):
        ToolsHelper.print_output("/sbin/route add default 198.51.100.1")

    def common_setup(self):
        ToolsHelper.print_output("/sbin/route add default 192.0.2.1")

        # Sanity check
        check("/sbin/ping -c 1 192.0.2.1")
        check("/sbin/ping -c 1 198.51.100.1")
        check("/sbin/ping -c 2 198.51.100.2")

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_tcp(self):
        self.common_setup()

        # Import in the correct vnet, so at to not confuse Scapy
        import scapy.all as sp

        # Send a TCP SYN, expect a RST
        pkt = sp.IP(src="192.0.2.2", dst="198.51.100.2") \
            / sp.TCP(sport=4321, dport=1234, flags="S")
        print(pkt)
        reply = sp.sr1(pkt, timeout=3, verbose=False)
        print(reply)

        ip = reply.getlayer(sp.IP)
        tcp = reply.getlayer(sp.TCP)
        assert ip
        assert ip.src == "198.51.100.2"
        assert ip.dst == "192.0.2.2"
        assert tcp
        assert tcp.sport == 1234
        assert tcp.dport == 4321
        assert "R" in tcp.flags

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_udp(self):
        self.common_setup()

        # Import in the correct vnet, so at to not confuse Scapy
        import scapy.all as sp

        # Send a UDP packet, expect ICMP error
        pkt = sp.IP(dst="198.51.100.2") \
            / sp.UDP(sport=4321, dport=1234)
        print(pkt)
        reply = sp.sr1(pkt, timeout=3, verbose=False)
        print(reply)
        ip = reply.getlayer(sp.IP)
        icmp = reply.getlayer(sp.ICMP)
        udp = reply.getlayer(sp.UDPerror)

        assert ip
        assert ip.src == "192.0.2.1"
        assert ip.dst == "192.0.2.2"
        assert icmp
        assert icmp.type == 3
        assert icmp.code == 3
        assert udp
        assert udp.sport == 4321
        assert udp.dport == 1234

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_sctp(self):
        self.common_setup()

        # Import in the correct vnet, so at to not confuse Scapy
        import scapy.all as sp

        # Send an SCTP init, expect an SCTP abort
        pkt = sp.IP(dst="198.51.100.2") \
            / sp.SCTP(sport=1111, dport=2222) \
            / sp.SCTPChunkInit(init_tag=1, n_in_streams=1, n_out_streams=1, a_rwnd=1500)
        print(pkt)
        reply = sp.sr1(pkt, timeout=3, verbose=False)
        print(reply)
        ip = reply.getlayer(sp.IP)
        sctp = reply.getlayer(sp.SCTP)
        abort = reply.getlayer(sp.SCTPChunkAbort)
        print(sctp)

        assert ip
        assert ip.src == "198.51.100.2"
        assert ip.dst == "192.0.2.2"
        assert sctp
        assert sctp.sport == 2222
        assert sctp.dport == 1111
        assert(abort)
