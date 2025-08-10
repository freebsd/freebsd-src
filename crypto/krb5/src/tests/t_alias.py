from k5test import *

realm = K5Realm(create_host=False)

mark('getprinc')
realm.addprinc('canon')
realm.run([kadminl, 'alias', 'alias', 'canon@KRBTEST.COM'])
realm.run([kadminl, 'getprinc', 'alias'],
          expected_msg='Principal: canon@KRBTEST.COM')

mark('delprinc')
realm.run([kadminl, 'delprinc', 'alias'])
realm.run([kadminl, 'getprinc', 'alias'], expected_code=1,
          expected_msg='does not exist')
realm.run([kadminl, 'getprinc', 'canon'], expected_msg=': canon@KRBTEST.COM')

mark('no specified realm')
realm.run([kadminl, 'alias', 'alias', 'canon'])
realm.run([kadminl, 'getprinc', 'alias'], expected_msg=': canon@KRBTEST.COM')

mark('cross-realm')
realm.run([kadminl, 'alias', 'x', 'y@OTHER.REALM'], expected_code=1,
          expected_msg='Alias target must be within the same realm')

mark('alias as service principal')
realm.extract_keytab('alias', realm.keytab)
realm.run([kvno, 'alias'])
realm.klist('user@KRBTEST.COM', 'alias@KRBTEST.COM')

mark('alias as client principal')
realm.kinit('alias', flags=['-k'])
realm.klist('alias@KRBTEST.COM')
realm.kinit('alias', flags=['-k', '-C'])
realm.klist('canon@KRBTEST.COM')

mark('chain')
realm.run([kadminl, 'alias', 'a1', 'canon'])
realm.run([kadminl, 'alias', 'a2', 'a1'])
realm.run([kadminl, 'alias', 'a3', 'a2'])
realm.run([kadminl, 'alias', 'a4', 'a3'])
realm.run([kadminl, 'alias', 'a5', 'a4'])
realm.run([kadminl, 'alias', 'a6', 'a5'])
realm.run([kadminl, 'alias', 'a7', 'a6'])
realm.run([kadminl, 'alias', 'a8', 'a7'])
realm.run([kadminl, 'alias', 'a9', 'a8'])
realm.run([kadminl, 'alias', 'a10', 'a9'])
realm.run([kadminl, 'alias', 'a11', 'a10'])
realm.run([kvno, 'a1'])
realm.run([kvno, 'a2'])
realm.run([kvno, 'a3'])
realm.run([kvno, 'a4'])
realm.run([kvno, 'a5'])
realm.run([kvno, 'a6'])
realm.run([kvno, 'a7'])
realm.run([kvno, 'a8'])
realm.run([kvno, 'a9'])
realm.run([kvno, 'a10'])
realm.run([kvno, 'a11'], expected_code=1,
          expected_msg='Server a11@KRBTEST.COM not found in Kerberos database')

mark('circular chain')
realm.run([kadminl, 'alias', 'selfalias', 'selfalias'])
realm.run([kvno, 'selfalias'], expected_code=1,
          expected_msg='Server selfalias@KRBTEST.COM not found')

mark('blocking creations')
realm.run([kadminl, 'addprinc', '-nokey', 'alias'], expected_code=1,
          expected_msg='already exists')
realm.run([kadminl, 'alias', 'alias', 'canon'], expected_code=1,
          expected_msg='already exists')
realm.run([kadminl, 'renprinc', 'user', 'alias'], expected_code=1,
          expected_msg='already exists')

# Non-resolving aliases being overwritable is emergent behavior;
# change the tests if the behavior changes.
mark('not blocking creations')
realm.run([kadminl, 'alias', 'xa1', 'x'])
realm.run([kadminl, 'alias', 'xa2', 'x'])
realm.run([kadminl, 'alias', 'xa3', 'x'])
realm.addprinc('xa1')
realm.run([kadminl, 'getprinc', 'xa1'], expected_msg=': xa1@KRBTEST.COM')
realm.run([kadminl, 'alias', 'xa2', 'canon'])
realm.run([kadminl, 'getprinc', 'xa2'], expected_msg=': canon@KRBTEST.COM')
realm.run([kadminl, 'renprinc', 'xa1', 'xa3'])
realm.run([kadminl, 'getprinc', 'xa3'], expected_msg=': xa3@KRBTEST.COM')

mark('renprinc')
realm.run([kadminl, 'renprinc', 'alias', 'nalias'], expected_code=1,
          expected_msg='Operation unsupported on alias principal name')

mark('modprinc')
realm.run([kadminl, 'modprinc', '+preauth', 'alias'])
realm.run([kadminl, 'getprinc', 'canon'], expected_msg='REQUIRES_PRE_AUTH')

mark('cpw')
realm.run([kadminl, 'cpw', '-pw', 'pw', 'alias'])
realm.run([kadminl, 'getprinc', 'canon'], expected_msg='vno 2,')
realm.run([kadminl, 'cpw', '-e', 'aes256-cts', '-pw', 'pw', 'alias'])
realm.run([kadminl, 'getprinc', 'canon'], expected_msg='vno 3,')
realm.run([kadminl, 'cpw', '-randkey', 'alias'])
realm.run([kadminl, 'getprinc', 'canon'], expected_msg='vno 4,')
realm.run([kadminl, 'cpw', '-e', 'aes256-cts', '-randkey', 'alias'])
realm.run([kadminl, 'getprinc', 'canon'], expected_msg='vno 5,')

mark('listprincs')
realm.run([kadminl, 'listprincs'], expected_msg='alias@KRBTEST.COM')

mark('purgekeys')
realm.run([kadminl, 'purgekeys', '-all', 'alias'])
realm.run([kadminl, 'getprinc', 'canon'], expected_msg='Number of keys: 0')

mark('setstr')
realm.run([kadminl, 'setstr', 'alias', 'key', 'value'])
realm.run([kadminl, 'getstrs', 'canon'], expected_msg='key: value')

mark('getstrs')
realm.run([kadminl, 'getstrs', 'alias'], expected_msg='key: value')

mark('delstr')
realm.run([kadminl, 'delstr', 'alias', 'key'])
realm.run([kadminl, 'getstrs', 'canon'],
          expected_msg='(No string attributes.)')

success('alias tests')
