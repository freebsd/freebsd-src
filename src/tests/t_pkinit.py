#!/usr/bin/python
from k5test import *

# Skip this test if pkinit wasn't built.
if not os.path.exists(os.path.join(plugins, 'preauth', 'pkinit.so')):
    skip_rest('PKINIT tests', 'PKINIT module not built')

# Check if soft-pkcs11.so is available.
try:
    import ctypes
    lib = ctypes.LibraryLoader(ctypes.CDLL).LoadLibrary('soft-pkcs11.so')
    del lib
    have_soft_pkcs11 = True
except:
    have_soft_pkcs11 = False

# Construct a krb5.conf fragment configuring pkinit.
certs = os.path.join(srctop, 'tests', 'dejagnu', 'pkinit-certs')
ca_pem = os.path.join(certs, 'ca.pem')
kdc_pem = os.path.join(certs, 'kdc.pem')
user_pem = os.path.join(certs, 'user.pem')
privkey_pem = os.path.join(certs, 'privkey.pem')
privkey_enc_pem = os.path.join(certs, 'privkey-enc.pem')
user_p12 = os.path.join(certs, 'user.p12')
user_enc_p12 = os.path.join(certs, 'user-enc.p12')
user_upn_p12 = os.path.join(certs, 'user-upn.p12')
user_upn2_p12 = os.path.join(certs, 'user-upn2.p12')
user_upn3_p12 = os.path.join(certs, 'user-upn3.p12')
generic_p12 = os.path.join(certs, 'generic.p12')
path = os.path.join(os.getcwd(), 'testdir', 'tmp-pkinit-certs')
path_enc = os.path.join(os.getcwd(), 'testdir', 'tmp-pkinit-certs-enc')

pkinit_krb5_conf = {'realms': {'$realm': {
            'pkinit_anchors': 'FILE:%s' % ca_pem}}}
pkinit_kdc_conf = {'realms': {'$realm': {
            'default_principal_flags': '+preauth',
            'pkinit_eku_checking': 'none',
            'pkinit_identity': 'FILE:%s,%s' % (kdc_pem, privkey_pem),
            'pkinit_indicator': ['indpkinit1', 'indpkinit2']}}}
restrictive_kdc_conf = {'realms': {'$realm': {
            'restrict_anonymous_to_tgt': 'true' }}}

testprincs = {'krbtgt/KRBTEST.COM': {'keys': 'aes128-cts'},
              'user': {'keys': 'aes128-cts', 'flags': '+preauth'},
              'user2': {'keys': 'aes128-cts', 'flags': '+preauth'}}
alias_kdc_conf = {'realms': {'$realm': {
            'default_principal_flags': '+preauth',
            'pkinit_eku_checking': 'none',
            'pkinit_allow_upn': 'true',
            'pkinit_identity': 'FILE:%s,%s' % (kdc_pem, privkey_pem),
            'database_module': 'test'}},
                  'dbmodules': {'test': {
                      'db_library': 'test',
                      'alias': {'user@krbtest.com': 'user'},
                      'princs': testprincs}}}

file_identity = 'FILE:%s,%s' % (user_pem, privkey_pem)
file_enc_identity = 'FILE:%s,%s' % (user_pem, privkey_enc_pem)
dir_identity = 'DIR:%s' % path
dir_enc_identity = 'DIR:%s' % path_enc
dir_file_identity = 'FILE:%s,%s' % (os.path.join(path, 'user.crt'),
                                    os.path.join(path, 'user.key'))
dir_file_enc_identity = 'FILE:%s,%s' % (os.path.join(path_enc, 'user.crt'),
                                        os.path.join(path_enc, 'user.key'))
p12_identity = 'PKCS12:%s' % user_p12
p12_upn_identity = 'PKCS12:%s' % user_upn_p12
p12_upn2_identity = 'PKCS12:%s' % user_upn2_p12
p12_upn3_identity = 'PKCS12:%s' % user_upn3_p12
p12_generic_identity = 'PKCS12:%s' % generic_p12
p12_enc_identity = 'PKCS12:%s' % user_enc_p12
p11_identity = 'PKCS11:soft-pkcs11.so'
p11_token_identity = ('PKCS11:module_name=soft-pkcs11.so:'
                      'slotid=1:token=SoftToken (token)')

# Start a realm with the test kdb module for the following UPN SAN tests.
realm = K5Realm(krb5_conf=pkinit_krb5_conf, kdc_conf=alias_kdc_conf,
                create_kdb=False)
realm.start_kdc()

# Compatibility check: cert contains UPN "user", which matches the
# request principal user@KRBTEST.COM if parsed as a normal principal.
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % p12_upn2_identity])

# Compatibility check: cert contains UPN "user@KRBTEST.COM", which matches
# the request principal user@KRBTEST.COM if parsed as a normal principal.
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % p12_upn3_identity])

