from k5test import *

# Skip this test if pkinit wasn't built.
if not pkinit_enabled:
    skip_rest('certauth tests', 'PKINIT module not built')

modpath = os.path.join(buildtop, 'plugins', 'certauth', 'test',
                       'certauth_test.so')
krb5_conf = {'plugins': {'certauth': {
    'module': ['test1:' + modpath, 'test2:' + modpath, 'test3:' + modpath],
    'enable_only': ['test1', 'test2', 'test3']}}}
kdc_conf = {'realms': {'$realm': {
    'default_principal_flags': '+preauth',
    'pkinit_indicator': ['indpkinit1', 'indpkinit2']}}}

realm = K5Realm(krb5_conf=krb5_conf, kdc_conf=kdc_conf, get_creds=False,
                pkinit=True)
realm.addprinc('nocert')

def check_indicators(inds):
    msg = '+97: [%s]' % inds
    realm.run(['./adata', realm.host_princ], expected_msg=msg)

# Test that authentication fails if no module accepts.
realm.pkinit('nocert', expected_code=1, expected_msg='Client name mismatch')

# Let the test2 module match user to CN=user, with indicators.
realm.pkinit(realm.user_princ)
realm.klist(realm.user_princ)
check_indicators('test1, test2, user, indpkinit1, indpkinit2')

# Let the test2 module mismatch with user2 to CN=user.
realm.addprinc('user2@KRBTEST.COM')
realm.pkinit('user2', expected_code=1, expected_msg='Certificate mismatch')

# Test the KRB5_CERTAUTH_HWAUTH return code.
mark('hw-authent flag tests')
# First test +requires_hwauth without causing the hw-authent ticket
# flag to be set.  This currently results in a preauth loop.
realm.run([kadminl, 'modprinc', '+requires_hwauth', realm.user_princ])
realm.pkinit(realm.user_princ, expected_code=1,
             expected_msg='Looping detected')
# Cause the test3 module to return KRB5_CERTAUTH_HWAUTH and try again.
# Authentication should succeed whether or not another module accepts,
# but not if another module rejects.
realm.run([kadminl, 'setstr', realm.user_princ, 'hwauth', 'ok'])
realm.run([kadminl, 'setstr', 'user2', 'hwauth', 'ok'])
realm.run([kadminl, 'setstr', 'nocert', 'hwauth', 'ok'])
realm.pkinit(realm.user_princ)
check_indicators('test1, test2, user, hwauth:ok, indpkinit1, indpkinit2')
realm.pkinit('user2', expected_code=1, expected_msg='Certificate mismatch')
realm.pkinit('nocert')
check_indicators('test1, hwauth:ok, indpkinit1, indpkinit2')

# Cause the test3 module to return KRB5_CERTAUTH_HWAUTH_PASS and try
# again.  Authentication should succeed only if another module accepts.
realm.run([kadminl, 'setstr', realm.user_princ, 'hwauth', 'pass'])
realm.run([kadminl, 'setstr', 'user2', 'hwauth', 'pass'])
realm.run([kadminl, 'setstr', 'nocert', 'hwauth', 'pass'])
realm.pkinit(realm.user_princ)
check_indicators('test1, test2, user, hwauth:pass, indpkinit1, indpkinit2')
realm.pkinit('user2', expected_code=1, expected_msg='Certificate mismatch')
realm.pkinit('nocert', expected_code=1, expected_msg='Client name mismatch')

success("certauth tests")
