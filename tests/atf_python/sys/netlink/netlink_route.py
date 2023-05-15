import socket
import struct
from ctypes import c_int
from ctypes import c_ubyte
from ctypes import c_uint
from ctypes import c_ushort
from ctypes import sizeof
from ctypes import Structure
from enum import auto
from enum import Enum

from atf_python.sys.netlink.attrs import NlAttr
from atf_python.sys.netlink.attrs import NlAttrIp
from atf_python.sys.netlink.attrs import NlAttrNested
from atf_python.sys.netlink.attrs import NlAttrStr
from atf_python.sys.netlink.attrs import NlAttrU32
from atf_python.sys.netlink.attrs import NlAttrU8
from atf_python.sys.netlink.message import StdNetlinkMessage
from atf_python.sys.netlink.message import NlMsgProps
from atf_python.sys.netlink.message import NlMsgCategory
from atf_python.sys.netlink.utils import AttrDescr
from atf_python.sys.netlink.utils import get_bitmask_str
from atf_python.sys.netlink.utils import prepare_attrs_map


class RtattrType(Enum):
    RTA_UNSPEC = 0
    RTA_DST = 1
    RTA_SRC = 2
    RTA_IIF = 3
    RTA_OIF = 4
    RTA_GATEWAY = 5
    RTA_PRIORITY = 6
    RTA_PREFSRC = 7
    RTA_METRICS = 8
    RTA_MULTIPATH = 9
    # RTA_PROTOINFO = 10
    RTA_KNH_ID = 10
    RTA_FLOW = 11
    RTA_CACHEINFO = 12
    RTA_SESSION = 13
    # RTA_MP_ALGO = 14
    RTA_RTFLAGS = 14
    RTA_TABLE = 15
    RTA_MARK = 16
    RTA_MFC_STATS = 17
    RTA_VIA = 18
    RTA_NEWDST = 19
    RTA_PREF = 20
    RTA_ENCAP_TYPE = 21
    RTA_ENCAP = 22
    RTA_EXPIRES = 23
    RTA_PAD = 24
    RTA_UID = 25
    RTA_TTL_PROPAGATE = 26
    RTA_IP_PROTO = 27
    RTA_SPORT = 28
    RTA_DPORT = 29
    RTA_NH_ID = 30


class NlRtMsgType(Enum):
    RTM_NEWLINK = 16
    RTM_DELLINK = 17
    RTM_GETLINK = 18
    RTM_SETLINK = 19
    RTM_NEWADDR = 20
    RTM_DELADDR = 21
    RTM_GETADDR = 22
    RTM_NEWROUTE = 24
    RTM_DELROUTE = 25
    RTM_GETROUTE = 26
    RTM_NEWNEIGH = 28
    RTM_DELNEIGH = 29
    RTM_GETNEIGH = 30
    RTM_NEWRULE = 32
    RTM_DELRULE = 33
    RTM_GETRULE = 34
    RTM_NEWQDISC = 36
    RTM_DELQDISC = 37
    RTM_GETQDISC = 38
    RTM_NEWTCLASS = 40
    RTM_DELTCLASS = 41
    RTM_GETTCLASS = 42
    RTM_NEWTFILTER = 44
    RTM_DELTFILTER = 45
    RTM_GETTFILTER = 46
    RTM_NEWACTION = 48
    RTM_DELACTION = 49
    RTM_GETACTION = 50
    RTM_NEWPREFIX = 52
    RTM_GETMULTICAST = 58
    RTM_GETANYCAST = 62
    RTM_NEWNEIGHTBL = 64
    RTM_GETNEIGHTBL = 66
    RTM_SETNEIGHTBL = 67
    RTM_NEWNDUSEROPT = 68
    RTM_NEWADDRLABEL = 72
    RTM_DELADDRLABEL = 73
    RTM_GETADDRLABEL = 74
    RTM_GETDCB = 78
    RTM_SETDCB = 79
    RTM_NEWNETCONF = 80
    RTM_GETNETCONF = 82
    RTM_NEWMDB = 84
    RTM_DELMDB = 85
    RTM_GETMDB = 86
    RTM_NEWNSID = 88
    RTM_DELNSID = 89
    RTM_GETNSID = 90
    RTM_NEWSTATS = 92
    RTM_GETSTATS = 94


