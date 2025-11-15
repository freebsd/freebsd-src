from k5test import *
import pwd
import stat

krb5_conf = '/etc/krb5.conf'
krb5_conf_save = krb5_conf + '.save-ksutest'
krb5_conf_nosave = krb5_conf + '.nosave-ksutest'
ksu = './ksu.ksutest'
if 'SUDO_UID' not in os.environ or os.geteuid() != 0:
    fail('this script must be run as root via sudo')
caller_uid = int(os.environ['SUDO_UID'])
if caller_uid == 0:
    fail('the user invoking sudo must not be root')
caller_username = os.environ['SUDO_USER']
os.chown('testlog', caller_uid, -1)

# Set the real and effective UIDs to the calling user, but preserve
# the ability to restore root privileges.
def be_caller():
    os.setresuid(caller_uid, caller_uid, 0)


# Restore root privileges.
def be_root():
    os.setresuid(0, 0, 0)


# Remove the ksutest account.
def cleanup_user():
    # userdel commonly gives a warning about being unable to delete
    # the mail spool; filter it out.
    out = subprocess.check_output(['userdel', '-r', 'ksutest'],
                                  stderr=subprocess.STDOUT)
    if out.count(b'\n') > 1 or b'ksutest mail spool' not in out:
        print(out)


# Restore /etc/krb5.conf to the state it was in previously.
def cleanup_krb5_conf():
    if os.path.exists(krb5_conf_save):
        os.unlink(krb5_conf)
        os.rename(krb5_conf_save, krb5_conf)
    elif os.path.exists(krb5_conf_nosave):
        os.unlink(krb5_conf)
        os.unlink(krb5_conf_nosave)


def onexit():
    if len(sys.argv) >= 2 and sys.argv[1] == 'nocleanup':
        return
    be_root()
    cleanup_user()
    cleanup_krb5_conf()
    if os.path.exists(ksu):
        os.unlink(ksu)


# Create a ksutest account and return its home directory.
def setup_user():
    try:
        ent = pwd.getpwnam('ksutest')
        return ent.pw_dir
    except KeyError:
        subprocess.check_call(['useradd', '-m', '-r', 'ksutest'])
        return pwd.getpwnam('ksutest').pw_dir


# Make krb5.conf a copy of realm's krb5.conf file.  Save the old
# contents in krb5_conf_save, or create krb5_conf_noexist to indicate
# that the file didn't previously exist.
def setup_krb5_conf(realm):
    if not os.path.exists(krb5_conf):
        open(krb5_conf_nosave, 'w').close()
    elif not os.path.exists(krb5_conf_save):
        os.rename(krb5_conf, krb5_conf_save)
    shutil.copyfile(os.path.join(realm.testdir, 'krb5.conf'), krb5_conf)


# Temporarily acting as root, write a file named fname in ksutest's
# home directory with the given contents.  If wrong_owner is set, make
# the file owned by the caller uid in order to trip ksu's owner check.
def write_authz_file(fname, contents, wrong_owner=False):
    be_root()
    path = os.path.join(ksutest_home, fname)
    with open(path, 'w') as f:
        f.write('\n'.join(contents) + '\n')
    if wrong_owner:
        os.chown(path, caller_uid, -1)
    be_caller()


# Temporarily acting as root, remove fname from ksutest's home
# directory.
def remove_authz_file(fname):
    be_root()
    path = os.path.join(ksutest_home, fname)
    if os.path.exists(path):
        os.remove(path)
    be_caller()


be_caller()

# Set up a realm.  Set default_keytab_name since ksu won't respect the
# KRB5_KTNAME environment variable.
keytab = os.path.join(os.getcwd(), 'testdir', 'keytab')
realm = K5Realm(create_user=False,
                krb5_conf={'libdefaults': {'default_keytab_name': keytab}})
realm.addprinc('alice', 'pwalice')
realm.addprinc('ksutest', 'pwksutest')
realm.addprinc('ksutest/root', 'pwroot')
realm.addprinc(caller_username, 'pwcaller')

