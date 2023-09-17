#!/usr/local/bin/python3
import os
import socket
import struct
import sys
from ctypes import c_byte
from ctypes import c_char
from ctypes import c_int
from ctypes import c_long
from ctypes import c_uint32
from ctypes import c_ulong
from ctypes import c_ushort
from ctypes import sizeof
from ctypes import Structure
from typing import Dict
from typing import List
from typing import Optional
from typing import Union


def roundup2(val: int, num: int) -> int:
    if val % num:
        return (val | (num - 1)) + 1
    else:
        return val


class RtSockException(OSError):
    pass


class RtConst:
    RTM_VERSION = 5
    ALIGN = sizeof(c_long)

    AF_INET = socket.AF_INET
    AF_INET6 = socket.AF_INET6
    AF_LINK = socket.AF_LINK

    RTA_DST = 0x1
    RTA_GATEWAY = 0x2
    RTA_NETMASK = 0x4
    RTA_GENMASK = 0x8
    RTA_IFP = 0x10
    RTA_IFA = 0x20
    RTA_AUTHOR = 0x40
    RTA_BRD = 0x80

    RTM_ADD = 1
    RTM_DELETE = 2
    RTM_CHANGE = 3
    RTM_GET = 4

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

    RTV_MTU = 0x1
    RTV_HOPCOUNT = 0x2
    RTV_EXPIRE = 0x4
    RTV_RPIPE = 0x8
    RTV_SPIPE = 0x10
    RTV_SSTHRESH = 0x20
    RTV_RTT = 0x40
    RTV_RTTVAR = 0x80
    RTV_WEIGHT = 0x100

    @staticmethod
    def get_props(prefix: str) -> List[str]:
        return [n for n in dir(RtConst) if n.startswith(prefix)]

    @staticmethod
    def get_name(prefix: str, value: int) -> str:
        props = RtConst.get_props(prefix)
        for prop in props:
            if getattr(RtConst, prop) == value:
                return prop
        return "U:{}:{}".format(prefix, value)

    @staticmethod
    def get_bitmask_map(prefix: str, value: int) -> Dict[int, str]:
        props = RtConst.get_props(prefix)
        propmap = {getattr(RtConst, prop): prop for prop in props}
        v = 1
        ret = {}
        while value:
            if v & value:
                if v in propmap:
                    ret[v] = propmap[v]
                else:
                    ret[v] = hex(v)
                value -= v
            v *= 2
        return ret

    @staticmethod
    def get_bitmask_str(prefix: str, value: int) -> str:
        bmap = RtConst.get_bitmask_map(prefix, value)
        return ",".join([v for k, v in bmap.items()])


class RtMetrics(Structure):
    _fields_ = [
        ("rmx_locks", c_ulong),
        ("rmx_mtu", c_ulong),
        ("rmx_hopcount", c_ulong),
        ("rmx_expire", c_ulong),
        ("rmx_recvpipe", c_ulong),
        ("rmx_sendpipe", c_ulong),
        ("rmx_ssthresh", c_ulong),
        ("rmx_rtt", c_ulong),
        ("rmx_rttvar", c_ulong),
        ("rmx_pksent", c_ulong),
        ("rmx_weight", c_ulong),
        ("rmx_nhidx", c_ulong),
        ("rmx_filler", c_ulong * 2),
    ]


class RtMsgHdr(Structure):
    _fields_ = [
        ("rtm_msglen", c_ushort),
        ("rtm_version", c_byte),
        ("rtm_type", c_byte),
        ("rtm_index", c_ushort),
        ("_rtm_spare1", c_ushort),
        ("rtm_flags", c_int),
        ("rtm_addrs", c_int),
        ("rtm_pid", c_int),
        ("rtm_seq", c_int),
        ("rtm_errno", c_int),
        ("rtm_fmask", c_int),
        ("rtm_inits", c_ulong),
        ("rtm_rmx", RtMetrics),
    ]


class SockaddrIn(Structure):
    _fields_ = [
        ("sin_len", c_byte),
        ("sin_family", c_byte),
        ("sin_port", c_ushort),
        ("sin_addr", c_uint32),
        ("sin_zero", c_char * 8),
    ]


