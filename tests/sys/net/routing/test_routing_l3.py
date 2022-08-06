import ipaddress

import pytest
from atf_python.sys.net.tools import ToolsHelper
from atf_python.sys.net.vnet import VnetTestTemplate


class TestIfOps(VnetTestTemplate):
    TOPOLOGY = {
        "vnet1": {"ifaces": ["if1", "if2"]},
        "if1": {"prefixes4": [], "prefixes6": []},
        "if2": {"prefixes4": [], "prefixes6": []},
    }

    @pytest.mark.parametrize("family", ["inet", "inet6"])
    @pytest.mark.require_user("root")
    def test_change_prefix_route(self, family):
        """Tests that prefix route changes to the new one upon addr deletion"""
        vnet = self.vnet_map["vnet1"]
        first_iface = vnet.iface_alias_map["if1"]
        second_iface = vnet.iface_alias_map["if2"]
        if family == "inet":
            first_addr = ipaddress.ip_interface("192.0.2.1/24")
            second_addr = ipaddress.ip_interface("192.0.2.2/24")
        else:
            first_addr = ipaddress.ip_interface("2001:db8::1/64")
            second_addr = ipaddress.ip_interface("2001:db8::2/64")

        first_iface.setup_addr(str(first_addr))
        second_iface.setup_addr(str(second_addr))

        # At this time prefix should be pointing to the first interface
        routes = ToolsHelper.get_routes(family)
        px = [r for r in routes if r["destination"] == str(first_addr.network)][0]
        assert px["interface-name"] == first_iface.name

        # Now delete address from the first interface and verify switchover
        first_iface.delete_addr(first_addr.ip)

        routes = ToolsHelper.get_routes(family)
        px = [r for r in routes if r["destination"] == str(first_addr.network)][0]
        assert px["interface-name"] == second_iface.name

    @pytest.mark.parametrize(
        "family",
        [
            "inet",
            pytest.param("inet6", marks=pytest.mark.xfail(reason="currently fails")),
        ],
    )
    @pytest.mark.require_user("root")
    def test_change_prefix_route_same_iface(self, family):
        """Tests that prefix route changes to the new ifa upon addr deletion"""
        vnet = self.vnet_map["vnet1"]
        first_iface = vnet.iface_alias_map["if1"]

        if family == "inet":
            first_addr = ipaddress.ip_interface("192.0.2.1/24")
            second_addr = ipaddress.ip_interface("192.0.2.2/24")
        else:
            first_addr = ipaddress.ip_interface("2001:db8::1/64")
            second_addr = ipaddress.ip_interface("2001:db8::2/64")

        first_iface.setup_addr(str(first_addr))
        first_iface.setup_addr(str(second_addr))

        # At this time prefix should be pointing to the first interface
        routes = ToolsHelper.get_routes(family)
        px = [r for r in routes if r["destination"] == str(first_addr.network)][0]
        assert px["interface-name"] == first_iface.name

        # Now delete address from the first interface and verify switchover
        first_iface.delete_addr(str(first_addr.ip))

        routes = ToolsHelper.get_routes(family)
        px = [r for r in routes if r["destination"] == str(first_addr.network)][0]
        nhop_kidx = px["nhop"]
        assert px["interface-name"] == first_iface.name
        nhops = ToolsHelper.get_nhops(family)
        nh = [nh for nh in nhops if nh["index"] == nhop_kidx][0]
        assert nh["ifa"] == str(second_addr.ip)
