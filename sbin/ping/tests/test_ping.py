import pytest

import logging
import os
import re
import subprocess

from atf_python.sys.net.vnet import IfaceFactory
from atf_python.sys.net.vnet import SingleVnetTestTemplate
from atf_python.sys.net.tools import ToolsHelper
from typing import List
from typing import Optional

logging.getLogger("scapy").setLevel(logging.CRITICAL)
import scapy.all as sc


def build_response_packet(echo, ip, icmp, oip_ihl, special):
    icmp_id_seq_types = [0, 8, 13, 14, 15, 16, 17, 18, 37, 38]
    oip = echo[sc.IP]
    oicmp = echo[sc.ICMP]
    load = echo[sc.ICMP].payload
    oip[sc.IP].remove_payload()
    oicmp[sc.ICMP].remove_payload()
    oicmp.type = 8

    # As if the original IP packet had these set
    oip.ihl = None
    oip.len = None
    oip.id = 1
    oip.flags = ip.flags
    oip.chksum = None
    oip.options = ip.options

    # Inner packet (oip) options
    if oip_ihl:
        oip.ihl = oip_ihl

    # Special options
    if special == "no-payload":
        load = ""
    if special == "tcp":
        oip.proto = "tcp"
        tcp = sc.TCP(sport=1234, dport=5678)
        return ip / icmp / oip / tcp
    if special == "udp":
        oip.proto = "udp"
        udp = sc.UDP(sport=1234, dport=5678)
        return ip / icmp / oip / udp
    if special == "warp":
        # Build a package with a timestamp of INT_MAX
        # (time-warped package)
        payload_no_timestamp = sc.bytes_hex(load)[16:]
        load = b"\x7f" + (b"\xff" * 7) + sc.hex_bytes(payload_no_timestamp)
    if special == "wrong":
        # Build a package with a wrong last byte
        payload_no_last_byte = sc.bytes_hex(load)[:-2]
        load = (sc.hex_bytes(payload_no_last_byte)) + b"\x00"
    if special == "not-mine":
        # Modify the ICMP Identifier field
        oicmp.id += 1

    if icmp.type in icmp_id_seq_types:
        pkt = ip / icmp / load
    else:
        ip.options = ""
        pkt = ip / icmp / oip / oicmp / load
    return pkt


def generate_ip_options(opts):
    if not opts:
        return ""

    routers = [
        "192.0.2.10",
        "192.0.2.20",
        "192.0.2.30",
        "192.0.2.40",
        "192.0.2.50",
        "192.0.2.60",
        "192.0.2.70",
        "192.0.2.80",
        "192.0.2.90",
    ]
    routers_zero = [0, 0, 0, 0, 0, 0, 0, 0, 0]
    if opts == "EOL":
        options = sc.IPOption(b"\x00")
    elif opts == "NOP":
        options = sc.IPOption(b"\x01")
    elif opts == "NOP-40":
        options = sc.IPOption(b"\x01" * 40)
    elif opts == "RR":
        ToolsHelper.set_sysctl("net.inet.ip.process_options", 0)
        options = sc.IPOption_RR(pointer=40, routers=routers)
    elif opts == "RR-same":
        ToolsHelper.set_sysctl("net.inet.ip.process_options", 0)
        options = sc.IPOption_RR(pointer=3, routers=routers_zero)
    elif opts == "RR-trunc":
        ToolsHelper.set_sysctl("net.inet.ip.process_options", 0)
        options = sc.IPOption_RR(length=7, routers=routers_zero)
    elif opts == "LSRR":
        ToolsHelper.set_sysctl("net.inet.ip.process_options", 0)
        options = sc.IPOption_LSRR(routers=routers)
    elif opts == "LSRR-trunc":
        ToolsHelper.set_sysctl("net.inet.ip.process_options", 0)
        options = sc.IPOption_LSRR(length=3, routers=routers_zero)
    elif opts == "SSRR":
        ToolsHelper.set_sysctl("net.inet.ip.process_options", 0)
        options = sc.IPOption_SSRR(routers=routers)
    elif opts == "SSRR-trunc":
        ToolsHelper.set_sysctl("net.inet.ip.process_options", 0)
        options = sc.IPOption_SSRR(length=3, routers=routers_zero)
    elif opts == "unk":
        ToolsHelper.set_sysctl("net.inet.ip.process_options", 0)
        options = sc.IPOption(b"\x9f")
    elif opts == "unk-40":
        ToolsHelper.set_sysctl("net.inet.ip.process_options", 0)
        options = sc.IPOption(b"\x9f" * 40)
    else:
        options = ""
    return options


