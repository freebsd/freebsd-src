import errno
import socket

import pytest
from atf_python.sys.net.netlink import IflattrType
from atf_python.sys.net.netlink import IflinkInfo
from atf_python.sys.net.netlink import IfLinkInfoDataVlan
from atf_python.sys.net.netlink import NetlinkIflaMessage
from atf_python.sys.net.netlink import NetlinkTestTemplate
from atf_python.sys.net.netlink import NlAttrNested
from atf_python.sys.net.netlink import NlAttrStr
from atf_python.sys.net.netlink import NlAttrStrn
from atf_python.sys.net.netlink import NlAttrU16
from atf_python.sys.net.netlink import NlAttrU32
from atf_python.sys.net.netlink import NlConst
from atf_python.sys.net.netlink import NlmBaseFlags
from atf_python.sys.net.netlink import NlmNewFlags
from atf_python.sys.net.netlink import NlMsgType
from atf_python.sys.net.netlink import NlRtMsgType
from atf_python.sys.net.vnet import SingleVnetTestTemplate


class TestRtNlIface(SingleVnetTestTemplate, NetlinkTestTemplate):
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
    @pytest.mark.skip(reason="vlan support needs more work")
    @pytest.mark.require_user("root")
    def test_create_vlan_plain(self):
        """Creates 802.1Q VLAN interface in vlanXX and ifX fashion"""
        os_ifname = self.vnet.iface_alias_map["if1"].name
        ifindex = socket.if_nametoindex(os_ifname)

        flags = NlmNewFlags.NLM_F_EXCL.value | NlmNewFlags.NLM_F_CREATE.value
        msg = NetlinkIflaMessage(self.helper, NlRtMsgType.RTM_NEWLINK.value)
        msg.nl_hdr.nlmsg_flags = (
            flags | NlmBaseFlags.NLM_F_ACK.value | NlmBaseFlags.NLM_F_REQUEST.value
        )

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

        self.get_interface_byname("vlan22")
        # ToolsHelper.print_net_debug()
