#!/usr/local/bin/python3
import copy
import ipaddress
import os
import re
import socket
import sys
import time
from multiprocessing import connection
from multiprocessing import Pipe
from multiprocessing import Process
from typing import Dict
from typing import List
from typing import NamedTuple

from atf_python.sys.net.tools import ToolsHelper
from atf_python.utils import BaseTest
from atf_python.utils import libc


def run_cmd(cmd: str, verbose=True) -> str:
    if verbose:
        print("run: '{}'".format(cmd))
    return os.popen(cmd).read()


def get_topology_id(test_id: str) -> str:
    """
    Gets a unique topology id based on the pytest test_id.
      "test_ip6_output.py::TestIP6Output::test_output6_pktinfo[ipandif]" ->
      "TestIP6Output:test_output6_pktinfo[ipandif]"
    """
    return ":".join(test_id.split("::")[-2:])


def convert_test_name(test_name: str) -> str:
    """Convert test name to a string that can be used in the file/jail names"""
    ret = ""
    for char in test_name:
        if char.isalnum() or char in ("_", "-", ":"):
            ret += char
        elif char in ("["):
            ret += "_"
    return ret


class VnetInterface(object):
    # defines from net/if_types.h
    IFT_LOOP = 0x18
    IFT_ETHER = 0x06

    def __init__(self, iface_alias: str, iface_name: str):
        self.name = iface_name
        self.alias = iface_alias
        self.vnet_name = ""
        self.jailed = False
        self.addr_map: Dict[str, Dict] = {"inet6": {}, "inet": {}}
        self.prefixes4: List[List[str]] = []
        self.prefixes6: List[List[str]] = []
        if iface_name.startswith("lo"):
            self.iftype = self.IFT_LOOP
        else:
            self.iftype = self.IFT_ETHER

    @property
    def ifindex(self):
        return socket.if_nametoindex(self.name)

    @property
    def first_ipv6(self):
        d = self.addr_map["inet6"]
        return d[next(iter(d))]

    @property
    def first_ipv4(self):
        d = self.addr_map["inet"]
        return d[next(iter(d))]

    def set_vnet(self, vnet_name: str):
        self.vnet_name = vnet_name

    def set_jailed(self, jailed: bool):
        self.jailed = jailed

    def run_cmd(self, cmd, verbose=False):
        if self.vnet_name and not self.jailed:
            cmd = "/usr/sbin/jexec {} {}".format(self.vnet_name, cmd)
        return run_cmd(cmd, verbose)

    @classmethod
    def setup_loopback(cls, vnet_name: str):
        lo = VnetInterface("", "lo0")
        lo.set_vnet(vnet_name)
        lo.setup_addr("127.0.0.1/8")
        lo.turn_up()

    @classmethod
    def create_iface(cls, alias_name: str, iface_name: str) -> List["VnetInterface"]:
        name = run_cmd("/sbin/ifconfig {} create".format(iface_name)).rstrip()
        if not name:
            raise Exception("Unable to create iface {}".format(iface_name))
        ret = [cls(alias_name, name)]
        if name.startswith("epair"):
            ret.append(cls(alias_name, name[:-1] + "b"))
        return ret

    def setup_addr(self, _addr: str):
        addr = ipaddress.ip_interface(_addr)
        if addr.version == 6:
            family = "inet6"
            cmd = "/sbin/ifconfig {} {} {}".format(self.name, family, addr)
        else:
            family = "inet"
            if self.addr_map[family]:
                cmd = "/sbin/ifconfig {} alias {}".format(self.name, addr)
            else:
                cmd = "/sbin/ifconfig {} {} {}".format(self.name, family, addr)
        self.run_cmd(cmd)
        self.addr_map[family][str(addr.ip)] = addr

    def delete_addr(self, _addr: str):
        addr = ipaddress.ip_address(_addr)
        if addr.version == 6:
            family = "inet6"
            cmd = "/sbin/ifconfig {} inet6 {} delete".format(self.name, addr)
        else:
            family = "inet"
            cmd = "/sbin/ifconfig {} -alias {}".format(self.name, addr)
        self.run_cmd(cmd)
        del self.addr_map[family][str(addr)]

    def turn_up(self):
        cmd = "/sbin/ifconfig {} up".format(self.name)
        self.run_cmd(cmd)

    def enable_ipv6(self):
        cmd = "/usr/sbin/ndp -i {} -- -disabled".format(self.name)
        self.run_cmd(cmd)

    def has_tentative(self) -> bool:
        """True if an interface has some addresses in tenative state"""
        cmd = "/sbin/ifconfig {} inet6".format(self.name)
        out = self.run_cmd(cmd, verbose=False)
        for line in out.splitlines():
            if "tentative" in line:
                return True
        return False


