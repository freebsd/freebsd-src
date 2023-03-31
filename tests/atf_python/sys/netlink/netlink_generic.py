#!/usr/local/bin/python3
from ctypes import sizeof
from enum import Enum

from atf_python.sys.netlink.attrs import NlAttrStr
from atf_python.sys.netlink.attrs import NlAttrU16
from atf_python.sys.netlink.attrs import NlAttrU32
from atf_python.sys.netlink.base_headers import GenlMsgHdr
from atf_python.sys.netlink.message import NlMsgCategory
from atf_python.sys.netlink.message import NlMsgProps
from atf_python.sys.netlink.message import StdNetlinkMessage
from atf_python.sys.netlink.utils import AttrDescr
from atf_python.sys.netlink.utils import prepare_attrs_map
from atf_python.sys.netlink.utils import enum_or_int


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


handler_classes = {
    GenlCtrlFamilyName: [NetlinkGenlCtrlMessage],
}
