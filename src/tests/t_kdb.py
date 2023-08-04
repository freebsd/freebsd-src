from k5test import *
import time

# Run kdbtest against the non-LDAP KDB modules.
for realm in multidb_realms(create_kdb=False):
    realm.run(['./kdbtest'])

# Set up an OpenLDAP test server if we can.

if (not os.path.exists(os.path.join(plugins, 'kdb', 'kldap.so')) and
    not os.path.exists(os.path.join(buildtop, 'lib', 'libkdb_ldap.a'))):
    skip_rest('LDAP KDB tests', 'LDAP KDB module not built')

if 'SLAPD' not in os.environ and not which('slapd'):
    skip_rest('LDAP KDB tests', 'slapd not found')

slapadd = which('slapadd')
if not slapadd:
    skip_rest('LDAP KDB tests', 'slapadd not found')

ldapdir = os.path.abspath('ldap')
dbdir = os.path.join(ldapdir, 'ldap')
slapd_conf = os.path.join(ldapdir, 'slapd.d')
slapd_out = os.path.join(ldapdir, 'slapd.out')
slapd_pidfile = os.path.join(ldapdir, 'pid')
ldap_pwfile = os.path.join(ldapdir, 'pw')
ldap_sock = os.path.join(ldapdir, 'sock')
ldap_uri = 'ldapi://%s/' % ldap_sock.replace(os.path.sep, '%2F')
schema = os.path.join(srctop, 'plugins', 'kdb', 'ldap', 'libkdb_ldap',
                      'kerberos.openldap.ldif')
top_dn = 'cn=krb5'
admin_dn = 'cn=admin,cn=krb5'
admin_pw = 'admin'

shutil.rmtree(ldapdir, True)
os.mkdir(ldapdir)
os.mkdir(slapd_conf)
os.mkdir(dbdir)

if 'SLAPD' in os.environ:
    slapd = os.environ['SLAPD']
else:
    # Some Linux installations have AppArmor or similar restrictions
    # on the slapd binary, which would prevent it from accessing the
    # build directory.  Try to defeat this by copying the binary.
    system_slapd = which('slapd')
    slapd = os.path.join(ldapdir, 'slapd')
    shutil.copy(system_slapd, slapd)

def slap_add(ldif):
    proc = subprocess.Popen([slapadd, '-b', 'cn=config', '-F', slapd_conf],
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, universal_newlines=True)
    (out, dummy) = proc.communicate(ldif)
    output(out)
    return proc.wait()


# Configure the pid file and some authorization rules we will need for
# SASL testing.
if slap_add('dn: cn=config\n'
            'objectClass: olcGlobal\n'
            'olcPidFile: %s\n'
            'olcAuthzRegexp: '
            '".*uidNumber=%d,cn=peercred,cn=external,cn=auth" "%s"\n'
            'olcAuthzRegexp: "uid=digestuser,cn=digest-md5,cn=auth" "%s"\n' %
            (slapd_pidfile, os.geteuid(), admin_dn, admin_dn)) != 0:
    skip_rest('LDAP KDB tests', 'slapd basic configuration failed')

# Find a working writable database type, trying mdb (added in OpenLDAP
# 2.4.27) and bdb (deprecated and sometimes not built due to licensing
# incompatibilities).
for dbtype in ('mdb', 'bdb'):
    # Try to load the module.  This could fail if OpenLDAP is built
    # without module support, so ignore errors.
    slap_add('dn: cn=module,cn=config\n'
             'objectClass: olcModuleList\n'
             'olcModuleLoad: back_%s\n' % dbtype)

    dbclass = 'olc%sConfig' % dbtype.capitalize()
    if slap_add('dn: olcDatabase=%s,cn=config\n'
                'objectClass: olcDatabaseConfig\n'
                'objectClass: %s\n'
                'olcSuffix: %s\n'
                'olcRootDN: %s\n'
                'olcRootPW: %s\n'
                'olcDbDirectory: %s\n' %
                (dbtype, dbclass, top_dn, admin_dn, admin_pw, dbdir)) == 0:
        break
