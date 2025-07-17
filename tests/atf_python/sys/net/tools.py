#!/usr/local/bin/python3
import json
import os
import subprocess


class ToolsHelper(object):
    NETSTAT_PATH = "/usr/bin/netstat"
    IFCONFIG_PATH = "/sbin/ifconfig"

    @classmethod
    def get_output(cls, cmd: str, verbose=False) -> str:
        if verbose:
            print("run: '{}'".format(cmd))
        return os.popen(cmd).read()

    @classmethod
    def pf_rules(cls, rules, verbose=True):
        pf_conf = ""
        for r in rules:
            pf_conf = pf_conf + r + "\n"

        if verbose:
            print("Set rules:")
            print(pf_conf)

        ps = subprocess.Popen("/sbin/pfctl -g -f -", shell=True,
            stdin=subprocess.PIPE)
        ps.communicate(bytes(pf_conf, 'utf-8'))
        ret = ps.wait()
        if ret != 0:
            raise Exception("Failed to set pf rules %d" % ret)

        if verbose:
            cls.print_output("/sbin/pfctl -sr")

    @classmethod
    def print_output(cls, cmd: str, verbose=True):
        if verbose:
            print("======= {} =====".format(cmd))
        print(cls.get_output(cmd))
        if verbose:
            print()

    @classmethod
    def print_net_debug(cls):
        cls.print_output("ifconfig")
        cls.print_output("netstat -rnW")

    @classmethod
    def set_sysctl(cls, oid, val):
        cls.get_output("sysctl {}={}".format(oid, val))

    @classmethod
    def get_routes(cls, family: str, fibnum: int = 0):
        family_key = {"inet": "-4", "inet6": "-6"}.get(family)
        out = cls.get_output(
            "{} {} -rnW -F {} --libxo json".format(cls.NETSTAT_PATH, family_key, fibnum)
        )
        js = json.loads(out)
        js = js["statistics"]["route-information"]["route-table"]["rt-family"]
        if js:
            return js[0]["rt-entry"]
        else:
            return []

    @classmethod
    def get_nhops(cls, family: str, fibnum: int = 0):
        family_key = {"inet": "-4", "inet6": "-6"}.get(family)
        out = cls.get_output(
            "{} {} -onW -F {} --libxo json".format(cls.NETSTAT_PATH, family_key, fibnum)
        )
        js = json.loads(out)
        js = js["statistics"]["route-nhop-information"]["nhop-table"]["rt-family"]
        if js:
            return js[0]["nh-entry"]
        else:
            return []

    @classmethod
    def get_linklocals(cls):
        ret = {}
        ifname = None
        ips = []
        for line in cls.get_output(cls.IFCONFIG_PATH).splitlines():
            if line[0].isalnum():
                if ifname:
                    ret[ifname] = ips
                    ips = []
                ifname = line.split(":")[0]
            else:
                words = line.split()
                if words[0] == "inet6" and words[1].startswith("fe80"):
                    # inet6 fe80::1%lo0 prefixlen 64 scopeid 0x2
                    ip = words[1].split("%")[0]
                    scopeid = int(words[words.index("scopeid") + 1], 16)
                    ips.append((ip, scopeid))
        if ifname:
            ret[ifname] = ips
        return ret
