#!/usr/local/bin/python3
#
# Copyright (c) 2014 The FreeBSD Foundation
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



import binascii
import errno
import cryptodev
import itertools
import os
import struct
import unittest
from cryptodev import *
from glob import iglob

katdir = '/usr/local/share/nist-kat'

def katg(base, glob):
    assert os.path.exists(katdir), "Please 'pkg install nist-kat'"
    if not os.path.exists(os.path.join(katdir, base)):
        raise unittest.SkipTest("Missing %s test vectors" % (base))
    return iglob(os.path.join(katdir, base, glob))

aesmodules = [ 'cryptosoft0', 'aesni0', 'armv8crypto0', 'ccr0', 'ccp0', 'ossl0', 'safexcel0', 'qat0' ]
shamodules = [ 'cryptosoft0', 'aesni0', 'armv8crypto0', 'ccr0', 'ccp0', 'ossl0', 'safexcel0', 'qat0' ]

def GenTestCase(cname):
    try:
        crid = cryptodev.Crypto.findcrid(cname)
    except IOError:
        return None

    class GendCryptoTestCase(unittest.TestCase):
        ###############
        ##### AES #####
        ###############
        @unittest.skipIf(cname not in aesmodules, 'skipping AES-XTS on %s' % (cname))
        def test_xts(self):
            for i in katg('XTSTestVectors/format tweak value input - data unit seq no', '*.rsp'):
                self.runXTS(i, cryptodev.CRYPTO_AES_XTS)

        @unittest.skipIf(cname not in aesmodules, 'skipping AES-CBC on %s' % (cname))
        def test_cbc(self):
            for i in katg('KAT_AES', 'CBC[GKV]*.rsp'):
                self.runCBC(i)

        @unittest.skipIf(cname not in aesmodules, 'skipping AES-CCM on %s' % (cname))
        def test_ccm(self):
            for i in katg('ccmtestvectors', 'V*.rsp'):
                self.runCCMEncrypt(i)

            for i in katg('ccmtestvectors', 'D*.rsp'):
                self.runCCMDecrypt(i)

        @unittest.skipIf(cname not in aesmodules, 'skipping AES-GCM on %s' % (cname))
        def test_gcm(self):
            for i in katg('gcmtestvectors', 'gcmEncrypt*'):
                self.runGCM(i, 'ENCRYPT')

            for i in katg('gcmtestvectors', 'gcmDecrypt*'):
                self.runGCM(i, 'DECRYPT')

        def runGCM(self, fname, mode):
            curfun = None
            if mode == 'ENCRYPT':
                swapptct = False
                curfun = Crypto.encrypt
            elif mode == 'DECRYPT':
                swapptct = True
                curfun = Crypto.decrypt
            else:
                raise RuntimeError('unknown mode: %r' % repr(mode))

            columns = [ 'Count', 'Key', 'IV', 'CT', 'AAD', 'Tag', 'PT', ]
            with cryptodev.KATParser(fname, columns) as parser:
                self.runGCMWithParser(parser, mode)

        def runGCMWithParser(self, parser, mode):
            for _, lines in next(parser):
                for data in lines:
                    curcnt = int(data['Count'])
                    cipherkey = binascii.unhexlify(data['Key'])
                    iv = binascii.unhexlify(data['IV'])
                    aad = binascii.unhexlify(data['AAD'])
                    tag = binascii.unhexlify(data['Tag'])
                    if 'FAIL' not in data:
                        pt = binascii.unhexlify(data['PT'])
                    ct = binascii.unhexlify(data['CT'])

                    if len(iv) != 12:
                        # XXX - isn't supported
                        continue

                    try:
                        c = Crypto(cryptodev.CRYPTO_AES_NIST_GCM_16,
                            cipherkey, crid=crid,
                            maclen=16)
                    except EnvironmentError as e:
                        # Can't test algorithms the driver does not support.
                        if e.errno != errno.EOPNOTSUPP:
                            raise
                        continue

                    if mode == 'ENCRYPT':
                        try:
                            rct, rtag = c.encrypt(pt, iv, aad)
                        except EnvironmentError as e:
                            # Can't test inputs the driver does not support.
                            if e.errno != errno.EINVAL:
                                raise
                            continue
                        rtag = rtag[:len(tag)]
                        data['rct'] = binascii.hexlify(rct)
                        data['rtag'] = binascii.hexlify(rtag)
                        self.assertEqual(rct, ct, repr(data))
                        self.assertEqual(rtag, tag, repr(data))
                    else:
                        if len(tag) != 16:
                            continue
                        args = (ct, iv, aad, tag)
                        if 'FAIL' in data:
                            self.assertRaises(IOError,
                                c.decrypt, *args)
                        else:
                            try:
                                rpt, rtag = c.decrypt(*args)
                            except EnvironmentError as e:
                                # Can't test inputs the driver does not support.
                                if e.errno != errno.EINVAL:
                                    raise
                                continue
                            data['rpt'] = binascii.hexlify(rpt)
                            data['rtag'] = binascii.hexlify(rtag)
                            self.assertEqual(rpt, pt,
                                repr(data))

        def runCBC(self, fname):
            columns = [ 'COUNT', 'KEY', 'IV', 'PLAINTEXT', 'CIPHERTEXT', ]
            with cryptodev.KATParser(fname, columns) as parser:
                self.runCBCWithParser(parser)

        def runCBCWithParser(self, parser):
            curfun = None
            for mode, lines in next(parser):
                if mode == 'ENCRYPT':
                    swapptct = False
                    curfun = Crypto.encrypt
                elif mode == 'DECRYPT':
                    swapptct = True
                    curfun = Crypto.decrypt
                else:
                    raise RuntimeError('unknown mode: %r' % repr(mode))

                for data in lines:
                    curcnt = int(data['COUNT'])
                    cipherkey = binascii.unhexlify(data['KEY'])
                    iv = binascii.unhexlify(data['IV'])
                    pt = binascii.unhexlify(data['PLAINTEXT'])
                    ct = binascii.unhexlify(data['CIPHERTEXT'])

                    if swapptct:
                        pt, ct = ct, pt
                    # run the fun
                    c = Crypto(cryptodev.CRYPTO_AES_CBC, cipherkey, crid=crid)
                    r = curfun(c, pt, iv)
                    self.assertEqual(r, ct)

        def runXTS(self, fname, meth):
            columns = [ 'COUNT', 'DataUnitLen', 'Key', 'DataUnitSeqNumber', 'PT',
                        'CT']
            with cryptodev.KATParser(fname, columns) as parser:
                self.runXTSWithParser(parser, meth)

        def runXTSWithParser(self, parser, meth):
            curfun = None
            for mode, lines in next(parser):
                if mode == 'ENCRYPT':
                    swapptct = False
                    curfun = Crypto.encrypt
                elif mode == 'DECRYPT':
                    swapptct = True
                    curfun = Crypto.decrypt
                else:
                    raise RuntimeError('unknown mode: %r' % repr(mode))

                for data in lines:
                    curcnt = int(data['COUNT'])
                    nbits = int(data['DataUnitLen'])
                    cipherkey = binascii.unhexlify(data['Key'])
                    iv = struct.pack('QQ', int(data['DataUnitSeqNumber']), 0)
                    pt = binascii.unhexlify(data['PT'])
                    ct = binascii.unhexlify(data['CT'])

                    if nbits % 128 != 0:
                        # XXX - mark as skipped
                        continue
                    if swapptct:
                        pt, ct = ct, pt
                    # run the fun
                    try:
                        c = Crypto(meth, cipherkey, crid=crid)
                        r = curfun(c, pt, iv)
                    except EnvironmentError as e:
                        # Can't test hashes the driver does not support.
                        if e.errno != errno.EOPNOTSUPP:
                            raise
                        continue
                    self.assertEqual(r, ct)

        def runCCMEncrypt(self, fname):
            with cryptodev.KATCCMParser(fname) as parser:
                self.runCCMEncryptWithParser(parser)

        def runCCMEncryptWithParser(self, parser):
            for data in next(parser):
                Nlen = int(data['Nlen'])
                Tlen = int(data['Tlen'])
                key = binascii.unhexlify(data['Key'])
                nonce = binascii.unhexlify(data['Nonce'])
                Alen = int(data['Alen'])
                Plen = int(data['Plen'])
                if Alen != 0:
                    aad = binascii.unhexlify(data['Adata'])
                else:
                    aad = None
                if Plen != 0:
                    payload = binascii.unhexlify(data['Payload'])
                else:
                    payload = None
                ct = binascii.unhexlify(data['CT'])

                try:
                    c = Crypto(crid=crid,
                        cipher=cryptodev.CRYPTO_AES_CCM_16,
                        key=key,
                        mackey=key, maclen=Tlen, ivlen=Nlen)
                    r, tag = Crypto.encrypt(c, payload,
                        nonce, aad)
                except EnvironmentError as e:
                    if e.errno != errno.EOPNOTSUPP:
                        raise
                    continue

                out = r + tag
                self.assertEqual(out, ct,
                    "Count " + data['Count'] + " Actual: " + \
                    repr(binascii.hexlify(out)) + " Expected: " + \
                    repr(data) + " on " + cname)

        def runCCMDecrypt(self, fname):
            with cryptodev.KATCCMParser(fname) as parser:
                self.runCCMDecryptWithParser(parser)

        def runCCMDecryptWithParser(self, parser):
            for data in next(parser):
                Nlen = int(data['Nlen'])
                Tlen = int(data['Tlen'])
                key = binascii.unhexlify(data['Key'])
                nonce = binascii.unhexlify(data['Nonce'])
                Alen = int(data['Alen'])
                Plen = int(data['Plen'])
                if Alen != 0:
                    aad = binascii.unhexlify(data['Adata'])
                else:
                    aad = None
                ct = binascii.unhexlify(data['CT'])
                tag = ct[-Tlen:]
                if Plen != 0:
                    payload = ct[:-Tlen]
                else:
                    payload = None

                try:
                    c = Crypto(crid=crid,
                        cipher=cryptodev.CRYPTO_AES_CCM_16,
                        key=key,
                        mackey=key, maclen=Tlen, ivlen=Nlen)
                except EnvironmentError as e:
                    if e.errno != errno.EOPNOTSUPP:
                        raise
                    continue

                if data['Result'] == 'Fail':
                    self.assertRaises(IOError,
                        c.decrypt, payload, nonce, aad, tag)
                else:
                    r, tag = Crypto.decrypt(c, payload, nonce,
                                            aad, tag)

                    payload = binascii.unhexlify(data['Payload'])
                    payload = payload[:Plen]
                    self.assertEqual(r, payload,
                        "Count " + data['Count'] + \
                        " Actual: " + repr(binascii.hexlify(r)) + \
                        " Expected: " + repr(data) + \
                        " on " + cname)

        ###############
        ##### SHA #####
        ###############
        @unittest.skipIf(cname not in shamodules, 'skipping SHA on %s' % str(cname))
        def test_sha(self):
            for i in katg('shabytetestvectors', 'SHA*Msg.rsp'):
                self.runSHA(i)

        def runSHA(self, fname):
            # Skip SHA512_(224|256) tests
            if fname.find('SHA512_') != -1:
                return
            columns = [ 'Len', 'Msg', 'MD' ]
            with cryptodev.KATParser(fname, columns) as parser:
                self.runSHAWithParser(parser)

        def runSHAWithParser(self, parser):
            for hashlength, lines in next(parser):
                # E.g., hashlength will be "L=20" (bytes)
                hashlen = int(hashlength.split("=")[1])

                if hashlen == 20:
                    alg = cryptodev.CRYPTO_SHA1
                elif hashlen == 28:
                    alg = cryptodev.CRYPTO_SHA2_224
                elif hashlen == 32:
                    alg = cryptodev.CRYPTO_SHA2_256
                elif hashlen == 48:
                    alg = cryptodev.CRYPTO_SHA2_384
                elif hashlen == 64:
                    alg = cryptodev.CRYPTO_SHA2_512
                else:
                    # Skip unsupported hashes
                    # Slurp remaining input in section
                    for data in lines:
                        continue
                    continue

                for data in lines:
                    msg = binascii.unhexlify(data['Msg'])
                    msg = msg[:int(data['Len'])]
                    md = binascii.unhexlify(data['MD'])

                    try:
                        c = Crypto(mac=alg, crid=crid,
                            maclen=hashlen)
                    except EnvironmentError as e:
                        # Can't test hashes the driver does not support.
                        if e.errno != errno.EOPNOTSUPP:
                            raise
                        continue

                    _, r = c.encrypt(msg, iv="")

                    self.assertEqual(r, md, "Actual: " + \
                        repr(binascii.hexlify(r)) + " Expected: " + repr(data) + " on " + cname)

        @unittest.skipIf(cname not in shamodules, 'skipping SHA-HMAC on %s' % str(cname))
        def test_sha1hmac(self):
            for i in katg('hmactestvectors', 'HMAC.rsp'):
                self.runSHA1HMAC(i)

        def runSHA1HMAC(self, fname):
            columns = [ 'Count', 'Klen', 'Tlen', 'Key', 'Msg', 'Mac' ]
            with cryptodev.KATParser(fname, columns) as parser:
                self.runSHA1HMACWithParser(parser)

        def runSHA1HMACWithParser(self, parser):
            for hashlength, lines in next(parser):
                # E.g., hashlength will be "L=20" (bytes)
                hashlen = int(hashlength.split("=")[1])

                blocksize = None
                if hashlen == 20:
                    alg = cryptodev.CRYPTO_SHA1_HMAC
                    blocksize = 64
                elif hashlen == 28:
                    alg = cryptodev.CRYPTO_SHA2_224_HMAC
                    blocksize = 64
                elif hashlen == 32:
                    alg = cryptodev.CRYPTO_SHA2_256_HMAC
                    blocksize = 64
                elif hashlen == 48:
                    alg = cryptodev.CRYPTO_SHA2_384_HMAC
                    blocksize = 128
                elif hashlen == 64:
                    alg = cryptodev.CRYPTO_SHA2_512_HMAC
                    blocksize = 128
                else:
                    # Skip unsupported hashes
                    # Slurp remaining input in section
                    for data in lines:
                        continue
                    continue

                for data in lines:
                    key = binascii.unhexlify(data['Key'])
                    msg = binascii.unhexlify(data['Msg'])
                    mac = binascii.unhexlify(data['Mac'])
                    tlen = int(data['Tlen'])

                    if len(key) > blocksize:
                        continue

                    try:
                        c = Crypto(mac=alg, mackey=key,
                            crid=crid, maclen=hashlen)
                    except EnvironmentError as e:
                        # Can't test hashes the driver does not support.
                        if e.errno != errno.EOPNOTSUPP:
                            raise
                        continue

                    _, r = c.encrypt(msg, iv="")

                    self.assertEqual(r[:tlen], mac, "Actual: " + \
                        repr(binascii.hexlify(r)) + " Expected: " + repr(data))

    return GendCryptoTestCase

cryptosoft = GenTestCase('cryptosoft0')
aesni = GenTestCase('aesni0')
armv8crypto = GenTestCase('armv8crypto0')
ccr = GenTestCase('ccr0')
ccp = GenTestCase('ccp0')
ossl = GenTestCase('ossl0')
safexcel = GenTestCase('safexcel0')
qat = GenTestCase('qat0')

if __name__ == '__main__':
    unittest.main()
