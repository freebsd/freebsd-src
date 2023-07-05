import ipaddress
import socket
import struct

import pytest
from atf_python.sys.net.vnet import SingleVnetTestTemplate
from atf_python.sys.netlink.attrs import NlAttr
from atf_python.sys.netlink.attrs import NlAttrIp
from atf_python.sys.netlink.attrs import NlAttrNested
from atf_python.sys.netlink.attrs import NlAttrU32
from atf_python.sys.netlink.base_headers import NlmBaseFlags
from atf_python.sys.netlink.base_headers import NlmNewFlags
from atf_python.sys.netlink.base_headers import Nlmsghdr
from atf_python.sys.netlink.message import NlMsgType
from atf_python.sys.netlink.netlink import NetlinkTestTemplate
from atf_python.sys.netlink.netlink import Nlsock
from atf_python.sys.netlink.netlink_generic import CarpAttrType
from atf_python.sys.netlink.netlink_generic import CarpGenMessage
from atf_python.sys.netlink.netlink_generic import CarpMsgType
from atf_python.sys.netlink.netlink_route import IfaAttrType
from atf_python.sys.netlink.netlink_route import IfaCacheInfo
from atf_python.sys.netlink.netlink_route import IfafAttrType
from atf_python.sys.netlink.netlink_route import IfafFlags6
from atf_python.sys.netlink.netlink_route import IfaFlags
from atf_python.sys.netlink.netlink_route import NetlinkIfaMessage
from atf_python.sys.netlink.netlink_route import NlRtMsgType
from atf_python.sys.netlink.netlink_route import RtScope
from atf_python.sys.netlink.utils import enum_or_int
from atf_python.sys.netlink.utils import NlConst


