import pytest
from atf_python.sys.net.tools import ToolsHelper
from atf_python.sys.net.vnet import VnetTestTemplate

sc = None


def filter_f(x):
    ip = x.getlayer(sc.IP)
    if not ip:
        return False

    return ip.proto == 112


class TestCarp(VnetTestTemplate):
    REQUIRED_MODULES = ["carp"]
    TOPOLOGY = {
        "vnet1": {"ifaces": ["if1"]},
        "if1": {"prefixes4": [("192.0.2.1/24", "192.0.2.2/24")]},
    }

    def setup_method(self, method):
        global sc
        if sc is None:
            import scapy.all as _sc

            sc = _sc
        super().setup_method(method)

    @classmethod
    def check_carp_src_mac(self, pkts):
        for p in pkts:
            if not filter_f(p):
                continue

            print("Packet src mac {}".format(p.src))

            if p.src != "00:00:5e:00:01:01":
                raise

    @pytest.mark.require_progs(["scapy"])
    def test_source_mac(self):
        "Test carp packets source address"

        if1 = self.vnet.iface_alias_map["if1"]

        ToolsHelper.print_output(
            "ifconfig {} add vhid 1 192.0.2.203/24".format(if1.name)
        )

        carp_pkts = sc.sniff(iface=if1.name, stop_filter=filter_f, timeout=5)

        self.check_carp_src_mac(carp_pkts)

    @pytest.mark.require_progs(["scapy"])
    def test_source_mac_vrrp(self):
        "Test VRRP packets source address"

        if1 = self.vnet.iface_alias_map["if1"]

        ToolsHelper.print_output(
            "ifconfig {} add vhid 1 carpver 3 192.0.2.203/24".format(if1.name)
        )

        carp_pkts = sc.sniff(iface=if1.name, stop_filter=filter_f, timeout=5)

        self.check_carp_src_mac(carp_pkts)

