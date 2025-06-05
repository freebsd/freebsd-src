from k5test import *

# Load the sample KDC authdata module.  Allow renewable tickets.
greet_path = os.path.join(buildtop, 'plugins', 'authdata', 'greet_server',
                          'greet_server.so')
conf = {'realms': {'$realm': {'max_life': '20h', 'max_renewable_life': '20h'}},
        'plugins': {'kdcauthdata': {'module': 'greet:' + greet_path}}}
realm = K5Realm(krb5_conf=conf)

# With no requested authdata, we expect to see PAC (128) in an
# if-relevant container and the greet authdata in a kdc-issued
# container.
mark('baseline authdata')
out = realm.run(['./adata', realm.host_princ])
if '?128: [6, 7, 10, 16, 19]' not in out or '^-42: Hello' not in out:
    fail('expected authdata not seen for basic request')

# Requested authdata is copied into the ticket, with KDC-only types
# filtered out.  (128 is win2k-pac, which should be filtered.)
mark('request authdata')
out = realm.run(['./adata', realm.host_princ, '-5', 'test1', '?-6', 'test2',
                 '128', 'fakepac', '?128', 'ifrelfakepac',
                 '^-8', 'fakekdcissued', '?^-8', 'ifrelfakekdcissued'])
if ' -5: test1' not in out or '?-6: test2' not in out:
    fail('expected authdata not seen for request with authdata')
if 'fake' in out:
    fail('KDC-only authdata not filtered for request with authdata')

mark('AD-MANDATORY-FOR-KDC')
realm.run(['./adata', realm.host_princ, '!-1', 'mandatoryforkdc'],
          expected_code=1, expected_msg='KDC policy rejects request')

# The no_auth_data_required server flag should suppress the PAC, but
# not module or request authdata.
mark('no_auth_data_required server flag')
realm.run([kadminl, 'ank', '-randkey', '+no_auth_data_required', 'noauth'])
realm.extract_keytab('noauth', realm.keytab)
out = realm.run(['./adata', 'noauth', '-2', 'test'])
if '^-42: Hello' not in out or ' -2: test' not in out:
    fail('expected authdata not seen for no_auth_data_required request')
if '128: ' in out:
    fail('PAC authdata seen for no_auth_data_required request')

# Cross-realm TGT requests should not suppress PAC or request
# authdata.
mark('cross-realm')
realm.addprinc('krbtgt/XREALM')
realm.extract_keytab('krbtgt/XREALM', realm.keytab)
out = realm.run(['./adata', 'krbtgt/XREALM', '-3', 'test'])
if '128:' not in out or  '^-42: Hello' not in out or ' -3: test' not in out:
    fail('expected authdata not seen for cross-realm TGT request')

mark('pac_privsvr_enctype')
# Change the privsvr enctype and make sure we can still verify the PAC
# on a service ticket in a TGS request.
realm.run([kadminl, 'setstr', realm.host_princ,
           'pac_privsvr_enctype', 'aes128-sha1'])
realm.kinit(realm.user_princ, password('user'),
            ['-S', realm.host_princ, '-r', '1h'])
realm.kinit(realm.user_princ, None, ['-S', realm.host_princ, '-R'])
# Remove the attribute and make sure the previously-issued service
# ticket PAC no longer verifies.
realm.run([kadminl, 'delstr', realm.host_princ, 'pac_privsvr_enctype'])
realm.kinit(realm.user_princ, None, ['-S', realm.host_princ, '-R'],
            expected_code=1, expected_msg='Message stream modified')

realm.stop()

if not pkinit_enabled:
    skipped('anonymous ticket authdata tests', 'PKINIT not built')
else:
    # Set up a realm with PKINIT support and get anonymous tickets.
    realm = K5Realm(krb5_conf=conf, get_creds=False, pkinit=True)
    realm.addprinc('WELLKNOWN/ANONYMOUS')
    realm.kinit('@%s' % realm.realm, flags=['-n'])

    # PAC and module authdata should be suppressed for anonymous
    # tickets, but not request authdata.
    mark('anonymous')
    out = realm.run(['./adata', realm.host_princ, '-4', 'test'])
    if ' -4: test' not in out:
        fail('expected authdata not seen for anonymous request')
    if '128: ' in out or '-42: ' in out:
        fail('PAC or greet authdata seen for anonymous request')

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
realm.run([kadminl, 'modprinc', '-maxrenewlife', '2 days', realm.krbtgt_princ])
realm.extract_keytab(realm.krbtgt_princ, realm.keytab)
realm.extract_keytab(realm.host_princ, realm.keytab)
realm.extract_keytab('krbtgt/FOREIGN', realm.keytab)
realm2.extract_keytab(realm2.krbtgt_princ, realm.keytab)
realm2.extract_keytab(realm2.host_princ, realm.keytab)
realm2.extract_keytab('krbtgt/LOCAL', realm.keytab)

