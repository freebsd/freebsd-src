#!/usr/bin/env python3

import os
import socket
import struct
import subprocess
import sys
from enum import Enum
from typing import Dict
from typing import List
from typing import Optional
from typing import Union
from typing import Any
from typing import NamedTuple
import pytest


def roundup2(val: int, num: int) -> int:
    if val % num:
        return (val | (num - 1)) + 1
    else:
        return val


def align8(val: int) -> int:
    return roundup2(val, 8)


def enum_or_int(val) -> int:
    if isinstance(val, Enum):
        return val.value
    return val


def enum_from_int(enum_class: Enum, val) -> Enum:
    if isinstance(val, Enum):
        return val
    for item in enum_class:
        if val == item.value:
            return item
    return None


class AttrDescr(NamedTuple):
    val: Enum
    cls: Any
    child_map: Any = None
    is_array: bool = False


def prepare_attrs_map(attrs: List[AttrDescr]) -> Dict[str, Dict]:
    ret = {}
    for ad in attrs:
        ret[ad.val.value] = {"ad": ad}
        if ad.child_map:
            ret[ad.val.value]["child"] = prepare_attrs_map(ad.child_map)
            ret[ad.val.value]["is_array"] = ad.is_array
    return ret



