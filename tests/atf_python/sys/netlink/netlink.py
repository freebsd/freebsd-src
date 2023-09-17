#!/usr/local/bin/python3
import os
import socket
import sys
from ctypes import c_int
from ctypes import c_ubyte
from ctypes import c_uint
from ctypes import c_ushort
from ctypes import sizeof
from ctypes import Structure
from enum import auto
from enum import Enum

from atf_python.sys.netlink.attrs import NlAttr
from atf_python.sys.netlink.attrs import NlAttrStr
from atf_python.sys.netlink.attrs import NlAttrU32
from atf_python.sys.netlink.base_headers import GenlMsgHdr
from atf_python.sys.netlink.base_headers import NlmBaseFlags
from atf_python.sys.netlink.base_headers import Nlmsghdr
from atf_python.sys.netlink.base_headers import NlMsgType
from atf_python.sys.netlink.message import BaseNetlinkMessage
from atf_python.sys.netlink.message import NlMsgCategory
from atf_python.sys.netlink.message import NlMsgProps
from atf_python.sys.netlink.message import StdNetlinkMessage
from atf_python.sys.netlink.netlink_generic import GenlCtrlAttrType
from atf_python.sys.netlink.netlink_generic import GenlCtrlMsgType
from atf_python.sys.netlink.netlink_generic import handler_classes as genl_classes
from atf_python.sys.netlink.netlink_route import handler_classes as rt_classes
from atf_python.sys.netlink.utils import align4
from atf_python.sys.netlink.utils import AttrDescr
from atf_python.sys.netlink.utils import build_propmap
from atf_python.sys.netlink.utils import enum_or_int
from atf_python.sys.netlink.utils import get_bitmask_map
from atf_python.sys.netlink.utils import NlConst
from atf_python.sys.netlink.utils import prepare_attrs_map


