import mmap
import pytest

from atf_python.ktest import BaseKernelTest
from atf_python.sys.netlink.attrs import NlAttrU32


M_NOWAIT = 1
M_WAITOK = 2
NS_WRITER_TYPE_MBUF = 0
NS_WRITER_TYPE_BUF = 1
NS_WRITER_TYPE_LBUF = 1

MHLEN = 160
MCLBYTES = 2048  # XXX: may differ on some archs?
MJUMPAGESIZE = mmap.PAGESIZE
MJUM9BYTES = 9 * 1024
MJUM16BYTES = 16 * 1024

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
        "writer_type",
        [
            pytest.param(NS_WRITER_TYPE_MBUF, id="MBUF"),
            pytest.param(NS_WRITER_TYPE_BUF, id="BUF"),
        ],
    )
    @pytest.mark.parametrize(
        "sz",
        [
            pytest.param([160, 160], id="MHLEN"),
            pytest.param([MCLBYTES, MCLBYTES], id="MCLBYTES"),
        ],
    )
    def test_mbuf_writer_allocation(self, sz, writer_type, malloc_flags):
        """override to parametrize"""

        test_meta = [
            NlAttrU32(1, sz[0]),  # size
            NlAttrU32(2, sz[1]),  # expected_avail
            NlAttrU32(4, writer_type),
            NlAttrU32(5, malloc_flags),
        ]
        self.runtest(test_meta)

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
            pytest.param([160, 160, 1], id="MHLEN"),
            pytest.param([MCLBYTES, MCLBYTES, 1], id="MCLBYTES"),
            pytest.param([MCLBYTES + 1, MCLBYTES + 1, 2], id="MCLBYTES_MHLEN"),
            pytest.param([MCLBYTES + 256, MCLBYTES * 2, 2], id="MCLBYTESx2"),
        ],
    )
    def test_mbuf_chain_allocation(self, sz, malloc_flags):
        test_meta = [
            NlAttrU32(1, sz[0]),  # size
            NlAttrU32(2, sz[1]),  # expected_avail
            NlAttrU32(3, sz[2]),  # expected_count
            NlAttrU32(5, malloc_flags),
        ]
        self.runtest(test_meta)

    @pytest.mark.parametrize(
        "sz",
        [
            # buffer size, completed message, unfinished message, attribute
            pytest.param([NLMSG_SMALL, 0, 20, 112], id="small"),
            pytest.param([NLMSG_SMALL, 0, 20, 8176], id="large_attr"),
            pytest.param([NLMSG_LARGE, 0, 2000, 4092], id="large_msg"),
            pytest.param([NLMSG_LARGE, 100, 1900, 196], id="after_completed"),
        ],
    )
    def test_nlbuf_writer_refill(self, sz):
        """override to parametrize"""

        test_meta = [
            NlAttrU32(1, sz[0]),  # size
            NlAttrU32(2, sz[1]),  # completed
            NlAttrU32(3, sz[2]),  # unfinished
            NlAttrU32(4, sz[3]),  # attr_len
        ]
        self.runtest(test_meta)