class SockaddrIn6(Structure):
    _fields_ = [
        ("sin6_len", c_byte),
        ("sin6_family", c_byte),
        ("sin6_port", c_ushort),
        ("sin6_flowinfo", c_uint32),
        ("sin6_addr", c_byte * 16),
        ("sin6_scope_id", c_uint32),
    ]


class SockaddrDl(Structure):
    _fields_ = [
        ("sdl_len", c_byte),
        ("sdl_family", c_byte),
        ("sdl_index", c_ushort),
        ("sdl_type", c_byte),
        ("sdl_nlen", c_byte),
        ("sdl_alen", c_byte),
        ("sdl_slen", c_byte),
        ("sdl_data", c_byte * 8),
    ]


class SaHelper(object):
    @staticmethod
    def is_ipv6(ip: str) -> bool:
        return ":" in ip

    @staticmethod
    def ip_sa(ip: str, scopeid: int = 0) -> bytes:
        if SaHelper.is_ipv6(ip):
            return SaHelper.ip6_sa(ip, scopeid)
        else:
            return SaHelper.ip4_sa(ip)

    @staticmethod
    def ip4_sa(ip: str) -> bytes:
        addr_int = int.from_bytes(socket.inet_pton(2, ip), sys.byteorder)
        sin = SockaddrIn(sizeof(SockaddrIn), socket.AF_INET, 0, addr_int)
        return bytes(sin)

    @staticmethod
    def ip6_sa(ip6: str, scopeid: int) -> bytes:
        addr_bytes = (c_byte * 16)()
        for i, b in enumerate(socket.inet_pton(socket.AF_INET6, ip6)):
            addr_bytes[i] = b
        sin6 = SockaddrIn6(
            sizeof(SockaddrIn6), socket.AF_INET6, 0, 0, addr_bytes, scopeid
        )
        return bytes(sin6)

    @staticmethod
    def link_sa(ifindex: int = 0, iftype: int = 0) -> bytes:
        sa = SockaddrDl(sizeof(SockaddrDl), socket.AF_LINK, c_ushort(ifindex), iftype)
        return bytes(sa)

    @staticmethod
    def pxlen4_sa(pxlen: int) -> bytes:
        return SaHelper.ip_sa(SaHelper.pxlen_to_ip4(pxlen))

    @staticmethod
    def pxlen_to_ip4(pxlen: int) -> str:
        if pxlen == 32:
            return "255.255.255.255"
        else:
            addr = 0xFFFFFFFF - ((1 << (32 - pxlen)) - 1)
            addr_bytes = struct.pack("!I", addr)
            return socket.inet_ntop(socket.AF_INET, addr_bytes)

    @staticmethod
    def pxlen6_sa(pxlen: int) -> bytes:
        return SaHelper.ip_sa(SaHelper.pxlen_to_ip6(pxlen))

    @staticmethod
    def pxlen_to_ip6(pxlen: int) -> str:
        ip6_b = [0] * 16
        start = 0
        while pxlen > 8:
            ip6_b[start] = 0xFF
            pxlen -= 8
            start += 1
        ip6_b[start] = 0xFF - ((1 << (8 - pxlen)) - 1)
        return socket.inet_ntop(socket.AF_INET6, bytes(ip6_b))

    @staticmethod
    def print_sa_inet(sa: bytes):
        if len(sa) < 8:
            raise RtSockException("IPv4 sa size too small: {}".format(len(sa)))
        addr = socket.inet_ntop(socket.AF_INET, sa[4:8])
        return "{}".format(addr)

    @staticmethod
    def print_sa_inet6(sa: bytes):
        if len(sa) < sizeof(SockaddrIn6):
            raise RtSockException("IPv6 sa size too small: {}".format(len(sa)))
        addr = socket.inet_ntop(socket.AF_INET6, sa[8:24])
        scopeid = struct.unpack(">I", sa[24:28])[0]
        return "{} scopeid {}".format(addr, scopeid)

    @staticmethod
    def print_sa_link(sa: bytes, hd: Optional[bool] = True):
        if len(sa) < sizeof(SockaddrDl):
            raise RtSockException("LINK sa size too small: {}".format(len(sa)))
        sdl = SockaddrDl.from_buffer_copy(sa)
        if sdl.sdl_index:
            ifindex = "link#{} ".format(sdl.sdl_index)
        else:
            ifindex = ""
        if sdl.sdl_nlen:
            iface_offset = 8
            if sdl.sdl_nlen + iface_offset > len(sa):
                raise RtSockException(
                    "LINK sa sdl_nlen {} > total len {}".format(sdl.sdl_nlen, len(sa))
                )
            ifname = "ifname:{} ".format(
                bytes.decode(sa[iface_offset : iface_offset + sdl.sdl_nlen])
            )
        else:
            ifname = ""
        return "{}{}".format(ifindex, ifname)

    @staticmethod
    def print_sa_unknown(sa: bytes):
        return "unknown_type:{}".format(sa[1])

    @classmethod
    def print_sa(cls, sa: bytes, hd: Optional[bool] = False):
        if sa[0] != len(sa):
            raise Exception("sa size {} != buffer size {}".format(sa[0], len(sa)))

        if len(sa) < 2:
            raise Exception(
                "sa type {} too short: {}".format(
                    RtConst.get_name("AF_", sa[1]), len(sa)
                )
            )

        if sa[1] == socket.AF_INET:
            text = cls.print_sa_inet(sa)
        elif sa[1] == socket.AF_INET6:
            text = cls.print_sa_inet6(sa)
        elif sa[1] == socket.AF_LINK:
            text = cls.print_sa_link(sa)
        else:
            text = cls.print_sa_unknown(sa)
        if hd:
            dump = " [{!r}]".format(sa)
        else:
            dump = ""
        return "{}{}".format(text, dump)


