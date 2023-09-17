#!/usr/local/bin/python3
import struct
from ctypes import sizeof
from enum import Enum
from typing import List
from typing import NamedTuple

from atf_python.sys.netlink.attrs import NlAttr
from atf_python.sys.netlink.attrs import NlAttrNested
from atf_python.sys.netlink.base_headers import NlmAckFlags
from atf_python.sys.netlink.base_headers import NlmNewFlags
from atf_python.sys.netlink.base_headers import NlmGetFlags
from atf_python.sys.netlink.base_headers import NlmDeleteFlags
from atf_python.sys.netlink.base_headers import NlmBaseFlags
from atf_python.sys.netlink.base_headers import Nlmsghdr
from atf_python.sys.netlink.base_headers import NlMsgType
from atf_python.sys.netlink.utils import align4
from atf_python.sys.netlink.utils import enum_or_int
from atf_python.sys.netlink.utils import get_bitmask_str


class NlMsgCategory(Enum):
    UNKNOWN = 0
    GET = 1
    NEW = 2
    DELETE = 3
    ACK = 4


class NlMsgProps(NamedTuple):
    msg: Enum
    category: NlMsgCategory


class BaseNetlinkMessage(object):
    def __init__(self, helper, nlmsg_type):
        self.nlmsg_type = enum_or_int(nlmsg_type)
        self.nla_list = []
        self._orig_data = None
        self.helper = helper
        self.nl_hdr = Nlmsghdr(
            nlmsg_type=self.nlmsg_type, nlmsg_seq=helper.get_seq(), nlmsg_pid=helper.pid
        )
        self.base_hdr = None

    def set_request(self, need_ack=True):
        self.add_nlflags([NlmBaseFlags.NLM_F_REQUEST])
        if need_ack:
            self.add_nlflags([NlmBaseFlags.NLM_F_ACK])

    def add_nlflags(self, flags: List):
        int_flags = 0
        for flag in flags:
            int_flags |= enum_or_int(flag)
        self.nl_hdr.nlmsg_flags |= int_flags

    def add_nla(self, nla):
        self.nla_list.append(nla)

    def _get_nla(self, nla_list, nla_type):
        nla_type_raw = enum_or_int(nla_type)
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
        nlmsg_type_raw = enum_or_int(nlmsg_type)
        return nlmsg_type_raw == self.nl_hdr.nlmsg_type

    def is_reply(self, hdr):
        return hdr.nlmsg_type == NlMsgType.NLMSG_ERROR.value

    @property
    def msg_name(self):
        return "msg#{}".format(self._get_msg_type())

    def _get_nl_category(self):
        if self.is_reply(self.nl_hdr):
            return NlMsgCategory.ACK
        return NlMsgCategory.UNKNOWN

    def get_nlm_flags_str(self):
        category = self._get_nl_category()
        flags = self.nl_hdr.nlmsg_flags

        if category == NlMsgCategory.UNKNOWN:
            return self.helper.get_bitmask_str(NlmBaseFlags, flags)
        elif category == NlMsgCategory.GET:
            flags_enum = NlmGetFlags
        elif category == NlMsgCategory.NEW:
            flags_enum = NlmNewFlags
        elif category == NlMsgCategory.DELETE:
            flags_enum = NlmDeleteFlags
        elif category == NlMsgCategory.ACK:
            flags_enum = NlmAckFlags
        return get_bitmask_str([NlmBaseFlags, flags_enum], flags)

    def print_nl_header(self, prepend=""):
        # len=44, type=RTM_DELROUTE, flags=NLM_F_REQUEST|NLM_F_ACK, seq=1641163704, pid=0  # noqa: E501
        hdr = self.nl_hdr
        print(
            "{}len={}, type={}, flags={}(0x{:X}), seq={}, pid={}".format(
                prepend,
                hdr.nlmsg_len,
                self.msg_name,
                self.get_nlm_flags_str(),
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
        self.print_nl_header()

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

    def parse_child(self, data: bytes, attr_key, attr_map):
        attrs, _ = self.parse_attrs(data, attr_map)
        return NlAttrNested(attr_key, attrs)

    def parse_child_array(self, data: bytes, attr_key, attr_map):
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
            nla_type = raw_nla_type & 0x3FFF
            val = self.parse_child(data[off + 4 : off + nla_len], nla_type, attr_map)
            ret.append(val)
            off += align4(nla_len)
        return NlAttrNested(attr_key, ret)

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
            nla_type = raw_nla_type & 0x3FFF
            if nla_type in attr_map:
                v = attr_map[nla_type]
                val = v["ad"].cls.from_bytes(data[off : off + nla_len], v["ad"].val)
                if "child" in v:
                    # nested
                    child_data = data[off + 4 : off + nla_len]
                    if v.get("is_array", False):
                        # Array of nested attributes
                        val = self.parse_child_array(
                            child_data, v["ad"].val, v["child"]
                        )
                    else:
                        val = self.parse_child(child_data, v["ad"].val, v["child"])
            else:
                # unknown attribute
                val = NlAttr(raw_nla_type, data[off + 4 : off + nla_len])
            ret.append(val)
            off += align4(nla_len)
        return ret, off

    def parse_nla_list(self, data: bytes) -> List[NlAttr]:
        return self.parse_attrs(data, self.nl_attrs_map)

    def __bytes__(self):
        ret = bytes()
        for nla in self.nla_list:
            ret += bytes(nla)
        ret = bytes(self.base_hdr) + ret
        self.nl_hdr.nlmsg_len = len(ret) + sizeof(Nlmsghdr)
        return bytes(self.nl_hdr) + ret

    def _get_msg_type(self):
        return self.nl_hdr.nlmsg_type

    @property
    def msg_props(self):
        msg_type = self._get_msg_type()
        for msg_props in self.messages:
            if msg_props.msg.value == msg_type:
                return msg_props
        return None

    @property
    def msg_name(self):
        msg_props = self.msg_props
        if msg_props is not None:
            return msg_props.msg.name
        return super().msg_name

    def print_base_header(self, hdr, prepend=""):
        pass

    def print_message(self):
        self.print_nl_header()
        self.print_base_header(self.base_hdr, " ")
        for nla in self.nla_list:
            nla.print_attr("  ")
