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
import selectors
import socket
import sys
from utils import DelayedSend
from atf_python.sys.net.tools import ToolsHelper
from atf_python.sys.net.vnet import VnetTestTemplate

class TestNAT64(VnetTestTemplate):
    REQUIRED_MODULES = [ "pf", "pflog" ]
    TOPOLOGY = {
        "vnet1": {"ifaces": ["if1"]},
        "vnet2": {"ifaces": ["if1", "if2"]},
        "vnet3": {"ifaces": ["if2", "if3"]},
        "vnet4": {"ifaces": ["if3"]},
        "if1": {"prefixes6": [("2001:db8::2/64", "2001:db8::1/64")]},
        "if2": {"prefixes4": [("192.0.2.1/24", "192.0.2.2/24")]},
        "if3": {"prefixes4": [("198.51.100.1/24", "198.51.100.2/24")]}
    }

    def vnet4_handler(self, vnet):
        ToolsHelper.print_output("/sbin/route add default 198.51.100.1")

    def vnet3_handler(self, vnet):
        ToolsHelper.print_output("/sbin/sysctl net.inet.ip.forwarding=1")
        ToolsHelper.print_output("/sbin/sysctl net.inet.ip.ttl=62")
        ToolsHelper.print_output("/sbin/sysctl net.inet.udp.checksum=0")

        sel = selectors.DefaultSelector()
        t = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        t.bind(("0.0.0.0", 1234))
        t.setblocking(False)
        t.listen()
        sel.register(t, selectors.EVENT_READ, data=None)

        u = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        u.bind(("0.0.0.0", 4444))
        u.setblocking(False)
        sel.register(u, selectors.EVENT_READ, data="UDP")

        while True:
            events = sel.select(timeout=20)
            for key, mask in events:
                sock = key.fileobj
                if key.data is None:
                    conn, addr = sock.accept()
                    print(f"Accepted connection from {addr}")
                    data = types.SimpleNamespace(addr=addr, inb=b"", outb=b"")
                    events = selectors.EVENT_READ | selectors.EVENT_WRITE
                    sel.register(conn, events, data=data)
                elif key.data == "UDP":
                    recv_data, addr = sock.recvfrom(1024)
                    print(f"Received UDP {recv_data} from {addr}")
                    sock.sendto(b"foo", addr)
                else:
                    if mask & selectors.EVENT_READ:
                        recv_data = sock.recv(1024)
                        print(f"Received TCP {recv_data}")
                        sock.send(b"foo")
                    else:
                        print("Unknown event?")
                        t.close()
                        u.close()
                        return

    def vnet2_handler(self, vnet):
        ifname = vnet.iface_alias_map["if1"].name

        ToolsHelper.print_output("/sbin/sysctl net.inet6.ip6.forwarding=1")
        ToolsHelper.print_output("/sbin/route add default 192.0.2.2")
        ToolsHelper.print_output("/sbin/pfctl -e")
        ToolsHelper.pf_rules([
            "block",
            "pass inet6 proto icmp6 icmp6-type { neighbrsol, neighbradv }",
            "pass in on %s inet6 af-to inet from 192.0.2.1" % ifname,
        ])

        vnet.pipe.send(socket.if_nametoindex("pflog0"))

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_tcp_rst(self):
        ToolsHelper.print_output("/sbin/route -6 add default 2001:db8::1")

        import scapy.all as sp

        # Send a SYN
        packet = sp.IPv6(dst="64:ff9b::192.0.2.2") \
            / sp.TCP(dport=1222, flags="S")

        # Get a reply
        reply = sp.sr1(packet)

        # We expect to get a RST here.
        tcp = reply.getlayer(sp.TCP)
        assert tcp
        assert "R" in tcp.flags

        # Now try to SYN to an open port
        packet = sp.IPv6(dst="64:ff9b::192.0.2.2") \
            / sp.TCP(dport=1234, flags="S")
        reply = sp.sr1(packet)

        tcp = reply.getlayer(sp.TCP)
        assert tcp

        # We don't get RST
        assert "R" not in tcp.flags

        # We do get SYN|ACK
        assert "S" in tcp.flags
        assert "A" in tcp.flags

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_udp_port_closed(self):
        ToolsHelper.print_output("/sbin/route -6 add default 2001:db8::1")

        import scapy.all as sp

        packet = sp.IPv6(dst="64:ff9b::192.0.2.2") \
            / sp.UDP(dport=1222) / sp.Raw("bar")
        reply = sp.sr1(packet, timeout=3)
        print(reply.show())

        # We expect an ICMPv6 error, not a UDP reply
        assert not reply.getlayer(sp.UDP)
        icmp = reply.getlayer(sp.ICMPv6DestUnreach)
        assert icmp
        assert icmp.type == 1
        assert icmp.code == 4
        udp = reply.getlayer(sp.UDPerror)
        assert udp
        assert udp.dport == 1222

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_address_unreachable(self):
        ToolsHelper.print_output("/sbin/route -6 add default 2001:db8::1")

        import scapy.all as sp

        packet = sp.IPv6(dst="64:ff9b::203.0.113.2") \
            / sp.UDP(dport=1222) / sp.Raw("bar")
        reply = sp.sr1(packet, timeout=3)
        print(reply.show())

        # We expect an ICMPv6 error, not a UDP reply
        assert not reply.getlayer(sp.UDP)
        icmp = reply.getlayer(sp.ICMPv6DestUnreach)
        assert icmp
        assert icmp.type == 1
        assert icmp.code == 0
        udp = reply.getlayer(sp.UDPerror)
        assert udp
        assert udp.dport == 1222

        # Check the hop limit
        ip6 = reply.getlayer(sp.IPv6)
        assert ip6.hlim == 61

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_udp_checksum(self):
        ToolsHelper.print_output("/sbin/route -6 add default 2001:db8::1")

        import scapy.all as sp

        # Send an outbound UDP packet to establish state
        packet = sp.IPv6(dst="64:ff9b::192.0.2.2") \
            / sp.UDP(sport=3333, dport=4444) / sp.Raw("foo")

        # Get a reply
        # We'll send the reply without UDP checksum on the IPv4 side
        # but that's not valid for IPv6, so expect pf to update the checksum.
        reply = sp.sr1(packet, timeout=5)

        udp = reply.getlayer(sp.UDP)
        assert udp
        assert udp.chksum != 0

    def common_test_source_addr(self, packet):
        vnet = self.vnet_map["vnet1"]
        sendif = vnet.iface_alias_map["if1"].name

        import scapy.all as sp

        print("Outbound:\n")
        packet.show()

        s = DelayedSend(packet)

        # We expect an ICMPv6 error here, where we'll verify the source address of
        # the outer packet
        packets = sp.sniff(iface=sendif, timeout=5)

        for reply in packets:
            print("Reply:\n")
            reply.show()
            icmp = reply.getlayer(sp.ICMPv6TimeExceeded)
            if not icmp:
                continue

            ip = reply.getlayer(sp.IPv6)
            assert icmp
            assert ip.src == "64:ff9b::c000:202"
            return reply

        # If we don't find the packet we expect to see
        assert False

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_source_addr_tcp(self):
        ToolsHelper.print_output("/sbin/route -6 add default 2001:db8::1")
        import scapy.all as sp

        packet = sp.IPv6(dst="64:ff9b::198.51.100.2", hlim=2) \
            / sp.TCP(sport=1111, dport=2222, flags="S")
        self.common_test_source_addr(packet)

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_source_addr_udp(self):
        ToolsHelper.print_output("/sbin/route -6 add default 2001:db8::1")
        import scapy.all as sp

        packet = sp.IPv6(dst="64:ff9b::198.51.100.2", hlim=2) \
            / sp.UDP(sport=1111, dport=2222) / sp.Raw("foo")
        self.common_test_source_addr(packet)

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_source_addr_sctp(self):
        ToolsHelper.print_output("/sbin/route -6 add default 2001:db8::1")
        import scapy.all as sp

        packet = sp.IPv6(dst="64:ff9b::198.51.100.2", hlim=2) \
            / sp.SCTP(sport=1111, dport=2222) \
            / sp.SCTPChunkInit(init_tag=1, n_in_streams=1, n_out_streams=1, a_rwnd=1500)
        self.common_test_source_addr(packet)

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_source_addr_icmp(self):
        ToolsHelper.print_output("/sbin/route -6 add default 2001:db8::1")
        import scapy.all as sp

        packet = sp.IPv6(dst="64:ff9b::198.51.100.2", hlim=2) \
            / sp.ICMPv6EchoRequest() / sp.Raw("foo")
        reply = self.common_test_source_addr(packet)
        icmp = reply.getlayer(sp.ICMPv6EchoRequest)
        assert icmp

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_bad_len(self):
        """
            PR 288224: we can panic if the IPv6 plen is longer than the packet length.
        """
        ToolsHelper.print_output("/sbin/route -6 add default 2001:db8::1")
        import scapy.all as sp

        packet = sp.IPv6(dst="64:ff9b::198.51.100.2", hlim=2, plen=512) \
            / sp.ICMPv6EchoRequest() / sp.Raw("foo")
        reply = sp.sr1(packet, timeout=3)
        # We don't expect a reply to a corrupted packet
        assert not reply

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_noip6(self):
        """
            PR 288263: link-local target address in icmp6 ADVERT can cause NULL deref
        """
        ifname = self.vnet.iface_alias_map["if1"].name
        gw_mac = self.vnet.iface_alias_map["if1"].epairb.ether
        scopeid = self.wait_object(self.vnet_map["vnet2"].pipe)
        ToolsHelper.print_output("/sbin/route -6 add default 2001:db8::1")

        import scapy.all as sp

        pkt = sp.Ether(dst=gw_mac) \
            / sp.IPv6(dst="64:ff9b::203.0.113.2") \
            / sp.ICMPv6ND_NA(tgt="FFA2:%x:2821:125F:1D27:B3B2:3F6F:C43C" % scopeid)
        pkt.show()
        sp.hexdump(pkt)
        s = DelayedSend(pkt, sendif=ifname)

        packets = sp.sniff(iface=ifname, timeout=5)
        for r in packets:
            r.show()

        # Try scope id that likely doesn't have an interface at all
        pkt = sp.Ether(dst=gw_mac) \
            / sp.IPv6(dst="64:ff9b::203.0.113.2") \
            / sp.ICMPv6ND_NA(tgt="FFA2:%x:2821:125F:1D27:B3B2:3F6F:C43C" % 255)
        pkt.show()
        sp.hexdump(pkt)
        s = DelayedSend(pkt, sendif=ifname)

        packets = sp.sniff(iface=ifname, timeout=5)
        for r in packets:
            r.show()

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_ttl_one(self):
        """
            PR 291527: invalid ICMP error generated by nat64 router
        """
        ifname = self.vnet.iface_alias_map["if1"].name
        gw_mac = self.vnet.iface_alias_map["if1"].epairb.ether
        ToolsHelper.print_output("/sbin/route -6 add default 2001:db8::1")

        import scapy.all as sp

        pkt = sp.Ether(dst=gw_mac) \
            / sp.IPv6(dst="64:ff9b::198.51.100.2", hlim=1) \
            / sp.SCTP(sport=1111, dport=2222) \
            / sp.SCTPChunkInit(init_tag=1, n_in_streams=1, n_out_streams=1, \
                a_rwnd=1500, params=[])
        s = DelayedSend(pkt, sendif=ifname)

        found = False
        packets = sp.sniff(iface=ifname, timeout=5)
        for r in packets:
            print("Reply packet:")
            r.show()

            ip6 = r.getlayer(sp.IPv6)
            icmp6 = r.getlayer(sp.ICMPv6TimeExceeded)
            if not ip6 or not icmp6:
                continue
            assert ip6.src == "2001:db8::1"
            assert ip6.dst == "2001:db8::2"
            assert icmp6.type == 3 # Time exceeded
            assert icmp6.code == 0 # hop limit exceeded in transit
            found = True
        assert found

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_ttl_zero(self):
        """
            PR 288274: we can use an mbuf after free on TTL = 0
        """
        ifname = self.vnet.iface_alias_map["if1"].name
        gw_mac = self.vnet.iface_alias_map["if1"].epairb.ether
        ToolsHelper.print_output("/sbin/route -6 add default 2001:db8::1")

        import scapy.all as sp

        pkt = sp.Ether(dst=gw_mac) \
            / sp.IPv6(dst="64:ff9b::192.0.2.2", hlim=0) \
            / sp.SCTP(sport=1111, dport=2222) \
            / sp.SCTPChunkInit(init_tag=1, n_in_streams=1, n_out_streams=1, \
                a_rwnd=1500, params=[ \
                    sp.SCTPChunkParamIPv4Addr() \
                ])
        pkt.show()
        sp.hexdump(pkt)
        s = DelayedSend(pkt, sendif=ifname)

        packets = sp.sniff(iface=ifname, timeout=5)
        for r in packets:
            r.show()