class BaseRtsockMessage(object):
    def __init__(self, rtm_type):
        self.rtm_type = rtm_type
        self.sa = SaHelper()

    @staticmethod
    def print_rtm_type(rtm_type):
        return RtConst.get_name("RTM_", rtm_type)

    @property
    def rtm_type_str(self):
        return self.print_rtm_type(self.rtm_type)


class RtsockRtMessage(BaseRtsockMessage):
    messages = [
        RtConst.RTM_ADD,
        RtConst.RTM_DELETE,
        RtConst.RTM_CHANGE,
        RtConst.RTM_GET,
    ]

    def __init__(self, rtm_type, rtm_seq=1, dst_sa=None, mask_sa=None):
        super().__init__(rtm_type)
        self.rtm_flags = 0
        self.rtm_seq = rtm_seq
        self._attrs = {}
        self.rtm_errno = 0
        self.rtm_pid = 0
        self.rtm_inits = 0
        self.rtm_rmx = RtMetrics()
        self._orig_data = None
        if dst_sa:
            self.add_sa_attr(RtConst.RTA_DST, dst_sa)
        if mask_sa:
            self.add_sa_attr(RtConst.RTA_NETMASK, mask_sa)

    def add_sa_attr(self, attr_type, attr_bytes: bytes):
        self._attrs[attr_type] = attr_bytes

    def add_ip_attr(self, attr_type, ip_addr: str, scopeid: int = 0):
        if ":" in ip_addr:
            self.add_ip6_attr(attr_type, ip_addr, scopeid)
        else:
            self.add_ip4_attr(attr_type, ip_addr)

    def add_ip4_attr(self, attr_type, ip: str):
        self.add_sa_attr(attr_type, self.sa.ip_sa(ip))

    def add_ip6_attr(self, attr_type, ip6: str, scopeid: int):
        self.add_sa_attr(attr_type, self.sa.ip6_sa(ip6, scopeid))

    def add_link_attr(self, attr_type, ifindex: Optional[int] = 0):
        self.add_sa_attr(attr_type, self.sa.link_sa(ifindex))

    def get_sa(self, attr_type) -> bytes:
        return self._attrs.get(attr_type)

    def print_message(self):
        # RTM_GET: Report Metrics: len 272, pid: 87839, seq 1, errno 0, flags:<UP,GATEWAY,DONE,STATIC>
        if self._orig_data:
            rtm_len = len(self._orig_data)
        else:
            rtm_len = len(bytes(self))
        print(
            "{}: len {}, pid: {}, seq {}, errno {}, flags: <{}>".format(
                self.rtm_type_str,
                rtm_len,
                self.rtm_pid,
                self.rtm_seq,
                self.rtm_errno,
                RtConst.get_bitmask_str("RTF_", self.rtm_flags),
            )
        )
        rtm_addrs = sum(list(self._attrs.keys()))
        print("Addrs: <{}>".format(RtConst.get_bitmask_str("RTA_", rtm_addrs)))
        for attr in sorted(self._attrs.keys()):
            sa_data = SaHelper.print_sa(self._attrs[attr])
            print(" {}: {}".format(RtConst.get_name("RTA_", attr), sa_data))

    def print_in_message(self):
        print("vvvvvvvv  IN vvvvvvvv")
        self.print_message()
        print()

    def verify_sa_inet(self, sa_data):
        if len(sa_data) < 8:
            raise Exception("IPv4 sa size too small: {}".format(sa_data))
        if sa_data[0] > len(sa_data):
            raise Exception(
                "IPv4 sin_len too big: {} vs sa size {}: {}".format(
                    sa_data[0], len(sa_data), sa_data
                )
            )
        sin = SockaddrIn.from_buffer_copy(sa_data)
        assert sin.sin_port == 0
        assert sin.sin_zero == [0] * 8

    def compare_sa(self, sa_type, sa_data):
        if len(sa_data) < 4:
            sa_type_name = RtConst.get_name("RTA_", sa_type)
            raise Exception(
                "sa_len for type {} too short: {}".format(sa_type_name, len(sa_data))
            )
        our_sa = self._attrs[sa_type]
        assert SaHelper.print_sa(sa_data) == SaHelper.print_sa(our_sa)
        assert len(sa_data) == len(our_sa)
        assert sa_data == our_sa

    def verify(self, rtm_type: int, rtm_sa):
        assert self.rtm_type_str == self.print_rtm_type(rtm_type)
        assert self.rtm_errno == 0
        hdr = RtMsgHdr.from_buffer_copy(self._orig_data)
        assert hdr._rtm_spare1 == 0
        for sa_type, sa_data in rtm_sa.items():
            if sa_type not in self._attrs:
                sa_type_name = RtConst.get_name("RTA_", sa_type)
                raise Exception("SA type {} not present".format(sa_type_name))
            self.compare_sa(sa_type, sa_data)

    @classmethod
    def from_bytes(cls, data: bytes):
        if len(data) < sizeof(RtMsgHdr):
            raise Exception(
                "messages size {} is less than expected {}".format(
                    len(data), sizeof(RtMsgHdr)
                )
            )
        hdr = RtMsgHdr.from_buffer_copy(data)

        self = cls(hdr.rtm_type)
        self.rtm_flags = hdr.rtm_flags
        self.rtm_seq = hdr.rtm_seq
        self.rtm_errno = hdr.rtm_errno
        self.rtm_pid = hdr.rtm_pid
        self.rtm_inits = hdr.rtm_inits
        self.rtm_rmx = hdr.rtm_rmx
        self._orig_data = data

        off = sizeof(RtMsgHdr)
        v = 1
        addrs_mask = hdr.rtm_addrs
        while addrs_mask:
            if addrs_mask & v:
                addrs_mask -= v

                if off + data[off] > len(data):
                    raise Exception(
                        "SA sizeof for {} > total message length: {}+{} > {}".format(
                            RtConst.get_name("RTA_", v), off, data[off], len(data)
                        )
                    )
                self._attrs[v] = data[off : off + data[off]]
                off += roundup2(data[off], RtConst.ALIGN)
            v *= 2
        return self

    def __bytes__(self):
        sz = sizeof(RtMsgHdr)
        addrs_mask = 0
        for k, v in self._attrs.items():
            sz += roundup2(len(v), RtConst.ALIGN)
            addrs_mask += k
        hdr = RtMsgHdr(
            rtm_msglen=sz,
            rtm_version=RtConst.RTM_VERSION,
            rtm_type=self.rtm_type,
            rtm_flags=self.rtm_flags,
            rtm_seq=self.rtm_seq,
            rtm_addrs=addrs_mask,
            rtm_inits=self.rtm_inits,
            rtm_rmx=self.rtm_rmx,
        )
        buf = bytearray(sz)
        buf[0 : sizeof(RtMsgHdr)] = hdr
        off = sizeof(RtMsgHdr)
        for attr in sorted(self._attrs.keys()):
            v = self._attrs[attr]
            sa_len = len(v)
            buf[off : off + sa_len] = v
            off += roundup2(len(v), RtConst.ALIGN)
        return bytes(buf)


