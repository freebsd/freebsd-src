from k5test import *

# Set up a basic realm and a client keytab containing two user principals.
# Point HOME at realm.testdir for tests using .k5identity.
realm = K5Realm(get_creds=False)
bob = 'bob@' + realm.realm
phost = 'p:' + realm.host_princ
puser = 'p:' + realm.user_princ
pbob = 'p:' + bob
gssserver = 'h:host@' + hostname
realm.env['HOME'] = realm.testdir
realm.addprinc(bob, password('bob'))
realm.extract_keytab(realm.user_princ, realm.client_keytab)
realm.extract_keytab(bob, realm.client_keytab)

# Test 1: no name/cache specified, pick first principal from client keytab
realm.run(['./t_ccselect', phost], expected_msg=realm.user_princ)
realm.run([kdestroy])

# Test 2: no name/cache specified, pick principal from k5identity
k5idname = os.path.join(realm.testdir, '.k5identity')
k5id = open(k5idname, 'w')
k5id.write('%s service=host host=%s\n' % (bob, hostname))
k5id.close()
realm.run(['./t_ccselect', gssserver], expected_msg=bob)
os.remove(k5idname)
realm.run([kdestroy])

# Test 3: no name/cache specified, default ccache has name but no creds
realm.run(['./ccinit', realm.ccache, bob])
realm.run(['./t_ccselect', phost], expected_msg=bob)
# Leave tickets for next test.

# Test 4: name specified, non-collectable default cache doesn't match
msg = 'Principal in credential cache does not match desired name'
realm.run(['./t_ccselect', phost, puser], expected_code=1, expected_msg=msg)
realm.run([kdestroy])

# Test 5: name specified, nonexistent default cache
realm.run(['./t_ccselect', phost, pbob], expected_msg=bob)
# Leave tickets for next test.

# Test 6: name specified, matches default cache, time to refresh
realm.run(['./ccrefresh', realm.ccache, '1'])
realm.run(['./t_ccselect', phost, pbob], expected_msg=bob)
out = realm.run(['./ccrefresh', realm.ccache])
if int(out) < 1000:
    fail('Credentials apparently not refreshed')
realm.run([kdestroy])

# Test 7: empty ccache specified, pick first principal from client keytab
realm.run(['./t_imp_cred', phost])
realm.klist(realm.user_princ)
realm.run([kdestroy])

# Test 8: ccache specified with name but no creds; name not in client keytab
realm.run(['./ccinit', realm.ccache, realm.host_princ])
realm.run(['./t_imp_cred', phost], expected_code=1,
          expected_msg='Credential cache is empty')
realm.run([kdestroy])

# Test 9: ccache specified with name but no creds; name in client keytab
realm.run(['./ccinit', realm.ccache, bob])
realm.run(['./t_imp_cred', phost])
realm.klist(bob)
# Leave tickets for next test.

# Test 10: ccache specified with creds, time to refresh
realm.run(['./ccrefresh', realm.ccache, '1'])
realm.run(['./t_imp_cred', phost])
realm.klist(bob)
out = realm.run(['./ccrefresh', realm.ccache])
if int(out) < 1000:
    fail('Credentials apparently not refreshed')
realm.run([kdestroy])

# Test 11: gss_import_cred_from with client_keytab value
store_keytab = os.path.join(realm.testdir, 'store_keytab')
os.rename(realm.client_keytab, store_keytab)
realm.run(['./t_credstore', '-i', 'p:' + realm.user_princ, 'client_keytab',
           store_keytab])
realm.klist(realm.user_princ)
os.rename(store_keytab, realm.client_keytab)

# Use a cache collection for the remaining tests.
ccdir = os.path.join(realm.testdir, 'cc')
ccname = 'DIR:' + ccdir
os.mkdir(ccdir)
realm.env['KRB5CCNAME'] = ccname

# Test 12: name specified, matching cache in collection with no creds
bobcache = os.path.join(ccdir, 'tktbob')
realm.run(['./ccinit', bobcache, bob])
realm.run(['./t_ccselect', phost, pbob], expected_msg=bob)
# Leave tickets for next test.

# Test 13: name specified, matching cache in collection, time to refresh
realm.run(['./ccrefresh', bobcache, '1'])
realm.run(['./t_ccselect', phost, pbob], expected_msg=bob)
out = realm.run(['./ccrefresh', bobcache])
if int(out) < 1000:
    fail('Credentials apparently not refreshed')
realm.run([kdestroy, '-A'])

