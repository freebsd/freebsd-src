# RADIUS DAS extensions to pyrad
# Copyright (c) 2014, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import hashlib
import random
import struct
import pyrad.packet

class DisconnectPacket(pyrad.packet.Packet):
    def __init__(self, code=pyrad.packet.DisconnectRequest, id=None,
                 secret=None, authenticator=None, **attributes):
        pyrad.packet.Packet.__init__(self, code, id, secret, authenticator,
                                     **attributes)

    def RequestPacket(self):
        attr = b''
        for code, datalst in sorted(self.items()):
            for data in datalst:
                attr += self._PktEncodeAttribute(code, data)

        if self.id is None:
            self.id = random.randrange(0, 256)

        header = struct.pack('!BBH', self.code, self.id, (20 + len(attr)))
        self.authenticator = hashlib.md5(header[0:4] + 16 * b'\x00' + attr
            + self.secret).digest()
        return header + self.authenticator + attr

class CoAPacket(pyrad.packet.Packet):
    def __init__(self, code=pyrad.packet.CoARequest, id=None,
                 secret=None, authenticator=None, **attributes):
        pyrad.packet.Packet.__init__(self, code, id, secret, authenticator,
                                     **attributes)

    def RequestPacket(self):
        attr = self._PktEncodeAttributes()

        if self.id is None:
            self.id = random.randrange(0, 256)

        header = struct.pack('!BBH', self.code, self.id, (20 + len(attr)))
        self.authenticator = hashlib.md5(header[0:4] + 16 * b'\x00' + attr
            + self.secret).digest()
        return header + self.authenticator + attr
