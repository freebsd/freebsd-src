import mmap
import pytest

from atf_python.ktest import BaseKernelTest
from atf_python.sys.netlink.attrs import NlAttrU32

M_NOWAIT = 1
M_WAITOK = 2

NLMSG_SMALL = 128
NLMSG_LARGE = 2048

class TestNetlinkMessageWriter(BaseKernelTest):
    KTEST_MODULE_NAME = "ktest_netlink_message_writer"

    @pytest.mark.parametrize(
        "malloc_flags",
        [
            pytest.param(M_NOWAIT, id="NOWAIT"),
            pytest.param(M_WAITOK, id="WAITOK"),
        ],
    )
    @pytest.mark.parametrize(
        "sz",
        [
            pytest.param([NLMSG_SMALL, NLMSG_SMALL], id="NLMSG_SMALL"),
            pytest.param([NLMSG_LARGE, NLMSG_LARGE], id="NLMSG_LARGE"),
            pytest.param([NLMSG_LARGE + 256, NLMSG_LARGE + 256], id="NLMSG_LARGE+256"),
        ],
    )
    def test_nlbuf_writer_allocation(self, sz, malloc_flags):
        """override to parametrize"""

        test_meta = [
            NlAttrU32(1, sz[0]),  # size
            NlAttrU32(2, sz[1]),  # expected_avail
            NlAttrU32(3, malloc_flags),
        ]
        self.runtest(test_meta)
