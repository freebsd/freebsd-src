#!/usr/local/bin/python3
import os
import socket
import struct
import sys
import unittest
from ctypes import c_int
from ctypes import c_ubyte
from ctypes import c_uint
from ctypes import c_ushort
from ctypes import sizeof
from ctypes import Structure
from enum import auto
from enum import Enum
from typing import Any
from typing import Dict
from typing import List
from typing import NamedTuple


def roundup2(val: int, num: int) -> int:
    if val % num:
        return (val | (num - 1)) + 1
    else:
        return val


def align4(val: int) -> int:
    return roundup2(val, 4)


class SockaddrNl(Structure):
    _fields_ = [
        ("nl_len", c_ubyte),
        ("nl_family", c_ubyte),
        ("nl_pad", c_ushort),
        ("nl_pid", c_uint),
        ("nl_groups", c_uint),
    ]


class Nlmsghdr(Structure):
    _fields_ = [
        ("nlmsg_len", c_uint),
        ("nlmsg_type", c_ushort),
        ("nlmsg_flags", c_ushort),
        ("nlmsg_seq", c_uint),
        ("nlmsg_pid", c_uint),
    ]


class Nlmsgdone(Structure):
    _fields_ = [
        ("error", c_int),
    ]


class Nlmsgerr(Structure):
    _fields_ = [
        ("error", c_int),
        ("msg", Nlmsghdr),
    ]


class NlErrattrType(Enum):
    NLMSGERR_ATTR_UNUSED = 0
    NLMSGERR_ATTR_MSG = auto()
    NLMSGERR_ATTR_OFFS = auto()
    NLMSGERR_ATTR_COOKIE = auto()
    NLMSGERR_ATTR_POLICY = auto()


class RtattrType(Enum):
    RTA_UNSPEC = 0
    RTA_DST = auto()
    RTA_SRC = auto()
    RTA_IIF = auto()
    RTA_OIF = auto()
    RTA_GATEWAY = auto()
    RTA_PRIORITY = auto()
    RTA_PREFSRC = auto()
    RTA_METRICS = auto()
    RTA_MULTIPATH = auto()
    RTA_PROTOINFO = auto()
    RTA_FLOW = auto()
    RTA_CACHEINFO = auto()
    RTA_SESSION = auto()
    RTA_MP_ALGO = auto()
    RTA_TABLE = auto()
    RTA_MARK = auto()
    RTA_MFC_STATS = auto()
    RTA_VIA = auto()
    RTA_NEWDST = auto()
    RTA_PREF = auto()
    RTA_ENCAP_TYPE = auto()
    RTA_ENCAP = auto()
    RTA_EXPIRES = auto()
    RTA_PAD = auto()
    RTA_UID = auto()
    RTA_TTL_PROPAGATE = auto()
    RTA_IP_PROTO = auto()
    RTA_SPORT = auto()
    RTA_DPORT = auto()
    RTA_NH_ID = auto()


class NlMsgType(Enum):
    NLMSG_NOOP = 1
    NLMSG_ERROR = 2
    NLMSG_DONE = 3
    NLMSG_OVERRUN = 4


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
    RTM_DELNEIGH = 27
    RTM_GETNEIGH = 28
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


class AddressFamilyLinux(Enum):
    AF_INET = socket.AF_INET
    AF_INET6 = socket.AF_INET6
    AF_NETLINK = 16


class AddressFamilyBsd(Enum):
    AF_INET = socket.AF_INET
    AF_INET6 = socket.AF_INET6
    AF_NETLINK = 38


class NlmBaseFlags(Enum):
    NLM_F_REQUEST = 0x01
    NLM_F_MULTI = 0x02
    NLM_F_ACK = 0x04
    NLM_F_ECHO = 0x08
    NLM_F_DUMP_INTR = 0x10
    NLM_F_DUMP_FILTERED = 0x20


# XXX: in python3.8 it is possible to
# class NlmGetFlags(Enum, NlmBaseFlags):


class NlmGetFlags(Enum):
    NLM_F_ROOT = 0x100
    NLM_F_MATCH = 0x200
    NLM_F_ATOMIC = 0x400


class NlmNewFlags(Enum):
    NLM_F_REPLACE = 0x100
    NLM_F_EXCL = 0x200
    NLM_F_CREATE = 0x400
    NLM_F_APPEND = 0x800


class NlmDeleteFlags(Enum):
    NLM_F_NONREC = 0x100


