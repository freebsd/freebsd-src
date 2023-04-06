#!/usr/bin/env python3
import os
import pwd
import re
from ctypes import CDLL
from ctypes import get_errno
from ctypes.util import find_library
from typing import Dict
from typing import List
from typing import Optional

from atf_python.sys.net.tools import ToolsHelper

import pytest


def run_cmd(cmd: str, verbose=True) -> str:
    print("run: '{}'".format(cmd))
    return os.popen(cmd).read()


def nodeid_to_method_name(nodeid: str) -> str:
    """file_name.py::ClassName::method_name[parametrize] -> method_name"""
    return nodeid.split("::")[-1].split("[")[0]


class LibCWrapper(object):
    def __init__(self):
        path: Optional[str] = find_library("c")
        if path is None:
            raise RuntimeError("libc not found")
        self._libc = CDLL(path, use_errno=True)

    def modfind(self, mod_name: str) -> int:
        if self._libc.modfind(bytes(mod_name, encoding="ascii")) == -1:
            return get_errno()
        return 0

    def jail_attach(self, jid: int) -> int:
        if self._libc.jail_attach(jid) != 0:
            return get_errno()
        return 0


libc = LibCWrapper()


class BaseTest(object):
    INTERFACES_FNAME = "created_ifaces.lst"
    NEED_ROOT: bool = False  # True if the class needs root privileges for the setup
    TARGET_USER = None  # Set to the target user by the framework
    REQUIRED_MODULES: List[str] = []

    def _check_modules(self):
        for mod_name in self.REQUIRED_MODULES:
            error_code = libc.modfind(mod_name)
            if error_code != 0:
                err_str = os.strerror(error_code)
                pytest.skip(
                    "kernel module '{}' not available: {}".format(mod_name, err_str)
                )
    @property
    def atf_vars(self) -> Dict[str, str]:
        px = "_ATF_VAR_"
        return {k[len(px):]: v for k, v in os.environ.items() if k.startswith(px)}

    def drop_privileges_user(self, user: str):
        uid = pwd.getpwnam(user)[2]
        print("Dropping privs to {}/{}".format(user, uid))
        os.setuid(uid)

    def drop_privileges(self):
        if self.TARGET_USER:
            if self.TARGET_USER == "unprivileged":
                user = self.atf_vars["unprivileged-user"]
            else:
                user = self.TARGET_USER
            self.drop_privileges_user(user)

    @property
    def test_id(self) -> str:
        # 'test_ip6_output.py::TestIP6Output::test_output6_pktinfo[ipandif] (setup)'
        return os.environ.get("PYTEST_CURRENT_TEST").split(" ")[0]

    def setup_method(self, method):
        """Run all pre-requisits for the test execution"""
        self._check_modules()

    def cleanup(self, test_id: str):
        # XXX Append to report.sections instead to avoid triggering another test
        print("==== cleanup ===")
        IfaceFactory().cleanup_interfaces()
        IfaceFactory().cleanup()


class Interface(object):
    # defines from net/if_types.h
    IFT_LOOP = 0x18
    IFT_ETHER = 0x06

    def __init__(self, iface_alias: str, iface_name: str):
        self.name = iface_name
        self.alias = iface_alias
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
    def first_ipv6_ll(self):
        out = self.run_cmd("ifconfig {} inet6".format(self.name))
        addrs = [l.strip() for l in out.splitlines() if "inet6" in l]
        for addr in addrs:
            parts = addr.split()
            addr = parts[1]
            plen = parts[3]
            if addr.startswith("fe80"):
                addr = addr.split("%")[0]
                ll_addr = ipaddress.ip_interface("{}%{}".format(addr, plen))
                return ll_addr

    @property
    def first_ipv4(self):
        d = self.addr_map["inet"]
        return d[next(iter(d))]

    @property
    def mac(self):
        out = self.run_cmd("ifconfig {} ether".format(self.name))
        return [l.strip() for l in out.splitlines() if "ether" in l][0].split()[1]

    def run_cmd(
        self,
        cmd,
        verbose=False,
    ):
        return run_cmd(cmd, verbose)

    @classmethod
    def create_iface(cls, alias_name: str, iface_name: str) -> List["Interface"]:
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
        cmd = "/usr/sbin/ndp -i {} -disabled".format(self.name)
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

    def create_iface(self, alias_name: str, iface_name: str) -> List[Interface]:
        ifaces = Interface.create_iface(alias_name, iface_name)
        for iface in ifaces:
            if not self.is_autodeleted(iface.name):
                self._register_iface(iface.name)
        return ifaces

    @staticmethod
    def is_autodeleted(iface_name: str) -> bool:
        return False

    def cleanup_interfaces(self) -> List[str]:
        """Destroys interfaces"""
        ifaces_lst = ToolsHelper.get_output("ifconfig -l")
        for iface_name in ifaces_lst.split():
            if not self.is_autodeleted(iface_name):
                if iface_name not in self._list_ifaces():
                    print("Skipping interface {}".format(iface_name))
                    continue
            # XXX epairs should only delete one side
            run_cmd("ifconfig {} destroy".format(iface_name))

    def cleanup(self):
        try:
            os.unlink(self.INTERFACES_FNAME)
        except OSError:
            pass