class SockaddrNl(Structure):
    _fields_ = [
        ("nl_len", c_ubyte),
        ("nl_family", c_ubyte),
        ("nl_pad", c_ushort),
        ("nl_pid", c_uint),
        ("nl_groups", c_uint),
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


class AddressFamilyLinux(Enum):
    AF_INET = socket.AF_INET
    AF_INET6 = socket.AF_INET6
    AF_NETLINK = 16


class AddressFamilyBsd(Enum):
    AF_INET = socket.AF_INET
    AF_INET6 = socket.AF_INET6
    AF_NETLINK = 38


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
            self._pmap[cls] = build_propmap(cls)
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

    def get_af_name(self, family):
        v = self.get_attr_byval(self._af_cls, family)
        if v is not None:
            return v
        return "af#{}".format(family)

    def get_af_value(self, family_str: str) -> int:
        propmap = self.get_name_propmap(self._af_cls)
        return propmap.get(family_str)

    def get_bitmask_str(self, cls, val):
        bmap = get_bitmask_map(self.get_propmap(cls), val)
        return ",".join([v for k, v in bmap.items()])

    @staticmethod
    def get_bitmask_str_uncached(cls, val):
        pmap = NlHelper.build_propmap(cls)
        bmap = NlHelper.get_bitmask_map(pmap, val)
        return ",".join([v for k, v in bmap.items()])


nldone_attrs = prepare_attrs_map([])

nlerr_attrs = prepare_attrs_map(
    [
        AttrDescr(NlErrattrType.NLMSGERR_ATTR_MSG, NlAttrStr),
        AttrDescr(NlErrattrType.NLMSGERR_ATTR_OFFS, NlAttrU32),
        AttrDescr(NlErrattrType.NLMSGERR_ATTR_COOKIE, NlAttr),
    ]
)


class NetlinkDoneMessage(StdNetlinkMessage):
    messages = [NlMsgProps(NlMsgType.NLMSG_DONE, NlMsgCategory.ACK)]
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
    messages = [NlMsgProps(NlMsgType.NLMSG_ERROR, NlMsgCategory.ACK)]
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
        hdr = errhdr.msg
        print(
            "{}len={}, type={}, flags={}(0x{:X}), seq={}, pid={}".format(
                prepend,
                hdr.nlmsg_len,
                "msg#{}".format(hdr.nlmsg_type),
                self.helper.get_bitmask_str(NlmBaseFlags, hdr.nlmsg_flags),
                hdr.nlmsg_flags,
                hdr.nlmsg_seq,
                hdr.nlmsg_pid,
            )
        )


core_classes = {
    "netlink_core": [
        NetlinkDoneMessage,
        NetlinkErrorMessage,
    ],
}


class Nlsock:
    HANDLER_CLASSES = [core_classes, rt_classes, genl_classes]

    def __init__(self, family, helper):
        self.helper = helper
        self.sock_fd = self._setup_netlink(family)
        self._sock_family = family
        self._data = bytes()
        self.msgmap = self.build_msgmap()
        self._family_map = {
            NlConst.GENL_ID_CTRL: "nlctrl",
        }

    def build_msgmap(self):
        handler_classes = {}
        for d in self.HANDLER_CLASSES:
            handler_classes.update(d)
        xmap = {}
        # 'family_name': [class.messages[MsgProps.msg],  ]
        for family_id, family_classes in handler_classes.items():
            xmap[family_id] = {}
            for cls in family_classes:
                for msg_props in cls.messages:
                    xmap[family_id][enum_or_int(msg_props.msg)] = cls
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

    def join_group(self, group_id: int):
        self.sock_fd.setsockopt(270, 1, group_id)

    def write_message(self, msg, verbose=True):
        if verbose:
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
        if hdr.nlmsg_type < 16:
            family_name = "netlink_core"
            nlmsg_type = hdr.nlmsg_type
        elif self._sock_family == NlConst.NETLINK_ROUTE:
            family_name = "netlink_route"
            nlmsg_type = hdr.nlmsg_type
        else:
            # Genetlink
            if len(data) < sizeof(Nlmsghdr) + sizeof(GenlMsgHdr):
                raise Exception("Short read from genl: {} bytes".format(len(data)))
            family_name = self._family_map.get(hdr.nlmsg_type, "")
            ghdr = GenlMsgHdr.from_buffer_copy(data[sizeof(Nlmsghdr):])
            nlmsg_type = ghdr.cmd
        cls = self.msgmap.get(family_name, {}).get(nlmsg_type)
        if not cls:
            cls = BaseNetlinkMessage
        return cls.from_bytes(self.helper, data)

    def get_genl_family_id(self, family_name):
        hdr = Nlmsghdr(
            nlmsg_type=NlConst.GENL_ID_CTRL,
            nlmsg_flags=NlmBaseFlags.NLM_F_REQUEST.value,
            nlmsg_seq=self.helper.get_seq(),
        )
        ghdr = GenlMsgHdr(cmd=GenlCtrlMsgType.CTRL_CMD_GETFAMILY.value)
        nla = NlAttrStr(GenlCtrlAttrType.CTRL_ATTR_FAMILY_NAME, family_name)
        hdr.nlmsg_len = sizeof(Nlmsghdr) + sizeof(GenlMsgHdr) + len(bytes(nla))

        msg_bytes = bytes(hdr) + bytes(ghdr) + bytes(nla)
        self.write_data(msg_bytes)
        while True:
            rx_msg = self.read_message()
            if hdr.nlmsg_seq == rx_msg.nl_hdr.nlmsg_seq:
                if rx_msg.is_type(NlMsgType.NLMSG_ERROR):
                    if rx_msg.error_code != 0:
                        raise ValueError("unable to get family {}".format(family_name))
                else:
                    family_id = rx_msg.get_nla(GenlCtrlAttrType.CTRL_ATTR_FAMILY_ID).u16
                    self._family_map[family_id] = family_name
                    return family_id
        raise ValueError("unable to get family {}".format(family_name))

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
        self._data = self._data[hdr.nlmsg_len:]
        return self.parse_message(raw_msg)

    def get_reply(self, tx_msg):
        self.write_message(tx_msg)
        while True:
            rx_msg = self.read_message()
            if tx_msg.nl_hdr.nlmsg_seq == rx_msg.nl_hdr.nlmsg_seq:
                return rx_msg


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

    def write_message(self, msg, silent=False):
        if not silent:
            print("")
            print("============= >> TX MESSAGE =============")
            msg.print_message()
            msg.print_as_bytes(bytes(msg), "-- DATA --")
        self.nlsock.write_data(bytes(msg))

    def read_message(self, silent=False):
        msg = self.nlsock.read_message()
        if not silent:
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
