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

class TestICMP(VnetTestTemplate):
    REQUIRED_MODULES = [ "pf" ]
    TOPOLOGY = {
        "vnet1": {"ifaces": ["if1"]},
        "vnet2": {"ifaces": ["if1", "if2"]},
        "vnet3": {"ifaces": ["if2"]},
        "if1": {"prefixes4": [("192.0.2.2/24", "192.0.2.1/24")]},
        "if2": {"prefixes4": [("198.51.100.1/24", "198.51.100.2/24")], "mtu": 1492},
    }

    def vnet2_handler(self, vnet):
        ToolsHelper.print_output("/sbin/pfctl -e")
        ToolsHelper.pf_rules([
            "set reassemble yes",
            "set state-policy if-bound",
            "block",
            "pass inet proto icmp icmp-type echoreq",
            ])

        ToolsHelper.print_output("/sbin/sysctl net.inet.ip.forwarding=1")
        ToolsHelper.print_output("/sbin/pfctl -x loud")

    def vnet3_handler(self, vnet):
        # Import in the correct vnet, so at to not confuse Scapy
        import scapy.all as sp

        ifname = vnet.iface_alias_map["if2"].name
        ToolsHelper.print_output("/sbin/route add default 198.51.100.1")
        ToolsHelper.print_output("/sbin/ifconfig %s inet alias 198.51.100.3/24" % ifname)

        def checkfn(packet):
            icmp = packet.getlayer(sp.ICMP)
            if not icmp:
                return False

            if icmp.type != 3:
                return False

            packet.show()
            return True

        sp.sniff(iface=ifname, stop_filter=checkfn)
        vnet.pipe.send("Got ICMP destination unreachable packet")

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_inner_match(self):
        vnet = self.vnet_map["vnet1"]
        dst_vnet = self.vnet_map["vnet3"]
        sendif = vnet.iface_alias_map["if1"]

        our_mac = sendif.ether
        dst_mac = sendif.epairb.ether

        # Import in the correct vnet, so at to not confuse Scapy
        import scapy.all as sp

        ToolsHelper.print_output("/sbin/route add default 192.0.2.1")

        # Sanity check
        check("/sbin/ping -c 1 192.0.2.1")
        check("/sbin/ping -c 1 198.51.100.1")
        check("/sbin/ping -c 2 198.51.100.3")

        # Establish a state
        pkt = sp.Ether(src=our_mac, dst=dst_mac) \
            / sp.IP(src="192.0.2.2", dst="198.51.100.2") \
            / sp.ICMP(type='echo-request') \
            / "PAYLOAD"
        sp.sendp(pkt, sendif.name, verbose=False)

        # Now try to pass an ICMP error message piggy-backing on that state, but
        # use a different source address
        pkt = sp.Ether(src=our_mac, dst=dst_mac) \
            / sp.IP(src="192.0.2.2", dst="198.51.100.3") \
            / sp.ICMP(type='dest-unreach') \
            / sp.IP(src="198.51.100.2", dst="192.0.2.2") \
            / sp.ICMP(type='echo-reply')
        sp.sendp(pkt, sendif.name, verbose=False)

        try:
            rcvd = self.wait_object(dst_vnet.pipe, timeout=1)
            if rcvd:
                raise Exception(rcvd)
        except TimeoutError as e:
            # We expect the timeout here. It means we didn't get the destination
            # unreachable packet in vnet3
            pass

    def check_icmp_echo(self, sp, payload_size):
        packet = sp.IP(dst="198.51.100.2", flags="DF") \
            / sp.ICMP(type='echo-request') \
            / sp.raw(bytes.fromhex('f0') * payload_size)

        p = sp.sr1(packet, timeout=3)
        p.show()

        ip = p.getlayer(sp.IP)
        icmp = p.getlayer(sp.ICMP)
        assert ip
        assert icmp

        if payload_size > 1464:
            # Expect ICMP destination unreachable, fragmentation needed
            assert ip.src == "192.0.2.1"
            assert ip.dst == "192.0.2.2"
            assert icmp.type == 3 # dest-unreach
            assert icmp.code == 4
            assert icmp.nexthopmtu == 1492
        else:
            # Expect echo reply
            assert ip.src == "198.51.100.2"
            assert ip.dst == "192.0.2.2"
            assert icmp.type == 0 # "echo-reply"
            assert icmp.code == 0

        return

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_fragmentation_needed(self):
        ToolsHelper.print_output("/sbin/route add default 192.0.2.1")

        ToolsHelper.print_output("/sbin/ping -c 1 198.51.100.2")
        ToolsHelper.print_output("/sbin/ping -c 1 -D -s 1472 198.51.100.2")

        # Import in the correct vnet, so at to not confuse Scapy
        import scapy.all as sp

        self.check_icmp_echo(sp, 128)
        self.check_icmp_echo(sp, 1464)
        self.check_icmp_echo(sp, 1468)

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_truncated_opts(self):
        ToolsHelper.print_output("/sbin/route add default 192.0.2.1")

        # Import in the correct vnet, so at to not confuse Scapy
        import scapy.all as sp

        packet = sp.IP(dst="198.51.100.2", flags="DF") \
            / sp.ICMP(type='dest-unreach', length=108) \
            / sp.IP(src="198.51.100.2", dst="192.0.2.2", len=1000, \
              ihl=(120 >> 2), options=[ \
              sp.IPOption_Security(length=100)])
        packet.show()
        sp.sr1(packet, timeout=3)

