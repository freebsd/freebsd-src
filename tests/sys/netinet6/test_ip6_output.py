import errno
import ipaddress
import socket
import struct
import time
from ctypes import c_byte
from ctypes import c_uint
from ctypes import Structure

import pytest
from atf_python.sys.net.rtsock import SaHelper
from atf_python.sys.net.tools import ToolsHelper
from atf_python.sys.net.vnet import run_cmd
from atf_python.sys.net.vnet import SingleVnetTestTemplate
from atf_python.sys.net.vnet import VnetTestTemplate


class In6Pktinfo(Structure):
    _fields_ = [
        ("ipi6_addr", c_byte * 16),
        ("ipi6_ifindex", c_uint),
    ]


class VerboseSocketServer:
    def __init__(self, ip: str, port: int, ifname: str = None):
        self.ip = ip
        self.port = port

        s = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        s.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_RECVPKTINFO, 1)
        addr = ipaddress.ip_address(ip)
        if addr.is_link_local and ifname:
            ifindex = socket.if_nametoindex(ifname)
            addr_tuple = (ip, port, 0, ifindex)
        elif addr.is_multicast and ifname:
            ifindex = socket.if_nametoindex(ifname)
            mreq = socket.inet_pton(socket.AF_INET6, ip) + struct.pack("I", ifindex)
            s.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_JOIN_GROUP, mreq)
            print("## JOINED group {} % {}".format(ip, ifname))
            addr_tuple = ("::", port, 0, ifindex)
        else:
            addr_tuple = (ip, port, 0, 0)
        print("## Listening on [{}]:{}".format(addr_tuple[0], port))
        s.bind(addr_tuple)
        self.socket = s

    def recv(self):
        # data = self.socket.recv(4096)
        # print("RX: " + data)
        data, ancdata, msg_flags, address = self.socket.recvmsg(4096, 128)
        # Assume ancdata has just 1 item
        info = In6Pktinfo.from_buffer_copy(ancdata[0][2])
        dst_ip = socket.inet_ntop(socket.AF_INET6, info.ipi6_addr)
        dst_iface = socket.if_indextoname(info.ipi6_ifindex)

        tx_obj = {
            "data": data,
            "src_ip": address[0],
            "dst_ip": dst_ip,
            "dst_iface": dst_iface,
        }
        return tx_obj


class BaseTestIP6Ouput(VnetTestTemplate):
    TOPOLOGY = {
        "vnet1": {"ifaces": ["if1", "if2", "if3"]},
        "vnet2": {"ifaces": ["if1", "if2", "if3"]},
        "if1": {"prefixes6": [("2001:db8:a::1/64", "2001:db8:a::2/64")]},
        "if2": {"prefixes6": [("2001:db8:b::1/64", "2001:db8:b::2/64")]},
        "if3": {"prefixes6": [("2001:db8:c::1/64", "2001:db8:c::2/64")]},
    }
    DEFAULT_PORT = 45365

    def _vnet2_handler(self, vnet, obj_map, pipe, ip: str, os_ifname: str = None):
        """Generic listener that sends first received packet with metadata
        back to the sender via pipw
        """
        ll_data = ToolsHelper.get_linklocals()
        # Start listener
        ss = VerboseSocketServer(ip, self.DEFAULT_PORT, os_ifname)
        pipe.send(ll_data)

        tx_obj = ss.recv()
        tx_obj["dst_iface_alias"] = vnet.iface_map[tx_obj["dst_iface"]].alias
        pipe.send(tx_obj)