else:
    skip_rest('LDAP KDB tests', 'could not find working slapd db type')

if slap_add('include: file://%s\n' % schema) != 0:
    skip_rest('LDAP KDB tests', 'failed to load Kerberos schema')

# Load the core schema if we can.
ldap_homes = ['/etc/ldap', '/etc/openldap', '/usr/local/etc/openldap',
              '/usr/local/etc/ldap']
local_schema_path = '/schema/core.ldif'
core_schema = next((i for i in map(lambda x:x+local_schema_path, ldap_homes)
                    if os.path.isfile(i)), None)
if core_schema:
    if slap_add('include: file://%s\n' % core_schema) != 0:
        core_schema = None

slapd_pid = -1
def kill_slapd():
    global slapd_pid
    if slapd_pid != -1:
        os.kill(slapd_pid, signal.SIGTERM)
        slapd_pid = -1
atexit.register(kill_slapd)

out = open(slapd_out, 'w')
subprocess.call([slapd, '-h', ldap_uri, '-F', slapd_conf], stdout=out,
                stderr=out, universal_newlines=True)
out.close()
pidf = open(slapd_pidfile, 'r')
slapd_pid = int(pidf.read())
pidf.close()
output('*** Started slapd (pid %d, output in %s)\n' % (slapd_pid, slapd_out))

# slapd detaches before it finishes setting up its listener sockets
# (they are bound but listen() has not been called).  Give it a second
# to finish.
time.sleep(1)

# Run kdbtest against the LDAP module.
conf = {'realms': {'$realm': {'database_module': 'ldap'}},
        'dbmodules': {'ldap': {'db_library': 'kldap',
                               'ldap_kerberos_container_dn': top_dn,
                               'ldap_kdc_dn': admin_dn,
                               'ldap_kadmind_dn': admin_dn,
                               'ldap_service_password_file': ldap_pwfile,
                               'ldap_servers': ldap_uri}}}
realm = K5Realm(create_kdb=False, kdc_conf=conf)
input = admin_pw + '\n' + admin_pw + '\n'
realm.run([kdb5_ldap_util, 'stashsrvpw', admin_dn], input=input)
realm.run(['./kdbtest'])

# Run a kdb5_ldap_util command using the test server's admin DN and password.
def kldaputil(args, **kw):
    return realm.run([kdb5_ldap_util, '-D', admin_dn, '-w', admin_pw] + args,
                     **kw)

# kdbtest can't currently clean up after itself since the LDAP module
# doesn't support krb5_db_destroy.  So clean up after it with
# kdb5_ldap_util before proceeding.
kldaputil(['destroy', '-f'])

ldapmodify = which('ldapmodify')
ldapsearch = which('ldapsearch')
if not ldapmodify or not ldapsearch:
    skip_rest('some LDAP KDB tests', 'ldapmodify or ldapsearch not found')

def ldap_search(args):
    proc = subprocess.Popen([ldapsearch, '-H', ldap_uri, '-b', top_dn,
                             '-D', admin_dn, '-w', admin_pw, args],
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, universal_newlines=True)
    (out, dummy) = proc.communicate()
    return out

def ldap_modify(ldif, args=[]):
    proc = subprocess.Popen([ldapmodify, '-H', ldap_uri, '-D', admin_dn,
                             '-x', '-w', admin_pw] + args,
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, universal_newlines=True)
    (out, dummy) = proc.communicate(ldif)
    output(out)

def ldap_add(dn, objectclass, attrs=[]):
    in_data = 'dn: %s\nobjectclass: %s\n' % (dn, objectclass)
    in_data += '\n'.join(attrs) + '\n'
    ldap_modify(in_data, ['-a'])

# Create krbContainer objects for use as subtrees.
ldap_add('cn=t1,cn=krb5', 'krbContainer')
ldap_add('cn=t2,cn=krb5', 'krbContainer')
ldap_add('cn=x,cn=t1,cn=krb5', 'krbContainer')
ldap_add('cn=y,cn=t2,cn=krb5', 'krbContainer')

