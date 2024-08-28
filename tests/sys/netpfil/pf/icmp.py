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
        "if2": {"prefixes4": [("198.51.100.1/24", "198.51.100.2/24")]},
    }

    def vnet2_handler(self, vnet):
        ifname = vnet.iface_alias_map["if1"].name

        ToolsHelper.print_output("/sbin/pfctl -e")
        ToolsHelper.pf_rules([
            "set reassemble yes",
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
    def test_inner_match(self):
        vnet = self.vnet_map["vnet1"]
        dst_vnet = self.vnet_map["vnet3"]
        sendif = vnet.iface_alias_map["if1"].name

        our_mac = ToolsHelper.get_output("/sbin/ifconfig %s ether | awk '/ether/ { print $2; }'" % sendif)
        dst_mac = re.sub("0a$", "0b", our_mac)

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
        sp.sendp(pkt, sendif, verbose=False)

        # Now try to pass an ICMP error message piggy-backing on that state, but
        # use a different source address
        pkt = sp.Ether(src=our_mac, dst=dst_mac) \
            / sp.IP(src="192.0.2.2", dst="198.51.100.3") \
            / sp.ICMP(type='dest-unreach') \
            / sp.IP(src="198.51.100.2", dst="192.0.2.2") \
            / sp.ICMP(type='echo-reply')
        sp.sendp(pkt, sendif, verbose=False)

        try:
            rcvd = self.wait_object(dst_vnet.pipe, timeout=1)
            if rcvd:
                raise Exception(rcvd)
        except TimeoutError as e:
            # We expect the timeout here. It means we didn't get the destination
            # unreachable packet in vnet3
            pass