class TestRtNlIfaddrList(NetlinkTestTemplate, SingleVnetTestTemplate):
    def setup_method(self, method):
        method_name = method.__name__
        if "4" in method_name:
            self.IPV4_PREFIXES = ["192.0.2.1/24"]
        if "6" in method_name:
            self.IPV6_PREFIXES = ["2001:db8::1/64"]
        super().setup_method(method)
        self.setup_netlink(NlConst.NETLINK_ROUTE)

    def test_46_nofilter(self):
        """Tests that listing outputs both IPv4/IPv6 and interfaces"""
        msg = NetlinkIfaMessage(self.helper, NlRtMsgType.RTM_GETADDR.value)
        msg.set_request()
        self.write_message(msg)

        ret = []
        for rx_msg in self.read_msg_list(msg.nl_hdr.nlmsg_seq, NlRtMsgType.RTM_NEWADDR):
            ifname = socket.if_indextoname(rx_msg.base_hdr.ifa_index)
            family = rx_msg.base_hdr.ifa_family
            ret.append((ifname, family, rx_msg))

        ifname = "lo0"
        assert len([r for r in ret if r[0] == ifname]) > 0

        ifname = self.vnet.iface_alias_map["if1"].name
        assert len([r for r in ret if r[0] == ifname and r[1] == socket.AF_INET]) == 1
        assert len([r for r in ret if r[0] == ifname and r[1] == socket.AF_INET6]) == 2

    def test_46_filter_iface(self):
        """Tests that listing outputs both IPv4/IPv6 for the specific interface"""
        epair_ifname = self.vnet.iface_alias_map["if1"].name

        msg = NetlinkIfaMessage(self.helper, NlRtMsgType.RTM_GETADDR.value)
        msg.set_request()
        msg.base_hdr.ifa_index = socket.if_nametoindex(epair_ifname)
        self.write_message(msg)

        ret = []
        for rx_msg in self.read_msg_list(msg.nl_hdr.nlmsg_seq, NlRtMsgType.RTM_NEWADDR):
            ifname = socket.if_indextoname(rx_msg.base_hdr.ifa_index)
            family = rx_msg.base_hdr.ifa_family
            ret.append((ifname, family, rx_msg))

        ifname = epair_ifname
        assert len([r for r in ret if r[0] == ifname and r[1] == socket.AF_INET]) == 1
        assert len([r for r in ret if r[0] == ifname and r[1] == socket.AF_INET6]) == 2
        assert len(ret) == 3

    def test_46_filter_family_compat(self):
        """Tests that family filtering works with the stripped header"""

        hdr = Nlmsghdr(
            nlmsg_len=17,
            nlmsg_type=NlRtMsgType.RTM_GETADDR.value,
            nlmsg_flags=NlmBaseFlags.NLM_F_ACK.value | NlmBaseFlags.NLM_F_REQUEST.value,
            nlmsg_seq=self.helper.get_seq(),
        )
        data = bytes(hdr) + struct.pack("@B", socket.AF_INET)
        self.nlsock.write_data(data)

        ret = []
        for rx_msg in self.read_msg_list(hdr.nlmsg_seq, NlRtMsgType.RTM_NEWADDR):
            ifname = socket.if_indextoname(rx_msg.base_hdr.ifa_index)
            family = rx_msg.base_hdr.ifa_family
            ret.append((ifname, family, rx_msg))
        assert len(ret) == 2

    def filter_iface_family(self, family, num_items):
        """Tests that listing outputs IPv4 for the specific interface"""
        epair_ifname = self.vnet.iface_alias_map["if1"].name

        msg = NetlinkIfaMessage(self.helper, NlRtMsgType.RTM_GETADDR.value)
        msg.set_request()
        msg.base_hdr.ifa_family = family
        msg.base_hdr.ifa_index = socket.if_nametoindex(epair_ifname)
        self.write_message(msg)

        ret = []
        for rx_msg in self.read_msg_list(msg.nl_hdr.nlmsg_seq, NlRtMsgType.RTM_NEWADDR):
            assert family == rx_msg.base_hdr.ifa_family
            assert epair_ifname == socket.if_indextoname(rx_msg.base_hdr.ifa_index)
            ret.append(rx_msg)
        assert len(ret) == num_items
        return ret

    def test_4_broadcast(self):
        """Tests header/attr output for listing IPv4 ifas on broadcast iface"""
        ret = self.filter_iface_family(socket.AF_INET, 1)
        # Should be 192.0.2.1/24
        msg = ret[0]
        # Family and ifindex has been checked already
        assert msg.base_hdr.ifa_prefixlen == 24
        # Ignore IFA_FLAGS for now
        assert msg.base_hdr.ifa_scope == RtScope.RT_SCOPE_UNIVERSE.value

        assert msg.get_nla(IfaAttrType.IFA_ADDRESS).addr == "192.0.2.1"
        assert msg.get_nla(IfaAttrType.IFA_LOCAL).addr == "192.0.2.1"
        assert msg.get_nla(IfaAttrType.IFA_BROADCAST).addr == "192.0.2.255"

        epair_ifname = self.vnet.iface_alias_map["if1"].name
        assert msg.get_nla(IfaAttrType.IFA_LABEL).text == epair_ifname

    def test_6_broadcast(self):
        """Tests header/attr output for listing IPv6 ifas on broadcast iface"""
        ret = self.filter_iface_family(socket.AF_INET6, 2)
        # Should be 192.0.2.1/24
        if ret[0].base_hdr.ifa_scope == RtScope.RT_SCOPE_UNIVERSE.value:
            (gmsg, lmsg) = ret
        else:
            (lmsg, gmsg) = ret
        # Start with global ( 2001:db8::1/64 )
        msg = gmsg
        # Family and ifindex has been checked already
        assert msg.base_hdr.ifa_prefixlen == 64
        # Ignore IFA_FLAGS for now
        assert msg.base_hdr.ifa_scope == RtScope.RT_SCOPE_UNIVERSE.value

        assert msg.get_nla(IfaAttrType.IFA_ADDRESS).addr == "2001:db8::1"
        assert msg.get_nla(IfaAttrType.IFA_LOCAL) is None
        assert msg.get_nla(IfaAttrType.IFA_BROADCAST) is None

        epair_ifname = self.vnet.iface_alias_map["if1"].name
        assert msg.get_nla(IfaAttrType.IFA_LABEL).text == epair_ifname

        # Local: fe80::/64
        msg = lmsg
        assert msg.base_hdr.ifa_prefixlen == 64
        # Ignore IFA_FLAGS for now
        assert msg.base_hdr.ifa_scope == RtScope.RT_SCOPE_LINK.value

        addr = ipaddress.ip_address(msg.get_nla(IfaAttrType.IFA_ADDRESS).addr)
        assert addr.is_link_local
        # Verify that ifindex is not emmbedded
        assert struct.unpack("!H", addr.packed[2:4])[0] == 0
        assert msg.get_nla(IfaAttrType.IFA_LOCAL) is None
        assert msg.get_nla(IfaAttrType.IFA_BROADCAST) is None

        epair_ifname = self.vnet.iface_alias_map["if1"].name
        assert msg.get_nla(IfaAttrType.IFA_LABEL).text == epair_ifname


