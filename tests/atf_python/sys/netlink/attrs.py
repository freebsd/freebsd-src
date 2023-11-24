import socket
import struct
from enum import Enum

from atf_python.sys.netlink.utils import align4
from atf_python.sys.netlink.utils import enum_or_int


class NlAttr(object):
    HDR_LEN = 4  # sizeof(struct nlattr)

    def __init__(self, nla_type, data):
        if isinstance(nla_type, Enum):
            self._nla_type = nla_type.value
            self._enum = nla_type
        else:
            self._nla_type = nla_type
            self._enum = None
        self.nla_list = []
        self._data = data

    @property
    def nla_type(self):
        return self._nla_type & 0x3FFF

    @property
    def nla_len(self):
        return len(self._data) + 4

    def add_nla(self, nla):
        self.nla_list.append(nla)

    def print_attr(self, prepend=""):
        if self._enum is not None:
            type_str = self._enum.name
        else:
            type_str = "nla#{}".format(self.nla_type)
        print(
            "{}len={} type={}({}){}".format(
                prepend, self.nla_len, type_str, self.nla_type, self._print_attr_value()
            )
        )

    @staticmethod
    def _validate(data):
        if len(data) < 4:
            raise ValueError("attribute too short")
        nla_len, nla_type = struct.unpack("@HH", data[:4])
        if nla_len > len(data):
            raise ValueError("attribute length too big")
        if nla_len < 4:
            raise ValueError("attribute length too short")

    @classmethod
    def _parse(cls, data):
        nla_len, nla_type = struct.unpack("@HH", data[:4])
        return cls(nla_type, data[4:])

    @classmethod
    def from_bytes(cls, data, attr_type_enum=None):
        cls._validate(data)
        attr = cls._parse(data)
        attr._enum = attr_type_enum
        return attr

    def _to_bytes(self, data: bytes):
        ret = data
        if align4(len(ret)) != len(ret):
            ret = data + bytes(align4(len(ret)) - len(ret))
        return struct.pack("@HH", len(data) + 4, self._nla_type) + ret

    def __bytes__(self):
        return self._to_bytes(self._data)

    def _print_attr_value(self):
        return " " + " ".join(["x{:02X}".format(b) for b in self._data])


class NlAttrNested(NlAttr):
    def __init__(self, nla_type, val):
        super().__init__(nla_type, b"")
        self.nla_list = val

    def get_nla(self, nla_type):
        nla_type_raw = enum_or_int(nla_type)
        for nla in self.nla_list:
            if nla.nla_type == nla_type_raw:
                return nla
        return None

    @property
    def nla_len(self):
        return align4(len(b"".join([bytes(nla) for nla in self.nla_list]))) + 4

    def print_attr(self, prepend=""):
        if self._enum is not None:
            type_str = self._enum.name
        else:
            type_str = "nla#{}".format(self.nla_type)
        print(
            "{}len={} type={}({}) {{".format(
                prepend, self.nla_len, type_str, self.nla_type
            )
        )
        for nla in self.nla_list:
            nla.print_attr(prepend + "  ")
        print("{}}}".format(prepend))

    def __bytes__(self):
        return self._to_bytes(b"".join([bytes(nla) for nla in self.nla_list]))


class NlAttrU32(NlAttr):
    def __init__(self, nla_type, val):
        self.u32 = enum_or_int(val)
        super().__init__(nla_type, b"")

    @property
    def nla_len(self):
        return 8

    def _print_attr_value(self):
        return " val={}".format(self.u32)

    @staticmethod
    def _validate(data):
        assert len(data) == 8
        nla_len, nla_type = struct.unpack("@HH", data[:4])
        assert nla_len == 8

    @classmethod
    def _parse(cls, data):
        nla_len, nla_type, val = struct.unpack("@HHI", data)
        return cls(nla_type, val)

    def __bytes__(self):
        return self._to_bytes(struct.pack("@I", self.u32))


class NlAttrS32(NlAttr):
    def __init__(self, nla_type, val):
        self.s32 = enum_or_int(val)
        super().__init__(nla_type, b"")

    @property
    def nla_len(self):
        return 8

    def _print_attr_value(self):
        return " val={}".format(self.s32)

    @staticmethod
    def _validate(data):
        assert len(data) == 8
        nla_len, nla_type = struct.unpack("@HH", data[:4])
        assert nla_len == 8

    @classmethod
    def _parse(cls, data):
        nla_len, nla_type, val = struct.unpack("@HHi", data)
        return cls(nla_type, val)

    def __bytes__(self):
        return self._to_bytes(struct.pack("@i", self.s32))


