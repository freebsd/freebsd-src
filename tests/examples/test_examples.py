import pytest
from atf_python.utils import BaseTest
from atf_python.sys.net.tools import ToolsHelper
from atf_python.sys.net.vnet import SingleVnetTestTemplate
from atf_python.sys.net.vnet import VnetTestTemplate
from atf_python.sys.net.vnet import VnetInstance

import errno
import socket
import subprocess
import json

from typing import List


# Test classes should be inherited
# from the BaseTest


class TestExampleSimplest(BaseTest):
    @pytest.mark.skip(reason="comment me to run the test")
    def test_one(self):
        assert ToolsHelper.get_output("uname -s").strip() == "FreeBSD"


class TestExampleSimple(BaseTest):
    # List of required kernel modules (kldstat -v)
    # that needs to be present for the tests to run
    REQUIRED_MODULES = ["null"]

    @pytest.mark.skip(reason="comment me to run the test")
    def test_one(self):
        """Optional test description
        This and the following lines are not propagated
        to the ATF test description.
        """
        pass

    @pytest.mark.skip(reason="comment me to run the test")
    # List of all requirements supported by an atf runner
    # See atf-test-case(4) for the detailed description
    @pytest.mark.require_user("root")
    @pytest.mark.require_arch(["amd64", "i386"])
    @pytest.mark.require_files(["/path/file1", "/path/file2"])
    @pytest.mark.require_machine(["amd64", "i386"])
    @pytest.mark.require_memory("200M")
    @pytest.mark.require_progs(["prog1", "prog2"])
    @pytest.mark.timeout(300)
    def test_two(self):
        pass

    @pytest.mark.skip(reason="comment me to run the test")
    def test_get_properties(self, request):
        """Shows fetching of test src dir and ATF-set variables"""
        print()
        print("SRC_DIR={}".format(request.fspath.dirname))
        print("ATF VARS:")
        for k, v in self.atf_vars.items():
            print("  {}: {}".format(k, v))
        print()

    @pytest.mark.skip(reason="comment me to run the test")
    @pytest.mark.require_user("unprivileged")
    def test_syscall_failure(self):
        s = socket.socket(socket.AF_INET6, socket.SOCK_STREAM)
        with pytest.raises(OSError) as exc_info:
            s.bind(("::1", 42))
        assert exc_info.value.errno == errno.EACCES

    @pytest.mark.skip(reason="comment me to run the test")
    @pytest.mark.parametrize(
        "family_tuple",
        [
            pytest.param([socket.AF_INET, None], id="AF_INET"),
            pytest.param([socket.AF_INET6, None], id="AF_INET6"),
            pytest.param([39, errno.EAFNOSUPPORT], id="FAMILY_39"),
        ],
    )
    def test_parametrize(self, family_tuple):
        family, error = family_tuple
        try:
            s = socket.socket(family, socket.SOCK_STREAM)
            s.close()
        except OSError as e:
            if error is None or error != e.errno:
                raise

    # @pytest.mark.skip(reason="comment me to run the test")
    def test_with_cleanup(self):
        print("TEST BODY")

    def cleanup_test_with_cleanup(self, test_id):
        print("CLEANUP HANDLER")


