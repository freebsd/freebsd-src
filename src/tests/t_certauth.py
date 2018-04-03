#!/usr/bin/python
from k5test import *

# Skip this test if pkinit wasn't built.
if not os.path.exists(os.path.join(plugins, 'preauth', 'pkinit.so')):
    skip_rest('certauth tests', 'PKINIT module not built')

certs = os.path.join(srctop, 'tests', 'dejagnu', 'pkinit-certs')
ca_pem = os.path.join(certs, 'ca.pem')
kdc_pem = os.path.join(certs, 'kdc.pem')
privkey_pem = os.path.join(certs, 'privkey.pem')
user_pem = os.path.join(certs, 'user.pem')

modpath = os.path.join(buildtop, 'plugins', 'certauth', 'test',
                       'certauth_test.so')
pkinit_krb5_conf = {'realms': {'$realm': {
            'pkinit_anchors': 'FILE:%s' % ca_pem}},
            'plugins': {'certauth': {'module': ['test1:' + modpath,
                                                'test2:' + modpath],
                                     'enable_only': ['test1', 'test2']}}}
pkinit_kdc_conf = {'realms': {'$realm': {
            'default_principal_flags': '+preauth',
            'pkinit_eku_checking': 'none',
            'pkinit_identity': 'FILE:%s,%s' % (kdc_pem, privkey_pem),
            'pkinit_indicator': ['indpkinit1', 'indpkinit2']}}}

file_identity = 'FILE:%s,%s' % (user_pem, privkey_pem)

realm = K5Realm(krb5_conf=pkinit_krb5_conf, kdc_conf=pkinit_kdc_conf,
                get_creds=False)

# Let the test module match user to CN=user, with indicators.
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % file_identity])
realm.klist(realm.user_princ)
realm.run([kvno, realm.host_princ])
realm.run(['./adata', realm.host_princ],
          expected_msg='+97: [test1, test2, user, indpkinit1, indpkinit2]')

# Let the test module mismatch with user2 to CN=user.
realm.addprinc("user2@KRBTEST.COM")
out = realm.kinit("user2@KRBTEST.COM",
                  flags=['-X', 'X509_user_identity=%s' % file_identity],
                  expected_code=1,
                  expected_msg='kinit: Certificate mismatch')

success("certauth tests")
