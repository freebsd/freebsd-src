import errno
import json
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
from atf_python.sys.netpfil.ipfw.insns import Icmp6RejectCode
from atf_python.sys.netpfil.ipfw.insns import IcmpRejectCode
from atf_python.sys.netpfil.ipfw.insns import Insn
from atf_python.sys.netpfil.ipfw.insns import InsnComment
from atf_python.sys.netpfil.ipfw.insns import InsnEmpty
from atf_python.sys.netpfil.ipfw.insns import InsnIp
from atf_python.sys.netpfil.ipfw.insns import InsnIp6
from atf_python.sys.netpfil.ipfw.insns import InsnPorts
from atf_python.sys.netpfil.ipfw.insns import InsnProb
from atf_python.sys.netpfil.ipfw.insns import InsnProto
from atf_python.sys.netpfil.ipfw.insns import InsnReject
from atf_python.sys.netpfil.ipfw.insns import InsnTable
from atf_python.sys.netpfil.ipfw.insns import IpFwOpcode
from atf_python.sys.netpfil.ipfw.ioctl import CTlv
from atf_python.sys.netpfil.ipfw.ioctl import CTlvRule
from atf_python.sys.netpfil.ipfw.ioctl import IpFwTlvType
from atf_python.sys.netpfil.ipfw.ioctl import IpFwXRule
from atf_python.sys.netpfil.ipfw.ioctl import NTlv
from atf_python.sys.netpfil.ipfw.ioctl import Op3CmdType
from atf_python.sys.netpfil.ipfw.ioctl import RawRule
from atf_python.sys.netpfil.ipfw.ipfw import DebugIoReader
from atf_python.sys.netpfil.ipfw.utils import enum_from_int
from atf_python.utils import BaseTest


IPFW_PATH = "/sbin/ipfw"


def differ(w_obj, g_obj, w_stack=[], g_stack=[]):
    if bytes(w_obj) == bytes(g_obj):
        return True
    num_objects = 0
    for i, w_child in enumerate(w_obj.obj_list):
        if i >= len(g_obj.obj_list):
            print("MISSING object from chain {}".format(" / ".join(w_stack)))
            w_child.print_obj()
            print("==========================")
            return False
        g_child = g_obj.obj_list[i]
        if bytes(w_child) == bytes(g_child):
            num_objects += 1
            continue
        w_stack.append(w_obj.obj_name)
        g_stack.append(g_obj.obj_name)
        if not differ(w_child, g_child, w_stack, g_stack):
            return False
        break
    if num_objects == len(w_obj.obj_list) and num_objects < len(g_obj.obj_list):
        g_child = g_obj.obj_list[num_objects]
        print("EXTRA object from chain {}".format(" / ".join(g_stack)))
        g_child.print_obj()
        print("==========================")
        return False
    print("OBJECTS DIFFER")
    print("WANTED CHAIN: {}".format(" / ".join(w_stack)))
    w_obj.print_obj()
    w_obj.print_obj_hex()
    print("==========================")
    print("GOT CHAIN: {}".format(" / ".join(g_stack)))
    g_obj.print_obj()
    g_obj.print_obj_hex()
    print("==========================")
    return False


