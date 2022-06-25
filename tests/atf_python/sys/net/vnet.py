#!/usr/local/bin/python3
import os
import socket
import time
from ctypes import cdll
from ctypes import get_errno
from ctypes.util import find_library
from typing import List
from typing import Optional


def run_cmd(cmd: str) -> str:
    print("run: '{}'".format(cmd))
    return os.popen(cmd).read()


class VnetInterface(object):
    INTERFACES_FNAME = "created_interfaces.lst"

    # defines from net/if_types.h
    IFT_LOOP = 0x18
    IFT_ETHER = 0x06

    def __init__(self, iface_name: str):
        self.name = iface_name
        self.vnet_name = ""
        self.jailed = False
        if iface_name.startswith("lo"):
            self.iftype = self.IFT_LOOP
        else:
            self.iftype = self.IFT_ETHER

    @property
    def ifindex(self):
        return socket.if_nametoindex(self.name)

    def set_vnet(self, vnet_name: str):
        self.vnet_name = vnet_name

    def set_jailed(self, jailed: bool):
        self.jailed = jailed

    def run_cmd(self, cmd):
        if self.vnet_name and not self.jailed:
            cmd = "jexec {} {}".format(self.vnet_name, cmd)
        run_cmd(cmd)

    @staticmethod
    def file_append_line(line):
        with open(VnetInterface.INTERFACES_FNAME, "a") as f:
            f.write(line + "\n")

    @classmethod
    def create_iface(cls, iface_name: str):
        name = run_cmd("/sbin/ifconfig {} create".format(iface_name)).rstrip()
        if not name:
            raise Exception("Unable to create iface {}".format(iface_name))
        cls.file_append_line(name)
        if name.startswith("epair"):
            cls.file_append_line(name[:-1] + "b")
        return cls(name)

    @staticmethod
    def cleanup_ifaces():
        try:
            with open(VnetInterface.INTERFACES_FNAME, "r") as f:
                for line in f:
                    run_cmd("/sbin/ifconfig {} destroy".format(line.strip()))
            os.unlink(VnetInterface.INTERFACES_FNAME)
        except Exception:
            pass

    def setup_addr(self, addr: str):
        if ":" in addr:
            family = "inet6"
        else:
            family = "inet"
        cmd = "/sbin/ifconfig {} {} {}".format(self.name, family, addr)
        self.run_cmd(cmd)

    def delete_addr(self, addr: str):
        if ":" in addr:
            cmd = "/sbin/ifconfig {} inet6 {} delete".format(self.name, addr)
        else:
            cmd = "/sbin/ifconfig {} -alias {}".format(self.name, addr)
        self.run_cmd(cmd)

    def turn_up(self):
        cmd = "/sbin/ifconfig {} up".format(self.name)
        self.run_cmd(cmd)

    def enable_ipv6(self):
        cmd = "/usr/sbin/ndp -i {} -disabled".format(self.name)
        self.run_cmd(cmd)


class VnetInstance(object):
    JAILS_FNAME = "created_jails.lst"

    def __init__(self, vnet_name: str, jid: int, ifaces: List[VnetInterface]):
        self.name = vnet_name
        self.jid = jid
        self.ifaces = ifaces
        for iface in ifaces:
            iface.set_vnet(vnet_name)
            iface.set_jailed(True)

    def run_vnet_cmd(self, cmd):
        if self.vnet_name:
            cmd = "jexec {} {}".format(self.vnet_name, cmd)
        return run_cmd(cmd)

    @staticmethod
    def wait_interface(vnet_name: str, iface_name: str):
        cmd = "jexec {} /sbin/ifconfig -l".format(vnet_name)
        for i in range(50):
            ifaces = run_cmd(cmd).strip().split(" ")
            if iface_name in ifaces:
                return True
            time.sleep(0.1)
        return False

    @staticmethod
    def file_append_line(line):
        with open(VnetInstance.JAILS_FNAME, "a") as f:
            f.write(line + "\n")

    @staticmethod
    def cleanup_vnets():
        try:
            with open(VnetInstance.JAILS_FNAME) as f:
                for line in f:
                    run_cmd("/usr/sbin/jail -r {}".format(line.strip()))
            os.unlink(VnetInstance.JAILS_FNAME)
        except Exception:
            pass

    @classmethod
    def create_with_interfaces(cls, vnet_name: str, ifaces: List[VnetInterface]):
        iface_cmds = " ".join(["vnet.interface={}".format(i.name) for i in ifaces])
        cmd = "/usr/sbin/jail -i -c name={} persist vnet {}".format(
            vnet_name, iface_cmds
        )
        jid_str = run_cmd(cmd)
        jid = int(jid_str)
        if jid <= 0:
            raise Exception("Jail creation failed, output: {}".format(jid))
        cls.file_append_line(vnet_name)

        for iface in ifaces:
            if cls.wait_interface(vnet_name, iface.name):
                continue
            raise Exception(
                "Interface {} has not appeared in vnet {}".format(iface.name, vnet_name)
            )
        return cls(vnet_name, jid, ifaces)

    @staticmethod
    def attach_jid(jid: int):
        _path: Optional[str] = find_library("c")
        if _path is None:
            raise Exception("libc not found")
        path: str = _path
        libc = cdll.LoadLibrary(path)
        if libc.jail_attach(jid) != 0:
            raise Exception("jail_attach() failed: errno {}".format(get_errno()))

    def attach(self):
        self.attach_jid(self.jid)


class SingleVnetTestTemplate(object):
    num_epairs = 1
    IPV6_PREFIXES: List[str] = []
    IPV4_PREFIXES: List[str] = []

    def setup_method(self, method):
        test_name = method.__name__
        vnet_name = "jail_{}".format(test_name)
        ifaces = []
        for i in range(self.num_epairs):
            ifaces.append(VnetInterface.create_iface("epair"))
        self.vnet = VnetInstance.create_with_interfaces(vnet_name, ifaces)
        self.vnet.attach()
        for i, addr in enumerate(self.IPV6_PREFIXES):
            if addr:
                iface = self.vnet.ifaces[i]
                iface.turn_up()
                iface.enable_ipv6()
                iface.setup_addr(addr)
        for i, addr in enumerate(self.IPV4_PREFIXES):
            if addr:
                iface = self.vnet.ifaces[i]
                iface.turn_up()
                iface.setup_addr(addr)

    def cleanup(self, nodeid: str):
        print("==== vnet cleanup ===")
        VnetInstance.cleanup_vnets()
        VnetInterface.cleanup_ifaces()

    def run_cmd(self, cmd: str) -> str:
        return os.popen(cmd).read()
