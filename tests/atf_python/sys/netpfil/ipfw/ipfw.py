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

from atf_python.sys.netpfil.ipfw.ioctl import get3_classes
from atf_python.sys.netpfil.ipfw.ioctl import legacy_classes
from atf_python.sys.netpfil.ipfw.ioctl import set3_classes
from atf_python.sys.netpfil.ipfw.utils import AttrDescr
from atf_python.sys.netpfil.ipfw.utils import enum_from_int
from atf_python.sys.netpfil.ipfw.utils import enum_or_int
from atf_python.sys.netpfil.ipfw.utils import prepare_attrs_map


class DebugHeader(Structure):
    _fields_ = [
        ("cmd_type", c_ushort),
        ("spare1", c_ushort),
        ("opt_name", c_uint32),
        ("total_len", c_uint32),
        ("spare2", c_uint32),
    ]


class DebugType(Enum):
    DO_CMD = 1
    DO_SET3 = 2
    DO_GET3 = 3


class DebugIoReader(object):
    HANDLER_CLASSES = {
        DebugType.DO_CMD: legacy_classes,
        DebugType.DO_SET3: set3_classes,
        DebugType.DO_GET3: get3_classes,
    }

    def __init__(self, ipfw_path):
        self._msgmap = self.build_msgmap()
        self.ipfw_path = ipfw_path

    def build_msgmap(self):
        xmap = {}
        for debug_type, handler_classes in self.HANDLER_CLASSES.items():
            debug_type = enum_or_int(debug_type)
            if debug_type not in xmap:
                xmap[debug_type] = {}
            for handler_class in handler_classes:
                for msg in handler_class.messages:
                    xmap[debug_type][enum_or_int(msg)] = handler_class
        return xmap

    def print_obj_header(self, hdr):
        debug_type = "#{}".format(hdr.cmd_type)
        for _type in self.HANDLER_CLASSES.keys():
            if _type.value == hdr.cmd_type:
                debug_type = _type.name.lower()
                break
        print(
            "@@ record for {} len={} optname={}".format(
                debug_type, hdr.total_len, hdr.opt_name
            )
        )

    def parse_record(self, data):
        hdr = DebugHeader.from_buffer_copy(data[: sizeof(DebugHeader)])
        data = data[sizeof(DebugHeader) :]
        cls = self._msgmap[hdr.cmd_type].get(hdr.opt_name)
        if cls is not None:
            return cls.from_bytes(data)
        raise ValueError(
            "unsupported cmd_type={} opt_name={}".format(hdr.cmd_type, hdr.opt_name)
        )

    def get_record_from_stdin(self):
        data = sys.stdin.buffer.peek(sizeof(DebugHeader))
        if len(data) == 0:
            return None

        hdr = DebugHeader.from_buffer_copy(data)
        data = sys.stdin.buffer.read(hdr.total_len)
        return self.parse_record(data)

    def get_records_from_buffer(self, data):
        off = 0
        ret = []
        while off + sizeof(DebugHeader) <= len(data):
            hdr = DebugHeader.from_buffer_copy(data[off : off + sizeof(DebugHeader)])
            ret.append(self.parse_record(data[off : off + hdr.total_len]))
            off += hdr.total_len
        return ret

    def run_ipfw(self, cmd: str) -> bytes:
        args = [self.ipfw_path, "-xqn"] + cmd.split()
        r = subprocess.run(args, capture_output=True)
        return r.stdout

    def get_records(self, cmd: str):
        return self.get_records_from_buffer(self.run_ipfw(cmd))