class TestIP6Output(BaseTestIP6Ouput):
    def vnet2_handler(self, vnet, obj_map, pipe):
        ip = str(vnet.iface_alias_map["if2"].first_ipv6.ip)
        self._vnet2_handler(vnet, obj_map, pipe, ip, None)

    @pytest.mark.require_user("root")
    def test_output6_base(self):
        """Tests simple UDP output"""
        second_vnet = self.vnet_map["vnet2"]

        # Pick target on if2 vnet2's end
        ifaddr = ipaddress.ip_interface(self.TOPOLOGY["if2"]["prefixes6"][0][1])
        ip = str(ifaddr.ip)

        s = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        data = bytes("AAAA", "utf-8")
        print("## TX packet to {},{}".format(ip, self.DEFAULT_PORT))

        # Wait for the child to become ready
        self.wait_object(second_vnet.pipe)
        s.sendto(data, (ip, self.DEFAULT_PORT))

        # Wait for the received object
        rx_obj = self.wait_object(second_vnet.pipe)
        assert rx_obj["dst_ip"] == ip
        assert rx_obj["dst_iface_alias"] == "if2"

    @pytest.mark.require_user("root")
    def test_output6_nhop(self):
        """Tests UDP output with custom nhop set"""
        second_vnet = self.vnet_map["vnet2"]

        # Pick target on if2 vnet2's end
        ifaddr = ipaddress.ip_interface(self.TOPOLOGY["if2"]["prefixes6"][0][1])
        ip_dst = str(ifaddr.ip)
        # Pick nexthop on if1
        ifaddr = ipaddress.ip_interface(self.TOPOLOGY["if1"]["prefixes6"][0][1])
        ip_next = str(ifaddr.ip)
        sin6_next = SaHelper.ip6_sa(ip_next, 0)

        s = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM, 0)
        s.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_NEXTHOP, sin6_next)

        # Wait for the child to become ready
        self.wait_object(second_vnet.pipe)
        data = bytes("AAAA", "utf-8")
        s.sendto(data, (ip_dst, self.DEFAULT_PORT))

        # Wait for the received object
        rx_obj = self.wait_object(second_vnet.pipe)
        assert rx_obj["dst_ip"] == ip_dst
        assert rx_obj["dst_iface_alias"] == "if1"

    @pytest.mark.parametrize(
        "params",
        [
            # esrc: src-ip, if: src-interface, esrc: expected-src,
            # eif: expected-rx-interface
            pytest.param({"esrc": "2001:db8:b::1", "eif": "if2"}, id="empty"),
            pytest.param(
                {"src": "2001:db8:c::1", "esrc": "2001:db8:c::1", "eif": "if2"},
                id="iponly1",
            ),
            pytest.param(
                {
                    "src": "2001:db8:c::1",
                    "if": "if3",
                    "ex": errno.EHOSTUNREACH,
                },
                id="ipandif",
            ),
            pytest.param(
                {
                    "src": "2001:db8:c::aaaa",
                    "ex": errno.EADDRNOTAVAIL,
                },
                id="nolocalip",
            ),
            pytest.param(
                {"if": "if2", "src": "2001:db8:b::1", "eif": "if2"}, id="ifsame"
            ),
        ],
    )
    @pytest.mark.require_user("root")
    def test_output6_pktinfo(self, params):
        """Tests simple UDP output"""
        second_vnet = self.vnet_map["vnet2"]
        vnet = self.vnet

        # Pick target on if2 vnet2's end
        ifaddr = ipaddress.ip_interface(self.TOPOLOGY["if2"]["prefixes6"][0][1])
        dst_ip = str(ifaddr.ip)

        src_ip = params.get("src", "")
        src_ifname = params.get("if", "")
        expected_ip = params.get("esrc", "")
        expected_ifname = params.get("eif", "")
        errno = params.get("ex", 0)

        pktinfo = In6Pktinfo()
        if src_ip:
            for i, b in enumerate(socket.inet_pton(socket.AF_INET6, src_ip)):
                pktinfo.ipi6_addr[i] = b
        if src_ifname:
            os_ifname = vnet.iface_alias_map[src_ifname].name
            pktinfo.ipi6_ifindex = socket.if_nametoindex(os_ifname)

        # Wait for the child to become ready
        self.wait_object(second_vnet.pipe)
        data = bytes("AAAA", "utf-8")

        s = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM, 0)
        try:
            s.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_PKTINFO, bytes(pktinfo))
            aux = (socket.IPPROTO_IPV6, socket.IPV6_PKTINFO, bytes(pktinfo))
            s.sendto(data, (dst_ip, self.DEFAULT_PORT))
        except OSError as e:
            if not errno:
                raise
            assert e.errno == errno
            print("Correctly raised {}".format(e))
            return

        # Wait for the received object
        rx_obj = self.wait_object(second_vnet.pipe)

        assert rx_obj["dst_ip"] == dst_ip
        if expected_ip:
            assert rx_obj["src_ip"] == expected_ip
        if expected_ifname:
            assert rx_obj["dst_iface_alias"] == expected_ifname


