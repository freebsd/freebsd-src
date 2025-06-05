from k5test import *

realm = K5Realm()

def check_cache(ccache, expected_services):
    # Fetch the klist output and skip past the header.
    lines = realm.run([klist, '-c', ccache]).splitlines()
    lines = lines[4:]

    # For each line not beginning with an indent, match against the
    # expected service principals.
    svcs = {x: True for x in expected_services}
    for l in lines:
        if not l.startswith('\t'):
            svcprinc = l.split()[4]
            if svcprinc in svcs:
                del svcs[svcprinc]
            else:
                fail('unexpected service princ ' + svcprinc)

    if svcs:
        fail('services not found in klist output: ' + ' '.join(svcs.keys()))


mark('no options')
realm.run([kvno, realm.user_princ], expected_msg='user@KRBTEST.COM: kvno = 1')
check_cache(realm.ccache, [realm.krbtgt_princ, realm.user_princ])

mark('-e')
msgs = ('etypes requested in TGS request: camellia128-cts',
        '/KDC has no support for encryption type')
realm.run([kvno, '-e', 'camellia128-cts', realm.host_princ],
          expected_code=1, expected_trace=msgs)

mark('--cached-only')
realm.run([kvno, '--cached-only', realm.user_princ], expected_msg='kvno = 1')
realm.run([kvno, '--cached-only', realm.host_princ],
          expected_code=1, expected_msg='Matching credential not found')
check_cache(realm.ccache, [realm.krbtgt_princ, realm.user_princ])

mark('--no-store')
realm.run([kvno, '--no-store', realm.host_princ], expected_msg='kvno = 1')
check_cache(realm.ccache, [realm.krbtgt_princ, realm.user_princ])

mark('--out-cache') # and multiple services
out_ccache = os.path.join(realm.testdir, 'ccache.out')
realm.run([kvno, '--out-cache', out_ccache,
           realm.host_princ, realm.admin_princ])
check_cache(realm.ccache, [realm.krbtgt_princ, realm.user_princ])
check_cache(out_ccache, [realm.host_princ, realm.admin_princ])

mark('--out-cache --cached-only') # tests out-cache overwriting, and -q
realm.run([kvno, '--out-cache', out_ccache, '--cached-only', realm.host_princ],
          expected_code=1, expected_msg='Matching credential not found')
out = realm.run([kvno, '-q', '--out-cache', out_ccache, '--cached-only',
                 realm.user_princ])
if out:
    fail('unexpected kvno output with -q')
check_cache(out_ccache, [realm.user_princ])

mark('-U') # and -c
svc_ccache = os.path.join(realm.testdir, 'ccache.svc')
realm.run([kinit, '-k', '-c', svc_ccache, realm.host_princ])
realm.run([kvno, '-c', svc_ccache, '-U', 'user', realm.host_princ])
realm.run([klist, '-c', svc_ccache], expected_msg='for client user@')
realm.run([kvno, '-c', svc_ccache, '-U', 'user', '--out-cache', out_ccache,
           realm.host_princ])
out = realm.run([klist, '-c', out_ccache])
if ('Default principal: user@KRBTEST.COM' not in out):
    fail('wrong default principal in klist output')

# More S4U options are tested in tests/gssapi/t_s4u.py.
# --u2u is tested in tests/t_u2u.py.

success('kvno tests')
