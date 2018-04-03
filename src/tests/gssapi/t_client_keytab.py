#!/usr/bin/python
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

success('Client keytab tests')