# Cert contains UPN "user@krbtest.com" which is aliased to the request
# principal.
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % p12_upn_identity])

# Test an id-pkinit-san match to a post-canonical principal.
realm.kinit('user@krbtest.com',
            flags=['-E', '-X', 'X509_user_identity=%s' % p12_identity])

# Test a UPN match to a post-canonical principal.  (This only works
# for the cert with the UPN containing just "user", as we don't allow
# UPN reparsing when comparing to the canonicalized client principal.)
realm.kinit('user@krbtest.com',
            flags=['-E', '-X', 'X509_user_identity=%s' % p12_upn2_identity])

# Test a mismatch.
msg = 'kinit: Client name mismatch while getting initial credentials'
realm.run([kinit, '-X', 'X509_user_identity=%s' % p12_upn2_identity, 'user2'],
          expected_code=1, expected_msg=msg)
realm.stop()

realm = K5Realm(krb5_conf=pkinit_krb5_conf, kdc_conf=pkinit_kdc_conf,
                get_creds=False)

# Sanity check - password-based preauth should still work.
realm.run(['./responder', '-r', 'password=%s' % password('user'),
           realm.user_princ])
realm.kinit(realm.user_princ, password=password('user'))
realm.klist(realm.user_princ)
realm.run([kvno, realm.host_princ])

# Test anonymous PKINIT.
realm.kinit('@%s' % realm.realm, flags=['-n'], expected_code=1,
            expected_msg='not found in Kerberos database')
realm.addprinc('WELLKNOWN/ANONYMOUS')
realm.kinit('@%s' % realm.realm, flags=['-n'])
realm.klist('WELLKNOWN/ANONYMOUS@WELLKNOWN:ANONYMOUS')
realm.run([kvno, realm.host_princ])
out = realm.run(['./adata', realm.host_princ])
if '97:' in out:
    fail('auth indicators seen in anonymous PKINIT ticket')

# Test anonymous kadmin.
f = open(os.path.join(realm.testdir, 'acl'), 'a')
f.write('WELLKNOWN/ANONYMOUS@WELLKNOWN:ANONYMOUS a *')
f.close()
realm.start_kadmind()
realm.run([kadmin, '-n', 'addprinc', '-pw', 'test', 'testadd'])
realm.run([kadmin, '-n', 'getprinc', 'testadd'], expected_code=1,
          expected_msg="Operation requires ``get'' privilege")
realm.stop_kadmind()

# Test with anonymous restricted; FAST should work but kvno should fail.
r_env = realm.special_env('restrict', True, kdc_conf=restrictive_kdc_conf)
realm.stop_kdc()
realm.start_kdc(env=r_env)
realm.kinit('@%s' % realm.realm, flags=['-n'])
realm.kinit('@%s' % realm.realm, flags=['-n', '-T', realm.ccache])
realm.run([kvno, realm.host_princ], expected_code=1,
          expected_msg='KDC policy rejects request')

# Regression test for #8458: S4U2Self requests crash the KDC if
# anonymous is restricted.
realm.kinit(realm.host_princ, flags=['-k'])
realm.run([kvno, '-U', 'user', realm.host_princ])

# Go back to a normal KDC and disable anonymous PKINIT.
realm.stop_kdc()
realm.start_kdc()
realm.run([kadminl, 'delprinc', 'WELLKNOWN/ANONYMOUS'])

# Run the basic test - PKINIT with FILE: identity, with no password on the key.
realm.run(['./responder', '-x', 'pkinit=',
           '-X', 'X509_user_identity=%s' % file_identity, realm.user_princ])
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % file_identity])
realm.klist(realm.user_princ)
realm.run([kvno, realm.host_princ])

# Try again using RSA instead of DH.
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % file_identity,
                   '-X', 'flag_RSA_PROTOCOL=yes'])
realm.klist(realm.user_princ)

# Test a DH parameter renegotiation by temporarily setting a 4096-bit
# minimum on the KDC.  (Preauth type 16 is PKINIT PA_PK_AS_REQ;
# 109 is PKINIT TD_DH_PARAMETERS; 133 is FAST PA-FX-COOKIE.)
minbits_kdc_conf = {'realms': {'$realm': {'pkinit_dh_min_bits': '4096'}}}
minbits_env = realm.special_env('restrict', True, kdc_conf=minbits_kdc_conf)
realm.stop_kdc()
realm.start_kdc(env=minbits_env)
expected_trace = ('Sending unauthenticated request',
                  '/Additional pre-authentication required',
                  'Preauthenticating using KDC method data',
                  'Preauth module pkinit (16) (real) returned: 0/Success',
                  'Produced preauth for next request: 133, 16',
                  '/Key parameters not accepted',
                  'Preauth tryagain input types (16): 109, 133',
                  'trying again with KDC-provided parameters',
                  'Preauth module pkinit (16) tryagain returned: 0/Success',
                  'Followup preauth for next request: 16, 133')
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % file_identity],
            expected_trace=expected_trace)
