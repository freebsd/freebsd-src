#
# Copyright (c) 2025 Stormshield
#
# SPDX-License-Identifier: BSD-2-Clause
#

import pytest
import socket
import struct
import subprocess
import time
from pathlib import Path

from atf_python.sys.net.vnet import VnetTestTemplate


class MRouteTestTemplate(VnetTestTemplate):
    """
    Helper class for multicast routing tests.  Test classes should inherit from this one.
    """
    COORD_SOCK = "coord.sock"

    @staticmethod
    def _msgwait(sock: socket.socket, expected: bytes):
        msg = sock.recv(1024)
        assert msg == expected

    @staticmethod
    def sendmsg(msg: bytes, path: str):
        s = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        s.sendto(msg, path)
        s.close()

    @staticmethod
    def _makesock(path: str):
        s = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        s.bind(path)
        return s

    @staticmethod
    def mcast_join_INET6(addr: str, port: int):
        pass

    def jointest(self, vnet):
        """Let the coordinator know that we're ready, and wait for go-ahead."""
        coord = self._makesock(vnet.alias + ".sock")
        self.sendmsg(b"ok " + vnet.alias.encode(), self.COORD_SOCK)
        self._msgwait(coord, b"join")

    def donetest(self):
        """Let the coordinator that we completed successfully."""
        self.sendmsg(b"done", self.COORD_SOCK)

    def starttest(self, vnets: list[str]):
        self.vnets = vnets
        for vnet in vnets:
            self.sendmsg(b"join", vnet + ".sock")

    def waittest(self):
        for vnet in self.vnets:
            self._msgwait(self.coord, b"done")

    def setup_method(self, method):
        self.coord = self._makesock(self.COORD_SOCK)
        super().setup_method(method)

        # Loop until all other hosts have sent the ok message.
        received = set()
        vnet_names = set(self.vnet_map.keys()) - {self.vnet.alias}
        while len(received) < len(vnet_names):
            msg = self.coord.recv(1024)
            received.add(msg)
        assert received == {b"ok " + name.encode() for name in vnet_names}


class MRouteINETTestTemplate(MRouteTestTemplate):
    @staticmethod
    def run_pimd(ident: str, ifaces: list[str], rpaddr: str, group: str, fib=0):
        conf = f"pimd-{ident}.conf"
        with open(conf, "w") as conf_file:
            conf_file.write("no phyint\n")
            for iface in ifaces:
                conf_file.write(f"phyint {iface} enable\n")
            conf_file.write(f"rp-address {rpaddr} {group}\n")

        cmd = f"setfib {fib} pimd -i {ident} -f {conf} -p pimd-{ident}.pid -n"
        return subprocess.Popen(cmd.split(), stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL)

    @staticmethod
    def mcast_join(addr: str, port: int):
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        mreq = struct.pack("4si", socket.inet_aton(addr), socket.INADDR_ANY)
        s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
        s.bind((addr, port))
        time.sleep(1)  # Give the kernel a bit of time to join the group.
        return s

    @staticmethod
    def mcast_sendto(addr: str, port: int, iface: str, msg: bytes):
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        mreqn = struct.pack("iii", socket.INADDR_ANY, socket.INADDR_ANY,
                            socket.if_nametoindex(iface))
        s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, mreqn)
        s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 64)
        s.sendto(msg, (addr, port))
        s.close()

    def setup_method(self, method):
        self.require_module("ip_mroute")
        super().setup_method(method)


class MRouteINET6TestTemplate(MRouteTestTemplate):
    @staticmethod
    def run_ip6_mrouted(ident: str, ifaces: list[str], fib=0):
        ifaces_str = ' '.join(f"-i {iface}" for iface in ifaces)
        exepath = Path(__file__).parent / "ip6_mrouted"
        cmd = f"setfib {fib} {exepath} {ifaces_str}"
        return subprocess.Popen(cmd.split(), stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL)

    @staticmethod
    def mcast_join(addr: str, port: int, iface: str):
        s = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        mreq = struct.pack("16si", socket.inet_pton(socket.AF_INET6, addr),
                            socket.if_nametoindex(iface))
        s.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_JOIN_GROUP, mreq)
        s.bind((addr, port))
        time.sleep(1)  # Give the kernel a bit of time to join the
        return s

    @staticmethod
    def mcast_sendto(addr: str, port: int, iface: str, msg: bytes):
        s = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        mreq = struct.pack("i", socket.if_nametoindex(iface))
        s.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_MULTICAST_IF, mreq)
        s.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_MULTICAST_HOPS, 64)
        s.sendto(msg, (addr, port))
        s.close()

    def setup_method(self, method):
        self.require_module("ip6_mroute")
        super().setup_method(method)


