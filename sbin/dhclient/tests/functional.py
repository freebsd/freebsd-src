#
# Copyright (c) 2026 The FreeBSD Foundation
#
# This software was developed by Mark Johnston under sponsorship from
# the FreeBSD Foundation.
#
# SPDX-License-Identifier: BSD-2-Clause
#

import subprocess

import pytest
from atf_python.sys.net.tools import ToolsHelper
from atf_python.sys.net.vnet import VnetTestTemplate


def mac_to_chaddr(mac_str):
    """Convert a MAC address string to a 16-byte BOOTP chaddr."""
    mac_bytes = bytes.fromhex(mac_str.replace(":", ""))
    return mac_bytes + b"\x00" * (16 - len(mac_bytes))




class TestDhclient(VnetTestTemplate):
    _SERVER_IP = "192.0.2.1"
    _CLIENT_IP = "192.0.2.10"

    TOPOLOGY = {
        "vnet1": {"ifaces": ["if1"]},
        "vnet2": {"ifaces": ["if1"]},
        "if1": {},
    }

    def vnet1_handler(self, vnet):
        """
        Run dhclient in vnet1 (the client side of the epair).  Send
        back the output of "ifconfig <iface>" once dhclient has finished
        its exchange.
        """
        ifname = vnet.iface_alias_map["if1"].name

        # Wait for the test body to be ready before starting dhclient.
        vnet.pipe.recv()

        try:
            result = subprocess.run(
                ["/sbin/dhclient", "-c", "/dev/null",
                 "-l", "dhclient.lease", ifname],
                capture_output=True, text=True, timeout=30,
            )
            stderr = result.stderr
        except subprocess.TimeoutExpired:
            stderr = "dhclient timed out"
        out = ToolsHelper.get_output("/sbin/ifconfig {}".format(ifname))
        lease = ""
        try:
            with open("dhclient.lease", "r") as f:
                lease = f.read()
        except FileNotFoundError:
            pass
        vnet.pipe.send({"ifconfig": out, "stderr": stderr, "lease": lease})

    def _setup_server(self):
        """Configure the server side of the epair and return its name."""
        ifname = self.vnet.iface_alias_map["if1"].name
        ToolsHelper.print_output(
            "/sbin/ifconfig {} inet {}/24 up".format(ifname, self._SERVER_IP)
        )
        # Import in the correct vnet, so as to not confuse Scapy
        import scapy.all as sp
        self.sp = sp
        return ifname

    def _do_dora(self, ifname, first_extra_opts=None, extra_opts=None,
                 sname=b"", file=b""):
        """
        Run a discover-offer-request-acknowledge exchange and
        return (discover_count, result).

        If first_extra_opts is set, it is used in the OFFER for the first
        DHCPDISCOVER only; subsequent DISCOVERs use extra_opts.
        """
        discover_count = 0
        done = False

        def handle_dhcp(pkt):
            nonlocal discover_count, done

            msg_type = self.get_dhcp_type(pkt)
            if msg_type is None:
                return

            client_mac = pkt[self.sp.Ether].src
            xid = pkt[self.sp.BOOTP].xid

            if msg_type == "discover":
                discover_count += 1
                opts = extra_opts
                if discover_count == 1 and first_extra_opts is not None:
                    opts = first_extra_opts
                reply = self.build_reply(
                    ifname, self._SERVER_IP, self._CLIENT_IP, client_mac,
                    xid, "offer", extra_opts=opts, sname=sname, file=file,
                )
                self.sp.sendp(reply, iface=ifname, verbose=0)

            elif msg_type == "request":
                reply = self.build_reply(
                    ifname, self._SERVER_IP, self._CLIENT_IP, client_mac,
                    xid, "ack", sname=sname, file=file,
                )
                self.sp.sendp(reply, iface=ifname, verbose=0)
                done = True

        sniffer = self.sp.AsyncSniffer(
            iface=ifname,
            filter="udp and dst port 67",
            prn=handle_dhcp,
            stop_filter=lambda _: done,
            started_callback=lambda: self.send_object(
                self.vnet_map["vnet1"].pipe, "go",
            ),
        )
        sniffer.start()
        sniffer.join(timeout=30)

        assert done, "DHCP exchange did not complete"
        result = self.wait_object(self.vnet_map["vnet1"].pipe, timeout=15)
        return discover_count, result


    def get_dhcp_type(self, pkt):
        """Extract the DHCP message type from a packet."""
        if not pkt.haslayer(self.sp.DHCP):
            return None
        for opt in pkt[self.sp.DHCP].options:
            if isinstance(opt, tuple) and opt[0] == "message-type":
                return self.sp.DHCPTypes[opt[1]]
        return None

    def build_reply(self, iface, server_ip, client_ip, client_mac, xid, msg_type,
                    extra_opts=None, sname=b"", file=b""):
        """Build a DHCP OFFER or ACK packet."""
        options = [
            ("message-type", msg_type),
            ("server_id", server_ip),
            ("lease_time", 3600),
            ("subnet_mask", "255.255.255.0"),
            ("router", server_ip),
        ]
        if extra_opts:
            options.extend(extra_opts)
        options.append("end")

        return (
            self.sp.Ether(dst=client_mac, src=self.sp.get_if_hwaddr(iface))
            / self.sp.IP(src=server_ip, dst="255.255.255.255")
            / self.sp.UDP(sport=67, dport=68)
            / self.sp.BOOTP(op=2,
                xid=xid,
                yiaddr=client_ip,
                siaddr=server_ip,
                chaddr=mac_to_chaddr(client_mac),
                sname=sname,
                file=file,
            )
            / self.sp.DHCP(options=options)
        )

    @pytest.mark.require_user("root")
    def test_classless_routes_truncated(self):
        """
        Verify that dhclient rejects a DHCPOFFER containing a truncated
        classless static routes option.

        A malformed option 121 payload is constructed with a valid first
        entry (default route, width=0) followed by a second entry with
        width=24 that has only 1 byte of subnet data instead of the
        required 3 bytes.

        The test sends the bad offer first, then a valid offer, and
        verifies that dhclient rejects the bad offer and successfully
        acquires a lease from the good one.
        """
        ifname = self._setup_server()
        bad_routes = (
            b"\x00"              # width=0 (default route)
            b"\xc0\xa8\x01\x01"  # gateway: 192.168.1.1
            b"\x18"              # width=24 (/24 subnet)
            b"\x0a"              # only 1 of 3 required subnet octets
        )
        discover_count, result = self._do_dora(
            ifname, first_extra_opts=[(121, bad_routes)],
        )
        assert discover_count >= 2, \
            "Expected dhclient to reject the first (bad) offer and retry"
        assert "inet {}".format(self._CLIENT_IP) in result["ifconfig"]

    @pytest.mark.require_user("root")
    def test_filename_backslash(self):
        """
        Verify that backslashes in the BOOTP filename don't cause the
        lease to be rejected.  These can appear in Windows boot paths.
        """
        ifname = self._setup_server()
        discover_count, result = self._do_dora(
            ifname, file=b"\\windows\\boot\\pxeboot.com",
        )
        assert discover_count == 1, \
            "Lease with backslash in filename should be accepted"
        assert "inet {}".format(self._CLIENT_IP) in result["ifconfig"]

    @pytest.mark.require_user("root")
    def test_sname_valid(self):
        """
        Verify that a valid server name is preserved in the lease file.
        """
        ifname = self._setup_server()
        discover_count, result = self._do_dora(
            ifname, sname=b"dhcp.example.com",
        )
        assert discover_count == 1
        assert "inet {}".format(self._CLIENT_IP) in result["ifconfig"]
        assert "dhcp.example.com" in result["lease"]

    @pytest.mark.require_user("root")
    def test_sname_quote(self):
        """
        Verify that a double quote in the BOOTP server name causes the
        server name to be discarded but does not cause the lease to be
        rejected.  Values from the lease file may be handled by
        dhclient-script and thus are susceptible to shell injection.
        """
        ifname = self._setup_server()
        discover_count, result = self._do_dora(
            ifname, sname=b'evil"server',
        )
        assert discover_count == 1, \
            "Lease with quote in sname should be accepted"
        assert "inet {}".format(self._CLIENT_IP) in result["ifconfig"]
        assert 'evil"server' not in result["lease"], \
            "Server name with quote should not appear in the lease file"
        assert "server-name" not in result["lease"], \
            "Unsafe server name should be discarded entirely"
