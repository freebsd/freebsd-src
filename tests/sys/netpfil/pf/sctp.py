import pytest
import ctypes
import socket
import ipaddress
import re
from atf_python.sys.net.tools import ToolsHelper
from atf_python.sys.net.vnet import VnetTestTemplate

import time

SCTP_UNORDERED = 0x0400

SCTP_NODELAY                 = 0x00000004
SCTP_SET_PEER_PRIMARY_ADDR   = 0x00000006
SCTP_PRIMARY_ADDR            = 0x00000007

SCTP_BINDX_ADD_ADDR          = 0x00008001
SCTP_BINDX_REM_ADDR          = 0x00008002

class sockaddr_in(ctypes.Structure):
    _fields_ = [
        ('sin_len', ctypes.c_uint8),
        ('sin_family', ctypes.c_uint8),
        ('sin_port', ctypes.c_uint16),
        ('sin_addr', ctypes.c_uint32),
        ('sin_zero', ctypes.c_int8 * 8)
    ]

class sockaddr_in6(ctypes.Structure):
    _fields_ = [
        ('sin6_len',      ctypes.c_uint8),
        ('sin6_family',   ctypes.c_uint8),
        ('sin6_port',     ctypes.c_uint16),
        ('sin6_flowinfo', ctypes.c_uint32),
        ('sin6_addr',     ctypes.c_uint8 * 16),
        ('sin6_scope_id', ctypes.c_uint32)
    ]

class sockaddr_storage(ctypes.Union):
    _fields_ = [
        ("v4",    sockaddr_in),
        ("v6",   sockaddr_in6)
    ]

class sctp_sndrcvinfo(ctypes.Structure):
    _fields_ = [
        ('sinfo_stream',        ctypes.c_uint16),
        ('sinfo_ssn',           ctypes.c_uint16),
        ('sinfo_flags',         ctypes.c_uint16),
        ('sinfo_ppid',          ctypes.c_uint32),
        ('sinfo_context',       ctypes.c_uint32),
        ('sinfo_timetolive',    ctypes.c_uint32),
        ('sinfo_tsn',           ctypes.c_uint32),
        ('sinfo_cumtsn',        ctypes.c_uint32),
        ('sinfo_assoc_id',      ctypes.c_uint32),
    ]

class sctp_setprim(ctypes.Structure):
    _fields_ = [
        ('ssp_addr',        sockaddr_storage),
        ('ssp_pad',         ctypes.c_int8 * (128 - 16)),
        ('ssp_assoc_id',    ctypes.c_uint32),
        ('ssp_padding',     ctypes.c_uint32)
    ]

def to_sockaddr(ip, port):
    ip = ipaddress.ip_address(ip)

    if ip.version == 4:
        addr = sockaddr_in()
        addr.sin_len = ctypes.sizeof(addr)
        addr.sin_family = socket.AF_INET
        addr.sin_port = socket.htons(port)
        addr.sin_addr = socket.htonl(int.from_bytes(ip.packed, byteorder='big'))
    else:
        assert ip.version == 6

        addr = sockaddr_in6()
        addr.sin6_len = ctypes.sizeof(addr)
        addr.sin6_family = socket.AF_INET6
        addr.sin6_port = socket.htons(port)
        for i in range(0, 16):
            addr.sin6_addr[i] = ip.packed[i]

    return addr

