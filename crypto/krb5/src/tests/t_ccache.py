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
import tempfile

socketdir = tempfile.TemporaryDirectory()
kcm_socket_path = os.path.join(socketdir.name, 'kcm')
conf = {'libdefaults': {'kcm_socket': kcm_socket_path,
                        'kcm_mach_service': '-'}}
realm = K5Realm(krb5_conf=conf)

realm.addprinc('contest')
realm.extract_keytab('contest', realm.keytab)
realm.run(['./conccache', realm.ccache + '.contest', 'contest',
           realm.host_princ])

keyctl = which('keyctl')
out = realm.run([klist, '-c', 'KEYRING:process:abcd'], expected_code=1)
test_keyring = (keyctl is not None and
                'Unknown credential cache type' not in out)
if not test_keyring:
    skipped('keyring ccache tests', 'keyring support not built')

# Test kdestroy and klist of a non-existent ccache.
mark('no ccache')
realm.run([kdestroy])
realm.run([klist], expected_code=1, expected_msg='No credentials cache found')

# Test kinit with an inaccessible ccache.
mark('inaccessible ccache')
realm.kinit(realm.user_princ, password('user'), flags=['-c', 'testdir/xx/yy'],
            expected_code=1, expected_msg='Failed to store credentials')

# Test klist -s with a single ccache.
mark('klist -s single ccache')
realm.run([klist, '-s'], expected_code=1)
realm.kinit(realm.user_princ, password('user'))
realm.run([klist, '-s'])
realm.kinit(realm.user_princ, password('user'), ['-l', '-10s'])
realm.run([klist, '-s'], expected_code=1)
realm.kinit(realm.user_princ, password('user'), ['-S', 'kadmin/admin'])
realm.run([klist, '-s'])
realm.run([kdestroy])
realm.run([klist, '-s'], expected_code=1)

realm.addprinc('alice', password('alice'))
realm.addprinc('bob', password('bob'))
realm.addprinc('carol', password('carol'))
realm.addprinc('doug', password('doug'))

def collection_test(realm, ccname):
    cctype = ccname.partition(':')[0]
    oldccname = realm.env['KRB5CCNAME']
    realm.env['KRB5CCNAME'] = ccname

    mark('%s collection, single cache' % cctype)
    realm.run([klist, '-A', '-s'], expected_code=1)
    realm.kinit('alice', password('alice'))
    realm.run([klist], expected_msg='Default principal: alice@')
    realm.run([klist, '-A', '-s'])
    realm.run([kvno, realm.host_princ], expected_msg = 'kvno = 1')
    realm.run([kvno, realm.host_princ], expected_msg = 'kvno = 1')
    out = realm.run([klist])
    if out.count(realm.host_princ) != 1:
        fail('Wrong number of service tickets in cache')
    realm.run([kdestroy])
    output = realm.run([klist], expected_code=1)
    if 'No credentials cache' not in output and 'not found' not in output:
        fail('Initial kdestroy failed to destroy primary cache.')
    output = realm.run([klist, '-l'], expected_code=1)
    if not output.endswith('---\n') or output.count('\n') != 2:
        fail('Initial kdestroy failed to empty cache collection.')
    realm.run([klist, '-A', '-s'], expected_code=1)

    mark('%s collection, multiple caches' % cctype)
    realm.kinit('alice', password('alice'))
    realm.kinit('carol', password('carol'))
    output = realm.run([klist, '-l'])
    if '---\ncarol@' not in output or '\nalice@' not in output:
        fail('klist -l did not show expected output after two kinits.')
    realm.kinit('alice', password('alice'))
    output = realm.run([klist, '-l'])
    if '---\nalice@' not in output or output.count('\n') != 4:
        fail('klist -l did not show expected output after re-kinit for alice.')
    realm.kinit('doug', password('doug'))
    realm.kinit('bob', password('bob'))
    output = realm.run([klist, '-A', ccname])
    if 'bob@' not in output.splitlines()[1] or 'alice@' not in output or \
       'carol@' not in output or 'doug@' not in output or \
       output.count('Default principal:') != 4:
        fail('klist -A did not show expected output after kinit doug+bob.')
    realm.run([kswitch, '-p', 'carol'])
    output = realm.run([klist, '-l'])
    if '---\ncarol@' not in output or output.count('\n') != 6:
        fail('klist -l did not show expected output after kswitch to carol.')

    # Switch to specifying the collection name on the command line
    # (only works with klist/kdestroy for now, not kinit/kswitch).
    realm.env['KRB5CCNAME'] = oldccname

    mark('%s collection, command-line specifier' % cctype)
    realm.run([kdestroy, '-c', ccname])
    output = realm.run([klist, '-l', ccname])
    if 'carol@' in output or 'bob@' not in output or output.count('\n') != 5:
        fail('kdestroy failed to remove only primary ccache.')
    realm.run([klist, '-s', ccname], expected_code=1)
    realm.run([klist, '-A', '-s', ccname])
    realm.run([kdestroy, '-p', 'alice', '-c', ccname])
    output = realm.run([klist, '-l', ccname])
    if 'alice@' in output or 'bob@' not in output or output.count('\n') != 4:
        fail('kdestroy -p failed to remove alice')
    realm.run([kdestroy, '-A', '-c', ccname])
    output = realm.run([klist, '-l', ccname], expected_code=1)
    if not output.endswith('---\n') or output.count('\n') != 2:
        fail('kdestroy -a failed to empty cache collection.')
    realm.run([klist, '-A', '-s', ccname], expected_code=1)