# Create a realm, exercising all of the realm options.
kldaputil(['create', '-s', '-P', 'master', '-subtrees', 'cn=t2,cn=krb5',
           '-containerref', 'cn=t2,cn=krb5', '-sscope', 'one',
           '-maxtktlife', '5min', '-maxrenewlife', '10min', '-allow_svr'])

# Modify the realm, exercising overlapping subtree pruning.
kldaputil(['modify', '-subtrees',
           'cn=x,cn=t1,cn=krb5:cn=t1,cn=krb5:cn=t2,cn=krb5:cn=y,cn=t2,cn=krb5',
           '-containerref', 'cn=t1,cn=krb5', '-sscope', 'sub',
           '-maxtktlife', '5hour', '-maxrenewlife', '10hour', '+allow_svr'])

out = kldaputil(['list'])
if out != 'KRBTEST.COM\n':
    fail('Unexpected kdb5_ldap_util list output')

# Create a principal at a specified DN.  This is a little dodgy
# because we're sticking a krbPrincipalAux objectclass onto a subtree
# krbContainer, but it works and it avoids having to load core.schema
# in the test LDAP server.
mark('LDAP specified dn')
realm.run([kadminl, 'ank', '-randkey', '-x', 'dn=cn=krb5', 'princ1'],
          expected_code=1, expected_msg='DN is out of the realm subtree')
# Check that the DN container check is a hierarchy test, not a simple
# suffix match (CVE-2018-5730).  We expect this operation to fail
# either way (because "xcn" isn't a valid DN tag) but the container
# check should happen before the DN is parsed.
realm.run([kadminl, 'ank', '-randkey', '-x', 'dn=xcn=t1,cn=krb5', 'princ1'],
          expected_code=1, expected_msg='DN is out of the realm subtree')
realm.run([kadminl, 'ank', '-randkey', '-x', 'dn=cn=t2,cn=krb5', 'princ1'])
realm.run([kadminl, 'getprinc', 'princ1'], expected_msg='Principal: princ1')
realm.run([kadminl, 'ank', '-randkey', '-x', 'dn=cn=t2,cn=krb5', 'again'],
          expected_code=1, expected_msg='ldap object is already kerberized')
# Check that we can't set linkdn on a non-standalone object.
realm.run([kadminl, 'modprinc', '-x', 'linkdn=cn=t1,cn=krb5', 'princ1'],
          expected_code=1, expected_msg='link information can not be set')

# Create a principal with a specified linkdn.
mark('LDAP specified linkdn')
realm.run([kadminl, 'ank', '-randkey', '-x', 'linkdn=cn=krb5', 'princ2'],
          expected_code=1, expected_msg='DN is out of the realm subtree')
realm.run([kadminl, 'ank', '-randkey', '-x', 'linkdn=cn=t1,cn=krb5', 'princ2'])
# Check that we can't reset linkdn.
realm.run([kadminl, 'modprinc', '-x', 'linkdn=cn=t2,cn=krb5', 'princ2'],
          expected_code=1, expected_msg='kerberos principal is already linked')

# Create a principal with a specified containerdn.
mark('LDAP specified containerdn')
realm.run([kadminl, 'ank', '-randkey', '-x', 'containerdn=cn=krb5', 'princ3'],
          expected_code=1, expected_msg='DN is out of the realm subtree')
realm.run([kadminl, 'ank', '-randkey', '-x', 'containerdn=cn=t1,cn=krb5',
           'princ3'])
realm.run([kadminl, 'modprinc', '-x', 'containerdn=cn=t2,cn=krb5', 'princ3'],
          expected_code=1, expected_msg='containerdn option not supported')
# Verify that containerdn is checked when linkdn is also supplied
# (CVE-2018-5730).
realm.run([kadminl, 'ank', '-randkey', '-x', 'containerdn=cn=krb5',
           '-x', 'linkdn=cn=t2,cn=krb5', 'princ4'], expected_code=1,
          expected_msg='DN is out of the realm subtree')