class SCTPServer:
    def __init__(self, family, port=1234):
        self._libc = ctypes.CDLL("libc.so.7", use_errno=True)

        self._listen_fd = self._libc.socket(family, socket.SOCK_STREAM, socket.IPPROTO_SCTP)
        if self._listen_fd == -1:
            raise Exception("Failed to create socket")

        if family == socket.AF_INET:
            srvaddr = sockaddr_in()
            srvaddr.sin_len = ctypes.sizeof(srvaddr)
            srvaddr.sin_family = socket.AF_INET
            srvaddr.sin_port = socket.htons(port)
            srvaddr.sin_addr = socket.INADDR_ANY
        else:
            srvaddr = sockaddr_in6()
            srvaddr.sin6_len = ctypes.sizeof(srvaddr)
            srvaddr.sin6_family = family
            srvaddr.sin6_port = socket.htons(port)
            # Leave sin_addr empty, because ANY is zero

        ret = self._libc.bind(self._listen_fd, ctypes.pointer(srvaddr),
            ctypes.sizeof(srvaddr))
        if ret == -1:
            raise Exception("Failed to bind: %d" % ctypes.get_errno())

        ret = self._libc.listen(self._listen_fd, 2)
        if ret == -1:
            raise Exception("Failed to listen")

    def _to_string(self, buf):
        return ''.join([chr(int.from_bytes(i, byteorder='big')) for i in buf]).rstrip('\x00')

    def accept(self, vnet):
        fd = self._libc.accept(self._listen_fd, 0, 0)
        if fd < 0:
            raise Exception("Failed to accept")

        print("SCTPServer: connection opened")
        while True:
            rcvinfo = sctp_sndrcvinfo()
            flags = ctypes.c_int()
            buf = ctypes.create_string_buffer(128)

            # Receive a single message, and inform the other vnet about it.
            ret = self._libc.sctp_recvmsg(fd, ctypes.cast(buf, ctypes.c_void_p), 128,
                0, 0, ctypes.pointer(rcvinfo), ctypes.pointer(flags))
            if ret < 0:
                print("SCTPServer: connection closed")
                return
            if ret == 0:
                continue

            rcvd = {}
            rcvd['ppid'] = socket.ntohl(rcvinfo.sinfo_ppid)
            rcvd['data'] = self._to_string(buf)
            rcvd['len'] = ret
            print(rcvd)
            vnet.pipe.send(rcvd)

class SCTPClient:
    def __init__(self, ip, port=1234, fromaddr=None):
        self._libc = ctypes.CDLL("libc.so.7", use_errno=True)

        if ipaddress.ip_address(ip).version == 4:
            family = socket.AF_INET
        else:
            family = socket.AF_INET6

        self._fd = self._libc.socket(family, socket.SOCK_STREAM,
            socket.IPPROTO_SCTP)
        if self._fd == -1:
            raise Exception("Failed to open socket")

        if fromaddr is not None:
            addr = to_sockaddr(fromaddr, 0)

            ret = self._libc.bind(self._fd, ctypes.pointer(addr), ctypes.sizeof(addr))
            if ret != 0:
                print("bind() => %d", ctypes.get_errno())
                raise

        addr = to_sockaddr(ip, port)
        ret = self._libc.connect(self._fd, ctypes.pointer(addr), ctypes.sizeof(addr))
        if ret == -1:
            raise Exception("Failed to connect")

        # Enable NODELAY, because otherwise the sending host may wait for SACK
        # on a data chunk we've removed
        enable = ctypes.c_int(1)
        ret = self._libc.setsockopt(self._fd, socket.IPPROTO_SCTP,
                SCTP_NODELAY, ctypes.pointer(enable), 4)

    def newpeer(self, addr):
        print("newpeer(%s)" % (addr))

        setp = sctp_setprim()
        a = to_sockaddr(addr, 0)
        if type(a) is sockaddr_in:
            setp.ssp_addr.v4 = a
        else:
            assert type(a) is sockaddr_in6
            setp.ssp_addr.v6 = a

        ret = self._libc.setsockopt(self._fd, socket.IPPROTO_SCTP,
            SCTP_PRIMARY_ADDR, ctypes.pointer(setp), ctypes.sizeof(setp))
        if ret != 0:
            print("errno %d" % ctypes.get_errno())
            raise Exception(ctypes.get_errno())

    def newprimary(self, addr):
        print("newprimary(%s)" % (addr))

        # Strictly speaking needs to be struct sctp_setpeerprim, but that's
        # identical to sctp_setprim
        setp = sctp_setprim()
        a = to_sockaddr(addr, 0)
        if type(a) is sockaddr_in:
            setp.ssp_addr.v4 = a
        else:
            assert type(a) is sockaddr_in6
            setp.ssp_addr.v6 = a

        ret = self._libc.setsockopt(self._fd, socket.IPPROTO_SCTP,
            SCTP_SET_PEER_PRIMARY_ADDR, ctypes.pointer(setp), ctypes.sizeof(setp))
        if ret != 0:
            print("errno %d" % ctypes.get_errno())
            raise

    def bindx(self, addr, add):
        print("bindx(%s, %s)" % (addr, add))

        addr = to_sockaddr(addr, 0)

        if add:
            flag = SCTP_BINDX_ADD_ADDR
        else:
            flag = SCTP_BINDX_REM_ADDR
        ret = self._libc.sctp_bindx(self._fd, ctypes.pointer(addr), 1, flag)
        if ret != 0:
            print("sctp_bindx() errno %d" % ctypes.get_errno())
            raise

    def send(self, buf, ppid, ordered=False):
        flags = 0

        if not ordered:
            flags = SCTP_UNORDERED

        ppid = socket.htonl(ppid)
        ret = self._libc.sctp_sendmsg(self._fd, ctypes.c_char_p(buf), len(buf),
            ctypes.c_void_p(0), 0, ppid, flags, 0, 0, 0)
        if ret < 0:
            raise Exception("Failed to send message")

    def close(self):
        self._libc.close(self._fd)
        self._fd = -1

