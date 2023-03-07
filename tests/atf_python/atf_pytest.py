import types
from typing import Any
from typing import Dict
from typing import List
from typing import NamedTuple
from typing import Optional
from typing import Tuple

import pytest
import os


def nodeid_to_method_name(nodeid: str) -> str:
    """file_name.py::ClassName::method_name[parametrize] -> method_name"""
    return nodeid.split("::")[-1].split("[")[0]


class ATFCleanupItem(pytest.Item):
    def runtest(self):
        """Runs cleanup procedure for the test instead of the test itself"""
        instance = self.parent.cls()
        cleanup_name = "cleanup_{}".format(nodeid_to_method_name(self.nodeid))
        if hasattr(instance, cleanup_name):
            cleanup = getattr(instance, cleanup_name)
            cleanup(self.nodeid)
        elif hasattr(instance, "cleanup"):
            instance.cleanup(self.nodeid)

    def setup_method_noop(self, method):
        """Overrides runtest setup method"""
        pass

    def teardown_method_noop(self, method):
        """Overrides runtest teardown method"""
        pass


class ATFTestObj(object):
    def __init__(self, obj, has_cleanup):
        # Use nodeid without name to properly name class-derived tests
        self.ident = obj.nodeid.split("::", 1)[1]
        self.description = self._get_test_description(obj)
        self.has_cleanup = has_cleanup
        self.obj = obj

    def _get_test_description(self, obj):
        """Returns first non-empty line from func docstring or func name"""
        docstr = obj.function.__doc__
        if docstr:
            for line in docstr.split("\n"):
                if line:
                    return line
        return obj.name

    @staticmethod
    def _convert_user_mark(mark, obj, ret: Dict):
        username = mark.args[0]
        if username == "unprivileged":
            # Special unprivileged user requested.
            # First, require the unprivileged-user config option presence
            key = "require.config"
            if key not in ret:
                ret[key] = "unprivileged_user"
            else:
                ret[key] = "{} {}".format(ret[key], "unprivileged_user")
        # Check if the framework requires root
        test_cls = ATFHandler.get_test_class(obj)
        if test_cls and getattr(test_cls, "NEED_ROOT", False):
            # Yes, so we ask kyua to run us under root instead
            # It is up to the implementation to switch back to the desired
            # user
            ret["require.user"] = "root"
        else:
            ret["require.user"] = username


    def _convert_marks(self, obj) -> Dict[str, Any]:
        wj_func = lambda x: " ".join(x)  # noqa: E731
        _map: Dict[str, Dict] = {
            "require_user": {"handler": self._convert_user_mark},
            "require_arch": {"name": "require.arch", "fmt": wj_func},
            "require_diskspace": {"name": "require.diskspace"},
            "require_files": {"name": "require.files", "fmt": wj_func},
            "require_machine": {"name": "require.machine", "fmt": wj_func},
            "require_memory": {"name": "require.memory"},
            "require_progs": {"name": "require.progs", "fmt": wj_func},
            "timeout": {},
        }
        ret = {}
        for mark in obj.iter_markers():
            if mark.name in _map:
                if "handler" in _map[mark.name]:
                    _map[mark.name]["handler"](mark, obj, ret)
                    continue
                name = _map[mark.name].get("name", mark.name)
                if "fmt" in _map[mark.name]:
                    val = _map[mark.name]["fmt"](mark.args[0])
                else:
                    val = mark.args[0]
                ret[name] = val
        return ret

    def as_lines(self) -> List[str]:
        """Output test definition in ATF-specific format"""
        ret = []
        ret.append("ident: {}".format(self.ident))
        ret.append("descr: {}".format(self._get_test_description(self.obj)))
        if self.has_cleanup:
            ret.append("has.cleanup: true")
        for key, value in self._convert_marks(self.obj).items():
            ret.append("{}: {}".format(key, value))
        return ret


