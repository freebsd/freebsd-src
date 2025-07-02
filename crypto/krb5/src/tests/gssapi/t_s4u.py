from k5test import *
from base64 import b64encode
import shutil

realm = K5Realm(create_host=False, get_creds=False)
usercache = 'FILE:' + os.path.join(realm.testdir, 'usercache')
storagecache = 'FILE:' + os.path.join(realm.testdir, 'save')

# Create two service principals with keys in the default keytab.
service1 = 'service/1@%s' % realm.realm
realm.addprinc(service1)
realm.extract_keytab(service1, realm.keytab)
service2 = 'service/2@%s' % realm.realm
realm.addprinc(service2)
realm.extract_keytab(service2, realm.keytab)

puser = 'p:' + realm.user_princ
pservice1 = 'p:' + service1
pservice2 = 'p:' + service2

# Get forwardable creds for service1 in the default cache.
realm.kinit(service1, None, ['-f', '-k'])

# Try S4U2Self for user with a restricted password.
realm.run([kadminl, 'modprinc', '+needchange', realm.user_princ])
realm.run(['./t_s4u', 'e:user', '-'])
realm.run([kadminl, 'modprinc', '-needchange',
          '-pwexpire', '1/1/2000', realm.user_princ])
realm.run(['./t_s4u', 'e:user', '-'])
realm.run([kadminl, 'modprinc', '-pwexpire', 'never', realm.user_princ])

# Try krb5 -> S4U2Proxy with forwardable user creds.  This should fail
# at the S4U2Proxy step since the DB2 back end currently has no
# support for allowing it.
realm.kinit(realm.user_princ, password('user'), ['-f', '-c', usercache])
output = realm.run(['./t_s4u2proxy_krb5', usercache, storagecache, '-',
                    pservice1, pservice2], expected_code=1)
if ('auth1: ' + realm.user_princ not in output or
    'KDC can\'t fulfill requested option' not in output):
    fail('krb5 -> s4u2proxy')

# Again with SPNEGO.
output = realm.run(['./t_s4u2proxy_krb5', '--spnego', usercache, storagecache,
                    '-', pservice1, pservice2],
                   expected_code=1)
if ('auth1: ' + realm.user_princ not in output or
    'KDC can\'t fulfill requested option' not in output):
    fail('krb5 -> s4u2proxy (SPNEGO)')

# Try krb5 -> S4U2Proxy without forwardable user creds.
realm.kinit(realm.user_princ, password('user'), ['-c', usercache])
output = realm.run(['./t_s4u2proxy_krb5', usercache, storagecache, pservice1,
                   pservice1, pservice2], expected_code=1)
if ('auth1: ' + realm.user_princ not in output or
    'KDC can\'t fulfill requested option' not in output):
    fail('krb5 -> s4u2proxy not-forwardable')

# Try S4U2Self.  Ask for an S4U2Proxy step; this won't succeed because
# service/1 isn't allowed to get a forwardable S4U2Self ticket.
realm.run(['./t_s4u', puser, pservice2], expected_code=1,
          expected_msg='KDC can\'t fulfill requested option')
realm.run(['./t_s4u', '--spnego', puser, pservice2], expected_code=1,
          expected_msg='KDC can\'t fulfill requested option')

# Correct that problem and try again.  As above, the S4U2Proxy step
# won't actually succeed since we don't support that in DB2.
realm.run([kadminl, 'modprinc', '+ok_to_auth_as_delegate', service1])
realm.run(['./t_s4u', puser, pservice2], expected_code=1,
          expected_msg='KDC can\'t fulfill requested option')

# Again with SPNEGO.  This uses SPNEGO for the initial authentication,
# but still uses krb5 for S4U2Proxy--the delegated cred is returned as
# a krb5 cred, not a SPNEGO cred, and t_s4u uses the delegated cred
# directly rather than saving and reacquiring it.
realm.run(['./t_s4u', '--spnego', puser, pservice2], expected_code=1,
          expected_msg='KDC can\'t fulfill requested option')

realm.stop()

# Set up a realm using the test KDB module so that we can do
# successful S4U2Proxy delegations.
testprincs = {'krbtgt/KRBTEST.COM': {'keys': 'aes128-cts'},
              'user': {'keys': 'aes128-cts'},
              'service/1': {'flags': '+ok-to-auth-as-delegate',
                            'keys': 'aes128-cts'},
              'service/2': {'keys': 'aes128-cts'}}