# AS request to local-realm service
mark('AS-REQ to local service auth indicator')
realm.kinit(realm.user_princ, password('user'),
            ['-X', 'indicators=indcl', '-r', '2d', '-S', realm.host_princ])
realm.run(['./adata', realm.host_princ], expected_msg='+97: [indcl]')

# Ticket modification request
mark('ticket modification auth indicator')
realm.kinit(realm.user_princ, None, ['-R', '-S', realm.host_princ])
realm.run(['./adata', realm.host_princ], expected_msg='+97: [indcl]')

# AS request to cross TGT
mark('AS-REQ to cross TGT auth indicator')
realm.kinit(realm.user_princ, password('user'),
            ['-X', 'indicators=indcl', '-S', 'krbtgt/FOREIGN'])
realm.run(['./adata', 'krbtgt/FOREIGN'], expected_msg='+97: [indcl]')

# Multiple indicators
mark('AS multiple indicators')
realm.kinit(realm.user_princ, password('user'),
            ['-X', 'indicators=indcl indcl2 indcl3'])
realm.run(['./adata', realm.krbtgt_princ],
          expected_msg='+97: [indcl, indcl2, indcl3]')

# AS request to local TGT (resulting creds are used for TGS tests)
mark('AS-REQ to local TGT auth indicator')
realm.kinit(realm.user_princ, password('user'), ['-X', 'indicators=indcl'])
realm.run(['./adata', realm.krbtgt_princ], expected_msg='+97: [indcl]')

# Local TGS request for local realm service
mark('TGS-REQ to local service auth indicator')
realm.run(['./adata', realm.host_princ], expected_msg='+97: [indcl]')

# Local TGS request for cross TGT service
mark('TGS-REQ to cross TGT auth indicator')
realm.run(['./adata', 'krbtgt/FOREIGN'], expected_msg='+97: [indcl]')

# We don't yet have support for passing auth indicators across realms,
# so just verify that indicators don't survive cross-realm requests.
mark('TGS-REQ to foreign service auth indicator')
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
mark('CAMMAC signature across krbtgt rollover')
realm.run([kadminl, 'cpw', '-randkey', '-keepold', realm.krbtgt_princ])
realm.run(['./adata', realm.host_princ], expected_msg='+97: [indcl]')

# Test indicator enforcement.
mark('auth indicator enforcement')
realm.addprinc('restricted')
realm.run([kadminl, 'setstr', 'restricted', 'require_auth', 'superstrong'])
realm.kinit(realm.user_princ, password('user'), ['-S', 'restricted'],
            expected_code=1, expected_msg='KDC policy rejects request')
realm.run([kvno, 'restricted'], expected_code=1,
          expected_msg='KDC policy rejects request')
realm.run([kadminl, 'setstr', 'restricted', 'require_auth', 'indcl'])
realm.run([kvno, 'restricted'])
realm.kinit(realm.user_princ, password('user'), ['-X', 'indicators=ind1 ind2'])
realm.run([kvno, 'restricted'], expected_code=1)
realm.run([kadminl, 'setstr', 'restricted', 'require_auth', 'a b c ind2'])
realm.run([kvno, 'restricted'])

# Regression test for one manifestation of #8139: ensure that
# forwarded TGTs obtained across a TGT re-key still work when the
# preferred krbtgt enctype changes.
mark('#8139 regression test')
realm.kinit(realm.user_princ, password('user'), ['-f'])
realm.run([kadminl, 'cpw', '-randkey', '-keepold', '-e', 'des3-cbc-sha1',
           realm.krbtgt_princ])
realm.run(['./forward'])
realm.run([kvno, realm.host_princ])

# Repeat the above test using a renewed TGT.
mark('#8139 regression test (renewed TGT)')
realm.kinit(realm.user_princ, password('user'), ['-r', '2d'])
realm.run([kadminl, 'cpw', '-randkey', '-keepold', '-e', 'aes128-cts',
           realm.krbtgt_princ])
realm.kinit(realm.user_princ, None, ['-R'])
realm.run([kvno, realm.host_princ])

realm.stop()
realm2.stop()

