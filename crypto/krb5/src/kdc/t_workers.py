from k5test import *

realm = K5Realm(start_kdc=False, create_host=False)
realm.start_kdc(['-w', '3'])
realm.kinit(realm.user_princ, password('user'))
realm.klist(realm.user_princ)
success('KDC worker processes')