# Root setup:
# - /etc/krb5.conf is a copy of the test realm krb5.conf
# - a newly created user named ksutest exists (with homedir ksutest_home)
# - a setuid copy of ksu exists in the build dir
# Register an atexit handler to undo these changes.
atexit.register(onexit)
be_root()
ksutest_home = setup_user()
setup_krb5_conf(realm)
if os.path.exists(ksu):
    os.unlink(ksu)
shutil.copyfile('ksu', ksu)
os.chmod(ksu, 0o4755)
be_caller()

mark('no authorization')
realm.kinit('alice', 'pwalice')
realm.run([ksu, 'ksutest', '-n', 'alice', '-a', '-c', klist], expected_code=1,
          expected_msg='authorization of alice@KRBTEST.COM failed')

mark('an2ln authorization')
realm.kinit('ksutest', 'pwksutest')
realm.run([ksu, 'ksutest', '-a', '-c', klist],
          expected_msg='authorization for ksutest@KRBTEST.COM successful')
realm.run([ksu, 'ksutest', '-e', klist], expected_code=1,
          expected_msg='account ksutest: authorization failed')

mark('.k5login wrong owner')
write_authz_file('.k5login', ['ksutest@KRBTEST.COM'], wrong_owner=True)
realm.run([ksu, 'ksutest', '-e', klist], expected_code=1,
          expected_msg='account ksutest: authorization failed')
remove_authz_file('.k5login')

mark('.k5users wrong owner')
write_authz_file('.k5users', ['ksutest@KRBTEST.COM'], wrong_owner=True)
realm.run([ksu, 'ksutest', '-e', klist], expected_code=1,
          expected_msg='account ksutest: authorization failed')
remove_authz_file('.k5users')

mark('.k5login authorization')
realm.kinit('alice', 'pwalice')
write_authz_file('.k5login', ['alice@KRBTEST.COM'])
realm.run([ksu, 'ksutest', '-a', '-c', klist],
          expected_msg='authorization for alice@KRBTEST.COM successful')
realm.run([ksu, 'ksutest', '-e', klist],
          expected_msg='authorization for alice@KRBTEST.COM for execution of')
write_authz_file('.k5login', ['bob@KRBTEST.COM'])
realm.run([ksu, 'ksutest', '-a', '-c', klist], expected_code=1,
          expected_msg='account ksutest: authorization failed')
remove_authz_file('.k5login')

mark('.k5users authorization (no second field)')
write_authz_file('.k5users', ['alice@KRBTEST.COM'])
realm.run([ksu, 'ksutest', '-a', '-c', klist],
          expected_msg='authorization for alice@KRBTEST.COM successful')
realm.run([ksu, 'ksutest', '-e', klist], expected_code=1,
          expected_msg='account ksutest: authorization failed')
write_authz_file('.k5users', ['bob@KRBTEST.COM'])
realm.run([ksu, 'ksutest', '-a', '-c', klist], expected_code=1,
          expected_msg='account ksutest: authorization failed')

mark('k5users authorization (wildcard)')
write_authz_file('.k5users', ['alice@KRBTEST.COM *'])
realm.run([ksu, 'ksutest', '-a', '-c', klist],
          expected_msg='authorization for alice@KRBTEST.COM successful')
realm.run([ksu, 'ksutest', '-e', klist],
          expected_msg='authorization for alice@KRBTEST.COM for execution of')

mark('k5users authorization (command list)')
write_authz_file('.k5users', ['alice@KRBTEST.COM doesnotexist ' + klist])
realm.run([ksu, 'ksutest', '-a', '-c', klist], expected_code=1,
          expected_msg='account ksutest: authorization failed')
realm.run([ksu, 'ksutest', '-e', klist],
          expected_msg='authorization for alice@KRBTEST.COM for execution of')
realm.run([ksu, 'ksutest', '-e', kvno], expected_code=1,
          expected_msg='account ksutest: authorization failed')
realm.run([ksu, 'ksutest', '-e', 'doesnotexist'], expected_code=1,
          expected_msg='Error: not found ->')
remove_authz_file('.k5users')