class ATFHandler(object):
    class ReportState(NamedTuple):
        state: str
        reason: str

    def __init__(self, report_file_name: Optional[str]):
        self._tests_state_map: Dict[str, ReportStatus] = {}
        self._report_file_name = report_file_name
        self._report_file_handle = None

    def setup_configure(self):
        fname = self._report_file_name
        if fname:
            self._report_file_handle = open(fname, mode="w")

    def setup_method_pre(self, item):
        """Called before actually running the test setup_method"""
        # Check if we need to manually drop the privileges
        for mark in item.iter_markers():
            if mark.name == "require_user":
                cls = self.get_test_class(item)
                cls.TARGET_USER = mark.args[0]
                break

    def override_runtest(self, obj):
        # Override basic runtest command
        obj.runtest = types.MethodType(ATFCleanupItem.runtest, obj)
        # Override class setup/teardown
        obj.parent.cls.setup_method = ATFCleanupItem.setup_method_noop
        obj.parent.cls.teardown_method = ATFCleanupItem.teardown_method_noop

    @staticmethod
    def get_test_class(obj):
        if hasattr(obj, "parent") and obj.parent is not None:
            if hasattr(obj.parent, "cls"):
                return obj.parent.cls

    def has_object_cleanup(self, obj):
        cls = self.get_test_class(obj)
        if cls is not None:
            method_name = nodeid_to_method_name(obj.nodeid)
            cleanup_name = "cleanup_{}".format(method_name)
            if hasattr(cls, "cleanup") or hasattr(cls, cleanup_name):
                return True
        return False

    def list_tests(self, tests: List[str]):
        print('Content-Type: application/X-atf-tp; version="1"')
        print()
        for test_obj in tests:
            has_cleanup = self.has_object_cleanup(test_obj)
            atf_test = ATFTestObj(test_obj, has_cleanup)
            for line in atf_test.as_lines():
                print(line)
            print()

    def set_report_state(self, test_name: str, state: str, reason: str):
        self._tests_state_map[test_name] = self.ReportState(state, reason)

    def _extract_report_reason(self, report):
        data = report.longrepr
        if data is None:
            return None
        if isinstance(data, Tuple):
            # ('/path/to/test.py', 23, 'Skipped: unable to test')
            reason = data[2]
            for prefix in "Skipped: ":
                if reason.startswith(prefix):
                    reason = reason[len(prefix):]
            return reason
        else:
            # string/ traceback / exception report. Capture the last line
            return str(data).split("\n")[-1]
        return None

    def add_report(self, report):
        # MAP pytest report state to the atf-desired state
        #
        # ATF test states:
        # (1) expected_death, (2) expected_exit, (3) expected_failure
        # (4) expected_signal, (5) expected_timeout, (6) passed
        # (7) skipped, (8) failed
        #
        # Note that ATF don't have the concept of "soft xfail" - xpass
        # is a failure. It also calls teardown routine in a separate
        # process, thus teardown states (pytest-only) are handled as
        # body continuation.

        # (stage, state, wasxfail)

        # Just a passing test: WANT: passed
        # GOT: (setup, passed, F), (call, passed, F), (teardown, passed, F)
        #
        # Failing body test: WHAT: failed
        # GOT: (setup, passed, F), (call, failed, F), (teardown, passed, F)
        #
        # pytest.skip test decorator: WANT: skipped
        # GOT: (setup,skipped, False), (teardown, passed, False)
        #
        # pytest.skip call inside test function: WANT: skipped
        # GOT: (setup, passed, F), (call, skipped, F), (teardown,passed, F)
        #
        # mark.xfail decorator+pytest.xfail: WANT: expected_failure
        # GOT: (setup, passed, F), (call, skipped, T), (teardown, passed, F)
        #
        # mark.xfail decorator+pass: WANT: failed
        # GOT: (setup, passed, F), (call, passed, T), (teardown, passed, F)

        test_name = report.location[2]
        stage = report.when
        state = report.outcome
        reason = self._extract_report_reason(report)

        # We don't care about strict xfail - it gets translated to False

        if stage == "setup":
            if state in ("skipped", "failed"):
                # failed init -> failed test, skipped setup -> xskip
                # for the whole test
                self.set_report_state(test_name, state, reason)
        elif stage == "call":
            # "call" stage shouldn't matter if setup failed
            if test_name in self._tests_state_map:
                if self._tests_state_map[test_name].state == "failed":
                    return
            if state == "failed":
                # Record failure  & override "skipped" state
                self.set_report_state(test_name, state, reason)
            elif state == "skipped":
                if hasattr(reason, "wasxfail"):
                    # xfail() called in the test body
                    state = "expected_failure"
                else:
                    # skip inside the body
                    pass
                self.set_report_state(test_name, state, reason)
            elif state == "passed":
                if hasattr(reason, "wasxfail"):
                    # the test was expected to fail but didn't
                    # mark as hard failure
                    state = "failed"
                self.set_report_state(test_name, state, reason)
        elif stage == "teardown":
            if state == "failed":
                # teardown should be empty, as the cleanup
                # procedures should be implemented as a separate
                # function/method, so mark teardown failure as
                # global failure
                self.set_report_state(test_name, state, reason)

    def write_report(self):
        if self._report_file_handle is None:
            return
        if self._tests_state_map:
            # If we're executing in ATF mode, there has to be just one test
            # Anyway, deterministically pick the first one
            first_test_name = next(iter(self._tests_state_map))
            test = self._tests_state_map[first_test_name]
            if test.state == "passed":
                line = test.state
            else:
                line = "{}: {}".format(test.state, test.reason)
            print(line, file=self._report_file_handle)
        self._report_file_handle.close()

    @staticmethod
    def get_atf_vars() -> Dict[str, str]:
        px = "_ATF_VAR_"
        return {k[len(px):]: v for k, v in os.environ.items() if k.startswith(px)}