mark('LDAP ticket policy')

# Create and modify a ticket policy.
kldaputil(['create_policy', '-maxtktlife', '3hour', '-maxrenewlife', '6hour',
           '-allow_forwardable', 'tktpol'])
kldaputil(['modify_policy', '-maxtktlife', '4hour', '-maxrenewlife', '8hour',
           '+requires_preauth', 'tktpol'])
out = kldaputil(['view_policy', 'tktpol'])
if ('Ticket policy: tktpol\n' not in out or
    'Maximum ticket life: 0 days 04:00:00\n' not in out or
    'Maximum renewable life: 0 days 08:00:00\n' not in out or
    'Ticket flags: DISALLOW_FORWARDABLE REQUIRES_PRE_AUTH' not in out):
    fail('Unexpected kdb5_ldap_util view_policy output')

out = kldaputil(['list_policy'])
if out != 'tktpol\n':
    fail('Unexpected kdb5_ldap_util list_policy output')

# Associate the ticket policy to a principal.
realm.run([kadminl, 'ank', '-randkey', '-x', 'tktpolicy=tktpol', 'princ4'])
out = realm.run([kadminl, 'getprinc', 'princ4'])
if ('Maximum ticket life: 0 days 04:00:00\n' not in out or
    'Maximum renewable life: 0 days 08:00:00\n' not in out or
    'Attributes: DISALLOW_FORWARDABLE REQUIRES_PRE_AUTH\n' not in out):
    fail('Unexpected getprinc output with ticket policy')

# Destroying the policy should fail while a principal references it.
kldaputil(['destroy_policy', '-force', 'tktpol'], expected_code=1)

# Dissociate the ticket policy from the principal.
realm.run([kadminl, 'modprinc', '-x', 'tktpolicy=', 'princ4'])
out = realm.run([kadminl, 'getprinc', 'princ4'])
if ('Maximum ticket life: 0 days 05:00:00\n' not in out or
    'Maximum renewable life: 0 days 10:00:00\n' not in out or
    'Attributes:\n' not in out):
    fail('Unexpected getprinc output without ticket policy')

# Destroy the ticket policy.
kldaputil(['destroy_policy', '-force', 'tktpol'])
kldaputil(['view_policy', 'tktpol'], expected_code=1)
out = kldaputil(['list_policy'])
if out:
    fail('Unexpected kdb5_ldap_util list_policy output after destroy')

# Create another ticket policy to be destroyed with the realm.
kldaputil(['create_policy', 'tktpol2'])

# Try to create a password policy conflicting with a ticket policy.
realm.run([kadminl, 'addpol', 'tktpol2'], expected_code=1,
          expected_msg='Already exists while creating policy "tktpol2"')

# Try to create a ticket policy conflicting with a password policy.
realm.run([kadminl, 'addpol', 'pwpol'])
out = kldaputil(['create_policy', 'pwpol'], expected_code=1)
if 'Already exists while creating policy object' not in out:
    fail('Expected error not seen in kdb5_ldap_util output')

# Try to use a password policy as a ticket policy.
realm.run([kadminl, 'modprinc', '-x', 'tktpolicy=pwpol', 'princ4'],
          expected_code=1, expected_msg='Object class violation')

# Use a ticket policy as a password policy (CVE-2014-5353).  This
# works with a warning; use kadmin.local -q so the warning is shown.
realm.run([kadminl, '-q', 'modprinc -policy tktpol2 princ4'],
          expected_msg='WARNING: policy "tktpol2" does not exist')

# Do some basic tests with a KDC against the LDAP module, exercising the
# db_args processing code.
mark('LDAP KDC operation')
realm.start_kdc(['-x', 'nconns=3', '-x', 'host=' + ldap_uri,
                 '-x', 'binddn=' + admin_dn, '-x', 'bindpwd=' + admin_pw])