class IfaceFactory(object):
    INTERFACES_FNAME = "created_ifaces.lst"
    AUTODELETE_TYPES = ("epair", "gif", "gre", "lo", "tap", "tun")

    def __init__(self):
        self.file_name = self.INTERFACES_FNAME

    def _register_iface(self, iface_name: str):
        with open(self.file_name, "a") as f:
            f.write(iface_name + "\n")

    def _list_ifaces(self) -> List[str]:
        ret: List[str] = []
        try:
            with open(self.file_name, "r") as f:
                for line in f:
                    ret.append(line.strip())
        except OSError:
            pass
        return ret

    def create_iface(self, alias_name: str, iface_name: str) -> List[VnetInterface]:
        ifaces = VnetInterface.create_iface(alias_name, iface_name)
        for iface in ifaces:
            if not self.is_autodeleted(iface.name):
                self._register_iface(iface.name)
        return ifaces

    @staticmethod
    def is_autodeleted(iface_name: str) -> bool:
        if iface_name == "lo0":
            return False
        iface_type = re.split(r"\d+", iface_name)[0]
        return iface_type in IfaceFactory.AUTODELETE_TYPES

    def cleanup_vnet_interfaces(self, vnet_name: str) -> List[str]:
        """Destroys"""
        ifaces_lst = ToolsHelper.get_output(
            "/usr/sbin/jexec {} /sbin/ifconfig -l".format(vnet_name)
        )
        for iface_name in ifaces_lst.split():
            if not self.is_autodeleted(iface_name):
                if iface_name not in self._list_ifaces():
                    print("Skipping interface {}:{}".format(vnet_name, iface_name))
                    continue
            run_cmd(
                "/usr/sbin/jexec {} /sbin/ifconfig {} destroy".format(vnet_name, iface_name)
            )

    def cleanup(self):
        try:
            os.unlink(self.INTERFACES_FNAME)
        except OSError:
            pass


class VnetInstance(object):
    def __init__(
        self, vnet_alias: str, vnet_name: str, jid: int, ifaces: List[VnetInterface]
    ):
        self.name = vnet_name
        self.alias = vnet_alias  # reference in the test topology
        self.jid = jid
        self.ifaces = ifaces
        self.iface_alias_map = {}  # iface.alias: iface
        self.iface_map = {}  # iface.name: iface
        for iface in ifaces:
            iface.set_vnet(vnet_name)
            iface.set_jailed(True)
            self.iface_alias_map[iface.alias] = iface
            self.iface_map[iface.name] = iface
            # Allow reference to interfce aliases as attributes
            setattr(self, iface.alias, iface)
        self.need_dad = False  # Disable duplicate address detection by default
        self.attached = False
        self.pipe = None
        self.subprocess = None

    def run_vnet_cmd(self, cmd, verbose=True):
        if not self.attached:
            cmd = "/usr/sbin/jexec {} {}".format(self.name, cmd)
        return run_cmd(cmd, verbose)

    def disable_dad(self):
        self.run_vnet_cmd("/sbin/sysctl net.inet6.ip6.dad_count=0")

    def set_pipe(self, pipe):
        self.pipe = pipe

    def set_subprocess(self, p):
        self.subprocess = p

    @staticmethod
    def attach_jid(jid: int):
        error_code = libc.jail_attach(jid)
        if error_code != 0:
            raise Exception("jail_attach() failed: errno {}".format(error_code))

    def attach(self):
        self.attach_jid(self.jid)
        self.attached = True


class VnetFactory(object):
    JAILS_FNAME = "created_jails.lst"

    def __init__(self, topology_id: str):
        self.topology_id = topology_id
        self.file_name = self.JAILS_FNAME
        self._vnets: List[str] = []

    def _register_vnet(self, vnet_name: str):
        self._vnets.append(vnet_name)
        with open(self.file_name, "a") as f:
            f.write(vnet_name + "\n")

    @staticmethod
    def _wait_interfaces(vnet_name: str, ifaces: List[str]) -> List[str]:
        cmd = "/usr/sbin/jexec {} /sbin/ifconfig -l".format(vnet_name)
        not_matched: List[str] = []
        for i in range(50):
            vnet_ifaces = run_cmd(cmd).strip().split(" ")
            not_matched = []
            for iface_name in ifaces:
                if iface_name not in vnet_ifaces:
                    not_matched.append(iface_name)
            if len(not_matched) == 0:
                return []
            time.sleep(0.1)
        return not_matched

    def create_vnet(self, vnet_alias: str, ifaces: List[VnetInterface]):
        vnet_name = "pytest:{}".format(convert_test_name(self.topology_id))
        if self._vnets:
            # add number to distinguish jails
            vnet_name = "{}_{}".format(vnet_name, len(self._vnets) + 1)
        iface_cmds = " ".join(["vnet.interface={}".format(i.name) for i in ifaces])
        cmd = "/usr/sbin/jail -i -c name={} persist vnet {}".format(
            vnet_name, iface_cmds
        )
        jid = 0
        try:
            jid_str = run_cmd(cmd)
            jid = int(jid_str)
        except ValueError:
            print("Jail creation failed, output: {}".format(jid_str))
            raise
        self._register_vnet(vnet_name)

        # Run expedited version of routing
        VnetInterface.setup_loopback(vnet_name)

        not_found = self._wait_interfaces(vnet_name, [i.name for i in ifaces])
        if not_found:
            raise Exception(
                "Interfaces {} has not appeared in vnet {}".format(not_found, vnet_name)
            )
        return VnetInstance(vnet_alias, vnet_name, jid, ifaces)

    def cleanup(self):
        iface_factory = IfaceFactory()
        try:
            with open(self.file_name) as f:
                for line in f:
                    vnet_name = line.strip()
                    iface_factory.cleanup_vnet_interfaces(vnet_name)
                    run_cmd("/usr/sbin/jail -r  {}".format(vnet_name))
            os.unlink(self.JAILS_FNAME)
        except OSError:
            pass


