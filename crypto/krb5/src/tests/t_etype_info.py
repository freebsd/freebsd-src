from k5test import *

supported_enctypes = 'aes128-cts des3-cbc-sha1 rc4-hmac'
conf = {'libdefaults': {'allow_des3': 'true', 'allow_rc4': 'true'},
        'realms': {'$realm': {'supported_enctypes': supported_enctypes}}}
realm = K5Realm(create_host=False, get_creds=False, krb5_conf=conf)

realm.run([kadminl, 'addprinc', '-pw', 'pw', '+requires_preauth',
           'preauthuser'])
realm.run([kadminl, 'addprinc', '-pw', 'pw', '-e', 'rc4-hmac',
           '+requires_preauth', 'rc4user'])
realm.run([kadminl, 'addprinc', '-nokey', '+requires_preauth', 'nokeyuser'])


# Run the test harness for the given principal and request enctype
# list.  Compare the output to the expected lines, ignoring order.
def test_etinfo(princ, enctypes, expected_lines):
    mark('etinfo test: %s %s' % (princ.partition('@')[0], enctypes))
    conf = {'libdefaults': {'default_tkt_enctypes': enctypes}}
    etypes_env = realm.special_env('etypes', False, krb5_conf=conf)
    lines = realm.run(['./etinfo', princ], env=etypes_env).splitlines()
    if sorted(lines) != sorted(expected_lines):
        fail('Unexpected output for princ %s, etypes %s' % (princ, enctypes))


# With no newer enctypes in the request, PA-ETYPE-INFO2,
# PA-ETYPE-INFO, and PA-PW-SALT appear in the AS-REP, each listing one
# key for the most preferred matching enctype.
test_etinfo('user', 'rc4-hmac-exp des3 rc4',
            ['asrep etype_info2 des3-cbc-sha1 KRBTEST.COMuser',
             'asrep etype_info des3-cbc-sha1 KRBTEST.COMuser',
             'asrep pw_salt KRBTEST.COMuser'])

# With a newer enctype in the request (even if it is not the most
# preferred enctype and doesn't match any keys), only PA-ETYPE-INFO2
# appears.
test_etinfo('user', 'rc4 aes256-cts',
            ['asrep etype_info2 rc4-hmac KRBTEST.COMuser'])

# In preauth-required errors, PA-PW-SALT does not appear, but the same
# etype-info2 values are expected.
test_etinfo('preauthuser', 'rc4-hmac-exp des3 rc4',
            ['error etype_info2 des3-cbc-sha1 KRBTEST.COMpreauthuser',
             'error etype_info des3-cbc-sha1 KRBTEST.COMpreauthuser'])
test_etinfo('preauthuser', 'rc4 aes256-cts',
            ['error etype_info2 rc4-hmac KRBTEST.COMpreauthuser'])

# If no keys are found matching the request enctypes, a
# preauth-required error can be generated with no etype-info at all
# (to allow for preauth mechs which don't depend on long-term keys).
# An AS-REP cannot be generated without preauth as there is no reply
# key.
test_etinfo('rc4user', 'des3', [])
test_etinfo('nokeyuser', 'des3', [])

# Verify that etype-info2 is included in a MORE_PREAUTH_DATA_REQUIRED
# error if the client does optimistic preauth.
mark('MORE_PREAUTH_DATA_REQUIRED test')
realm.stop()
testpreauth = os.path.join(buildtop, 'plugins', 'preauth', 'test', 'test.so')
conf = {'plugins': {'kdcpreauth': {'module': 'test:' + testpreauth},
                    'clpreauth': {'module': 'test:' + testpreauth}}}
realm = K5Realm(create_host=False, get_creds=False, krb5_conf=conf)
realm.run([kadminl, 'setstr', realm.user_princ, '2rt', '2rtval'])
out = realm.run(['./etinfo', realm.user_princ, '-123'])
if out != 'more etype_info2 aes256-cts KRBTEST.COMuser\n':
    fail('Unexpected output for MORE_PREAUTH_DATA_REQUIRED test')

success('KDC etype-info tests')
