from k5test import *

realm = K5Realm()
realm.run(['./replay', realm.host_princ])

success('Replay tests')
