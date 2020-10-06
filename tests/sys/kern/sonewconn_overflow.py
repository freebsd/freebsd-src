#!/usr/bin/env python
#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Netflix, Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$
#

import socket
import os
import sys
from subprocess import check_output
from time import sleep

V4HOST = '127.0.0.1'
V6HOST = '::1'
TCPPORT = 65432
UNIXSOCK = '/tmp/testsock'
TYPE = socket.SOCK_STREAM

class GenericTest(object):
    def __init__(self):
        raise NotImplementedError("Subclass must override the __init__ method")
    def setup(self, af, addr):
        self.sockets = []
        self.ls = None
        self.ls = socket.socket(af, TYPE)
        self.ls.bind(addr)
        self.ls.listen(2)
        self.af = af
        self.addr = addr
    def doTest(self, cnt):
        rv = 0
        for i in range(0, cnt):
            try:
                s = socket.socket(self.af, TYPE)
                s.connect(self.addr)
            except:
                continue
            self.sockets.append(s)
            rv += 1
        return rv
    def __del__(self):
        for s in self.sockets:
            s.close()
        if self.ls is not None:
            self.ls.close()

class IPv4Test(GenericTest):
    def __init__(self):
        super(IPv4Test, self).setup(socket.AF_INET, (V4HOST, TCPPORT))

class IPv6Test(GenericTest):
    def __init__(self):
        super(IPv6Test, self).setup(socket.AF_INET6, (V6HOST, TCPPORT))

class UnixTest(GenericTest):
    def __init__(self):
        super(UnixTest, self).setup(socket.AF_UNIX, UNIXSOCK)
    def __del__(self):
        super(UnixTest, self).__del__()
        os.remove(UNIXSOCK)

class LogChecker():
    def __init__(self):
        # Clear the dmesg buffer to prevent rotating causes issues
        os.system('/sbin/dmesg -c > /dev/null')
        # Figure out how big the dmesg buffer is.
        self.dmesgOff = len(check_output("/sbin/dmesg"))

    def checkForMsg(self, expected):
        newOff = self.dmesgOff
        for i in range(0, 3):
            dmesg = check_output("/sbin/dmesg")
            newOff = len(dmesg)
            if newOff >= self.dmesgOff:
                dmesg = dmesg[self.dmesgOff:]
            for line in dmesg.splitlines():
                try:
                    if str(line).find(expected) >= 0:
                        self.dmesgOff = newOff
                        return True
                except:
                    pass
            sleep(0.5)
        self.dmesgOff = newOff
        return False

def main():
    ip4 = IPv4Test()
    ip6 = IPv6Test()
    lcl = UnixTest()
    lc = LogChecker()
    failure = False

    STDLOGMSG = "Listen queue overflow: 4 already in queue awaiting acceptance (1 occurrences)"

    V4LOGMSG = "(%s:%d (proto 6)): %s" % (V4HOST, TCPPORT, STDLOGMSG)
    ip4.doTest(5)
    if not lc.checkForMsg(V4LOGMSG):
        failure = True
        sys.stderr.write("IPv4 log message not seen\n")
    else:
        ip4.doTest(1)
        if lc.checkForMsg(V4LOGMSG):
            failure = True
            sys.stderr.write("Subsequent IPv4 log message not suppressed\n")

    V6LOGMSG = "([%s]:%d (proto 6)): %s" % (V6HOST, TCPPORT, STDLOGMSG)
    ip6.doTest(5)
    if not lc.checkForMsg(V6LOGMSG):
        failure = True
        sys.stderr.write("IPv6 log message not seen\n")
    else:
        ip6.doTest(1)
        if lc.checkForMsg(V6LOGMSG):
            failure = True
            sys.stderr.write("Subsequent IPv6 log message not suppressed\n")

    UNIXLOGMSG = "(local:%s): %s" % (UNIXSOCK, STDLOGMSG)
    lcl.doTest(5)
    if not lc.checkForMsg(UNIXLOGMSG):
        failure = True
        sys.stderr.write("Unix socket log message not seen\n")
    else:
        lcl.doTest(1)
        if lc.checkForMsg(UNIXLOGMSG):
            failure = True
            sys.stderr.write("Subsequent Unix socket log message not suppressed\n")

    if failure:
        sys.exit(1)
    sys.exit(0)

if __name__ == '__main__':
    main()
