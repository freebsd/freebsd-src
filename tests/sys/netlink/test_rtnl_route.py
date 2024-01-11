import ipaddress
import socket

import pytest
from atf_python.sys.net.tools import ToolsHelper
from atf_python.sys.net.vnet import IfaceFactory
from atf_python.sys.net.vnet import SingleVnetTestTemplate
from atf_python.sys.netlink.attrs import NlAttrIp
from atf_python.sys.netlink.attrs import NlAttrU32
from atf_python.sys.netlink.base_headers import NlmBaseFlags
from atf_python.sys.netlink.base_headers import NlmGetFlags
from atf_python.sys.netlink.base_headers import NlmNewFlags
from atf_python.sys.netlink.base_headers import NlMsgType
from atf_python.sys.netlink.netlink import NetlinkTestTemplate
from atf_python.sys.netlink.netlink_route import NetlinkRtMessage
from atf_python.sys.netlink.netlink_route import NlRtMsgType
from atf_python.sys.netlink.netlink_route import RtattrType
from atf_python.sys.netlink.utils import NlConst


class TestRtNlRoute(NetlinkTestTemplate, SingleVnetTestTemplate):
    IPV6_PREFIXES = ["2001:db8::1/64"]

    def setup_method(self, method):
        super().setup_method(method)
        self.setup_netlink(NlConst.NETLINK_ROUTE)

    @pytest.mark.timeout(5)
    def test_add_route6_ll_gw(self):
        epair_ifname = self.vnet.iface_alias_map["if1"].name
        epair_ifindex = socket.if_nametoindex(epair_ifname)

        msg = NetlinkRtMessage(self.helper, NlRtMsgType.RTM_NEWROUTE)
        msg.set_request()
        msg.add_nlflags([NlmNewFlags.NLM_F_CREATE])
        msg.base_hdr.rtm_family = socket.AF_INET6
        msg.base_hdr.rtm_dst_len = 64
        msg.add_nla(NlAttrIp(RtattrType.RTA_DST, "2001:db8:2::"))
        msg.add_nla(NlAttrIp(RtattrType.RTA_GATEWAY, "fe80::1"))
        msg.add_nla(NlAttrU32(RtattrType.RTA_OIF, epair_ifindex))

        rx_msg = self.get_reply(msg)
        assert rx_msg.is_type(NlMsgType.NLMSG_ERROR)
        assert rx_msg.error_code == 0

        ToolsHelper.print_net_debug()
        ToolsHelper.print_output("netstat -6onW")

    @pytest.mark.timeout(5)
    def test_add_route6_ll_if_gw(self):
        self.require_module("if_tun")
        tun_ifname = IfaceFactory().create_iface("", "tun")[0].name
        tun_ifindex = socket.if_nametoindex(tun_ifname)

        msg = NetlinkRtMessage(self.helper, NlRtMsgType.RTM_NEWROUTE)
        msg.set_request()
        msg.add_nlflags([NlmNewFlags.NLM_F_CREATE])
        msg.base_hdr.rtm_family = socket.AF_INET6
        msg.base_hdr.rtm_dst_len = 64
        msg.add_nla(NlAttrIp(RtattrType.RTA_DST, "2001:db8:2::"))
        msg.add_nla(NlAttrU32(RtattrType.RTA_OIF, tun_ifindex))

        rx_msg = self.get_reply(msg)
        assert rx_msg.is_type(NlMsgType.NLMSG_ERROR)
        assert rx_msg.error_code == 0

        ToolsHelper.print_net_debug()
        ToolsHelper.print_output("netstat -6onW")

    @pytest.mark.timeout(5)
    def test_add_route4_ll_if_gw(self):
        self.require_module("if_tun")
        tun_ifname = IfaceFactory().create_iface("", "tun")[0].name
        tun_ifindex = socket.if_nametoindex(tun_ifname)

        msg = NetlinkRtMessage(self.helper, NlRtMsgType.RTM_NEWROUTE)
        msg.set_request()
        msg.add_nlflags([NlmNewFlags.NLM_F_CREATE])
        msg.base_hdr.rtm_family = socket.AF_INET
        msg.base_hdr.rtm_dst_len = 32
        msg.add_nla(NlAttrIp(RtattrType.RTA_DST, "192.0.2.1"))
        msg.add_nla(NlAttrU32(RtattrType.RTA_OIF, tun_ifindex))

        rx_msg = self.get_reply(msg)
        assert rx_msg.is_type(NlMsgType.NLMSG_ERROR)
        assert rx_msg.error_code == 0

        ToolsHelper.print_net_debug()
        ToolsHelper.print_output("netstat -4onW")

    @pytest.mark.timeout(20)
    def test_buffer_override(self):
        msg_flags = (
            NlmBaseFlags.NLM_F_ACK.value
            | NlmBaseFlags.NLM_F_REQUEST.value
            | NlmNewFlags.NLM_F_CREATE.value
        )

        num_routes = 1000
        base_address = bytearray(ipaddress.ip_address("2001:db8:ffff::").packed)
        for i in range(num_routes):
            base_address[7] = i % 256
            base_address[6] = i // 256
            prefix_address = ipaddress.IPv6Address(bytes(base_address))

            msg = NetlinkRtMessage(self.helper, NlRtMsgType.RTM_NEWROUTE.value)
            msg.nl_hdr.nlmsg_flags = msg_flags
            msg.base_hdr.rtm_family = socket.AF_INET6
            msg.base_hdr.rtm_dst_len = 65
            msg.add_nla(NlAttrIp(RtattrType.RTA_DST, str(prefix_address)))
            msg.add_nla(NlAttrIp(RtattrType.RTA_GATEWAY, "2001:db8::2"))

            self.write_message(msg, silent=True)
            rx_msg = self.read_message(silent=True)
            assert rx_msg.is_type(NlMsgType.NLMSG_ERROR)
            assert msg.nl_hdr.nlmsg_seq == rx_msg.nl_hdr.nlmsg_seq
            assert rx_msg.error_code == 0
        # Now, dump
        msg = NetlinkRtMessage(self.helper, NlRtMsgType.RTM_GETROUTE.value)
        msg.nl_hdr.nlmsg_flags = (
            NlmBaseFlags.NLM_F_ACK.value
            | NlmBaseFlags.NLM_F_REQUEST.value
            | NlmGetFlags.NLM_F_ROOT.value
            | NlmGetFlags.NLM_F_MATCH.value
        )
        msg.base_hdr.rtm_family = socket.AF_INET6
        self.write_message(msg)
        num_received = 0
        while True:
            rx_msg = self.read_message(silent=True)
            if msg.nl_hdr.nlmsg_seq == rx_msg.nl_hdr.nlmsg_seq:
                if rx_msg.is_type(NlMsgType.NLMSG_ERROR):
                    if rx_msg.error_code != 0:
                        raise ValueError(
                            "unable to dump routes: error {}".format(rx_msg.error_code)
                        )
                if rx_msg.is_type(NlMsgType.NLMSG_DONE):
                    break
                if rx_msg.is_type(NlRtMsgType.RTM_NEWROUTE):
                    if rx_msg.base_hdr.rtm_dst_len == 65:
                        num_received += 1
        assert num_routes == num_received