class TestIP6OutputLL(BaseTestIP6Ouput):
    def vnet2_handler(self, vnet, obj_map, pipe):
        """Generic listener that sends first received packet with metadata
        back to the sender via pipw
        """
        os_ifname = vnet.iface_alias_map["if2"].name
        ll_data = ToolsHelper.get_linklocals()
        ll_ip, _ = ll_data[os_ifname][0]
        self._vnet2_handler(vnet, obj_map, pipe, ll_ip, os_ifname)

    @pytest.mark.require_user("root")
    def test_output6_linklocal(self):
        """Tests simple UDP output"""
        second_vnet = self.vnet_map["vnet2"]

        # Wait for the child to become ready
        ll_data = self.wait_object(second_vnet.pipe)

        # Pick LL address on if2 vnet2's end
        ip, _ = ll_data[second_vnet.iface_alias_map["if2"].name][0]
        # Get local interface scope
        os_ifname = self.vnet.iface_alias_map["if2"].name
        scopeid = socket.if_nametoindex(os_ifname)

        s = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        data = bytes("AAAA", "utf-8")
        target = (ip, self.DEFAULT_PORT, 0, scopeid)
        print("## TX packet to {}%{},{}".format(ip, scopeid, target[1]))

        s.sendto(data, target)

        # Wait for the received object
        rx_obj = self.wait_object(second_vnet.pipe)
        assert rx_obj["dst_ip"] == ip
        assert rx_obj["dst_iface_alias"] == "if2"


class TestIP6OutputNhopLL(BaseTestIP6Ouput):
    def vnet2_handler(self, vnet, obj_map, pipe):
        """Generic listener that sends first received packet with metadata
        back to the sender via pipw
        """
        ip = str(vnet.iface_alias_map["if2"].first_ipv6.ip)
        self._vnet2_handler(vnet, obj_map, pipe, ip, None)

    @pytest.mark.require_user("root")
    def test_output6_nhop_linklocal(self):
        """Tests UDP output with custom link-local nhop set"""
        second_vnet = self.vnet_map["vnet2"]

        # Wait for the child to become ready
        ll_data = self.wait_object(second_vnet.pipe)

        # Pick target on if2 vnet2's end
        ifaddr = ipaddress.ip_interface(self.TOPOLOGY["if2"]["prefixes6"][0][1])
        ip_dst = str(ifaddr.ip)
        # Pick nexthop on if1
        ip_next, _ = ll_data[second_vnet.iface_alias_map["if1"].name][0]
        # Get local interfaces
        os_ifname = self.vnet.iface_alias_map["if1"].name
        scopeid = socket.if_nametoindex(os_ifname)
        sin6_next = SaHelper.ip6_sa(ip_next, scopeid)

        s = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM, 0)
        s.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_NEXTHOP, sin6_next)

        data = bytes("AAAA", "utf-8")
        s.sendto(data, (ip_dst, self.DEFAULT_PORT))

        # Wait for the received object
        rx_obj = self.wait_object(second_vnet.pipe)
        assert rx_obj["dst_ip"] == ip_dst
        assert rx_obj["dst_iface_alias"] == "if1"


