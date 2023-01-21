import errno
import socket

import pytest
from atf_python.sys.net.netlink import NetlinkTestTemplate
from atf_python.sys.net.netlink import NlConst
from atf_python.sys.net.vnet import SingleVnetTestTemplate


class TestNlCore(NetlinkTestTemplate, SingleVnetTestTemplate):
    @pytest.mark.parametrize(
        "params",
        [
            pytest.param({"type": socket.SOCK_RAW}, id="SOCK_RAW"),
            pytest.param({"type": socket.SOCK_DGRAM}, id="SOCK_DGRAM"),
        ],
    )
    def test_socket_type(self, params):
        s = socket.socket(NlConst.AF_NETLINK, params["type"], NlConst.NETLINK_ROUTE)
        s.close()

    @pytest.mark.parametrize(
        "params",
        [
            pytest.param({"type": socket.SOCK_STREAM}, id="SOCK_STREAM"),
            pytest.param({"type": socket.SOCK_RDM}, id="SOCK_RDM"),
            pytest.param({"type": socket.SOCK_SEQPACKET}, id="SOCK_SEQPACKET"),
        ],
    )
    def test_socket_type_unsup(self, params):
        with pytest.raises(OSError) as exc_info:
            socket.socket(NlConst.AF_NETLINK, params["type"], NlConst.NETLINK_ROUTE)
        assert exc_info.value.errno == errno.EPROTOTYPE