collection_test(realm, 'DIR:' + os.path.join(realm.testdir, 'cc'))

# Test KCM with and without RETRIEVE and GET_CRED_LIST support.
kcmserver_path = os.path.join(srctop, 'tests', 'kcmserver.py')
kcmd = realm.start_server([sys.executable, kcmserver_path, kcm_socket_path],
                          'starting...')
collection_test(realm, 'KCM:')
stop_daemon(kcmd)
os.remove(kcm_socket_path)
realm.start_server([sys.executable, kcmserver_path, '-f', kcm_socket_path],
                   'starting...')
collection_test(realm, 'KCM:')

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
    mark('legacy keyring cache linkage')
    realm.env['KRB5CCNAME'] = 'KEYRING:' + cname
    realm.run([kdestroy, '-A'])
    realm.kinit(realm.user_princ, password('user'))
    msg = 'KEYRING:legacy:' + cname + ':' + cname
    realm.run([klist, '-l'], expected_msg=msg)
    # Make sure this cache is linked to the session keyring.
    id = realm.run([keyctl, 'search', '@s', 'keyring', cname])
    realm.run([keyctl, 'list', id.strip()],
              expected_msg='user: __krb5_princ__')
    # Remove the collection keyring.  When the collection is
    # reinitialized, the legacy cache should reappear inside it
    # automatically as the primary cache.
    cleanup_keyring('@s', col_ringname)
    realm.run([klist], expected_msg=realm.user_princ)
    coll_id = realm.run([keyctl, 'search', '@s', 'keyring', '_krb_' + cname])
    msg = id.strip() + ':'
    realm.run([keyctl, 'list', coll_id.strip()], expected_msg=msg)
    # Destroy the cache and check that it is unlinked from the session keyring.
    realm.run([kdestroy])
    realm.run([keyctl, 'search', '@s', 'keyring', cname], expected_code=1)
    cleanup_keyring('@s', col_ringname)

# Test parameter expansion in default_ccache_name
mark('default_ccache_name parameter expansion')
realm.stop()
conf = {'libdefaults': {'default_ccache_name': 'testdir/%{null}abc%{uid}'}}
realm = K5Realm(krb5_conf=conf, create_kdb=False)
del realm.env['KRB5CCNAME']
uidstr = str(os.getuid())
msg = 'testdir/abc%s' % uidstr
realm.run([klist], expected_code=1, expected_msg=msg)

success('Credential cache tests')
