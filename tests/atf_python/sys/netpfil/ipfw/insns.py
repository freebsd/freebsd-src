#!/usr/bin/env python3
import os
import socket
import struct
import subprocess
import sys
from ctypes import c_byte
from ctypes import c_char
from ctypes import c_int
from ctypes import c_long
from ctypes import c_uint32
from ctypes import c_uint8
from ctypes import c_ulong
from ctypes import c_ushort
from ctypes import sizeof
from ctypes import Structure
from enum import Enum
from typing import Any
from typing import Dict
from typing import List
from typing import NamedTuple
from typing import Optional
from typing import Union

from atf_python.sys.netpfil.ipfw.insn_headers import IpFwOpcode
from atf_python.sys.netpfil.ipfw.insn_headers import IcmpRejectCode
from atf_python.sys.netpfil.ipfw.insn_headers import Icmp6RejectCode
from atf_python.sys.netpfil.ipfw.utils import AttrDescr
from atf_python.sys.netpfil.ipfw.utils import enum_or_int
from atf_python.sys.netpfil.ipfw.utils import enum_from_int
from atf_python.sys.netpfil.ipfw.utils import prepare_attrs_map


insn_actions = (
    IpFwOpcode.O_CHECK_STATE.value,
    IpFwOpcode.O_REJECT.value,
    IpFwOpcode.O_UNREACH6.value,
    IpFwOpcode.O_ACCEPT.value,
    IpFwOpcode.O_DENY.value,
    IpFwOpcode.O_COUNT.value,
    IpFwOpcode.O_NAT.value,
    IpFwOpcode.O_QUEUE.value,
    IpFwOpcode.O_PIPE.value,
    IpFwOpcode.O_SKIPTO.value,
    IpFwOpcode.O_NETGRAPH.value,
    IpFwOpcode.O_NGTEE.value,
    IpFwOpcode.O_DIVERT.value,
    IpFwOpcode.O_TEE.value,
    IpFwOpcode.O_CALLRETURN.value,
    IpFwOpcode.O_FORWARD_IP.value,
    IpFwOpcode.O_FORWARD_IP6.value,
    IpFwOpcode.O_SETFIB.value,
    IpFwOpcode.O_SETDSCP.value,
    IpFwOpcode.O_REASS.value,
    IpFwOpcode.O_SETMARK.value,
    IpFwOpcode.O_EXTERNAL_ACTION.value,
)


class IpFwInsn(Structure):
    _fields_ = [
        ("opcode", c_uint8),
        ("length", c_uint8),
        ("arg1", c_ushort),
    ]


class BaseInsn(object):
    obj_enum_class = IpFwOpcode

    def __init__(self, opcode, is_or, is_not, arg1):
        if isinstance(opcode, Enum):
            self.obj_type = opcode.value
            self._enum = opcode
        else:
            self.obj_type = opcode
            self._enum = enum_from_int(self.obj_enum_class, self.obj_type)
        self.is_or = is_or
        self.is_not = is_not
        self.arg1 = arg1
        self.is_action = self.obj_type in insn_actions
        self.ilen = 1
        self.obj_list = []

    @property
    def obj_name(self):
        if self._enum is not None:
            return self._enum.name
        else:
            return "opcode#{}".format(self.obj_type)

    @staticmethod
    def get_insn_len(data: bytes) -> int:
        (opcode_len,) = struct.unpack("@B", data[1:2])
        return opcode_len & 0x3F

    @classmethod
    def _validate_len(cls, data, valid_options=None):
        if len(data) < 4:
            raise ValueError("opcode too short")
        opcode_type, opcode_len = struct.unpack("@BB", data[:2])
        if len(data) != ((opcode_len & 0x3F) * 4):
            raise ValueError("wrong length")
        if valid_options and len(data) not in valid_options:
            raise ValueError(
                "len {} not in {} for {}".format(
                    len(data), valid_options,
                    enum_from_int(cls.obj_enum_class, data[0])
                )
            )

    @classmethod
    def _validate(cls, data):
        cls._validate_len(data)

    @classmethod
    def _parse(cls, data):
        insn = IpFwInsn.from_buffer_copy(data[:4])
        is_or = (insn.length & 0x40) != 0
        is_not = (insn.length & 0x80) != 0
        return cls(opcode=insn.opcode, is_or=is_or, is_not=is_not, arg1=insn.arg1)

    @classmethod
    def from_bytes(cls, data, attr_type_enum):
        cls._validate(data)
        opcode = cls._parse(data)
        opcode._enum = attr_type_enum
        return opcode

    def __bytes__(self):
        raise NotImplementedError()

    def print_obj(self, prepend=""):
        is_or = ""
        if self.is_or:
            is_or = " [OR]\\"
        is_not = ""
        if self.is_not:
            is_not = "[!] "
        print(
            "{}{}len={} type={}({}){}{}".format(
                prepend,
                is_not,
                len(bytes(self)),
                self.obj_name,
                self.obj_type,
                self._print_obj_value(),
                is_or,
            )
        )

    def _print_obj_value(self):
        raise NotImplementedError()

    def print_obj_hex(self, prepend=""):
        print(prepend)
        print()
        print(" ".join(["x{:02X}".format(b) for b in bytes(self)]))

    @staticmethod
    def parse_insns(data, attr_map):
        ret = []
        off = 0
        while off + sizeof(IpFwInsn) <= len(data):
            hdr = IpFwInsn.from_buffer_copy(data[off : off + sizeof(IpFwInsn)])
            insn_len = (hdr.length & 0x3F) * 4
            if off + insn_len > len(data):
                raise ValueError("wrng length")
            # print("GET insn type {} len {}".format(hdr.opcode, insn_len))
            attr = attr_map.get(hdr.opcode, None)
            if attr is None:
                cls = InsnUnknown
                type_enum = enum_from_int(BaseInsn.obj_enum_class, hdr.opcode)
            else:
                cls = attr["ad"].cls
                type_enum = attr["ad"].val
            insn = cls.from_bytes(data[off : off + insn_len], type_enum)
            ret.append(insn)
            off += insn_len

        if off != len(data):
            raise ValueError("empty space")
        return ret