class TestIP6OutputScope(BaseTestIP6Ouput):
    def vnet2_handler(self, vnet, obj_map, pipe):
        """Generic listener that sends first received packet with metadata
        back to the sender via pipw
        """
        bind_ip, bind_ifp = self.wait_object(pipe)
        if bind_ip is None:
            os_ifname = vnet.iface_alias_map[bind_ifp].name
            ll_data = ToolsHelper.get_linklocals()
            bind_ip, _ = ll_data[os_ifname][0]
        if bind_ifp is not None:
            bind_ifp = vnet.iface_alias_map[bind_ifp].name
        print("## BIND {}%{}".format(bind_ip, bind_ifp))
        self._vnet2_handler(vnet, obj_map, pipe, bind_ip, bind_ifp)

    @pytest.mark.parametrize(
        "params",
        [
            # sif/dif: source/destination interface (for link-local addr)
            # sip/dip: source/destination ip (for non-LL addr)
            # ex: OSError errno that sendto() must raise
            pytest.param({"sif": "if2", "dif": "if2"}, id="same"),
            pytest.param(
                {
                    "sif": "if1",
                    "dif": "if2",
                    "ex": errno.EHOSTUNREACH,
                },
                id="ll_differentif1",
            ),
            pytest.param(
                {
                    "sif": "if1",
                    "dip": "2001:db8:b::2",
                    "ex": errno.EHOSTUNREACH,
                },
                id="ll_differentif2",
            ),
            pytest.param(
                {
                    "sip": "2001:db8:a::1",
                    "dif": "if2",
                },
                id="gu_to_ll",
            ),
        ],
    )
    @pytest.mark.require_user("root")
    def test_output6_linklocal_scope(self, params):
        """Tests simple UDP output"""
        second_vnet = self.vnet_map["vnet2"]

        src_ifp = params.get("sif")
        src_ip = params.get("sip")
        dst_ifp = params.get("dif")
        dst_ip = params.get("dip")
        errno = params.get("ex", 0)

        # Sent ifp/IP to bind on
        second_vnet = self.vnet_map["vnet2"]
        second_vnet.pipe.send((dst_ip, dst_ifp))

        # Wait for the child to become ready
        ll_data = self.wait_object(second_vnet.pipe)

        if dst_ip is None:
            # Pick LL address on dst_ifp vnet2's end
            dst_ip, _ = ll_data[second_vnet.iface_alias_map[dst_ifp].name][0]
            # Get local interface scope
            os_ifname = self.vnet.iface_alias_map[dst_ifp].name
            scopeid = socket.if_nametoindex(os_ifname)
            target = (dst_ip, self.DEFAULT_PORT, 0, scopeid)
        else:
            target = (dst_ip, self.DEFAULT_PORT, 0, 0)

        # Bind
        if src_ip is None:
            ll_data = ToolsHelper.get_linklocals()
            os_ifname = self.vnet.iface_alias_map[src_ifp].name
            src_ip, _ = ll_data[os_ifname][0]
            scopeid = socket.if_nametoindex(os_ifname)
            src = (src_ip, self.DEFAULT_PORT, 0, scopeid)
        else:
            src = (src_ip, self.DEFAULT_PORT, 0, 0)

        s = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        s.bind(src)
        data = bytes("AAAA", "utf-8")
        print("## TX packet {} -> {}".format(src, target))

        try:
            s.sendto(data, target)
        except OSError as e:
            if not errno:
                raise
            assert e.errno == errno
            print("Correctly raised {}".format(e))
            return

        # Wait for the received object
        rx_obj = self.wait_object(second_vnet.pipe)
        assert rx_obj["dst_ip"] == dst_ip
        assert rx_obj["src_ip"] == src_ip
        # assert rx_obj["dst_iface_alias"] == "if2"


class TestIP6OutputMulticast(BaseTestIP6Ouput):
    def vnet2_handler(self, vnet, obj_map, pipe):
        group = self.wait_object(pipe)
        os_ifname = vnet.iface_alias_map["if2"].name
        self._vnet2_handler(vnet, obj_map, pipe, group, os_ifname)

    @pytest.mark.parametrize("group_scope", ["ff02", "ff05", "ff08", "ff0e"])
    @pytest.mark.require_user("root")
    def test_output6_multicast(self, group_scope):
        """Tests simple UDP output"""
        second_vnet = self.vnet_map["vnet2"]

        group = "{}::3456".format(group_scope)
        second_vnet.pipe.send(group)

        # Pick target on if2 vnet2's end
        ip = group
        os_ifname = self.vnet.iface_alias_map["if2"].name
        ifindex = socket.if_nametoindex(os_ifname)
        optval = struct.pack("I", ifindex)

        s = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        s.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_MULTICAST_IF, optval)

        data = bytes("AAAA", "utf-8")

        # Wait for the child to become ready
        self.wait_object(second_vnet.pipe)

        print("## TX packet to {},{}".format(ip, self.DEFAULT_PORT))
        s.sendto(data, (ip, self.DEFAULT_PORT))

        # Wait for the received object
        rx_obj = self.wait_object(second_vnet.pipe)
        assert rx_obj["dst_ip"] == ip
        assert rx_obj["dst_iface_alias"] == "if2"