class RtnlIfaOps(NetlinkTestTemplate, SingleVnetTestTemplate):
    def setup_method(self, method):
        super().setup_method(method)
        self.setup_netlink(NlConst.NETLINK_ROUTE)

    def send_check_success(self, msg):
        rx_msg = self.get_reply(msg)
        assert rx_msg.is_type(NlMsgType.NLMSG_ERROR)
        assert rx_msg.error_code == 0

    @staticmethod
    def get_family_from_ip(ip):
        if ip.version == 4:
            return socket.AF_INET
        return socket.AF_INET6

    def create_msg(self, ifa):
        iface = self.vnet.iface_alias_map["if1"]

        msg = NetlinkIfaMessage(self.helper, NlRtMsgType.RTM_NEWADDR.value)
        msg.set_request()
        msg.nl_hdr.nlmsg_flags |= (
            NlmNewFlags.NLM_F_EXCL.value | NlmNewFlags.NLM_F_CREATE.value
        )
        msg.base_hdr.ifa_family = self.get_family_from_ip(ifa.ip)
        msg.base_hdr.ifa_index = iface.ifindex
        msg.base_hdr.ifa_prefixlen = ifa.network.prefixlen
        return msg

    def get_ifa_list(self, ifindex=0, family=0):
        msg = NetlinkIfaMessage(self.helper, NlRtMsgType.RTM_GETADDR.value)
        msg.set_request()
        msg.base_hdr.ifa_family = family
        msg.base_hdr.ifa_index = ifindex
        self.write_message(msg)
        return self.read_msg_list(msg.nl_hdr.nlmsg_seq, NlRtMsgType.RTM_NEWADDR)

    def find_msg_by_ifa(self, msg_list, ip):
        for msg in msg_list:
            if msg.get_nla(IfaAttrType.IFA_ADDRESS).addr == str(ip):
                return msg
        return None

    def setup_dummy_carp(self, ifindex: int, vhid: int):
        self.require_module("carp")

        nlsock = Nlsock(NlConst.NETLINK_GENERIC, self.helper)
        family_id = nlsock.get_genl_family_id("carp")

        msg = CarpGenMessage(self.helper, family_id, CarpMsgType.CARP_NL_CMD_SET)
        msg.set_request()
        msg.add_nla(NlAttrU32(CarpAttrType.CARP_NL_VHID, vhid))
        msg.add_nla(NlAttrU32(CarpAttrType.CARP_NL_IFINDEX, ifindex))
        rx_msg = nlsock.get_reply(msg)

        assert rx_msg.is_type(NlMsgType.NLMSG_ERROR)
        assert rx_msg.error_code == 0


