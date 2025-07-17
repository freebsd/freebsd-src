from k5test import *

# Skip this test if pkinit wasn't built.
if not pkinit_enabled:
    skip_rest('PKINIT tests', 'PKINIT module not built')

soft_pkcs11 = os.path.join(buildtop, 'tests', 'softpkcs11', 'softpkcs11.so')

# Construct a krb5.conf fragment configuring pkinit.
user_pem = os.path.join(pkinit_certs, 'user.pem')
privkey_pem = os.path.join(pkinit_certs, 'privkey.pem')
privkey_enc_pem = os.path.join(pkinit_certs, 'privkey-enc.pem')
user_p12 = os.path.join(pkinit_certs, 'user.p12')
user_enc_p12 = os.path.join(pkinit_certs, 'user-enc.p12')
user_upn_p12 = os.path.join(pkinit_certs, 'user-upn.p12')
user_upn2_p12 = os.path.join(pkinit_certs, 'user-upn2.p12')
user_upn3_p12 = os.path.join(pkinit_certs, 'user-upn3.p12')
generic_p12 = os.path.join(pkinit_certs, 'generic.p12')
path = os.path.join(os.getcwd(), 'testdir', 'tmp-pkinit-certs')
path_enc = os.path.join(os.getcwd(), 'testdir', 'tmp-pkinit-certs-enc')

pkinit_kdc_conf = {'realms': {'$realm': {
            'default_principal_flags': '+preauth',
            'pkinit_eku_checking': 'none',
            'pkinit_indicator': ['indpkinit1', 'indpkinit2']}}}
restrictive_kdc_conf = {'realms': {'$realm': {
            'restrict_anonymous_to_tgt': 'true' }}}
freshness_kdc_conf = {'realms': {'$realm': {
            'pkinit_require_freshness': 'true'}}}

testprincs = {'krbtgt/KRBTEST.COM': {'keys': 'aes128-cts'},
              'user': {'keys': 'aes128-cts', 'flags': '+preauth'},
              'user2': {'keys': 'aes128-cts', 'flags': '+preauth'}}