class NlAttrU16(NlAttr):
    def __init__(self, nla_type, val):
        self.u16 = enum_or_int(val)
        super().__init__(nla_type, b"")

    @property
    def nla_len(self):
        return 6

    def _print_attr_value(self):
        return " val={}".format(self.u16)

    @staticmethod
    def _validate(data):
        assert len(data) == 6
        nla_len, nla_type = struct.unpack("@HH", data[:4])
        assert nla_len == 6

    @classmethod
    def _parse(cls, data):
        nla_len, nla_type, val = struct.unpack("@HHH", data)
        return cls(nla_type, val)

    def __bytes__(self):
        return self._to_bytes(struct.pack("@H", self.u16))


class NlAttrU8(NlAttr):
    def __init__(self, nla_type, val):
        self.u8 = enum_or_int(val)
        super().__init__(nla_type, b"")

    @property
    def nla_len(self):
        return 5

    def _print_attr_value(self):
        return " val={}".format(self.u8)

    @staticmethod
    def _validate(data):
        assert len(data) == 5
        nla_len, nla_type = struct.unpack("@HH", data[:4])
        assert nla_len == 5

    @classmethod
    def _parse(cls, data):
        nla_len, nla_type, val = struct.unpack("@HHB", data)
        return cls(nla_type, val)

    def __bytes__(self):
        return self._to_bytes(struct.pack("@B", self.u8))


class NlAttrIp(NlAttr):
    def __init__(self, nla_type, addr: str):
        super().__init__(nla_type, b"")
        self.addr = addr
        if ":" in self.addr:
            self.family = socket.AF_INET6
        else:
            self.family = socket.AF_INET

    @staticmethod
    def _validate(data):
        nla_len, nla_type = struct.unpack("@HH", data[:4])
        data_len = nla_len - 4
        if data_len != 4 and data_len != 16:
            raise ValueError(
                "Error validating attr {}: nla_len is not valid".format(  # noqa: E501
                    nla_type
                )
            )

    @property
    def nla_len(self):
        if self.family == socket.AF_INET6:
            return 20
        else:
            return 8
        return align4(len(self._data)) + 4

    @classmethod
    def _parse(cls, data):
        nla_len, nla_type = struct.unpack("@HH", data[:4])
        data_len = len(data) - 4
        if data_len == 4:
            addr = socket.inet_ntop(socket.AF_INET, data[4:8])
        else:
            addr = socket.inet_ntop(socket.AF_INET6, data[4:20])
        return cls(nla_type, addr)

    def __bytes__(self):
        return self._to_bytes(socket.inet_pton(self.family, self.addr))

    def _print_attr_value(self):
        return " addr={}".format(self.addr)


class NlAttrIp4(NlAttrIp):
    def __init__(self, nla_type, addr: str):
        super().__init__(nla_type, addr)
        assert self.family == socket.AF_INET


class NlAttrIp6(NlAttrIp):
    def __init__(self, nla_type, addr: str):
        super().__init__(nla_type, addr)
        assert self.family == socket.AF_INET6


class NlAttrStr(NlAttr):
    def __init__(self, nla_type, text):
        super().__init__(nla_type, b"")
        self.text = text

    @staticmethod
    def _validate(data):
        NlAttr._validate(data)
        try:
            data[4:].decode("utf-8")
        except Exception as e:
            raise ValueError("wrong utf-8 string: {}".format(e))

    @property
    def nla_len(self):
        return len(self.text) + 5

    @classmethod
    def _parse(cls, data):
        text = data[4:-1].decode("utf-8")
        nla_len, nla_type = struct.unpack("@HH", data[:4])
        return cls(nla_type, text)

    def __bytes__(self):
        return self._to_bytes(bytes(self.text, encoding="utf-8") + bytes(1))

    def _print_attr_value(self):
        return ' val="{}"'.format(self.text)


class NlAttrStrn(NlAttr):
    def __init__(self, nla_type, text):
        super().__init__(nla_type, b"")
        self.text = text

    @staticmethod
    def _validate(data):
        NlAttr._validate(data)
        try:
            data[4:].decode("utf-8")
        except Exception as e:
            raise ValueError("wrong utf-8 string: {}".format(e))

    @property
    def nla_len(self):
        return len(self.text) + 4

    @classmethod
    def _parse(cls, data):
        text = data[4:].decode("utf-8")
        nla_len, nla_type = struct.unpack("@HH", data[:4])
        return cls(nla_type, text)

    def __bytes__(self):
        return self._to_bytes(bytes(self.text, encoding="utf-8"))

    def _print_attr_value(self):
        return ' val="{}"'.format(self.text)
