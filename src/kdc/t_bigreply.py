from k5test import *
import struct

# Set the maximum UDP reply size very low, so that all replies go
# through the RESPONSE_TOO_BIG path.
kdc_conf = {'kdcdefaults': {'kdc_max_dgram_reply_size': '10'}}
realm = K5Realm(kdc_conf=kdc_conf, get_creds=False)

msgs = ('Sending initial UDP request',
        'Received answer',
        'Request or response is too big for UDP; retrying with TCP',
        ' to KRBTEST.COM (tcp only)',
        'Initiating TCP connection',
        'Sending TCP request',
        'Terminating TCP connection')
realm.kinit(realm.user_princ, password('user'), expected_trace=msgs)
realm.run([kvno, realm.host_princ], expected_trace=msgs)

# Pretend to send an absurdly long request over TCP, and verify that
# we get back a reply of plausible length to be an encoded
# KRB_ERR_RESPONSE_TOO_BIG error.
s = socket.create_connection((hostname, realm.portbase))
s.sendall(b'\xFF\xFF\xFF\xFF')
lenbytes = s.recv(4)
assert(len(lenbytes) == 4)
resplen, = struct.unpack('>L', lenbytes)
if resplen < 10:
    fail('KDC response too short (KRB_ERR_RESPONSE_TOO_BIG error expected)')
resp = s.recv(resplen)
assert(len(resp) == resplen)

success('Large KDC replies')