class TestRtNlIfaddrOpsBroadcast(RtnlIfaOps):
    def test_add_4(self):
        """Tests IPv4 address addition to the standard broadcast interface"""
        ifa = ipaddress.ip_interface("192.0.2.1/24")
        ifa_brd = ifa.network.broadcast_address
        iface = self.vnet.iface_alias_map["if1"]

        msg = self.create_msg(ifa)
        msg.add_nla(NlAttrIp(IfaAttrType.IFA_LOCAL, str(ifa.ip)))
        msg.add_nla(NlAttrIp(IfaAttrType.IFA_BROADCAST, str(ifa_brd)))

        self.send_check_success(msg)

        lst = self.get_ifa_list(iface.ifindex, self.get_family_from_ip(ifa.ip))
        assert len(lst) == 1
        rx_msg = lst[0]

        assert rx_msg.base_hdr.ifa_prefixlen == ifa.network.prefixlen
        assert rx_msg.base_hdr.ifa_scope == RtScope.RT_SCOPE_UNIVERSE.value

        assert rx_msg.get_nla(IfaAttrType.IFA_ADDRESS).addr == str(ifa.ip)
        assert rx_msg.get_nla(IfaAttrType.IFA_LOCAL).addr == str(ifa.ip)
        assert rx_msg.get_nla(IfaAttrType.IFA_BROADCAST).addr == str(ifa_brd)

    @pytest.mark.parametrize(
        "brd",
        [
            pytest.param((32, True, "192.0.2.1"), id="auto_32"),
            pytest.param((31, True, "255.255.255.255"), id="auto_31"),
            pytest.param((30, True, "192.0.2.3"), id="auto_30"),
            pytest.param((30, False, "192.0.2.2"), id="custom_30"),
            pytest.param((24, False, "192.0.2.7"), id="custom_24"),
        ],
    )
    def test_add_4_brd(self, brd):
        """Tests proper broadcast setup when adding IPv4 ifa"""
        plen, auto_brd, ifa_brd_str = brd
        ifa = ipaddress.ip_interface("192.0.2.1/{}".format(plen))
        iface = self.vnet.iface_alias_map["if1"]
        ifa_brd = ipaddress.ip_address(ifa_brd_str)

        msg = self.create_msg(ifa)
        msg.add_nla(NlAttrIp(IfaAttrType.IFA_LOCAL, str(ifa.ip)))
        if not auto_brd:
            msg.add_nla(NlAttrIp(IfaAttrType.IFA_BROADCAST, str(ifa_brd)))

        self.send_check_success(msg)

        lst = self.get_ifa_list(iface.ifindex, self.get_family_from_ip(ifa.ip))
        assert len(lst) == 1
        rx_msg = lst[0]

        assert rx_msg.base_hdr.ifa_prefixlen == ifa.network.prefixlen
        assert rx_msg.base_hdr.ifa_scope == RtScope.RT_SCOPE_UNIVERSE.value

        assert rx_msg.get_nla(IfaAttrType.IFA_ADDRESS).addr == str(ifa.ip)
        assert rx_msg.get_nla(IfaAttrType.IFA_LOCAL).addr == str(ifa.ip)
        assert rx_msg.get_nla(IfaAttrType.IFA_BROADCAST).addr == str(ifa_brd)

    def test_add_6(self):
        ifa = ipaddress.ip_interface("2001:db8::1/64")
        iface = self.vnet.iface_alias_map["if1"]

        msg = self.create_msg(ifa)
        msg.add_nla(NlAttrIp(IfaAttrType.IFA_LOCAL, str(ifa.ip)))

        self.send_check_success(msg)

        lst = self.get_ifa_list(iface.ifindex, self.get_family_from_ip(ifa.ip))
        assert len(lst) == 2
        rx_msg_gu = self.find_msg_by_ifa(lst, ifa.ip)
        assert rx_msg_gu is not None

        assert rx_msg_gu.base_hdr.ifa_prefixlen == ifa.network.prefixlen
        assert rx_msg_gu.base_hdr.ifa_scope == RtScope.RT_SCOPE_UNIVERSE.value
        assert rx_msg_gu.get_nla(IfaAttrType.IFA_ADDRESS).addr == str(ifa.ip)

    def test_add_4_carp(self):
        ifa = ipaddress.ip_interface("192.0.2.1/24")
        ifa_brd = ifa.network.broadcast_address
        iface = self.vnet.iface_alias_map["if1"]
        vhid = 77

        self.setup_dummy_carp(iface.ifindex, vhid)

        msg = self.create_msg(ifa)
        msg.add_nla(NlAttrIp(IfaAttrType.IFA_LOCAL, str(ifa.ip)))
        msg.add_nla(NlAttrIp(IfaAttrType.IFA_BROADCAST, str(ifa_brd)))
        attrs_bsd = [NlAttrU32(IfafAttrType.IFAF_VHID, vhid)]
        msg.add_nla(NlAttrNested(IfaAttrType.IFA_FREEBSD, attrs_bsd))

        self.send_check_success(msg)

        lst = self.get_ifa_list(iface.ifindex, self.get_family_from_ip(ifa.ip))
        assert len(lst) == 1
        rx_msg = lst[0]

        assert rx_msg.base_hdr.ifa_prefixlen == ifa.network.prefixlen
        assert rx_msg.base_hdr.ifa_scope == RtScope.RT_SCOPE_UNIVERSE.value

        assert rx_msg.get_nla(IfaAttrType.IFA_ADDRESS).addr == str(ifa.ip)
        assert rx_msg.get_nla(IfaAttrType.IFA_LOCAL).addr == str(ifa.ip)
        assert rx_msg.get_nla(IfaAttrType.IFA_BROADCAST).addr == str(ifa_brd)
        ifa_bsd = rx_msg.get_nla(IfaAttrType.IFA_FREEBSD)
        assert ifa_bsd.get_nla(IfafAttrType.IFAF_VHID).u32 == vhid

    def test_add_6_carp(self):
        ifa = ipaddress.ip_interface("2001:db8::1/64")
        iface = self.vnet.iface_alias_map["if1"]
        vhid = 77

        self.setup_dummy_carp(iface.ifindex, vhid)

        msg = self.create_msg(ifa)
        msg.add_nla(NlAttrIp(IfaAttrType.IFA_LOCAL, str(ifa.ip)))
        attrs_bsd = [NlAttrU32(IfafAttrType.IFAF_VHID, vhid)]
        msg.add_nla(NlAttrNested(IfaAttrType.IFA_FREEBSD, attrs_bsd))

        self.send_check_success(msg)

        lst = self.get_ifa_list(iface.ifindex, self.get_family_from_ip(ifa.ip))
        assert len(lst) == 2
        rx_msg_gu = self.find_msg_by_ifa(lst, ifa.ip)
        assert rx_msg_gu is not None

        assert rx_msg_gu.base_hdr.ifa_prefixlen == ifa.network.prefixlen
        assert rx_msg_gu.base_hdr.ifa_scope == RtScope.RT_SCOPE_UNIVERSE.value
        assert rx_msg_gu.get_nla(IfaAttrType.IFA_ADDRESS).addr == str(ifa.ip)
        ifa_bsd = rx_msg_gu.get_nla(IfaAttrType.IFA_FREEBSD)
        assert ifa_bsd.get_nla(IfafAttrType.IFAF_VHID).u32 == vhid

    def test_add_6_lifetime(self):
        ifa = ipaddress.ip_interface("2001:db8::1/64")
        iface = self.vnet.iface_alias_map["if1"]
        pref_time = 43200
        valid_time = 86400

        ci = IfaCacheInfo(ifa_prefered=pref_time, ifa_valid=valid_time)

        msg = self.create_msg(ifa)
        msg.add_nla(NlAttrIp(IfaAttrType.IFA_LOCAL, str(ifa.ip)))
        msg.add_nla(NlAttr(IfaAttrType.IFA_CACHEINFO, bytes(ci)))

        self.send_check_success(msg)

        lst = self.get_ifa_list(iface.ifindex, self.get_family_from_ip(ifa.ip))
        assert len(lst) == 2
        rx_msg = self.find_msg_by_ifa(lst, ifa.ip)
        assert rx_msg is not None

        ci = rx_msg.get_nla(IfaAttrType.IFA_CACHEINFO).ci
        assert pref_time - 5 <= ci.ifa_prefered <= pref_time
        assert valid_time - 5 <= ci.ifa_valid <= valid_time
        assert ci.cstamp > 0
        assert ci.tstamp > 0
        assert ci.tstamp >= ci.cstamp

    @pytest.mark.parametrize(
        "flags_str",
        [
            "autoconf",
            "deprecated",
            "autoconf,deprecated",
            "prefer_source",
        ],
    )
    def test_add_6_flags(self, flags_str):
        ifa = ipaddress.ip_interface("2001:db8::1/64")
        iface = self.vnet.iface_alias_map["if1"]

        flags_map = {
            "autoconf": {"nl": 0, "f": IfafFlags6.IN6_IFF_AUTOCONF},
            "deprecated": {
                "nl": IfaFlags.IFA_F_DEPRECATED,
                "f": IfafFlags6.IN6_IFF_DEPRECATED,
            },
            "prefer_source": {"nl": 0, "f": IfafFlags6.IN6_IFF_PREFER_SOURCE},
        }
        nl_flags = 0
        f_flags = 0

        for flag_str in flags_str.split(","):
            d = flags_map.get(flag_str, {})
            nl_flags |= enum_or_int(d.get("nl", 0))
            f_flags |= enum_or_int(d.get("f", 0))

        msg = self.create_msg(ifa)
        msg.add_nla(NlAttrIp(IfaAttrType.IFA_LOCAL, str(ifa.ip)))
        msg.add_nla(NlAttrU32(IfaAttrType.IFA_FLAGS, nl_flags))
        attrs_bsd = [NlAttrU32(IfafAttrType.IFAF_FLAGS, f_flags)]
        msg.add_nla(NlAttrNested(IfaAttrType.IFA_FREEBSD, attrs_bsd))

        self.send_check_success(msg)

        lst = self.get_ifa_list(iface.ifindex, self.get_family_from_ip(ifa.ip))
        assert len(lst) == 2
        rx_msg = self.find_msg_by_ifa(lst, ifa.ip)
        assert rx_msg is not None

        assert rx_msg.get_nla(IfaAttrType.IFA_FLAGS).u32 == nl_flags
        ifa_bsd = rx_msg.get_nla(IfaAttrType.IFA_FREEBSD)
        assert ifa_bsd.get_nla(IfafAttrType.IFAF_FLAGS).u32 == f_flags

    def test_add_4_empty_message(self):
        """Tests correct failure w/ empty message"""
        msg = NetlinkIfaMessage(self.helper, NlRtMsgType.RTM_NEWADDR.value)
        msg.set_request()
        msg.nl_hdr.nlmsg_flags |= (
            NlmNewFlags.NLM_F_EXCL.value | NlmNewFlags.NLM_F_CREATE.value
        )

        rx_msg = self.get_reply(msg)
        assert rx_msg.is_type(NlMsgType.NLMSG_ERROR)
        assert rx_msg.error_code != 0

    def test_add_4_empty_ifindex(self):
        """Tests correct failure w/ empty ifindex"""
        ifa = ipaddress.ip_interface("192.0.2.1/24")
        ifa_brd = ifa.network.broadcast_address

        msg = self.create_msg(ifa)
        msg.base_hdr.ifa_index = 0
        msg.add_nla(NlAttrIp(IfaAttrType.IFA_LOCAL, str(ifa.ip)))
        msg.add_nla(NlAttrIp(IfaAttrType.IFA_BROADCAST, str(ifa_brd)))

        rx_msg = self.get_reply(msg)
        assert rx_msg.is_type(NlMsgType.NLMSG_ERROR)
        assert rx_msg.error_code != 0

    def test_add_4_empty_addr(self):
        """Tests correct failure w/ empty address"""
        ifa = ipaddress.ip_interface("192.0.2.1/24")
        ifa_brd = ifa.network.broadcast_address

        msg = self.create_msg(ifa)
        msg.add_nla(NlAttrIp(IfaAttrType.IFA_BROADCAST, str(ifa_brd)))

        rx_msg = self.get_reply(msg)
        assert rx_msg.is_type(NlMsgType.NLMSG_ERROR)
        assert rx_msg.error_code != 0

    @pytest.mark.parametrize(
        "ifa_str",
        [
            pytest.param("192.0.2.1/32", id="ipv4_host"),
            pytest.param("192.0.2.1/24", id="ipv4_prefix"),
            pytest.param("2001:db8::1/64", id="ipv6_gu_prefix"),
            pytest.param("2001:db8::1/128", id="ipv6_gu_host"),
        ],
    )
    @pytest.mark.parametrize(
        "tlv",
        [
            pytest.param("local", id="ifa_local"),
            pytest.param("address", id="ifa_address"),
        ],
    )
    def test_del(self, tlv, ifa_str):
        """Tests address deletion from the standard broadcast interface"""
        ifa = ipaddress.ip_interface(ifa_str)
        ifa_brd = ifa.network.broadcast_address
        iface = self.vnet.iface_alias_map["if1"]

        msg = self.create_msg(ifa)
        msg.add_nla(NlAttrIp(IfaAttrType.IFA_LOCAL, str(ifa.ip)))
        msg.add_nla(NlAttrIp(IfaAttrType.IFA_BROADCAST, str(ifa_brd)))

        self.send_check_success(msg)
        lst = self.get_ifa_list(iface.ifindex, self.get_family_from_ip(ifa.ip))
        rx_msg = self.find_msg_by_ifa(lst, ifa.ip)
        assert rx_msg is not None

        msg = NetlinkIfaMessage(self.helper, NlRtMsgType.RTM_DELADDR.value)
        msg.set_request()
        msg.base_hdr.ifa_family = self.get_family_from_ip(ifa.ip)
        msg.base_hdr.ifa_index = iface.ifindex
        msg.base_hdr.ifa_prefixlen = ifa.network.prefixlen

        if tlv == "local":
            msg.add_nla(NlAttrIp(IfaAttrType.IFA_LOCAL, str(ifa.ip)))
        if tlv == "address":
            msg.add_nla(NlAttrIp(IfaAttrType.IFA_ADDRESS, str(ifa.ip)))

        self.send_check_success(msg)
        lst = self.get_ifa_list(iface.ifindex, self.get_family_from_ip(ifa.ip))
        rx_msg = self.find_msg_by_ifa(lst, ifa.ip)
        assert rx_msg is None


