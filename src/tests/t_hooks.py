#!/usr/bin/python
from k5test import *

# Test that KDC send and recv hooks work correctly.
realm = K5Realm(create_host=False, get_creds=False)
realm.run(['./hooks', realm.user_princ, password('user')])
realm.stop()

success('send and recv hook tests')
