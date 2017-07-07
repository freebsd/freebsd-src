#!/usr/bin/python

# Copyright (C) 2011 by the Massachusetts Institute of Technology.
# All rights reserved.

# Export of this software from the United States of America may
#   require a specific license from the United States Government.
#   It is the responsibility of any person or organization contemplating
#   export to obtain such a license before exporting.
#
# WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
# distribute this software and its documentation for any purpose and
# without fee is hereby granted, provided that the above copyright
# notice appear in all copies and that both that copyright notice and
# this permission notice appear in supporting documentation, and that
# the name of M.I.T. not be used in advertising or publicity pertaining
# to distribution of the software without specific, written prior
# permission.  Furthermore if you modify this software you must label
# your software as modified software and not distribute it in such a
# fashion that it might be confused with the original M.I.T. software.
# M.I.T. makes no representations about the suitability of
# this software for any purpose.  It is provided "as is" without express
# or implied warranty.

from k5test import *

realm = K5Realm(create_host=False)

keyctl = which('keyctl')
out = realm.run([klist, '-c', 'KEYRING:process:abcd'], expected_code=1)
test_keyring = (keyctl is not None and
                'Unknown credential cache type' not in out)
if not test_keyring:
    skipped('keyring ccache tests', 'keyring support not built')

# Test kdestroy and klist of a non-existent ccache.
realm.run([kdestroy])
output = realm.run([klist], expected_code=1)
if 'No credentials cache found' not in output:
    fail('Expected error message not seen in klist output')

# Test kinit with an inaccessible ccache.
out = realm.run([kinit, '-c', 'testdir/xx/yy', realm.user_princ],
                input=(password('user') + '\n'), expected_code=1)
if 'Failed to store credentials' not in out:
    fail('Expected error message not seen in kinit output')

# Test klist -s with a single ccache.
realm.run([klist, '-s'], expected_code=1)
realm.kinit(realm.user_princ, password('user'))
realm.run([klist, '-s'])
realm.kinit(realm.user_princ, password('user'), ['-l', '-1s'])
realm.run([klist, '-s'], expected_code=1)
realm.kinit(realm.user_princ, password('user'), ['-S', 'kadmin/admin'])
realm.run([klist, '-s'])
realm.run([kdestroy])
realm.run([klist, '-s'], expected_code=1)

realm.addprinc('alice', password('alice'))
realm.addprinc('bob', password('bob'))
realm.addprinc('carol', password('carol'))

def collection_test(realm, ccname):
    realm.env['KRB5CCNAME'] = ccname

    realm.run([klist, '-A', '-s'], expected_code=1)
    realm.kinit('alice', password('alice'))
    output = realm.run([klist])
    if 'Default principal: alice@' not in output:
        fail('Initial kinit failed to get credentials for alice.')
    realm.run([klist, '-A', '-s'])
    realm.run([kdestroy])
    output = realm.run([klist], expected_code=1)
    if 'No credentials cache' not in output and 'not found' not in output:
        fail('Initial kdestroy failed to destroy primary cache.')
    output = realm.run([klist, '-l'], expected_code=1)
    if not output.endswith('---\n') or output.count('\n') != 2:
        fail('Initial kdestroy failed to empty cache collection.')
    realm.run([klist, '-A', '-s'], expected_code=1)

    realm.kinit('alice', password('alice'))
    realm.kinit('carol', password('carol'))
    output = realm.run([klist, '-l'])
    if '---\ncarol@' not in output or '\nalice@' not in output:
        fail('klist -l did not show expected output after two kinits.')
    realm.kinit('alice', password('alice'))
    output = realm.run([klist, '-l'])
    if '---\nalice@' not in output or output.count('\n') != 4:
        fail('klist -l did not show expected output after re-kinit for alice.')
    realm.kinit('bob', password('bob'))
    output = realm.run([klist, '-A'])
    if 'bob@' not in output.splitlines()[1] or 'alice@' not in output or \
            'carol' not in output or output.count('Default principal:') != 3:
        fail('klist -A did not show expected output after kinit for bob.')
    realm.run([kswitch, '-p', 'carol'])
    output = realm.run([klist, '-l'])
    if '---\ncarol@' not in output or output.count('\n') != 5:
        fail('klist -l did not show expected output after kswitch to carol.')
    realm.run([kdestroy])
    output = realm.run([klist, '-l'])
    if 'carol@' in output or 'bob@' not in output or output.count('\n') != 4:
        fail('kdestroy failed to remove only primary ccache.')
    realm.run([klist, '-s'], expected_code=1)
    realm.run([klist, '-A', '-s'])
    realm.run([kdestroy, '-A'])
    output = realm.run([klist, '-l'], expected_code=1)
    if not output.endswith('---\n') or output.count('\n') != 2:
        fail('kdestroy -a failed to empty cache collection.')
    realm.run([klist, '-A', '-s'], expected_code=1)


collection_test(realm, 'DIR:' + os.path.join(realm.testdir, 'cc'))
if test_keyring:
    def cleanup_keyring(anchor, name):
        out = realm.run(['keyctl', 'list', anchor])
        if ('keyring: ' + name + '\n') in out:
            keyid = realm.run(['keyctl', 'search', anchor, 'keyring', name])
            realm.run(['keyctl', 'unlink', keyid.strip(), anchor])

    # Use realm.testdir as the collection name to avoid conflicts with
    # other build trees.
    cname = realm.testdir
    col_ringname = '_krb_' + cname

    cleanup_keyring('@s', col_ringname)
    collection_test(realm, 'KEYRING:session:' + cname)
    cleanup_keyring('@s', col_ringname)

    # Test legacy keyring cache linkage.
    realm.env['KRB5CCNAME'] = 'KEYRING:' + cname
    realm.run([kdestroy, '-A'])
    realm.kinit(realm.user_princ, password('user'))
    out = realm.run([klist, '-l'])
    if 'KEYRING:legacy:' + cname + ':' + cname not in out:
        fail('Wrong initial primary name in keyring legacy collection')
    # Make sure this cache is linked to the session keyring.
    id = realm.run([keyctl, 'search', '@s', 'keyring', cname])
    out = realm.run([keyctl, 'list', id.strip()])
    if 'user: __krb5_princ__' not in out:
        fail('Legacy cache not linked into session keyring')
    # Remove the collection keyring.  When the collection is
    # reinitialized, the legacy cache should reappear inside it
    # automatically as the primary cache.
    cleanup_keyring('@s', col_ringname)
    out = realm.run([klist])
    if realm.user_princ not in out:
        fail('Cannot see legacy cache after removing collection')
    coll_id = realm.run([keyctl, 'search', '@s', 'keyring', '_krb_' + cname])
    out = realm.run([keyctl, 'list', coll_id.strip()])
    if (id.strip() + ':') not in out:
        fail('Legacy cache did not reappear in collection after klist')
    # Destroy the cache and check that it is unlinked from the session keyring.
    realm.run([kdestroy])
    realm.run([keyctl, 'search', '@s', 'keyring', cname], expected_code=1)
    cleanup_keyring('@s', col_ringname)

# Test parameter expansion in default_ccache_name
realm.stop()
conf = {'libdefaults': {'default_ccache_name': 'testdir/%{null}abc%{uid}'}}
realm = K5Realm(krb5_conf=conf, create_kdb=False)
del realm.env['KRB5CCNAME']
uidstr = str(os.getuid())
out = realm.run([klist], expected_code=1)
if 'testdir/abc%s' % uidstr not in out:
    fail('Wrong ccache in klist')

success('Credential cache tests')