class Test1RBasicINET(MRouteINETTestTemplate):
    """Basic multicast routing setup with 2 hosts connected via a router."""

    TOPOLOGY = {
        "vnet_router": {"ifaces": ["if1", "if2"]},
        "vnet_host1": {"ifaces": ["if1"]},
        "vnet_host2": {"ifaces": ["if2"]},
        "if1": {"prefixes4": [("192.168.1.1/24", "192.168.1.2/24")]},
        "if2": {"prefixes4": [("192.168.2.1/24", "192.168.2.2/24")]},
    }
    MULTICAST_ADDR = "239.0.0.1"

    def setup_method(self, method):
        # Create VNETs and start the handlers.
        super().setup_method(method)

        ifaces = [self.vnet.iface_alias_map[i].name for i in ["if1", "if2"]]
        self.pimd = self.run_pimd("test", ifaces, "127.0.0.1", self.MULTICAST_ADDR + "/32")
        time.sleep(3)  # Give pimd a bit of time to get itself together.

    def vnet_host1_handler(self, vnet):
        self.jointest(vnet)

        self.sock = self.mcast_join(self.MULTICAST_ADDR, 12345)

        # Wait for host 2 to send a message, then send a reply.
        self._msgwait(self.sock, b"Hello, Multicast!")
        self.mcast_sendto(self.MULTICAST_ADDR, 12345, vnet.ifaces[0].name,
                          b"Goodbye, Multicast!")
        self._msgwait(self.sock, b"Goodbye, Multicast!")
        self.donetest()

    def vnet_host2_handler(self, vnet):
        self.jointest(vnet)

        self.sock = self.mcast_join(self.MULTICAST_ADDR, 12345)

        # Send a message to host 1, then wait for a reply.
        self.mcast_sendto(self.MULTICAST_ADDR, 12345, vnet.ifaces[0].name,
                          b"Hello, Multicast!")
        self._msgwait(self.sock, b"Hello, Multicast!")
        self._msgwait(self.sock, b"Goodbye, Multicast!")
        self.donetest()

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["pimd"])
    @pytest.mark.timeout(30)
    def test(self):
        self.starttest(["vnet_host1", "vnet_host2"])
        self.waittest()