class NlmAckFlags(Enum):
    NLM_F_CAPPED = 0x100
    NLM_F_ACK_TLVS = 0x200


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


class IfattrType(Enum):
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


class GenlMsgHdr(Structure):
    _fields_ = [
        ("cmd", c_ubyte),
        ("version", c_ubyte),
        ("reserved", c_ushort),
    ]


class NlConst:
    AF_NETLINK = 38
    NETLINK_ROUTE = 0
    NETLINK_GENERIC = 16


class NlHelper:
    def __init__(self):
        self._pmap = {}
        self._af_cls = self.get_af_cls()
        self._seq_counter = 1
        self.pid = os.getpid()

    def get_seq(self):
        ret = self._seq_counter
        self._seq_counter += 1
        return ret

    def get_af_cls(self):
        if sys.platform.startswith("freebsd"):
            cls = AddressFamilyBsd
        else:
            cls = AddressFamilyLinux
        return cls

    def get_propmap(self, cls):
        if cls not in self._pmap:
            ret = {}
            for prop in dir(cls):
                if not prop.startswith("_"):
                    ret[getattr(cls, prop).value] = prop
            self._pmap[cls] = ret
        return self._pmap[cls]

    def get_name_propmap(self, cls):
        ret = {}
        for prop in dir(cls):
            if not prop.startswith("_"):
                ret[prop] = getattr(cls, prop).value
        return ret

    def get_attr_byval(self, cls, attr_val):
        propmap = self.get_propmap(cls)
        return propmap.get(attr_val)

    def get_nlmsg_name(self, val):
        for cls in [NlRtMsgType, NlMsgType]:
            v = self.get_attr_byval(cls, val)
            if v is not None:
                return v
        return "msg#{}".format(val)

    def get_af_name(self, family):
        v = self.get_attr_byval(self._af_cls, family)
        if v is not None:
            return v
        return "af#{}".format(family)

    def get_af_value(self, family_str: str) -> int:
        propmap = self.get_name_propmap(self._af_cls)
        return propmap.get(family_str)

    def get_rta_name(self, val):
        return self.get_attr_byval(RtattrType, val)

    def get_bitmask_map(self, cls, val):
        propmap = self.get_propmap(cls)
        v = 1
        ret = {}
        while val:
            if v & val:
                if v in propmap:
                    ret[v] = propmap[v]
                else:
                    ret[v] = hex(v)
                val -= v
            v *= 2
        return ret

    def get_bitmask_str(self, cls, val):
        bmap = self.get_bitmask_map(cls, val)
        return ",".join([v for k, v in bmap.items()])

    def get_nlm_flags_str(self, msg_str: str, reply: bool, val):
        if reply:
            return self.get_bitmask_str(NlmAckFlags, val)
        if msg_str.startswith("RTM_GET"):
            return self.get_bitmask_str(NlmGetFlags, val)
        elif msg_str.startswith("RTM_DEL"):
            return self.get_bitmask_str(NlmDeleteFlags, val)
        elif msg_str.startswith("RTM_NEW"):
            return self.get_bitmask_str(NlmNewFlags, val)
        else:
            return self.get_bitmask_str(NlmBaseFlags, val)


class NlAttr(object):
    def __init__(self, nla_type, data):
        if isinstance(nla_type, Enum):
            self._nla_type = nla_type.value
            self._enum = nla_type
        else:
            self._nla_type = nla_type
            self._enum = None
        self.nla_list = []
        self._data = data

    @property
    def nla_type(self):
        return self._nla_type & 0x3F

    @property
    def nla_len(self):
        return len(self._data) + 4

    def add_nla(self, nla):
        self.nla_list.append(nla)

    def print_attr(self, prepend=""):
        if self._enum is not None:
            type_str = self._enum.name
        else:
            type_str = "nla#{}".format(self.nla_type)
        print(
            "{}len={} type={}({}){}".format(
                prepend, self.nla_len, type_str, self.nla_type, self._print_attr_value()
            )
        )

    @staticmethod
    def _validate(data):
        if len(data) < 4:
            raise ValueError("attribute too short")
        nla_len, nla_type = struct.unpack("@HH", data[:4])
        if nla_len > len(data):
            raise ValueError("attribute length too big")
        if nla_len < 4:
            raise ValueError("attribute length too short")

    @classmethod
    def _parse(cls, data):
        nla_len, nla_type = struct.unpack("@HH", data[:4])
        return cls(nla_type, data[4:])

    @classmethod
    def from_bytes(cls, data, attr_type_enum=None):
        cls._validate(data)
        attr = cls._parse(data)
        attr._enum = attr_type_enum
        return attr

    def _to_bytes(self, data: bytes):
        ret = data
        if align4(len(ret)) != len(ret):
            ret = data + bytes(align4(len(ret)) - len(ret))
        return struct.pack("@HH", len(data) + 4, self._nla_type) + ret

    def __bytes__(self):
        return self._to_bytes(self._data)

    def _print_attr_value(self):
        return " " + " ".join(["x{:02X}".format(b) for b in self._data])


