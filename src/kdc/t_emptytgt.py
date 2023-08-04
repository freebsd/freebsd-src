from k5test import *

realm = K5Realm(create_host=False)
realm.run([kvno, 'krbtgt/'], expected_code=1,
          expected_msg='not found in Kerberos database')
success('Empty tgt lookup.')