realm.addprinc(realm.user_princ, password('user'))
realm.addprinc(realm.host_princ)
realm.extract_keytab(realm.host_princ, realm.keytab)
realm.kinit(realm.user_princ, password('user'))
realm.run([kvno, realm.host_princ])
realm.klist(realm.user_princ, realm.host_princ)

mark('LDAP auth indicator')

# Test require_auth normalization.
realm.addprinc('authind', password('authind'))
realm.run([kadminl, 'setstr', 'authind', 'require_auth', 'otp radius'])

# Check that krbPrincipalAuthInd attributes are set when the string
# attribute it set.
out = ldap_search('(krbPrincipalName=authind*)')
if 'krbPrincipalAuthInd: otp' not in out:
    fail('Expected krbPrincipalAuthInd value not in output')
if 'krbPrincipalAuthInd: radius' not in out:
    fail('Expected krbPrincipalAuthInd value not in output')

# Check that the string attribute still appears when the principal is
# loaded.
realm.run([kadminl, 'getstrs', 'authind'],
          expected_msg='require_auth: otp radius')

# Modify the LDAP attributes and check that the change is reflected in
# the string attribute.
ldap_modify('dn: krbPrincipalName=authind@KRBTEST.COM,cn=t1,cn=krb5\n'
            'changetype: modify\n'
            'replace: krbPrincipalAuthInd\n'
            'krbPrincipalAuthInd: radius\n'
            'krbPrincipalAuthInd: pkinit\n')
realm.run([kadminl, 'getstrs', 'authind'],
           expected_msg='require_auth: radius pkinit')

# Regression test for #8877: remove the string attribute and check
# that it is reflected in the LDAP attributes and by getstrs.
realm.run([kadminl, 'delstr', 'authind', 'require_auth'])
out = ldap_search('(krbPrincipalName=authind*)')
if 'krbPrincipalAuthInd' in out:
    fail('krbPrincipalAuthInd attribute still present after delstr')
out = realm.run([kadminl, 'getstrs', 'authind'])
if 'require_auth' in out:
    fail('require_auth string attribute still visible after delstr')

mark('LDAP service principal aliases')

# Test service principal aliases.
realm.addprinc('canon', password('canon'))
ldap_modify('dn: krbPrincipalName=canon@KRBTEST.COM,cn=t1,cn=krb5\n'
            'changetype: modify\n'
            'add: krbPrincipalName\n'
            'krbPrincipalName: alias@KRBTEST.COM\n'
            'krbPrincipalName: ent@abc@KRBTEST.COM\n'
            '-\n'
            'add: krbCanonicalName\n'
            'krbCanonicalName: canon@KRBTEST.COM\n')
realm.run([kadminl, 'getprinc', 'alias'],
          expected_msg='Principal: canon@KRBTEST.COM\n')
realm.run([kadminl, 'getprinc', 'ent\\@abc'],
          expected_msg='Principal: canon@KRBTEST.COM\n')
realm.run([kadminl, 'getprinc', 'canon'],
          expected_msg='Principal: canon@KRBTEST.COM\n')
realm.run([kvno, 'alias', 'canon'])
out = realm.run([klist])
if 'alias@KRBTEST.COM\n' not in out or 'canon@KRBTEST.COM' not in out:
    fail('After fetching alias and canon, klist is missing one or both')
realm.kinit(realm.user_princ, password('user'), ['-S', 'alias'])
realm.klist(realm.user_princ, 'alias@KRBTEST.COM')

# Make sure an alias to the local TGS is still treated like an alias.
ldap_modify('dn: krbPrincipalName=krbtgt/KRBTEST.COM@KRBTEST.COM,'
            'cn=KRBTEST.COM,cn=krb5\n'
            'changetype: modify\n'
            'add:krbPrincipalName\n'
            'krbPrincipalName: tgtalias@KRBTEST.COM\n'
            '-\n'
            'add: krbCanonicalName\n'
            'krbCanonicalName: krbtgt/KRBTEST.COM@KRBTEST.COM\n')
realm.run([kadminl, 'getprinc', 'tgtalias'],
          expected_msg='Principal: krbtgt/KRBTEST.COM@KRBTEST.COM')