class NlAttrNested(NlAttr):
    def __init__(self, nla_type, val):
        super().__init__(nla_type, b"")
        self.nla_list = val

    @property
    def nla_len(self):
        return align4(len(b"".join([bytes(nla) for nla in self.nla_list]))) + 4

    def print_attr(self, prepend=""):
        if self._enum is not None:
            type_str = self._enum.name
        else:
            type_str = "nla#{}".format(self.nla_type)
        print(
            "{}len={} type={}({}) {{".format(
                prepend, self.nla_len, type_str, self.nla_type
            )
        )
        for nla in self.nla_list:
            nla.print_attr(prepend + "  ")
        print("{}}}".format(prepend))

    def __bytes__(self):
        return self._to_bytes(b"".join([bytes(nla) for nla in self.nla_list]))


class NlAttrU32(NlAttr):
    def __init__(self, nla_type, val):
        self.u32 = val
        super().__init__(nla_type, b"")

    @property
    def nla_len(self):
        return 8

    def _print_attr_value(self):
        return " val={}".format(self.u32)

    @staticmethod
    def _validate(data):
        assert len(data) == 8
        nla_len, nla_type = struct.unpack("@HH", data[:4])
        assert nla_len == 8

    @classmethod
    def _parse(cls, data):
        nla_len, nla_type, val = struct.unpack("@HHI", data)
        return cls(nla_type, val)

    def __bytes__(self):
        return self._to_bytes(struct.pack("@I", self.u32))


class NlAttrU16(NlAttr):
    def __init__(self, nla_type, val):
        self.u16 = val
        super().__init__(nla_type, b"")

    @property
    def nla_len(self):
        return 6

    def _print_attr_value(self):
        return " val={}".format(self.u16)

    @staticmethod
    def _validate(data):
        assert len(data) == 6
        nla_len, nla_type = struct.unpack("@HH", data[:4])
        assert nla_len == 6

    @classmethod
    def _parse(cls, data):
        nla_len, nla_type, val = struct.unpack("@HHH", data)
        return cls(nla_type, val)

    def __bytes__(self):
        return self._to_bytes(struct.pack("@H", self.u16))


class NlAttrU8(NlAttr):
    def __init__(self, nla_type, val):
        self.u8 = val
        super().__init__(nla_type, b"")

    @property
    def nla_len(self):
        return 5

    def _print_attr_value(self):
        return " val={}".format(self.u8)

    @staticmethod
    def _validate(data):
        assert len(data) == 5
        nla_len, nla_type = struct.unpack("@HH", data[:4])
        assert nla_len == 5

    @classmethod
    def _parse(cls, data):
        nla_len, nla_type, val = struct.unpack("@HHB", data)
        return cls(nla_type, val)

    def __bytes__(self):
        return self._to_bytes(struct.pack("@B", self.u8))


class NlAttrIp(NlAttr):
    def __init__(self, nla_type, addr: str):
        super().__init__(nla_type, b"")
        self.addr = addr
        if ":" in self.addr:
            self.family = socket.AF_INET6
        else:
            self.family = socket.AF_INET

    @staticmethod
    def _validate(data):
        nla_len, nla_type = struct.unpack("@HH", data[:4])
        data_len = nla_len - 4
        if data_len != 4 and data_len != 16:
            raise ValueError(
                "Error validating attr {}: nla_len is not valid".format(  # noqa: E501
                    nla_type
                )
            )

    @property
    def nla_len(self):
        if self.family == socket.AF_INET6:
            return 20
        else:
            return 8
        return align4(len(self._data)) + 4

    @classmethod
    def _parse(cls, data):
        nla_len, nla_type = struct.unpack("@HH", data[:4])
        data_len = len(data) - 4
        if data_len == 4:
            addr = socket.inet_ntop(socket.AF_INET, data[4:8])
        else:
            addr = socket.inet_ntop(socket.AF_INET6, data[4:20])
        return cls(nla_type, addr)

    def __bytes__(self):
        return self._to_bytes(socket.inet_pton(self.family, self.addr))

    def _print_attr_value(self):
        return " addr={}".format(self.addr)


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
            addr = socket.inet_ntop(family, data[off : off + 4])
        else:
            addr = socket.inet_ntop(family, data[off : off + 16])
        return cls(nla_type, family, addr)

    def __bytes__(self):
        addr = socket.inet_pton(self.family, self.addr)
        return self._to_bytes(struct.pack("@B", self.family) + addr)

    def _print_attr_value(self):
        return " via={}".format(self.addr)