class Rtsock:
    def __init__(self):
        self.socket = self._setup_rtsock()
        self.rtm_seq = 1
        self.msgmap = self.build_msgmap()

    def build_msgmap(self):
        classes = [RtsockRtMessage]
        xmap = {}
        for cls in classes:
            for message in cls.messages:
                xmap[message] = cls
        return xmap

    def get_seq(self):
        ret = self.rtm_seq
        self.rtm_seq += 1
        return ret

    def get_weight(self, weight) -> int:
        if weight:
            return weight
        else:
            return 1  # RT_DEFAULT_WEIGHT

    def new_rtm_any(self, msg_type, prefix: str, gw: Union[str, bytes]):
        px = prefix.split("/")
        addr_sa = SaHelper.ip_sa(px[0])
        if len(px) > 1:
            pxlen = int(px[1])
            if SaHelper.is_ipv6(px[0]):
                mask_sa = SaHelper.pxlen6_sa(pxlen)
            else:
                mask_sa = SaHelper.pxlen4_sa(pxlen)
        else:
            mask_sa = None
        msg = RtsockRtMessage(msg_type, self.get_seq(), addr_sa, mask_sa)
        if isinstance(gw, bytes):
            msg.add_sa_attr(RtConst.RTA_GATEWAY, gw)
        else:
            # String
            msg.add_ip_attr(RtConst.RTA_GATEWAY, gw)
        return msg

    def new_rtm_add(self, prefix: str, gw: Union[str, bytes]):
        return self.new_rtm_any(RtConst.RTM_ADD, prefix, gw)

    def new_rtm_del(self, prefix: str, gw: Union[str, bytes]):
        return self.new_rtm_any(RtConst.RTM_DELETE, prefix, gw)

    def new_rtm_change(self, prefix: str, gw: Union[str, bytes]):
        return self.new_rtm_any(RtConst.RTM_CHANGE, prefix, gw)

    def _setup_rtsock(self) -> socket.socket:
        s = socket.socket(socket.AF_ROUTE, socket.SOCK_RAW, socket.AF_UNSPEC)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_USELOOPBACK, 1)
        return s

    def print_hd(self, data: bytes):
        width = 16
        print("==========================================")
        for chunk in [data[i : i + width] for i in range(0, len(data), width)]:
            for b in chunk:
                print("0x{:02X} ".format(b), end="")
            print()
        print()

    def write_message(self, msg):
        print("vvvvvvvv OUT vvvvvvvv")
        msg.print_message()
        print()
        msg_bytes = bytes(msg)
        ret = os.write(self.socket.fileno(), msg_bytes)
        if ret != -1:
            assert ret == len(msg_bytes)

    def parse_message(self, data: bytes):
        if len(data) < 4:
            raise OSError("Short read from rtsock: {} bytes".format(len(data)))
        rtm_type = data[4]
        if rtm_type not in self.msgmap:
            return None

    def write_data(self, data: bytes):
        self.socket.send(data)

    def read_data(self, seq: Optional[int] = None) -> bytes:
        while True:
            data = self.socket.recv(4096)
            if seq is None:
                break
            if len(data) > sizeof(RtMsgHdr):
                hdr = RtMsgHdr.from_buffer_copy(data)
                if hdr.rtm_seq == seq:
                    break
        return data

    def read_message(self) -> bytes:
        data = self.read_data()
        return self.parse_message(data)