conf = {'realms': {'$realm': {'database_module': 'test'}},
        'dbmodules': {'test': {'db_library': 'test',
                               'princs': testprincs,
                               'delegation': {'service/1': 'service/2'}}}}
realm = K5Realm(create_kdb=False, kdc_conf=conf)
userkeytab = 'FILE:' + os.path.join(realm.testdir, 'userkeytab')
realm.extract_keytab(realm.user_princ, userkeytab)
realm.extract_keytab(service1, realm.keytab)
realm.extract_keytab(service2, realm.keytab)
realm.start_kdc()

# Get forwardable creds for service1 in the default cache.
realm.kinit(service1, None, ['-f', '-k'])

# Successful krb5 -> S4U2Proxy, with krb5 and SPNEGO mechs.
realm.kinit(realm.user_princ, None, ['-f', '-k', '-c', usercache,
                                     '-t', userkeytab])
out = realm.run(['./t_s4u2proxy_krb5', usercache, storagecache, '-',
                 pservice1, pservice2])
if 'auth1: user@' not in out or 'auth2: user@' not in out:
    fail('krb5 -> s4u2proxy')
out = realm.run(['./t_s4u2proxy_krb5', '--spnego', usercache, storagecache,
                 '-', pservice1, pservice2])
if 'auth1: user@' not in out or 'auth2: user@' not in out:
    fail('krb5 -> s4u2proxy')

# Successful S4U2Self -> S4U2Proxy.
out = realm.run(['./t_s4u', puser, pservice2])

# Regression test for #8139: get a user ticket directly for service1 and
# try krb5 -> S4U2Proxy.
realm.kinit(realm.user_princ, None, ['-f', '-k', '-c', usercache,
                                     '-t', userkeytab, '-S', service1])
out = realm.run(['./t_s4u2proxy_krb5', usercache, storagecache, '-',
                 pservice1, pservice2])
if 'auth1: user@' not in out or 'auth2: user@' not in out:
    fail('krb5 -> s4u2proxy')

# Simulate a krbtgt rollover and verify that the user ticket can still
# be validated.
realm.stop_kdc()
newtgt_keys = ['2 aes128-cts', '1 aes128-cts']
newtgt_princs = {'krbtgt/KRBTEST.COM': {'keys': newtgt_keys}}
newtgt_conf = {'dbmodules': {'test': {'princs': newtgt_princs}}}
newtgt_env = realm.special_env('newtgt', True, kdc_conf=newtgt_conf)
realm.start_kdc(env=newtgt_env)
out = realm.run(['./t_s4u2proxy_krb5', usercache, storagecache, '-',
                 pservice1, pservice2])
if 'auth1: user@' not in out or 'auth2: user@' not in out:
    fail('krb5 -> s4u2proxy')

# Get a user ticket after the krbtgt rollover and verify that
# S4U2Proxy delegation works (also a #8139 regression test).
realm.kinit(realm.user_princ, None, ['-f', '-k', '-c', usercache,
                                     '-t', userkeytab])
out = realm.run(['./t_s4u2proxy_krb5', usercache, storagecache, '-',
                 pservice1, pservice2])
if 'auth1: user@' not in out or 'auth2: user@' not in out:
    fail('krb5 -> s4u2proxy')

realm.stop()

mark('S4U2Self with various enctypes')
for realm in multipass_realms(create_host=False, get_creds=False):
    service1 = 'service/1@%s' % realm.realm
    realm.addprinc(service1)
    realm.extract_keytab(service1, realm.keytab)
    realm.kinit(service1, None, ['-k'])
    realm.run(['./t_s4u', 'e:user', '-'])

# Test cross realm S4U2Self using server referrals.
mark('cross-realm S4U2Self')
testprincs = {'krbtgt/SREALM': {'keys': 'aes128-cts'},
              'krbtgt/UREALM': {'keys': 'aes128-cts'},
              'user': {'keys': 'aes128-cts', 'flags': '+preauth'},
              'other': {'keys': 'aes128-cts'}}
