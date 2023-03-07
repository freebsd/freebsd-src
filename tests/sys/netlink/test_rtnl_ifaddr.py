import ipaddress
import socket
import struct

from atf_python.sys.net.netlink import IfattrType
from atf_python.sys.net.netlink import NetlinkIfaMessage
from atf_python.sys.net.netlink import NetlinkTestTemplate
from atf_python.sys.net.netlink import NlConst
from atf_python.sys.net.netlink import NlHelper
from atf_python.sys.net.netlink import NlmBaseFlags
from atf_python.sys.net.netlink import Nlmsghdr
from atf_python.sys.net.netlink import NlMsgType
from atf_python.sys.net.netlink import NlRtMsgType
from atf_python.sys.net.netlink import Nlsock
from atf_python.sys.net.netlink import RtScope
from atf_python.sys.net.vnet import SingleVnetTestTemplate


class TestRtNlIfaddr(NetlinkTestTemplate, SingleVnetTestTemplate):
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
        msg.nl_hdr.nlmsg_flags = (
            NlmBaseFlags.NLM_F_ACK.value | NlmBaseFlags.NLM_F_REQUEST.value
        )
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
        msg.nl_hdr.nlmsg_flags = (
            NlmBaseFlags.NLM_F_ACK.value | NlmBaseFlags.NLM_F_REQUEST.value
        )
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
                nlmsg_seq=self.helper.get_seq()
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
        msg.nl_hdr.nlmsg_flags = (
            NlmBaseFlags.NLM_F_ACK.value | NlmBaseFlags.NLM_F_REQUEST.value
        )
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

        assert msg.get_nla(IfattrType.IFA_ADDRESS).addr == "192.0.2.1"
        assert msg.get_nla(IfattrType.IFA_LOCAL).addr == "192.0.2.1"
        assert msg.get_nla(IfattrType.IFA_BROADCAST).addr == "192.0.2.255"

        epair_ifname = self.vnet.iface_alias_map["if1"].name
        assert msg.get_nla(IfattrType.IFA_LABEL).text == epair_ifname

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

        assert msg.get_nla(IfattrType.IFA_ADDRESS).addr == "2001:db8::1"
        assert msg.get_nla(IfattrType.IFA_LOCAL) is None
        assert msg.get_nla(IfattrType.IFA_BROADCAST) is None

        epair_ifname = self.vnet.iface_alias_map["if1"].name
        assert msg.get_nla(IfattrType.IFA_LABEL).text == epair_ifname

        # Local: fe80::/64
        msg = lmsg
        assert msg.base_hdr.ifa_prefixlen == 64
        # Ignore IFA_FLAGS for now
        assert msg.base_hdr.ifa_scope == RtScope.RT_SCOPE_LINK.value

        addr = ipaddress.ip_address(msg.get_nla(IfattrType.IFA_ADDRESS).addr)
        assert addr.is_link_local
        # Verify that ifindex is not emmbedded
        assert struct.unpack("!H", addr.packed[2:4])[0] == 0
        assert msg.get_nla(IfattrType.IFA_LOCAL) is None
        assert msg.get_nla(IfattrType.IFA_BROADCAST) is None

        epair_ifname = self.vnet.iface_alias_map["if1"].name
        assert msg.get_nla(IfattrType.IFA_LABEL).text == epair_ifname