class NlAttrStr(NlAttr):
    def __init__(self, nla_type, text):
        super().__init__(nla_type, b"")
        self.text = text

    @staticmethod
    def _validate(data):
        NlAttr._validate(data)
        try:
            data[4:].decode("utf-8")
        except Exception as e:
            raise ValueError("wrong utf-8 string: {}".format(e))

    @property
    def nla_len(self):
        return len(self.text) + 5

    @classmethod
    def _parse(cls, data):
        text = data[4:-1].decode("utf-8")
        nla_len, nla_type = struct.unpack("@HH", data[:4])
        return cls(nla_type, text)

    def __bytes__(self):
        return self._to_bytes(bytes(self.text, encoding="utf-8") + bytes(1))

    def _print_attr_value(self):
        return ' val="{}"'.format(self.text)


class NlAttrStrn(NlAttr):
    def __init__(self, nla_type, text):
        super().__init__(nla_type, b"")
        self.text = text

    @staticmethod
    def _validate(data):
        NlAttr._validate(data)
        try:
            data[4:].decode("utf-8")
        except Exception as e:
            raise ValueError("wrong utf-8 string: {}".format(e))

    @property
    def nla_len(self):
        return len(self.text) + 4

    @classmethod
    def _parse(cls, data):
        text = data[4:].decode("utf-8")
        nla_len, nla_type = struct.unpack("@HH", data[:4])
        return cls(nla_type, text)

    def __bytes__(self):
        return self._to_bytes(bytes(self.text, encoding="utf-8"))

    def _print_attr_value(self):
        return ' val="{}"'.format(self.text)


class AttrDescr(NamedTuple):
    val: Enum
    cls: NlAttr
    child_map: Any = None


def prepare_attrs_map(attrs: List[AttrDescr]) -> Dict[str, Dict]:
    ret = {}
    for ad in attrs:
        ret[ad.val.value] = {"ad": ad}
        if ad.child_map:
            ret[ad.val.value]["child"] = prepare_attrs_map(ad.child_map)
    return ret


rtnl_route_attrs = prepare_attrs_map(
    [
        AttrDescr(RtattrType.RTA_DST, NlAttrIp),
        AttrDescr(RtattrType.RTA_SRC, NlAttrIp),
        AttrDescr(RtattrType.RTA_IIF, NlAttrIfindex),
        AttrDescr(RtattrType.RTA_OIF, NlAttrIfindex),
        AttrDescr(RtattrType.RTA_GATEWAY, NlAttrTable),
        AttrDescr(RtattrType.RTA_VIA, NlAttrVia),
        AttrDescr(RtattrType.RTA_NH_ID, NlAttrNhId),
        AttrDescr(
            RtattrType.RTA_METRICS,
            NlAttrNested,
            [
                AttrDescr(NlRtaxType.RTAX_MTU, NlAttrU32),
            ],
        ),
    ]
)

nldone_attrs = prepare_attrs_map([])

nlerr_attrs = prepare_attrs_map(
    [
        AttrDescr(NlErrattrType.NLMSGERR_ATTR_MSG, NlAttrStr),
        AttrDescr(NlErrattrType.NLMSGERR_ATTR_OFFS, NlAttrU32),
        AttrDescr(NlErrattrType.NLMSGERR_ATTR_COOKIE, NlAttr),
    ]
)

rtnl_ifla_attrs = prepare_attrs_map(
    [
        AttrDescr(IflattrType.IFLA_ADDRESS, NlAttrMac),
        AttrDescr(IflattrType.IFLA_BROADCAST, NlAttrMac),
        AttrDescr(IflattrType.IFLA_IFNAME, NlAttrStr),
        AttrDescr(IflattrType.IFLA_MTU, NlAttrU32),
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
        AttrDescr(IfattrType.IFA_ADDRESS, NlAttrIp),
        AttrDescr(IfattrType.IFA_LOCAL, NlAttrIp),
        AttrDescr(IfattrType.IFA_LABEL, NlAttrStr),
        AttrDescr(IfattrType.IFA_BROADCAST, NlAttrIp),
        AttrDescr(IfattrType.IFA_ANYCAST, NlAttrIp),
        AttrDescr(IfattrType.IFA_FLAGS, NlAttrU32),
    ]
)


