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

import pytest
from atf_python.sys.netpfil.ipfw.insns import BaseInsn
from atf_python.sys.netpfil.ipfw.insns import insn_attrs
from atf_python.sys.netpfil.ipfw.ioctl_headers import IpFwTableLookupType
from atf_python.sys.netpfil.ipfw.ioctl_headers import IpFwTlvType
from atf_python.sys.netpfil.ipfw.ioctl_headers import Op3CmdType
from atf_python.sys.netpfil.ipfw.utils import align8
from atf_python.sys.netpfil.ipfw.utils import AttrDescr
from atf_python.sys.netpfil.ipfw.utils import enum_from_int
from atf_python.sys.netpfil.ipfw.utils import prepare_attrs_map


class IpFw3OpHeader(Structure):
    _fields_ = [
        ("opcode", c_ushort),
        ("version", c_ushort),
        ("reserved1", c_ushort),
        ("reserved2", c_ushort),
    ]


class IpFwObjTlv(Structure):
    _fields_ = [
        ("n_type", c_ushort),
        ("flags", c_ushort),
        ("length", c_uint32),
    ]


class BaseTlv(object):
    obj_enum_class = IpFwTlvType

    def __init__(self, obj_type):
        if isinstance(obj_type, Enum):
            self.obj_type = obj_type.value
            self._enum = obj_type
        else:
            self.obj_type = obj_type
            self._enum = enum_from_int(self.obj_enum_class, obj_type)
        self.obj_list = []

    def add_obj(self, obj):
        self.obj_list.append(obj)

    @property
    def len(self):
        return len(bytes(self))

    @property
    def obj_name(self):
        if self._enum is not None:
            return self._enum.name
        else:
            return "tlv#{}".format(self.obj_type)

    def print_hdr(self, prepend=""):
        print(
            "{}len={} type={}({}){}".format(
                prepend, self.len, self.obj_name, self.obj_type, self._print_obj_value()
            )
        )

    def print_obj(self, prepend=""):
        self.print_hdr(prepend)
        prepend = "  " + prepend
        for obj in self.obj_list:
            obj.print_obj(prepend)

    def print_obj_hex(self, prepend=""):
        print(prepend)
        print()
        print(" ".join(["x{:02X}".format(b) for b in bytes(self)]))

    @classmethod
    def _validate(cls, data):
        if len(data) < sizeof(IpFwObjTlv):
            raise ValueError("TLV too short")
        hdr = IpFwObjTlv.from_buffer_copy(data[: sizeof(IpFwObjTlv)])
        if len(data) != hdr.length:
            raise ValueError("wrong TLV size")

    @classmethod
    def _parse(cls, data, attr_map):
        hdr = IpFwObjTlv.from_buffer_copy(data[: sizeof(IpFwObjTlv)])
        return cls(hdr.n_type)

    @classmethod
    def from_bytes(cls, data, attr_map=None):
        cls._validate(data)
        obj = cls._parse(data, attr_map)
        return obj

    def __bytes__(self):
        raise NotImplementedError()

    def _print_obj_value(self):
        return " " + " ".join(
            ["x{:02X}".format(b) for b in self._data[sizeof(IpFwObjTlv) :]]
        )

    def as_hexdump(self):
        return " ".join(["x{:02X}".format(b) for b in bytes(self)])


class UnknownTlv(BaseTlv):
    def __init__(self, obj_type, data):
        super().__init__(obj_type)
        self._data = data

    @classmethod
    def _validate(cls, data):
        if len(data) < sizeof(IpFwObjNTlv):
            raise ValueError("TLV size is too short")
        hdr = IpFwObjTlv.from_buffer_copy(data[: sizeof(IpFwObjTlv)])
        if len(data) != hdr.length:
            raise ValueError("wrong TLV size")

    @classmethod
    def _parse(cls, data, attr_map):
        hdr = IpFwObjTlv.from_buffer_copy(data[: sizeof(IpFwObjTlv)])
        self = cls(hdr.n_type, data)
        return self

    def __bytes__(self):
        return self._data