mark('principal heuristic (no authz files)')
realm.run([ksu, 'ksutest', '-a', '-c', klist], input='pwksutest\n',
          expected_msg='Authenticated ksutest@KRBTEST.COM')
realm.run([ksu, 'ksutest', '-e', klist], expected_code=1,
          expected_msg='account ksutest: authorization failed')

mark('principal heuristic (empty authz files)')
write_authz_file('.k5login', [])
write_authz_file('.k5users', [])
realm.run([ksu, 'ksutest', '-e', klist], expected_code=1,
          expected_msg='account ksutest: authorization failed')
remove_authz_file('.k5login')
remove_authz_file('.k5users')

# Untested: if the ccache default principal is not authorized,
# get_best_princ_for_target() looks for a TGT or host service ticket
# for the target and source users (if authorized) or any other
# authorized user.  This is not really useful because a ccache usually
# only contains tickets for its default client principal (aside from
# caches created for S4U2Proxy).  If the heuristic is ever changed to
# search the cache collection instead of only the primary cache, we
# should add tests for that here.

mark('principal heuristic (.k5login)')
write_authz_file('.k5login', ['ksutest@KRBTEST.COM'])
realm.run([ksu, 'ksutest', '-a', '-c', klist], input='pwksutest\n',
          expected_msg='Authenticated ksutest@KRBTEST.COM')
realm.run([ksu, 'ksutest', '-e', klist], input='pwksutest\n',
          expected_msg='Authenticated ksutest@KRBTEST.COM')
write_authz_file('.k5login', [caller_username + '@KRBTEST.COM'])
realm.run([ksu, 'ksutest', '-e', klist], input='pwcaller\n',
          expected_msg='Authenticated %s@KRBTEST.COM' % caller_username)
remove_authz_file('.k5login')

mark('principal heuristic (.k5users)')
write_authz_file('.k5users', ['alice@KRBTEST.COM ' + klist,
                              'ksutest@KRBTEST.COM',
                              caller_username + '@KRBTEST.COM *'])
realm.run([ksu, 'ksutest', '-e', klist],
          expected_msg='Authenticated alice@KRBTEST.COM')
realm.run([ksu, 'ksutest', '-a', '-c', klist], input='pwksutest\n',
          expected_msg='Authenticated ksutest@KRBTEST.COM')
realm.run([ksu, 'ksutest', '-e', kvno, 'alice'], input='pwcaller\n',
          expected_msg='Authenticated %s@KRBTEST.COM' % caller_username)
write_authz_file('.k5users', ['alice@KRBTEST.COM ' + klist,
                              'ksutest/root@KRBTEST.COM ' + kvno])
realm.run([ksu, 'ksutest', '-e', kvno, 'alice'], input='pwroot\n',
          expected_msg='Authenticated ksutest/root@KRBTEST.COM')

mark('principal heuristic (no authorization)')
realm.run([ksu, '.', '-e', klist],
          expected_msg='Default principal: alice@KRBTEST.COM')
be_root()
realm.run([ksu, 'ksutest', '-e', klist], expected_code=1,
          expected_msg='No credentials cache found')
be_caller()
realm.kinit('ksutest', 'pwksutest')
be_root()
realm.run([ksu, 'ksutest', '-e', klist],
          expected_msg='Default principal: ksutest@KRBTEST.COM')
be_caller()
realm.run([kdestroy])
realm.run([ksu, '.', '-e', klist], expected_code=1,
          expected_msg='No credentials cache found')

mark('authentication without authorization')
realm.run([ksu, '.', '-n', 'ksutest', '-e', klist], input='pwksutest\n',
          expected_msg='Leaving uid as ' + caller_username)

# It's hard to make this flag do anything detectable, but we can
# exercise the code.
mark('-z flag')
realm.kinit(caller_username, 'pwcaller')
realm.run([ksu, '.', '-z', '-e', klist],
          expected_msg='Default principal: ' + caller_username)

realm.run([ksu, '.', '-Z', '-e', klist], expected_code=1,
          expected_msg='No credentials cache found')

success('ksu tests')
