#!/usr/local/bin/python3
#
# Copyright (c) 2014 The FreeBSD Foundation
# Copyright 2014 John-Mark Gurney
# All rights reserved.
# Copyright 2019 Enji Cooper
#
# This software was developed by John-Mark Gurney under
# the sponsorship from the FreeBSD Foundation.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#


import array
import binascii
from fcntl import ioctl
import os
import platform
import random
import re
import signal
from struct import pack as _pack
import sys
import time

import dpkt

from cryptodevh import *

__all__ = [ 'Crypto', 'MismatchError', ]

class FindOp(dpkt.Packet):
    __byte_order__ = '@'
    __hdr__ = (
        ('crid', 'i',   0),
        ('name', '32s', 0),
    )

class SessionOp(dpkt.Packet):
    __byte_order__ = '@'
    __hdr__ = (
        ('cipher',    'I', 0),
        ('mac',       'I', 0),
        ('keylen',    'I', 0),
        ('key',       'P', 0),
        ('mackeylen', 'i', 0),
        ('mackey',    'P', 0),
        ('ses',       'I', 0),
    )

class SessionOp2(dpkt.Packet):
    __byte_order__ = '@'
    __hdr__ = (
        ('cipher',    'I', 0),
        ('mac',       'I', 0),
        ('keylen',    'I', 0),
        ('key',       'P', 0),
        ('mackeylen', 'i', 0),
        ('mackey',    'P', 0),
        ('ses',       'I', 0),
        ('crid',      'i', 0),
        ('ivlen',     'i', 0),
        ('maclen',    'i', 0),
        ('pad0',      'i', 0),
        ('pad1',      'i', 0),
    )

class CryptOp(dpkt.Packet):
    __byte_order__ = '@'
    __hdr__ = (
        ('ses',   'I', 0),
        ('op',    'H', 0),
        ('flags', 'H', 0),
        ('len',   'I', 0),
        ('src',   'P', 0),
        ('dst',   'P', 0),
        ('mac',   'P', 0),
        ('iv',    'P', 0),
    )

class CryptAEAD(dpkt.Packet):
    __byte_order__ = '@'
    __hdr__ = (
        ('ses',    'I', 0),
        ('op',     'H', 0),
        ('flags',  'H', 0),
        ('len',    'I', 0),
        ('aadlen', 'I', 0),
        ('ivlen',  'I', 0),
        ('src',    'P', 0),
        ('dst',    'P', 0),
        ('aad',    'P', 0),
        ('tag',    'P', 0),
        ('iv',     'P', 0),
    )

# h2py.py can't handle multiarg macros
CIOCGSESSION = 3224396645
CIOCFSESSION = 2147771238
CIOCKEY = 3230688104
CIOCASYMFEAT = 1074029417
CIOCKEY2 = 3230688107
CIOCFINDDEV = 3223610220
if platform.architecture()[0] == '64bit':
    CIOCGSESSION2 = 3225445226
    CIOCCRYPT = 3224396647
    CIOCCRYPTAEAD = 3225445229
else:
    CIOCGSESSION2 = 3224396650
    CIOCCRYPT = 3223085927
    CIOCCRYPTAEAD = 3223872365

_cryptodev = os.open('/dev/crypto', os.O_RDWR)

def str_to_ascii(val):
    if sys.version_info[0] >= 3:
        if isinstance(val, str):
            return val.encode("ascii")
    return val

def _findop(crid, name):
    fop = FindOp()
    fop.crid = crid
    fop.name = str_to_ascii(name)
    s = array.array('B', fop.pack_hdr())
    ioctl(_cryptodev, CIOCFINDDEV, s, 1)
    fop.unpack(s)

    try:
        idx = fop.name.index(b'\x00')
        name = fop.name[:idx]
    except ValueError:
        name = fop.name

    return fop.crid, name

def array_tobytes(array_obj):
    if sys.version_info[:2] >= (3, 2):
        return array_obj.tobytes()
    return array_obj.tostring()

def empty_bytes():
    if sys.version_info[0] >= 3:
        return b''
    return ""

