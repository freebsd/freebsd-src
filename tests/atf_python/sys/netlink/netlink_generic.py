#!/usr/local/bin/python3
import struct
from ctypes import c_int64
from ctypes import c_long
from ctypes import sizeof
from ctypes import Structure
from enum import Enum

from atf_python.sys.netlink.attrs import NlAttr
from atf_python.sys.netlink.attrs import NlAttrIp4
from atf_python.sys.netlink.attrs import NlAttrIp6
from atf_python.sys.netlink.attrs import NlAttrS32
from atf_python.sys.netlink.attrs import NlAttrStr
from atf_python.sys.netlink.attrs import NlAttrU16
from atf_python.sys.netlink.attrs import NlAttrU32
from atf_python.sys.netlink.attrs import NlAttrU8
from atf_python.sys.netlink.base_headers import GenlMsgHdr
from atf_python.sys.netlink.message import NlMsgCategory
from atf_python.sys.netlink.message import NlMsgProps
from atf_python.sys.netlink.message import StdNetlinkMessage
from atf_python.sys.netlink.utils import AttrDescr
from atf_python.sys.netlink.utils import enum_or_int
from atf_python.sys.netlink.utils import prepare_attrs_map


class NetlinkGenlMessage(StdNetlinkMessage):
    messages = []
    nl_attrs_map = {}
    family_name = None

    def __init__(self, helper, family_id, cmd=0):
        super().__init__(helper, family_id)
        self.base_hdr = GenlMsgHdr(cmd=enum_or_int(cmd))

    def parse_base_header(self, data):
        if len(data) < sizeof(GenlMsgHdr):
            raise ValueError("length less than GenlMsgHdr header")
        ghdr = GenlMsgHdr.from_buffer_copy(data)
        return (ghdr, sizeof(GenlMsgHdr))

    def _get_msg_type(self):
        return self.base_hdr.cmd

    def print_nl_header(self, prepend=""):
        # len=44, type=RTM_DELROUTE, flags=NLM_F_REQUEST|NLM_F_ACK, seq=1641163704, pid=0  # noqa: E501
        hdr = self.nl_hdr
        print(
            "{}len={}, family={}, flags={}(0x{:X}), seq={}, pid={}".format(
                prepend,
                hdr.nlmsg_len,
                self.family_name,
                self.get_nlm_flags_str(),
                hdr.nlmsg_flags,
                hdr.nlmsg_seq,
                hdr.nlmsg_pid,
            )
        )

    def print_base_header(self, hdr, prepend=""):
        print(
            "{}cmd={} version={} reserved={}".format(
                prepend, self.msg_name, hdr.version, hdr.reserved
            )
        )


GenlCtrlFamilyName = "nlctrl"


class GenlCtrlMsgType(Enum):
    CTRL_CMD_UNSPEC = 0
    CTRL_CMD_NEWFAMILY = 1
    CTRL_CMD_DELFAMILY = 2
    CTRL_CMD_GETFAMILY = 3
    CTRL_CMD_NEWOPS = 4
    CTRL_CMD_DELOPS = 5
    CTRL_CMD_GETOPS = 6
    CTRL_CMD_NEWMCAST_GRP = 7
    CTRL_CMD_DELMCAST_GRP = 8
    CTRL_CMD_GETMCAST_GRP = 9
    CTRL_CMD_GETPOLICY = 10


class GenlCtrlAttrType(Enum):
    CTRL_ATTR_FAMILY_ID = 1
    CTRL_ATTR_FAMILY_NAME = 2
    CTRL_ATTR_VERSION = 3
    CTRL_ATTR_HDRSIZE = 4
    CTRL_ATTR_MAXATTR = 5
    CTRL_ATTR_OPS = 6
    CTRL_ATTR_MCAST_GROUPS = 7
    CTRL_ATTR_POLICY = 8
    CTRL_ATTR_OP_POLICY = 9
    CTRL_ATTR_OP = 10


