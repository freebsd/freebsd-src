#!/usr/bin/python
from k5test import *
import re

realm = K5Realm(create_host=False, start_kadmind=True)

# Test password quality enforcement.
realm.run([kadminl, 'addpol', '-minlength', '6', '-minclasses', '2', 'pwpol'])
realm.run([kadminl, 'addprinc', '-randkey', '-policy', 'pwpol', 'pwuser'])
out = realm.run([kadminl, 'cpw', '-pw', 'sh0rt', 'pwuser'], expected_code=1)
if 'Password is too short' not in out:
    fail('short password')
out = realm.run([kadminl, 'cpw', '-pw', 'longenough', 'pwuser'],
                expected_code=1)
if 'Password does not contain enough character classes' not in out:
    fail('insufficient character classes')
realm.run([kadminl, 'cpw', '-pw', 'l0ngenough', 'pwuser'])

# Test some password history enforcement.  Even with no history value,
# the current password should be denied.
out = realm.run([kadminl, 'cpw', '-pw', 'l0ngenough', 'pwuser'],
                expected_code=1)
if 'Cannot reuse password' not in out:
    fail('reuse of current password')
realm.run([kadminl, 'modpol', '-history', '2', 'pwpol'])
realm.run([kadminl, 'cpw', '-pw', 'an0therpw', 'pwuser'])
out = realm.run([kadminl, 'cpw', '-pw', 'l0ngenough', 'pwuser'],
                expected_code=1)
if 'Cannot reuse password' not in out:
    fail('reuse of old password')
realm.run([kadminl, 'cpw', '-pw', '3rdpassword', 'pwuser'])
realm.run([kadminl, 'cpw', '-pw', 'l0ngenough', 'pwuser'])

# Test references to nonexistent policies.
realm.run([kadminl, 'addprinc', '-randkey', '-policy', 'newpol', 'newuser'])
out = realm.run([kadminl, 'getprinc', 'newuser'])
if 'Policy: newpol [does not exist]\n' not in out:
    fail('getprinc output for principal referencing nonexistent policy')
realm.run([kadminl, 'modprinc', '-policy', 'newpol', 'pwuser'])
# pwuser should allow reuse of the current password since newpol doesn't exist.
realm.run([kadminl, 'cpw', '-pw', '3rdpassword', 'pwuser'])
# Regression test for #8427 (min_life check with nonexistent policy).
realm.run([kadmin, '-p', 'pwuser', '-w', '3rdpassword', 'cpw', '-pw',
           '3rdpassword', 'pwuser'])

# Create newpol and verify that it is enforced.
realm.run([kadminl, 'addpol', '-minlength', '3', 'newpol'])
out = realm.run([kadminl, 'getprinc', 'pwuser'])
if 'Policy: newpol\n' not in out:
    fail('getprinc after creating policy (pwuser)')
out = realm.run([kadminl, 'cpw', '-pw', 'aa', 'pwuser'], expected_code=1)
if 'Password is too short' not in out:
    fail('short password after creating policy (pwuser)')
out = realm.run([kadminl, 'cpw', '-pw', '3rdpassword', 'pwuser'],
                expected_code=1)
if 'Cannot reuse password' not in out:
    fail('reuse of current password after creating policy')

out = realm.run([kadminl, 'getprinc', 'newuser'])
if 'Policy: newpol\n' not in out:
    fail('getprinc after creating policy (newuser)')
out = realm.run([kadminl, 'cpw', '-pw', 'aa', 'newuser'], expected_code=1)
if 'Password is too short' not in out:
    fail('short password after creating policy (newuser)')

# Delete the policy and verify that it is no longer enforced.
realm.run([kadminl, 'delpol', 'newpol'])
out = realm.run([kadminl, 'getpol', 'newpol'], expected_code=1)
if 'Policy does not exist' not in out:
    fail('deletion of referenced policy')
realm.run([kadminl, 'cpw', '-pw', 'aa', 'pwuser'])

# Test basic password lockout support.

realm.run([kadminl, 'addpol', '-maxfailure', '2', '-failurecountinterval',
           '5m', 'lockout'])
realm.run([kadminl, 'modprinc', '+requires_preauth', '-policy', 'lockout',
           'user'])

# kinit twice with the wrong password.
output = realm.run([kinit, realm.user_princ], input='wrong\n', expected_code=1)
if 'Password incorrect while getting initial credentials' not in output:
    fail('Expected error message not seen in kinit output')
output = realm.run([kinit, realm.user_princ], input='wrong\n', expected_code=1)
if 'Password incorrect while getting initial credentials' not in output:
    fail('Expected error message not seen in kinit output')

# Now the account should be locked out.
output = realm.run([kinit, realm.user_princ], expected_code=1)
if 'Client\'s credentials have been revoked while getting initial credentials' \
        not in output:
    fail('Expected lockout error message not seen in kinit output')

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
out = realm.run([kadminl, 'cpw', '-pw', password('user'), 'user'],
                expected_code=1)
if 'Cannot reuse password' not in out:
    fail('Expected error not seen in output')

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
out = realm.run([kadminl, 'getpol', 'ak'])
if not 'Allowed key/salt types: aes256-cts,rc4-hmac' in out:
    fail('getpol does not implement allowedkeysalts?')

# Test subsets and full set.
realm.run([kadminl, 'cpw', '-randkey', '-e', 'rc4-hmac', 'server'])
realm.run([kadminl, 'cpw', '-randkey', '-e', 'aes256-cts', 'server'])
realm.run([kadminl, 'cpw', '-randkey', '-e', 'aes256-cts,rc4-hmac', 'server'])
realm.run([kadminl, 'cpw', '-randkey', '-e', 'rc4-hmac,aes256-cts', 'server'])

# Check that the order we got is the one from the policy.
out = realm.run([kadminl, 'getprinc', '-terse', 'server'])
if not '2\t1\t6\t18\t0\t1\t6\t23\t0' in out:
    fail('allowed_keysalts policy did not preserve order')

# Test partially intersecting sets.
out = realm.run([kadminl, 'cpw', '-randkey', '-e', 'rc4-hmac,aes128-cts',
                 'server'], expected_code=1)
if not 'Invalid key/salt tuples' in out:
    fail('allowed_keysalts policy not applied properly')
out = realm.run([kadminl, 'cpw', '-randkey', '-e',
                 'rc4-hmac,aes256-cts,aes128-cts', 'server'], expected_code=1)
if not 'Invalid key/salt tuples' in out:
    fail('allowed_keysalts policy not applied properly')

# Test reset of allowedkeysalts.
realm.run([kadminl, 'modpol', '-allowedkeysalts', '-', 'ak'])
out = realm.run([kadminl, 'getpol', 'ak'])
if 'Allowed key/salt types' in out:
    fail('failed to clear allowedkeysalts')
realm.run([kadminl, 'cpw', '-randkey', '-e', 'aes128-cts', 'server'])

success('Policy tests')