realm.kinit(realm.user_princ, password('user'))
realm.run([kvno, 'tgtalias'])
realm.klist(realm.user_princ, 'tgtalias@KRBTEST.COM')

# Make sure aliases work in header tickets.
realm.run([kadminl, 'modprinc', '-maxrenewlife', '3 hours', 'user'])
realm.run([kadminl, 'modprinc', '-maxrenewlife', '3 hours',
           'krbtgt/KRBTEST.COM'])
realm.kinit(realm.user_princ, password('user'), ['-l', '1h', '-r', '2h'])
realm.run([kvno, 'alias'])
realm.kinit(realm.user_princ, flags=['-R', '-S', 'alias'])
realm.klist(realm.user_princ, 'alias@KRBTEST.COM')

# Test client principal aliases, with and without preauth.
realm.kinit('canon', password('canon'))
realm.kinit('alias', password('canon'))
realm.run([kvno, 'alias'])
realm.klist('alias@KRBTEST.COM', 'alias@KRBTEST.COM')
realm.kinit('alias', password('canon'), ['-C'])
realm.run([kvno, 'alias'])
realm.klist('canon@KRBTEST.COM', 'alias@KRBTEST.COM')
realm.run([kadminl, 'modprinc', '+requires_preauth', 'canon'])
realm.kinit('canon', password('canon'))
realm.kinit('alias', password('canon'), ['-C'])

# Test enterprise alias with and without canonicalization.
realm.kinit('ent@abc', password('canon'), ['-E', '-C'])
realm.run([kvno, 'alias'])
realm.klist('canon@KRBTEST.COM', 'alias@KRBTEST.COM')

realm.kinit('ent@abc', password('canon'), ['-E'])
realm.run([kvno, 'alias'])
realm.klist('ent\\@abc@KRBTEST.COM', 'alias@KRBTEST.COM')

# Test client name canonicalization in non-krbtgt AS reply
realm.kinit('alias', password('canon'), ['-C', '-S', 'kadmin/changepw'])

mark('LDAP password history')

# Test password history.
def test_pwhist(nhist):
    def cpw(n, **kwargs):
        realm.run([kadminl, 'cpw', '-pw', str(n), princ], **kwargs)
    def cpw_fail(n):
        cpw(n, expected_code=1)
    output('*** Testing password history of size %d\n' % nhist)
    princ = 'pwhistprinc' + str(nhist)
    pol = 'pwhistpol' + str(nhist)
    realm.run([kadminl, 'addpol', '-history', str(nhist), pol])
    realm.run([kadminl, 'addprinc', '-policy', pol, '-nokey', princ])
    for i in range(nhist):
        # Set a password, then check that all previous passwords fail.
        cpw(i)
        for j in range(i + 1):
            cpw_fail(j)
    # Set one more new password, and make sure the oldest key is
    # rotated out.
    cpw(nhist)
    cpw_fail(1)
    cpw(0)

for n in (1, 2, 3, 4, 5):
    test_pwhist(n)

# Regression test for #8193: test password character class requirements.
princ = 'charclassprinc'
pol = 'charclasspol'
realm.run([kadminl, 'addpol', '-minclasses', '3', pol])
realm.run([kadminl, 'addprinc', '-policy', pol, '-nokey', princ])
realm.run([kadminl, 'cpw', '-pw', 'abcdef', princ], expected_code=1)
realm.run([kadminl, 'cpw', '-pw', 'Abcdef', princ], expected_code=1)
realm.run([kadminl, 'cpw', '-pw', 'Abcdef1', princ])

# Test principal renaming and make sure last modified is changed
def get_princ(princ):
    out = realm.run([kadminl, 'getprinc', princ])
    return dict(map(str.strip, x.split(":", 1)) for x in out.splitlines())