class Crypto:
    @staticmethod
    def findcrid(name):
        return _findop(-1, name)[0]

    @staticmethod
    def getcridname(crid):
        return _findop(crid, '')[1]

    def __init__(self, cipher=0, key=None, mac=0, mackey=None,
        crid=CRYPTOCAP_F_SOFTWARE | CRYPTOCAP_F_HARDWARE, maclen=None,
        ivlen=None):
        self._ses = None
        self._maclen = maclen
        ses = SessionOp2()
        ses.cipher = cipher
        ses.mac = mac

        if key is not None:
            ses.keylen = len(key)
            k = array.array('B', key)
            ses.key = k.buffer_info()[0]
        else:
            self.key = None

        if mackey is not None:
            ses.mackeylen = len(mackey)
            mk = array.array('B', mackey)
            ses.mackey = mk.buffer_info()[0]

        if not cipher and not mac:
            raise ValueError('one of cipher or mac MUST be specified.')
        ses.crid = crid
        if ivlen:
            ses.ivlen = ivlen
        if maclen:
            ses.maclen = maclen
        #print(ses)
        s = array.array('B', ses.pack_hdr())
        #print(s)
        ioctl(_cryptodev, CIOCGSESSION2, s, 1)
        ses.unpack(s)

        self._ses = ses.ses

    def __del__(self):
        if self._ses is None:
            return

        try:
            ioctl(_cryptodev, CIOCFSESSION, _pack('I', self._ses))
        except TypeError:
            pass
        self._ses = None

    def _doop(self, op, src, iv, mac=None):
        cop = CryptOp()
        cop.ses = self._ses
        cop.op = op
        cop.flags = 0
        if src is not None:
            cop.len = len(src)
            s = array.array('B', src)
            cop.src = cop.dst = s.buffer_info()[0]
        if mac is not None:
            assert len(mac) == self._maclen, \
                '%d != %d' % (len(tag), self._maclen)
        if self._maclen is not None:
            if mac is None:
                m = array.array('B', [0] * self._maclen)
            else:
                m = array.array('B', mac)
            cop.mac = m.buffer_info()[0]
        ivbuf = array.array('B', str_to_ascii(iv))
        cop.iv = ivbuf.buffer_info()[0]

        #print('cop:', cop)
        ioctl(_cryptodev, CIOCCRYPT, bytes(cop))

        if src is not None:
            s = array_tobytes(s)
        else:
            s = empty_bytes()
        if self._maclen is not None:
            return s, array_tobytes(m)

        return s

    def _doaead(self, op, src, aad, iv, tag=None):
        caead = CryptAEAD()
        caead.ses = self._ses
        caead.op = op
        caead.flags = CRD_F_IV_EXPLICIT
        caead.flags = 0
        if src:
            src = str_to_ascii(src)
            caead.len = len(src)
            s = array.array('B', src)
            caead.src = caead.dst = s.buffer_info()[0]
        aad = str_to_ascii(aad)
        caead.aadlen = len(aad)
        saad = array.array('B', aad)
        caead.aad = saad.buffer_info()[0]

        if self._maclen is None:
            raise ValueError('must have a tag length')

        tag = str_to_ascii(tag)
        if tag is None:
            tag = array.array('B', [0] * self._maclen)
        else:
            assert len(tag) == self._maclen, \
                '%d != %d' % (len(tag), self._maclen)
            tag = array.array('B', tag)

        caead.tag = tag.buffer_info()[0]

        ivbuf = array.array('B', iv)
        caead.ivlen = len(iv)
        caead.iv = ivbuf.buffer_info()[0]

        ioctl(_cryptodev, CIOCCRYPTAEAD, bytes(caead))

        if src:
            s = array_tobytes(s)
        else:
            s = empty_bytes()

        return s, array_tobytes(tag)

    def perftest(self, op, size, timeo=3):
        inp = array.array('B', (random.randint(0, 255) for x in range(size)))
        inp = str_to_ascii(inp)
        out = array.array('B', inp)

        # prep ioctl
        cop = CryptOp()
        cop.ses = self._ses
        cop.op = op
        cop.flags = 0
        cop.len = len(inp)
        s = array.array('B', inp)
        cop.src = s.buffer_info()[0]
        cop.dst = out.buffer_info()[0]
        if self._maclen is not None:
            m = array.array('B', [0] * self._maclen)
            cop.mac = m.buffer_info()[0]
        ivbuf = array.array('B', (random.randint(0, 255) for x in range(16)))
        cop.iv = ivbuf.buffer_info()[0]

        exit = [ False ]
        def alarmhandle(a, b, exit=exit):
            exit[0] = True

        oldalarm = signal.signal(signal.SIGALRM, alarmhandle)
        signal.alarm(timeo)

        start = time.time()
        reps = 0
        cop = bytes(cop)
        while not exit[0]:
            ioctl(_cryptodev, CIOCCRYPT, cop)
            reps += 1

        end = time.time()

        signal.signal(signal.SIGALRM, oldalarm)

        print('time:', end - start)
        print('perf MB/sec:', (reps * size) / (end - start) / 1024 / 1024)

    def encrypt(self, data, iv, aad=None):
        if aad is None:
            return self._doop(COP_ENCRYPT, data, iv)
        else:
            return self._doaead(COP_ENCRYPT, data, aad,
                iv)

    def decrypt(self, data, iv, aad=None, tag=None):
        if aad is None:
            return self._doop(COP_DECRYPT, data, iv, mac=tag)
        else:
            return self._doaead(COP_DECRYPT, data, aad,
                iv, tag=tag)

