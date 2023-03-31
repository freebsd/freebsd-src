import socket

import pytest
from atf_python.sys.net.vnet import SingleVnetTestTemplate
from atf_python.sys.netlink.netlink import NetlinkTestTemplate
from atf_python.sys.netlink.netlink_route import NdAttrType
from atf_python.sys.netlink.netlink_route import NetlinkNdMessage
from atf_python.sys.netlink.netlink_route import NlRtMsgType
from atf_python.sys.netlink.utils import NlConst


class TestRtNlNeigh(NetlinkTestTemplate, SingleVnetTestTemplate):
    def setup_method(self, method):
        method_name = method.__name__
        if "4" in method_name:
            self.IPV4_PREFIXES = ["192.0.2.1/24"]
        if "6" in method_name:
            self.IPV6_PREFIXES = ["2001:db8::1/64"]
        super().setup_method(method)
        self.setup_netlink(NlConst.NETLINK_ROUTE)

    def filter_iface(self, family, num_items):
        epair_ifname = self.vnet.iface_alias_map["if1"].name
        epair_ifindex = socket.if_nametoindex(epair_ifname)

        msg = NetlinkNdMessage(self.helper, NlRtMsgType.RTM_GETNEIGH)
        msg.set_request()
        msg.base_hdr.ndm_family = family
        msg.base_hdr.ndm_ifindex = epair_ifindex
        self.write_message(msg)

        ret = []
        for rx_msg in self.read_msg_list(
            msg.nl_hdr.nlmsg_seq, NlRtMsgType.RTM_NEWNEIGH
        ):
            ifname = socket.if_indextoname(rx_msg.base_hdr.ndm_ifindex)
            family = rx_msg.base_hdr.ndm_family
            assert ifname == epair_ifname
            assert family == family
            assert rx_msg.get_nla(NdAttrType.NDA_DST) is not None
            assert rx_msg.get_nla(NdAttrType.NDA_LLADDR) is not None
            ret.append(rx_msg)
        assert len(ret) == num_items

    @pytest.mark.timeout(5)
    def test_6_filter_iface(self):
        """Tests that listing outputs all nd6 records"""
        return self.filter_iface(socket.AF_INET6, 2)

    @pytest.mark.timeout(5)
    def test_4_filter_iface(self):
        """Tests that listing outputs all arp records"""
        return self.filter_iface(socket.AF_INET, 1)