class RtAttr(Structure):
    _fields_ = [
        ("rta_len", c_ushort),
        ("rta_type", c_ushort),
    ]


class RtMsgHdr(Structure):
    _fields_ = [
        ("rtm_family", c_ubyte),
        ("rtm_dst_len", c_ubyte),
        ("rtm_src_len", c_ubyte),
        ("rtm_tos", c_ubyte),
        ("rtm_table", c_ubyte),
        ("rtm_protocol", c_ubyte),
        ("rtm_scope", c_ubyte),
        ("rtm_type", c_ubyte),
        ("rtm_flags", c_uint),
    ]


class RtMsgFlags(Enum):
    RTM_F_NOTIFY = 0x100
    RTM_F_CLONED = 0x200
    RTM_F_EQUALIZE = 0x400
    RTM_F_PREFIX = 0x800
    RTM_F_LOOKUP_TABLE = 0x1000
    RTM_F_FIB_MATCH = 0x2000
    RTM_F_OFFLOAD = 0x4000
    RTM_F_TRAP = 0x8000
    RTM_F_OFFLOAD_FAILED = 0x20000000


class RtScope(Enum):
    RT_SCOPE_UNIVERSE = 0
    RT_SCOPE_SITE = 200
    RT_SCOPE_LINK = 253
    RT_SCOPE_HOST = 254
    RT_SCOPE_NOWHERE = 255


class RtType(Enum):
    RTN_UNSPEC = 0
    RTN_UNICAST = auto()
    RTN_LOCAL = auto()
    RTN_BROADCAST = auto()
    RTN_ANYCAST = auto()
    RTN_MULTICAST = auto()
    RTN_BLACKHOLE = auto()
    RTN_UNREACHABLE = auto()
    RTN_PROHIBIT = auto()
    RTN_THROW = auto()
    RTN_NAT = auto()
    RTN_XRESOLVE = auto()


class RtProto(Enum):
    RTPROT_UNSPEC = 0
    RTPROT_REDIRECT = 1
    RTPROT_KERNEL = 2
    RTPROT_BOOT = 3
    RTPROT_STATIC = 4
    RTPROT_GATED = 8
    RTPROT_RA = 9
    RTPROT_MRT = 10
    RTPROT_ZEBRA = 11
    RTPROT_BIRD = 12
    RTPROT_DNROUTED = 13
    RTPROT_XORP = 14
    RTPROT_NTK = 15
    RTPROT_DHCP = 16
    RTPROT_MROUTED = 17
    RTPROT_KEEPALIVED = 18
    RTPROT_BABEL = 42
    RTPROT_OPENR = 99
    RTPROT_BGP = 186
    RTPROT_ISIS = 187
    RTPROT_OSPF = 188
    RTPROT_RIP = 189
    RTPROT_EIGRP = 192


class NlRtaxType(Enum):
    RTAX_UNSPEC = 0
    RTAX_LOCK = auto()
    RTAX_MTU = auto()
    RTAX_WINDOW = auto()
    RTAX_RTT = auto()
    RTAX_RTTVAR = auto()
    RTAX_SSTHRESH = auto()
    RTAX_CWND = auto()
    RTAX_ADVMSS = auto()
    RTAX_REORDERING = auto()
    RTAX_HOPLIMIT = auto()
    RTAX_INITCWND = auto()
    RTAX_FEATURES = auto()
    RTAX_RTO_MIN = auto()
    RTAX_INITRWND = auto()
    RTAX_QUICKACK = auto()
    RTAX_CC_ALGO = auto()
    RTAX_FASTOPEN_NO_COOKIE = auto()


