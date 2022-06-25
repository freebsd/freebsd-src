#!/usr/local/bin/python3
import json
import os
import socket
import time
from ctypes import cdll
from ctypes import get_errno
from ctypes.util import find_library
from typing import List
from typing import Optional


class ToolsHelper(object):
    NETSTAT_PATH = "/usr/bin/netstat"

    @classmethod
    def get_output(cls, cmd: str, verbose=False) -> str:
        if verbose:
            print("run: '{}'".format(cmd))
        return os.popen(cmd).read()

    @classmethod
    def get_routes(cls, family: str, fibnum: int = 0):
        family_key = {"inet": "-4", "inet6": "-6"}.get(family)
        out = cls.get_output(
            "{} {} -rn -F {} --libxo json".format(cls.NETSTAT_PATH, family_key, fibnum)
        )
        js = json.loads(out)
        js = js["statistics"]["route-information"]["route-table"]["rt-family"]
        if js:
            return js[0]["rt-entry"]
        else:
            return []