class BaseNetlinkMessage(object):
    def __init__(self, helper, nlmsg_type):
        self.nlmsg_type = nlmsg_type
        self.ut = unittest.TestCase()
        self.nla_list = []
        self._orig_data = None
        self.helper = helper
        self.nl_hdr = Nlmsghdr(
            nlmsg_type=nlmsg_type, nlmsg_seq=helper.get_seq(), nlmsg_pid=helper.pid
        )
        self.base_hdr = None

    def add_nla(self, nla):
        self.nla_list.append(nla)

    def _get_nla(self, nla_list, nla_type):
        if isinstance(nla_type, Enum):
            nla_type_raw = nla_type.value
        else:
            nla_type_raw = nla_type
        for nla in nla_list:
            if nla.nla_type == nla_type_raw:
                return nla
        return None

    def get_nla(self, nla_type):
        return self._get_nla(self.nla_list, nla_type)

    @staticmethod
    def parse_nl_header(data: bytes):
        if len(data) < sizeof(Nlmsghdr):
            raise ValueError("length less than netlink message header")
        return Nlmsghdr.from_buffer_copy(data), sizeof(Nlmsghdr)

    def is_type(self, nlmsg_type):
        if isinstance(nlmsg_type, Enum):
            nlmsg_type_raw = nlmsg_type.value
        else:
            nlmsg_type_raw = nlmsg_type
        return nlmsg_type_raw == self.nl_hdr.nlmsg_type

    def is_reply(self, hdr):
        return hdr.nlmsg_type == NlMsgType.NLMSG_ERROR.value

    def print_nl_header(self, hdr, prepend=""):
        # len=44, type=RTM_DELROUTE, flags=NLM_F_REQUEST|NLM_F_ACK, seq=1641163704, pid=0  # noqa: E501
        is_reply = self.is_reply(hdr)
        msg_name = self.helper.get_nlmsg_name(hdr.nlmsg_type)
        print(
            "{}len={}, type={}, flags={}(0x{:X}), seq={}, pid={}".format(
                prepend,
                hdr.nlmsg_len,
                msg_name,
                self.helper.get_nlm_flags_str(
                    msg_name, is_reply, hdr.nlmsg_flags
                ),  # noqa: E501
                hdr.nlmsg_flags,
                hdr.nlmsg_seq,
                hdr.nlmsg_pid,
            )
        )

    @classmethod
    def from_bytes(cls, helper, data):
        try:
            hdr, hdrlen = BaseNetlinkMessage.parse_nl_header(data)
            self = cls(helper, hdr.nlmsg_type)
            self._orig_data = data
            self.nl_hdr = hdr
        except ValueError as e:
            print("Failed to parse nl header: {}".format(e))
            cls.print_as_bytes(data)
            raise
        return self

    def print_message(self):
        self.print_nl_header(self.nl_hdr)

    @staticmethod
    def print_as_bytes(data: bytes, descr: str):
        print("===vv {} (len:{:3d}) vv===".format(descr, len(data)))
        off = 0
        step = 16
        while off < len(data):
            for i in range(step):
                if off + i < len(data):
                    print(" {:02X}".format(data[off + i]), end="")
            print("")
            off += step
        print("--------------------")