class RtFlagsBSD(Enum):
    RTF_UP = 0x1
    RTF_GATEWAY = 0x2
    RTF_HOST = 0x4
    RTF_REJECT = 0x8
    RTF_DYNAMIC = 0x10
    RTF_MODIFIED = 0x20
    RTF_DONE = 0x40
    RTF_XRESOLVE = 0x200
    RTF_LLINFO = 0x400
    RTF_LLDATA = 0x400
    RTF_STATIC = 0x800
    RTF_BLACKHOLE = 0x1000
    RTF_PROTO2 = 0x4000
    RTF_PROTO1 = 0x8000
    RTF_PROTO3 = 0x40000
    RTF_FIXEDMTU = 0x80000
    RTF_PINNED = 0x100000
    RTF_LOCAL = 0x200000
    RTF_BROADCAST = 0x400000
    RTF_MULTICAST = 0x800000
    RTF_STICKY = 0x10000000
    RTF_RNH_LOCKED = 0x40000000
    RTF_GWFLAG_COMPAT = 0x80000000


class NlRtGroup(Enum):
    RTNLGRP_NONE = 0
    RTNLGRP_LINK = auto()
    RTNLGRP_NOTIFY = auto()
    RTNLGRP_NEIGH = auto()
    RTNLGRP_TC = auto()
    RTNLGRP_IPV4_IFADDR = auto()
    RTNLGRP_IPV4_MROUTE = auto()
    RTNLGRP_IPV4_ROUTE = auto()
    RTNLGRP_IPV4_RULE = auto()
    RTNLGRP_IPV6_IFADDR = auto()
    RTNLGRP_IPV6_MROUTE = auto()
    RTNLGRP_IPV6_ROUTE = auto()
    RTNLGRP_IPV6_IFINFO = auto()
    RTNLGRP_DECnet_IFADDR = auto()
    RTNLGRP_NOP2 = auto()
    RTNLGRP_DECnet_ROUTE = auto()
    RTNLGRP_DECnet_RULE = auto()
    RTNLGRP_NOP4 = auto()
    RTNLGRP_IPV6_PREFIX = auto()
    RTNLGRP_IPV6_RULE = auto()
    RTNLGRP_ND_USEROPT = auto()
    RTNLGRP_PHONET_IFADDR = auto()
    RTNLGRP_PHONET_ROUTE = auto()
    RTNLGRP_DCB = auto()
    RTNLGRP_IPV4_NETCONF = auto()
    RTNLGRP_IPV6_NETCONF = auto()
    RTNLGRP_MDB = auto()
    RTNLGRP_MPLS_ROUTE = auto()
    RTNLGRP_NSID = auto()
    RTNLGRP_MPLS_NETCONF = auto()
    RTNLGRP_IPV4_MROUTE_R = auto()
    RTNLGRP_IPV6_MROUTE_R = auto()
    RTNLGRP_NEXTHOP = auto()
    RTNLGRP_BRVLAN = auto()


class IfinfoMsg(Structure):
    _fields_ = [
        ("ifi_family", c_ubyte),
        ("__ifi_pad", c_ubyte),
        ("ifi_type", c_ushort),
        ("ifi_index", c_int),
        ("ifi_flags", c_uint),
        ("ifi_change", c_uint),
    ]