class Test1RCrissCrossINET(MRouteINETTestTemplate):
    """
    Test a router connected to four hosts, with pairs of interfaces
    in different FIBs.
    """

    TOPOLOGY = {
        "vnet_router": {"ifaces": ["if1", "if2", "if3", "if4"]},
        "vnet_host1": {"ifaces": ["if1"]},
        "vnet_host2": {"ifaces": ["if2"]},
        "vnet_host3": {"ifaces": ["if3"]},
        "vnet_host4": {"ifaces": ["if4"]},
        "if1": {
            "prefixes4": [("192.168.1.1/24", "192.168.1.2/24")],
            "prefixes6": [],
            "fib": (0, 0),
        },
        "if2": {
            "prefixes4": [("192.168.2.1/24", "192.168.2.2/24")],
            "prefixes6": [],
            "fib": (0, 0),
        },
        "if3": {
            "prefixes4": [("192.168.3.1/24", "192.168.3.2/24")],
            "prefixes6": [],
            "fib": (1, 0),
        },
        "if4": {
            "prefixes4": [("192.168.4.1/24", "192.168.4.2/24")],
            "prefixes6": [],
            "fib": (1, 0),
        },
    }
    MULTICAST_ADDR = "239.0.0.1"

    def setup_method(self, method):
        # Create VNETs and start the handlers.
        super().setup_method(method)

        # Start a pimd instance per FIB.
        ifaces = [self.vnet.iface_alias_map[i].name for i in ["if1", "if2"]]
        self.pimd0 = self.run_pimd("test0", ifaces, "127.0.0.1", self.MULTICAST_ADDR + "/32",
                                   fib=0)
        ifaces = [self.vnet.iface_alias_map[i].name for i in ["if3", "if4"]]
        self.pimd1 = self.run_pimd("test1", ifaces, "127.0.0.1", self.MULTICAST_ADDR + "/32",
                                   fib=1)
        time.sleep(3)  # Give pimd a bit of time to get itself together.

    def vnet_host1_handler(self, vnet):
        self.jointest(vnet)

        self.sock = self.mcast_join(self.MULTICAST_ADDR, 12345)
        self._msgwait(self.sock, b"Hello, Multicast on FIB 0!")
        self.mcast_sendto(self.MULTICAST_ADDR, 12345, vnet.ifaces[0].name,
                          b"Goodbye, Multicast on FIB 0!")
        self.donetest()

    def vnet_host2_handler(self, vnet):
        self.jointest(vnet)
        self.sock = self.mcast_join(self.MULTICAST_ADDR, 12345)
        self.mcast_sendto(self.MULTICAST_ADDR, 12345, vnet.ifaces[0].name,
                          b"Hello, Multicast on FIB 0!")
        self._msgwait(self.sock, b"Hello, Multicast on FIB 0!")
        self._msgwait(self.sock, b"Goodbye, Multicast on FIB 0!")
        self.donetest()

    def vnet_host3_handler(self, vnet):
        self.jointest(vnet)
        self.sock = self.mcast_join(self.MULTICAST_ADDR, 12345)
        self._msgwait(self.sock, b"Hello, Multicast on FIB 1!")
        self.mcast_sendto(self.MULTICAST_ADDR, 12345,
                          vnet.ifaces[0].name, b"Goodbye, Multicast on FIB 1!")
        self.donetest()

    def vnet_host4_handler(self, vnet):
        self.jointest(vnet)
        self.sock = self.mcast_join(self.MULTICAST_ADDR, 12345)
        time.sleep(1)
        self.mcast_sendto(self.MULTICAST_ADDR, 12345, vnet.ifaces[0].name,
                          b"Hello, Multicast on FIB 1!")
        self._msgwait(self.sock, b"Hello, Multicast on FIB 1!")
        self._msgwait(self.sock, b"Goodbye, Multicast on FIB 1!")
        self.donetest()

    @pytest.mark.require_user("root")
    @pytest.mark.require_progs(["pimd"])
    @pytest.mark.timeout(30)
    def test(self):
        self.starttest(["vnet_host1", "vnet_host2", "vnet_host3", "vnet_host4"])
        self.waittest()


class Test1RBasicINET6(MRouteINET6TestTemplate):
    """Basic multicast routing setup with 2 hosts connected via a router."""

    TOPOLOGY = {
        "vnet_router": {"ifaces": ["if1", "if2"]},
        "vnet_host1": {"ifaces": ["if1"]},
        "vnet_host2": {"ifaces": ["if2"]},
        "if1": {
            "prefixes6": [("2001:db8:0:1::1/64", "2001:db8:0:1::2/64")]
        },
        "if2": {
            "prefixes6": [("2001:db8:0:2::1/64", "2001:db8:0:2::2/64")]
        },
    }
    MULTICAST_ADDR = "ff05::1"

    def setup_method(self, method):
        # Create VNETs and start the handlers.
        super().setup_method(method)

        ifaces = [self.vnet.iface_alias_map[i].name for i in ["if1", "if2"]]
        self.mrouted = self.run_ip6_mrouted("test", ifaces)
        time.sleep(1)

    def vnet_host1_handler(self, vnet):
        self.jointest(vnet)

        self.sock = self.mcast_join(self.MULTICAST_ADDR, 12345, vnet.ifaces[0].name)

        # Wait for host 2 to send a message, then send a reply.
        self._msgwait(self.sock, b"Hello, Multicast!")
        self.mcast_sendto(self.MULTICAST_ADDR, 12345, vnet.ifaces[0].name,
                          b"Goodbye, Multicast!")
        self._msgwait(self.sock, b"Goodbye, Multicast!")
        self.donetest()

    def vnet_host2_handler(self, vnet):
        self.jointest(vnet)

        self.sock = self.mcast_join(self.MULTICAST_ADDR, 12345, vnet.ifaces[0].name)

        # Send a message to host 1, then wait for a reply.
        self.mcast_sendto(self.MULTICAST_ADDR, 12345, vnet.ifaces[0].name,
                          b"Hello, Multicast!")
        self._msgwait(self.sock, b"Hello, Multicast!")
        self._msgwait(self.sock, b"Goodbye, Multicast!")
        self.donetest()

    @pytest.mark.require_user("root")
    @pytest.mark.timeout(30)
    def test(self):
        self.starttest(["vnet_host1", "vnet_host2"])
        self.waittest()