class Insn(BaseInsn):
    def __init__(self, opcode, is_or=False, is_not=False, arg1=0):
        super().__init__(opcode, is_or=is_or, is_not=is_not, arg1=arg1)

    @classmethod
    def _validate(cls, data):
        cls._validate_len(data, [4])

    def __bytes__(self):
        length = self.ilen
        if self.is_or:
            length |= 0x40
        if self.is_not:
            length | 0x80
        insn = IpFwInsn(opcode=self.obj_type, length=length, arg1=enum_or_int(self.arg1))
        return bytes(insn)

    def _print_obj_value(self):
        return " arg1={}".format(self.arg1)


class InsnUnknown(Insn):
    @classmethod
    def _validate(cls, data):
        cls._validate_len(data)

    @classmethod
    def _parse(cls, data):
        self = super()._parse(data)
        self._data = data
        return self

    def __bytes__(self):
        return self._data

    def _print_obj_value(self):
        return " " + " ".join(["x{:02X}".format(b) for b in self._data])


class InsnEmpty(Insn):
    @classmethod
    def _validate(cls, data):
        cls._validate_len(data, [4])
        insn = IpFwInsn.from_buffer_copy(data[:4])
        if insn.arg1 != 0:
            raise ValueError("arg1 should be empty")

    def _print_obj_value(self):
        return ""


class InsnComment(Insn):
    def __init__(self, opcode=IpFwOpcode.O_NOP, is_or=False, is_not=False, arg1=0, comment=""):
        super().__init__(opcode, is_or=is_or, is_not=is_not, arg1=arg1)
        if comment:
            self.comment = comment
        else:
            self.comment = ""

    @classmethod
    def _validate(cls, data):
        cls._validate_len(data)
        if len(data) > 88:
            raise ValueError("comment too long")

    @classmethod
    def _parse(cls, data):
        self = super()._parse(data)
        # Comment encoding can be anything,
        # use utf-8 to ease debugging
        max_len = 0
        for b in range(4, len(data)):
            if data[b] == b"\0":
                break
            max_len += 1
        self.comment = data[4:max_len].decode("utf-8")
        return self

    def __bytes__(self):
        ret = super().__bytes__()
        comment_bytes = self.comment.encode("utf-8") + b"\0"
        if len(comment_bytes) % 4 > 0:
            comment_bytes += b"\0" * (4 - (len(comment_bytes) % 4))
        ret += comment_bytes
        return ret

    def _print_obj_value(self):
        return " comment='{}'".format(self.comment)


class InsnProto(Insn):
    def __init__(self, opcode=IpFwOpcode.O_PROTO, is_or=False, is_not=False, arg1=0):
        super().__init__(opcode, is_or=is_or, is_not=is_not, arg1=arg1)

    def _print_obj_value(self):
        known_map = {6: "TCP", 17: "UDP", 41: "IPV6"}
        proto = self.arg1
        if proto in known_map:
            return " proto={}".format(known_map[proto])
        else:
            return " proto=#{}".format(proto)