class IflattrType(Enum):
    IFLA_UNSPEC = 0
    IFLA_ADDRESS = auto()
    IFLA_BROADCAST = auto()
    IFLA_IFNAME = auto()
    IFLA_MTU = auto()
    IFLA_LINK = auto()
    IFLA_QDISC = auto()
    IFLA_STATS = auto()
    IFLA_COST = auto()
    IFLA_PRIORITY = auto()
    IFLA_MASTER = auto()
    IFLA_WIRELESS = auto()
    IFLA_PROTINFO = auto()
    IFLA_TXQLEN = auto()
    IFLA_MAP = auto()
    IFLA_WEIGHT = auto()
    IFLA_OPERSTATE = auto()
    IFLA_LINKMODE = auto()
    IFLA_LINKINFO = auto()
    IFLA_NET_NS_PID = auto()
    IFLA_IFALIAS = auto()
    IFLA_NUM_VF = auto()
    IFLA_VFINFO_LIST = auto()
    IFLA_STATS64 = auto()
    IFLA_VF_PORTS = auto()
    IFLA_PORT_SELF = auto()
    IFLA_AF_SPEC = auto()
    IFLA_GROUP = auto()
    IFLA_NET_NS_FD = auto()
    IFLA_EXT_MASK = auto()
    IFLA_PROMISCUITY = auto()
    IFLA_NUM_TX_QUEUES = auto()
    IFLA_NUM_RX_QUEUES = auto()
    IFLA_CARRIER = auto()
    IFLA_PHYS_PORT_ID = auto()
    IFLA_CARRIER_CHANGES = auto()
    IFLA_PHYS_SWITCH_ID = auto()
    IFLA_LINK_NETNSID = auto()
    IFLA_PHYS_PORT_NAME = auto()
    IFLA_PROTO_DOWN = auto()
    IFLA_GSO_MAX_SEGS = auto()
    IFLA_GSO_MAX_SIZE = auto()
    IFLA_PAD = auto()
    IFLA_XDP = auto()
    IFLA_EVENT = auto()
    IFLA_NEW_NETNSID = auto()
    IFLA_IF_NETNSID = auto()
    IFLA_CARRIER_UP_COUNT = auto()
    IFLA_CARRIER_DOWN_COUNT = auto()
    IFLA_NEW_IFINDEX = auto()
    IFLA_MIN_MTU = auto()
    IFLA_MAX_MTU = auto()
    IFLA_PROP_LIST = auto()
    IFLA_ALT_IFNAME = auto()
    IFLA_PERM_ADDRESS = auto()
    IFLA_PROTO_DOWN_REASON = auto()


class IflinkInfo(Enum):
    IFLA_INFO_UNSPEC = 0
    IFLA_INFO_KIND = auto()
    IFLA_INFO_DATA = auto()
    IFLA_INFO_XSTATS = auto()
    IFLA_INFO_SLAVE_KIND = auto()
    IFLA_INFO_SLAVE_DATA = auto()


class IfLinkInfoDataVlan(Enum):
    IFLA_VLAN_UNSPEC = 0
    IFLA_VLAN_ID = auto()
    IFLA_VLAN_FLAGS = auto()
    IFLA_VLAN_EGRESS_QOS = auto()
    IFLA_VLAN_INGRESS_QOS = auto()
    IFLA_VLAN_PROTOCOL = auto()


class IfaddrMsg(Structure):
    _fields_ = [
        ("ifa_family", c_ubyte),
        ("ifa_prefixlen", c_ubyte),
        ("ifa_flags", c_ubyte),
        ("ifa_scope", c_ubyte),
        ("ifa_index", c_uint),
    ]


class IfaAttrType(Enum):
    IFA_UNSPEC = 0
    IFA_ADDRESS = auto()
    IFA_LOCAL = auto()
    IFA_LABEL = auto()
    IFA_BROADCAST = auto()
    IFA_ANYCAST = auto()
    IFA_CACHEINFO = auto()
    IFA_MULTICAST = auto()
    IFA_FLAGS = auto()
    IFA_RT_PRIORITY = auto()
    IFA_TARGET_NETNSID = auto()


class NdMsg(Structure):
    _fields_ = [
        ("ndm_family", c_ubyte),
        ("ndm_pad1", c_ubyte),
        ("ndm_pad2", c_ubyte),
        ("ndm_ifindex", c_uint),
        ("ndm_state", c_ushort),
        ("ndm_flags", c_ubyte),
        ("ndm_type", c_ubyte),
    ]


class NdAttrType(Enum):
    NDA_UNSPEC = 0
    NDA_DST = 1
    NDA_LLADDR = 2
    NDA_CACHEINFO = 3
    NDA_PROBES = 4
    NDA_VLAN = 5
    NDA_PORT = 6
    NDA_VNI = 7
    NDA_IFINDEX = 8
    NDA_MASTER = 9
    NDA_LINK_NETNSID = 10
    NDA_SRC_VNI = 11
    NDA_PROTOCOL = 12
    NDA_NH_ID = 13
    NDA_FDB_EXT_ATTRS = 14
    NDA_FLAGS_EXT = 15
    NDA_NDM_STATE_MASK = 16
    NDA_NDM_FLAGS_MASK = 17


