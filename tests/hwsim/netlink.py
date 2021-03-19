#
# (Generic) Netlink message generation/parsing
# Copyright (c) 2007	Johannes Berg <johannes@sipsolutions.net>
# Copyright (c) 2014	Intel Corporation
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import struct, socket

# flags
NLM_F_REQUEST = 1
NLM_F_MULTI = 2
NLM_F_ACK = 4
NLM_F_ECHO = 8

# types
NLMSG_NOOP	= 1
NLMSG_ERROR	= 2
NLMSG_DONE	= 3
NLMSG_OVERRUN	= 4
NLMSG_MIN_TYPE	= 0x10

class Attr(object):
    def __init__(self, attr_type, data, *values):
        self._type = attr_type
        if len(values):
            self._data = struct.pack(data, *values)
        else:
            self._data = data

    def _dump(self):
        hdr = struct.pack("HH", len(self._data) + 4, self._type)
        length = len(self._data)
        pad = ((length + 4 - 1) & ~3) - length
        return hdr + self._data + b'\x00' * pad

    def __repr__(self):
        return '<Attr type %d, data "%s">' % (self._type, repr(self._data))

    def u16(self):
        return struct.unpack('H', self._data)[0]
    def s16(self):
        return struct.unpack('h', self._data)[0]
    def u32(self):
        return struct.unpack('I', self._data)[0]
    def s32(self):
        return struct.unpack('i', self._data)[0]
    def str(self):
        return self._data
    def nulstr(self):
        return self._data.split('\0')[0]
    def nested(self):
        return parse_attributes(self._data)

class StrAttr(Attr):
    def __init__(self, attr_type, data):
        Attr.__init__(self, attr_type, "%ds" % len(data), data)

class NulStrAttr(Attr):
    def __init__(self, attr_type, data):
        Attr.__init__(self, attr_type, "%dsB" % len(data), data, 0)

class U32Attr(Attr):
    def __init__(self, attr_type, val):
        Attr.__init__(self, attr_type, "I", val)

class U8Attr(Attr):
    def __init__(self, attr_type, val):
        Attr.__init__(self, attr_type, "B", val)

class FlagAttr(Attr):
    def __init__(self, attr_type):
        Attr.__init__(self, attr_type, b"")

class Nested(Attr):
    def __init__(self, attr_type, attrs):
        self.attrs = attrs
        self.type = attr_type

    def _dump(self):
        contents = []
        for attr in self.attrs:
            contents.append(attr._dump())
        contents = ''.join(contents)
        length = len(contents)
        hdr = struct.pack("HH", length+4, self.type)
        return hdr + contents

NETLINK_ROUTE = 0
NETLINK_UNUSED = 1
NETLINK_USERSOCK = 2
NETLINK_FIREWALL = 3
NETLINK_INET_DIAG = 4
NETLINK_NFLOG = 5
NETLINK_XFRM = 6
NETLINK_SELINUX = 7
NETLINK_ISCSI = 8
NETLINK_AUDIT = 9
NETLINK_FIB_LOOKUP = 10
NETLINK_CONNECTOR = 11
NETLINK_NETFILTER = 12
NETLINK_IP6_FW = 13
NETLINK_DNRTMSG = 14
NETLINK_KOBJECT_UEVENT = 15
NETLINK_GENERIC = 16

class Message(object):
    def __init__(self, msg_type, flags=0, seq=-1, payload=None):
        self.type = msg_type
        self.flags = flags
        self.seq = seq
        self.pid = -1
        payload = payload or []
        if isinstance(payload, list):
            self.payload = bytes()
            for attr in payload:
                self.payload += attr._dump()
        else:
            self.payload = payload

    def send(self, conn):
        if self.seq == -1:
            self.seq = conn.seq()

        self.pid = conn.pid
        length = len(self.payload)

        hdr = struct.pack("IHHII", length + 4*4, self.type,
                          self.flags, self.seq, self.pid)
        conn.send(hdr + self.payload)

    def __repr__(self):
        return '<netlink.Message type=%d, pid=%d, seq=%d, flags=0x%x "%s">' % (
            self.type, self.pid, self.seq, self.flags, repr(self.payload))

    @property
    def ret(self):
        assert self.type == NLMSG_ERROR
        return struct.unpack("i", self.payload[:4])[0]

    def send_and_recv(self, conn):
        self.send(conn)
        while True:
            m = conn.recv()
            if m.seq == self.seq:
                return m