class SingleInterfaceMap(NamedTuple):
    ifaces: List[VnetInterface]
    vnet_aliases: List[str]


class ObjectsMap(NamedTuple):
    iface_map: Dict[str, SingleInterfaceMap]  # keyed by ifX
    vnet_map: Dict[str, VnetInstance]  # keyed by vnetX
    topo_map: Dict  # self.TOPOLOGY


class VnetTestTemplate(BaseTest):
    NEED_ROOT: bool = True
    TOPOLOGY = {}

    def _require_default_modules(self):
        libc.kldload("if_epair.ko")
        self.require_module("if_epair")

    def _get_vnet_handler(self, vnet_alias: str):
        handler_name = "{}_handler".format(vnet_alias)
        return getattr(self, handler_name, None)

    def _setup_vnet(self, vnet: VnetInstance, obj_map: Dict, pipe):
        """Base Handler to setup given VNET.
        Can be run in a subprocess. If so, passes control to the special
        vnetX_handler() after setting up interface addresses
        """
        vnet.attach()
        print("# setup_vnet({})".format(vnet.name))
        if pipe is not None:
            vnet.set_pipe(pipe)

        topo = obj_map.topo_map
        ipv6_ifaces = []
        # Disable DAD
        if not vnet.need_dad:
            vnet.disable_dad()
        for iface in vnet.ifaces:
            # check index of vnet within an interface
            # as we have prefixes for both ends of the interface
            iface_map = obj_map.iface_map[iface.alias]
            idx = iface_map.vnet_aliases.index(vnet.alias)
            prefixes6 = topo[iface.alias].get("prefixes6", [])
            prefixes4 = topo[iface.alias].get("prefixes4", [])
            if prefixes6 or prefixes4:
                ipv6_ifaces.append(iface)
                iface.turn_up()
                if prefixes6:
                    iface.enable_ipv6()
            for prefix in prefixes6 + prefixes4:
                if prefix[idx]:
                    iface.setup_addr(prefix[idx])
        for iface in ipv6_ifaces:
            while iface.has_tentative():
                time.sleep(0.1)

        # Run actual handler
        handler = self._get_vnet_handler(vnet.alias)
        if handler:
            # Do unbuffered stdout for children
            # so the logs are present if the child hangs
            sys.stdout.reconfigure(line_buffering=True)
            self.drop_privileges()
            handler(vnet)

    def _get_topo_ifmap(self, topo: Dict):
        iface_factory = IfaceFactory()
        iface_map: Dict[str, SingleInterfaceMap] = {}
        iface_aliases = set()
        for obj_name, obj_data in topo.items():
            if obj_name.startswith("vnet"):
                for iface_alias in obj_data["ifaces"]:
                    iface_aliases.add(iface_alias)
        for iface_alias in iface_aliases:
            print("Creating {}".format(iface_alias))
            iface_data = topo[iface_alias]
            iface_type = iface_data.get("type", "epair")
            ifaces = iface_factory.create_iface(iface_alias, iface_type)
            smap = SingleInterfaceMap(ifaces, [])
            iface_map[iface_alias] = smap
        return iface_map

    def setup_topology(self, topo: Dict, topology_id: str):
        """Creates jails & interfaces for the provided topology"""
        vnet_map = {}
        vnet_factory = VnetFactory(topology_id)
        iface_map = self._get_topo_ifmap(topo)
        for obj_name, obj_data in topo.items():
            if obj_name.startswith("vnet"):
                vnet_ifaces = []
                for iface_alias in obj_data["ifaces"]:
                    # epair creates 2 interfaces, grab first _available_
                    # and map it to the VNET being created
                    idx = len(iface_map[iface_alias].vnet_aliases)
                    iface_map[iface_alias].vnet_aliases.append(obj_name)
                    vnet_ifaces.append(iface_map[iface_alias].ifaces[idx])
                vnet = vnet_factory.create_vnet(obj_name, vnet_ifaces)
                vnet_map[obj_name] = vnet
                # Allow reference to VNETs as attributes
                setattr(self, obj_name, vnet)
        # Debug output
        print("============= TEST TOPOLOGY =============")
        for vnet_alias, vnet in vnet_map.items():
            print("# vnet {} -> {}".format(vnet.alias, vnet.name), end="")
            handler = self._get_vnet_handler(vnet.alias)
            if handler:
                print(" handler: {}".format(handler.__name__), end="")
            print()
        for iface_alias, iface_data in iface_map.items():
            vnets = iface_data.vnet_aliases
            ifaces: List[VnetInterface] = iface_data.ifaces
            if len(vnets) == 1 and len(ifaces) == 2:
                print(
                    "# iface {}: {}::{} -> main::{}".format(
                        iface_alias, vnets[0], ifaces[0].name, ifaces[1].name
                    )
                )
            elif len(vnets) == 2 and len(ifaces) == 2:
                print(
                    "# iface {}: {}::{} -> {}::{}".format(
                        iface_alias, vnets[0], ifaces[0].name, vnets[1], ifaces[1].name
                    )
                )
            else:
                print(
                    "# iface {}: ifaces: {} vnets: {}".format(
                        iface_alias, vnets, [i.name for i in ifaces]
                    )
                )
        print()
        return ObjectsMap(iface_map, vnet_map, topo)

    def setup_method(self, _method):
        """Sets up all the required topology and handlers for the given test"""
        super().setup_method(_method)
        self._require_default_modules()

        # TestIP6Output.test_output6_pktinfo[ipandif]
        topology_id = get_topology_id(self.test_id)
        topology = self.TOPOLOGY
        # First, setup kernel objects - interfaces & vnets
        obj_map = self.setup_topology(topology, topology_id)
        main_vnet = None  # one without subprocess handler
        for vnet_alias, vnet in obj_map.vnet_map.items():
            if self._get_vnet_handler(vnet_alias):
                # Need subprocess to run
                parent_pipe, child_pipe = Pipe()
                p = Process(
                    target=self._setup_vnet,
                    args=(
                        vnet,
                        obj_map,
                        child_pipe,
                    ),
                )
                vnet.set_pipe(parent_pipe)
                vnet.set_subprocess(p)
                p.start()
            else:
                if main_vnet is not None:
                    raise Exception("there can be only 1 VNET w/o handler")
                main_vnet = vnet
        # Main vnet needs to be the last, so all the other subprocesses
        # are started & their pipe handles collected
        self.vnet = main_vnet
        self._setup_vnet(main_vnet, obj_map, None)
        # Save state for the main handler
        self.iface_map = obj_map.iface_map
        self.vnet_map = obj_map.vnet_map
        self.drop_privileges()

    def cleanup(self, test_id: str):
        # pytest test id: file::class::test_name
        topology_id = get_topology_id(self.test_id)

        print("============= vnet cleanup =============")
        print("# topology_id: '{}'".format(topology_id))
        VnetFactory(topology_id).cleanup()
        IfaceFactory().cleanup()

    def wait_object(self, pipe, timeout=5):
        if pipe.poll(timeout):
            return pipe.recv()
        raise TimeoutError

    def wait_objects_any(self, pipe_list, timeout=5):
        objects = connection.wait(pipe_list, timeout)
        if objects:
            return objects[0].recv()
        raise TimeoutError

    def send_object(self, pipe, obj):
        pipe.send(obj)

    def wait(self):
        while True:
            time.sleep(1)

    @property
    def curvnet(self):
        pass


class SingleVnetTestTemplate(VnetTestTemplate):
    IPV6_PREFIXES: List[str] = []
    IPV4_PREFIXES: List[str] = []
    IFTYPE = "epair"

    def _setup_default_topology(self):
        topology = copy.deepcopy(
            {
                "vnet1": {"ifaces": ["if1"]},
                "if1": {"type": self.IFTYPE, "prefixes4": [], "prefixes6": []},
            }
        )
        for prefix in self.IPV6_PREFIXES:
            topology["if1"]["prefixes6"].append((prefix,))
        for prefix in self.IPV4_PREFIXES:
            topology["if1"]["prefixes4"].append((prefix,))
        return topology

    def setup_method(self, method):
        if not getattr(self, "TOPOLOGY", None):
            self.TOPOLOGY = self._setup_default_topology()
        else:
            names = self.TOPOLOGY.keys()
            assert len([n for n in names if n.startswith("vnet")]) == 1
        super().setup_method(method)