class NlAttrRtFlags(NlAttrU32):
    def _print_attr_value(self):
        s = get_bitmask_str(RtFlagsBSD, self.u32)
        return " rtflags={}".format(s)


class NlAttrIfindex(NlAttrU32):
    def _print_attr_value(self):
        try:
            ifname = socket.if_indextoname(self.u32)
            return " iface={}(#{})".format(ifname, self.u32)
        except OSError:
            pass
        return " iface=if#{}".format(self.u32)


class NlAttrTable(NlAttrU32):
    def _print_attr_value(self):
        return " rtable={}".format(self.u32)


class NlAttrNhId(NlAttrU32):
    def _print_attr_value(self):
        return " nh_id={}".format(self.u32)


class NlAttrKNhId(NlAttrU32):
    def _print_attr_value(self):
        return " knh_id={}".format(self.u32)


class NlAttrMac(NlAttr):
    def _print_attr_value(self):
        return ' mac="' + ":".join(["{:02X}".format(b) for b in self._data]) + '"'


class NlAttrIfStats(NlAttr):
    def _print_attr_value(self):
        return " stats={...}"


class NlAttrVia(NlAttr):
    def __init__(self, nla_type, family, addr: str):
        super().__init__(nla_type, b"")
        self.addr = addr
        self.family = family

    @staticmethod
    def _validate(data):
        nla_len, nla_type = struct.unpack("@HH", data[:4])
        data_len = nla_len - 4
        if data_len == 0:
            raise ValueError(
                "Error validating attr {}: empty data".format(nla_type)
            )  # noqa: E501
        family = int(data_len[0])
        if family not in (socket.AF_INET, socket.AF_INET6):
            raise ValueError(
                "Error validating attr {}: unsupported AF {}".format(  # noqa: E501
                    nla_type, family
                )
            )
        if family == socket.AF_INET:
            expected_len = 1 + 4
        else:
            expected_len = 1 + 16
        if data_len != expected_len:
            raise ValueError(
                "Error validating attr {}: expected len {} got {}".format(  # noqa: E501
                    nla_type, expected_len, data_len
                )
            )

    @property
    def nla_len(self):
        if self.family == socket.AF_INET6:
            return 21
        else:
            return 9

    @classmethod
    def _parse(cls, data):
        nla_len, nla_type, family = struct.unpack("@HHB", data[:5])
        off = 5
        if family == socket.AF_INET:
            addr = socket.inet_ntop(family, data[off:off + 4])
        else:
            addr = socket.inet_ntop(family, data[off:off + 16])
        return cls(nla_type, family, addr)

    def __bytes__(self):
        addr = socket.inet_pton(self.family, self.addr)
        return self._to_bytes(struct.pack("@B", self.family) + addr)

    def _print_attr_value(self):
        return " via={}".format(self.addr)


rtnl_route_attrs = prepare_attrs_map(
    [
        AttrDescr(RtattrType.RTA_DST, NlAttrIp),
        AttrDescr(RtattrType.RTA_SRC, NlAttrIp),
        AttrDescr(RtattrType.RTA_IIF, NlAttrIfindex),
        AttrDescr(RtattrType.RTA_OIF, NlAttrIfindex),
        AttrDescr(RtattrType.RTA_GATEWAY, NlAttrIp),
        AttrDescr(RtattrType.RTA_TABLE, NlAttrTable),
        AttrDescr(RtattrType.RTA_PRIORITY, NlAttrU32),
        AttrDescr(RtattrType.RTA_VIA, NlAttrVia),
        AttrDescr(RtattrType.RTA_NH_ID, NlAttrNhId),
        AttrDescr(RtattrType.RTA_KNH_ID, NlAttrKNhId),
        AttrDescr(RtattrType.RTA_RTFLAGS, NlAttrRtFlags),
        AttrDescr(
            RtattrType.RTA_METRICS,
            NlAttrNested,
            [
                AttrDescr(NlRtaxType.RTAX_MTU, NlAttrU32),
            ],
        ),
    ]
)