class StdNetlinkMessage(BaseNetlinkMessage):
    nl_attrs_map = {}

    @classmethod
    def from_bytes(cls, helper, data):
        try:
            hdr, hdrlen = BaseNetlinkMessage.parse_nl_header(data)
            self = cls(helper, hdr.nlmsg_type)
            self._orig_data = data
            self.nl_hdr = hdr
        except ValueError as e:
            print("Failed to parse nl header: {}".format(e))
            cls.print_as_bytes(data)
            raise

        offset = align4(hdrlen)
        try:
            base_hdr, hdrlen = self.parse_base_header(data[offset:])
            self.base_hdr = base_hdr
            offset += align4(hdrlen)
            # XXX: CAP_ACK
        except ValueError as e:
            print("Failed to parse nl rt header: {}".format(e))
            cls.print_as_bytes(data)
            raise

        orig_offset = offset
        try:
            nla_list, nla_len = self.parse_nla_list(data[offset:])
            offset += nla_len
            if offset != len(data):
                raise ValueError(
                    "{} bytes left at the end of the packet".format(len(data) - offset)
                )  # noqa: E501
            self.nla_list = nla_list
        except ValueError as e:
            print(
                "Failed to parse nla attributes at offset {}: {}".format(orig_offset, e)
            )  # noqa: E501
            cls.print_as_bytes(data, "msg dump")
            cls.print_as_bytes(data[orig_offset:], "failed block")
            raise
        return self

    def parse_attrs(self, data: bytes, attr_map):
        ret = []
        off = 0
        while len(data) - off >= 4:
            nla_len, raw_nla_type = struct.unpack("@HH", data[off : off + 4])
            if nla_len + off > len(data):
                raise ValueError(
                    "attr length {} > than the remaining length {}".format(
                        nla_len, len(data) - off
                    )
                )
            nla_type = raw_nla_type & 0x3F
            if nla_type in attr_map:
                v = attr_map[nla_type]
                val = v["ad"].cls.from_bytes(data[off : off + nla_len], v["ad"].val)
                if "child" in v:
                    # nested
                    attrs, _ = self.parse_attrs(data[off : off + nla_len], v["child"])
                    val = NlAttrNested(raw_nla_type, attrs)
            else:
                # unknown attribute
                val = NlAttr(raw_nla_type, data[off + 4 : off + nla_len])
            ret.append(val)
            off += align4(nla_len)
        return ret, off

    def parse_nla_list(self, data: bytes) -> List[NlAttr]:
        return self.parse_attrs(data, self.nl_attrs_map)

    def print_message(self):
        self.print_nl_header(self.nl_hdr)
        self.print_base_header(self.base_hdr, " ")
        for nla in self.nla_list:
            nla.print_attr("  ")


class NetlinkDoneMessage(StdNetlinkMessage):
    messages = [NlMsgType.NLMSG_DONE.value]
    nl_attrs_map = nldone_attrs

    @property
    def error_code(self):
        return self.base_hdr.error

    def parse_base_header(self, data):
        if len(data) < sizeof(Nlmsgdone):
            raise ValueError("length less than nlmsgdone header")
        done_hdr = Nlmsgdone.from_buffer_copy(data)
        sz = sizeof(Nlmsgdone)
        return (done_hdr, sz)

    def print_base_header(self, hdr, prepend=""):
        print("{}error={}".format(prepend, hdr.error))


class NetlinkErrorMessage(StdNetlinkMessage):
    messages = [NlMsgType.NLMSG_ERROR.value]
    nl_attrs_map = nlerr_attrs

    @property
    def error_code(self):
        return self.base_hdr.error

    @property
    def error_str(self):
        nla = self.get_nla(NlErrattrType.NLMSGERR_ATTR_MSG)
        if nla:
            return nla.text
        return None

    @property
    def error_offset(self):
        nla = self.get_nla(NlErrattrType.NLMSGERR_ATTR_OFFS)
        if nla:
            return nla.u32
        return None

    @property
    def cookie(self):
        return self.get_nla(NlErrattrType.NLMSGERR_ATTR_COOKIE)

    def parse_base_header(self, data):
        if len(data) < sizeof(Nlmsgerr):
            raise ValueError("length less than nlmsgerr header")
        err_hdr = Nlmsgerr.from_buffer_copy(data)
        sz = sizeof(Nlmsgerr)
        if (self.nl_hdr.nlmsg_flags & 0x100) == 0:
            sz += align4(err_hdr.msg.nlmsg_len - sizeof(Nlmsghdr))
        return (err_hdr, sz)

    def print_base_header(self, errhdr, prepend=""):
        print("{}error={}, ".format(prepend, errhdr.error), end="")
        self.print_nl_header(errhdr.msg, prepend)


class BaseNetlinkRtMessage(StdNetlinkMessage):
    def __init__(self, helper, nlm_type):
        super().__init__(helper, nlm_type)
        self.base_hdr = None

    def __bytes__(self):
        ret = bytes()
        for nla in self.nla_list:
            ret += bytes(nla)
        ret = bytes(self.base_hdr) + ret
        self.nl_hdr.nlmsg_len = len(ret) + sizeof(Nlmsghdr)
        return bytes(self.nl_hdr) + ret

    def print_message(self):
        self.print_nl_header(self.nl_hdr)
        self.print_base_header(self.base_hdr, " ")
        for nla in self.nla_list:
            nla.print_attr("  ")