class TestSCTP(VnetTestTemplate):
    REQUIRED_MODULES = ["sctp", "pf"]
    TOPOLOGY = {
        "vnet1": {"ifaces": ["if1"]},
        "vnet2": {"ifaces": ["if1"]},
        "if1": {"prefixes4": [("192.0.2.1/24", "192.0.2.2/24")]},
    }

    def vnet2_handler(self, vnet):
        # Give ourself a second IP address, for multihome testing
        ifname = vnet.iface_alias_map["if1"].name
        ToolsHelper.print_output("/sbin/ifconfig %s inet alias 192.0.2.3/24" % ifname)

        # Start an SCTP server process, pipe the ppid + data back to the other vnet?
        srv = SCTPServer(socket.AF_INET, port=1234)
        while True:
            srv.accept(vnet)

    @pytest.mark.require_user("root")
    def test_multihome(self):
        srv_vnet = self.vnet_map["vnet2"]

        ToolsHelper.print_output("/sbin/pfctl -e")
        ToolsHelper.pf_rules([
            "block proto sctp",
            "pass inet proto sctp to 192.0.2.0/24"])

        # Sanity check, we can communicate with the primary address.
        client = SCTPClient("192.0.2.3", 1234)
        client.send(b"hello", 0)
        rcvd = self.wait_object(srv_vnet.pipe)
        print(rcvd)
        assert rcvd['ppid'] == 0
        assert rcvd['data'] == "hello"

        try:
            client.newpeer("192.0.2.2")
            client.send(b"world", 0)
            rcvd = self.wait_object(srv_vnet.pipe)
            print(rcvd)
            assert rcvd['ppid'] == 0
            assert rcvd['data'] == "world"
        finally:
            # Debug output
            ToolsHelper.print_output("/sbin/pfctl -ss")
            ToolsHelper.print_output("/sbin/pfctl -sr -vv")

        # Check that we have a state for 192.0.2.3 and 192.0.2.2 to 192.0.2.1
        states = ToolsHelper.get_output("/sbin/pfctl -ss")
        assert re.search(r"all sctp 192.0.2.1:.*192.0.2.3:1234", states)
        assert re.search(r"all sctp 192.0.2.1:.*192.0.2.2:1234", states)

    @pytest.mark.require_user("root")
    def test_multihome_asconf(self):
        srv_vnet = self.vnet_map["vnet2"]

        # Assign a second IP to ourselves
        ToolsHelper.print_output("/sbin/ifconfig %s inet alias 192.0.2.10/24"
            % self.vnet.iface_alias_map["if1"].name)
        ToolsHelper.print_output("/sbin/pfctl -e")
        ToolsHelper.pf_rules([
            "block proto sctp",
            "pass inet proto sctp from 192.0.2.0/24"])

        # Sanity check, we can communicate with the primary address.
        client = SCTPClient("192.0.2.3", 1234, "192.0.2.1")
        client.send(b"hello", 0)
        rcvd = self.wait_object(srv_vnet.pipe)
        print(rcvd)
        assert rcvd['ppid'] == 0
        assert rcvd['data'] == "hello"

        # Now add our second address to the connection
        client.bindx("192.0.2.10", True)

        # We can still communicate
        client.send(b"world", 0)
        rcvd = self.wait_object(srv_vnet.pipe)
        print(rcvd)
        assert rcvd['ppid'] == 0
        assert rcvd['data'] == "world"

        # Now change to a different peer address
        try:
            client.newprimary("192.0.2.10")
            client.send(b"!", 0)
            rcvd = self.wait_object(srv_vnet.pipe, 5)
            print(rcvd)
            assert rcvd['ppid'] == 0
            assert rcvd['data'] == "!"
        finally:
            # Debug output
            ToolsHelper.print_output("/sbin/pfctl -ss -vv")

        # Ensure we have the states we'd expect
        states = ToolsHelper.get_output("/sbin/pfctl -ss")
        assert re.search(r"all sctp 192.0.2.1:.*192.0.2.3:1234", states)
        assert re.search(r"all sctp 192.0.2.10:.*192.0.2.3:1234", states)

        # Now remove 192.0.2.1 as an address
        client.bindx("192.0.2.1", False)

        # We can still communicate
        try:
            client.send(b"More data", 0)
            rcvd = self.wait_object(srv_vnet.pipe, 5)
            print(rcvd)
            assert rcvd['ppid'] == 0
            assert rcvd['data'] =="More data"
        finally:
            # Debug output
            ToolsHelper.print_output("/sbin/pfctl -ss -vv")

        # Verify that state is closing
        states = ToolsHelper.get_output("/sbin/pfctl -ss")
        assert re.search(r"all sctp 192.0.2.1:.*192.0.2.3:1234.*SHUTDOWN", states)


    @pytest.mark.require_user("root")
    def test_permutation(self):
        # Test that we generate all permutations of src/dst addresses.
        # Assign two addresses to each end, and check for the expected states
        srv_vnet = self.vnet_map["vnet2"]

        ifname = self.vnet_map["vnet1"].iface_alias_map["if1"].name
        ToolsHelper.print_output("/sbin/ifconfig %s inet alias 192.0.2.4/24" % ifname)

        ToolsHelper.print_output("/sbin/pfctl -e")
        ToolsHelper.pf_rules([
            "set state-policy if-bound",
            "block proto sctp",
            "pass inet proto sctp to 192.0.2.0/24"])

        # Sanity check, we can communicate with the primary address.
        client = SCTPClient("192.0.2.3", 1234)
        client.send(b"hello", 0)
        rcvd = self.wait_object(srv_vnet.pipe)
        print(rcvd)
        assert rcvd['ppid'] == 0
        assert rcvd['data'] == "hello"

        # Check that we have a state for 192.0.2.3 and 192.0.2.2 to 192.0.2.1, but also to 192.0.2.4
        states = ToolsHelper.get_output("/sbin/pfctl -ss")
        print(states)
        assert re.search(r".*sctp 192.0.2.1:.*192.0.2.3:1234", states)
        assert re.search(r"all sctp 192.0.2.1:.*192.0.2.2:1234", states)
        assert re.search(r".*sctp 192.0.2.4:.*192.0.2.3:1234", states)
        assert re.search(r"all sctp 192.0.2.4:.*192.0.2.2:1234", states)