alias_kdc_conf = {'realms': {'$realm': {
            'default_principal_flags': '+preauth',
            'pkinit_eku_checking': 'none',
            'pkinit_allow_upn': 'true',
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
p11_identity = 'PKCS11:' + soft_pkcs11
p11_token_identity = ('PKCS11:module_name=' + soft_pkcs11 +
                      ':slotid=1:token=SoftToken (token)')

# Start a realm with the test kdb module for the following UPN SAN tests.
realm = K5Realm(kdc_conf=alias_kdc_conf, create_kdb=False, pkinit=True)
realm.start_kdc()

mark('UPN SANs')

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

realm = K5Realm(kdc_conf=pkinit_kdc_conf, get_creds=False, pkinit=True)

# Sanity check - password-based preauth should still work.
mark('password preauth sanity check')
realm.run(['./responder', '-r', 'password=%s' % password('user'),
           realm.user_princ])
realm.kinit(realm.user_princ, password=password('user'))
realm.klist(realm.user_princ)
realm.run([kvno, realm.host_princ])

# Having tested password preauth, remove the keys for better error
# reporting.
realm.run([kadminl, 'purgekeys', '-all', realm.user_princ])

# Test anonymous PKINIT.
mark('anonymous')
realm.kinit('@%s' % realm.realm, flags=['-n'], expected_code=1,
            expected_msg='not found in Kerberos database')
realm.addprinc('WELLKNOWN/ANONYMOUS')
realm.kinit('@%s' % realm.realm, flags=['-n'])
realm.klist('WELLKNOWN/ANONYMOUS@WELLKNOWN:ANONYMOUS')
realm.run([kvno, realm.host_princ])
out = realm.run(['./adata', realm.host_princ])
if '97:' in out:
    fail('auth indicators seen in anonymous PKINIT ticket')
# Verify start_realm setting and test referrals TGS request.
realm.run([klist, '-C'], expected_msg='start_realm = KRBTEST.COM')
realm.run([kvno, '-S', 'host', hostname])

# Test anonymous kadmin.
mark('anonymous kadmin')
f = open(os.path.join(realm.testdir, 'acl'), 'a')
f.write('WELLKNOWN/ANONYMOUS@WELLKNOWN:ANONYMOUS a *')
f.close()
realm.start_kadmind()
realm.run([kadmin, '-n', 'addprinc', '-pw', 'test', 'testadd'])
realm.run([kadmin, '-n', 'getprinc', 'testadd'], expected_code=1,
          expected_msg="Operation requires ``get'' privilege")
realm.stop_kadmind()

# Test with anonymous restricted; FAST should work but kvno should fail.
mark('anonymous restricted')
r_env = realm.special_env('restrict', True, kdc_conf=restrictive_kdc_conf)
realm.stop_kdc()
realm.start_kdc(env=r_env)
realm.kinit('@%s' % realm.realm, flags=['-n'])
realm.kinit('@%s' % realm.realm, flags=['-n', '-T', realm.ccache])
realm.run([kvno, realm.host_princ], expected_code=1,
          expected_msg='KDC policy rejects request')

# Regression test for #8458: S4U2Self requests crash the KDC if
# anonymous is restricted.
mark('#8458 regression test')
realm.kinit(realm.host_princ, flags=['-k'])
realm.run([kvno, '-U', 'user', realm.host_princ])

# Go back to the normal KDC environment.
realm.stop_kdc()
realm.start_kdc()

# Run the basic test - PKINIT with FILE: identity, with no password on the key.
mark('FILE identity, no password')
msgs = ('Sending unauthenticated request',
        '/Additional pre-authentication required',
        'Preauthenticating using KDC method data',
        'PKINIT client received freshness token from KDC',
        'PKINIT loading CA certs and CRLs from FILE',
        'PKINIT client making DH request',
        ' preauth for next request: PA-FX-COOKIE (133), PA-PK-AS-REQ (16)',
        'PKINIT client verified DH reply',
        'PKINIT client found id-pkinit-san in KDC cert',
        'PKINIT client matched KDC principal krbtgt/')
realm.pkinit(realm.user_princ, expected_trace=msgs)
realm.klist(realm.user_princ)
realm.run([kvno, realm.host_princ])

# Try using multiple configured pkinit_identities, to make sure we
# fall back to the second one when the first one cannot be read.
id_conf = {'realms': {'$realm': {'pkinit_identities': [file_identity + 'X',
                                                       file_identity]}}}
id_env = realm.special_env('idconf', False, krb5_conf=id_conf)
realm.kinit(realm.user_princ, expected_trace=msgs, env=id_env)

# Try again using RSA instead of DH.
mark('FILE identity, no password, RSA')
realm.pkinit(realm.user_princ, flags=['-X', 'flag_RSA_PROTOCOL=yes'],
             expected_trace=('PKINIT client making RSA request',
                             'PKINIT client verified RSA reply'))
realm.klist(realm.user_princ)

# Test a DH parameter renegotiation by temporarily setting a 4096-bit
# minimum on the KDC.  (Preauth type 16 is PKINIT PA_PK_AS_REQ;
# 109 is PKINIT TD_DH_PARAMETERS; 133 is FAST PA-FX-COOKIE.)
mark('DH parameter renegotiation')
minbits_kdc_conf = {'realms': {'$realm': {'pkinit_dh_min_bits': '4096'}}}
minbits_env = realm.special_env('restrict', True, kdc_conf=minbits_kdc_conf)
realm.stop_kdc()
realm.start_kdc(env=minbits_env)
msgs = ('Sending unauthenticated request',
        '/Additional pre-authentication required',
        'Preauthenticating using KDC method data',
        'Preauth module pkinit (16) (real) returned: 0/Success',
        ' preauth for next request: PA-FX-COOKIE (133), PA-PK-AS-REQ (16)',
        '/Key parameters not accepted',
        'Preauth tryagain input types (16): 109, PA-FX-COOKIE (133)',
        'trying again with KDC-provided parameters',
        'Preauth module pkinit (16) tryagain returned: 0/Success',
        ' preauth for next request: PA-PK-AS-REQ (16), PA-FX-COOKIE (133)')
realm.pkinit(realm.user_princ, expected_trace=msgs)

# Test enforcement of required freshness tokens.  (We can leave
# freshness tokens required after this test.)
mark('freshness token enforcement')
realm.pkinit(realm.user_princ, flags=['-X', 'disable_freshness=yes'])
f_env = realm.special_env('freshness', True, kdc_conf=freshness_kdc_conf)
realm.stop_kdc()
realm.start_kdc(env=f_env)
realm.pkinit(realm.user_princ)
realm.pkinit(realm.user_princ, flags=['-X', 'disable_freshness=yes'],
             expected_code=1, expected_msg='Preauthentication failed')
# Anonymous should never require a freshness token.
realm.kinit('@%s' % realm.realm, flags=['-n', '-X', 'disable_freshness=yes'])

# Run the basic test - PKINIT with FILE: identity, with a password on the key,
# supplied by the prompter.
# Expect failure if the responder does nothing, and we have no prompter.
mark('FILE identity, password on key (prompter)')
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
mark('FILE identity, password on key (responder)')
out = realm.run(['./responder', '-x', 'pkinit={"%s": 0}' % file_enc_identity,
                 '-r', 'pkinit={"%s": "encrypted"}' % file_enc_identity,
                 '-X', 'X509_user_identity=%s' % file_enc_identity,
                 realm.user_princ])
# Regression test for #8885 (password question asked twice).
if out.count('OK: ') != 1:
    fail('Wrong number of responder calls')
# Supply the response through the convenience API.
realm.run(['./responder', '-X', 'X509_user_identity=%s' % file_enc_identity,
           '-p', '%s=%s' % (file_enc_identity, 'encrypted'), realm.user_princ])
realm.klist(realm.user_princ)
realm.run([kvno, realm.host_princ])

# PKINIT with DIR: identity, with no password on the key.
mark('DIR identity, no password')
os.mkdir(path)
os.mkdir(path_enc)
shutil.copy(privkey_pem, os.path.join(path, 'user.key'))
shutil.copy(privkey_enc_pem, os.path.join(path_enc, 'user.key'))
shutil.copy(user_pem, os.path.join(path, 'user.crt'))
shutil.copy(user_pem, os.path.join(path_enc, 'user.crt'))
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % dir_identity])
realm.klist(realm.user_princ)
realm.run([kvno, realm.host_princ])

# PKINIT with DIR: identity, with a password on the key, supplied by the
# prompter.
# Expect failure if the responder does nothing, and we have no prompter.
mark('DIR identity, password on key (prompter)')
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
mark('DIR identity, password on key (responder)')
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
mark('PKCS12 identity, no password')
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % p12_identity])
realm.klist(realm.user_princ)
realm.run([kvno, realm.host_princ])

