from k5test import *

realm = K5Realm(create_host=False)

# Create a second user principal and get tickets for it.
u2u_ccache = 'FILE:' + os.path.join(realm.testdir, 'ccu2u')
realm.addprinc('alice', password('alice'))
realm.kinit('alice', password('alice'), ['-c', u2u_ccache])

# Verify that -allow_dup_skey denies u2u requests.
realm.run([kadminl, 'modprinc', '-allow_dup_skey', 'alice'])
realm.run([kvno, '--u2u', u2u_ccache, 'alice'], expected_code=1,
          expected_msg='KDC policy rejects request')
realm.run([kadminl, 'modprinc', '+allow_dup_skey', 'alice'])

# Verify that -allow_svr denies regular TGS requests, but allows
# user-to-user TGS requests.
realm.run([kadminl, 'modprinc', '-allow_svr', 'alice'])
realm.run([kvno, 'alice'], expected_code=1,
          expected_msg='Server principal valid for user2user only')
realm.run([kvno, '--u2u', u2u_ccache, 'alice'], expected_msg='kvno = 0')
realm.run([kadminl, 'modprinc', '+allow_svr', 'alice'])

# Verify that normal lookups ignore the user-to-user ticket.
realm.run([kvno, 'alice'], expected_msg='kvno = 1')
out = realm.run([klist])
if out.count('alice@KRBTEST.COM') != 2:
    fail('expected two alice tickets after regular kvno')

# Try u2u against the client user.
realm.run([kvno, '--u2u', realm.ccache, realm.user_princ])

realm.run([klist])

realm.stop()

# Load the test KDB module to test aliases
testprincs = {'krbtgt/KRBTEST.COM': {'keys': 'aes128-cts'},
              'user': {'keys': 'aes128-cts', 'flags': '+preauth'},
              'WIN10': {'keys': 'aes128-cts'}}
kdcconf = {'realms': {'$realm': {'database_module': 'test'}},
           'dbmodules': {'test': {'db_library': 'test',
                                  'princs': testprincs,
                                  'alias': {'HOST/win10': 'WIN10'}}}}

realm = K5Realm(kdc_conf=kdcconf, create_kdb=False)
realm.start_kdc()

# Create a second user principal and get tickets for it.
u2u_ccache = 'FILE:' + os.path.join(realm.testdir, 'ccu2u')
realm.extract_keytab('WIN10', realm.keytab)
realm.kinit('WIN10', None, ['-k', '-c', u2u_ccache])

realm.extract_keytab(realm.user_princ, realm.keytab)
realm.kinit(realm.user_princ, None, ['-k'])

realm.run([kvno, '--u2u', u2u_ccache, 'HOST/win10'], expected_msg='kvno = 0')
realm.run([kvno, '--u2u', u2u_ccache, 'WIN10'], expected_msg='kvno = 0')

success('user-to-user tests')