class TestSCTPv6(VnetTestTemplate):
    REQUIRED_MODULES = ["sctp", "pf"]
    TOPOLOGY = {
        "vnet1": {"ifaces": ["if1"]},
        "vnet2": {"ifaces": ["if1"]},
        "if1": {"prefixes6": [("2001:db8::1/64", "2001:db8::2/64")]},
    }

    def vnet2_handler(self, vnet):
        # Give ourself a second IP address, for multihome testing
        ifname = vnet.iface_alias_map["if1"].name
        ToolsHelper.print_output("/sbin/ifconfig %s inet6 alias 2001:db8::3/64" % ifname)

        # Start an SCTP server process, pipe the ppid + data back to the other vnet?
        srv = SCTPServer(socket.AF_INET6, port=1234)
        while True:
            srv.accept(vnet)

    @pytest.mark.require_user("root")
    def test_multihome(self):
        srv_vnet = self.vnet_map["vnet2"]

        ToolsHelper.print_output("/sbin/pfctl -e")
        ToolsHelper.pf_rules([
            "block proto sctp",
            "pass inet6 proto sctp to 2001:db8::0/64"])

        # Sanity check, we can communicate with the primary address.
        client = SCTPClient("2001:db8::3", 1234)
        client.send(b"hello", 0)
        rcvd = self.wait_object(srv_vnet.pipe)
        print(rcvd)
        assert rcvd['ppid'] == 0
        assert rcvd['data'] == "hello"

        # Now change to a different peer address
        try:
            client.newpeer("2001:db8::2")
            client.send(b"world", 0)
            rcvd = self.wait_object(srv_vnet.pipe)
            print(rcvd)
            assert rcvd['ppid'] == 0
            assert rcvd['data'] == "world"
        finally:
            # Debug output
            ToolsHelper.print_output("/sbin/pfctl -ss -vv")

        # Check that we have the expected states
        states = ToolsHelper.get_output("/sbin/pfctl -ss")
        assert re.search(r"all sctp 2001:db8::1\[.*2001:db8::3\[1234\]", states)
        assert re.search(r"all sctp 2001:db8::1\[.*2001:db8::2\[1234\]", states)

    @pytest.mark.require_user("root")
    def test_multihome_asconf(self):
        srv_vnet = self.vnet_map["vnet2"]

        # Assign a second IP to ourselves
        ToolsHelper.print_output("/sbin/ifconfig %s inet6 alias 2001:db8::10/64"
            % self.vnet.iface_alias_map["if1"].name)
        ToolsHelper.print_output("/sbin/pfctl -e")
        ToolsHelper.pf_rules([
            "block proto sctp",
            "pass inet6 proto sctp from 2001:db8::/64"])

        # Sanity check, we can communicate with the primary address.
        client = SCTPClient("2001:db8::3", 1234, "2001:db8::1")
        client.send(b"hello", 0)
        rcvd = self.wait_object(srv_vnet.pipe)
        print(rcvd)
        assert rcvd['ppid'] == 0
        assert rcvd['data'] == "hello"

        # Now add our second address to the connection
        client.bindx("2001:db8::10", True)

        # We can still communicate
        client.send(b"world", 0)
        rcvd = self.wait_object(srv_vnet.pipe)
        print(rcvd)
        assert rcvd['ppid'] == 0
        assert rcvd['data'] == "world"

        # Now change to a different peer address
        try:
            client.newprimary("2001:db8::10")
            client.send(b"!", 0)
            rcvd = self.wait_object(srv_vnet.pipe, 5)
            print(rcvd)
            assert rcvd['ppid'] == 0
            assert rcvd['data'] == "!"
        finally:
            # Debug output
            ToolsHelper.print_output("/sbin/pfctl -ss -vv")

        # Check that we have the expected states
        states = ToolsHelper.get_output("/sbin/pfctl -ss")
        assert re.search(r"all sctp 2001:db8::1\[.*2001:db8::3\[1234\]", states)
        assert re.search(r"all sctp 2001:db8::10\[.*2001:db8::3\[1234\]", states)

        # Now remove 2001:db8::1 as an address
        client.bindx("2001:db8::1", False)

        # Wecan still communicate
        try:
            client.send(b"More data", 0)
            rcvd = self.wait_object(srv_vnet.pipe, 5)
            print(rcvd)
            assert rcvd['ppid'] == 0
            assert rcvd['data'] == "More data"
        finally:
            # Debug output
            ToolsHelper.print_output("/sbin/pfctl -ss -vv")

        # Verify that the state is closing
        states = ToolsHelper.get_output("/sbin/pfctl -ss")
        assert re.search(r"all sctp 2001:db8::1\[.*2001:db8::3\[1234\].*SHUTDOWN", states)

    @pytest.mark.require_user("root")
    def test_permutation(self):
        # Test that we generate all permutations of src/dst addresses.
        # Assign two addresses to each end, and check for the expected states
        srv_vnet = self.vnet_map["vnet2"]

        ifname = self.vnet_map["vnet1"].iface_alias_map["if1"].name
        ToolsHelper.print_output("/sbin/ifconfig %s inet6 alias 2001:db8::4/64" % ifname)

        ToolsHelper.print_output("/sbin/pfctl -e")
        ToolsHelper.pf_rules([
            "block proto sctp",
            "pass inet6 proto sctp to 2001:db8::0/64"])

        # Sanity check, we can communicate with the primary address.
        client = SCTPClient("2001:db8::3", 1234)
        client.send(b"hello", 0)
        rcvd = self.wait_object(srv_vnet.pipe)
        print(rcvd)
        assert rcvd['ppid'] == 0
        assert rcvd['data'] == "hello"

        # Check that we have a state for 2001:db8::3 and 2001:db8::2 to 2001:db8::1, but also to 2001:db8::4
        states = ToolsHelper.get_output("/sbin/pfctl -ss")
        print(states)
        assert re.search(r"all sctp 2001:db8::1\[.*2001:db8::2\[1234\]", states)
        assert re.search(r"all sctp 2001:db8::1\[.*2001:db8::3\[1234\]", states)
        assert re.search(r"all sctp 2001:db8::4\[.*2001:db8::2\[1234\]", states)
        assert re.search(r"all sctp 2001:db8::4\[.*2001:db8::3\[1234\]", states)
