#!/usr/bin/python
from k5test import *
import re

realm = K5Realm(create_host=False, start_kadmind=True)

# Test password quality enforcement.
realm.run([kadminl, 'addpol', '-minlength', '6', '-minclasses', '2', 'pwpol'])
realm.run([kadminl, 'addprinc', '-randkey', '-policy', 'pwpol', 'pwuser'])
realm.run([kadminl, 'cpw', '-pw', 'sh0rt', 'pwuser'], expected_code=1,
          expected_msg='Password is too short')
realm.run([kadminl, 'cpw', '-pw', 'longenough', 'pwuser'], expected_code=1,
          expected_msg='Password does not contain enough character classes')
realm.run([kadminl, 'cpw', '-pw', 'l0ngenough', 'pwuser'])

# Test some password history enforcement.  Even with no history value,
# the current password should be denied.
realm.run([kadminl, 'cpw', '-pw', 'l0ngenough', 'pwuser'], expected_code=1,
          expected_msg='Cannot reuse password')
realm.run([kadminl, 'modpol', '-history', '2', 'pwpol'])
realm.run([kadminl, 'cpw', '-pw', 'an0therpw', 'pwuser'])
realm.run([kadminl, 'cpw', '-pw', 'l0ngenough', 'pwuser'], expected_code=1,
          expected_msg='Cannot reuse password')
realm.run([kadminl, 'cpw', '-pw', '3rdpassword', 'pwuser'])
realm.run([kadminl, 'cpw', '-pw', 'l0ngenough', 'pwuser'])

# Test references to nonexistent policies.
realm.run([kadminl, 'addprinc', '-randkey', '-policy', 'newpol', 'newuser'])
realm.run([kadminl, 'getprinc', 'newuser'],
          expected_msg='Policy: newpol [does not exist]\n')
realm.run([kadminl, 'modprinc', '-policy', 'newpol', 'pwuser'])
# pwuser should allow reuse of the current password since newpol doesn't exist.
realm.run([kadminl, 'cpw', '-pw', '3rdpassword', 'pwuser'])
# Regression test for #8427 (min_life check with nonexistent policy).
realm.run([kadmin, '-p', 'pwuser', '-w', '3rdpassword', 'cpw', '-pw',
           '3rdpassword', 'pwuser'])

# Create newpol and verify that it is enforced.
realm.run([kadminl, 'addpol', '-minlength', '3', 'newpol'])
realm.run([kadminl, 'getprinc', 'pwuser'], expected_msg='Policy: newpol\n')
realm.run([kadminl, 'cpw', '-pw', 'aa', 'pwuser'], expected_code=1,
          expected_msg='Password is too short')
realm.run([kadminl, 'cpw', '-pw', '3rdpassword', 'pwuser'], expected_code=1,
          expected_msg='Cannot reuse password')

realm.run([kadminl, 'getprinc', 'newuser'], expected_msg='Policy: newpol\n')
realm.run([kadminl, 'cpw', '-pw', 'aa', 'newuser'], expected_code=1,
          expected_msg='Password is too short')

# Delete the policy and verify that it is no longer enforced.
realm.run([kadminl, 'delpol', 'newpol'])
realm.run([kadminl, 'getpol', 'newpol'], expected_code=1,
          expected_msg='Policy does not exist')
realm.run([kadminl, 'cpw', '-pw', 'aa', 'pwuser'])

# Test basic password lockout support.

realm.run([kadminl, 'addpol', '-maxfailure', '2', '-failurecountinterval',
           '5m', 'lockout'])
realm.run([kadminl, 'modprinc', '+requires_preauth', '-policy', 'lockout',
           'user'])

# kinit twice with the wrong password.
realm.run([kinit, realm.user_princ], input='wrong\n', expected_code=1,
          expected_msg='Password incorrect while getting initial credentials')
realm.run([kinit, realm.user_princ], input='wrong\n', expected_code=1,
          expected_msg='Password incorrect while getting initial credentials')

# Now the account should be locked out.
m = 'Client\'s credentials have been revoked while getting initial credentials'
realm.run([kinit, realm.user_princ], expected_code=1, expected_msg=m)