# Load the test KDB module to allow successful S4U2Proxy
# auth-indicator requests and to detect whether replaced_reply_key is
# set.
testprincs = {'krbtgt/KRBTEST.COM': {'keys': 'aes128-cts'},
              'krbtgt/FOREIGN': {'keys': 'aes128-cts'},
              'user': {'keys': 'aes128-cts', 'flags': '+preauth'},
              'user2': {'keys': 'aes128-cts', 'flags': '+preauth'},
              'rservice': {'keys': 'aes128-cts',
                           'strings': 'require_auth:strong'},
              'service/1': {'keys': 'aes128-cts',
                            'flags': '+ok_to_auth_as_delegate'},
              'service/2': {'keys': 'aes128-cts'},
              'noauthdata': {'keys': 'aes128-cts',
                             'flags': '+no_auth_data_required'}}
kdcconf = {'realms': {'$realm': {'database_module': 'test'}},
           'dbmodules': {'test': {'db_library': 'test',
                                  'princs': testprincs,
                                  'delegation': {'service/1': 'service/2'}}}}
realm = K5Realm(krb5_conf=krb5conf, kdc_conf=kdcconf, create_kdb=False,
                pkinit=True)
usercache = 'FILE:' + os.path.join(realm.testdir, 'usercache')
realm.extract_keytab(realm.krbtgt_princ, realm.keytab)
realm.extract_keytab('krbtgt/FOREIGN', realm.keytab)
realm.extract_keytab(realm.user_princ, realm.keytab)
realm.extract_keytab('ruser', realm.keytab)
realm.extract_keytab('service/1', realm.keytab)
realm.extract_keytab('service/2', realm.keytab)
realm.extract_keytab('noauthdata', realm.keytab)
realm.start_kdc()

if not pkinit_enabled:
    skipped('replaced_reply_key test', 'PKINIT not built')
else:
    # Check that replaced_reply_key is set in issue_pac() when PKINIT
    # is used.  The test KDB module will indicate this by including a
    # fake PAC_CREDENTIAL_INFO(2) buffer in the PAC.
    mark('PKINIT (replaced_reply_key set)')
    realm.pkinit(realm.user_princ)
    realm.run(['./adata', realm.krbtgt_princ],
              expected_msg='?128: [1, 2, 6, 7, 10]')

# S4U2Self (should have no indicators since client did not authenticate)
mark('S4U2Self (no auth indicators expected)')
realm.kinit('service/1', None, ['-k', '-f', '-X', 'indicators=inds1'])
realm.run([kvno, '-U', 'user', 'service/1'])
out = realm.run(['./adata', '-p', realm.user_princ, 'service/1'])
if '97:' in out:
    fail('auth-indicator present in S4U2Self response')

# Get another S4U2Self ticket with requested authdata.
realm.run(['./s4u2self', 'user', 'service/1', '-', '-2', 'self_ad'])
realm.run(['./adata', '-p', realm.user_princ, 'service/1', '-2', 'self_ad'],
          expected_msg=' -2: self_ad')

# S4U2Proxy (indicators should come from evidence ticket, not TGT)
mark('S4U2Proxy (auth indicators from evidence ticket expected)')
realm.kinit(realm.user_princ, None, ['-k', '-f', '-X', 'indicators=indcl',
                                     '-S', 'service/1', '-c', usercache])
realm.run(['./s4u2proxy', usercache, 'service/2'])
out = realm.run(['./adata', '-p', realm.user_princ, 'service/2'])
if '+97: [indcl]' not in out or '[inds1]' in out:
    fail('correct auth-indicator not seen for S4U2Proxy req')
# Make sure a PAC with an S4U_DELEGATION_INFO(11) buffer is included.
if '?128: [1, 6, 7, 10, 11, 16, 19]' not in out:
    fail('PAC with delegation info not seen for S4U2Proxy req')

# Get another S4U2Proxy ticket including request-authdata.
realm.run(['./s4u2proxy', usercache, 'service/2', '-2', 'proxy_ad'])
realm.run(['./adata', '-p', realm.user_princ, 'service/2', '-2', 'proxy_ad'],
          expected_msg=' -2: proxy_ad')

# Get an S4U2Proxy ticket using an evidence ticket obtained by S4U2Self,
# with request authdata in both steps.
realm.run(['./s4u2self', 'user2', 'service/1', usercache, '-2', 'self_ad'])
realm.run(['./s4u2proxy', usercache, 'service/2', '-2', 'proxy_ad'])
out = realm.run(['./adata', '-p', 'user2', 'service/2', '-2', 'proxy_ad'])
if ' -2: self_ad' not in out or ' -2: proxy_ad' not in out:
    fail('expected authdata not seen in S4U2Proxy ticket')