# Test 14: name specified, collection has default for different principal
realm.kinit(realm.user_princ, password('user'))
realm.run(['./t_ccselect', phost, pbob], expected_msg=bob)
msg = 'Default principal: %s\n' % realm.user_princ
realm.run([klist], expected_msg=msg)
realm.run([kdestroy, '-A'])

# Test 15: name specified, collection has no default cache
realm.run(['./t_ccselect', phost, pbob], expected_msg=bob)
# Make sure the tickets we acquired didn't become the default
realm.run([klist], expected_code=1, expected_msg='No credentials cache found')
realm.run([kdestroy, '-A'])

# Test 16: default client keytab cannot be resolved, but valid
# credentials exist in ccache.
conf = {'libdefaults': {'default_client_keytab_name': '%{'}}
bad_cktname = realm.special_env('bad_cktname', False, krb5_conf=conf)
del bad_cktname['KRB5_CLIENT_KTNAME']
realm.kinit(realm.user_princ, password('user'))
realm.run(['./t_ccselect', phost], env=bad_cktname,
          expected_msg=realm.user_princ)

mark('refresh of manually acquired creds')

# Test 17: no name/ccache specified, manually acquired creds which
# will expire soon.  Verify that creds are refreshed using the current
# client name, with refresh_time set in the refreshed ccache.
realm.kinit('bob', password('bob'), ['-l', '15s'])
realm.run(['./t_ccselect', phost], expected_msg='bob')
realm.run([klist, '-C'], expected_msg='refresh_time = ')

# Test 18: no name/ccache specified, manually acquired creds with a
# client principal not present in the client keytab.  A refresh is
# attempted but fails, and an expired ticket error results.
realm.kinit(realm.admin_princ, password('admin'), ['-l', '-10s'])
msgs = ('Getting initial credentials for user/admin@KRBTEST.COM',
        '/Matching credential not found')
realm.run(['./t_ccselect', phost], expected_code=1,
          expected_msg='Ticket expired', expected_trace=msgs)
realm.run([kdestroy, '-A'])

# Test 19: host-based initiator name
mark('host-based initiator name')
hsvc = 'h:svc@' + hostname
svcprinc = 'svc/%s@%s' % (hostname, realm.realm)
realm.addprinc(svcprinc)
realm.extract_keytab(svcprinc, realm.client_keytab)
# On the first run we match against the keytab while getting tickets,
# substituting the default realm.
msgs = ('/Can\'t find client principal svc/%s@ in' % hostname,
        'Getting initial credentials for svc/%s@' % hostname,
        'Found entries for %s in keytab' % svcprinc,
        'Retrieving %s from FILE:%s' % (svcprinc, realm.client_keytab),
        'Storing %s -> %s in' % (svcprinc, realm.krbtgt_princ),
        'Retrieving %s -> %s from' % (svcprinc, realm.krbtgt_princ),
        'authenticator for %s -> %s' % (svcprinc, realm.host_princ))
realm.run(['./t_ccselect', phost, hsvc], expected_trace=msgs)
# On the second run we match against the collection.
msgs = ('Matching svc/%s@ in collection with result: 0' % hostname,
        'Getting credentials %s -> %s' % (svcprinc, realm.host_princ),
        'authenticator for %s -> %s' % (svcprinc, realm.host_princ))
realm.run(['./t_ccselect', phost, hsvc], expected_trace=msgs)
realm.run([kdestroy, '-A'])

# Test 20: host-based initiator name with fallback
mark('host-based fallback initiator name')
canonname = canonicalize_hostname(hostname)
if canonname != hostname:
    hfsvc = 'h:fsvc@' + hostname
    canonprinc = 'fsvc/%s@%s' % (canonname, realm.realm)
    realm.addprinc(canonprinc)
    realm.extract_keytab(canonprinc, realm.client_keytab)
    msgs = ('/Can\'t find client principal fsvc/%s@ in' % hostname,
            'Found entries for %s in keytab' % canonprinc,
            'authenticator for %s -> %s' % (canonprinc, realm.host_princ))
    realm.run(['./t_ccselect', phost, hfsvc], expected_trace=msgs)
    msgs = ('Matching fsvc/%s@ in collection with result: 0' % hostname,
            'Getting credentials %s -> %s' % (canonprinc, realm.host_princ))
    realm.run(['./t_ccselect', phost, hfsvc], expected_trace=msgs)
    realm.run([kdestroy, '-A'])
else:
    skipped('GSS initiator name fallback test',
            '%s does not canonicalize to a different name' % hostname)

success('Client keytab tests')
