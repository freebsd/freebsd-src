#!/usr/local/bin/python3
import struct
from ctypes import sizeof
from typing import List

from atf_python.sys.netlink.attrs import NlAttr
from atf_python.sys.netlink.attrs import NlAttrNested
from atf_python.sys.netlink.base_headers import NlmBaseFlags
from atf_python.sys.netlink.base_headers import Nlmsghdr
from atf_python.sys.netlink.base_headers import NlMsgType
from atf_python.sys.netlink.utils import align4
from atf_python.sys.netlink.utils import enum_or_int


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
            nla_len, raw_nla_type = struct.unpack("@HH", data[off:off + 4])
            if nla_len + off > len(data):
                raise ValueError(
                    "attr length {} > than the remaining length {}".format(
                        nla_len, len(data) - off
                    )
                )
            nla_type = raw_nla_type & 0x3F
            if nla_type in attr_map:
                v = attr_map[nla_type]
                val = v["ad"].cls.from_bytes(data[off:off + nla_len], v["ad"].val)
                if "child" in v:
                    # nested
                    attrs, _ = self.parse_attrs(
                        data[off + 4:off + nla_len], v["child"]
                    )
                    val = NlAttrNested(v["ad"].val, attrs)
            else:
                # unknown attribute
                val = NlAttr(raw_nla_type, data[off + 4:off + nla_len])
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

    def print_base_header(self, hdr, prepend=""):
        pass

    def print_message(self):
        self.print_nl_header(self.nl_hdr)
        self.print_base_header(self.base_hdr, " ")
        for nla in self.nla_list:
            nla.print_attr("  ")