kdcconf1 = {'realms': {'$realm': {'database_module': 'test'}},
            'dbmodules': {'test': {'db_library': 'test',
                                   'princs': testprincs,
                                   'alias': {'enterprise@abc': '@UREALM',
                                             'user@UREALM': '@UREALM'}}}}
kdcconf2 = {'realms': {'$realm': {'database_module': 'test'}},
            'dbmodules': {'test': {'db_library': 'test',
                                   'princs': testprincs,
                                   'alias': {'user@SREALM': '@SREALM',
                                             'user@UREALM': 'user',
                                             'enterprise@abc': 'user'}}}}
r1, r2 = cross_realms(2, xtgts=(),
                      args=({'realm': 'SREALM', 'kdc_conf': kdcconf1},
                            {'realm': 'UREALM', 'kdc_conf': kdcconf2}),
                      create_kdb=False)

r1.start_kdc()
r2.start_kdc()
r1.extract_keytab(r1.user_princ, r1.keytab)
r1.kinit(r1.user_princ, None, ['-k', '-t', r1.keytab])
savefile = r1.ccache + '.save'
shutil.copyfile(r1.ccache, savefile)

# Include a regression test for #8741 by unsetting the default realm.
remove_default = {'libdefaults': {'default_realm': None}}
no_default = r1.special_env('no_default', False, krb5_conf=remove_default)
msgs = ('Getting credentials user@UREALM -> user@SREALM',
        '/Matching credential not found',
        'Getting credentials user@SREALM -> krbtgt/UREALM@SREALM',
        'Received creds for desired service krbtgt/UREALM@SREALM',
        'via TGT krbtgt/UREALM@SREALM after requesting user\\@SREALM@UREALM',
        'krbtgt/SREALM@UREALM differs from requested user\\@SREALM@UREALM',
        'via TGT krbtgt/SREALM@UREALM after requesting user@SREALM',
        'TGS reply is for user@UREALM -> user@SREALM')
r1.run(['./t_s4u', 'p:' + r2.user_princ, '-', r1.keytab], env=no_default,
       expected_trace=msgs)

# Test realm identification of enterprise principal names ([MS-SFU]
# 3.1.5.1.1.1).  Attach a bogus realm to the enterprise name to verify
# that we start at the server realm.
mark('cross-realm S4U2Self with enterprise name')
msgs = ('Getting initial credentials for enterprise\\@abc@SREALM',
        'Sending unauthenticated request',
        '/Realm not local to KDC',
        'Following referral to realm UREALM',
        'Sending unauthenticated request',
        '/Additional pre-authentication required',
        'Identified realm of client principal as UREALM',
        'Getting credentials enterprise\\@abc@UREALM -> user@SREALM',
        'TGS reply is for enterprise\\@abc@UREALM -> user@SREALM')
r1.run(['./t_s4u', 'e:enterprise@abc@NOREALM', '-', r1.keytab],
       expected_trace=msgs)

mark('S4U2Self using X509 certificate')

# Encode name as a PEM certificate file (sort of) for use by kvno.
def princ_cert(name):
    enc = b64encode(name.encode('ascii')).decode('ascii')
    return '-----BEGIN CERTIFICATE-----\n%s\n-----END y\n' % enc

cert_path = os.path.join(r1.testdir, 'fake_cert')
with open(cert_path, "w") as cert_file:
    cert_file.write(princ_cert('other'))

shutil.copyfile(savefile, r1.ccache)
msgs = ('Getting initial credentials for @SREALM',
        'Identified realm of client principal as SREALM',
        'TGS reply is for other@SREALM',
        'Getting credentials other@SREALM',
        'Storing other@SREALM')
r1.run([kvno, '-F', cert_path, r1.user_princ], expected_trace=msgs)

shutil.copyfile(savefile, r1.ccache)
msgs = ('Getting credentials other@SREALM',
        'TGS reply is for other@SREALM',
        'Storing other@SREALM')
r1.run([kvno, '-I', 'other', '-F', cert_path, r1.user_princ],
       expected_trace=msgs)

shutil.copyfile(savefile, r1.ccache)
msgs = ('Getting initial credentials for other@SREALM',
        'Identified realm of client principal as SREALM',
        'Getting credentials other@SREALM',
        'TGS reply is for other@SREALM',
        'Storing other@SREALM')
