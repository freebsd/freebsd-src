#!/usr/bin/python
from k5test import *

for realm in multipass_realms(create_host=False):
    # Check that kinit fails appropriately with the wrong password.
    output = realm.run([kinit, realm.user_princ], input='wrong\n',
                       expected_code=1)
    if 'Password incorrect while getting initial credentials' not in output:
        fail('Expected error message not seen in kinit output')

    # Check that we can kinit as a different principal.
    realm.kinit(realm.admin_princ, password('admin'))
    realm.klist(realm.admin_princ)

    # Test FAST kinit.
    fastpw = password('fast')
    realm.run([kadminl, 'ank', '-pw', fastpw, '+requires_preauth',
               'user/fast'])
    realm.kinit('user/fast', fastpw)
    realm.kinit('user/fast', fastpw, flags=['-T', realm.ccache])
    realm.klist('user/fast@%s' % realm.realm)

    # Test kinit against kdb keytab
    realm.run([kinit, "-k", "-t", "KDB:", realm.user_princ])

# Test that we can get initial creds with an empty password via the
# API.  We have to disable the "empty" pwqual module to create a
# principal with an empty password.  (Regression test for #7642.)
conf={'plugins': {'pwqual': {'disable': 'empty'}}}
realm = K5Realm(create_user=False, create_host=False, krb5_conf=conf)
realm.run([kadminl, 'addprinc', '-pw', '', 'user'])
realm.run(['./icred', 'user', ''])
realm.stop()

realm = K5Realm(create_host=False)

# Regression test for #8454 (responder callback isn't used when
# preauth is not required).
realm.run(['./responder', '-r', 'password=%s' % password('user'),
           realm.user_princ])

# Test that WRONG_REALM responses aren't treated as referrals unless
# they contain a crealm field pointing to a different realm.
# (Regression test for #8060.)
out = realm.run([kinit, '-C', 'notfoundprinc'], expected_code=1)
if 'not found in Kerberos database' not in out:
    fail('Expected error message not seen in kinit -C output')

# Spot-check KRB5_TRACE output
tracefile = os.path.join(realm.testdir, 'trace')
realm.run(['env', 'KRB5_TRACE=' + tracefile, kinit, realm.user_princ],
          input=(password('user') + "\n"))
f = open(tracefile, 'r')
trace = f.read()
f.close()
expected = ('Sending initial UDP request',
            'Received answer',
            'Selected etype info',
            'AS key obtained',
            'Decrypted AS reply',
            'FAST negotiation: available',
            'Storing user@KRBTEST.COM')
for e in expected:
    if e not in trace:
        fail('Expected output not in kinit trace log')

success('FAST kinit, trace logging')
