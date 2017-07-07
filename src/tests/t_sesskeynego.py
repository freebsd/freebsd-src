#!/usr/bin/python
from k5test import *
import re

# Run "kvno server" with a fresh set of client tickets, then check that the
# enctypes in the service ticket match the expected values.
etypes_re = re.compile(r'server@[^\n]+\n\tEtype \(skey, tkt\): '
                       '([^,]+), ([^\s]+)')
def test_kvno(realm, expected_skey, expected_tkt):
    realm.kinit(realm.user_princ, password('user'))
    realm.run([kvno, 'server'])
    output = realm.run([klist, '-e'])
    m = etypes_re.search(output)
    if not m:
        fail('could not parse etypes from klist -e output')
    skey, tkt = m.groups()
    if skey != expected_skey:
        fail('got session key type %s, expected %s' % (skey, expected_skey))
    if tkt != expected_tkt:
        fail('got ticket key type %s, expected %s' % (tkt, expected_tkt))

conf1 = {'libdefaults': {'default_tgs_enctypes': 'aes128-cts,aes256-cts'}}
conf2 = {'libdefaults': {'default_tgs_enctypes': 'aes256-cts,aes128-cts'}}
conf3 = {'libdefaults': {
        'allow_weak_crypto': 'true',
        'default_tkt_enctypes': 'aes128-cts',
        'default_tgs_enctypes': 'rc4-hmac,aes128-cts,des-cbc-crc'}}
conf4 = {'libdefaults': {
        'allow_weak_crypto': 'true',
        'default_tkt_enctypes': 'aes256-cts',
        'default_tgs_enctypes': 'des-cbc-crc,rc4-hmac,aes256-cts'},
         'realms': {'$realm': {'des_crc_session_supported': 'false'}}}

# Test with client request and session_enctypes preferring aes128, but
# aes256 long-term key.
realm = K5Realm(krb5_conf=conf1, create_host=False, get_creds=False)
realm.run([kadminl, 'addprinc', '-randkey', '-e', 'aes256-cts', 'server'])
realm.run([kadminl, 'setstr', 'server', 'session_enctypes',
           'aes128-cts,aes256-cts'])
test_kvno(realm, 'aes128-cts-hmac-sha1-96', 'aes256-cts-hmac-sha1-96')
realm.stop()

# Second go, almost same as first, but resulting session key must be aes256
# because of the difference in default_tgs_enctypes order.  This tests that
# session_enctypes doesn't change the order in which we negotiate.
realm = K5Realm(krb5_conf=conf2, create_host=False, get_creds=False)
realm.run([kadminl, 'addprinc', '-randkey', '-e', 'aes256-cts', 'server'])
realm.run([kadminl, 'setstr', 'server', 'session_enctypes',
           'aes128-cts,aes256-cts'])
test_kvno(realm, 'aes256-cts-hmac-sha1-96', 'aes256-cts-hmac-sha1-96')
realm.stop()

# Next we use conf3 and try various things.
realm = K5Realm(krb5_conf=conf3, create_host=False, get_creds=False)
realm.run([kadminl, 'addprinc', '-randkey', '-e', 'aes256-cts:normal',
           'server'])

# 3a: Negotiate aes128 session key when principal only has aes256 long-term.
realm.run([kadminl, 'setstr', 'server', 'session_enctypes',
           'aes128-cts,aes256-cts'])
test_kvno(realm, 'aes128-cts-hmac-sha1-96', 'aes256-cts-hmac-sha1-96')

# 3b: Negotiate rc4-hmac session key when principal only has aes256 long-term.
realm.run([kadminl, 'setstr', 'server', 'session_enctypes',
           'rc4-hmac,aes128-cts,aes256-cts'])
test_kvno(realm, 'arcfour-hmac', 'aes256-cts-hmac-sha1-96')

# 3c: Test des-cbc-crc default assumption.
realm.run([kadminl, 'delstr', 'server', 'session_enctypes'])
test_kvno(realm, 'des-cbc-crc', 'aes256-cts-hmac-sha1-96')
realm.stop()

# Last go: test that we can disable the des-cbc-crc assumption
realm = K5Realm(krb5_conf=conf4, get_creds=False)
realm.run([kadminl, 'addprinc', '-randkey', '-e', 'aes256-cts', 'server'])
test_kvno(realm, 'aes256-cts-hmac-sha1-96', 'aes256-cts-hmac-sha1-96')
realm.stop()

success('sesskeynego')