r1.run([kvno, '-U', 'other', '-F', cert_path, r1.user_princ],
       expected_trace=msgs)

mark('cross-realm S4U2Self using X509 certificate')

with open(cert_path, "w") as cert_file:
    cert_file.write(princ_cert('user@UREALM'))

shutil.copyfile(savefile, r1.ccache)
msgs = ('Getting initial credentials for @SREALM',
        'Identified realm of client principal as UREALM',
        'TGS reply is for user@UREALM',
        'Getting credentials user@UREALM',
        'Storing user@UREALM')
r1.run([kvno, '-F', cert_path, r1.user_princ], expected_trace=msgs)

shutil.copyfile(savefile, r1.ccache)
msgs = ('Getting credentials user@UREALM',
        'TGS reply is for user@UREALM',
        'Storing user@UREALM')
r1.run([kvno, '-I', 'user@UREALM', '-F', cert_path, r1.user_princ],
       expected_trace=msgs)

shutil.copyfile(savefile, r1.ccache)
msgs = ('Getting initial credentials for enterprise\\@abc@SREALM',
        'Identified realm of client principal as UREALM',
        'Getting credentials enterprise\\@abc@UREALM',
        'TGS reply is for enterprise\\@abc@UREALM',
        'Storing enterprise\\@abc@UREALM')
r1.run([kvno, '-U', 'enterprise@abc', '-F', cert_path, r1.user_princ],
       expected_trace=msgs)

shutil.copyfile(savefile, r1.ccache)

mark('S4U2Self using X509 certificate (GSSAPI)')

r1.run(['./t_s4u', 'c:other', '-', r1.keytab])
r1.run(['./t_s4u', 'c:user@UREALM', '-', r1.keytab])

r1.run(['./t_s4u', '--spnego', 'c:other', '-', r1.keytab])
r1.run(['./t_s4u', '--spnego', 'c:user@UREALM', '-', r1.keytab])

r1.stop()
r2.stop()

mark('Resource-based constrained delegation')

a_princs = {'krbtgt/A': {'keys': 'aes128-cts'},
            'krbtgt/B': {'keys': 'aes128-cts'},
            'user': {'keys': 'aes128-cts', 'flags': '+preauth'},
            'sensitive': {'keys': 'aes128-cts',
                          'flags': '+disallow_forwardable'},
            'impersonator': {'keys': 'aes128-cts'},
            'service1': {'keys': 'aes128-cts'},
            'rb2': {'keys': 'aes128-cts'},
            'rb': {'keys': 'aes128-cts'}}
a_kconf = {'realms': {'$realm': {'database_module': 'test'}},
           'dbmodules': {'test': {'db_library': 'test',
                                  'princs': a_princs,
                                  'rbcd': {'rb@A': 'impersonator@A',
                                           'rb2@A': 'service1@A'},
                                  'delegation': {'service1': 'rb2'},
                                  'alias': {'rb@A': 'rb',
                                            'rb@B': '@B',
                                            'rb@C': '@B',
                                            'service/rb.a': 'rb',
                                            'service/rb.b': '@B',
                                            'service/rb.c': '@B' }}}}

b_princs = {'krbtgt/B': {'keys': 'aes128-cts'},
            'krbtgt/A': {'keys': 'aes128-cts'},
            'krbtgt/C': {'keys': 'aes128-cts'},
            'user': {'keys': 'aes128-cts', 'flags': '+preauth'},
            'rb': {'keys': 'aes128-cts'}}
b_kconf = {'realms': {'$realm': {'database_module': 'test'}},
           'dbmodules': {'test': {'db_library': 'test',
                                  'princs': b_princs,
                                  'rbcd': {'rb@B': 'impersonator@A'},
                                  'alias': {'rb@B': 'rb',
                                            'service/rb.b': 'rb',
                                            'rb@C': '@C',
                                            'impersonator@A': '@A',
                                            'service/rb.c': '@C'}}}}

c_princs = {'krbtgt/C': {'keys': 'aes128-cts'},
            'krbtgt/B': {'keys': 'aes128-cts'},
            'rb': {'keys': 'aes128-cts'}}
