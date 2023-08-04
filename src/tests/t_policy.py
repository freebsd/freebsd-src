from k5test import *
import re

realm = K5Realm(create_host=False, start_kadmind=True)

# Test password quality enforcement.
mark('password quality')
realm.run([kadminl, 'addpol', '-minlength', '6', '-minclasses', '2', 'pwpol'])
realm.run([kadminl, 'addprinc', '-randkey', '-policy', 'pwpol', 'pwuser'])
realm.run([kadminl, 'cpw', '-pw', 'sh0rt', 'pwuser'], expected_code=1,
          expected_msg='Password is too short')
realm.run([kadminl, 'cpw', '-pw', 'longenough', 'pwuser'], expected_code=1,
          expected_msg='Password does not contain enough character classes')
realm.run([kadminl, 'cpw', '-pw', 'l0ngenough', 'pwuser'])

# Test some password history enforcement.  Even with no history value,
# the current password should be denied.
mark('password history')
realm.run([kadminl, 'cpw', '-pw', 'l0ngenough', 'pwuser'], expected_code=1,
          expected_msg='Cannot reuse password')
realm.run([kadminl, 'modpol', '-history', '2', 'pwpol'])
realm.run([kadminl, 'cpw', '-pw', 'an0therpw', 'pwuser'])
realm.run([kadminl, 'cpw', '-pw', 'l0ngenough', 'pwuser'], expected_code=1,
          expected_msg='Cannot reuse password')
realm.run([kadminl, 'cpw', '-pw', '3rdpassword', 'pwuser'])
realm.run([kadminl, 'cpw', '-pw', 'l0ngenough', 'pwuser'])

# Regression test for #929 (kadmind crash with more historical
# passwords in a principal entry than current policy history setting).
mark('password history (policy value reduced below current array size)')
realm.run([kadminl, 'addpol', '-history', '5', 'histpol'])
realm.addprinc('histprinc', 'first')
realm.run([kadminl, 'modprinc', '-policy', 'histpol', 'histprinc'])
realm.run([kadminl, 'cpw', '-pw', 'second', 'histprinc'])
realm.run([kadminl, 'cpw', '-pw', 'third', 'histprinc'])
realm.run([kadminl, 'cpw', '-pw', 'fourth', 'histprinc'])
realm.run([kadminl, 'modpol', '-history', '3', 'histpol'])
realm.run([kadminl, 'cpw', '-pw', 'fifth', 'histprinc'])
realm.run([kadminl, 'delprinc', 'histprinc'])

# Regression test for #2841 (heap buffer overflow when policy history
# value is reduced to match the number of historical passwords for a
# principal).
mark('password history (policy value reduced to current array size)')
def histfail(*pwlist):
    for pw in pwlist:
        realm.run([kadminl, 'cpw', '-pw', pw, 'histprinc'], expected_code=1,
                  expected_msg='Cannot reuse password')
realm.run([kadminl, 'modpol', '-history', '3', 'histpol'])
realm.addprinc('histprinc', '1111')
realm.run([kadminl, 'modprinc', '-policy', 'histpol', 'histprinc'])
realm.run([kadminl, 'cpw', '-pw', '2222', 'histprinc'])
histfail('2222', '1111')
realm.run([kadminl, 'modpol', '-history', '2', 'histpol'])
realm.run([kadminl, 'cpw', '-pw', '3333', 'histprinc'])

# Test that the history array is properly resized if the policy
# history value is increased after the array is filled.
mark('password history (policy value increase)')
realm.run([kadminl, 'delprinc', 'histprinc'])
realm.addprinc('histprinc', '1111')
realm.run([kadminl, 'modprinc', '-policy', 'histpol', 'histprinc'])
realm.run([kadminl, 'cpw', '-pw', '2222', 'histprinc'])
histfail('2222', '1111')
realm.run([kadminl, 'cpw', '-pw', '2222', 'histprinc'], expected_code=1,
          expected_msg='Cannot reuse password')
realm.run([kadminl, 'cpw', '-pw', '1111', 'histprinc'], expected_code=1,
          expected_msg='Cannot reuse password')
realm.run([kadminl, 'modpol', '-history', '3', 'histpol'])
realm.run([kadminl, 'cpw', '-pw', '3333', 'histprinc'])
histfail('3333', '2222', '1111')
realm.run([kadminl, 'modpol', '-history', '4', 'histpol'])
histfail('3333', '2222', '1111')
realm.run([kadminl, 'cpw', '-pw', '4444', 'histprinc'])
histfail('4444', '3333', '2222', '1111')

# Test that when the policy history value is reduced, all currently
# known old passwords still fail until the next password change, after
# which the new number of old passwords fails (but no more).
mark('password history (policy value reduction)')
realm.run([kadminl, 'modpol', '-history', '3', 'histpol'])
histfail('4444', '3333', '2222', '1111')
realm.run([kadminl, 'cpw', '-pw', '5555', 'histprinc'])
histfail('5555', '3333', '3333')
realm.run([kadminl, 'cpw', '-pw', '2222', 'histprinc'])
realm.run([kadminl, 'modpol', '-history', '2', 'histpol'])
histfail('2222', '5555', '4444')
realm.run([kadminl, 'cpw', '-pw', '3333', 'histprinc'])

# Test references to nonexistent policies.
mark('nonexistent policy references')
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
mark('create referenced policy')
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
mark('delete referenced policy')
realm.run([kadminl, 'delpol', 'newpol'])
realm.run([kadminl, 'getpol', 'newpol'], expected_code=1,
          expected_msg='Policy does not exist')
realm.run([kadminl, 'cpw', '-pw', 'aa', 'pwuser'])

# Test basic password lockout support.
mark('password lockout')
realm.stop()
for realm in multidb_realms(create_host=False):
    realm.run([kadminl, 'addpol', '-maxfailure', '2', '-failurecountinterval',
               '5m', 'lockout'])
    realm.run([kadminl, 'modprinc', '+requires_preauth', '-policy', 'lockout',
               'user'])

    # kinit twice with the wrong password.
    msg = 'Password incorrect while getting initial credentials'
    realm.run([kinit, realm.user_princ], input='wrong\n', expected_code=1,
              expected_msg=msg)
    realm.run([kinit, realm.user_princ], input='wrong\n', expected_code=1,
              expected_msg=msg)

    # Now the account should be locked out.
    msg = 'credentials have been revoked while getting initial credentials'
    realm.run([kinit, realm.user_princ], expected_code=1, expected_msg=msg)

    # Check that modprinc -unlock allows a further attempt.
    realm.run([kadminl, 'modprinc', '-unlock', 'user'])
    realm.kinit(realm.user_princ, password('user'))

    # Make sure a nonexistent policy reference doesn't prevent authentication.
    realm.run([kadminl, 'delpol', 'lockout'])
    realm.kinit(realm.user_princ, password('user'))

# Regression test for issue #7099: databases created prior to krb5 1.3 have
# multiple history keys, and kadmin prior to 1.7 didn't necessarily use the
# first one to create history entries.
mark('#7099 regression test')
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
mark('allowedkeysalts')

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