class TestAddRule(BaseTest):
    def compile_rule(self, out):
        tlvs = []
        if "objs" in out:
            tlvs.append(CTlv(IpFwTlvType.IPFW_TLV_TBLNAME_LIST, out["objs"]))
        rule = RawRule(rulenum=out.get("rulenum", 0), obj_list=out["insns"])
        tlvs.append(CTlvRule(obj_list=[rule]))
        return IpFwXRule(Op3CmdType.IP_FW_XADD, tlvs)

    def verify_rule(self, in_data: str, out_data):
        # Prepare the desired output
        expected = self.compile_rule(out_data)

        reader = DebugIoReader(IPFW_PATH)
        ioctls = reader.get_records(in_data)
        assert len(ioctls) == 1  # Only 1 ioctl request expected
        got = ioctls[0]

        if not differ(expected, got):
            print("=> CMD: {}".format(in_data))
            print("=> WANTED:")
            expected.print_obj()
            print("==========================")
            print("=> GOT:")
            got.print_obj()
            print("==========================")
        assert bytes(got) == bytes(expected)

    @pytest.mark.parametrize(
        "rule",
        [
            pytest.param(
                {
                    "in": "add 200 allow ip from any to any",
                    "out": {
                        "insns": [InsnEmpty(IpFwOpcode.O_ACCEPT)],
                        "rulenum": 200,
                    },
                },
                id="test_rulenum",
            ),
            pytest.param(
                {
                    "in": "add allow ip from { 1.2.3.4 or 2.3.4.5 } to any",
                    "out": {
                        "insns": [
                            InsnIp(IpFwOpcode.O_IP_SRC, ip="1.2.3.4", is_or=True),
                            InsnIp(IpFwOpcode.O_IP_SRC, ip="2.3.4.5"),
                            InsnEmpty(IpFwOpcode.O_ACCEPT),
                        ],
                    },
                },
                id="test_or",
            ),
            pytest.param(
                {
                    "in": "add allow ip from table(AAA) to table(BBB)",
                    "out": {
                        "objs": [
                            NTlv(IpFwTlvType.IPFW_TLV_TBL_NAME, idx=1, name="AAA"),
                            NTlv(IpFwTlvType.IPFW_TLV_TBL_NAME, idx=2, name="BBB"),
                        ],
                        "insns": [
                            InsnTable(IpFwOpcode.O_IP_SRC_LOOKUP, arg1=1),
                            InsnTable(IpFwOpcode.O_IP_DST_LOOKUP, arg1=2),
                            InsnEmpty(IpFwOpcode.O_ACCEPT),
                        ],
                    },
                },
                id="test_tables",
            ),
            pytest.param(
                {
                    "in": "add allow ip from any to 1.2.3.4 // test comment",
                    "out": {
                        "insns": [
                            InsnIp(IpFwOpcode.O_IP_DST, ip="1.2.3.4"),
                            InsnComment(comment="test comment"),
                            InsnEmpty(IpFwOpcode.O_ACCEPT),
                        ],
                    },
                },
                id="test_comment",
            ),
            pytest.param(
                {
                    "in": "add tcp-setmss 123 ip from any to 1.2.3.4",
                    "out": {
                        "objs": [
                            NTlv(IpFwTlvType.IPFW_TLV_EACTION, idx=1, name="tcp-setmss"),
                        ],
                        "insns": [
                            InsnIp(IpFwOpcode.O_IP_DST, ip="1.2.3.4"),
                            Insn(IpFwOpcode.O_EXTERNAL_ACTION, arg1=1),
                            Insn(IpFwOpcode.O_EXTERNAL_DATA, arg1=123),
                        ],
                    },
                },
                id="test_eaction_tcp-setmss",
            ),
            pytest.param(
                {
                    "in": "add eaction ntpv6 AAA ip from any to 1.2.3.4",
                    "out": {
                        "objs": [
                            NTlv(IpFwTlvType.IPFW_TLV_EACTION, idx=1, name="ntpv6"),
                            NTlv(0, idx=2, name="AAA"),
                        ],
                        "insns": [
                            InsnIp(IpFwOpcode.O_IP_DST, ip="1.2.3.4"),
                            Insn(IpFwOpcode.O_EXTERNAL_ACTION, arg1=1),
                            Insn(IpFwOpcode.O_EXTERNAL_INSTANCE, arg1=2),
                        ],
                    },
                },
                id="test_eaction_ntp",
            ),
            pytest.param(
                {
                    "in": "add // test comment",
                    "out": {
                        "insns": [
                            InsnComment(comment="test comment"),
                            Insn(IpFwOpcode.O_COUNT),
                        ],
                    },
                },
                id="test_action_comment",
            ),
            pytest.param(
                {
                    "in": "add check-state :OUT // test comment",
                    "out": {
                        "objs": [
                            NTlv(IpFwTlvType.IPFW_TLV_STATE_NAME, idx=1, name="OUT"),
                        ],
                        "insns": [
                            InsnComment(comment="test comment"),
                            Insn(IpFwOpcode.O_CHECK_STATE, arg1=1),
                        ],
                    },
                },
                id="test_check_state",
            ),
            pytest.param(
                {
                    "in": "add allow tcp from any to any keep-state :OUT",
                    "out": {
                        "objs": [
                            NTlv(IpFwTlvType.IPFW_TLV_STATE_NAME, idx=1, name="OUT"),
                        ],
                        "insns": [
                            Insn(IpFwOpcode.O_PROBE_STATE, arg1=1),
                            Insn(IpFwOpcode.O_PROTO, arg1=6),
                            Insn(IpFwOpcode.O_KEEP_STATE, arg1=1),
                            InsnEmpty(IpFwOpcode.O_ACCEPT),
                        ],
                    },
                },
                id="test_keep_state",
            ),
            pytest.param(
                {
                    "in": "add allow tcp from any to any record-state",
                    "out": {
                        "objs": [
                            NTlv(IpFwTlvType.IPFW_TLV_STATE_NAME, idx=1, name="default"),
                        ],
                        "insns": [
                            Insn(IpFwOpcode.O_PROTO, arg1=6),
                            Insn(IpFwOpcode.O_KEEP_STATE, arg1=1),
                            InsnEmpty(IpFwOpcode.O_ACCEPT),
                        ],
                    },
                },
                id="test_record_state",
            ),
        ],
    )
    def test_add_rule(self, rule):
        """Tests if the compiled rule is sane and matches the spec"""
        self.verify_rule(rule["in"], rule["out"])

    @pytest.mark.parametrize(
        "action",
        [
            pytest.param(("allow", InsnEmpty(IpFwOpcode.O_ACCEPT)), id="test_allow"),
            pytest.param(
                (
                    "abort",
                    Insn(IpFwOpcode.O_REJECT, arg1=IcmpRejectCode.ICMP_REJECT_ABORT),
                ),
                id="abort",
            ),
            pytest.param(
                (
                    "abort6",
                    Insn(
                        IpFwOpcode.O_UNREACH6, arg1=Icmp6RejectCode.ICMP6_UNREACH_ABORT
                    ),
                ),
                id="abort6",
            ),
            pytest.param(("accept", InsnEmpty(IpFwOpcode.O_ACCEPT)), id="accept"),
            pytest.param(("deny", InsnEmpty(IpFwOpcode.O_DENY)), id="deny"),
            pytest.param(
                (
                    "reject",
                    Insn(IpFwOpcode.O_REJECT, arg1=IcmpRejectCode.ICMP_UNREACH_HOST),
                ),
                id="reject",
            ),
            pytest.param(
                (
                    "reset",
                    Insn(IpFwOpcode.O_REJECT, arg1=IcmpRejectCode.ICMP_REJECT_RST),
                ),
                id="reset",
            ),
            pytest.param(
                (
                    "reset6",
                    Insn(IpFwOpcode.O_UNREACH6, arg1=Icmp6RejectCode.ICMP6_UNREACH_RST),
                ),
                id="reset6",
            ),
            pytest.param(
                (
                    "unreach port",
                    InsnReject(
                        IpFwOpcode.O_REJECT, arg1=IcmpRejectCode.ICMP_UNREACH_PORT
                    ),
                ),
                id="unreach_port",
            ),
            pytest.param(
                (
                    "unreach port",
                    InsnReject(
                        IpFwOpcode.O_REJECT, arg1=IcmpRejectCode.ICMP_UNREACH_PORT
                    ),
                ),
                id="unreach_port",
            ),
            pytest.param(
                (
                    "unreach needfrag",
                    InsnReject(
                        IpFwOpcode.O_REJECT, arg1=IcmpRejectCode.ICMP_UNREACH_NEEDFRAG
                    ),
                ),
                id="unreach_needfrag",
            ),
            pytest.param(
                (
                    "unreach needfrag 1420",
                    InsnReject(
                        IpFwOpcode.O_REJECT,
                        arg1=IcmpRejectCode.ICMP_UNREACH_NEEDFRAG,
                        mtu=1420,
                    ),
                ),
                id="unreach_needfrag_mtu",
            ),
            pytest.param(
                (
                    "unreach6 port",
                    Insn(
                        IpFwOpcode.O_UNREACH6,
                        arg1=Icmp6RejectCode.ICMP6_DST_UNREACH_NOPORT,
                    ),
                ),
                id="unreach6_port",
            ),
            pytest.param(("count", InsnEmpty(IpFwOpcode.O_COUNT)), id="count"),
            # TOK_NAT
            pytest.param(
                ("queue 42", Insn(IpFwOpcode.O_QUEUE, arg1=42)), id="queue_42"
            ),
            pytest.param(("pipe 42", Insn(IpFwOpcode.O_PIPE, arg1=42)), id="pipe_42"),
            pytest.param(
                ("skipto 42", Insn(IpFwOpcode.O_SKIPTO, arg1=42)), id="skipto_42"
            ),
            pytest.param(
                ("netgraph 42", Insn(IpFwOpcode.O_NETGRAPH, arg1=42)), id="netgraph_42"
            ),
            pytest.param(
                ("ngtee 42", Insn(IpFwOpcode.O_NGTEE, arg1=42)), id="ngtee_42"
            ),
            pytest.param(
                ("divert 42", Insn(IpFwOpcode.O_DIVERT, arg1=42)), id="divert_42"
            ),
            pytest.param(
                ("divert natd", Insn(IpFwOpcode.O_DIVERT, arg1=8668)), id="divert_natd"
            ),
            pytest.param(("tee 42", Insn(IpFwOpcode.O_TEE, arg1=42)), id="tee_42"),
            pytest.param(
                ("call 420", Insn(IpFwOpcode.O_CALLRETURN, arg1=420)), id="call_420"
            ),
            # TOK_FORWARD
            pytest.param(
                ("setfib 1", Insn(IpFwOpcode.O_SETFIB, arg1=1 | 0x8000)),
                id="setfib_1",
                marks=pytest.mark.skip("needs net.fibs>1"),
            ),
            pytest.param(
                ("setdscp 42", Insn(IpFwOpcode.O_SETDSCP, arg1=42 | 0x8000)),
                id="setdscp_42",
            ),
            pytest.param(("reass", InsnEmpty(IpFwOpcode.O_REASS)), id="reass"),
            pytest.param(
                ("return", InsnEmpty(IpFwOpcode.O_CALLRETURN, is_not=True)), id="return"
            ),
        ],
    )
    def test_add_action(self, action):
        """Tests if the rule action is compiled properly"""
        rule_in = "add {} ip from any to any".format(action[0])
        rule_out = {"insns": [action[1]]}
        self.verify_rule(rule_in, rule_out)

    @pytest.mark.parametrize(
        "insn",
        [
            pytest.param(
                {
                    "in": "add prob 0.7 allow ip from any to any",
                    "out": InsnProb(prob=0.7),
                },
                id="test_prob",
            ),
            pytest.param(
                {
                    "in": "add allow tcp from any to any",
                    "out": InsnProto(arg1=6),
                },
                id="test_proto",
            ),
            pytest.param(
                {
                    "in": "add allow ip from any to any 57",
                    "out": InsnPorts(IpFwOpcode.O_IP_DSTPORT, port_pairs=[57, 57]),
                },
                id="test_ports",
            ),
        ],
    )
    def test_add_single_instruction(self, insn):
        """Tests if the compiled rule is sane and matches the spec"""

        # Prepare the desired output
        out = {
            "insns": [insn["out"], InsnEmpty(IpFwOpcode.O_ACCEPT)],
        }
        self.verify_rule(insn["in"], out)

    @pytest.mark.parametrize(
        "opcode",
        [
            pytest.param(IpFwOpcode.O_IP_SRCPORT, id="src"),
            pytest.param(IpFwOpcode.O_IP_DSTPORT, id="dst"),
        ],
    )
    @pytest.mark.parametrize(
        "params",
        [
            pytest.param(
                {
                    "in": "57",
                    "out": [(57, 57)],
                },
                id="test_single",
            ),
            pytest.param(
                {
                    "in": "57-59",
                    "out": [(57, 59)],
                },
                id="test_range",
            ),
            pytest.param(
                {
                    "in": "57-59,41",
                    "out": [(57, 59), (41, 41)],
                },
                id="test_ranges",
            ),
        ],
    )
    def test_add_ports(self, params, opcode):
        if opcode == IpFwOpcode.O_IP_DSTPORT:
            txt = "add allow ip from any to any " + params["in"]
        else:
            txt = "add allow ip from any " + params["in"] + " to any"
        out = {
            "insns": [
                InsnPorts(opcode, port_pairs=params["out"]),
                InsnEmpty(IpFwOpcode.O_ACCEPT),
            ]
        }
        self.verify_rule(txt, out)
