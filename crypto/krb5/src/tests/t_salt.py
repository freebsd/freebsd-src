from k5test import *
import re

realm = K5Realm(create_user=False)

# Check that a non-default salt type applies only to the key it is
# matched with and not to subsequent keys.  e1 and e2 are enctypes,
# and salt is a non-default salt type.
def test_salt(realm, e1, salt, e2):
    keysalts = e1 + ':' + salt + ',' + e2
    realm.run([kadminl, 'ank', '-e', keysalts, '-pw', 'password', 'user'])
    out = realm.run([kadminl, 'getprinc', 'user'])
    if len(re.findall(':' + salt, out)) != 1:
        fail(salt + ' present in second enctype or not present')
    realm.run([kadminl, 'delprinc', 'user'])

# Enctype/salt pairs chosen with non-default salt types.
# The enctypes are mostly arbitrary.
salts = [('des3-cbc-sha1', 'norealm'),
         ('arcfour-hmac', 'onlyrealm'),
         ('aes128-cts-hmac-sha1-96', 'special')]
# These enctypes are chosen to cover the different string-to-key routines.
# Omit ":normal" from aes256 to check that salttype defaulting works.
second_kstypes = ['aes256-cts-hmac-sha1-96', 'arcfour-hmac:normal',
                  'des3-cbc-sha1:normal']

# Test using different salt types in a principal's key list.
# Parameters from one key in the list must not leak over to later ones.
for e1, string in salts:
    for e2 in second_kstypes:
        test_salt(realm, e1, string, e2)

def test_dup(realm, ks):
    realm.run([kadminl, 'ank', '-e', ks, '-pw', 'password', 'ks_princ'])
    out = realm.run([kadminl, 'getprinc', 'ks_princ'])
    lines = out.split('\n')
    keys = [l for l in lines if 'Key: ' in l]
    uniq = set(keys)
    # 'Key:' matches 'MKey:' as well so len(keys) has one extra
    if (len(uniq) != len(keys)) or len(keys) > len(ks.split(',')):
        fail('Duplicate keysalt detection failed for keysalt ' + ks)
    realm.run([kadminl, 'delprinc', 'ks_princ'])

# All in-tree callers request duplicate suppression from
# krb5_string_to_keysalts(); we should check that it works, respects
# aliases, and doesn't result in an infinite loop.
dup_kstypes = ['arcfour-hmac-md5:normal,rc4-hmac:normal',
               'aes256-cts-hmac-sha1-96:normal,aes128-cts,aes256-cts',
               'aes256-cts-hmac-sha1-96:normal,aes256-cts:special,' +
               'aes256-cts-hmac-sha1-96:normal']

for ks in dup_kstypes:
    test_dup(realm, ks)

success("Salt types")