rtnl_ifla_attrs = prepare_attrs_map(
    [
        AttrDescr(IflattrType.IFLA_ADDRESS, NlAttrMac),
        AttrDescr(IflattrType.IFLA_BROADCAST, NlAttrMac),
        AttrDescr(IflattrType.IFLA_IFNAME, NlAttrStr),
        AttrDescr(IflattrType.IFLA_MTU, NlAttrU32),
        AttrDescr(IflattrType.IFLA_LINK, NlAttrU32),
        AttrDescr(IflattrType.IFLA_PROMISCUITY, NlAttrU32),
        AttrDescr(IflattrType.IFLA_OPERSTATE, NlAttrU8),
        AttrDescr(IflattrType.IFLA_CARRIER, NlAttrU8),
        AttrDescr(IflattrType.IFLA_IFALIAS, NlAttrStr),
        AttrDescr(IflattrType.IFLA_STATS64, NlAttrIfStats),
        AttrDescr(IflattrType.IFLA_NEW_IFINDEX, NlAttrU32),
        AttrDescr(
            IflattrType.IFLA_LINKINFO,
            NlAttrNested,
            [
                AttrDescr(IflinkInfo.IFLA_INFO_KIND, NlAttrStr),
                AttrDescr(IflinkInfo.IFLA_INFO_DATA, NlAttr),
            ],
        ),
    ]
)

rtnl_ifa_attrs = prepare_attrs_map(
    [
        AttrDescr(IfaAttrType.IFA_ADDRESS, NlAttrIp),
        AttrDescr(IfaAttrType.IFA_LOCAL, NlAttrIp),
        AttrDescr(IfaAttrType.IFA_LABEL, NlAttrStr),
        AttrDescr(IfaAttrType.IFA_BROADCAST, NlAttrIp),
        AttrDescr(IfaAttrType.IFA_ANYCAST, NlAttrIp),
        AttrDescr(IfaAttrType.IFA_FLAGS, NlAttrU32),
    ]
)


rtnl_nd_attrs = prepare_attrs_map(
    [
        AttrDescr(NdAttrType.NDA_DST, NlAttrIp),
        AttrDescr(NdAttrType.NDA_IFINDEX, NlAttrIfindex),
        AttrDescr(NdAttrType.NDA_FLAGS_EXT, NlAttrU32),
        AttrDescr(NdAttrType.NDA_LLADDR, NlAttrMac),
    ]
)


class BaseNetlinkRtMessage(StdNetlinkMessage):
    pass


class NetlinkRtMessage(BaseNetlinkRtMessage):
    messages = [
        NlMsgProps(NlRtMsgType.RTM_NEWROUTE, NlMsgCategory.NEW),
        NlMsgProps(NlRtMsgType.RTM_DELROUTE, NlMsgCategory.DELETE),
        NlMsgProps(NlRtMsgType.RTM_GETROUTE, NlMsgCategory.GET),
    ]
    nl_attrs_map = rtnl_route_attrs

    def __init__(self, helper, nlm_type):
        super().__init__(helper, nlm_type)
        self.base_hdr = RtMsgHdr()

    def parse_base_header(self, data):
        if len(data) < sizeof(RtMsgHdr):
            raise ValueError("length less than rtmsg header")
        rtm_hdr = RtMsgHdr.from_buffer_copy(data)
        return (rtm_hdr, sizeof(RtMsgHdr))

    def print_base_header(self, hdr, prepend=""):
        family = self.helper.get_af_name(hdr.rtm_family)
        print(
            "{}family={}, dst_len={}, src_len={}, tos={}, table={}, protocol={}({}), scope={}({}), type={}({}), flags={}({})".format(  # noqa: E501
                prepend,
                family,
                hdr.rtm_dst_len,
                hdr.rtm_src_len,
                hdr.rtm_tos,
                hdr.rtm_table,
                self.helper.get_attr_byval(RtProto, hdr.rtm_protocol),
                hdr.rtm_protocol,
                self.helper.get_attr_byval(RtScope, hdr.rtm_scope),
                hdr.rtm_scope,
                self.helper.get_attr_byval(RtType, hdr.rtm_type),
                hdr.rtm_type,
                self.helper.get_bitmask_str(RtMsgFlags, hdr.rtm_flags),
                hdr.rtm_flags,
            )
        )