class Test1RCrissCrossINET6(MRouteINET6TestTemplate):
    """
    Test a router connected to four hosts, with pairs of interfaces
    in different FIBs.
    """

    TOPOLOGY = {
        "vnet_router": {"ifaces": ["if1", "if2", "if3", "if4"]},
        "vnet_host1": {"ifaces": ["if1"]},
        "vnet_host2": {"ifaces": ["if2"]},
        "vnet_host3": {"ifaces": ["if3"]},
        "vnet_host4": {"ifaces": ["if4"]},
        "if1": {
            "prefixes6": [("2001:db8:0:1::1/64", "2001:db8:0:1::2/64")],
            "fib": (0, 0),
        },
        "if2": {
            "prefixes6": [("2001:db8:0:2::1/64", "2001:db8:0:2::2/64")],
            "fib": (0, 0),
        },
        "if3": {
            "prefixes6": [("2001:db8:0:3::1/64", "2001:db8:0:3::2/64")],
            "fib": (1, 0),
        },
        "if4": {
            "prefixes6": [("2001:db8:0:4::1/64", "2001:db8:0:4::2/64")],
            "fib": (1, 0),
        },
    }
    MULTICAST_ADDR = "ff05::1"

    def setup_method(self, method):
        # Create VNETs and start the handlers.
        super().setup_method(method)

        # Start an ip6_mrouted instance per FIB.
        ifaces = [self.vnet.iface_alias_map[i].name for i in ["if1", "if2"]]
        self.pimd0 = self.run_ip6_mrouted("test0", ifaces, fib=0)
        ifaces = [self.vnet.iface_alias_map[i].name for i in ["if3", "if4"]]
        self.pimd1 = self.run_ip6_mrouted("test1", ifaces, fib=1)
        time.sleep(1)  # Give ip6_mrouted a bit of time to get itself together.

    def vnet_host1_handler(self, vnet):
        self.jointest(vnet)

        self.sock = self.mcast_join(self.MULTICAST_ADDR, 12345, vnet.ifaces[0].name)
        self._msgwait(self.sock, b"Hello, Multicast on FIB 0!")
        self.mcast_sendto(self.MULTICAST_ADDR, 12345, vnet.ifaces[0].name,
                          b"Goodbye, Multicast on FIB 0!")
        self.donetest()

    def vnet_host2_handler(self, vnet):
        self.jointest(vnet)
        self.sock = self.mcast_join(self.MULTICAST_ADDR, 12345, vnet.ifaces[0].name)
        self.mcast_sendto(self.MULTICAST_ADDR, 12345, vnet.ifaces[0].name,
                          b"Hello, Multicast on FIB 0!")
        self._msgwait(self.sock, b"Hello, Multicast on FIB 0!")
        self._msgwait(self.sock, b"Goodbye, Multicast on FIB 0!")
        self.donetest()

    def vnet_host3_handler(self, vnet):
        self.jointest(vnet)
        self.sock = self.mcast_join(self.MULTICAST_ADDR, 12345, vnet.ifaces[0].name)
        self._msgwait(self.sock, b"Hello, Multicast on FIB 1!")
        self.mcast_sendto(self.MULTICAST_ADDR, 12345,
                          vnet.ifaces[0].name, b"Goodbye, Multicast on FIB 1!")
        self.donetest()

    def vnet_host4_handler(self, vnet):
        self.jointest(vnet)
        self.sock = self.mcast_join(self.MULTICAST_ADDR, 12345, vnet.ifaces[0].name)
        time.sleep(1)
        self.mcast_sendto(self.MULTICAST_ADDR, 12345, vnet.ifaces[0].name,
                          b"Hello, Multicast on FIB 1!")
        self._msgwait(self.sock, b"Hello, Multicast on FIB 1!")
        self._msgwait(self.sock, b"Goodbye, Multicast on FIB 1!")
        self.donetest()

    @pytest.mark.require_user("root")
    @pytest.mark.timeout(30)
    def test(self):
        self.starttest(["vnet_host1", "vnet_host2", "vnet_host3", "vnet_host4"])
        self.waittest()