mark('LDAP principal renaming')
realm.addprinc("rename", password('rename'))
renameprinc = get_princ("rename")
realm.run([kadminl, '-p', 'fake@KRBTEST.COM', 'renprinc', 'rename', 'renamed'])
renamedprinc = get_princ("renamed")
if renameprinc['Last modified'] == renamedprinc['Last modified']:
    fail('Last modified data not updated when principal was renamed')

# Regression test for #7980 (fencepost when dividing keys up by kvno).
mark('#7980 regression test')
realm.run([kadminl, 'addprinc', '-randkey', '-e', 'aes256-cts,aes128-cts',
           'kvnoprinc'])
realm.run([kadminl, 'cpw', '-randkey', '-keepold', '-e',
           'aes256-cts,aes128-cts', 'kvnoprinc'])
realm.run([kadminl, 'getprinc', 'kvnoprinc'], expected_msg='Number of keys: 4')
realm.run([kadminl, 'cpw', '-randkey', '-keepold', '-e',
           'aes256-cts,aes128-cts', 'kvnoprinc'])
realm.run([kadminl, 'getprinc', 'kvnoprinc'], expected_msg='Number of keys: 6')

# Regression test for #8041 (NULL dereference on keyless principals).
mark('#8041 regression test')
realm.run([kadminl, 'addprinc', '-nokey', 'keylessprinc'])
realm.run([kadminl, 'getprinc', 'keylessprinc'],
          expected_msg='Number of keys: 0')
realm.run([kadminl, 'cpw', '-randkey', '-e', 'aes256-cts,aes128-cts',
           'keylessprinc'])
realm.run([kadminl, 'cpw', '-randkey', '-keepold', '-e',
           'aes256-cts,aes128-cts', 'keylessprinc'])
realm.run([kadminl, 'getprinc', 'keylessprinc'],
          expected_msg='Number of keys: 4')
realm.run([kadminl, 'purgekeys', '-all', 'keylessprinc'])
realm.run([kadminl, 'getprinc', 'keylessprinc'],
          expected_msg='Number of keys: 0')

# Test for 8354 (old password history entries when -keepold is used)
mark('#8354 regression test')
realm.run([kadminl, 'addpol', '-history', '2', 'keepoldpasspol'])
realm.run([kadminl, 'addprinc', '-policy', 'keepoldpasspol', '-pw', 'aaaa',
           'keepoldpassprinc'])
for p in ('bbbb', 'cccc', 'aaaa'):
    realm.run([kadminl, 'cpw', '-keepold', '-pw', p, 'keepoldpassprinc'])

if runenv.sizeof_time_t <= 4:
    skipped('y2038 LDAP test', 'platform has 32-bit time_t')
else:
    # Test storage of timestamps after y2038.
    realm.run([kadminl, 'modprinc', '-pwexpire', '2040-02-03', 'user'])
    realm.run([kadminl, 'getprinc', 'user'], expected_msg=' 2040\n')

# Regression test for #8861 (pw_expiration policy enforcement).
mark('pw_expiration propogation')
# Create a policy with a max life and verify its application.
realm.run([kadminl, 'addpol', '-maxlife', '1s', 'pw_e'])
realm.run([kadminl, 'addprinc', '-policy', 'pw_e', '-pw', 'password',
           'pwuser'])
out = realm.run([kadminl, 'getprinc', 'pwuser'],
                expected_msg='Password expiration date: ')
if 'Password expiration date: [never]' in out:
    fail('pw_expiration not applied at principal creation')
# Unset the policy max life and verify its application during password
# change.
realm.run([kadminl, 'modpol', '-maxlife', '0', 'pw_e'])
realm.run([kadminl, 'cpw', '-pw', 'password_', 'pwuser'])
realm.run([kadminl, 'getprinc', 'pwuser'],
          expected_msg='Password expiration date: [never]')

realm.stop()

# Test dump and load.  Include a regression test for #8882
# (pw_expiration not set during load operation).
mark('LDAP dump and load')
realm.run([kadminl, 'modprinc', '-pwexpire', 'now', 'pwuser'])
dumpfile = os.path.join(realm.testdir, 'dump')
realm.run([kdb5_util, 'dump', dumpfile])
realm.run([kdb5_util, 'load', dumpfile], expected_code=1,
          expected_msg='KDB module requires -update argument')
