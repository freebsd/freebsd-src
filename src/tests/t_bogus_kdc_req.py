#!/usr/bin/python

import base64
import socket
from k5test import *

realm = K5Realm()

# Send encodings that are invalid KDC-REQs, but pass krb5_is_as_req()
# and krb5_is_tgs_req(), to make sure that the KDC recovers correctly
# from failures in decode_krb5_as_req() and decode_krb5_tgs_req().

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
a = (hostname, realm.portbase)


# Bogus AS-REQ

x1 = base64.b16decode('6AFF')
s.sendto(x1, a)

# Make sure kinit still works.

realm.kinit(realm.user_princ, password('user'))

# Bogus TGS-REQ

x2 = base64.b16decode('6CFF')
s.sendto(x2, a)

# Make sure kinit still works.

realm.kinit(realm.user_princ, password('user'))

# Not a KDC-REQ, even a little bit

x3 = base64.b16decode('FFFF')
s.sendto(x3, a)

# Make sure kinit still works.

realm.kinit(realm.user_princ, password('user'))

success('Bogus KDC-REQ test')