class TestRtNlIfaddrOpsP2p(RtnlIfaOps):
    IFTYPE = "gif"

    @pytest.mark.parametrize(
        "ifa_pair",
        [
            pytest.param(["192.0.2.1/24", "192.0.2.2"], id="dst_inside_24"),
            pytest.param(["192.0.2.1/30", "192.0.2.2"], id="dst_inside_30"),
            pytest.param(["192.0.2.1/31", "192.0.2.2"], id="dst_inside_31"),
            pytest.param(["192.0.2.1/32", "192.0.2.2"], id="dst_outside_32"),
            pytest.param(["192.0.2.1/30", "192.0.2.100"], id="dst_outside_30"),
        ],
    )
    def test_add_4(self, ifa_pair):
        """Tests IPv4 address addition to the p2p interface"""
        ifa = ipaddress.ip_interface(ifa_pair[0])
        peer_ip = ipaddress.ip_address(ifa_pair[1])
        iface = self.vnet.iface_alias_map["if1"]

        msg = self.create_msg(ifa)
        msg.add_nla(NlAttrIp(IfaAttrType.IFA_LOCAL, str(ifa.ip)))
        msg.add_nla(NlAttrIp(IfaAttrType.IFA_ADDRESS, str(peer_ip)))

        self.send_check_success(msg)

        lst = self.get_ifa_list(iface.ifindex, self.get_family_from_ip(ifa.ip))
        assert len(lst) == 1
        rx_msg = lst[0]

        assert rx_msg.base_hdr.ifa_prefixlen == ifa.network.prefixlen
        assert rx_msg.base_hdr.ifa_scope == RtScope.RT_SCOPE_UNIVERSE.value

        assert rx_msg.get_nla(IfaAttrType.IFA_LOCAL).addr == str(ifa.ip)
        assert rx_msg.get_nla(IfaAttrType.IFA_ADDRESS).addr == str(peer_ip)

    @pytest.mark.parametrize(
        "ifa_pair",
        [
            pytest.param(
                ["2001:db8::1/64", "2001:db8::2"],
                id="dst_inside_64",
                marks=pytest.mark.xfail(reason="currently fails"),
            ),
            pytest.param(
                ["2001:db8::1/127", "2001:db8::2"],
                id="dst_inside_127",
                marks=pytest.mark.xfail(reason="currently fails"),
            ),
            pytest.param(["2001:db8::1/128", "2001:db8::2"], id="dst_outside_128"),
            pytest.param(
                ["2001:db8::1/64", "2001:db8:2::2"],
                id="dst_outside_64",
                marks=pytest.mark.xfail(reason="currently fails"),
            ),
        ],
    )
    def test_add_6(self, ifa_pair):
        """Tests IPv6 address addition to the p2p interface"""
        ifa = ipaddress.ip_interface(ifa_pair[0])
        peer_ip = ipaddress.ip_address(ifa_pair[1])
        iface = self.vnet.iface_alias_map["if1"]

        msg = self.create_msg(ifa)
        msg.add_nla(NlAttrIp(IfaAttrType.IFA_LOCAL, str(ifa.ip)))
        msg.add_nla(NlAttrIp(IfaAttrType.IFA_ADDRESS, str(peer_ip)))

        self.send_check_success(msg)

        lst = self.get_ifa_list(iface.ifindex, self.get_family_from_ip(ifa.ip))
        assert len(lst) == 2
        rx_msg_gu = self.find_msg_by_ifa(lst, peer_ip)
        assert rx_msg_gu is not None

        assert rx_msg_gu.base_hdr.ifa_prefixlen == ifa.network.prefixlen
        assert rx_msg_gu.base_hdr.ifa_scope == RtScope.RT_SCOPE_UNIVERSE.value
        assert rx_msg_gu.get_nla(IfaAttrType.IFA_LOCAL).addr == str(ifa.ip)
        assert rx_msg_gu.get_nla(IfaAttrType.IFA_ADDRESS).addr == str(peer_ip)

    @pytest.mark.parametrize(
        "ifa_pair",
        [
            pytest.param(["192.0.2.1/30", "192.0.2.2"], id="ipv4_dst_inside_30"),
            pytest.param(["192.0.2.1/32", "192.0.2.2"], id="ipv4_dst_outside_32"),
            pytest.param(["2001:db8::1/128", "2001:db8::2"], id="ip6_dst_outside_128"),
        ],
    )
    @pytest.mark.parametrize(
        "tlv_pair",
        [
            pytest.param(["a", ""], id="ifa_addr=addr"),
            pytest.param(["", "a"], id="ifa_local=addr"),
            pytest.param(["a", "a"], id="ifa_addr=addr,ifa_local=addr"),
        ],
    )
    def test_del(self, tlv_pair, ifa_pair):
        """Tests address deletion from the P2P interface"""
        ifa = ipaddress.ip_interface(ifa_pair[0])
        peer_ip = ipaddress.ip_address(ifa_pair[1])
        iface = self.vnet.iface_alias_map["if1"]
        ifa_addr_str, ifa_local_str = tlv_pair

        msg = self.create_msg(ifa)
        msg.add_nla(NlAttrIp(IfaAttrType.IFA_LOCAL, str(ifa.ip)))
        msg.add_nla(NlAttrIp(IfaAttrType.IFA_ADDRESS, str(peer_ip)))

        self.send_check_success(msg)
        lst = self.get_ifa_list(iface.ifindex, self.get_family_from_ip(ifa.ip))
        rx_msg = self.find_msg_by_ifa(lst, peer_ip)
        assert rx_msg is not None

        msg = NetlinkIfaMessage(self.helper, NlRtMsgType.RTM_DELADDR.value)
        msg.set_request()
        msg.base_hdr.ifa_family = self.get_family_from_ip(ifa.ip)
        msg.base_hdr.ifa_index = iface.ifindex
        msg.base_hdr.ifa_prefixlen = ifa.network.prefixlen

        if "a" in ifa_addr_str:
            msg.add_nla(NlAttrIp(IfaAttrType.IFA_ADDRESS, str(ifa.ip)))
        if "p" in ifa_addr_str:
            msg.add_nla(NlAttrIp(IfaAttrType.IFA_ADDRESS, str(peer_ip)))
        if "a" in ifa_local_str:
            msg.add_nla(NlAttrIp(IfaAttrType.IFA_LOCAL, str(ifa.ip)))
        if "p" in ifa_local_str:
            msg.add_nla(NlAttrIp(IfaAttrType.IFA_LOCAL, str(peer_ip)))

        self.send_check_success(msg)
        lst = self.get_ifa_list(iface.ifindex, self.get_family_from_ip(ifa.ip))
        rx_msg = self.find_msg_by_ifa(lst, ifa.ip)
        assert rx_msg is None


