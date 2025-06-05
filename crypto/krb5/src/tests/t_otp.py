# Author: Nathaniel McCallum <npmccallum@redhat.com>
#
# Copyright (c) 2013 Red Hat, Inc.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.


#
# This script tests OTP, both UDP and Unix Sockets, with a variety of
# configuration. It requires pyrad to run, but exits gracefully if not found.
# It also deliberately shuts down the test daemons between tests in order to
# test how OTP handles the case of short daemon restarts.
#

from k5test import *
from queue import Empty
import io
import struct

try:
    from pyrad import packet, dictionary
except ImportError:
    skip_rest('OTP tests', 'Python pyrad module not found')
try:
    from multiprocessing import Process, Queue
except ImportError:
    skip_rest('OTP tests', 'Python version 2.6 required')

# We could use a dictionary file, but since we need so few attributes,
# we'll just include them here.
radius_attributes = '''
ATTRIBUTE    User-Name    1    string
ATTRIBUTE    User-Password   2    octets
ATTRIBUTE    Service-Type    6    integer
ATTRIBUTE    NAS-Identifier  32    string
'''

class RadiusDaemon(Process):
    MAX_PACKET_SIZE = 4096
    DICTIONARY = dictionary.Dictionary(io.StringIO(radius_attributes))

    def listen(self, addr):
        raise NotImplementedError()

    def recvRequest(self, data):
        raise NotImplementedError()

    def run(self):
        addr = self._args[0]
        secrfile = self._args[1]
        pswd = self._args[2]
        outq = self._args[3]

        if secrfile:
            with open(secrfile, 'rb') as file:
                secr = file.read().strip()
        else:
            secr = b''

        data = self.listen(addr)
        outq.put("started")
        (buf, sock, addr) = self.recvRequest(data)
        pkt = packet.AuthPacket(secret=secr,
                                dict=RadiusDaemon.DICTIONARY,
                                packet=buf)

        usernm = []
        passwd = []
        for key in pkt.keys():
            if key == 'User-Password':
                passwd = list(map(pkt.PwDecrypt, pkt[key]))
            elif key == 'User-Name':
                usernm = pkt[key]

        reply = pkt.CreateReply()
        replyq = {'user': usernm, 'pass': passwd}
        if passwd == [pswd]:
            reply.code = packet.AccessAccept
            replyq['reply'] = True
        else:
            reply.code = packet.AccessReject
            replyq['reply'] = False

        outq.put(replyq)
        if addr is None:
            sock.send(reply.ReplyPacket())
        else:
            sock.sendto(reply.ReplyPacket(), addr)
        sock.close()

class UDPRadiusDaemon(RadiusDaemon):
    def listen(self, addr):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind((addr.split(':')[0], int(addr.split(':')[1])))
        return sock

    def recvRequest(self, sock):
        (buf, addr) = sock.recvfrom(RadiusDaemon.MAX_PACKET_SIZE)
        return (buf, sock, addr)

class UnixRadiusDaemon(RadiusDaemon):
    def listen(self, addr):
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        if os.path.exists(addr):
            os.remove(addr)
        sock.bind(addr)
        sock.listen(1)
        return (sock, addr)

    def recvRequest(self, sock_and_addr):
        sock, addr = sock_and_addr
        conn = sock.accept()[0]
        sock.close()
        os.remove(addr)

        buf = b''
        remain = RadiusDaemon.MAX_PACKET_SIZE
        while True:
            buf += conn.recv(remain)
            remain = RadiusDaemon.MAX_PACKET_SIZE - len(buf)
            if (len(buf) >= 4):
                remain = struct.unpack("!BBH", buf[0:4])[2] - len(buf)
                if (remain <= 0):
                    return (buf, conn, None)

def verify(daemon, queue, reply, usernm, passwd):
    try:
        data = queue.get(timeout=1)
    except Empty:
        sys.stderr.write("ERROR: Packet not received by daemon!\n")
        daemon.terminate()
        sys.exit(1)
    assert data['reply'] is reply
    assert data['user'] == [usernm]
    assert data['pass'] == [passwd]
    daemon.join()

# Compose a single token configuration.
def otpconfig_1(toktype, username=None, indicators=None):
    val = '{"type": "%s"' % toktype
    if username is not None:
        val += ', "username": "%s"' % username
    if indicators is not None:
        qind = ['"%s"' % s for s in indicators]
        jsonlist = '[' + ', '.join(qind) + ']'
        val += ', "indicators":' + jsonlist
    val += '}'
    return val

# Compose a token configuration list suitable for the "otp" string
# attribute.
def otpconfig(toktype, username=None, indicators=None):
    return '[' + otpconfig_1(toktype, username, indicators) + ']'

