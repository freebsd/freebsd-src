import logging
import time
from typing import NamedTuple

import pytest
from atf_python.sys.netlink.attrs import NlAttrNested
from atf_python.sys.netlink.attrs import NlAttrStr
from atf_python.sys.netlink.netlink import NetlinkMultipartIterator
from atf_python.sys.netlink.netlink import NlHelper
from atf_python.sys.netlink.netlink import Nlsock
from atf_python.sys.netlink.netlink_generic import KtestAttrType
from atf_python.sys.netlink.netlink_generic import KtestInfoMessage
from atf_python.sys.netlink.netlink_generic import KtestLogMsgType
from atf_python.sys.netlink.netlink_generic import KtestMsgAttrType
from atf_python.sys.netlink.netlink_generic import KtestMsgType
from atf_python.sys.netlink.netlink_generic import timespec
from atf_python.sys.netlink.utils import NlConst
from atf_python.utils import BaseTest
from atf_python.utils import libc
from atf_python.utils import nodeid_to_method_name


datefmt = "%H:%M:%S"
fmt = "%(asctime)s.%(msecs)03d %(filename)s:%(funcName)s:%(lineno)d %(message)s"
logging.basicConfig(level=logging.DEBUG, format=fmt, datefmt=datefmt)
logger = logging.getLogger("ktest")


NETLINK_FAMILY = "ktest"


class KtestItem(pytest.Item):
    def __init__(self, *, descr, kcls, **kwargs):
        super().__init__(**kwargs)
        self.descr = descr
        self._kcls = kcls

    def runtest(self):
        self._kcls().runtest()


class KtestCollector(pytest.Class):
    def collect(self):
        obj = self.obj
        exclude_names = set([n for n in dir(obj) if not n.startswith("_")])

        autoload = obj.KTEST_MODULE_AUTOLOAD
        module_name = obj.KTEST_MODULE_NAME
        loader = KtestLoader(module_name, autoload)
        ktests = loader.load_ktests()
        if not ktests:
            return

        orig = pytest.Class.from_parent(self.parent, name=self.name, obj=obj)
        for py_test in orig.collect():
            yield py_test

        for ktest in ktests:
            name = ktest["name"]
            descr = ktest["desc"]
            if name in exclude_names:
                continue
            yield KtestItem.from_parent(self, name=name, descr=descr, kcls=obj)


class KtestLoader(object):
    def __init__(self, module_name: str, autoload: bool):
        self.module_name = module_name
        self.autoload = autoload
        self.helper = NlHelper()
        self.nlsock = Nlsock(NlConst.NETLINK_GENERIC, self.helper)
        self.family_id = self._get_family_id()

    def _get_family_id(self):
        try:
            family_id = self.nlsock.get_genl_family_id(NETLINK_FAMILY)
        except ValueError:
            if self.autoload:
                libc.kldload(self.module_name)
                family_id = self.nlsock.get_genl_family_id(NETLINK_FAMILY)
            else:
                raise
        return family_id

    def _load_ktests(self):
        msg = KtestInfoMessage(self.helper, self.family_id, KtestMsgType.KTEST_CMD_LIST)
        msg.set_request()
        msg.add_nla(NlAttrStr(KtestAttrType.KTEST_ATTR_MOD_NAME, self.module_name))
        self.nlsock.write_message(msg, verbose=False)
        nlmsg_seq = msg.nl_hdr.nlmsg_seq

        ret = []
        for rx_msg in NetlinkMultipartIterator(self.nlsock, nlmsg_seq, self.family_id):
            # rx_msg.print_message()
            tst = {
                "mod_name": rx_msg.get_nla(KtestAttrType.KTEST_ATTR_MOD_NAME).text,
                "name": rx_msg.get_nla(KtestAttrType.KTEST_ATTR_TEST_NAME).text,
                "desc": rx_msg.get_nla(KtestAttrType.KTEST_ATTR_TEST_DESCR).text,
            }
            ret.append(tst)
        return ret

    def load_ktests(self):
        ret = self._load_ktests()
        if not ret and self.autoload:
            libc.kldload(self.module_name)
            ret = self._load_ktests()
        return ret


def generate_ktests(collector, name, obj):
    if getattr(obj, "KTEST_MODULE_NAME", None) is not None:
        return KtestCollector.from_parent(collector, name=name, obj=obj)
    return None


class BaseKernelTest(BaseTest):
    KTEST_MODULE_AUTOLOAD = True
    KTEST_MODULE_NAME = None

    def _get_record_time(self, msg) -> float:
        timespec = msg.get_nla(KtestMsgAttrType.KTEST_MSG_ATTR_TS).ts
        epoch_ktime = timespec.tv_sec * 1.0 + timespec.tv_nsec * 1.0 / 1000000000
        if not hasattr(self, "_start_epoch"):
            self._start_ktime = epoch_ktime
            self._start_time = time.time()
            epoch_time = self._start_time
        else:
            epoch_time = time.time() - self._start_time + epoch_ktime
        return epoch_time

    def _log_message(self, msg):
        # Convert syslog-type l
        syslog_level = msg.get_nla(KtestMsgAttrType.KTEST_MSG_ATTR_LEVEL).u8
        if syslog_level <= 6:
            loglevel = logging.INFO
        else:
            loglevel = logging.DEBUG
        rec = logging.LogRecord(
            self.KTEST_MODULE_NAME,
            loglevel,
            msg.get_nla(KtestMsgAttrType.KTEST_MSG_ATTR_FILE).text,
            msg.get_nla(KtestMsgAttrType.KTEST_MSG_ATTR_LINE).u32,
            "%s",
            (msg.get_nla(KtestMsgAttrType.KTEST_MSG_ATTR_TEXT).text),
            None,
            msg.get_nla(KtestMsgAttrType.KTEST_MSG_ATTR_FUNC).text,
            None,
        )
        rec.created = self._get_record_time(msg)
        logger.handle(rec)

    def _runtest_name(self, test_name: str, test_data):
        module_name = self.KTEST_MODULE_NAME
        # print("Running kernel test {} for module {}".format(test_name, module_name))
        helper = NlHelper()
        nlsock = Nlsock(NlConst.NETLINK_GENERIC, helper)
        family_id = nlsock.get_genl_family_id(NETLINK_FAMILY)
        msg = KtestInfoMessage(helper, family_id, KtestMsgType.KTEST_CMD_RUN)
        msg.set_request()
        msg.add_nla(NlAttrStr(KtestAttrType.KTEST_ATTR_MOD_NAME, module_name))
        msg.add_nla(NlAttrStr(KtestAttrType.KTEST_ATTR_TEST_NAME, test_name))
        if test_data is not None:
            msg.add_nla(NlAttrNested(KtestAttrType.KTEST_ATTR_TEST_META, test_data))
        nlsock.write_message(msg, verbose=False)

        for log_msg in NetlinkMultipartIterator(
            nlsock, msg.nl_hdr.nlmsg_seq, family_id
        ):
            self._log_message(log_msg)

    def runtest(self, test_data=None):
        self._runtest_name(nodeid_to_method_name(self.test_id), test_data)