class TestIP6OutputLoopback(SingleVnetTestTemplate):
    IPV6_PREFIXES = ["2001:db8:a::1/64"]
    DEFAULT_PORT = 45365

    @pytest.mark.parametrize(
        "source_validation",
        [
            pytest.param(0, id="no_sav"),
            pytest.param(1, id="sav"),
        ],
    )
    @pytest.mark.parametrize("scope", ["gu", "ll", "lo"])
    def test_output6_self_tcp(self, scope, source_validation):
        """Tests IPv6 TCP connection to the local IPv6 address"""

        ToolsHelper.set_sysctl(
            "net.inet6.ip6.source_address_validation", source_validation
        )

        if scope == "gu":
            ip = "2001:db8:a::1"
            addr_tuple = (ip, self.DEFAULT_PORT)
        elif scope == "ll":
            os_ifname = self.vnet.iface_alias_map["if1"].name
            ifindex = socket.if_nametoindex(os_ifname)
            ll_data = ToolsHelper.get_linklocals()
            ip, _ = ll_data[os_ifname][0]
            addr_tuple = (ip, self.DEFAULT_PORT, 0, ifindex)
        elif scope == "lo":
            ip = "::1"
            ToolsHelper.get_output("route add -6 ::1/128 -iface lo0")
            ifindex = socket.if_nametoindex("lo0")
            addr_tuple = (ip, self.DEFAULT_PORT)
        else:
            assert 0 == 1
        print("address: {}".format(addr_tuple))

        start = time.perf_counter()
        ss = socket.socket(socket.AF_INET6, socket.SOCK_STREAM, socket.IPPROTO_TCP)
        ss.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_RECVPKTINFO, 1)
        ss.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        ss.bind(addr_tuple)
        ss.listen()
        s = socket.socket(socket.AF_INET6, socket.SOCK_STREAM, socket.IPPROTO_TCP)
        s.settimeout(2.0)
        s.connect(addr_tuple)
        conn, from_addr = ss.accept()
        duration = time.perf_counter() - start

        assert from_addr[0] == ip
        assert duration < 1.0

    @pytest.mark.parametrize(
        "source_validation",
        [
            pytest.param(0, id="no_sav"),
            pytest.param(1, id="sav"),
        ],
    )
    @pytest.mark.parametrize("scope", ["gu", "ll", "lo"])
    def test_output6_self_udp(self, scope, source_validation):
        """Tests IPv6 UDP connection to the local IPv6 address"""

        ToolsHelper.set_sysctl(
            "net.inet6.ip6.source_address_validation", source_validation
        )

        if scope == "gu":
            ip = "2001:db8:a::1"
            addr_tuple = (ip, self.DEFAULT_PORT)
        elif scope == "ll":
            os_ifname = self.vnet.iface_alias_map["if1"].name
            ifindex = socket.if_nametoindex(os_ifname)
            ll_data = ToolsHelper.get_linklocals()
            ip, _ = ll_data[os_ifname][0]
            addr_tuple = (ip, self.DEFAULT_PORT, 0, ifindex)
        elif scope == "lo":
            ip = "::1"
            ToolsHelper.get_output("route add -6 ::1/128 -iface lo0")
            ifindex = socket.if_nametoindex("lo0")
            addr_tuple = (ip, self.DEFAULT_PORT)
        else:
            assert 0 == 1
        print("address: {}".format(addr_tuple))

        start = time.perf_counter()
        ss = socket.socket(socket.AF_INET6, socket.SOCK_STREAM, socket.IPPROTO_TCP)
        ss.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_RECVPKTINFO, 1)
        ss.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        ss.bind(addr_tuple)
        ss.listen()
        s = socket.socket(socket.AF_INET6, socket.SOCK_STREAM, socket.IPPROTO_TCP)
        s.settimeout(2.0)
        s.connect(addr_tuple)
        conn, from_addr = ss.accept()
        duration = time.perf_counter() - start

        assert from_addr[0] == ip
        assert duration < 1.0
