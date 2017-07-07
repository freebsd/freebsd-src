#!/usr/bin/python
from k5test import *

# Load the sample KDC authdata module.
greet_path = os.path.join(buildtop, 'plugins', 'authdata', 'greet_server',
                          'greet_server.so')
conf = {'plugins': {'kdcauthdata': {'module': 'greet:' + greet_path}}}
realm = K5Realm(krb5_conf=conf)

# With no requested authdata, we expect to see SIGNTICKET (512) in an
# if-relevant container and the greet authdata in a kdc-issued
# container.
out = realm.run(['./adata', realm.host_princ])
if '?512: ' not in out or '^-42: Hello' not in out:
    fail('expected authdata not seen for basic request')

# Requested authdata is copied into the ticket, with KDC-only types
# filtered out.  (128 is win2k-pac, which should be filtered.)
out = realm.run(['./adata', realm.host_princ, '-5', 'test1', '?-6', 'test2',
                 '128', 'fakepac', '?128', 'ifrelfakepac',
                 '^-8', 'fakekdcissued', '?^-8', 'ifrelfakekdcissued'])
if ' -5: test1' not in out or '?-6: test2' not in out:
    fail('expected authdata not seen for request with authdata')
if 'fake' in out:
    fail('KDC-only authdata not filtered for request with authdata')

out = realm.run(['./adata', realm.host_princ, '!-1', 'mandatoryforkdc'],
                expected_code=1)
if 'KDC policy rejects request' not in out:
    fail('Wrong error seen for mandatory-for-kdc failure')

# The no_auth_data_required server flag should suppress SIGNTICKET,
# but not module or request authdata.
realm.run([kadminl, 'ank', '-randkey', '+no_auth_data_required', 'noauth'])
realm.extract_keytab('noauth', realm.keytab)
out = realm.run(['./adata', 'noauth', '-2', 'test'])
if '^-42: Hello' not in out or ' -2: test' not in out:
    fail('expected authdata not seen for no_auth_data_required request')
if '512: ' in out:
    fail('SIGNTICKET authdata seen for no_auth_data_required request')

# Cross-realm TGT requests should also suppress SIGNTICKET, but not
# module or request authdata.
realm.addprinc('krbtgt/XREALM')
realm.extract_keytab('krbtgt/XREALM', realm.keytab)
out = realm.run(['./adata', 'krbtgt/XREALM', '-3', 'test'])
if '^-42: Hello' not in out or ' -3: test' not in out:
    fail('expected authdata not seen for cross-realm TGT request')
if '512: ' in out:
    fail('SIGNTICKET authdata seen in cross-realm TGT')

realm.stop()

if not os.path.exists(os.path.join(plugins, 'preauth', 'pkinit.so')):
    skipped('anonymous ticket authdata tests', 'PKINIT not built')
else:
    # Set up a realm with PKINIT support and get anonymous tickets.
    certs = os.path.join(srctop, 'tests', 'dejagnu', 'pkinit-certs')
    ca_pem = os.path.join(certs, 'ca.pem')
    kdc_pem = os.path.join(certs, 'kdc.pem')
    privkey_pem = os.path.join(certs, 'privkey.pem')
    pkinit_conf = {'realms': {'$realm': {
                'pkinit_anchors': 'FILE:%s' % ca_pem,
                'pkinit_identity': 'FILE:%s,%s' % (kdc_pem, privkey_pem)}}}
    conf.update(pkinit_conf)
    realm = K5Realm(krb5_conf=conf, get_creds=False)
    realm.addprinc('WELLKNOWN/ANONYMOUS')
    realm.kinit('@%s' % realm.realm, flags=['-n'])

    # SIGNTICKET and module authdata should be suppressed for
    # anonymous tickets, but not request authdata.
    out = realm.run(['./adata', realm.host_princ, '-4', 'test'])
    if ' -4: test' not in out:
        fail('expected authdata not seen for anonymous request')
    if '512: ' in out or '-42: ' in out:
        fail('SIGNTICKET or greet authdata seen for anonymous request')

realm.stop()

# Test authentication indicators.  Load the test preauth module so we
# can control the indicators asserted.
testpreauth = os.path.join(buildtop, 'plugins', 'preauth', 'test', 'test.so')
krb5conf = {'plugins': {'kdcpreauth': {'module': 'test:' + testpreauth},
                        'clpreauth': {'module': 'test:' + testpreauth}}}