# PKINIT with PKCS12: identity, with a password on the bundle, supplied by the
# prompter.
# Expect failure if the responder does nothing, and we have no prompter.
mark('PKCS12 identity, password on bundle (prompter)')
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
mark('PKCS12 identity, password on bundle (responder)')
realm.run(['./responder', '-x', 'pkinit={"%s": 0}' % p12_enc_identity,
           '-r', 'pkinit={"%s": "encrypted"}' % p12_enc_identity,
           '-X', 'X509_user_identity=%s' % p12_enc_identity, realm.user_princ])
# Supply the response through the convenience API.
realm.run(['./responder', '-X', 'X509_user_identity=%s' % p12_enc_identity,
           '-p', '%s=%s' % (p12_enc_identity, 'encrypted'),
           realm.user_princ])
realm.klist(realm.user_princ)
realm.run([kvno, realm.host_princ])

mark('pkinit_cert_match rules')

# Match a single rule.
rule = '<SAN>^user@KRBTEST.COM$'
realm.run([kadminl, 'setstr', realm.user_princ, 'pkinit_cert_match', rule])
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % p12_identity])
realm.klist(realm.user_princ)

# Regression test for #8670: match a UPN SAN with a single rule.
rule = '<SAN>^user@krbtest.com$'
realm.run([kadminl, 'setstr', realm.user_princ, 'pkinit_cert_match', rule])
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % p12_upn_identity])
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

# Regression test for #8726: null deref when parsing a FILE residual
# beginning with a comma.
realm.kinit(realm.user_princ, flags=['-X', 'X509_user_identity=,'],
            expected_code=1, expected_msg='Preauthentication failed while')

softpkcs11rc = os.path.join(os.getcwd(), 'testdir', 'soft-pkcs11.rc')
realm.env['SOFTPKCS11RC'] = softpkcs11rc

# PKINIT with PKCS11: identity, with no need for a PIN.
mark('PKCS11 identity, no PIN')
conf = open(softpkcs11rc, 'w')
conf.write("%s\t%s\t%s\t%s\n" % ('user', 'user token', user_pem, privkey_pem))
conf.close()
# Expect to succeed without having to supply any more information.
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % p11_identity])
realm.klist(realm.user_princ)
realm.run([kvno, realm.host_princ])

# PKINIT with PKCS11: identity, with a PIN supplied by the prompter.
mark('PKCS11 identity, with PIN (prompter)')
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

# Supply the wrong PIN.
mark('PKCS11 identity, wrong PIN')
expected_trace = ('PKINIT client has no configured identity; giving up',)
realm.kinit(realm.user_princ,
            flags=['-X', 'X509_user_identity=%s' % p11_identity],
            password='wrong', expected_code=1, expected_trace=expected_trace)

# PKINIT with PKCS11: identity, with a PIN supplied by the responder.
# Supply the response in raw form.
mark('PKCS11 identity, with PIN (responder)')
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