realm.stop_kdc()
realm.start_kdc()

# Run the basic test - PKINIT with FILE: identity, with a password on the key,
# supplied by the prompter.
# Expect failure if the responder does nothing, and we have no prompter.
realm.run(['./responder', '-x', 'pkinit={"%s": 0}' % file_enc_identity,
          '-X', 'X509_user_identity=%s' % file_enc_identity, realm.user_princ],
          expected_code=2)
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % file_enc_identity],
            password='encrypted')
realm.klist(realm.user_princ)
realm.run([kvno, realm.host_princ])
realm.run(['./adata', realm.host_princ],
          expected_msg='+97: [indpkinit1, indpkinit2]')

# Run the basic test - PKINIT with FILE: identity, with a password on the key,
# supplied by the responder.
# Supply the response in raw form.
realm.run(['./responder', '-x', 'pkinit={"%s": 0}' % file_enc_identity,
           '-r', 'pkinit={"%s": "encrypted"}' % file_enc_identity,
           '-X', 'X509_user_identity=%s' % file_enc_identity,
           realm.user_princ])
# Supply the response through the convenience API.
realm.run(['./responder', '-X', 'X509_user_identity=%s' % file_enc_identity,
           '-p', '%s=%s' % (file_enc_identity, 'encrypted'), realm.user_princ])
realm.klist(realm.user_princ)
realm.run([kvno, realm.host_princ])

# PKINIT with DIR: identity, with no password on the key.
os.mkdir(path)
os.mkdir(path_enc)
shutil.copy(privkey_pem, os.path.join(path, 'user.key'))
shutil.copy(privkey_enc_pem, os.path.join(path_enc, 'user.key'))
shutil.copy(user_pem, os.path.join(path, 'user.crt'))
shutil.copy(user_pem, os.path.join(path_enc, 'user.crt'))
realm.run(['./responder', '-x', 'pkinit=', '-X',
           'X509_user_identity=%s' % dir_identity, realm.user_princ])
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % dir_identity])
realm.klist(realm.user_princ)
realm.run([kvno, realm.host_princ])

# PKINIT with DIR: identity, with a password on the key, supplied by the
# prompter.
# Expect failure if the responder does nothing, and we have no prompter.
realm.run(['./responder', '-x', 'pkinit={"%s": 0}' % dir_file_enc_identity,
           '-X', 'X509_user_identity=%s' % dir_enc_identity, realm.user_princ],
           expected_code=2)
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % dir_enc_identity],
            password='encrypted')
realm.klist(realm.user_princ)
realm.run([kvno, realm.host_princ])

# PKINIT with DIR: identity, with a password on the key, supplied by the
# responder.
# Supply the response in raw form.
realm.run(['./responder', '-x', 'pkinit={"%s": 0}' % dir_file_enc_identity,
           '-r', 'pkinit={"%s": "encrypted"}' % dir_file_enc_identity,
           '-X', 'X509_user_identity=%s' % dir_enc_identity, realm.user_princ])
# Supply the response through the convenience API.
realm.run(['./responder', '-X', 'X509_user_identity=%s' % dir_enc_identity,
           '-p', '%s=%s' % (dir_file_enc_identity, 'encrypted'),
           realm.user_princ])
realm.klist(realm.user_princ)
realm.run([kvno, realm.host_princ])

# PKINIT with PKCS12: identity, with no password on the bundle.
realm.run(['./responder', '-x', 'pkinit=',
           '-X', 'X509_user_identity=%s' % p12_identity, realm.user_princ])
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % p12_identity])
realm.klist(realm.user_princ)
realm.run([kvno, realm.host_princ])

# PKINIT with PKCS12: identity, with a password on the bundle, supplied by the
# prompter.
# Expect failure if the responder does nothing, and we have no prompter.
realm.run(['./responder', '-x', 'pkinit={"%s": 0}' % p12_enc_identity,
           '-X', 'X509_user_identity=%s' % p12_enc_identity, realm.user_princ],
           expected_code=2)
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % p12_enc_identity],
            password='encrypted')
realm.klist(realm.user_princ)
realm.run([kvno, realm.host_princ])

# PKINIT with PKCS12: identity, with a password on the bundle, supplied by the
# responder.
# Supply the response in raw form.
realm.run(['./responder', '-x', 'pkinit={"%s": 0}' % p12_enc_identity,
           '-r', 'pkinit={"%s": "encrypted"}' % p12_enc_identity,
           '-X', 'X509_user_identity=%s' % p12_enc_identity, realm.user_princ])