prefix = "/tmp/%d" % os.getpid()
secret_file = prefix + ".secret"
socket_file = prefix + ".socket"
with open(secret_file, "w") as file:
    file.write("otptest")
atexit.register(lambda: os.remove(secret_file))

conf = {'plugins': {'kdcpreauth': {'enable_only': 'otp'}},
        'otp': {'udp': {'server': '127.0.0.1:$port9',
                        'secret': secret_file,
                        'strip_realm': 'true',
                        'indicator': ['indotp1', 'indotp2']},
                'unix': {'server': socket_file,
                         'strip_realm': 'false'}}}

queue = Queue()

realm = K5Realm(kdc_conf=conf)
realm.run([kadminl, 'modprinc', '+requires_preauth', realm.user_princ])
flags = ['-T', realm.ccache]
server_addr = '127.0.0.1:' + str(realm.portbase + 9)

## Test UDP fail / custom username
mark('UDP fail / custom username')
daemon = UDPRadiusDaemon(args=(server_addr, secret_file, 'accept', queue))
daemon.start()
queue.get()
realm.run([kadminl, 'setstr', realm.user_princ, 'otp',
           otpconfig('udp', 'custom')])
realm.kinit(realm.user_princ, 'reject', flags=flags, expected_code=1)
verify(daemon, queue, False, 'custom', 'reject')

## Test UDP success / standard username
mark('UDP success / standard username')
daemon = UDPRadiusDaemon(args=(server_addr, secret_file, 'accept', queue))
daemon.start()
queue.get()
realm.run([kadminl, 'setstr', realm.user_princ, 'otp', otpconfig('udp')])
realm.kinit(realm.user_princ, 'accept', flags=flags)
verify(daemon, queue, True, realm.user_princ.split('@')[0], 'accept')
realm.extract_keytab(realm.krbtgt_princ, realm.keytab)
realm.run(['./adata', realm.krbtgt_princ],
          expected_msg='+97: [indotp1, indotp2]')

# Repeat with an indicators override in the string attribute.
mark('auth indicator override')
daemon = UDPRadiusDaemon(args=(server_addr, secret_file, 'accept', queue))
daemon.start()
queue.get()
oconf = otpconfig('udp', indicators=['indtok1', 'indtok2'])
realm.run([kadminl, 'setstr', realm.user_princ, 'otp', oconf])
realm.kinit(realm.user_princ, 'accept', flags=flags)
verify(daemon, queue, True, realm.user_princ.split('@')[0], 'accept')
realm.extract_keytab(realm.krbtgt_princ, realm.keytab)
realm.run(['./adata', realm.krbtgt_princ],
          expected_msg='+97: [indtok1, indtok2]')

# Detect upstream pyrad bug
#   https://github.com/wichert/pyrad/pull/18
try:
    auth = packet.Packet.CreateAuthenticator()
    packet.Packet(authenticator=auth, secret=b'').ReplyPacket()
except AssertionError:
    skip_rest('OTP UNIX domain socket tests', 'pyrad assertion bug detected')

## Test Unix fail / custom username
mark('Unix socket fail / custom username')
daemon = UnixRadiusDaemon(args=(socket_file, None, 'accept', queue))
daemon.start()
queue.get()
realm.run([kadminl, 'setstr', realm.user_princ, 'otp',
           otpconfig('unix', 'custom')])
realm.kinit(realm.user_princ, 'reject', flags=flags, expected_code=1)
verify(daemon, queue, False, 'custom', 'reject')

## Test Unix success / standard username
mark('Unix socket success / standard username')
daemon = UnixRadiusDaemon(args=(socket_file, None, 'accept', queue))
daemon.start()
queue.get()
realm.run([kadminl, 'setstr', realm.user_princ, 'otp', otpconfig('unix')])
realm.kinit(realm.user_princ, 'accept', flags=flags)
verify(daemon, queue, True, realm.user_princ, 'accept')

## Regression test for #8708: test with the standard username and two
## tokens configured, with the first rejecting and the second
## accepting.  With the bug, the KDC incorrectly rejects the request
## and then performs invalid memory accesses, most likely crashing.
queue2 = Queue()
daemon1 = UDPRadiusDaemon(args=(server_addr, secret_file, 'accept1', queue))
daemon2 = UnixRadiusDaemon(args=(socket_file, None, 'accept2', queue2))
daemon1.start()
queue.get()
daemon2.start()
queue2.get()
oconf = '[' + otpconfig_1('udp') + ', ' + otpconfig_1('unix') + ']'
realm.run([kadminl, 'setstr', realm.user_princ, 'otp', oconf])
realm.kinit(realm.user_princ, 'accept2', flags=flags)
verify(daemon1, queue, False, realm.user_princ.split('@')[0], 'accept2')
verify(daemon2, queue2, True, realm.user_princ, 'accept2')

success('OTP tests')