class NetlinkRtMessage(BaseNetlinkRtMessage):
    messages = [
        NlRtMsgType.RTM_NEWROUTE.value,
        NlRtMsgType.RTM_DELROUTE.value,
        NlRtMsgType.RTM_GETROUTE.value,
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
        NlRtMsgType.RTM_NEWLINK.value,
        NlRtMsgType.RTM_DELLINK.value,
        NlRtMsgType.RTM_GETLINK.value,
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
        NlRtMsgType.RTM_NEWADDR.value,
        NlRtMsgType.RTM_DELADDR.value,
        NlRtMsgType.RTM_GETADDR.value,
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


class Nlsock:
    def __init__(self, family, helper):
        self.helper = helper
        self.sock_fd = self._setup_netlink(family)
        self._data = bytes()
        self.msgmap = self.build_msgmap()
        # self.set_groups(NlRtGroup.RTNLGRP_IPV4_ROUTE.value | NlRtGroup.RTNLGRP_IPV6_ROUTE.value)  # noqa: E501

    def build_msgmap(self):
        classes = [
            NetlinkRtMessage,
            NetlinkIflaMessage,
            NetlinkIfaMessage,
            NetlinkDoneMessage,
            NetlinkErrorMessage,
        ]
        xmap = {}
        for cls in classes:
            for message in cls.messages:
                xmap[message] = cls
        return xmap

    def _setup_netlink(self, netlink_family) -> int:
        family = self.helper.get_af_value("AF_NETLINK")
        s = socket.socket(family, socket.SOCK_RAW, netlink_family)
        s.setsockopt(270, 10, 1)  # NETLINK_CAP_ACK
        s.setsockopt(270, 11, 1)  # NETLINK_EXT_ACK
        return s

    def set_groups(self, mask: int):
        self.sock_fd.setsockopt(socket.SOL_SOCKET, 1, mask)
        # snl = SockaddrNl(nl_len = sizeof(SockaddrNl), nl_family=38,
        #                  nl_pid=self.pid, nl_groups=mask)
        # xbuffer = create_string_buffer(sizeof(SockaddrNl))
        # memmove(xbuffer, addressof(snl), sizeof(SockaddrNl))
        # k = struct.pack("@BBHII", 12, 38, 0, self.pid, mask)
        # self.sock_fd.bind(k)

    def write_message(self, msg):
        print("vvvvvvvv OUT vvvvvvvv")
        msg.print_message()
        msg_bytes = bytes(msg)
        try:
            ret = os.write(self.sock_fd.fileno(), msg_bytes)
            assert ret == len(msg_bytes)
        except Exception as e:
            print("write({}) -> {}".format(len(msg_bytes), e))

    def parse_message(self, data: bytes):
        if len(data) < sizeof(Nlmsghdr):
            raise Exception("Short read from nl: {} bytes".format(len(data)))
        hdr = Nlmsghdr.from_buffer_copy(data)
        nlmsg_type = hdr.nlmsg_type
        cls = self.msgmap.get(nlmsg_type)
        if not cls:
            cls = BaseNetlinkMessage
        return cls.from_bytes(self.helper, data)

    def write_data(self, data: bytes):
        self.sock_fd.send(data)

    def read_data(self):
        while True:
            data = self.sock_fd.recv(65535)
            self._data += data
            if len(self._data) >= sizeof(Nlmsghdr):
                break

    def read_message(self) -> bytes:
        if len(self._data) < sizeof(Nlmsghdr):
            self.read_data()
        hdr = Nlmsghdr.from_buffer_copy(self._data)
        while hdr.nlmsg_len > len(self._data):
            self.read_data()
        raw_msg = self._data[: hdr.nlmsg_len]
        self._data = self._data[hdr.nlmsg_len :]
        return self.parse_message(raw_msg)

    def request_ifaces(self):
        msg = NetlinkIfaMessage(self.helper, NlRtMsgType.RTM_GETLINK.value)
        flags = NlmGetFlags.NLM_F_ROOT.value | NlmGetFlags.NLM_F_MATCH.value
        msg.nl_hdr.nlmsg_flags = flags | NlmBaseFlags.NLM_F_REQUEST.value

        msg_bytes = bytes(msg)
        x = self.parse_message(msg_bytes)
        x.print_message()
        print(msg_bytes)
        # Skip family for now
        self.write_data(msg_bytes)

    def request_ifaddrs(self, family):
        msg = NetlinkIfaMessage(self.helper, NlRtMsgType.RTM_GETADDR.value)
        flags = NlmGetFlags.NLM_F_ROOT.value | NlmGetFlags.NLM_F_MATCH.value
        msg.base_hdr.ifa_family = family
        msg.nl_hdr.nlmsg_flags = flags | NlmBaseFlags.NLM_F_REQUEST.value

        msg_bytes = bytes(msg)
        x = self.parse_message(msg_bytes)
        x.print_message()
        print(msg_bytes)
        # Skip family for now
        self.write_data(msg_bytes)

    def request_routes(self, family):
        msg = NetlinkRtMessage(self.helper, NlRtMsgType.RTM_GETROUTE.value)
        flags = NlmGetFlags.NLM_F_ROOT.value | NlmGetFlags.NLM_F_MATCH.value
        msg.base_hdr.rtm_family = family
        msg.nl_hdr.nlmsg_flags = flags | NlmBaseFlags.NLM_F_REQUEST.value

        msg_bytes = bytes(msg)
        x = self.parse_message(msg_bytes)
        x.print_message()
        print(msg_bytes)
        # Skip family for now
        self.write_data(msg_bytes)

    def request_raw(self):
        flags = NlmGetFlags.NLM_F_ROOT.value | NlmGetFlags.NLM_F_MATCH.value
        hdr = Nlmsghdr(
            nlmsg_type=NlRtMsgType.RTM_GETROUTE.value,
            nlmsg_flags=flags | NlmBaseFlags.NLM_F_REQUEST.value,
            nlmsg_len=sizeof(Nlmsghdr) + sizeof(RtMsgHdr) + 4,
        )
        rthdr = RtMsgHdr()

        rta = RtAttr(rta_len=3, rta_type=RtattrType.RTA_OIF.value)

        msg_bytes = bytes(hdr) + bytes(rthdr) + bytes(rta)
        # x = self.parse_message(msg_bytes)
        # x.print_message()
        print(msg_bytes)
        self.write_data(msg_bytes)

    def request_families(self):
        hdr = Nlmsghdr(
            nlmsg_type=16,
            nlmsg_flags=NlmBaseFlags.NLM_F_REQUEST.value,
            nlmsg_len=sizeof(Nlmsghdr) + sizeof(GenlMsgHdr) + 4,
        )
        ghdr = GenlMsgHdr(cmd=3)

        rta = RtAttr(rta_len=3, rta_type=RtattrType.RTA_OIF)

        msg_bytes = bytes(hdr) + bytes(ghdr) + bytes(rta)
        x = self.parse_message(msg_bytes)
        x.print_message()
        print(msg_bytes)
        self.write_data(msg_bytes)


class NetlinkMultipartIterator(object):
    def __init__(self, obj, seq_number: int, msg_type):
        self._obj = obj
        self._seq = seq_number
        self._msg_type = msg_type

    def __iter__(self):
        return self

    def __next__(self):
        msg = self._obj.read_message()
        if self._seq != msg.nl_hdr.nlmsg_seq:
            raise ValueError("bad sequence number")
        if msg.is_type(NlMsgType.NLMSG_ERROR):
            raise ValueError(
                "error while handling multipart msg: {}".format(msg.error_code)
            )
        elif msg.is_type(NlMsgType.NLMSG_DONE):
            if msg.error_code == 0:
                raise StopIteration
            raise ValueError(
                "error listing some parts of the multipart msg: {}".format(
                    msg.error_code
                )
            )
        elif not msg.is_type(self._msg_type):
            raise ValueError("bad message type: {}".format(msg))
        return msg


class NetlinkTestTemplate(object):
    REQUIRED_MODULES = ["netlink"]

    def setup_netlink(self, netlink_family: NlConst):
        self.helper = NlHelper()
        self.nlsock = Nlsock(netlink_family, self.helper)

    def write_message(self, msg):
        print("")
        print("============= >> TX MESSAGE =============")
        msg.print_message()
        self.nlsock.write_data(bytes(msg))
        msg.print_as_bytes(bytes(msg), "-- DATA --")

    def read_message(self):
        msg = self.nlsock.read_message()
        print("")
        print("============= << RX MESSAGE =============")
        msg.print_message()
        return msg

    def get_reply(self, tx_msg):
        self.write_message(tx_msg)
        while True:
            rx_msg = self.read_message()
            if tx_msg.nl_hdr.nlmsg_seq == rx_msg.nl_hdr.nlmsg_seq:
                return rx_msg

    def read_msg_list(self, seq, msg_type):
        return list(NetlinkMultipartIterator(self, seq, msg_type))
