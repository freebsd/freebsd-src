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

import sys
import pytest
import random
import socket
import selectors
from utils import DelayedSend
from atf_python.sys.net.tools import ToolsHelper
from atf_python.sys.net.vnet import VnetTestTemplate

class TCPClient:
    def __init__(self, src, dst, sport, dport, sp):
        self.src = src
        self.dst = dst
        self.sport = sport
        self.dport = dport
        self.sp = sp
        self.seq = random.randrange(1, (2**32)-1)
        self.ack = 0

    def syn(self):
        syn = self.sp.IP(src=self.src, dst=self.dst) \
            / self.sp.TCP(sport=self.sport, dport=self.dport, flags="S", seq=self.seq)
        return syn
    
    def connect(self):
        syn = self.syn()
        r = self.sp.sr1(syn, timeout=5)

        assert r
        t = r.getlayer(self.sp.TCP)
        assert t
        assert t.sport == self.dport
        assert t.dport == self.sport
        assert t.flags == "SA"

        self.seq += 1
        self.ack = t.seq + 1
        ack = self.sp.IP(src=self.src, dst=self.dst) \
            / self.sp.TCP(sport=self.sport, dport=self.dport, flags="A", ack=self.ack, seq=self.seq)
        self.sp.send(ack)

    def send(self, data):
        length = len(data)
        pkt = self.sp.IP(src=self.src, dst=self.dst) \
            / self.sp.TCP(sport=self.sport, dport=self.dport, ack=self.ack, seq=self.seq, flags="") \
            / self.sp.Raw(data)
        self.seq += length
        pkt.show()
        self.sp.send(pkt)

class TestTcp(VnetTestTemplate):
    REQUIRED_MODULES = [ "pf" ]
    TOPOLOGY = {
        "vnet1": {"ifaces": ["if1"]},
        "vnet2": {"ifaces": ["if1"]},
        "if1": {"prefixes4": [("192.0.2.1/24", "192.0.2.2/24")]},
    }

    def vnet2_handler(self, vnet):
        ToolsHelper.print_output("/usr/sbin/arp -s 192.0.2.3 00:01:02:03:04:05")
        ToolsHelper.print_output("/sbin/pfctl -e")
        ToolsHelper.pf_rules([
            "pass"
        ])
        ToolsHelper.print_output("/sbin/pfctl -x loud")

        # Start TCP listener
        sel = selectors.DefaultSelector()
        t = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        t.bind(("0.0.0.0", 1234))
        t.listen(100)
        t.setblocking(False)
        sel.register(t, selectors.EVENT_READ, data=None)

        while True:
            events = sel.select(timeout=2)
            for key, mask in events:
                sock = key.fileobj
                if key.data is None:
                    conn, addr = sock.accept()
                    print(f"Accepted connection from {addr}")
                    events = selectors.EVENT_READ | selectors.EVENT_WRITE
                    sel.register(conn, events, data="TCP")
                else:
                    if mask & selectors.EVENT_READ:
                        recv_data = sock.recv(1024)
                        print(f"Received TCP {recv_data}")
                        ToolsHelper.print_output("/sbin/pfctl -ss -vv")
                        sock.send(recv_data)

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_challenge_ack(self):
        vnet = self.vnet_map["vnet1"]
        ifname = vnet.iface_alias_map["if1"].name

        # Import in the correct vnet, so at to not confuse Scapy
        import scapy.all as sp

        a = TCPClient("192.0.2.3", "192.0.2.2", 1234, 1234, sp)
        a.connect()
        a.send(b"foo")

        b = TCPClient("192.0.2.3", "192.0.2.2", 1234, 1234, sp)
        syn = b.syn()
        syn.show()
        s = DelayedSend(syn)
        packets = sp.sniff(iface=ifname, timeout=3)
        found = False
        for p in packets:
            ip = p.getlayer(sp.IP)
            if not ip:
                continue
            tcp = p.getlayer(sp.TCP)
            if not tcp:
                continue

            if ip.src != "192.0.2.2":
                continue

            p.show()

            assert ip.dst == "192.0.2.3"
            assert tcp.sport == 1234
            assert tcp.dport == 1234
            assert tcp.flags == "A"

            # We only expect one
            assert not found
            found = True

        assert found
