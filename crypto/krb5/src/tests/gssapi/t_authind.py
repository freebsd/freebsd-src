from k5test import *

# Test authentication indicators.  Load the test preauth module so we
# can control the indicators asserted.
testpreauth = os.path.join(buildtop, 'plugins', 'preauth', 'test', 'test.so')
conf = {'plugins': {'kdcpreauth': {'module': 'test:' + testpreauth},
                    'clpreauth': {'module': 'test:' + testpreauth}}}
realm = K5Realm(krb5_conf=conf)
realm.run([kadminl, 'addprinc', '-randkey', 'service/1'])
realm.run([kadminl, 'addprinc', '-randkey', 'service/2'])
realm.run([kadminl, 'modprinc', '+requires_preauth', realm.user_princ])
realm.run([kadminl, 'setstr', 'service/1', 'require_auth', 'superstrong'])
realm.run([kadminl, 'setstr', 'service/2', 'require_auth', 'one two'])
realm.run([kadminl, 'xst', 'service/1'])
realm.run([kadminl, 'xst', 'service/2'])

realm.kinit(realm.user_princ, password('user'),
            ['-X', 'indicators=superstrong'])
out = realm.run(['./t_srcattrs', 'p:service/1'])
if ('Attribute auth-indicators Authenticated Complete') not in out:
    fail('Expected attribute type data not seen')
# UTF8 "superstrong"
if '73757065727374726f6e67' not in out:
    fail('Expected auth indicator not seen in name attributes')

msg = 'gss_init_sec_context: KDC policy rejects request'
realm.run(['./t_srcattrs', 'p:service/2'], expected_code=1, expected_msg=msg)

realm.kinit(realm.user_princ, password('user'), ['-X', 'indicators=one two'])
out = realm.run(['./t_srcattrs', 'p:service/2'])
# Hexadecimal "one" and "two"
if '6f6e65' not in out or '74776f' not in out:
    fail('Expected auth indicator not seen in name attributes')

realm.stop()

# Test the FAST encrypted challenge auth indicator.
kdcconf = {'realms': {'$realm': {'encrypted_challenge_indicator': 'fast'}}}
realm = K5Realm(kdc_conf=kdcconf)
realm.run([kadminl, 'modprinc', '+requires_preauth', realm.user_princ])
realm.run([kadminl, 'xst', realm.host_princ])
realm.kinit(realm.user_princ, password('user'))
realm.kinit(realm.user_princ, password('user'), ['-T', realm.ccache])
out = realm.run(['./t_srcattrs', 'p:' + realm.host_princ])
if ('Attribute auth-indicators Authenticated Complete') not in out:
    fail('Expected attribute type not seen')
if '66617374' not in out:
    fail('Expected auth indicator not seen in name attributes')

realm.stop()
success('GSSAPI auth indicator tests')