class TestICMP_NAT(VnetTestTemplate):
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
            "set reassemble yes",
            "set state-policy if-bound",
            "nat on %s inet from 192.0.2.0/24 to any -> (%s)" % (if2name, if2name),
            "block",
            "pass inet proto icmp icmp-type echoreq",
            ])

        ToolsHelper.print_output("/sbin/sysctl net.inet.ip.forwarding=1")
        ToolsHelper.print_output("/sbin/pfctl -x loud")

    def vnet3_handler(self, vnet):
        ifname = vnet.iface_alias_map["if2"].name
        ToolsHelper.print_output("/sbin/ifconfig %s inet alias 198.51.100.3/24" % ifname)

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_id_conflict(self):
        """
            Test ICMP echo requests with the same ID from different clients.
            Windows does this, and it can confuse pf.
        """
        ifname = self.vnet.iface_alias_map["if1"].name
        ToolsHelper.print_output("/sbin/route add default 192.0.2.1")
        ToolsHelper.print_output("/sbin/ifconfig %s inet alias 192.0.2.3/24" % ifname)

        ToolsHelper.print_output("/sbin/ping -c 1 192.0.2.1")
        ToolsHelper.print_output("/sbin/ping -c 1 198.51.100.1")
        ToolsHelper.print_output("/sbin/ping -c 1 198.51.100.2")
        ToolsHelper.print_output("/sbin/ping -c 1 198.51.100.3")

        # Import in the correct vnet, so at to not confuse Scapy
        import scapy.all as sp

        # Address one
        packet = sp.IP(src="192.0.2.2", dst="198.51.100.2", flags="DF") \
            / sp.ICMP(type='echo-request', id=42) \
            / sp.raw(bytes.fromhex('f0') * 16)

        p = sp.sr1(packet, timeout=3)
        p.show()
        ip = p.getlayer(sp.IP)
        icmp = p.getlayer(sp.ICMP)
        assert ip
        assert icmp
        assert ip.dst == "192.0.2.2"
        assert icmp.id == 42

        # Address one
        packet = sp.IP(src="192.0.2.3", dst="198.51.100.2", flags="DF") \
            / sp.ICMP(type='echo-request', id=42) \
            / sp.raw(bytes.fromhex('f0') * 16)

        p = sp.sr1(packet, timeout=3)
        p.show()
        ip = p.getlayer(sp.IP)
        icmp = p.getlayer(sp.ICMP)
        assert ip
        assert icmp
        assert ip.dst == "192.0.2.3"
        assert icmp.id == 42