realm.run([kadminl, 'delprinc', 'pwuser'])
realm.run([kdb5_util, 'load', '-update', dumpfile])
out = realm.run([kadminl, 'getprinc', 'pwuser'])
if 'Password expiration date: [never]' in out:
    fail('pw_expiration not preserved across dump and load')

# Destroy the realm.
kldaputil(['destroy', '-f'])
out = kldaputil(['list'])
if out:
    fail('Unexpected kdb5_ldap_util list output after destroy')

if not core_schema:
    skip_rest('LDAP SASL tests', 'core schema not found')

if runenv.have_sasl != 'yes':
    skip_rest('LDAP SASL tests', 'SASL support not built')

# Test SASL EXTERNAL auth.  Remove the DNs and service password file
# from the DB module config.
mark('LDAP SASL EXTERNAL auth')
os.remove(ldap_pwfile)
dbmod = conf['dbmodules']['ldap']
dbmod['ldap_kdc_sasl_mech'] = dbmod['ldap_kadmind_sasl_mech'] = 'EXTERNAL'
del dbmod['ldap_service_password_file']
del dbmod['ldap_kdc_dn'], dbmod['ldap_kadmind_dn']
realm = K5Realm(create_kdb=False, kdc_conf=conf)
realm.run([kdb5_ldap_util, 'create', '-s', '-P', 'master'])
realm.start_kdc()
realm.addprinc(realm.user_princ, password('user'))
realm.kinit(realm.user_princ, password('user'))
realm.stop()
realm.run([kdb5_ldap_util, 'destroy', '-f'])

# Test SASL DIGEST-MD5 auth.  We need to set a clear-text password for
# the admin DN, so create a person entry (requires the core schema).
# Restore the service password file in the config and set authcids.
mark('LDAP SASL DIGEST-MD5 auth')
ldap_add('cn=admin,cn=krb5', 'person',
         ['sn: dummy', 'userPassword: admin'])
dbmod['ldap_kdc_sasl_mech'] = dbmod['ldap_kadmind_sasl_mech'] = 'DIGEST-MD5'
dbmod['ldap_kdc_sasl_authcid'] = 'digestuser'
dbmod['ldap_kadmind_sasl_authcid'] = 'digestuser'
dbmod['ldap_service_password_file'] = ldap_pwfile
realm = K5Realm(create_kdb=False, kdc_conf=conf)
input = admin_pw + '\n' + admin_pw + '\n'
realm.run([kdb5_ldap_util, 'stashsrvpw', 'digestuser'], input=input)
realm.run([kdb5_ldap_util, 'create', '-s', '-P', 'master'])
realm.start_kdc()
realm.addprinc(realm.user_princ, password('user'))
realm.kinit(realm.user_princ, password('user'))
realm.stop()
# Exercise DB options, which should cause binding to fail.
realm.run([kadminl, '-x', 'sasl_authcid=ab', 'getprinc', 'user'],
          expected_code=1, expected_msg='Cannot bind to LDAP server')
realm.run([kadminl, '-x', 'bindpwd=wrong', 'getprinc', 'user'],
          expected_code=1, expected_msg='Cannot bind to LDAP server')
realm.run([kdb5_ldap_util, 'destroy', '-f'])

# We could still use tests to exercise:
# * DB arg handling in krb5_ldap_create
# * krbAllowedToDelegateTo attribute processing
# * A load operation overwriting a standalone principal entry which
#   already exists but doesn't have a krbPrincipalName attribute
#   matching the principal name.
# * A bunch of invalid-input error conditions
#
# There is no coverage for the following because it would be difficult:
# * Out-of-memory error conditions
# * Handling of failures from slapd (including krb5_retry_get_ldap_handle)
# * Handling of servers which don't support mod-increment
# * krb5_ldap_delete_krbcontainer (only happens if krb5_ldap_create fails)

success('LDAP and DB2 KDB tests')