class MismatchError(Exception):
    pass

class KATParser:
    def __init__(self, fname, fields):
        self.fields = set(fields)
        self._pending = None
        self.fname = fname
        self.fp = None
        self.field_re = re.compile(r"\[(?P<field>[^]]+)\]")

    def __enter__(self):
        self.fp = open(self.fname)
        return self

    def __exit__(self, exc_type, exc_value, exc_tb):
        if self.fp is not None:
            self.fp.close()

    def __iter__(self):
        return self

    def __next__(self):
        while True:
            while True:
                if self._pending is not None:
                    i = self._pending
                    self._pending = None
                else:
                    i = self.fp.readline()
                    if not i:
                        return

                if not i.startswith('#') and i.strip():
                    break

            matches = self.field_re.match(i)
            if matches is None:
                raise ValueError("Unknown line: %r" % (i))
            yield matches.group("field"), self.fielditer()

    def eatblanks(self):
        while True:
            line = self.fp.readline()
            if line == '':
                break

            line = line.strip()
            if line:
                break

        return line

    def fielditer(self):
        while True:
            values = {}

            line = self.eatblanks()
            if not line or line[0] == '[':
                self._pending = line
                return

            while True:
                try:
                    f, v = line.split(' =')
                except:
                    if line == 'FAIL':
                        f, v = 'FAIL', ''
                    else:
                        print('line:', repr(line))
                        raise
                v = v.strip()

                if f in values:
                    raise ValueError('already present: %r' % repr(f))
                values[f] = v
                line = self.fp.readline().strip()
                if not line:
                    break

            # we should have everything
            remain = self.fields.copy() - set(values.keys())
            # XXX - special case GCM decrypt
            if remain and not ('FAIL' in values and 'PT' in remain):
                raise ValueError('not all fields found: %r' % repr(remain))

            yield values