# Test alteration of auth indicators by KDB module (AS and TGS).
realm.kinit(realm.user_princ, None, ['-k', '-X', 'indicators=dummy dbincr1'])
realm.run(['./adata', realm.krbtgt_princ], expected_msg='+97: [dbincr2]')
realm.run(['./adata', 'service/1'], expected_msg='+97: [dbincr3]')
realm.kinit(realm.user_princ, None,
            ['-k', '-X', 'indicators=strong', '-S', 'rservice'])
# Test enforcement of altered indicators during AS request.
realm.kinit(realm.user_princ, None,
            ['-k', '-X', 'indicators=strong dbincr1', '-S', 'rservice'],
            expected_code=1)

# Test that the PAC is suppressed in an AS request by a negative PAC
# request.
mark('AS-REQ PAC client supression')
realm.kinit(realm.user_princ, None, ['-k', '--no-request-pac'])
out = realm.run(['./adata', realm.krbtgt_princ])
if '128:' in out:
    fail('PAC not suppressed by --no-request-pac')

mark('S4U2Proxy with a foreign client')

a_princs = {'krbtgt/A': {'keys': 'aes128-cts'},
            'krbtgt/B': {'keys': 'aes128-cts'},
            'impersonator': {'keys': 'aes128-cts'},
            'impersonator2': {'keys': 'aes128-cts'},
            'resource': {'keys': 'aes128-cts'}}
a_kconf = {'realms': {'$realm': {'database_module': 'test'}},
           'dbmodules': {'test': {'db_library': 'test',
                                  'delegation': {'impersonator' : 'resource'},
                                  'princs': a_princs,
                                  'alias': {'service/rb.b': '@B'}}}}

b_princs = {'krbtgt/B': {'keys': 'aes128-cts'},
            'krbtgt/A': {'keys': 'aes128-cts'},
            'user': {'keys': 'aes128-cts', 'flags': '+preauth'},
            'rb': {'keys': 'aes128-cts'}}
b_kconf = {'realms': {'$realm': {'database_module': 'test'}},
           'dbmodules': {'test': {'db_library': 'test',
                                  'princs': b_princs,
                                  'rbcd': {'rb@B': 'impersonator2@A'},
                                  'alias': {'service/rb.b': 'rb',
                                            'impersonator2@A': '@A'}}}}

ra, rb = cross_realms(2, xtgts=(),
                          args=({'realm': 'A', 'kdc_conf': a_kconf},
                                {'realm': 'B', 'kdc_conf': b_kconf}),
                          create_kdb=False)

ra.start_kdc()
rb.start_kdc()

ra.extract_keytab('impersonator@A', ra.keytab)
ra.extract_keytab('impersonator2@A', ra.keytab)
rb.extract_keytab('user@B', rb.keytab)

usercache = 'FILE:' + os.path.join(rb.testdir, 'usercache')
rb.kinit(rb.user_princ, None, ['-k', '-f', '-c', usercache])
rb.run([kvno, '-C', 'impersonator@A', '-c', usercache])

ra.kinit('impersonator@A', None, ['-f', '-k', '-t', ra.keytab])
ra.run(['./s4u2proxy', usercache, 'resource@A'])

mark('Cross realm S4U authdata tests')

ra.kinit('impersonator2@A', None, ['-f', '-k', '-t', ra.keytab])
ra.run(['./s4u2self', rb.user_princ, 'impersonator2@A', usercache, '-2',
        'cross_s4u_self_ad'])
out = ra.run(['./adata', '-c', usercache, '-p', rb.user_princ,
              'impersonator2@A', '-2', 'cross_s4u_self_ad'])
if out.count(' -2: cross_s4u_self_ad') != 1:
    fail('expected one cross_s4u_self_ad, got: %s' % count)

ra.run(['./s4u2proxy', usercache, 'service/rb.b', '-2',
        'cross_s4u_proxy_ad'])
rb.extract_keytab('service/rb.b', ra.keytab)
out = ra.run(['./adata', '-p', rb.user_princ, 'service/rb.b', '-2',
              'cross_s4u_proxy_ad'])
if out.count(' -2: cross_s4u_self_ad') != 1:
    fail('expected one cross_s4u_self_ad, got: %s' % count)
if out.count(' -2: cross_s4u_proxy_ad') != 1:
    fail('expected one cross_s4u_proxy_ad, got: %s' % count)

ra.stop()
rb.stop()

success('Authorization data tests')
