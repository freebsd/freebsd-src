#!/usr/bin/python
from k5test import *

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

# Try krb5 -> S4U2Proxy with forwardable user creds.  This should fail
# at the S4U2Proxy step since the DB2 back end currently has no
# support for allowing it.
realm.kinit(realm.user_princ, password('user'), ['-f', '-c', usercache])
output = realm.run(['./t_s4u2proxy_krb5', usercache, storagecache, '-',
                    pservice1, pservice2], expected_code=1)
if ('auth1: ' + realm.user_princ not in output or
    'NOT_ALLOWED_TO_DELEGATE' not in output):
    fail('krb5 -> s4u2proxy')

# Again with SPNEGO.
output = realm.run(['./t_s4u2proxy_krb5', '--spnego', usercache, storagecache,
                    '-', pservice1, pservice2],
                   expected_code=1)
if ('auth1: ' + realm.user_princ not in output or
    'NOT_ALLOWED_TO_DELEGATE' not in output):
    fail('krb5 -> s4u2proxy (SPNEGO)')

# Try krb5 -> S4U2Proxy without forwardable user creds.  This should
# result in no delegated credential being created by
# accept_sec_context.
realm.kinit(realm.user_princ, password('user'), ['-c', usercache])
output = realm.run(['./t_s4u2proxy_krb5', usercache, storagecache, pservice1,
                    pservice1, pservice2])
if 'no credential delegated' not in output:
    fail('krb5 -> no delegated cred')

# Try S4U2Self.  Ask for an S4U2Proxy step; this won't happen because
# service/1 isn't allowed to get a forwardable S4U2Self ticket.
output = realm.run(['./t_s4u', puser, pservice2])
if ('Warning: no delegated cred handle' not in output or
    'Source name:\t' + realm.user_princ not in output):
    fail('s4u2self')
output = realm.run(['./t_s4u', '--spnego', puser, pservice2])
if ('Warning: no delegated cred handle' not in output or
    'Source name:\t' + realm.user_princ not in output):
    fail('s4u2self (SPNEGO)')

# Correct that problem and try again.  As above, the S4U2Proxy step
# won't actually succeed since we don't support that in DB2.
realm.run([kadminl, 'modprinc', '+ok_to_auth_as_delegate', service1])
output = realm.run(['./t_s4u', puser, pservice2], expected_code=1)
if 'NOT_ALLOWED_TO_DELEGATE' not in output:
    fail('s4u2self')

# Again with SPNEGO.  This uses SPNEGO for the initial authentication,
# but still uses krb5 for S4U2Proxy--the delegated cred is returned as
# a krb5 cred, not a SPNEGO cred, and t_s4u uses the delegated cred
# directly rather than saving and reacquiring it.
output = realm.run(['./t_s4u', '--spnego', puser, pservice2], expected_code=1)
if 'NOT_ALLOWED_TO_DELEGATE' not in output:
    fail('s4u2self')

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

# Exercise cross-realm S4U2Self.  The query in the foreign realm will
# fail, but we can check that the right server principal was used.
r1, r2 = cross_realms(2, create_user=False)
r1.run([kinit, '-k', r1.host_princ])
out = r1.run(['./t_s4u', 'p:' + r2.host_princ], expected_code=1)
if 'Server not found in Kerberos database' not in out:
    fail('cross-realm s4u2self (t_s4u output)')
r1.stop()
r2.stop()
with open(os.path.join(r2.testdir, 'kdc.log')) as f:
    kdclog = f.read()
exp_princ = r1.host_princ.replace('/', '\\/').replace('@', '\\@')
if ('for %s@%s, Server not found' % (exp_princ, r2.realm)) not in kdclog:
    fail('cross-realm s4u2self (kdc log)')

success('S4U test cases')