# Check that modprinc -unlock allows a further attempt.
realm.run([kadminl, 'modprinc', '-unlock', 'user'])
realm.kinit(realm.user_princ, password('user'))

# Make sure a nonexistent policy reference doesn't prevent authentication.
realm.run([kadminl, 'delpol', 'lockout'])
realm.kinit(realm.user_princ, password('user'))

# Regression test for issue #7099: databases created prior to krb5 1.3 have
# multiple history keys, and kadmin prior to 1.7 didn't necessarily use the
# first one to create history entries.

realm.stop()
realm = K5Realm(start_kdc=False)
# Create a history principal with two keys.
realm.run(['./hist', 'make'])
realm.run([kadminl, 'addpol', '-history', '2', 'pol'])
realm.run([kadminl, 'modprinc', '-policy', 'pol', 'user'])
realm.run([kadminl, 'cpw', '-pw', 'pw2', 'user'])
# Swap the keys, simulating older kadmin having chosen the second entry.
realm.run(['./hist', 'swap'])
# Make sure we can read the history entry.
realm.run([kadminl, 'cpw', '-pw', password('user'), 'user'], expected_code=1,
          expected_msg='Cannot reuse password')

# Test key/salt constraints.

realm.stop()
krb5_conf1 = {'libdefaults': {'supported_enctypes': 'aes256-cts'}}
realm = K5Realm(krb5_conf=krb5_conf1, create_host=False, get_creds=False)

# Add policy.
realm.run([kadminl, 'addpol', '-allowedkeysalts', 'aes256-cts', 'ak'])
realm.run([kadminl, 'addprinc', '-randkey', '-e', 'aes256-cts', 'server'])

# Test with one-enctype allowed_keysalts.
realm.run([kadminl, 'modprinc', '-policy', 'ak', 'server'])
out = realm.run([kadminl, 'cpw', '-randkey', '-e', 'aes128-cts', 'server'],
                expected_code=1)
if not 'Invalid key/salt tuples' in out:
    fail('allowed_keysalts policy not applied properly')
realm.run([kadminl, 'cpw', '-randkey', '-e', 'aes256-cts', 'server'])

# Now test a multi-enctype allowed_keysalts.  Test that subsets are allowed,
# the the complete set is allowed, that order doesn't matter, and that
# enctypes outside the set are not allowed.

# Test modpol.
realm.run([kadminl, 'modpol', '-allowedkeysalts', 'aes256-cts,rc4-hmac', 'ak'])
realm.run([kadminl, 'getpol', 'ak'],
          expected_msg='Allowed key/salt types: aes256-cts,rc4-hmac')

# Test subsets and full set.
realm.run([kadminl, 'cpw', '-randkey', '-e', 'rc4-hmac', 'server'])
realm.run([kadminl, 'cpw', '-randkey', '-e', 'aes256-cts', 'server'])
realm.run([kadminl, 'cpw', '-randkey', '-e', 'aes256-cts,rc4-hmac', 'server'])
realm.run([kadminl, 'cpw', '-randkey', '-e', 'rc4-hmac,aes256-cts', 'server'])

# Check that the order we got is the one from the policy.
realm.run([kadminl, 'getprinc', '-terse', 'server'],
          expected_msg='2\t1\t6\t18\t0\t1\t6\t23\t0')

# Test partially intersecting sets.
realm.run([kadminl, 'cpw', '-randkey', '-e', 'rc4-hmac,aes128-cts', 'server'],
          expected_code=1, expected_msg='Invalid key/salt tuples')
realm.run([kadminl, 'cpw', '-randkey', '-e', 'rc4-hmac,aes256-cts,aes128-cts',
           'server'], expected_code=1, expected_msg='Invalid key/salt tuples')

# Test reset of allowedkeysalts.
realm.run([kadminl, 'modpol', '-allowedkeysalts', '-', 'ak'])
out = realm.run([kadminl, 'getpol', 'ak'])
if 'Allowed key/salt types' in out:
    fail('failed to clear allowedkeysalts')
realm.run([kadminl, 'cpw', '-randkey', '-e', 'aes128-cts', 'server'])

success('Policy tests')