class Tlv(BaseTlv):
    @staticmethod
    def parse_tlvs(data, attr_map):
        # print("PARSING " + " ".join(["x{:02X}".format(b) for b in data]))
        off = 0
        ret = []
        while off + sizeof(IpFwObjTlv) <= len(data):
            hdr = IpFwObjTlv.from_buffer_copy(data[off : off + sizeof(IpFwObjTlv)])
            if off + hdr.length > len(data):
                raise ValueError("TLV size do not match")
            obj_data = data[off : off + hdr.length]
            obj_descr = attr_map.get(hdr.n_type, None)
            if obj_descr is None:
                # raise ValueError("unknown child TLV {}".format(hdr.n_type))
                cls = UnknownTlv
                child_map = {}
            else:
                cls = obj_descr["ad"].cls
                child_map = obj_descr.get("child", {})
            # print("FOUND OBJECT type {}".format(cls))
            # print()
            obj = cls.from_bytes(obj_data, child_map)
            ret.append(obj)
            off += hdr.length
        return ret


class IpFwObjNTlv(Structure):
    _fields_ = [
        ("head", IpFwObjTlv),
        ("idx", c_ushort),
        ("n_set", c_uint8),
        ("n_type", c_uint8),
        ("spare", c_uint32),
        ("name", c_char * 64),
    ]


class NTlv(Tlv):
    def __init__(self, obj_type, idx=0, n_set=0, n_type=0, name=None):
        super().__init__(obj_type)
        self.n_idx = idx
        self.n_set = n_set
        self.n_type = n_type
        self.n_name = name

    @classmethod
    def _validate(cls, data):
        if len(data) != sizeof(IpFwObjNTlv):
            raise ValueError("TLV size is not correct")
        hdr = IpFwObjTlv.from_buffer_copy(data[: sizeof(IpFwObjTlv)])
        if len(data) != hdr.length:
            raise ValueError("wrong TLV size")

    @classmethod
    def _parse(cls, data, attr_map):
        hdr = IpFwObjNTlv.from_buffer_copy(data[: sizeof(IpFwObjNTlv)])
        name = hdr.name.decode("utf-8")
        self = cls(hdr.head.n_type, hdr.idx, hdr.n_set, hdr.n_type, name)
        return self

    def __bytes__(self):
        name_bytes = self.n_name.encode("utf-8")
        if len(name_bytes) < 64:
            name_bytes += b"\0" * (64 - len(name_bytes))
        hdr = IpFwObjNTlv(
            head=IpFwObjTlv(n_type=self.obj_type, length=sizeof(IpFwObjNTlv)),
            idx=self.n_idx,
            n_set=self.n_set,
            n_type=self.n_type,
            name=name_bytes[:64],
        )
        return bytes(hdr)

    def _print_obj_value(self):
        return " " + "type={} set={} idx={} name={}".format(
            self.n_type, self.n_set, self.n_idx, self.n_name
        )


class IpFwObjCTlv(Structure):
    _fields_ = [
        ("head", IpFwObjTlv),
        ("count", c_uint32),
        ("objsize", c_ushort),
        ("version", c_uint8),
        ("flags", c_uint8),
    ]


class CTlv(Tlv):
    def __init__(self, obj_type, obj_list=[]):
        super().__init__(obj_type)
        if obj_list:
            self.obj_list.extend(obj_list)

    @classmethod
    def _validate(cls, data):
        if len(data) < sizeof(IpFwObjCTlv):
            raise ValueError("TLV too short")
        hdr = IpFwObjCTlv.from_buffer_copy(data[: sizeof(IpFwObjCTlv)])
        if len(data) != hdr.head.length:
            raise ValueError("wrong TLV size")

    @classmethod
    def _parse(cls, data, attr_map):
        hdr = IpFwObjCTlv.from_buffer_copy(data[: sizeof(IpFwObjCTlv)])
        tlv_list = cls.parse_tlvs(data[sizeof(IpFwObjCTlv) :], attr_map)
        if len(tlv_list) != hdr.count:
            raise ValueError("wrong number of objects")
        self = cls(hdr.head.n_type, obj_list=tlv_list)
        return self

    def __bytes__(self):
        ret = b""
        for obj in self.obj_list:
            ret += bytes(obj)
        length = len(ret) + sizeof(IpFwObjCTlv)
        if self.obj_list:
            objsize = len(bytes(self.obj_list[0]))
        else:
            objsize = 0
        hdr = IpFwObjCTlv(
            head=IpFwObjTlv(n_type=self.obj_type, length=sizeof(IpFwObjNTlv)),
            count=len(self.obj_list),
            objsize=objsize,
        )
        return bytes(hdr) + ret

    def _print_obj_value(self):
        return ""