class InsnU32(Insn):
    def __init__(self, opcode, is_or=False, is_not=False, arg1=0, u32=0):
        super().__init__(opcode, is_or=is_or, is_not=is_not, arg1=arg1)
        self.u32 = u32
        self.ilen = 2

    @classmethod
    def _validate(cls, data):
        cls._validate_len(data, [8])

    @classmethod
    def _parse(cls, data):
        self = super()._parse(data[:4])
        self.u32 = struct.unpack("@I", data[4:8])[0]
        return self

    def __bytes__(self):
        return super().__bytes__() + struct.pack("@I", self.u32)

    def _print_obj_value(self):
        return " arg1={} u32={}".format(self.arg1, self.u32)


class InsnProb(InsnU32):
    def __init__(
        self,
        opcode=IpFwOpcode.O_PROB,
        is_or=False,
        is_not=False,
        arg1=0,
        u32=0,
        prob=0.0,
    ):
        super().__init__(opcode, is_or=is_or, is_not=is_not)
        self.prob = prob

    @property
    def prob(self):
        return 1.0 * self.u32 / 0x7FFFFFFF

    @prob.setter
    def prob(self, prob: float):
        self.u32 = int(prob * 0x7FFFFFFF)

    def _print_obj_value(self):
        return " prob={}".format(round(self.prob, 5))


class InsnIp(InsnU32):
    def __init__(self, opcode, is_or=False, is_not=False, arg1=0, u32=0, ip=None):
        super().__init__(opcode, is_or=is_or, is_not=is_not, arg1=arg1, u32=u32)
        if ip:
            self.ip = ip

    @property
    def ip(self):
        return socket.inet_ntop(socket.AF_INET, struct.pack("@I", self.u32))

    @ip.setter
    def ip(self, ip: str):
        ip_bin = socket.inet_pton(socket.AF_INET, ip)
        self.u32 = struct.unpack("@I", ip_bin)[0]

    def _print_opcode_value(self):
        return " ip={}".format(self.ip)


class InsnTable(Insn):
    @classmethod
    def _validate(cls, data):
        cls._validate_len(data, [4, 8])

    @classmethod
    def _parse(cls, data):
        self = super()._parse(data)

        if len(data) == 8:
            (self.val,) = struct.unpack("@I", data[4:8])
            self.ilen = 2
        else:
            self.val = None
        return self

    def __bytes__(self):
        ret = super().__bytes__()
        if getattr(self, "val", None) is not None:
            ret += struct.pack("@I", self.val)
        return ret

    def _print_obj_value(self):
        if getattr(self, "val", None) is not None:
            return " table={} value={}".format(self.arg1, self.val)
        else:
            return " table={}".format(self.arg1)


class InsnReject(Insn):
    def __init__(self, opcode, is_or=False, is_not=False, arg1=0, mtu=None):
        super().__init__(opcode, is_or=is_or, is_not=is_not, arg1=arg1)
        self.mtu = mtu
        if self.mtu is not None:
            self.ilen = 2

    @classmethod
    def _validate(cls, data):
        cls._validate_len(data, [4, 8])

    @classmethod
    def _parse(cls, data):
        self = super()._parse(data)

        if len(data) == 8:
            (self.mtu,) = struct.unpack("@I", data[4:8])
            self.ilen = 2
        else:
            self.mtu = None
        return self

    def __bytes__(self):
        ret = super().__bytes__()
        if getattr(self, "mtu", None) is not None:
            ret += struct.pack("@I", self.mtu)
        return ret

    def _print_obj_value(self):
        code = enum_from_int(IcmpRejectCode, self.arg1)
        if getattr(self, "mtu", None) is not None:
            return " code={} mtu={}".format(code, self.mtu)
        else:
            return " code={}".format(code)


class InsnPorts(Insn):
    def __init__(self, opcode, is_or=False, is_not=False, arg1=0, port_pairs=[]):
        super().__init__(opcode, is_or=is_or, is_not=is_not)
        self.port_pairs = []
        if port_pairs:
            self.port_pairs = port_pairs

    @classmethod
    def _validate(cls, data):
        if len(data) < 8:
            raise ValueError("no ports specified")
        cls._validate_len(data)

    @classmethod
    def _parse(cls, data):
        self = super()._parse(data)

        off = 4
        port_pairs = []
        while off + 4 <= len(data):
            low, high = struct.unpack("@HH", data[off : off + 4])
            port_pairs.append((low, high))
            off += 4
        self.port_pairs = port_pairs
        return self

    def __bytes__(self):
        ret = super().__bytes__()
        if getattr(self, "val", None) is not None:
            ret += struct.pack("@I", self.val)
        return ret

    def _print_obj_value(self):
        ret = []
        for p in self.port_pairs:
            if p[0] == p[1]:
                ret.append(str(p[0]))
            else:
                ret.append("{}-{}".format(p[0], p[1]))
        return " ports={}".format(",".join(ret))


