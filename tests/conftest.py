import pytest
from atf_python.atf_pytest import ATFHandler
from typing import Dict


PLUGIN_ENABLED = False
DEFAULT_HANDLER = None


def set_handler(config):
    global DEFAULT_HANDLER, PLUGIN_ENABLED
    DEFAULT_HANDLER = ATFHandler(report_file_name=config.option.atf_file)
    PLUGIN_ENABLED = True
    return DEFAULT_HANDLER


def get_handler():
    return DEFAULT_HANDLER


def pytest_addoption(parser):
    """Add file output"""
    # Add meta-values
    group = parser.getgroup("general", "Running and selection options")
    group.addoption(
        "--atf-source-dir",
        type=str,
        dest="atf_source_dir",
        help="Path to the test source directory",
    )
    group.addoption(
        "--atf-cleanup",
        default=False,
        action="store_true",
        dest="atf_cleanup",
        help="Call cleanup procedure for a given test",
    )
    group = parser.getgroup("terminal reporting", "reporting", after="general")
    group.addoption(
        "--atf",
        default=False,
        action="store_true",
        help="Enable test listing/results output in atf format",
    )
    group.addoption(
        "--atf-file",
        type=str,
        dest="atf_file",
        help="Path to the status file provided by atf runtime",
    )


@pytest.fixture(autouse=True, scope="session")
def atf_vars() -> Dict[str, str]:
    return ATFHandler.get_atf_vars()


@pytest.hookimpl(trylast=True)
def pytest_configure(config):
    if config.option.help:
        return

    # Register markings anyway to avoid warnings
    config.addinivalue_line("markers", "require_user(name): user to run the test with")
    config.addinivalue_line(
        "markers", "require_arch(names): List[str] of support archs"
    )
    # config.addinivalue_line("markers", "require_config(config): List[Tuple[str,Any]] of k=v pairs")
    config.addinivalue_line(
        "markers", "require_diskspace(amount): str with required diskspace"
    )
    config.addinivalue_line(
        "markers", "require_files(space): List[str] with file paths"
    )
    config.addinivalue_line(
        "markers", "require_machine(names): List[str] of support machine types"
    )
    config.addinivalue_line(
        "markers", "require_memory(amount): str with required memory"
    )
    config.addinivalue_line(
        "markers", "require_progs(space): List[str] with file paths"
    )
    config.addinivalue_line(
        "markers", "timeout(dur): int/float with max duration in sec"
    )

    if not config.option.atf:
        return
    handler = set_handler(config)

    if config.option.collectonly:
        # Need to output list of tests to stdout, hence override
        # standard reporter plugin
        reporter = config.pluginmanager.getplugin("terminalreporter")
        if reporter:
            config.pluginmanager.unregister(reporter)
    else:
        handler.setup_configure()


def pytest_collection_modifyitems(session, config, items):
    """If cleanup is requested, replace collected tests with their cleanups (if any)"""
    if PLUGIN_ENABLED and config.option.atf_cleanup:
        new_items = []
        handler = get_handler()
        for obj in items:
            if handler.has_object_cleanup(obj):
                handler.override_runtest(obj)
                new_items.append(obj)
        items.clear()
        items.extend(new_items)


def pytest_collection_finish(session):
    if PLUGIN_ENABLED and session.config.option.collectonly:
        handler = get_handler()
        handler.list_tests(session.items)


def pytest_runtest_setup(item):
    if PLUGIN_ENABLED:
        handler = get_handler()
        handler.setup_method_pre(item)


def pytest_runtest_logreport(report):
    if PLUGIN_ENABLED:
        handler = get_handler()
        handler.add_report(report)


def pytest_unconfigure(config):
    if PLUGIN_ENABLED and config.option.atf_file:
        handler = get_handler()
        handler.write_report()
