from k5test import *

realm = K5Realm()

mark('gss_store_cred_into() and ccache/keytab')
storagecache = 'FILE:' + os.path.join(realm.testdir, 'user_store')
servicekeytab = os.path.join(realm.testdir, 'kt')
service_cs = 'service/cs@%s' % realm.realm
realm.addprinc(service_cs)
realm.extract_keytab(service_cs, servicekeytab)
realm.kinit(service_cs, None, ['-k', '-t', servicekeytab])
msgs = ('Storing %s -> %s in MEMORY:' % (service_cs, realm.krbtgt_princ),
        'Moving ccache MEMORY:',
        'Retrieving %s from FILE:%s' % (service_cs, servicekeytab))
realm.run(['./t_credstore', '-s', 'p:' + service_cs, 'ccache', storagecache,
           'keytab', servicekeytab], expected_trace=msgs)

mark('matching')
scc = 'FILE:' + os.path.join(realm.testdir, 'service_cache')
realm.kinit(realm.host_princ, flags=['-k', '-c', scc])
realm.run(['./t_credstore', '-i', 'p:' + realm.host_princ, 'ccache', scc])
realm.run(['./t_credstore', '-i', 'h:host', 'ccache', scc])
realm.run(['./t_credstore', '-i', 'h:host@' + hostname, 'ccache', scc])
realm.run(['./t_credstore', '-i', 'p:wrong', 'ccache', scc],
          expected_code=1, expected_msg='does not match desired name')
realm.run(['./t_credstore', '-i', 'h:host@-nomatch-', 'ccache', scc],
          expected_code=1, expected_msg='does not match desired name')
realm.run(['./t_credstore', '-i', 'h:svc', 'ccache', scc],
          expected_code=1, expected_msg='does not match desired name')

mark('matching (fallback)')
canonname = canonicalize_hostname(hostname)
if canonname != hostname:
    canonprinc = 'host/%s@%s' % (canonname, realm.realm)
    realm.addprinc(canonprinc)
    realm.extract_keytab(canonprinc, realm.keytab)
    realm.kinit(canonprinc, flags=['-k', '-c', scc])
    realm.run(['./t_credstore', '-i', 'h:host', 'ccache', scc])
    realm.run(['./t_credstore', '-i', 'h:host@' + hostname, 'ccache', scc])
    realm.run(['./t_credstore', '-i', 'h:host@' + canonname, 'ccache', scc])
    realm.run(['./t_credstore', '-i', 'p:' + canonprinc, 'ccache', scc])
    realm.run(['./t_credstore', '-i', 'p:' + realm.host_princ, 'ccache', scc],
              expected_code=1, expected_msg='does not match desired name')
    realm.run(['./t_credstore', '-i', 'h:host@-nomatch-', 'ccache', scc],
              expected_code=1, expected_msg='does not match desired name')
else:
    skipped('fallback matching test',
            '%s does not canonicalize to a different name' % hostname)

mark('rcache')
# t_credstore -r should produce a replay error normally, but not with
# rcache set to "none:".
realm.run(['./t_credstore', '-r', '-a', 'p:' + realm.host_princ],
          expected_code=1,
          expected_msg='gss_accept_sec_context(2): Request is a replay')
realm.run(['./t_credstore', '-r', '-a', 'p:' + realm.host_princ,
           'rcache', 'none:'])

# Test password feature.
mark('password')
# Must be used with a desired name.
realm.run(['./t_credstore', '-i', '', 'password', 'pw'],
          expected_code=1, expected_msg='An invalid name was supplied')
# Must not be used with a client keytab.
realm.run(['./t_credstore', '-i', 'u:' + realm.user_princ,
           'password', 'pw', 'client_keytab', servicekeytab],
          expected_code=1, expected_msg='Credential usage type is unknown')
# Must not be used with a ccache.
realm.run(['./t_credstore', '-i', 'u:' + realm.user_princ,
           'password', 'pw', 'ccache', storagecache],
          expected_code=1, expected_msg='Credential usage type is unknown')
# Must be acquiring initiator credentials.
realm.run(['./t_credstore', '-a', 'u:' + realm.user_princ, 'password', 'pw'],
          expected_code=1, expected_msg='Credential usage type is unknown')
msgs = ('Getting initial credentials for %s' % realm.user_princ,
        'Storing %s -> %s in MEMORY:' % (realm.user_princ, realm.krbtgt_princ),
        'Destroying ccache MEMORY:')
realm.run(['./t_credstore', '-i', 'u:' + realm.user_princ, 'password',
           password('user')], expected_trace=msgs)

mark('verify')
msgs = ('Getting initial credentials for %s' % realm.user_princ,
        'Storing %s -> %s in MEMORY:' % (realm.user_princ, realm.krbtgt_princ),
        'Getting credentials %s -> %s' % (realm.user_princ, service_cs),
        'Storing %s -> %s in MEMORY:' % (realm.user_princ, service_cs))
realm.run(['./t_credstore', '-i', 'u:' + realm.user_princ, 'password',
           password('user'), 'keytab', servicekeytab, 'verify',
           service_cs], expected_trace=msgs)
# Try again with verification failing due to key mismatch.
realm.run([kadminl, 'cpw', '-randkey', service_cs])
realm.run([kadminl, 'modprinc', '-kvno', '1', service_cs])
errmsg = 'Cannot decrypt ticket for %s' % service_cs
realm.run(['./t_credstore', '-i', 'u:' + realm.user_princ, 'password',
           password('user'), 'keytab', servicekeytab, 'verify',
           service_cs], expected_code=1, expected_msg=errmsg)

success('Credential store tests')