class IpFwInsnIp6(Structure):
    _fields_ = [
        ("o", IpFwInsn),
        ("addr6", c_byte * 16),
        ("mask6", c_byte * 16),
    ]


class InsnIp6(Insn):
    def __init__(self, opcode, is_or=False, is_not=False, arg1=0, ip6=None, mask6=None):
        super().__init__(opcode, is_or=is_or, is_not=is_not, arg1=arg1)
        self.ip6 = ip6
        self.mask6 = mask6
        if mask6 is not None:
            self.ilen = 9
        else:
            self.ilen = 5

    @classmethod
    def _validate(cls, data):
        cls._validate_len(data, [4 + 16, 4 + 16 * 2])

    @classmethod
    def _parse(cls, data):
        self = super()._parse(data)
        self.ip6 = socket.inet_ntop(socket.AF_INET6, data[4:20])

        if len(data) == 4 + 16 * 2:
            self.mask6 = socket.inet_ntop(socket.AF_INET6, data[20:36])
            self.ilen = 9
        else:
            self.mask6 = None
            self.ilen = 5
        return self

    def __bytes__(self):
        ret = super().__bytes__() + socket.inet_pton(socket.AF_INET6, self.ip6)
        if self.mask6 is not None:
            ret += socket.inet_pton(socket.AF_INET6, self.mask6)
        return ret

    def _print_obj_value(self):
        if self.mask6:
            return " ip6={}/{}".format(self.ip6, self.mask6)
        else:
            return " ip6={}".format(self.ip6)


insn_attrs = prepare_attrs_map(
    [
        AttrDescr(IpFwOpcode.O_CHECK_STATE, Insn),
        AttrDescr(IpFwOpcode.O_ACCEPT, InsnEmpty),
        AttrDescr(IpFwOpcode.O_COUNT, InsnEmpty),

        AttrDescr(IpFwOpcode.O_REJECT, InsnReject),
        AttrDescr(IpFwOpcode.O_UNREACH6, Insn),
        AttrDescr(IpFwOpcode.O_DENY, InsnEmpty),
        AttrDescr(IpFwOpcode.O_DIVERT, Insn),
        AttrDescr(IpFwOpcode.O_COUNT, InsnEmpty),
        AttrDescr(IpFwOpcode.O_QUEUE, Insn),
        AttrDescr(IpFwOpcode.O_PIPE, Insn),
        AttrDescr(IpFwOpcode.O_SKIPTO, Insn),
        AttrDescr(IpFwOpcode.O_NETGRAPH, Insn),
        AttrDescr(IpFwOpcode.O_NGTEE, Insn),
        AttrDescr(IpFwOpcode.O_DIVERT, Insn),
        AttrDescr(IpFwOpcode.O_TEE, Insn),
        AttrDescr(IpFwOpcode.O_CALLRETURN, Insn),
        AttrDescr(IpFwOpcode.O_SETFIB, Insn),
        AttrDescr(IpFwOpcode.O_SETDSCP, Insn),
        AttrDescr(IpFwOpcode.O_REASS, InsnEmpty),
        AttrDescr(IpFwOpcode.O_SETMARK, Insn),



        AttrDescr(IpFwOpcode.O_NOP, InsnComment),
        AttrDescr(IpFwOpcode.O_PROTO, InsnProto),
        AttrDescr(IpFwOpcode.O_PROB, InsnProb),
        AttrDescr(IpFwOpcode.O_IP_DST_ME, InsnEmpty),
        AttrDescr(IpFwOpcode.O_IP_SRC_ME, InsnEmpty),
        AttrDescr(IpFwOpcode.O_IP6_DST_ME, InsnEmpty),
        AttrDescr(IpFwOpcode.O_IP6_SRC_ME, InsnEmpty),
        AttrDescr(IpFwOpcode.O_IP_SRC, InsnIp),
        AttrDescr(IpFwOpcode.O_IP_DST, InsnIp),
        AttrDescr(IpFwOpcode.O_IP6_DST, InsnIp6),
        AttrDescr(IpFwOpcode.O_IP6_SRC, InsnIp6),
        AttrDescr(IpFwOpcode.O_IP_SRC_LOOKUP, InsnTable),
        AttrDescr(IpFwOpcode.O_IP_DST_LOOKUP, InsnTable),
        AttrDescr(IpFwOpcode.O_IP_SRCPORT, InsnPorts),
        AttrDescr(IpFwOpcode.O_IP_DSTPORT, InsnPorts),
        AttrDescr(IpFwOpcode.O_PROBE_STATE, Insn),
        AttrDescr(IpFwOpcode.O_KEEP_STATE, Insn),
    ]
)
