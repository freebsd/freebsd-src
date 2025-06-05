from k5test import *

# Test gss_export_cred and gss_import_cred for initiator creds,
# acceptor creds, and traditional delegated creds.  t_s4u.py tests
# exporting and importing a synthesized S4U2Proxy delegated
# credential.

# Make up a filename to hold user's initial credentials.
def ccache_savefile(realm):
    return os.path.join(realm.testdir, 'ccache.copy')

# Move user's initial credentials into the save file.
def ccache_save(realm):
    os.rename(realm.ccache, ccache_savefile(realm))

# Copy user's initial credentials from the save file into the ccache.
def ccache_restore(realm):
    shutil.copyfile(ccache_savefile(realm), realm.ccache)

# Run t_export_cred with the saved ccache and verify that it stores a
# forwarded cred into the default ccache.
def check(realm, args):
    ccache_restore(realm)
    realm.run(['./t_export_cred'] + args)
    realm.run([klist, '-f'], expected_msg='Flags: Ff')

# Check a given set of arguments with no specified mech and with krb5
# and SPNEGO as the specified mech.
def check_mechs(realm, args):
    check(realm, args)
    check(realm, ['-k'] + args)
    check(realm, ['-s'] + args)

# Make a realm, get forwardable tickets, and save a copy for each test.
realm = K5Realm(get_creds=False)
realm.kinit(realm.user_princ, password('user'), ['-f'])
ccache_save(realm)

# Test with default initiator and acceptor cred.
tname = 'p:' + realm.host_princ
check_mechs(realm, [tname])

# Test with principal-named initiator and acceptor cred.
iname = 'p:' + realm.user_princ
check_mechs(realm, ['-i', iname, '-a', tname, tname])

# Test with host-based acceptor cred.
check_mechs(realm, ['-a', 'h:host', tname])

success('gss_export_cred/gss_import_cred tests')