c_kconf = {'realms': {'$realm': {'database_module': 'test'}},
           'capaths': { 'A' : { 'C' : 'B' }},
           'dbmodules': {'test': {'db_library': 'test',
                                  'princs': c_princs,
                                  'rbcd': {'rb@C': ['impersonator@A',
                                                    'service1@A']},
                                  'alias': {'rb@C': 'rb',
                                            'service/rb.c': 'rb' }}}}

ra, rb, rc = cross_realms(3, xtgts=(),
                          args=({'realm': 'A', 'kdc_conf': a_kconf},
                                {'realm': 'B', 'kdc_conf': b_kconf},
                                {'realm': 'C', 'kdc_conf': c_kconf}),
                          create_kdb=False)

ra.start_kdc()
rb.start_kdc()
rc.start_kdc()

domain_realm = {'domain_realm': {'.a':'A', '.b':'B', '.c':'C'}}
domain_conf = ra.special_env('domain_conf', False, krb5_conf=domain_realm)

ra.extract_keytab('impersonator@A', ra.keytab)
ra.kinit('impersonator@A', None, ['-f', '-k', '-t', ra.keytab])

mark('Local-realm RBCD')
ra.run(['./t_s4u', 'p:' + ra.user_princ, 'p:rb'])
ra.run(['./t_s4u', 'p:' + ra.user_princ, 'e:rb'])
ra.run(['./t_s4u', 'p:' + ra.user_princ, 'p:rb@A'])
ra.run(['./t_s4u', 'p:' + ra.user_princ, 'e:rb@A'])
ra.run(['./t_s4u', 'p:' + ra.user_princ, 'e:rb@A@'])
ra.run(['./t_s4u', 'p:' + ra.user_princ, 'e:rb@A@A'])
ra.run(['./t_s4u', 'p:' + ra.user_princ, 'h:service@rb.a'])
ra.run(['./t_s4u', 'p:' + ra.user_princ, 'h:service@rb.a'], env=domain_conf)
ra.run(['./t_s4u', 'p:' + 'sensitive@A', 'h:service@rb.a'], expected_code=1)
ra.run(['./t_s4u', 'p:' + rb.user_princ, 'h:service@rb.a'])

mark('Cross-realm RBCD')
ra.run(['./t_s4u', 'p:' + ra.user_princ, 'e:rb@B'])
ra.run(['./t_s4u', 'p:' + ra.user_princ, 'e:rb@B@'])
ra.run(['./t_s4u', 'p:' + ra.user_princ, 'e:rb@B@A'])
ra.run(['./t_s4u', 'p:' + ra.user_princ, 'h:service@rb.b'])
ra.run(['./t_s4u', 'p:' + ra.user_princ, 'h:service@rb.b'], env=domain_conf)
ra.run(['./t_s4u', 'p:' + 'sensitive@A', 'h:service@rb.b'], expected_code=1)
ra.run(['./t_s4u', 'p:' + rb.user_princ, 'h:service@rb.b'])

mark('RBCD transitive trust')
ra.run(['./t_s4u', 'p:' + ra.user_princ, 'e:rb@C'])
ra.run(['./t_s4u', 'p:' + ra.user_princ, 'e:rb@C@'])
ra.run(['./t_s4u', 'p:' + ra.user_princ, 'e:rb@C@A'])
ra.run(['./t_s4u', 'p:' + ra.user_princ, 'h:service@rb.c'])
ra.run(['./t_s4u', 'p:' + ra.user_princ, 'h:service@rb.c'], env=domain_conf)
ra.run(['./t_s4u', 'p:' + 'sensitive@A', 'h:service@rb.c'], expected_code=1)
ra.run(['./t_s4u', 'p:' + rb.user_princ, 'h:service@rb.c'])

# Although service1 has RBCD delegation privileges to rb2@A, it does
# not have ok-to-auth-as-delegate and does have traditional delegation
# privileges, so it cannot get a forwardable S4U2Self ticket.
mark('RBCD forwardable blocked by forward delegation privileges')
ra.extract_keytab('service1@A', ra.keytab)
ra.kinit('service1@A', None, ['-f', '-k', '-t', ra.keytab])
ra.run(['./t_s4u', 'p:' + ra.user_princ, 'e:rb2@A'], expected_code=1,
       expected_msg='KDC can\'t fulfill requested option')

ra.stop()
rb.stop()
rc.stop()

success('S4U test cases')