def pinger(
    # Required arguments
    # Avoid setting defaults on these arguments,
    # as we want to set them explicitly in the tests
    iface: str,
    /,
    src: sc.scapy.fields.SourceIPField,
    dst: sc.scapy.layers.inet.DestIPField,
    icmp_type: sc.scapy.fields.ByteEnumField,
    icmp_code: sc.scapy.fields.MultiEnumField,
    # IP arguments
    ihl: Optional[sc.scapy.fields.BitField] = None,
    flags: Optional[sc.scapy.fields.FlagsField] = None,
    opts: Optional[str] = None,
    oip_ihl: Optional[sc.scapy.fields.BitField] = None,
    special: Optional[str] = None,
    # ICMP arguments
    # Match names with <netinet/ip_icmp.h>
    icmp_pptr: sc.scapy.fields.ByteField = 0,
    icmp_gwaddr: sc.scapy.fields.IPField = "0.0.0.0",
    icmp_nextmtu: sc.scapy.fields.ShortField = 0,
    icmp_otime: sc.scapy.layers.inet.ICMPTimeStampField = 0,
    icmp_rtime: sc.scapy.layers.inet.ICMPTimeStampField = 0,
    icmp_ttime: sc.scapy.layers.inet.ICMPTimeStampField = 0,
    icmp_mask: sc.scapy.fields.IPField = "0.0.0.0",
    request: Optional[str] = None,
    # Miscellaneous arguments
    count: int = 1,
    dup: bool = False,
    verbose: bool = True,
) -> subprocess.CompletedProcess:
    """P I N G E R

    Echo reply faker

    :param str iface: Interface to send packet to
    :keyword src: Source packet IP
    :type src: class:`scapy.fields.SourceIPField`
    :keyword dst: Destination packet IP
    :type dst: class:`scapy.layers.inet.DestIPField`
    :keyword icmp_type: ICMP type
    :type icmp_type: class:`scapy.fields.ByteEnumField`
    :keyword icmp_code: ICMP code
    :type icmp_code: class:`scapy.fields.MultiEnumField`

    :keyword ihl: Internet Header Length, defaults to None
    :type ihl: class:`scapy.fields.BitField`, optional
    :keyword flags: IP flags - one of `DF`, `MF` or `evil`, defaults to None
    :type flags: class:`scapy.fields.FlagsField`, optional
    :keyword opts: Include IP options - one of `EOL`, `NOP`, `NOP-40`, `unk`,
        `unk-40`, `RR`, `RR-same`, `RR-trunc`, `LSRR`, `LSRR-trunc`, `SSRR` or
        `SSRR-trunc`, defaults to None
    :type opts: str, optional
    :keyword oip_ihl: Inner packet's Internet Header Length, defaults to None
    :type oip_ihl: class:`scapy.fields.BitField`, optional
    :keyword special: Send a special packet - one of `no-payload`, `not-mine`,
        `tcp`, `udp`, `wrong` or `warp`, defaults to None
    :type special: str, optional
    :keyword icmp_pptr: ICMP pointer, defaults to 0
    :type icmp_pptr: class:`scapy.fields.ByteField`
    :keyword icmp_gwaddr: ICMP gateway IP address, defaults to "0.0.0.0"
    :type icmp_gwaddr: class:`scapy.fields.IPField`
    :keyword icmp_nextmtu: ICMP next MTU, defaults to 0
    :type icmp_nextmtu: class:`scapy.fields.ShortField`
    :keyword icmp_otime: ICMP originate timestamp, defaults to 0
    :type icmp_otime: class:`scapy.layers.inet.ICMPTimeStampField`
    :keyword icmp_rtime: ICMP receive timestamp, defaults to 0
    :type icmp_rtime: class:`scapy.layers.inet.ICMPTimeStampField`
    :keyword icmp_ttime: ICMP transmit timestamp, defaults to 0
    :type icmp_ttime: class:`scapy.layers.inet.ICMPTimeStampField`
    :keyword icmp_mask: ICMP address mask, defaults to "0.0.0.0"
    :type icmp_mask: class:`scapy.fields.IPField`
    :keyword request: Request type - one of `mask` or `timestamp`,
        defaults to None
    :type request: str, optional
    :keyword count: Number of packets to send, defaults to 1
    :type count: int
    :keyword dup: Duplicate packets, defaults to `False`
    :type dup: bool
    :keyword verbose: Turn on/off verbosity, defaults to `True`
    :type verbose: bool

    :return: A class:`subprocess.CompletedProcess` with the output from the
        ping utility
    :rtype: class:`subprocess.CompletedProcess`
    """
    tun = sc.TunTapInterface(iface)
    subprocess.run(["ifconfig", tun.iface, "up"], check=True)
    subprocess.run(["ifconfig", tun.iface, src, dst], check=True)
    ip_opts = generate_ip_options(opts)
    ip = sc.IP(ihl=ihl, flags=flags, src=dst, dst=src, options=ip_opts)
    command = [
        "/sbin/ping",
        "-c",
        str(count),
        "-t",
        str(count),
    ]
    if verbose:
        command += ["-v"]
    if request == "mask":
        command += ["-Mm"]
    if request == "timestamp":
        command += ["-Mt"]
    if special:
        command += ["-p1"]
    if opts in [
        "RR",
        "RR-same",
        "RR-trunc",
        "LSRR",
        "LSRR-trunc",
        "SSRR",
        "SSRR-trunc",
    ]:
        command += ["-R"]
    command += [dst]
    with subprocess.Popen(
        args=command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
    ) as ping:
        for dummy in range(count):
            echo = tun.recv()
            icmp = sc.ICMP(
                type=icmp_type,
                code=icmp_code,
                id=echo[sc.ICMP].id,
                seq=echo[sc.ICMP].seq,
                ts_ori=icmp_otime,
                ts_rx=icmp_rtime,
                ts_tx=icmp_ttime,
                gw=icmp_gwaddr,
                ptr=icmp_pptr,
                addr_mask=icmp_mask,
                nexthopmtu=icmp_nextmtu,
            )
            pkt = build_response_packet(echo, ip, icmp, oip_ihl, special)
            tun.send(pkt)
            if dup is True:
                tun.send(pkt)
        stdout, stderr = ping.communicate()
    return subprocess.CompletedProcess(
        ping.args, ping.returncode, stdout, stderr
    )