# Supply the response through the convenience API.
realm.run(['./responder', '-X', 'X509_user_identity=%s' % p12_enc_identity,
           '-p', '%s=%s' % (p12_enc_identity, 'encrypted'),
           realm.user_princ])
realm.klist(realm.user_princ)
realm.run([kvno, realm.host_princ])

# Match a single rule.
rule = '<SAN>^user@KRBTEST.COM$'
realm.run([kadminl, 'setstr', realm.user_princ, 'pkinit_cert_match', rule])
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % p12_identity])
realm.klist(realm.user_princ)

# Match a combined rule (default prefix is &&).
rule = '<SUBJECT>CN=user$<KU>digitalSignature,keyEncipherment'
realm.run([kadminl, 'setstr', realm.user_princ, 'pkinit_cert_match', rule])
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % p12_identity])
realm.klist(realm.user_princ)

# Fail an && rule.
rule = '&&<SUBJECT>O=OTHER.COM<SAN>^user@KRBTEST.COM$'
realm.run([kadminl, 'setstr', realm.user_princ, 'pkinit_cert_match', rule])
msg = 'kinit: Certificate mismatch while getting initial credentials'
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % p12_identity],
            expected_code=1, expected_msg=msg)

# Pass an || rule.
rule = '||<SUBJECT>O=KRBTEST.COM<SAN>^otheruser@KRBTEST.COM$'
realm.run([kadminl, 'setstr', realm.user_princ, 'pkinit_cert_match', rule])
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % p12_identity])
realm.klist(realm.user_princ)

# Fail an || rule.
rule = '||<SUBJECT>O=OTHER.COM<SAN>^otheruser@KRBTEST.COM$'
realm.run([kadminl, 'setstr', realm.user_princ, 'pkinit_cert_match', rule])
msg = 'kinit: Certificate mismatch while getting initial credentials'
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % p12_identity],
            expected_code=1, expected_msg=msg)

# Authorize a client cert with no PKINIT extensions using subject and
# issuer.  (Relies on EKU checking being turned off.)
rule = '&&<SUBJECT>CN=user$<ISSUER>O=MIT,'
realm.run([kadminl, 'setstr', realm.user_princ, 'pkinit_cert_match', rule])
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % p12_generic_identity])
realm.klist(realm.user_princ)

if not have_soft_pkcs11:
    skip_rest('PKINIT PKCS11 tests', 'soft-pkcs11.so not found')

softpkcs11rc = os.path.join(os.getcwd(), 'testdir', 'soft-pkcs11.rc')
realm.env['SOFTPKCS11RC'] = softpkcs11rc

# PKINIT with PKCS11: identity, with no need for a PIN.
conf = open(softpkcs11rc, 'w')
conf.write("%s\t%s\t%s\t%s\n" % ('user', 'user token', user_pem, privkey_pem))
conf.close()
# Expect to succeed without having to supply any more information.
realm.run(['./responder', '-x', 'pkinit=',
           '-X', 'X509_user_identity=%s' % p11_identity, realm.user_princ])
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % p11_identity])
realm.klist(realm.user_princ)
realm.run([kvno, realm.host_princ])

# PKINIT with PKCS11: identity, with a PIN supplied by the prompter.
os.remove(softpkcs11rc)
conf = open(softpkcs11rc, 'w')
conf.write("%s\t%s\t%s\t%s\n" % ('user', 'user token', user_pem,
                                 privkey_enc_pem))
conf.close()
# Expect failure if the responder does nothing, and there's no prompter
realm.run(['./responder', '-x', 'pkinit={"%s": 0}' % p11_token_identity,
           '-X', 'X509_user_identity=%s' % p11_identity, realm.user_princ],
          expected_code=2)
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % p11_identity],
            password='encrypted')
realm.klist(realm.user_princ)
realm.run([kvno, realm.host_princ])

# Supply the wrong PIN, and verify that we ignore the draft9 padata offer
# in the KDC method data after RFC 4556 PKINIT fails.
expected_trace = ('PKINIT client has no configured identity; giving up',
                  'PKINIT client ignoring draft 9 offer from RFC 4556 KDC')
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % p11_identity],
            password='wrong', expected_code=1, expected_trace=expected_trace)

# PKINIT with PKCS11: identity, with a PIN supplied by the responder.
# Supply the response in raw form.
realm.run(['./responder', '-x', 'pkinit={"%s": 0}' % p11_token_identity,
           '-r', 'pkinit={"%s": "encrypted"}' % p11_token_identity,
           '-X', 'X509_user_identity=%s' % p11_identity, realm.user_princ])
# Supply the response through the convenience API.
realm.run(['./responder', '-X', 'X509_user_identity=%s' % p11_identity,
           '-p', '%s=%s' % (p11_token_identity, 'encrypted'),
           realm.user_princ])
realm.klist(realm.user_princ)
realm.run([kvno, realm.host_princ])

success('PKINIT tests')
