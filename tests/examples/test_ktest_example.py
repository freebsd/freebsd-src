import pytest

from atf_python.ktest import BaseKernelTest

from atf_python.sys.netlink.attrs import NlAttrStr
from atf_python.sys.netlink.attrs import NlAttrU32


class TestExample(BaseKernelTest):
    KTEST_MODULE_NAME = "ktest_example"

    @pytest.mark.parametrize(
        "numbers",
        [
            pytest.param([1, 2], id="1_2_Sum"),
            pytest.param([3, 4], id="3_4_Sum"),
        ],
    )
    def test_with_params(self, numbers):
        """override to parametrize"""

        test_meta = [
            NlAttrU32(1, numbers[0]),
            NlAttrU32(2, numbers[1]),
            NlAttrStr(3, "test string"),
        ]
        self.runtest(test_meta)

    @pytest.mark.skip(reason="comment me ( or delete the func) to run the test")
    def test_failed(self):
        pass

    @pytest.mark.skip(reason="comment me ( or delete the func) to run the test")
    def test_failed2(self):
        pass
