#!/usr/local/bin/python3
from enum import Enum
from typing import Any
from typing import Dict
from typing import List
from typing import NamedTuple


class NlConst:
    AF_NETLINK = 38
    NETLINK_ROUTE = 0
    NETLINK_GENERIC = 16
    GENL_ID_CTRL = 16


def roundup2(val: int, num: int) -> int:
    if val % num:
        return (val | (num - 1)) + 1
    else:
        return val


def align4(val: int) -> int:
    return roundup2(val, 4)


def enum_or_int(val) -> int:
    if isinstance(val, Enum):
        return val.value
    return val


class AttrDescr(NamedTuple):
    val: Enum
    cls: "NlAttr"
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


def build_propmap(cls):
    ret = {}
    for prop in dir(cls):
        if not prop.startswith("_"):
            ret[getattr(cls, prop).value] = prop
    return ret


def get_bitmask_map(propmap, val):
    v = 1
    ret = {}
    while val:
        if v & val:
            if v in propmap:
                ret[v] = propmap[v]
            else:
                ret[v] = hex(v)
            val -= v
        v *= 2
    return ret


def get_bitmask_str(cls, val):
    if isinstance(cls, type):
        pmap = build_propmap(cls)
    else:
        pmap = {}
        for _cls in cls:
            pmap.update(build_propmap(_cls))
    bmap = get_bitmask_map(pmap, val)
    return ",".join([v for k, v in bmap.items()])
