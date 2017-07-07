#!/usr/bin/python
from k5test import *

realm = K5Realm(create_kdb=False)

keyctl = which('keyctl')
out = realm.run([klist, '-c', 'KEYRING:process:abcd'], expected_code=1)
test_keyring = (keyctl is not None and
                'Unknown credential cache type' not in out)
if not test_keyring:
    skipped('keyring collection tests', 'keyring support not built')

# Run the collection test program against each collection-enabled type.
realm.run(['./t_cccol', 'DIR:' + os.path.join(realm.testdir, 'cc')])
if test_keyring:
    def cleanup_keyring(anchor, name):
        out = realm.run(['keyctl', 'list', anchor])
        if ('keyring: ' + name + '\n') in out:
            keyid = realm.run(['keyctl', 'search', anchor, 'keyring', name])
            realm.run(['keyctl', 'unlink', keyid.strip(), anchor])

    # Use the test directory as the collection name to avoid colliding
    # with other build trees.
    cname = realm.testdir
    col_ringname = '_krb_' + cname

    # Remove any keys left behind by previous failed test runs.
    cleanup_keyring('@s', cname)
    cleanup_keyring('@s', col_ringname)
    cleanup_keyring('@u', col_ringname)

    # Run test program over each subtype, cleaning up as we go.  Don't
    # test the persistent subtype, since it supports only one
    # collection and might be in actual use.
    realm.run(['./t_cccol', 'KEYRING:' + cname])
    cleanup_keyring('@s', col_ringname)
    realm.run(['./t_cccol', 'KEYRING:legacy:' + cname])
    cleanup_keyring('@s', col_ringname)
    realm.run(['./t_cccol', 'KEYRING:session:' + cname])
    cleanup_keyring('@s', col_ringname)
    realm.run(['./t_cccol', 'KEYRING:user:' + cname])
    cleanup_keyring('@u', col_ringname)
    realm.run(['./t_cccol', 'KEYRING:process:abcd'])
    realm.run(['./t_cccol', 'KEYRING:thread:abcd'])

realm.stop()

# Test cursor semantics using real ccaches.
realm = K5Realm(create_host=False)

realm.addprinc('alice', password('alice'))
realm.addprinc('bob', password('bob'))

ccdir = os.path.join(realm.testdir, 'cc')
dccname = 'DIR:%s' % ccdir
duser = 'DIR::%s/tkt1' % ccdir
dalice = 'DIR::%s/tkt2' % ccdir
dbob = 'DIR::%s/tkt3' % ccdir
dnoent = 'DIR::%s/noent' % ccdir
realm.kinit('user', password('user'), flags=['-c', duser])
realm.kinit('alice', password('alice'), flags=['-c', dalice])
realm.kinit('bob', password('bob'), flags=['-c', dbob])

if test_keyring:
    cleanup_keyring('@s', col_ringname)
    krccname = 'KEYRING:session:' + cname
    kruser = '%s:tkt1' % krccname
    kralice = '%s:tkt2' % krccname
    krbob = '%s:tkt3' % krccname
    krnoent = '%s:noent' % krccname
    realm.kinit('user', password('user'), flags=['-c', kruser])
    realm.kinit('alice', password('alice'), flags=['-c', kralice])
    realm.kinit('bob', password('bob'), flags=['-c', krbob])

def cursor_test(testname, args, expected):
    outlines = realm.run(['./t_cccursor'] + args).splitlines()
    outlines.sort()
    expected.sort()
    if outlines != expected:
        fail('Output not expected for %s\n' % testname +
             'Expected output:\n\n' + '\n'.join(expected) + '\n\n' +
             'Actual output:\n\n' + '\n'.join(outlines))

fccname = 'FILE:%s' % realm.ccache
cursor_test('file-default', [], [fccname])
cursor_test('file-default2', [realm.ccache], [fccname])
cursor_test('file-default3', [fccname], [fccname])

cursor_test('dir', [dccname], [duser, dalice, dbob])
cursor_test('dir-subsidiary', [duser], [duser])
cursor_test('dir-nofile', [dnoent], [])

if test_keyring:
    cursor_test('keyring', [krccname], [kruser, kralice, krbob])
    cursor_test('keyring-subsidiary', [kruser], [kruser])
    cursor_test('keyring-noent', [krnoent], [])

mfoo = 'MEMORY:foo'
mbar = 'MEMORY:bar'
cursor_test('filemem', [fccname, mfoo, mbar], [fccname, mfoo, mbar])
cursor_test('dirmem', [dccname, mfoo], [duser, dalice, dbob, mfoo])
if test_keyring:
    cursor_test('keyringmem', [krccname, mfoo], [kruser, kralice, krbob, mfoo])

# Test krb5_cccol_have_content.
realm.run(['./t_cccursor', dccname, 'CONTENT'])
realm.run(['./t_cccursor', fccname, 'CONTENT'])
realm.run(['./t_cccursor', realm.ccache, 'CONTENT'])
realm.run(['./t_cccursor', mfoo, 'CONTENT'], expected_code=1)
if test_keyring:
    realm.run(['./t_cccursor', krccname, 'CONTENT'])
    cleanup_keyring('@s', col_ringname)

# Make sure FILE doesn't yield a nonexistent default cache.
realm.run([kdestroy])
cursor_test('noexist', [], [])
realm.run(['./t_cccursor', fccname, 'CONTENT'], expected_code=1)

success('Renewing credentials')
