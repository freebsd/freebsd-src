#!/usr/bin/env python3
import os
from ctypes import CDLL
from ctypes import get_errno
from ctypes.util import find_library
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
    def test_id(self):
        # 'test_ip6_output.py::TestIP6Output::test_output6_pktinfo[ipandif] (setup)'
        return os.environ.get("PYTEST_CURRENT_TEST").split(" ")[0]

    def setup_method(self, method):
        """Run all pre-requisits for the test execution"""
        self._check_modules()

    def cleanup(self, test_id: str):
        """Cleanup all test resources here"""
        pass
