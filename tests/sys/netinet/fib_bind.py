#
# Copyright (c) 2026 Stormshield
#
# SPDX-License-Identifier: BSD-2-Clause
#

import pytest
import errno
import os
import socket
import struct
import sys
import logging

from atf_python.sys.net.vnet import VnetTestTemplate
from atf_python.sys.net.tools import ToolsHelper

def _common(tunable, addr, domain, type, proto):
    """
    Test what happens when we try to bind a socket to an
    address that is not present in the current FIB.
    """
    sysctl_output = ToolsHelper.get_output(f"sysctl {tunable}")
    tunable_val = int(sysctl_output.split(":")[1])
    if tunable_val != 0:
        pytest.skip(f"{tunable} must be set to 0")

    port = 12345 if type != socket.SOCK_RAW else 0
    s = socket.socket(domain, type, proto)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_SETFIB, 0)

    failed = False
    try:
        s.bind((addr, port))
    except OSError as e:
        assert e.errno == errno.EADDRNOTAVAIL
        failed = True
    assert failed

    s.setsockopt(socket.SOL_SOCKET, socket.SO_SETFIB, 1)

    failed = False
    try:
        s.bind((addr, port))
    except OSError:
        failed = True
    assert not failed
    s.close()

class TestFibBind(VnetTestTemplate):
    REQUIRED_MODULES = []
    TOPOLOGY = {
        "vnet1": { "ifaces": [ "if1", "if2" ] },
        "if1": {
            "prefixes4": [("192.168.1.1/24", "192.168.1.2/24")],
            "fib": (0, 0),
        },
        "if2": {
            "prefixes4": [("192.168.2.1/24", "192.168.2.2/24")],
            "fib": (1, 0),
        },
    }

    def setup_method(self, method):
        super().setup_method(method)

    def test_TCP(self):
        _common("net.inet.tcp.bind_all_fibs", "192.168.2.1", socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_TCP)

    def test_UDP(self):
        _common("net.inet.udp.bind_all_fibs", "192.168.2.1", socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)

    def test_RAW(self):
        _common("net.inet.raw.bind_all_fibs", "192.168.2.1", socket.AF_INET, socket.SOCK_RAW, socket.IPPROTO_IP)

class TestFibBind6(VnetTestTemplate):
    REQUIRED_MODULES = []
    TOPOLOGY = {
        "vnet1": { "ifaces": [ "if1", "if2" ] },
        "if1": {
            "prefixes6": [("2001:db8:0:1::1/64", "2001:db8:0:1::2/64")],
            "fib": (0, 0),
        },
        "if2": {
            "prefixes6": [("2001:db8:0:2::1/64", "2001:db8:0:2::2/64")],
            "fib": (1, 0),
        },
    }

    def setup_method(self, method):
        super().setup_method(method)

    def test_TCP(self):
        _common("net.inet.tcp.bind_all_fibs", "2001:db8:0:2::1", socket.AF_INET6, socket.SOCK_STREAM, socket.IPPROTO_TCP)

    def test_UDP(self):
        _common("net.inet.udp.bind_all_fibs", "2001:db8:0:2::1", socket.AF_INET6, socket.SOCK_DGRAM, socket.IPPROTO_UDP)

    def test_RAW(self):
        _common("net.inet.raw.bind_all_fibs", "2001:db8:0:2::1", socket.AF_INET6, socket.SOCK_RAW, socket.IPPROTO_IPV6)