# The CCM files use a bit of a different syntax that doesn't quite fit
# the generic KATParser.  In particular, some keys are set globally at
# the start of the file, and some are set globally at the start of a
# section.
class KATCCMParser:
    def __init__(self, fname):
        self._pending = None
        self.fname = fname
        self.fp = None

    def __enter__(self):
        self.fp = open(self.fname)
        self.read_globals()
        return self

    def __exit__(self, exc_type, exc_value, exc_tb):
        if self.fp is not None:
            self.fp.close()

    def read_globals(self):
        self.global_values = {}
        while True:
            line = self.fp.readline()
            if not line:
                return
            if line[0] == '#' or not line.strip():
                continue
            if line[0] == '[':
                self._pending = line
                return

            try:
                f, v = line.split(' =')
            except:
                print('line:', repr(line))
                raise

            v = v.strip()

            if f in self.global_values:
                raise ValueError('already present: %r' % repr(f))
            self.global_values[f] = v

    def read_section_values(self, kwpairs):
        self.section_values = self.global_values.copy()
        for pair in kwpairs.split(', '):
            f, v = pair.split(' = ')
            if f in self.section_values:
                raise ValueError('already present: %r' % repr(f))
            self.section_values[f] = v

        while True:
            line = self.fp.readline()
            if not line:
                return
            if line[0] == '#' or not line.strip():
                continue
            if line[0] == '[':
                self._pending = line
                return

            try:
                f, v = line.split(' =')
            except:
                print('line:', repr(line))
                raise

            if f == 'Count':
                self._pending = line
                return

            v = v.strip()

            if f in self.section_values:
                raise ValueError('already present: %r' % repr(f))
            self.section_values[f] = v

    def __iter__(self):
        return self

    def __next__(self):
        while True:
            if self._pending:
                line = self._pending
                self._pending = None
            else:
                line = self.fp.readline()
                if not line:
                    return

            if (line and line[0] == '#') or not line.strip():
                continue

            if line[0] == '[':
                section = line[1:].split(']', 1)[0]
                self.read_section_values(section)
                continue

            values = self.section_values.copy()

            while True:
                try:
                    f, v = line.split(' =')
                except:
                    print('line:', repr(line))
                    raise
                v = v.strip()

                if f in values:
                    raise ValueError('already present: %r' % repr(f))
                values[f] = v
                line = self.fp.readline().strip()
                if not line:
                    break

            yield values

def _spdechex(s):
    return binascii.hexlify(''.join(s.split()))

if sys.version_info[0] < 3:
    KATCCMParser.next = KATCCMParser.__next__
    KATParser.next = KATParser.__next__

