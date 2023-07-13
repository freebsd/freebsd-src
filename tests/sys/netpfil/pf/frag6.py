import pytest
import logging
import threading
import time
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
    REQUIRED_MODULES = ["pf"]
    TOPOLOGY = {
        "vnet1": {"ifaces": ["if1"]},
        "vnet2": {"ifaces": ["if1"]},
        "if1": {"prefixes6": [("2001:db8::1/64", "2001:db8::2/64")]},
    }

    def vnet2_handler(self, vnet):
        ToolsHelper.print_output("/sbin/pfctl -e")
        ToolsHelper.pf_rules([
            "scrub fragment reassemble",
            "pass",
            "block in inet6 proto icmp6 icmp6-type echoreq",
        ])

    def check_ping_reply(self, packet):
        print(packet)
        return False

    @pytest.mark.require_user("root")
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