realm, realm2 = cross_realms(2, args=({'realm': 'LOCAL'},
                                      {'realm': 'FOREIGN'}),
                             krb5_conf=krb5conf, get_creds=False)
realm.run([kadminl, 'modprinc', '+requires_preauth', '-maxrenewlife', '2 days',
           realm.user_princ])
realm.run([kadminl, 'modprinc', '-maxrenewlife', '2 days', realm.host_princ])
realm.extract_keytab(realm.krbtgt_princ, realm.keytab)
realm.extract_keytab(realm.host_princ, realm.keytab)
realm.extract_keytab('krbtgt/FOREIGN', realm.keytab)
realm2.extract_keytab(realm2.krbtgt_princ, realm.keytab)
realm2.extract_keytab(realm2.host_princ, realm.keytab)
realm2.extract_keytab('krbtgt/LOCAL', realm.keytab)

# AS request to local-realm service
realm.kinit(realm.user_princ, password('user'),
            ['-X', 'indicators=indcl', '-r', '2d', '-S', realm.host_princ])
out = realm.run(['./adata', realm.host_princ])
if '+97: [indcl]' not in out:
    fail('auth-indicator not seen for AS req to service')

# Ticket modification request
realm.kinit(realm.user_princ, None, ['-R', '-S', realm.host_princ])
out = realm.run(['./adata', realm.host_princ])
if '+97: [indcl]' not in out:
    fail('auth-indicator not seen for ticket modification request')

# AS request to cross TGT
realm.kinit(realm.user_princ, password('user'),
            ['-X', 'indicators=indcl', '-S', 'krbtgt/FOREIGN'])
out = realm.run(['./adata', 'krbtgt/FOREIGN'])
if '+97: [indcl]' not in out:
    fail('auth-indicator not seen for AS req to cross-realm TGT')

# Multiple indicators
realm.kinit(realm.user_princ, password('user'),
            ['-X', 'indicators=indcl indcl2 indcl3'])
out = realm.run(['./adata', realm.krbtgt_princ])
if '+97: [indcl, indcl2, indcl3]' not in out:
    fail('multiple auth-indicators not seen for normal AS req')

# AS request to local TGT (resulting creds are used for TGS tests)
realm.kinit(realm.user_princ, password('user'), ['-X', 'indicators=indcl'])
out = realm.run(['./adata', realm.krbtgt_princ])
if '+97: [indcl]' not in out:
    fail('auth-indicator not seen for normal AS req')

# Local TGS request for local realm service
out = realm.run(['./adata', realm.host_princ])
if '+97: [indcl]' not in out:
    fail('auth-indicator not seen for local TGS req')

# Local TGS request for cross TGT service
out = realm.run(['./adata', 'krbtgt/FOREIGN'])
if '+97: [indcl]' not in out:
    fail('auth-indicator not seen for TGS req to cross-realm TGT')

# We don't yet have support for passing auth indicators across realms,
# so just verify that indicators don't survive cross-realm requests.
out = realm.run(['./adata', realm2.krbtgt_princ])
if '97:' in out:
    fail('auth-indicator seen in cross TGT request to local TGT')
out = realm.run(['./adata', 'krbtgt/LOCAL@FOREIGN'])
if '97:' in out:
    fail('auth-indicator seen in cross TGT request to cross TGT')
out = realm.run(['./adata', realm2.host_princ])
if '97:' in out:
    fail('auth-indicator seen in cross TGT request to service')

# Test that the CAMMAC signature still works during a krbtgt rollover.
realm.run([kadminl, 'cpw', '-randkey', '-keepold', realm.krbtgt_princ])
out = realm.run(['./adata', realm.host_princ])
if '+97: [indcl]' not in out:
    fail('auth-indicator not seen for local TGS req after krbtgt rotation')

# Test indicator enforcement.
realm.addprinc('restricted')
realm.run([kadminl, 'setstr', 'restricted', 'require_auth', 'superstrong'])
out = realm.run([kvno, 'restricted'], expected_code=1)
if 'KDC policy rejects request' not in out:
    fail('expected error not seen for auth indicator enforcement')
realm.run([kadminl, 'setstr', 'restricted', 'require_auth', 'indcl'])
realm.run([kvno, 'restricted'])
realm.kinit(realm.user_princ, password('user'), ['-X', 'indicators=ind1 ind2'])
realm.run([kvno, 'restricted'], expected_code=1)
realm.run([kadminl, 'setstr', 'restricted', 'require_auth', 'a b c ind2'])
realm.run([kvno, 'restricted'])

