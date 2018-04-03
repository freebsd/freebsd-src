#!/usr/bin/python
from k5test import *

for realm in multipass_realms(create_host=False):
    # Check that kinit fails appropriately with the wrong password.
    msg = 'Password incorrect while getting initial credentials'
    realm.run([kinit, realm.user_princ], input='wrong\n', expected_code=1,
              expected_msg=msg)

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
realm.run(['./icred', '-s', 'user', ''])
realm.stop()

realm = K5Realm(create_host=False)

# Regression test for #8454 (responder callback isn't used when
# preauth is not required).
realm.run(['./responder', '-r', 'password=%s' % password('user'),
           realm.user_princ])

# Test that WRONG_REALM responses aren't treated as referrals unless
# they contain a crealm field pointing to a different realm.
# (Regression test for #8060.)
realm.run([kinit, '-C', 'notfoundprinc'], expected_code=1,
          expected_msg='not found in Kerberos database')

# Spot-check KRB5_TRACE output
expected_trace = ('Sending initial UDP request',
                  'Received answer',
                  'Selected etype info',
                  'AS key obtained',
                  'Decrypted AS reply',
                  'FAST negotiation: available',
                  'Storing user@KRBTEST.COM')
realm.kinit(realm.user_princ, password('user'), expected_trace=expected_trace)

success('FAST kinit, trace logging')