class IpFwRule(Structure):
    _fields_ = [
        ("act_ofs", c_ushort),
        ("cmd_len", c_ushort),
        ("spare", c_ushort),
        ("n_set", c_uint8),
        ("flags", c_uint8),
        ("rulenum", c_uint32),
        ("n_id", c_uint32),
    ]


class RawRule(Tlv):
    def __init__(self, obj_type=0, n_set=0, rulenum=0, obj_list=[]):
        super().__init__(obj_type)
        self.n_set = n_set
        self.rulenum = rulenum
        if obj_list:
            self.obj_list.extend(obj_list)

    @classmethod
    def _validate(cls, data):
        min_size = sizeof(IpFwRule)
        if len(data) < min_size:
            raise ValueError("rule TLV too short")
        rule = IpFwRule.from_buffer_copy(data[:min_size])
        if len(data) != min_size + rule.cmd_len * 4:
            raise ValueError("rule TLV cmd_len incorrect")

    @classmethod
    def _parse(cls, data, attr_map):
        hdr = IpFwRule.from_buffer_copy(data[: sizeof(IpFwRule)])
        self = cls(
            n_set=hdr.n_set,
            rulenum=hdr.rulenum,
            obj_list=BaseInsn.parse_insns(data[sizeof(IpFwRule) :], insn_attrs),
        )
        return self

    def __bytes__(self):
        act_ofs = 0
        cmd_len = 0
        ret = b""
        for obj in self.obj_list:
            if obj.is_action and act_ofs == 0:
                act_ofs = cmd_len
            obj_bytes = bytes(obj)
            cmd_len += len(obj_bytes) // 4
            ret += obj_bytes

        hdr = IpFwRule(
            act_ofs=act_ofs,
            cmd_len=cmd_len,
            n_set=self.n_set,
            rulenum=self.rulenum,
        )
        return bytes(hdr) + ret

    @property
    def obj_name(self):
        return "rule#{}".format(self.rulenum)

    def _print_obj_value(self):
        cmd_len = sum([len(bytes(obj)) for obj in self.obj_list]) // 4
        return " set={} cmd_len={}".format(self.n_set, cmd_len)


class CTlvRule(CTlv):
    def __init__(self, obj_type=IpFwTlvType.IPFW_TLV_RULE_LIST, obj_list=[]):
        super().__init__(obj_type, obj_list)

    @classmethod
    def _parse(cls, data, attr_map):
        chdr = IpFwObjCTlv.from_buffer_copy(data[: sizeof(IpFwObjCTlv)])
        rule_list = []
        off = sizeof(IpFwObjCTlv)
        while off + sizeof(IpFwRule) <= len(data):
            hdr = IpFwRule.from_buffer_copy(data[off : off + sizeof(IpFwRule)])
            rule_len = sizeof(IpFwRule) + hdr.cmd_len * 4
            # print("FOUND RULE len={} cmd_len={}".format(rule_len, hdr.cmd_len))
            if off + rule_len > len(data):
                raise ValueError("wrong rule size")
            rule = RawRule.from_bytes(data[off : off + rule_len])
            rule_list.append(rule)
            off += align8(rule_len)
        if off != len(data):
            raise ValueError("rule bytes left: off={} len={}".format(off, len(data)))
        return cls(chdr.head.n_type, obj_list=rule_list)

    # XXX: _validate

    def __bytes__(self):
        ret = b""
        for rule in self.obj_list:
            rule_bytes = bytes(rule)
            remainder = len(rule_bytes) % 8
            if remainder > 0:
                rule_bytes += b"\0" * (8 - remainder)
            ret += rule_bytes
        hdr = IpFwObjCTlv(
            head=IpFwObjTlv(
                n_type=self.obj_type, length=len(ret) + sizeof(IpFwObjCTlv)
            ),
            count=len(self.obj_list),
        )
        return bytes(hdr) + ret


