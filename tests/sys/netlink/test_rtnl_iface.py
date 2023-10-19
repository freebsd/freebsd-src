import errno
import socket

import pytest
from atf_python.sys.netlink.netlink_route import IflattrType
from atf_python.sys.netlink.netlink_route import IflinkInfo
from atf_python.sys.netlink.netlink_route import IfLinkInfoDataVlan
from atf_python.sys.netlink.netlink_route import NetlinkIflaMessage
from atf_python.sys.netlink.netlink import NetlinkTestTemplate
from atf_python.sys.netlink.attrs import NlAttrNested
from atf_python.sys.netlink.attrs import NlAttrStr
from atf_python.sys.netlink.attrs import NlAttrStrn
from atf_python.sys.netlink.attrs import NlAttrU16
from atf_python.sys.netlink.attrs import NlAttrU32
from atf_python.sys.netlink.utils import NlConst
from atf_python.sys.netlink.base_headers import NlmBaseFlags
from atf_python.sys.netlink.base_headers import NlmNewFlags
from atf_python.sys.netlink.base_headers import NlMsgType
from atf_python.sys.netlink.netlink_route import NlRtMsgType
from atf_python.sys.netlink.netlink_route import rtnl_ifla_attrs
from atf_python.sys.net.vnet import SingleVnetTestTemplate
from atf_python.sys.net.tools import ToolsHelper