genl_ctrl_attrs = prepare_attrs_map(
    [
        AttrDescr(GenlCtrlAttrType.CTRL_ATTR_FAMILY_ID, NlAttrU16),
        AttrDescr(GenlCtrlAttrType.CTRL_ATTR_FAMILY_NAME, NlAttrStr),
        AttrDescr(GenlCtrlAttrType.CTRL_ATTR_VERSION, NlAttrU32),
        AttrDescr(GenlCtrlAttrType.CTRL_ATTR_HDRSIZE, NlAttrU32),
        AttrDescr(GenlCtrlAttrType.CTRL_ATTR_MAXATTR, NlAttrU32),
    ]
)


class NetlinkGenlCtrlMessage(NetlinkGenlMessage):
    messages = [
        NlMsgProps(GenlCtrlMsgType.CTRL_CMD_NEWFAMILY, NlMsgCategory.NEW),
        NlMsgProps(GenlCtrlMsgType.CTRL_CMD_GETFAMILY, NlMsgCategory.GET),
        NlMsgProps(GenlCtrlMsgType.CTRL_CMD_DELFAMILY, NlMsgCategory.DELETE),
    ]
    nl_attrs_map = genl_ctrl_attrs
    family_name = GenlCtrlFamilyName


CarpFamilyName = "carp"


class CarpMsgType(Enum):
    CARP_NL_CMD_UNSPEC = 0
    CARP_NL_CMD_GET = 1
    CARP_NL_CMD_SET = 2


class CarpAttrType(Enum):
    CARP_NL_UNSPEC = 0
    CARP_NL_VHID = 1
    CARP_NL_STATE = 2
    CARP_NL_ADVBASE = 3
    CARP_NL_ADVSKEW = 4
    CARP_NL_KEY = 5
    CARP_NL_IFINDEX = 6
    CARP_NL_ADDR = 7
    CARP_NL_ADDR6 = 8
    CARP_NL_IFNAME = 9


carp_gen_attrs = prepare_attrs_map(
    [
        AttrDescr(CarpAttrType.CARP_NL_VHID, NlAttrU32),
        AttrDescr(CarpAttrType.CARP_NL_STATE, NlAttrU32),
        AttrDescr(CarpAttrType.CARP_NL_ADVBASE, NlAttrS32),
        AttrDescr(CarpAttrType.CARP_NL_ADVSKEW, NlAttrS32),
        AttrDescr(CarpAttrType.CARP_NL_KEY, NlAttr),
        AttrDescr(CarpAttrType.CARP_NL_IFINDEX, NlAttrU32),
        AttrDescr(CarpAttrType.CARP_NL_ADDR, NlAttrIp4),
        AttrDescr(CarpAttrType.CARP_NL_ADDR6, NlAttrIp6),
        AttrDescr(CarpAttrType.CARP_NL_IFNAME, NlAttrStr),
    ]
)


class CarpGenMessage(NetlinkGenlMessage):
    messages = [
        NlMsgProps(CarpMsgType.CARP_NL_CMD_GET, NlMsgCategory.GET),
        NlMsgProps(CarpMsgType.CARP_NL_CMD_SET, NlMsgCategory.NEW),
    ]
    nl_attrs_map = carp_gen_attrs
    family_name = CarpFamilyName


KtestFamilyName = "ktest"


class KtestMsgType(Enum):
    KTEST_CMD_UNSPEC = 0
    KTEST_CMD_LIST = 1
    KTEST_CMD_RUN = 2
    KTEST_CMD_NEWTEST = 3
    KTEST_CMD_NEWMESSAGE = 4


class KtestAttrType(Enum):
    KTEST_ATTR_MOD_NAME = 1
    KTEST_ATTR_TEST_NAME = 2
    KTEST_ATTR_TEST_DESCR = 3
    KTEST_ATTR_TEST_META = 4


class KtestLogMsgType(Enum):
    KTEST_MSG_START = 1
    KTEST_MSG_END = 2
    KTEST_MSG_LOG = 3
    KTEST_MSG_FAIL = 4


