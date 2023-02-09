#!/usr/bin/env python3
import os
import pwd
from ctypes import CDLL
from ctypes import get_errno
from ctypes.util import find_library
from typing import Dict
from typing import List
from typing import Optional

import pytest


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
