import os
from pathlib import Path

import pytest


def _required_loader_path(env_name):
    value = os.environ.get(env_name)
    if not value:
        pytest.fail(
            f"{env_name} is not set; run `make test` from "
            "stand/kboot/tests"
        )

    path = Path(value)
    if not path.is_file():
        pytest.fail(f"{env_name} does not name a loader binary: {path}")

    return path


@pytest.fixture(scope="session")
def loader_kboot_bin():
    return _required_loader_path("KBOOT_LOADER")


@pytest.fixture(scope="session")
def freebsd_src():
    return Path(
        os.environ.get("FREEBSD_SRC", Path(__file__).resolve().parents[3])
    )