if __name__ == '__main__':
    if True:
        try:
            crid = Crypto.findcrid('aesni0')
            print('aesni:', crid)
        except IOError:
            print('aesni0 not found')

        for i in range(10):
            try:
                name = Crypto.getcridname(i)
                print('%2d: %r' % (i, repr(name)))
            except IOError:
                pass
    elif False:
        columns = [ 'COUNT', 'DataUnitLen', 'Key', 'DataUnitSeqNumber', 'PT', 'CT' ]
        fname = '/usr/home/jmg/aesni.testing/format tweak value input - data unit seq no/XTSGenAES128.rsp'
        with KATParser(fname, columns) as kp:
            for mode, ni in kp:
                print(i, ni)
                for j in ni:
                    print(j)
    elif False:
        key = _spdechex('c939cc13397c1d37de6ae0e1cb7c423c')
        iv = _spdechex('00000000000000000000000000000001')
        pt = _spdechex('ab3cabed693a32946055524052afe3c9cb49664f09fc8b7da824d924006b7496353b8c1657c5dec564d8f38d7432e1de35aae9d95590e66278d4acce883e51abaf94977fcd3679660109a92bf7b2973ccd547f065ec6cee4cb4a72a5e9f45e615d920d76cb34cba482467b3e21422a7242e7d931330c0fbf465c3a3a46fae943029fd899626dda542750a1eee253df323c6ef1573f1c8c156613e2ea0a6cdbf2ae9701020be2d6a83ecb7f3f9d8e')
        #pt = _spdechex('00000000000000000000000000000000')
        ct = _spdechex('f42c33853ecc5ce2949865fdb83de3bff1089e9360c94f830baebfaff72836ab5236f77212f1e7396c8c54ac73d81986375a6e9e299cfeca5ba051ed25e8d1affa5beaf6c1d2b45e90802408f2ced21663497e906de5f29341e5e52ddfea5363d628b3eb7806835e17bae051b3a6da3f8e2941fe44384eac17a9d298d2c331ca8320c775b5d53263a5e905059d891b21dede2d8110fd427c7bd5a9a274ddb47b1945ee79522203b6e297d0e399ef')

        c = Crypto(CRYPTO_AES_ICM, key)
        enc = c.encrypt(pt, iv)

        print('enc:', binascii.hexlify(enc))
        print(' ct:', binascii.hexlify(ct))

        assert ct == enc

        dec = c.decrypt(ct, iv)

        print('dec:', binascii.hexlify(dec))
        print(' pt:', binascii.hexlify(pt))

        assert pt == dec
    elif False:
        key = _spdechex('c939cc13397c1d37de6ae0e1cb7c423c')
        iv = _spdechex('00000000000000000000000000000001')
        pt = _spdechex('ab3cabed693a32946055524052afe3c9cb49664f09fc8b7da824d924006b7496353b8c1657c5dec564d8f38d7432e1de35aae9d95590e66278d4acce883e51abaf94977fcd3679660109a92bf7b2973ccd547f065ec6cee4cb4a72a5e9f45e615d920d76cb34cba482467b3e21422a7242e7d931330c0fbf465c3a3a46fae943029fd899626dda542750a1eee253df323c6ef1573f1c8c156613e2ea0a6cdbf2ae9701020be2d6a83ecb7f3f9d8e0a3f')
        #pt = _spdechex('00000000000000000000000000000000')
        ct = _spdechex('f42c33853ecc5ce2949865fdb83de3bff1089e9360c94f830baebfaff72836ab5236f77212f1e7396c8c54ac73d81986375a6e9e299cfeca5ba051ed25e8d1affa5beaf6c1d2b45e90802408f2ced21663497e906de5f29341e5e52ddfea5363d628b3eb7806835e17bae051b3a6da3f8e2941fe44384eac17a9d298d2c331ca8320c775b5d53263a5e905059d891b21dede2d8110fd427c7bd5a9a274ddb47b1945ee79522203b6e297d0e399ef3768')

        c = Crypto(CRYPTO_AES_ICM, key)
        enc = c.encrypt(pt, iv)

        print('enc:', binascii.hexlify(enc))
        print(' ct:', binascii.hexlify(ct))

        assert ct == enc

        dec = c.decrypt(ct, iv)

        print('dec:', binascii.hexlify(dec))
        print(' pt:', binascii.hexlify(pt))

        assert pt == dec
    elif False:
        key = _spdechex('c939cc13397c1d37de6ae0e1cb7c423c')
        iv = _spdechex('6eba2716ec0bd6fa5cdef5e6d3a795bc')
        pt = _spdechex('ab3cabed693a32946055524052afe3c9cb49664f09fc8b7da824d924006b7496353b8c1657c5dec564d8f38d7432e1de35aae9d95590e66278d4acce883e51abaf94977fcd3679660109a92bf7b2973ccd547f065ec6cee4cb4a72a5e9f45e615d920d76cb34cba482467b3e21422a7242e7d931330c0fbf465c3a3a46fae943029fd899626dda542750a1eee253df323c6ef1573f1c8c156613e2ea0a6cdbf2ae9701020be2d6a83ecb7f3f9d8e0a3f')
        ct = _spdechex('f1f81f12e72e992dbdc304032705dc75dc3e4180eff8ee4819906af6aee876d5b00b7c36d282a445ce3620327be481e8e53a8e5a8e5ca9abfeb2281be88d12ffa8f46d958d8224738c1f7eea48bda03edbf9adeb900985f4fa25648b406d13a886c25e70cfdecdde0ad0f2991420eb48a61c64fd797237cf2798c2675b9bb744360b0a3f329ac53bbceb4e3e7456e6514f1a9d2f06c236c31d0f080b79c15dce1096357416602520daa098b17d1af427')
        c = Crypto(CRYPTO_AES_CBC, key)

        enc = c.encrypt(pt, iv)

        print('enc:', binascii.hexlify(enc))
        print(' ct:', binascii.hexlify(ct))

        assert ct == enc

        dec = c.decrypt(ct, iv)

        print('dec:', binascii.hexlify(dec))
        print(' pt:', binascii.hexlify(pt))

        assert pt == dec
    elif False:
        key = _spdechex('c939cc13397c1d37de6ae0e1cb7c423c')
        iv = _spdechex('b3d8cc017cbb89b39e0f67e2')
        pt = _spdechex('c3b3c41f113a31b73d9a5cd4321030')
        aad = _spdechex('24825602bd12a984e0092d3e448eda5f')
        ct = _spdechex('93fe7d9e9bfd10348a5606e5cafa7354')
        ct = _spdechex('93fe7d9e9bfd10348a5606e5cafa73')
        tag = _spdechex('0032a1dc85f1c9786925a2e71d8272dd')
        tag = _spdechex('8d11a0929cb3fbe1fef01a4a38d5f8ea')

        c = Crypto(CRYPTO_AES_NIST_GCM_16, key)

        enc, enctag = c.encrypt(pt, iv, aad=aad)

        print('enc:', binascii.hexlify(enc))
        print(' ct:', binascii.hexlify(ct))

        assert enc == ct

        print('etg:', binascii.hexlify(enctag))
        print('tag:', binascii.hexlify(tag))
        assert enctag == tag

        # Make sure we get EBADMSG
        #enctag = enctag[:-1] + 'a'
        dec, dectag = c.decrypt(ct, iv, aad=aad, tag=enctag)

        print('dec:', binascii.hexlify(dec))
        print(' pt:', binascii.hexlify(pt))

        assert dec == pt

        print('dtg:', binascii.hexlify(dectag))
        print('tag:', binascii.hexlify(tag))

        assert dectag == tag
    elif False:
        key = _spdechex('c939cc13397c1d37de6ae0e1cb7c423c')
        iv = _spdechex('b3d8cc017cbb89b39e0f67e2')
        key = key + iv[:4]
        iv = iv[4:]
        pt = _spdechex('c3b3c41f113a31b73d9a5cd432103069')
        aad = _spdechex('24825602bd12a984e0092d3e448eda5f')
        ct = _spdechex('93fe7d9e9bfd10348a5606e5cafa7354')
        tag = _spdechex('0032a1dc85f1c9786925a2e71d8272dd')

        c = Crypto(CRYPTO_AES_GCM_16, key)

        enc, enctag = c.encrypt(pt, iv, aad=aad)

        print('enc:', binascii.hexlify(enc))
        print(' ct:', binascii.hexlify(ct))

        assert enc == ct

        print('etg:', binascii.hexlify(enctag))
        print('tag:', binascii.hexlify(tag))
        assert enctag == tag
    elif False:
        for i in range(100000):
            c = Crypto(CRYPTO_AES_XTS, binascii.unhexlify('1bbfeadf539daedcae33ced497343f3ca1f2474ad932b903997d44707db41382'))
            data = binascii.unhexlify('52a42bca4e9425a25bbc8c8bf6129dec')
            ct = binascii.unhexlify('517e602becd066b65fa4f4f56ddfe240')
            iv = _pack('QQ', 71, 0)

            enc = c.encrypt(data, iv)
            assert enc == ct
    elif True:
        c = Crypto(CRYPTO_AES_XTS, binascii.unhexlify('1bbfeadf539daedcae33ced497343f3ca1f2474ad932b903997d44707db41382'))
        data = binascii.unhexlify('52a42bca4e9425a25bbc8c8bf6129dec')
        ct = binascii.unhexlify('517e602becd066b65fa4f4f56ddfe240')
        iv = _pack('QQ', 71, 0)

        enc = c.encrypt(data, iv)
        assert enc == ct

        dec = c.decrypt(enc, iv)
        assert dec == data

        #c.perftest(COP_ENCRYPT, 192*1024, reps=30000)

    else:
        key = binascii.unhexlify('1bbfeadf539daedcae33ced497343f3ca1f2474ad932b903997d44707db41382')
        print('XTS %d testing:' % (len(key) * 8))
        c = Crypto(CRYPTO_AES_XTS, key)
        for i in [ 8192, 192*1024]:
            print('block size: %d' % i)
            c.perftest(COP_ENCRYPT, i)
            c.perftest(COP_DECRYPT, i)