class NetlinkIflaMessage(BaseNetlinkRtMessage):
    messages = [
        NlMsgProps(NlRtMsgType.RTM_NEWLINK, NlMsgCategory.NEW),
        NlMsgProps(NlRtMsgType.RTM_DELLINK, NlMsgCategory.DELETE),
        NlMsgProps(NlRtMsgType.RTM_GETLINK, NlMsgCategory.GET),
    ]
    nl_attrs_map = rtnl_ifla_attrs

    def __init__(self, helper, nlm_type):
        super().__init__(helper, nlm_type)
        self.base_hdr = IfinfoMsg()

    def parse_base_header(self, data):
        if len(data) < sizeof(IfinfoMsg):
            raise ValueError("length less than IfinfoMsg header")
        rtm_hdr = IfinfoMsg.from_buffer_copy(data)
        return (rtm_hdr, sizeof(IfinfoMsg))

    def print_base_header(self, hdr, prepend=""):
        family = self.helper.get_af_name(hdr.ifi_family)
        print(
            "{}family={}, ifi_type={}, ifi_index={}, ifi_flags={}, ifi_change={}".format(  # noqa: E501
                prepend,
                family,
                hdr.ifi_type,
                hdr.ifi_index,
                hdr.ifi_flags,
                hdr.ifi_change,
            )
        )


class NetlinkIfaMessage(BaseNetlinkRtMessage):
    messages = [
        NlMsgProps(NlRtMsgType.RTM_NEWADDR, NlMsgCategory.NEW),
        NlMsgProps(NlRtMsgType.RTM_DELADDR, NlMsgCategory.DELETE),
        NlMsgProps(NlRtMsgType.RTM_GETADDR, NlMsgCategory.GET),
    ]
    nl_attrs_map = rtnl_ifa_attrs

    def __init__(self, helper, nlm_type):
        super().__init__(helper, nlm_type)
        self.base_hdr = IfaddrMsg()

    def parse_base_header(self, data):
        if len(data) < sizeof(IfaddrMsg):
            raise ValueError("length less than IfaddrMsg header")
        rtm_hdr = IfaddrMsg.from_buffer_copy(data)
        return (rtm_hdr, sizeof(IfaddrMsg))

    def print_base_header(self, hdr, prepend=""):
        family = self.helper.get_af_name(hdr.ifa_family)
        print(
            "{}family={}, ifa_prefixlen={}, ifa_flags={}, ifa_scope={}, ifa_index={}".format(  # noqa: E501
                prepend,
                family,
                hdr.ifa_prefixlen,
                hdr.ifa_flags,
                hdr.ifa_scope,
                hdr.ifa_index,
            )
        )


class NetlinkNdMessage(BaseNetlinkRtMessage):
    messages = [
        NlMsgProps(NlRtMsgType.RTM_NEWNEIGH, NlMsgCategory.NEW),
        NlMsgProps(NlRtMsgType.RTM_DELNEIGH, NlMsgCategory.DELETE),
        NlMsgProps(NlRtMsgType.RTM_GETNEIGH, NlMsgCategory.GET),
    ]
    nl_attrs_map = rtnl_nd_attrs

    def __init__(self, helper, nlm_type):
        super().__init__(helper, nlm_type)
        self.base_hdr = NdMsg()

    def parse_base_header(self, data):
        if len(data) < sizeof(NdMsg):
            raise ValueError("length less than NdMsg header")
        nd_hdr = NdMsg.from_buffer_copy(data)
        return (nd_hdr, sizeof(NdMsg))

    def print_base_header(self, hdr, prepend=""):
        family = self.helper.get_af_name(hdr.ndm_family)
        print(
            "{}family={}, ndm_ifindex={}, ndm_state={}, ndm_flags={}".format(  # noqa: E501
                prepend,
                family,
                hdr.ndm_ifindex,
                hdr.ndm_state,
                hdr.ndm_flags,
            )
        )


handler_classes = {
    "netlink_route": [
        NetlinkRtMessage,
        NetlinkIflaMessage,
        NetlinkIfaMessage,
        NetlinkNdMessage,
    ],
}
