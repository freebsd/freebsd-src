from atf_python.utils import BaseTest
from atf_python.utils import IfaceFactory

import pytest
import subprocess
import re


class TestIfconfig(BaseTest):
    def test_bridge_create_without_unit_number(self):
        """PR: 270618"""

        # Create an epair interface
        iface = IfaceFactory().create_iface("", "epair")[0].name

        # Create a bridge and add the epair as a member in a single command
        ifconfig_cmd = f"ifconfig bridge create addm {iface}".split()
        cmd = subprocess.run(ifconfig_cmd, capture_output=True, text=True)

        # Add the bridge to the list of interfaces to be removed upon cleanup
        IfaceFactory()._register_iface(cmd.stdout)

        assert cmd.returncode == 0
        assert re.match("bridge\d.*\n", cmd.stdout)
        assert cmd.stderr == ""