class TestVnetSimple(SingleVnetTestTemplate):
    """
    SingleVnetTestTemplate creates a topology with a single
    vnet and a single epair between this vnet and the host system.
    Additionally, lo0 interface is created inside the vnet.

    Both vnets and interfaces are aliased as vnetX and ifY.
    They can be accessed via maps:
        vnet: VnetInstance = self.vnet_map["vnet1"]
        iface: VnetInterface = vnet.iface_alias_map["if1"]

    All prefixes from IPV4_PREFIXES and IPV6_PREFIXES are
    assigned to the single epair interface inside the jail.

    One can rely on the fact that there are no IPv6 prefixes
    in the tentative state when the test method is called.
    """

    IPV6_PREFIXES: List[str] = ["2001:db8::1/64"]
    IPV4_PREFIXES: List[str] = ["192.0.2.1/24"]

    def setup_method(self, method):
        """
        Optional pre-setup for all of the tests inside the class
        """
        # Code to run before vnet setup
        #
        super().setup_method(method)
        #
        # Code to run after vnet setup
        # Executed inside the vnet

    @pytest.mark.skip(reason="comment me to run the test")
    @pytest.mark.require_user("root")
    def test_ping(self):
        assert subprocess.run("ping -c1 192.0.2.1".split()).returncode == 0
        assert subprocess.run("ping -c1 2001:db8::1".split()).returncode == 0

    @pytest.mark.skip(reason="comment me to run the test")
    def test_topology(self):
        vnet = self.vnet_map["vnet1"]
        iface = vnet.iface_alias_map["if1"]
        print("Iface {} inside vnet {}".format(iface.name, vnet.name))


class TestVnetDual1(VnetTestTemplate):
    """
    VnetTestTemplate creates topology described in the self.TOPOLOGY

    Each vnet (except vnet1) can have a handler function, named
      vnetX_handler. This function will be run in a separate process
      inside vnetX jail. The framework automatically creates a pipe
      to allow communication between the main test and the vnet handler.

    This topology contains 2 VNETs connected with 2 epairs:

    [           VNET1          ]     [          VNET2           ]
     if1(epair) 2001:db8:a::1/64 <-> 2001:db8:a::2/64 if1(epair)
     if2(epair) 2001:db8:b::1/64 <-> 2001:db8:b::2/64 if2(epair)
                 lo0                             lo0

    """

    TOPOLOGY = {
        "vnet1": {"ifaces": ["if1", "if2"]},
        "vnet2": {"ifaces": ["if1", "if2"]},
        "if1": {"prefixes6": [("2001:db8:a::1/64", "2001:db8:a::2/64")]},
        "if2": {"prefixes6": [("2001:db8:b::1/64", "2001:db8:b::2/64")]},
    }

    def _get_iface_stat(self, os_ifname: str):
        out = ToolsHelper.get_output(
            "{} -I {} --libxo json".format(ToolsHelper.NETSTAT_PATH, os_ifname)
        )
        js = json.loads(out)
        return js["statistics"]["interface"][0]

    def vnet2_handler(self, vnet: VnetInstance):
        """
        Test handler that runs in the vnet2 as a separate process.

        This handler receives an interface name, fetches received/sent packets
         and returns this data back to the parent process.
        """
        while True:
            # receives 'ifX' with an infinite timeout
            iface_alias = self.wait_object(vnet.pipe, None)
            # Translates topology interface name to the actual OS-assigned name
            os_ifname = vnet.iface_alias_map[iface_alias].name
            self.send_object(vnet.pipe, self._get_iface_stat(os_ifname))

    @pytest.mark.skip(reason="comment me to run the test")
    @pytest.mark.require_user("root")
    def test_ifstat(self):
        """Checks that RX interface packets are properly accounted for"""
        second_vnet = self.vnet_map["vnet2"]
        pipe = second_vnet.pipe

        # Ping neighbor IP on if1 and verify that the counter was incremented
        self.send_object(pipe, "if1")
        old_stat = self.wait_object(pipe)
        assert subprocess.run("ping -c5 2001:db8:a::2".split()).returncode == 0
        self.send_object(pipe, "if1")
        new_stat = self.wait_object(pipe)
        assert new_stat["received-packets"] - old_stat["received-packets"] >= 5

        # Ping neighbor IP on if2 and verify that the counter was incremented
        self.send_object(pipe, "if2")
        old_stat = self.wait_object(pipe)
        assert subprocess.run("ping -c5 2001:db8:b::2".split()).returncode == 0
        self.send_object(pipe, "if2")
        new_stat = self.wait_object(pipe)
        assert new_stat["received-packets"] - old_stat["received-packets"] >= 5
