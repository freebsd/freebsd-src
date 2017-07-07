#!/usr/bin/python
#
# Copyright 2013 Red Hat, Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#    1. Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#    2. Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
# IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
# OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import StringIO
import os
import sys
import signal

try:
    from pyrad import dictionary, packet, server
except ImportError:
    sys.stderr.write("pyrad not found!\n")
    sys.exit(0)

# We could use a dictionary file, but since we need
# such few attributes, we'll just include them here
DICTIONARY = """
ATTRIBUTE\tUser-Name\t1\tstring
ATTRIBUTE\tUser-Password\t2\toctets
ATTRIBUTE\tNAS-Identifier\t32\tstring
"""

class TestServer(server.Server):
    def _HandleAuthPacket(self, pkt):
        server.Server._HandleAuthPacket(self, pkt)

        passwd = []

        for key in pkt.keys():
            if key == "User-Password":
                passwd = map(pkt.PwDecrypt, pkt[key])

        reply = self.CreateReplyPacket(pkt)
        if passwd == ['accept']:
            reply.code = packet.AccessAccept
        else:
            reply.code = packet.AccessReject
        self.SendReplyPacket(pkt.fd, reply)

srv = TestServer(addresses=["localhost"],
                 hosts={"127.0.0.1":
                        server.RemoteHost("127.0.0.1", "foo", "localhost")},
                 dict=dictionary.Dictionary(StringIO.StringIO(DICTIONARY)))

# Write a sentinel character to let the parent process know we're listening.
sys.stdout.write("~")
sys.stdout.flush()

srv.Run()