class TestRtNlAddIfaddrLo(RtnlIfaOps):
    IFTYPE = "lo"

    @pytest.mark.parametrize(
        "ifa_str",
        [
            pytest.param("192.0.2.1/24", id="prefix"),
            pytest.param("192.0.2.1/32", id="host"),
        ],
    )
    def test_add_4(self, ifa_str):
        """Tests IPv4 address addition to the loopback interface"""
        ifa = ipaddress.ip_interface(ifa_str)
        iface = self.vnet.iface_alias_map["if1"]

        msg = self.create_msg(ifa)
        msg.add_nla(NlAttrIp(IfaAttrType.IFA_LOCAL, str(ifa.ip)))

        self.send_check_success(msg)

        lst = self.get_ifa_list(iface.ifindex, self.get_family_from_ip(ifa.ip))
        assert len(lst) == 1
        rx_msg = lst[0]

        assert rx_msg.base_hdr.ifa_prefixlen == ifa.network.prefixlen
        assert rx_msg.base_hdr.ifa_scope == RtScope.RT_SCOPE_UNIVERSE.value

        assert rx_msg.get_nla(IfaAttrType.IFA_ADDRESS).addr == str(ifa.ip)
        assert rx_msg.get_nla(IfaAttrType.IFA_LOCAL).addr == str(ifa.ip)

    @pytest.mark.parametrize(
        "ifa_str",
        [
            pytest.param("2001:db8::1/64", id="gu_prefix"),
            pytest.param("2001:db8::1/128", id="gu_host"),
        ],
    )
    def test_add_6(self, ifa_str):
        """Tests IPv6 address addition to the loopback interface"""
        ifa = ipaddress.ip_interface(ifa_str)
        iface = self.vnet.iface_alias_map["if1"]

        msg = self.create_msg(ifa)
        msg.add_nla(NlAttrIp(IfaAttrType.IFA_LOCAL, str(ifa.ip)))

        self.send_check_success(msg)

        lst = self.get_ifa_list(iface.ifindex, self.get_family_from_ip(ifa.ip))
        assert len(lst) == 2  # link-local should be auto-created as well
        rx_msg = self.find_msg_by_ifa(lst, ifa.ip)
        assert rx_msg is not None

        assert rx_msg.base_hdr.ifa_prefixlen == ifa.network.prefixlen
        assert rx_msg.base_hdr.ifa_scope == RtScope.RT_SCOPE_UNIVERSE.value
        assert rx_msg.get_nla(IfaAttrType.IFA_ADDRESS).addr == str(ifa.ip)

    @pytest.mark.parametrize(
        "ifa_str",
        [
            pytest.param("192.0.2.1/32", id="ipv4_host"),
            pytest.param("192.0.2.1/24", id="ipv4_prefix"),
            pytest.param("2001:db8::1/64", id="ipv6_gu_prefix"),
            pytest.param("2001:db8::1/128", id="ipv6_gu_host"),
        ],
    )
    @pytest.mark.parametrize(
        "tlv",
        [
            pytest.param("local", id="ifa_local"),
            pytest.param("address", id="ifa_address"),
        ],
    )
    def test_del(self, tlv, ifa_str):
        """Tests address deletion from the loopback interface"""
        ifa = ipaddress.ip_interface(ifa_str)
        iface = self.vnet.iface_alias_map["if1"]

        msg = self.create_msg(ifa)
        msg.add_nla(NlAttrIp(IfaAttrType.IFA_LOCAL, str(ifa.ip)))

        self.send_check_success(msg)
        lst = self.get_ifa_list(iface.ifindex, self.get_family_from_ip(ifa.ip))
        rx_msg = self.find_msg_by_ifa(lst, ifa.ip)
        assert rx_msg is not None

        msg = NetlinkIfaMessage(self.helper, NlRtMsgType.RTM_DELADDR.value)
        msg.set_request()
        msg.base_hdr.ifa_family = self.get_family_from_ip(ifa.ip)
        msg.base_hdr.ifa_index = iface.ifindex
        msg.base_hdr.ifa_prefixlen = ifa.network.prefixlen

        if tlv == "local":
            msg.add_nla(NlAttrIp(IfaAttrType.IFA_LOCAL, str(ifa.ip)))
        if tlv == "address":
            msg.add_nla(NlAttrIp(IfaAttrType.IFA_ADDRESS, str(ifa.ip)))

        self.send_check_success(msg)
        lst = self.get_ifa_list(iface.ifindex, self.get_family_from_ip(ifa.ip))
        rx_msg = self.find_msg_by_ifa(lst, ifa.ip)
        assert rx_msg is None