class BaseIpFwMessage(object):
    messages = []

    def __init__(self, msg_type, obj_list=[]):
        if isinstance(msg_type, Enum):
            self.obj_type = msg_type.value
            self._enum = msg_type
        else:
            self.obj_type = msg_type
            self._enum = enum_from_int(self.messages, self.obj_type)
        self.obj_list = []
        if obj_list:
            self.obj_list.extend(obj_list)

    def add_obj(self, obj):
        self.obj_list.append(obj)

    def get_obj(self, obj_type):
        obj_type_raw = enum_or_int(obj_type)
        for obj in self.obj_list:
            if obj.obj_type == obj_type_raw:
                return obj
        return None

    @staticmethod
    def parse_header(data: bytes):
        if len(data) < sizeof(IpFw3OpHeader):
            raise ValueError("length less than op3 message header")
        return IpFw3OpHeader.from_buffer_copy(data), sizeof(IpFw3OpHeader)

    def parse_obj_list(self, data: bytes):
        off = 0
        while off < len(data):
            # print("PARSE off={} rem={}".format(off, len(data) - off))
            hdr = IpFwObjTlv.from_buffer_copy(data[off : off + sizeof(IpFwObjTlv)])
            # print(" tlv len {}".format(hdr.length))
            if hdr.length + off > len(data):
                raise ValueError("TLV too big")
            tlv = Tlv(hdr.n_type, data[off : off + hdr.length])
            self.add_obj(tlv)
            off += hdr.length

    def is_type(self, msg_type):
        return enum_or_int(msg_type) == self.msg_type

    @property
    def obj_name(self):
        if self._enum is not None:
            return self._enum.name
        else:
            return "msg#{}".format(self.obj_type)

    def print_hdr(self, prepend=""):
        print("{}len={}, type={}".format(prepend, len(bytes(self)), self.obj_name))

    @classmethod
    def from_bytes(cls, data):
        try:
            hdr, hdrlen = cls.parse_header(data)
            self = cls(hdr.opcode)
            self._orig_data = data
        except ValueError as e:
            print("Failed to parse op3 header: {}".format(e))
            cls.print_as_bytes(data)
            raise
        tlv_list = Tlv.parse_tlvs(data[hdrlen:], self.attr_map)
        self.obj_list.extend(tlv_list)
        return self

    def __bytes__(self):
        ret = bytes(IpFw3OpHeader(opcode=self.obj_type))
        for obj in self.obj_list:
            ret += bytes(obj)
        return ret

    def print_obj(self):
        self.print_hdr()
        for obj in self.obj_list:
            obj.print_obj("  ")

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


rule_attrs = prepare_attrs_map(
    [
        AttrDescr(
            IpFwTlvType.IPFW_TLV_TBLNAME_LIST,
            CTlv,
            [
                AttrDescr(IpFwTlvType.IPFW_TLV_TBL_NAME, NTlv),
                AttrDescr(IpFwTlvType.IPFW_TLV_STATE_NAME, NTlv),
                AttrDescr(IpFwTlvType.IPFW_TLV_EACTION, NTlv),
            ],
            True,
        ),
        AttrDescr(IpFwTlvType.IPFW_TLV_RULE_LIST, CTlvRule),
    ]
)


class IpFwXRule(BaseIpFwMessage):
    messages = [Op3CmdType.IP_FW_XADD]
    attr_map = rule_attrs


legacy_classes = []
set3_classes = []
get3_classes = [IpFwXRule]
