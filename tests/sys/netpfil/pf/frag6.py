import pytest
import logging
import threading
import time
import random
logging.getLogger("scapy").setLevel(logging.CRITICAL)
from atf_python.sys.net.tools import ToolsHelper
from atf_python.sys.net.vnet import VnetTestTemplate

class DelayedSend(threading.Thread):
    def __init__(self, packet):
        threading.Thread.__init__(self)
        self._packet = packet

        self.start()

    def run(self):
        import scapy.all as sp
        time.sleep(1)
        sp.send(self._packet)

class TestFrag6(VnetTestTemplate):
    REQUIRED_MODULES = ["pf", "dummymbuf"]
    TOPOLOGY = {
        "vnet1": {"ifaces": ["if1"]},
        "vnet2": {"ifaces": ["if1"]},
        "if1": {"prefixes6": [("2001:db8::1/64", "2001:db8::2/64")]},
    }

    def vnet2_handler(self, vnet):
        ifname = vnet.iface_alias_map["if1"].name
        ToolsHelper.print_output("/sbin/pfctl -e")
        ToolsHelper.pf_rules([
            "scrub fragment reassemble min-ttl 10",
            "pass",
            "block in inet6 proto icmp6 icmp6-type echoreq",
        ])
        ToolsHelper.print_output("/sbin/pfilctl link -i dummymbuf:inet6 inet6")
        ToolsHelper.print_output("/sbin/sysctl net.dummymbuf.rules=\"inet6 in %s enlarge 3000;\"" % ifname)

    def check_ping_reply(self, packet):
        print(packet)
        return False

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_dup_frag_hdr(self):
        "Test packets with duplicate fragment headers"
        srv_vnet = self.vnet_map["vnet2"]

        # Import in the correct vnet, so at to not confuse Scapy
        import scapy.all as sp

        packet = sp.IPv6(src="2001:db8::1", dst="2001:db8::2") \
            / sp.IPv6ExtHdrFragment(offset = 0, m = 0) \
            / sp.IPv6ExtHdrFragment(offset = 0, m = 0) \
            / sp.ICMPv6EchoRequest(data=sp.raw(bytes.fromhex('f00f') * 128))

        # Delay the send so the sniffer is running when we transmit.
        s = DelayedSend(packet)

        packets = sp.sniff(iface=self.vnet.iface_alias_map["if1"].name,
            timeout=3)
        for p in packets:
            assert not p.getlayer(sp.ICMPv6EchoReply)

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_overlong(self):
        "Test overly long fragmented packet"

        # Import in the correct vnet, so at to not confuse Scapy
        import scapy.all as sp

        curr = 0
        pkts = []

        frag_id = random.randint(0,0xffffffff)
        gran = 1200

        i = 0
        while curr <= 65535:
            ipv61 = sp.IPv6(src="2001:db8::1", dst="2001:db8::2")
            more = True
            g = gran
            if curr + gran > 65535:
                more = False
                g = 65530 - curr
            if i == 0:
                pkt = ipv61 / sp.IPv6ExtHdrHopByHop(options=[sp.PadN(optlen=2), sp.Pad1()]) / \
                    sp.IPv6ExtHdrFragment(id = frag_id, offset = curr // 8, m = more) / bytes([i] * g)
            else:
                pkt = ipv61 / sp.IPv6ExtHdrFragment(id = frag_id, offset = curr // 8, m = more) / bytes([i] * g)
            pkts.append(pkt)
            curr += gran
            i += 1

        sp.send(pkts, inter = 0.1)

class TestFrag6_Overlap(VnetTestTemplate):
    REQUIRED_MODULES = ["pf"]
    TOPOLOGY = {
        "vnet1": {"ifaces": ["if1"]},
        "vnet2": {"ifaces": ["if1"]},
        "if1": {"prefixes6": [("2001:db8::1/64", "2001:db8::2/64")]},
    }

    def vnet2_handler(self, vnet):
        ToolsHelper.print_output("/sbin/pfctl -e")
        ToolsHelper.print_output("/sbin/pfctl -x loud")
        ToolsHelper.pf_rules([
            "scrub fragment reassemble",
            "pass",
        ])

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["scapy"])
    def test_overlap(self):
        "Ensure we discard packets with overlapping fragments"

        # Import in the correct vnet, so at to not confuse Scapy
        import scapy.all as sp

        packet = sp.IPv6(src="2001:db8::1", dst="2001:db8::2") \
            / sp.ICMPv6EchoRequest(data=sp.raw(bytes.fromhex('f00f') * 90))
        frags = sp.fragment6(packet, 128)
        assert len(frags) == 3

        f = frags[0].getlayer(sp.IPv6ExtHdrFragment)
        # Fragment with overlap
        overlap = sp.IPv6(src="2001:db8::1", dst="2001:db8::2") \
            / sp.IPv6ExtHdrFragment(offset = 4, m = 1, id = f.id, nh = f.nh) \
            / sp.raw(bytes.fromhex('f00f') * 4)
        frags = [ frags[0], frags[1], overlap, frags[2] ]

        # Delay the send so the sniffer is running when we transmit.
        s = DelayedSend(frags)

        packets = sp.sniff(iface=self.vnet.iface_alias_map["if1"].name,
            timeout=3)
        for p in packets:
            p.show()
            assert not p.getlayer(sp.ICMPv6EchoReply)