class Connection(object):
    def __init__(self, nltype, groups=0, unexpected_msg_handler=None):
        self.descriptor = socket.socket(socket.AF_NETLINK,
                                        socket.SOCK_RAW, nltype)
        self.descriptor.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 65536)
        self.descriptor.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 65536)
        self.descriptor.bind((0, groups))
        self.pid, self.groups = self.descriptor.getsockname()
        self._seq = 0
        self.unexpected = unexpected_msg_handler
    def send(self, msg):
        self.descriptor.send(msg)
    def recv(self):
        contents = self.descriptor.recv(16384)
        # XXX: python doesn't give us message flags, check
        #      len(contents) vs. msglen for TRUNC
        msglen, msg_type, flags, seq, pid = struct.unpack("IHHII",
                                                          contents[:16])
        msg = Message(msg_type, flags, seq, contents[16:])
        msg.pid = pid
        if msg.type == NLMSG_ERROR:
            import os
            errno = msg.ret
            if errno < 0:
                err = OSError("Netlink error: %s (%d)" % (
                    os.strerror(-errno), -errno))
                err.errno = -errno
                raise err
        return msg
    def seq(self):
        self._seq += 1
        return self._seq

def parse_attributes(data):
    attrs = {}
    while len(data):
        attr_len, attr_type = struct.unpack("HH", data[:4])
        attrs[attr_type] = Attr(attr_type, data[4:attr_len])
        attr_len = ((attr_len + 4 - 1) & ~3)
        data = data[attr_len:]
    return attrs



CTRL_CMD_UNSPEC = 0
CTRL_CMD_NEWFAMILY = 1
CTRL_CMD_DELFAMILY = 2
CTRL_CMD_GETFAMILY = 3
CTRL_CMD_NEWOPS = 4
CTRL_CMD_DELOPS = 5
CTRL_CMD_GETOPS = 6

CTRL_ATTR_UNSPEC = 0
CTRL_ATTR_FAMILY_ID = 1
CTRL_ATTR_FAMILY_NAME = 2
CTRL_ATTR_VERSION = 3
CTRL_ATTR_HDRSIZE = 4
CTRL_ATTR_MAXATTR = 5
CTRL_ATTR_OPS = 6

class GenlHdr(object):
    def __init__(self, cmd, version=0):
        self.cmd = cmd
        self.version = version
    def _dump(self):
        return struct.pack("BBxx", self.cmd, self.version)

def _genl_hdr_parse(data):
    return GenlHdr(*struct.unpack("BBxx", data))

GENL_ID_CTRL = NLMSG_MIN_TYPE

class GenlMessage(Message):
    def __init__(self, family, cmd, attrs=[], flags=0):
        Message.__init__(self, family, flags=flags, payload=[GenlHdr(cmd)] + attrs)

class GenlController(object):
    def __init__(self, conn):
        self.conn = conn
    def get_family_id(self, family):
        a = NulStrAttr(CTRL_ATTR_FAMILY_NAME, family)
        m = GenlMessage(GENL_ID_CTRL, CTRL_CMD_GETFAMILY, flags=NLM_F_REQUEST, attrs=[a])
        m.send(self.conn)
        m = self.conn.recv()
        gh = _genl_hdr_parse(m.payload[:4])
        attrs = parse_attributes(m.payload[4:])
        return attrs[CTRL_ATTR_FAMILY_ID].u16()

genl_controller = GenlController(Connection(NETLINK_GENERIC))
