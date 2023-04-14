from ctypes import c_ubyte
from ctypes import c_uint
from ctypes import c_ushort
from ctypes import Structure
from enum import Enum


class Nlmsghdr(Structure):
    _fields_ = [
        ("nlmsg_len", c_uint),
        ("nlmsg_type", c_ushort),
        ("nlmsg_flags", c_ushort),
        ("nlmsg_seq", c_uint),
        ("nlmsg_pid", c_uint),
    ]


class Nlattr(Structure):
    _fields_ = [
        ("nla_len", c_ushort),
        ("nla_type", c_ushort),
    ]


class NlMsgType(Enum):
    NLMSG_NOOP = 1
    NLMSG_ERROR = 2
    NLMSG_DONE = 3
    NLMSG_OVERRUN = 4


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


class GenlMsgHdr(Structure):
    _fields_ = [
        ("cmd", c_ubyte),
        ("version", c_ubyte),
        ("reserved", c_ushort),
    ]