class TestRtNlIface(NetlinkTestTemplate, SingleVnetTestTemplate):
    def setup_method(self, method):
        super().setup_method(method)
        self.setup_netlink(NlConst.NETLINK_ROUTE)

    def get_interface_byname(self, ifname):
        msg = NetlinkIflaMessage(self.helper, NlRtMsgType.RTM_GETLINK.value)
        msg.nl_hdr.nlmsg_flags = (
            NlmBaseFlags.NLM_F_ACK.value | NlmBaseFlags.NLM_F_REQUEST.value
        )
        msg.add_nla(NlAttrStr(IflattrType.IFLA_IFNAME, ifname))
        self.write_message(msg)
        while True:
            rx_msg = self.read_message()
            if msg.nl_hdr.nlmsg_seq == rx_msg.nl_hdr.nlmsg_seq:
                if rx_msg.is_type(NlMsgType.NLMSG_ERROR):
                    if rx_msg.error_code != 0:
                        raise ValueError("unable to get interface {}".format(ifname))
                elif rx_msg.is_type(NlRtMsgType.RTM_NEWLINK):
                    return rx_msg
                else:
                    raise ValueError("bad message")

    def test_get_iface_byname_error(self):
        """Tests error on fetching non-existing interface name"""
        msg = NetlinkIflaMessage(self.helper, NlRtMsgType.RTM_GETLINK.value)
        msg.nl_hdr.nlmsg_flags = (
            NlmBaseFlags.NLM_F_ACK.value | NlmBaseFlags.NLM_F_REQUEST.value
        )
        msg.add_nla(NlAttrStr(IflattrType.IFLA_IFNAME, "lo10"))

        rx_msg = self.get_reply(msg)
        assert rx_msg.is_type(NlMsgType.NLMSG_ERROR)
        assert rx_msg.error_code == errno.ENODEV

    def test_get_iface_byindex_error(self):
        """Tests error on fetching non-existing interface index"""
        msg = NetlinkIflaMessage(self.helper, NlRtMsgType.RTM_GETLINK.value)
        msg.nl_hdr.nlmsg_flags = (
            NlmBaseFlags.NLM_F_ACK.value | NlmBaseFlags.NLM_F_REQUEST.value
        )
        msg.base_hdr.ifi_index = 2147483647

        rx_msg = self.get_reply(msg)
        assert rx_msg.is_type(NlMsgType.NLMSG_ERROR)
        assert rx_msg.error_code == errno.ENODEV

    @pytest.mark.require_user("root")
    def test_create_iface_plain(self):
        """Tests loopback creation w/o any parameters"""
        flags = NlmNewFlags.NLM_F_EXCL.value | NlmNewFlags.NLM_F_CREATE.value
        msg = NetlinkIflaMessage(self.helper, NlRtMsgType.RTM_NEWLINK.value)
        msg.nl_hdr.nlmsg_flags = (
            flags | NlmBaseFlags.NLM_F_ACK.value | NlmBaseFlags.NLM_F_REQUEST.value
        )
        msg.add_nla(NlAttrStr(IflattrType.IFLA_IFNAME, "lo10"))
        msg.add_nla(
            NlAttrNested(
                IflattrType.IFLA_LINKINFO,
                [
                    NlAttrStrn(IflinkInfo.IFLA_INFO_KIND, "lo"),
                ],
            )
        )

        rx_msg = self.get_reply(msg)
        assert rx_msg.is_type(NlMsgType.NLMSG_ERROR)
        assert rx_msg.error_code == 0

        self.get_interface_byname("lo10")

    @pytest.mark.require_user("root")
    def test_create_iface_plain_retvals(self):
        """Tests loopback creation w/o any parameters"""
        flags = NlmNewFlags.NLM_F_EXCL.value | NlmNewFlags.NLM_F_CREATE.value
        msg = NetlinkIflaMessage(self.helper, NlRtMsgType.RTM_NEWLINK.value)
        msg.nl_hdr.nlmsg_flags = (
            flags | NlmBaseFlags.NLM_F_ACK.value | NlmBaseFlags.NLM_F_REQUEST.value
        )
        msg.add_nla(NlAttrStr(IflattrType.IFLA_IFNAME, "lo10"))
        msg.add_nla(
            NlAttrNested(
                IflattrType.IFLA_LINKINFO,
                [
                    NlAttrStrn(IflinkInfo.IFLA_INFO_KIND, "lo"),
                ],
            )
        )

        rx_msg = self.get_reply(msg)
        assert rx_msg.is_type(NlMsgType.NLMSG_ERROR)
        assert rx_msg.error_code == 0
        assert rx_msg.cookie is not None
        nla_list, _ = rx_msg.parse_attrs(bytes(rx_msg.cookie)[4:], rtnl_ifla_attrs)
        nla_map = {n.nla_type: n for n in nla_list}
        assert IflattrType.IFLA_IFNAME.value in nla_map
        assert nla_map[IflattrType.IFLA_IFNAME.value].text == "lo10"
        assert IflattrType.IFLA_NEW_IFINDEX.value in nla_map
        assert nla_map[IflattrType.IFLA_NEW_IFINDEX.value].u32 > 0

        lo_msg = self.get_interface_byname("lo10")
        assert (
            lo_msg.base_hdr.ifi_index == nla_map[IflattrType.IFLA_NEW_IFINDEX.value].u32
        )

    @pytest.mark.require_user("root")
    def test_create_iface_attrs(self):
        """Tests interface creation with additional properties"""
        flags = NlmNewFlags.NLM_F_EXCL.value | NlmNewFlags.NLM_F_CREATE.value
        msg = NetlinkIflaMessage(self.helper, NlRtMsgType.RTM_NEWLINK.value)
        msg.nl_hdr.nlmsg_flags = (
            flags | NlmBaseFlags.NLM_F_ACK.value | NlmBaseFlags.NLM_F_REQUEST.value
        )
        msg.add_nla(NlAttrStr(IflattrType.IFLA_IFNAME, "lo10"))
        msg.add_nla(
            NlAttrNested(
                IflattrType.IFLA_LINKINFO,
                [
                    NlAttrStrn(IflinkInfo.IFLA_INFO_KIND, "lo"),
                ],
            )
        )

        # Custom attributes
        msg.add_nla(NlAttrStr(IflattrType.IFLA_IFALIAS, "test description"))
        msg.add_nla(NlAttrU32(IflattrType.IFLA_MTU, 1024))

        rx_msg = self.get_reply(msg)
        assert rx_msg.is_type(NlMsgType.NLMSG_ERROR)
        assert rx_msg.error_code == 0

        iface_msg = self.get_interface_byname("lo10")
        assert iface_msg.get_nla(IflattrType.IFLA_IFALIAS).text == "test description"
        assert iface_msg.get_nla(IflattrType.IFLA_MTU).u32 == 1024

    @pytest.mark.require_user("root")
    def test_modify_iface_attrs(self):
        """Tests interface modifications"""
        flags = NlmNewFlags.NLM_F_EXCL.value | NlmNewFlags.NLM_F_CREATE.value
        msg = NetlinkIflaMessage(self.helper, NlRtMsgType.RTM_NEWLINK.value)
        msg.nl_hdr.nlmsg_flags = (
            flags | NlmBaseFlags.NLM_F_ACK.value | NlmBaseFlags.NLM_F_REQUEST.value
        )
        msg.add_nla(NlAttrStr(IflattrType.IFLA_IFNAME, "lo10"))
        msg.add_nla(
            NlAttrNested(
                IflattrType.IFLA_LINKINFO,
                [
                    NlAttrStrn(IflinkInfo.IFLA_INFO_KIND, "lo"),
                ],
            )
        )

        rx_msg = self.get_reply(msg)
        assert rx_msg.is_type(NlMsgType.NLMSG_ERROR)
        assert rx_msg.error_code == 0

        msg = NetlinkIflaMessage(self.helper, NlRtMsgType.RTM_NEWLINK.value)
        msg.nl_hdr.nlmsg_flags = (
            NlmBaseFlags.NLM_F_ACK.value | NlmBaseFlags.NLM_F_REQUEST.value
        )
        msg.add_nla(NlAttrStr(IflattrType.IFLA_IFNAME, "lo10"))

        # Custom attributes
        msg.add_nla(NlAttrStr(IflattrType.IFLA_IFALIAS, "test description"))
        msg.add_nla(NlAttrU32(IflattrType.IFLA_MTU, 1024))

        rx_msg = self.get_reply(msg)
        assert rx_msg.is_type(NlMsgType.NLMSG_ERROR)
        assert rx_msg.error_code == 0

        iface_msg = self.get_interface_byname("lo10")
        assert iface_msg.get_nla(IflattrType.IFLA_IFALIAS).text == "test description"
        assert iface_msg.get_nla(IflattrType.IFLA_MTU).u32 == 1024

    @pytest.mark.require_user("root")
    def test_delete_iface(self):
        """Tests interface modifications"""
        flags = NlmNewFlags.NLM_F_EXCL.value | NlmNewFlags.NLM_F_CREATE.value
        msg = NetlinkIflaMessage(self.helper, NlRtMsgType.RTM_NEWLINK.value)
        msg.nl_hdr.nlmsg_flags = (
            flags | NlmBaseFlags.NLM_F_ACK.value | NlmBaseFlags.NLM_F_REQUEST.value
        )
        msg.add_nla(NlAttrStr(IflattrType.IFLA_IFNAME, "lo10"))
        msg.add_nla(
            NlAttrNested(
                IflattrType.IFLA_LINKINFO,
                [
                    NlAttrStrn(IflinkInfo.IFLA_INFO_KIND, "lo"),
                ],
            )
        )

        rx_msg = self.get_reply(msg)
        assert rx_msg.is_type(NlMsgType.NLMSG_ERROR)
        assert rx_msg.error_code == 0

        iface_msg = self.get_interface_byname("lo10")
        iface_idx = iface_msg.base_hdr.ifi_index

        msg = NetlinkIflaMessage(self.helper, NlRtMsgType.RTM_DELLINK.value)
        msg.nl_hdr.nlmsg_flags = (
            NlmBaseFlags.NLM_F_ACK.value | NlmBaseFlags.NLM_F_REQUEST.value
        )
        msg.base_hdr.ifi_index = iface_idx
        # msg.add_nla(NlAttrStr(IflattrType.IFLA_IFNAME, "lo10"))

        rx_msg = self.get_reply(msg)
        assert rx_msg.is_type(NlMsgType.NLMSG_ERROR)
        assert rx_msg.error_code == 0

        msg = NetlinkIflaMessage(self.helper, NlRtMsgType.RTM_GETLINK.value)
        msg.nl_hdr.nlmsg_flags = (
            NlmBaseFlags.NLM_F_ACK.value | NlmBaseFlags.NLM_F_REQUEST.value
        )
        msg.base_hdr.ifi_index = 2147483647

        rx_msg = self.get_reply(msg)
        assert rx_msg.is_type(NlMsgType.NLMSG_ERROR)
        assert rx_msg.error_code == errno.ENODEV

    @pytest.mark.require_user("root")
    def test_dump_ifaces_many(self):
        """Tests if interface dummp is not missing interfaces"""

        ifmap = {}
        ifmap[socket.if_nametoindex("lo0")] = "lo0"

        for i in range(40):
            ifname = "lo{}".format(i + 1)
            flags = NlmNewFlags.NLM_F_EXCL.value | NlmNewFlags.NLM_F_CREATE.value
            msg = NetlinkIflaMessage(self.helper, NlRtMsgType.RTM_NEWLINK.value)
            msg.nl_hdr.nlmsg_flags = (
                flags | NlmBaseFlags.NLM_F_ACK.value | NlmBaseFlags.NLM_F_REQUEST.value
            )
            msg.add_nla(NlAttrStr(IflattrType.IFLA_IFNAME, ifname))
            msg.add_nla(
                NlAttrNested(
                    IflattrType.IFLA_LINKINFO,
                    [
                        NlAttrStrn(IflinkInfo.IFLA_INFO_KIND, "lo"),
                    ],
                )
            )

            rx_msg = self.get_reply(msg)
            assert rx_msg.is_type(NlMsgType.NLMSG_ERROR)
            nla_list, _ = rx_msg.parse_attrs(bytes(rx_msg.cookie)[4:], rtnl_ifla_attrs)
            nla_map = {n.nla_type: n for n in nla_list}
            assert nla_map[IflattrType.IFLA_IFNAME.value].text == ifname
            ifindex = nla_map[IflattrType.IFLA_NEW_IFINDEX.value].u32
            assert ifindex > 0
            assert ifindex not in ifmap
            ifmap[ifindex] = ifname

            # Dump all interfaces and check if the output matches ifmap
            kernel_ifmap = {}
            msg = NetlinkIflaMessage(self.helper, NlRtMsgType.RTM_GETLINK.value)
            msg.nl_hdr.nlmsg_flags = (
                NlmBaseFlags.NLM_F_ACK.value | NlmBaseFlags.NLM_F_REQUEST.value
            )
            self.write_message(msg)
            while True:
                rx_msg = self.read_message()
                if msg.nl_hdr.nlmsg_seq != rx_msg.nl_hdr.nlmsg_seq:
                    raise ValueError(
                        "unexpected seq {}".format(rx_msg.nl_hdr.nlmsg_seq)
                    )
                if rx_msg.is_type(NlMsgType.NLMSG_ERROR):
                    raise ValueError("unexpected message {}".format(rx_msg))
                if rx_msg.is_type(NlMsgType.NLMSG_DONE):
                    break
                if not rx_msg.is_type(NlRtMsgType.RTM_NEWLINK):
                    raise ValueError("unexpected message {}".format(rx_msg))

                ifindex = rx_msg.base_hdr.ifi_index
                assert ifindex == rx_msg.base_hdr.ifi_index
                ifname = rx_msg.get_nla(IflattrType.IFLA_IFNAME).text
                if ifname.startswith("lo"):
                    kernel_ifmap[ifindex] = ifname
            assert kernel_ifmap == ifmap

    #
    # *
    # * {len=76, type=RTM_NEWLINK, flags=NLM_F_REQUEST|NLM_F_ACK|NLM_F_EXCL|NLM_F_CREATE, seq=1662892737, pid=0},
    # *  {ifi_family=AF_UNSPEC, ifi_type=ARPHRD_NETROM, ifi_index=0, ifi_flags=0, ifi_change=0},
    # *    {{nla_len=8, nla_type=IFLA_LINK}, 2},
    # *    {{nla_len=12, nla_type=IFLA_IFNAME}, "xvlan22"},
    # *    {{nla_len=24, nla_type=IFLA_LINKINFO},
    # *      {{nla_len=8, nla_type=IFLA_INFO_KIND}, "vlan"...},
    # *      {{nla_len=12, nla_type=IFLA_INFO_DATA}, "\x06\x00\x01\x00\x16\x00\x00\x00"}
    # */
    @pytest.mark.require_user("root")
    def test_create_vlan_plain(self):
        """Creates 802.1Q VLAN interface in vlanXX and ifX fashion"""
        self.require_module("if_vlan")
        os_ifname = self.vnet.iface_alias_map["if1"].name
        ifindex = socket.if_nametoindex(os_ifname)

        flags = NlmNewFlags.NLM_F_EXCL.value | NlmNewFlags.NLM_F_CREATE.value
        msg = NetlinkIflaMessage(self.helper, NlRtMsgType.RTM_NEWLINK.value)
        msg.nl_hdr.nlmsg_flags = (
            flags | NlmBaseFlags.NLM_F_ACK.value | NlmBaseFlags.NLM_F_REQUEST.value
        )
        msg.base_hdr.ifi_index = ifindex

        msg.add_nla(NlAttrU32(IflattrType.IFLA_LINK, ifindex))
        msg.add_nla(NlAttrStr(IflattrType.IFLA_IFNAME, "vlan22"))

        msg.add_nla(
            NlAttrNested(
                IflattrType.IFLA_LINKINFO,
                [
                    NlAttrStrn(IflinkInfo.IFLA_INFO_KIND, "vlan"),
                    NlAttrNested(
                        IflinkInfo.IFLA_INFO_DATA,
                        [
                            NlAttrU16(IfLinkInfoDataVlan.IFLA_VLAN_ID, 22),
                        ],
                    ),
                ],
            )
        )

        rx_msg = self.get_reply(msg)
        assert rx_msg.is_type(NlMsgType.NLMSG_ERROR)
        assert rx_msg.error_code == 0

        ToolsHelper.print_net_debug()
        self.get_interface_byname("vlan22")
        # ToolsHelper.print_net_debug()
