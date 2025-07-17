from k5test import *

conf = {'libdefaults': {'allow_weak_crypto': 'true'}}
realm = K5Realm(create_host=False, krb5_conf=conf)

realm.run([kadminl, 'ank', '-pw', 'pw', '+preauth', 'puser'])
realm.run([kadminl, 'ank', '-nokey', 'nokey'])
realm.run([kadminl, 'ank', '-nokey', '+preauth', 'pnokey'])
realm.run([kadminl, 'ank', '-e', 'aes256-cts:special', '-pw', 'pw', 'exp'])
realm.run([kadminl, 'ank', '-e', 'aes256-cts:special', '-pw', 'pw', '+preauth',
           'pexp'])

# Extract the explicit salt values from the database.
out = realm.run([kdb5_util, 'tabdump', 'keyinfo'])
salt_dict = {f[0]: f[5] for f in [l.split('\t') for l in out.splitlines()]}
exp_salt = bytes.fromhex(salt_dict['exp@KRBTEST.COM']).decode('ascii')
pexp_salt = bytes.fromhex(salt_dict['pexp@KRBTEST.COM']).decode('ascii')

# Test an error reply (other than PREAUTH_REQUIRED).
out = realm.run(['./t_get_etype_info', 'notfound'], expected_code=1,
                expected_msg='Client not found in Kerberos database')

# Test with default salt and no specific options, with and without
# preauth.  (Our KDC always sends an explicit salt, so unfortunately
# we aren't really testing client handling of the default salt.)
realm.run(['./t_get_etype_info', 'user'],
          expected_msg='etype: aes256-cts\nsalt: KRBTEST.COMuser\n')
realm.run(['./t_get_etype_info', 'puser'],
          expected_msg='etype: aes256-cts\nsalt: KRBTEST.COMpuser\n')

# Test with a specified request enctype.
msg = 'etype: aes128-cts\nsalt: KRBTEST.COMuser\n'
realm.run(['./t_get_etype_info', '-e', 'aes128-cts', 'user'],
          expected_msg='etype: aes128-cts\nsalt: KRBTEST.COMuser\n')
realm.run(['./t_get_etype_info', '-e', 'aes128-cts', 'puser'],
          expected_msg='etype: aes128-cts\nsalt: KRBTEST.COMpuser\n')

# Test with FAST.
msg = 'etype: aes256-cts\nsalt: KRBTEST.COMuser\n'
realm.run(['./t_get_etype_info', '-T', realm.ccache, 'user'],
          expected_msg='etype: aes256-cts\nsalt: KRBTEST.COMuser\n')
realm.run(['./t_get_etype_info', '-T', realm.ccache, 'puser'],
          expected_msg='etype: aes256-cts\nsalt: KRBTEST.COMpuser\n')

# Test with no available etype-info.
realm.run(['./t_get_etype_info', 'nokey'], expected_code=1,
          expected_msg='KDC has no support for encryption type')
realm.run(['./t_get_etype_info', 'pnokey'], expected_msg='no etype-info')

# Test with explicit salt.
realm.run(['./t_get_etype_info', 'exp'],
          expected_msg='etype: aes256-cts\nsalt: ' + exp_salt + '\n')
realm.run(['./t_get_etype_info', 'pexp'],
          expected_msg='etype: aes256-cts\nsalt: ' + pexp_salt + '\n')

success('krb5_get_etype_info() tests')