def redact(output):
    """Redact some elements of ping's output"""
    pattern_replacements = [
        ("localhost \([0-9]{1,3}(\.[0-9]{1,3}){3}\)", "localhost"),
        ("from [0-9]{1,3}(\.[0-9]{1,3}){3}", "from"),
        ("hlim=[0-9]*", "hlim="),
        ("ttl=[0-9]*", "ttl="),
        ("time=[0-9.-]*", "time="),
        ("cp: .*", "cp: xx xx xx xx xx xx xx xx"),
        ("dp: .*", "dp: xx xx xx xx xx xx xx xx"),
        ("\(-[0-9\.]+[0-9]+ ms\)", "(- ms)"),
        ("[0-9\.]+/[0-9.]+", "/"),
    ]
    for pattern, repl in pattern_replacements:
        output = re.sub(pattern, repl, output)
    return output


class TestPing(SingleVnetTestTemplate):
    IPV6_PREFIXES: List[str] = ["2001:db8::1/64"]
    IPV4_PREFIXES: List[str] = ["192.0.2.1/24"]

    # Each param in testdata contains a dictionary with the command,
    # and the expected outcome (returncode, redacted stdout, and stderr)
    testdata = [
        pytest.param(
            {
                "args": "ping -4 -c1 -s56 -t1 localhost",
                "returncode": 0,
                "stdout": """\
PING localhost: 56 data bytes
64 bytes from: icmp_seq=0 ttl= time= ms

--- localhost ping statistics ---
1 packets transmitted, 1 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
            },
            id="_4_c1_s56_t1_localhost",
        ),
        pytest.param(
            {
                "args": "ping -6 -c1 -s8 -t1 localhost",
                "returncode": 0,
                "stdout": """\
PING(56=40+8+8 bytes) ::1 --> ::1
16 bytes from ::1, icmp_seq=0 hlim= time= ms

--- localhost ping statistics ---
1 packets transmitted, 1 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
            },
            id="_6_c1_s8_t1_localhost",
        ),
        pytest.param(
            {
                "args": "ping -A -c1 192.0.2.1",
                "returncode": 0,
                "stdout": """\
PING 192.0.2.1 (192.0.2.1): 56 data bytes
64 bytes from: icmp_seq=0 ttl= time= ms

--- 192.0.2.1 ping statistics ---
1 packets transmitted, 1 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
            },
            id="_A_c1_192_0_2_1",
        ),
        pytest.param(
            {
                "args": "ping -A -c1 192.0.2.2",
                "returncode": 2,
                "stdout": """\
PING 192.0.2.2 (192.0.2.2): 56 data bytes

--- 192.0.2.2 ping statistics ---
1 packets transmitted, 0 packets received, 100.0% packet loss
""",
                "stderr": "",
            },
            id="_A_c1_192_0_2_2",
        ),
        pytest.param(
            {
                "args": "ping -A -c1 2001:db8::1",
                "returncode": 0,
                "stdout": """\
PING(56=40+8+8 bytes) 2001:db8::1 --> 2001:db8::1
16 bytes from 2001:db8::1, icmp_seq=0 hlim= time= ms

--- 2001:db8::1 ping statistics ---
1 packets transmitted, 1 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
            },
            id="_A_c1_2001_db8__1",
        ),
        pytest.param(
            {
                "args": "ping -A -c1 2001:db8::2",
                "returncode": 2,
                "stdout": """\
PING(56=40+8+8 bytes) 2001:db8::1 --> 2001:db8::2

--- 2001:db8::2 ping statistics ---
1 packets transmitted, 0 packets received, 100.0% packet loss
""",
                "stderr": "",
            },
            id="_A_c1_2001_db8__2",
        ),
        pytest.param(
            {
                "args": "ping -A -c3 192.0.2.1",
                "returncode": 0,
                "stdout": """\
PING 192.0.2.1 (192.0.2.1): 56 data bytes
64 bytes from: icmp_seq=0 ttl= time= ms
64 bytes from: icmp_seq=1 ttl= time= ms
64 bytes from: icmp_seq=2 ttl= time= ms

--- 192.0.2.1 ping statistics ---
3 packets transmitted, 3 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
            },
            id="_A_3_192_0.2.1",
        ),
        pytest.param(
            {
                "args": "ping -A -c3 192.0.2.2",
                "returncode": 2,
                "stdout": """\
\x07\x07PING 192.0.2.2 (192.0.2.2): 56 data bytes

--- 192.0.2.2 ping statistics ---
3 packets transmitted, 0 packets received, 100.0% packet loss
""",
                "stderr": "",
            },
            id="_A_c3_192_0_2_2",
        ),
        pytest.param(
            {
                "args": "ping -A -c3 2001:db8::1",
                "returncode": 0,
                "stdout": """\
PING(56=40+8+8 bytes) 2001:db8::1 --> 2001:db8::1
16 bytes from 2001:db8::1, icmp_seq=0 hlim= time= ms
16 bytes from 2001:db8::1, icmp_seq=1 hlim= time= ms
16 bytes from 2001:db8::1, icmp_seq=2 hlim= time= ms

--- 2001:db8::1 ping statistics ---
3 packets transmitted, 3 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
            },
            id="_A_c3_2001_db8__1",
        ),
        pytest.param(
            {
                "args": "ping -A -c3 2001:db8::2",
                "returncode": 2,
                "stdout": """\
\x07\x07PING(56=40+8+8 bytes) 2001:db8::1 --> 2001:db8::2

--- 2001:db8::2 ping statistics ---
3 packets transmitted, 0 packets received, 100.0% packet loss
""",
                "stderr": "",
            },
            id="_A_c3_2001_db8__2",
        ),
        pytest.param(
            {
                "args": "ping -c1 192.0.2.1",
                "returncode": 0,
                "stdout": """\
PING 192.0.2.1 (192.0.2.1): 56 data bytes
64 bytes from: icmp_seq=0 ttl= time= ms

--- 192.0.2.1 ping statistics ---
1 packets transmitted, 1 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
            },
            id="_c1_192_0_2_1",
        ),
        pytest.param(
            {
                "args": "ping -c1 192.0.2.2",
                "returncode": 2,
                "stdout": """\
PING 192.0.2.2 (192.0.2.2): 56 data bytes

--- 192.0.2.2 ping statistics ---
1 packets transmitted, 0 packets received, 100.0% packet loss
""",
                "stderr": "",
            },
            id="_c1_192_0_2_2",
        ),
        pytest.param(
            {
                "args": "ping -c1 2001:db8::1",
                "returncode": 0,
                "stdout": """\
PING(56=40+8+8 bytes) 2001:db8::1 --> 2001:db8::1
16 bytes from 2001:db8::1, icmp_seq=0 hlim= time= ms

--- 2001:db8::1 ping statistics ---
1 packets transmitted, 1 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
            },
            id="_c1_2001_db8__1",
        ),
        pytest.param(
            {
                "args": "ping -c1 2001:db8::2",
                "returncode": 2,
                "stdout": """\
PING(56=40+8+8 bytes) 2001:db8::1 --> 2001:db8::2

--- 2001:db8::2 ping statistics ---
1 packets transmitted, 0 packets received, 100.0% packet loss
""",
                "stderr": "",
            },
            id="_c1_2001_db8__2",
        ),
        pytest.param(
            {
                "args": "ping -c1 -S127.0.0.1 -s56 -t1 localhost",
                "returncode": 0,
                "stdout": """\
PING localhost from: 56 data bytes
64 bytes from: icmp_seq=0 ttl= time= ms

--- localhost ping statistics ---
1 packets transmitted, 1 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
            },
            id="_c1_S127_0_0_1_s56_t1_localhost",
        ),
        pytest.param(
            {
                "args": "ping -c1 -S::1 -s8 -t1 localhost",
                "returncode": 0,
                "stdout": """\
PING(56=40+8+8 bytes) ::1 --> ::1
16 bytes from ::1, icmp_seq=0 hlim= time= ms

--- localhost ping statistics ---
1 packets transmitted, 1 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
            },
            id="_c1_S__1_s8_t1_localhost",
        ),
        pytest.param(
            {
                "args": "ping -c3 192.0.2.1",
                "returncode": 0,
                "stdout": """\
PING 192.0.2.1 (192.0.2.1): 56 data bytes
64 bytes from: icmp_seq=0 ttl= time= ms
64 bytes from: icmp_seq=1 ttl= time= ms
64 bytes from: icmp_seq=2 ttl= time= ms

--- 192.0.2.1 ping statistics ---
3 packets transmitted, 3 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
            },
            id="_c3_192_0_2_1",
        ),
        pytest.param(
            {
                "args": "ping -c3 192.0.2.2",
                "returncode": 2,
                "stdout": """\
PING 192.0.2.2 (192.0.2.2): 56 data bytes

--- 192.0.2.2 ping statistics ---
3 packets transmitted, 0 packets received, 100.0% packet loss
""",
                "stderr": "",
            },
            id="_c3_192_0_2_2",
        ),
        pytest.param(
            {
                "args": "ping -c3 2001:db8::1",
                "returncode": 0,
                "stdout": """\
PING(56=40+8+8 bytes) 2001:db8::1 --> 2001:db8::1
16 bytes from 2001:db8::1, icmp_seq=0 hlim= time= ms
16 bytes from 2001:db8::1, icmp_seq=1 hlim= time= ms
16 bytes from 2001:db8::1, icmp_seq=2 hlim= time= ms

--- 2001:db8::1 ping statistics ---
3 packets transmitted, 3 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
            },
            id="_c3_2001_db8__1",
        ),
        pytest.param(
            {
                "args": "ping -c3 2001:db8::2",
                "returncode": 2,
                "stdout": """\
PING(56=40+8+8 bytes) 2001:db8::1 --> 2001:db8::2

--- 2001:db8::2 ping statistics ---
3 packets transmitted, 0 packets received, 100.0% packet loss
""",
                "stderr": "",
            },
            id="_c3_2001_db8__2",
        ),
        pytest.param(
            {
                "args": "ping -q -c1 192.0.2.1",
                "returncode": 0,
                "stdout": """\
PING 192.0.2.1 (192.0.2.1): 56 data bytes

--- 192.0.2.1 ping statistics ---
1 packets transmitted, 1 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
            },
            id="_q_c1_192_0_2_1",
        ),
        pytest.param(
            {
                "args": "ping -q -c1 192.0.2.2",
                "returncode": 2,
                "stdout": """\
PING 192.0.2.2 (192.0.2.2): 56 data bytes

--- 192.0.2.2 ping statistics ---
1 packets transmitted, 0 packets received, 100.0% packet loss
""",
                "stderr": "",
            },
            id="_q_c1_192_0_2_2",
        ),
        pytest.param(
            {
                "args": "ping -q -c1 2001:db8::1",
                "returncode": 0,
                "stdout": """\
PING(56=40+8+8 bytes) 2001:db8::1 --> 2001:db8::1

--- 2001:db8::1 ping statistics ---
1 packets transmitted, 1 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
            },
            id="_q_c1_2001_db8__1",
        ),
        pytest.param(
            {
                "args": "ping -q -c1 2001:db8::2",
                "returncode": 2,
                "stdout": """\
PING(56=40+8+8 bytes) 2001:db8::1 --> 2001:db8::2

--- 2001:db8::2 ping statistics ---
1 packets transmitted, 0 packets received, 100.0% packet loss
""",
                "stderr": "",
            },
            id="_q_c1_2001_db8__2",
        ),
        pytest.param(
            {
                "args": "ping -q -c3 192.0.2.1",
                "returncode": 0,
                "stdout": """\
PING 192.0.2.1 (192.0.2.1): 56 data bytes

--- 192.0.2.1 ping statistics ---
3 packets transmitted, 3 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
            },
            id="_q_c3_192_0_2_1",
        ),
        pytest.param(
            {
                "args": "ping -q -c3 192.0.2.2",
                "returncode": 2,
                "stdout": """\
PING 192.0.2.2 (192.0.2.2): 56 data bytes

--- 192.0.2.2 ping statistics ---
3 packets transmitted, 0 packets received, 100.0% packet loss
""",
                "stderr": "",
            },
            id="_q_c3_192_0_2_2",
        ),
        pytest.param(
            {
                "args": "ping -q -c3 2001:db8::1",
                "returncode": 0,
                "stdout": """\
PING(56=40+8+8 bytes) 2001:db8::1 --> 2001:db8::1

--- 2001:db8::1 ping statistics ---
3 packets transmitted, 3 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
            },
            id="_q_c3_2001_db8__1",
        ),
        pytest.param(
            {
                "args": "ping -q -c3 2001:db8::2",
                "returncode": 2,
                "stdout": """\
PING(56=40+8+8 bytes) 2001:db8::1 --> 2001:db8::2

--- 2001:db8::2 ping statistics ---
3 packets transmitted, 0 packets received, 100.0% packet loss
""",
                "stderr": "",
            },
            id="_q_c3_2001_db8__2",
        ),
    ]

    @pytest.mark.parametrize("expected", testdata)
    @pytest.mark.require_user("root")
    def test_ping(self, expected):
        """Test ping"""
        ping = subprocess.run(
            expected["args"].split(),
            capture_output=True,
            timeout=15,
            text=True,
        )
        assert ping.returncode == expected["returncode"]
        assert redact(ping.stdout) == expected["stdout"]
        assert ping.stderr == expected["stderr"]

    # Each param in ping46_testdata contains a dictionary with the arguments
    # and the expected outcome (returncode, redacted stdout, and stderr)
    # common to `ping -4` and `ping -6`
    ping46_testdata = [
        pytest.param(
            {
                "args": "-Wx localhost",
                "returncode": os.EX_USAGE,
                "stdout": "",
                "stderr": "ping: invalid timing interval: `x'\n",
            },
            id="_Wx_localhost",
        ),
    ]

    @pytest.mark.parametrize("expected", ping46_testdata)
    @pytest.mark.require_user("root")
    def test_ping_46(self, expected):
        """Test ping -4/ping -6"""
        for version in [4, 6]:
            ping = subprocess.run(
                ["ping", f"-{version}"] + expected["args"].split(),
                capture_output=True,
                timeout=15,
                text=True,
            )
            assert ping.returncode == expected["returncode"]
            assert redact(ping.stdout) == expected["stdout"]
            assert ping.stderr == expected["stderr"]

    # Each param in pinger_testdata contains a dictionary with the keywords to
    # `pinger()` and a dictionary with the expected outcome (returncode,
    # stdout, stderr, and if ping's output is redacted)
    pinger_testdata = [
        pytest.param(
            {
                "src": "192.0.2.1",
                "dst": "192.0.2.2",
                "icmp_type": 0,
                "icmp_code": 0,
            },
            {
                "returncode": 0,
                "stdout": """\
PING 192.0.2.2 (192.0.2.2): 56 data bytes
64 bytes from: icmp_seq=0 ttl= time= ms

--- 192.0.2.2 ping statistics ---
1 packets transmitted, 1 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
                "redacted": True,
            },
            id="_0_0",
        ),
        pytest.param(
            {
                "src": "192.0.2.1",
                "dst": "192.0.2.2",
                "icmp_type": 0,
                "icmp_code": 0,
                "opts": "EOL",
            },
            {
                "returncode": 0,
                "stdout": """\
PING 192.0.2.2 (192.0.2.2): 56 data bytes
64 bytes from: icmp_seq=0 ttl= time= ms
wrong total length 88 instead of 84

--- 192.0.2.2 ping statistics ---
1 packets transmitted, 1 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
                "redacted": True,
            },
            id="_0_0_opts_EOL",
        ),
        pytest.param(
            {
                "src": "192.0.2.1",
                "dst": "192.0.2.2",
                "icmp_type": 0,
                "icmp_code": 0,
                "opts": "LSRR",
            },
            {
                "returncode": 0,
                "stdout": """\
PING 192.0.2.2 (192.0.2.2): 56 data bytes
64 bytes from: icmp_seq=0 ttl= time= ms
LSRR: 	192.0.2.10
	192.0.2.20
	192.0.2.30
	192.0.2.40
	192.0.2.50
	192.0.2.60
	192.0.2.70
	192.0.2.80
	192.0.2.90

--- 192.0.2.2 ping statistics ---
1 packets transmitted, 1 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
                "redacted": True,
            },
            id="_0_0_opts_LSRR",
        ),
        pytest.param(
            {
                "src": "192.0.2.1",
                "dst": "192.0.2.2",
                "icmp_type": 0,
                "icmp_code": 0,
                "opts": "LSRR-trunc",
            },
            {
                "returncode": 0,
                "stdout": """\
PING 192.0.2.2 (192.0.2.2): 56 data bytes
64 bytes from: icmp_seq=0 ttl= time= ms
LSRR: 	(truncated route)

--- 192.0.2.2 ping statistics ---
1 packets transmitted, 1 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
                "redacted": True,
            },
            id="_0_0_opts_LSRR_trunc",
        ),
        pytest.param(
            {
                "src": "192.0.2.1",
                "dst": "192.0.2.2",
                "icmp_type": 0,
                "icmp_code": 0,
                "opts": "SSRR",
            },
            {
                "returncode": 0,
                "stdout": """\
PING 192.0.2.2 (192.0.2.2): 56 data bytes
64 bytes from: icmp_seq=0 ttl= time= ms
SSRR: 	192.0.2.10
	192.0.2.20
	192.0.2.30
	192.0.2.40
	192.0.2.50
	192.0.2.60
	192.0.2.70
	192.0.2.80
	192.0.2.90

--- 192.0.2.2 ping statistics ---
1 packets transmitted, 1 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
                "redacted": True,
            },
            id="_0_0_opts_SSRR",
        ),
        pytest.param(
            {
                "src": "192.0.2.1",
                "dst": "192.0.2.2",
                "icmp_type": 0,
                "icmp_code": 0,
                "opts": "SSRR-trunc",
            },
            {
                "returncode": 0,
                "stdout": """\
PING 192.0.2.2 (192.0.2.2): 56 data bytes
64 bytes from: icmp_seq=0 ttl= time= ms
SSRR: 	(truncated route)

--- 192.0.2.2 ping statistics ---
1 packets transmitted, 1 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
                "redacted": True,
            },
            id="_0_0_opts_SSRR_trunc",
        ),
        pytest.param(
            {
                "src": "192.0.2.1",
                "dst": "192.0.2.2",
                "icmp_type": 0,
                "icmp_code": 0,
                "opts": "RR",
            },
            {
                "returncode": 0,
                "stdout": """\
PING 192.0.2.2 (192.0.2.2): 56 data bytes
64 bytes from: icmp_seq=0 ttl= time= ms
RR: 	192.0.2.10
	192.0.2.20
	192.0.2.30
	192.0.2.40
	192.0.2.50
	192.0.2.60
	192.0.2.70
	192.0.2.80
	192.0.2.90

--- 192.0.2.2 ping statistics ---
1 packets transmitted, 1 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
                "redacted": True,
            },
            id="_0_0_opts_RR",
        ),
        pytest.param(
            {
                "src": "192.0.2.1",
                "dst": "192.0.2.2",
                "icmp_type": 0,
                "icmp_code": 0,
                "opts": "RR-same",
            },
            {
                "returncode": 0,
                "stdout": """\
PING 192.0.2.2 (192.0.2.2): 56 data bytes
64 bytes from: icmp_seq=0 ttl= time= ms	(same route)

--- 192.0.2.2 ping statistics ---
1 packets transmitted, 1 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
                "redacted": True,
            },
            id="_0_0_opts_RR_same",
        ),
        pytest.param(
            {
                "src": "192.0.2.1",
                "dst": "192.0.2.2",
                "icmp_type": 0,
                "icmp_code": 0,
                "opts": "RR-trunc",
            },
            {
                "returncode": 0,
                "stdout": """\
PING 192.0.2.2 (192.0.2.2): 56 data bytes
64 bytes from: icmp_seq=0 ttl= time= ms
RR: 	(truncated route)

--- 192.0.2.2 ping statistics ---
1 packets transmitted, 1 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
                "redacted": True,
            },
            id="_0_0_opts_RR_trunc",
        ),
        pytest.param(
            {
                "src": "192.0.2.1",
                "dst": "192.0.2.2",
                "icmp_type": 0,
                "icmp_code": 0,
                "opts": "NOP",
            },
            {
                "returncode": 0,
                "stdout": """\
PING 192.0.2.2 (192.0.2.2): 56 data bytes
64 bytes from: icmp_seq=0 ttl= time= ms
wrong total length 88 instead of 84
NOP

--- 192.0.2.2 ping statistics ---
1 packets transmitted, 1 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
                "redacted": True,
            },
            id="_0_0_opts_NOP",
        ),
        pytest.param(
            {
                "src": "192.0.2.1",
                "dst": "192.0.2.2",
                "icmp_type": 3,
                "icmp_code": 1,
                "ihl": 0x4,
            },
            {
                "returncode": 2,
                "stdout": """\
PING 192.0.2.2 (192.0.2.2): 56 data bytes

--- 192.0.2.2 ping statistics ---
1 packets transmitted, 0 packets received, 100.0% packet loss
""",
                "stderr": "",  # "IHL too short" message not shown
                "redacted": False,
            },
            id="_IHL_too_short",
        ),
        pytest.param(
            {
                "src": "192.0.2.1",
                "dst": "192.0.2.2",
                "icmp_type": 3,
                "icmp_code": 1,
                "special": "no-payload",
            },
            {
                "returncode": 2,
                "stdout": """\
PATTERN: 0x01
PING 192.0.2.2 (192.0.2.2): 56 data bytes

--- 192.0.2.2 ping statistics ---
1 packets transmitted, 0 packets received, 100.0% packet loss
""",
                "stderr": """\
ping: quoted data too short (28 bytes) from 192.0.2.2
""",
                "redacted": False,
            },
            id="_quoted_data_too_short",
        ),
        pytest.param(
            {
                "src": "192.0.2.1",
                "dst": "192.0.2.2",
                "icmp_type": 3,
                "icmp_code": 1,
                "oip_ihl": 0x4,
            },
            {
                "returncode": 2,
                "stdout": """\
PING 192.0.2.2 (192.0.2.2): 56 data bytes

--- 192.0.2.2 ping statistics ---
1 packets transmitted, 0 packets received, 100.0% packet loss
""",
                "stderr": "",  # "inner IHL too short" message not shown
                "redacted": False,
            },
            id="_inner_IHL_too_short",
        ),
        pytest.param(
            {
                "src": "192.0.2.1",
                "dst": "192.0.2.2",
                "icmp_type": 3,
                "icmp_code": 1,
                "oip_ihl": 0xF,
            },
            {
                "returncode": 2,
                "stdout": """\
PING 192.0.2.2 (192.0.2.2): 56 data bytes

--- 192.0.2.2 ping statistics ---
1 packets transmitted, 0 packets received, 100.0% packet loss
""",
                "stderr": """\
ping: inner packet too short (84 bytes) from 192.0.2.2
""",
                "redacted": False,
            },
            id="_inner_packet_too_short",
        ),
        pytest.param(
            {
                "src": "192.0.2.1",
                "dst": "192.0.2.2",
                "icmp_type": 3,
                "icmp_code": 1,
                "oip_ihl": 0xF,
                "special": "no-payload",
            },
            {
                "returncode": 2,
                "stdout": """\
PATTERN: 0x01
PING 192.0.2.2 (192.0.2.2): 56 data bytes

--- 192.0.2.2 ping statistics ---
1 packets transmitted, 0 packets received, 100.0% packet loss
""",
                "stderr": "",
                "redacted": False,
            },
            id="_max_inner_packet_ihl_without_payload",
        ),
        pytest.param(
            {
                "src": "192.0.2.1",
                "dst": "192.0.2.2",
                "icmp_type": 0,
                "icmp_code": 0,
                "opts": "NOP-40",
            },
            {
                "returncode": 0,
                "stdout": """\
PING 192.0.2.2 (192.0.2.2): 56 data bytes
64 bytes from: icmp_seq=0 ttl= time= ms
wrong total length 124 instead of 84
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP

--- 192.0.2.2 ping statistics ---
1 packets transmitted, 1 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
                "redacted": True,
            },
            id="_0_0_opts_NOP_40",
        ),
        pytest.param(
            {
                "src": "192.0.2.1",
                "dst": "192.0.2.2",
                "icmp_type": 0,
                "icmp_code": 0,
                "opts": "unk",
            },
            {
                "returncode": 0,
                "stdout": """\
PING 192.0.2.2 (192.0.2.2): 56 data bytes
64 bytes from: icmp_seq=0 ttl= time= ms
wrong total length 88 instead of 84
unknown option 9f

--- 192.0.2.2 ping statistics ---
1 packets transmitted, 1 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
                "redacted": True,
            },
            id="_0_0_opts_unk",
        ),
        pytest.param(
            {
                "src": "192.0.2.1",
                "dst": "192.0.2.2",
                "icmp_type": 3,
                "icmp_code": 1,
                "opts": "NOP-40",
            },
            {
                "returncode": 2,
                "stdout": """\
PING 192.0.2.2 (192.0.2.2): 56 data bytes
132 bytes from 192.0.2.2: Destination Host Unreachable
Vr HL TOS  Len   ID Flg  off TTL Pro  cks       Src       Dst Opts
 4  f  00 007c 0001   0 0000  40  01 d868 192.0.2.1 192.0.2.2 01010101010101010101010101010101010101010101010101010101010101010101010101010101


--- 192.0.2.2 ping statistics ---
1 packets transmitted, 0 packets received, 100.0% packet loss
""",
                "stderr": "",
                "redacted": False,
            },
            id="_3_1_opts_NOP_40",
        ),
        pytest.param(
            {
                "src": "192.0.2.1",
                "dst": "192.0.2.2",
                "icmp_type": 3,
                "icmp_code": 1,
                "flags": "DF",
            },
            {
                "returncode": 2,
                "stdout": """\
PING 192.0.2.2 (192.0.2.2): 56 data bytes
92 bytes from 192.0.2.2: Destination Host Unreachable
Vr HL TOS  Len   ID Flg  off TTL Pro  cks       Src       Dst
 4  5  00 0054 0001   2 0000  40  01 b6a4 192.0.2.1 192.0.2.2


--- 192.0.2.2 ping statistics ---
1 packets transmitted, 0 packets received, 100.0% packet loss
""",
                "stderr": "",
                "redacted": False,
            },
            id="_3_1_flags_DF",
        ),
        pytest.param(
            {
                "src": "192.0.2.1",
                "dst": "192.0.2.2",
                "icmp_type": 3,
                "icmp_code": 1,
                "special": "tcp",
            },
            {
                "returncode": 2,
                "stdout": """\
PATTERN: 0x01
PING 192.0.2.2 (192.0.2.2): 56 data bytes

--- 192.0.2.2 ping statistics ---
1 packets transmitted, 0 packets received, 100.0% packet loss
""",
                "stderr": """\
ping: quoted data too short (40 bytes) from 192.0.2.2
""",
                "redacted": False,
            },
            id="_3_1_special_tcp",
        ),
        pytest.param(
            {
                "src": "192.0.2.1",
                "dst": "192.0.2.2",
                "icmp_type": 3,
                "icmp_code": 1,
                "special": "udp",
            },
            {
                "returncode": 2,
                "stdout": """\
PATTERN: 0x01
PING 192.0.2.2 (192.0.2.2): 56 data bytes

--- 192.0.2.2 ping statistics ---
1 packets transmitted, 0 packets received, 100.0% packet loss
""",
                "stderr": """\
ping: quoted data too short (28 bytes) from 192.0.2.2
""",
                "redacted": False,
            },
            id="_3_1_special_udp",
        ),
        pytest.param(
            {
                "src": "192.0.2.1",
                "dst": "192.0.2.2",
                "icmp_type": 3,
                "icmp_code": 1,
                "verbose": False,
            },
            {
                "returncode": 2,
                "stdout": """\
PING 192.0.2.2 (192.0.2.2): 56 data bytes
92 bytes from 192.0.2.2: Destination Host Unreachable
Vr HL TOS  Len   ID Flg  off TTL Pro  cks       Src       Dst
 4  5  00 0054 0001   0 0000  40  01 f6a4 192.0.2.1 192.0.2.2


--- 192.0.2.2 ping statistics ---
1 packets transmitted, 0 packets received, 100.0% packet loss
""",
                "stderr": "",
                "redacted": False,
            },
            id="_3_1_verbose_false",
        ),
        pytest.param(
            {
                "src": "192.0.2.1",
                "dst": "192.0.2.2",
                "icmp_type": 3,
                "icmp_code": 1,
                "special": "not-mine",
                "verbose": False,
            },
            {
                "returncode": 2,
                "stdout": """\
PATTERN: 0x01
PING 192.0.2.2 (192.0.2.2): 56 data bytes

--- 192.0.2.2 ping statistics ---
1 packets transmitted, 0 packets received, 100.0% packet loss
""",
                "stderr": "",
                "redacted": False,
            },
            id="_3_1_special_not_mine_verbose_false",
        ),
        pytest.param(
            {
                "src": "192.0.2.1",
                "dst": "192.0.2.2",
                "icmp_type": 0,
                "icmp_code": 0,
                "special": "warp",
            },
            {
                "returncode": 0,
                "stdout": """\
PATTERN: 0x01
PING 192.0.2.2 (192.0.2.2): 56 data bytes
64 bytes from: icmp_seq=0 ttl= time= ms

--- 192.0.2.2 ping statistics ---
1 packets transmitted, 1 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": """\
ping: time of day goes back (- ms), clamping time to 0
""",
                "redacted": True,
            },
            id="_0_0_special_warp",
        ),
        pytest.param(
            {
                "src": "192.0.2.1",
                "dst": "192.0.2.2",
                "icmp_type": 0,
                "icmp_code": 0,
                "special": "wrong",
            },
            {
                "returncode": 0,
                "stdout": """\
PATTERN: 0x01
PING 192.0.2.2 (192.0.2.2): 56 data bytes
64 bytes from: icmp_seq=0 ttl= time= ms
wrong data byte #55 should be 0x1 but was 0x0
cp: xx xx xx xx xx xx xx xx
	  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1
	  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1
	  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  0
dp: xx xx xx xx xx xx xx xx
	  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1
	  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1
	  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1

--- 192.0.2.2 ping statistics ---
1 packets transmitted, 1 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = /// ms
""",
                "stderr": "",
                "redacted": True,
            },
            id="_0_0_special_wrong",
        ),
    ]

    @pytest.mark.parametrize("pinger_kargs, expected", pinger_testdata)
    @pytest.mark.require_progs(["scapy"])
    @pytest.mark.require_user("root")
    def test_pinger(self, pinger_kargs, expected):
        """Test ping using pinger(), a reply faker"""
        iface = IfaceFactory().create_iface("", "tun")[0].name
        ping = pinger(iface, **pinger_kargs)
        assert ping.returncode == expected["returncode"]
        if expected["redacted"]:
            assert redact(ping.stdout) == expected["stdout"]
            assert redact(ping.stderr) == expected["stderr"]
        else:
            assert ping.stdout == expected["stdout"]
            assert ping.stderr == expected["stderr"]