class KtestMsgAttrType(Enum):
    KTEST_MSG_ATTR_TS = 1
    KTEST_MSG_ATTR_FUNC = 2
    KTEST_MSG_ATTR_FILE = 3
    KTEST_MSG_ATTR_LINE = 4
    KTEST_MSG_ATTR_TEXT = 5
    KTEST_MSG_ATTR_LEVEL = 6
    KTEST_MSG_ATTR_META = 7


class timespec(Structure):
    _fields_ = [
        ("tv_sec", c_int64),
        ("tv_nsec", c_long),
    ]


class NlAttrTS(NlAttr):
    DATA_LEN = sizeof(timespec)

    def __init__(self, nla_type, val):
        self.ts = val
        super().__init__(nla_type, b"")

    @property
    def nla_len(self):
        return NlAttr.HDR_LEN + self.DATA_LEN

    def _print_attr_value(self):
        return " tv_sec={} tv_nsec={}".format(self.ts.tv_sec, self.ts.tv_nsec)

    @staticmethod
    def _validate(data):
        assert len(data) == NlAttr.HDR_LEN + NlAttrTS.DATA_LEN
        nla_len, nla_type = struct.unpack("@HH", data[:NlAttr.HDR_LEN])
        assert nla_len == NlAttr.HDR_LEN + NlAttrTS.DATA_LEN

    @classmethod
    def _parse(cls, data):
        nla_len, nla_type = struct.unpack("@HH", data[:NlAttr.HDR_LEN])
        val = timespec.from_buffer_copy(data[NlAttr.HDR_LEN:])
        return cls(nla_type, val)

    def __bytes__(self):
        return self._to_bytes(bytes(self.ts))


ktest_info_attrs = prepare_attrs_map(
    [
        AttrDescr(KtestAttrType.KTEST_ATTR_MOD_NAME, NlAttrStr),
        AttrDescr(KtestAttrType.KTEST_ATTR_TEST_NAME, NlAttrStr),
        AttrDescr(KtestAttrType.KTEST_ATTR_TEST_DESCR, NlAttrStr),
    ]
)


ktest_msg_attrs = prepare_attrs_map(
    [
        AttrDescr(KtestMsgAttrType.KTEST_MSG_ATTR_FUNC, NlAttrStr),
        AttrDescr(KtestMsgAttrType.KTEST_MSG_ATTR_FILE, NlAttrStr),
        AttrDescr(KtestMsgAttrType.KTEST_MSG_ATTR_LINE, NlAttrU32),
        AttrDescr(KtestMsgAttrType.KTEST_MSG_ATTR_TEXT, NlAttrStr),
        AttrDescr(KtestMsgAttrType.KTEST_MSG_ATTR_LEVEL, NlAttrU8),
        AttrDescr(KtestMsgAttrType.KTEST_MSG_ATTR_TS, NlAttrTS),
    ]
)


class KtestInfoMessage(NetlinkGenlMessage):
    messages = [
        NlMsgProps(KtestMsgType.KTEST_CMD_LIST, NlMsgCategory.GET),
        NlMsgProps(KtestMsgType.KTEST_CMD_RUN, NlMsgCategory.NEW),
        NlMsgProps(KtestMsgType.KTEST_CMD_NEWTEST, NlMsgCategory.NEW),
    ]
    nl_attrs_map = ktest_info_attrs
    family_name = KtestFamilyName


class KtestMsgMessage(NetlinkGenlMessage):
    messages = [
        NlMsgProps(KtestMsgType.KTEST_CMD_NEWMESSAGE, NlMsgCategory.NEW),
    ]
    nl_attrs_map = ktest_msg_attrs
    family_name = KtestFamilyName


handler_classes = {
    CarpFamilyName: [CarpGenMessage],
    GenlCtrlFamilyName: [NetlinkGenlCtrlMessage],
    KtestFamilyName: [KtestInfoMessage, KtestMsgMessage],
}