# Regression test for one manifestation of #8139: ensure that
# forwarded TGTs obtained across a TGT re-key still work when the
# preferred krbtgt enctype changes.
realm.kinit(realm.user_princ, password('user'), ['-f'])
realm.run([kadminl, 'cpw', '-randkey', '-keepold', '-e', 'des3-cbc-sha1',
           realm.krbtgt_princ])
realm.run(['./forward'])
realm.run([kvno, realm.host_princ])

realm.stop()
realm2.stop()

# Load the test KDB module to allow successful S4U2Proxy
# auth-indicator requests.
testprincs = {'krbtgt/KRBTEST.COM': {'keys': 'aes128-cts'},
              'krbtgt/FOREIGN': {'keys': 'aes128-cts'},
              'user': {'keys': 'aes128-cts', 'flags': '+preauth'},
              'service/1': {'keys': 'aes128-cts', 'flags': '+preauth'},
              'service/2': {'keys': 'aes128-cts'},
              'noauthdata': {'keys': 'aes128-cts',
                             'flags': '+no_auth_data_required'}}
kdcconf = {'realms': {'$realm': {'database_module': 'test'}},
           'dbmodules': {'test': {'db_library': 'test',
                                  'princs': testprincs,
                                  'delegation': {'service/1': 'service/2'}}}}
realm = K5Realm(krb5_conf=krb5conf, kdc_conf=kdcconf, create_kdb=False)
usercache = 'FILE:' + os.path.join(realm.testdir, 'usercache')
realm.extract_keytab(realm.krbtgt_princ, realm.keytab)
realm.extract_keytab('krbtgt/FOREIGN', realm.keytab)
realm.extract_keytab(realm.user_princ, realm.keytab)
realm.extract_keytab('service/1', realm.keytab)
realm.extract_keytab('service/2', realm.keytab)
realm.extract_keytab('noauthdata', realm.keytab)
realm.start_kdc()

# S4U2Self (should have no indicators since client did not authenticate)
realm.kinit('service/1', None, ['-k', '-f', '-X', 'indicators=inds1'])
realm.run([kvno, '-U', 'user', 'service/1'])
out = realm.run(['./adata', '-p', realm.user_princ, 'service/1'])
if '97:' in out:
    fail('auth-indicator present in S4U2Self response')

# S4U2Proxy (indicators should come from evidence ticket, not TGT)
realm.kinit(realm.user_princ, None, ['-k', '-f', '-X', 'indicators=indcl',
                                     '-S', 'service/1', '-c', usercache])
realm.run(['./s4u2proxy', usercache, 'service/2'])
out = realm.run(['./adata', '-p', realm.user_princ, 'service/2'])
if '+97: [indcl]' not in out or '[inds1]' in out:
    fail('correct auth-indicator not seen for S4U2Proxy req')

# Test that KDB module authdata is included in an AS request, by
# default or with an explicit PAC request.
realm.kinit(realm.user_princ, None, ['-k'])
out = realm.run(['./adata', realm.krbtgt_princ])
if '-456: db-authdata-test' not in out:
    fail('DB authdata not seen in default AS request')
realm.kinit(realm.user_princ, None, ['-k', '--request-pac'])
out = realm.run(['./adata', realm.krbtgt_princ])
if '-456: db-authdata-test' not in out:
    fail('DB authdata not seen with --request-pac')

# Test that KDB module authdata is suppressed in an AS request by a
# negative PAC request.
realm.kinit(realm.user_princ, None, ['-k', '--no-request-pac'])
out = realm.run(['./adata', realm.krbtgt_princ])
if '-456: db-authdata-test' in out:
    fail('DB authdata not suppressed by --no-request-pac')

# Test that KDB authdata is included in a TGS request by default.
out = realm.run(['./adata', 'service/1'])
if '-456: db-authdata-test' not in out:
    fail('DB authdata not seen in TGS request')

# Test that KDB authdata is suppressed in a TGS request by the
# +no_auth_data_required flag.
out = realm.run(['./adata', 'noauthdata'])
if '-456: db-authdata-test' in out:
    fail('DB authdata not suppressed by +no_auth_data_required')

# Additional KDB module authdata behavior we don't currently test:
# * KDB module authdata is suppressed in TGS requests if the TGT
#   contains no authdata and the request is not cross-realm or S4U.
# * KDB module authdata is suppressed for anonymous tickets.

success('Authorization data tests')
