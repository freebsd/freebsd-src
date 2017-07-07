#!/usr/bin/python
from k5test import *
import time
from itertools import imap

# Run kdbtest against the BDB module.
realm = K5Realm(create_kdb=False)
realm.run(['./kdbtest'])

# Set up an OpenLDAP test server if we can.

if (not os.path.exists(os.path.join(plugins, 'kdb', 'kldap.so')) and
    not os.path.exists(os.path.join(buildtop, 'lib', 'libkdb_ldap.a'))):
    skip_rest('LDAP KDB tests', 'LDAP KDB module not built')

if 'SLAPD' not in os.environ and not which('slapd'):
    skip_rest('LDAP KDB tests', 'slapd not found')

ldapdir = os.path.abspath('ldap')
dbdir = os.path.join(ldapdir, 'ldap')
slapd_conf = os.path.join(ldapdir, 'slapd.conf')
slapd_out = os.path.join(ldapdir, 'slapd.out')
slapd_pidfile = os.path.join(ldapdir, 'pid')
ldap_pwfile = os.path.join(ldapdir, 'pw')
ldap_sock = os.path.join(ldapdir, 'sock')
ldap_uri = 'ldapi://%s/' % ldap_sock.replace(os.path.sep, '%2F')
schema = os.path.join(srctop, 'plugins', 'kdb', 'ldap', 'libkdb_ldap',
                      'kerberos.schema')
top_dn = 'cn=krb5'
admin_dn = 'cn=admin,cn=krb5'
admin_pw = 'admin'

shutil.rmtree(ldapdir, True)
os.mkdir(ldapdir)
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

# Find the core schema file if we can.
ldap_homes = ['/etc/ldap', '/etc/openldap', '/usr/local/etc/openldap',
              '/usr/local/etc/ldap']
local_schema_path = '/schema/core.schema'
core_schema = next((i for i in imap(lambda x:x+local_schema_path, ldap_homes)
                    if os.path.isfile(i)), None)

# Make a slapd config file.  This is deprecated in OpenLDAP 2.3 and
# later, but it's easier than using LDIF and slapadd.  Include some
# authz-regexp entries for SASL authentication tests.  Load the core
# schema if we found it, for use in the DIGEST-MD5 test.
file = open(slapd_conf, 'w')
file.write('pidfile %s\n' % slapd_pidfile)
file.write('include %s\n' % schema)
if core_schema:
    file.write('include %s\n' % core_schema)
file.write('moduleload back_bdb\n')
file.write('database bdb\n')
file.write('suffix %s\n' % top_dn)
file.write('rootdn %s\n' % admin_dn)
file.write('rootpw %s\n' % admin_pw)
file.write('directory %s\n' % dbdir)
file.write('authz-regexp .*uidNumber=%d,cn=peercred,cn=external,cn=auth %s\n' %
           (os.geteuid(), admin_dn))
file.write('authz-regexp uid=digestuser,cn=digest-md5,cn=auth %s\n' % admin_dn)
file.close()

slapd_pid = -1
def kill_slapd():
    global slapd_pid
    if slapd_pid != -1:
        os.kill(slapd_pid, signal.SIGTERM)
        slapd_pid = -1
atexit.register(kill_slapd)

out = open(slapd_out, 'w')
subprocess.call([slapd, '-h', ldap_uri, '-f', slapd_conf], stdout=out,
                stderr=out)
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
                            stderr=subprocess.STDOUT)
    (out, dummy) = proc.communicate()
    return out

def ldap_modify(ldif, args=[]):
    proc = subprocess.Popen([ldapmodify, '-H', ldap_uri, '-D', admin_dn,
                             '-x', '-w', admin_pw] + args,
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
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
out = realm.run([kadminl, 'ank', '-randkey', '-x', 'dn=cn=krb5', 'princ1'],
                expected_code=1)
if 'DN is out of the realm subtree' not in out:
    fail('Unexpected kadmin.local output for out-of-realm dn')
realm.run([kadminl, 'ank', '-randkey', '-x', 'dn=cn=t2,cn=krb5', 'princ1'])
out = realm.run([kadminl, 'getprinc', 'princ1'])
if 'Principal: princ1' not in out:
    fail('Unexpected kadmin.local output after creating princ1')
out = realm.run([kadminl, 'ank', '-randkey', '-x', 'dn=cn=t2,cn=krb5',
                 'again'], expected_code=1)
if 'ldap object is already kerberized' not in out:
    fail('Unexpected kadmin.local output trying to re-kerberize DN')
# Check that we can't set linkdn on a non-standalone object.
out = realm.run([kadminl, 'modprinc', '-x', 'linkdn=cn=t1,cn=krb5', 'princ1'],
                expected_code=1)
if 'link information can not be set' not in out:
    fail('Unexpected kadmin.local output trying to set linkdn on princ1')

# Create a principal with a specified linkdn.
out = realm.run([kadminl, 'ank', '-randkey', '-x', 'linkdn=cn=krb5', 'princ2'],
                expected_code=1)
if 'DN is out of the realm subtree' not in out:
    fail('Unexpected kadmin.local output for out-of-realm linkdn')
realm.run([kadminl, 'ank', '-randkey', '-x', 'linkdn=cn=t1,cn=krb5', 'princ2'])
# Check that we can't reset linkdn.
out = realm.run([kadminl, 'modprinc', '-x', 'linkdn=cn=t2,cn=krb5', 'princ2'],
                expected_code=1)
if 'kerberos principal is already linked' not in out:
    fail('Unexpected kadmin.local output for re-specified linkdn')

# Create a principal with a specified containerdn.
out = realm.run([kadminl, 'ank', '-randkey', '-x', 'containerdn=cn=krb5',
                 'princ3'], expected_code=1)
if 'DN is out of the realm subtree' not in out:
    fail('Unexpected kadmin.local output for out-of-realm containerdn')
realm.run([kadminl, 'ank', '-randkey', '-x', 'containerdn=cn=t1,cn=krb5',
           'princ3'])
out = realm.run([kadminl, 'modprinc', '-x', 'containerdn=cn=t2,cn=krb5',
                 'princ3'], expected_code=1)
if 'containerdn option not supported' not in out:
    fail('Unexpected kadmin.local output trying to reset containerdn')

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
out = realm.run([kadminl, 'addpol', 'tktpol2'], expected_code=1)
if 'Already exists while creating policy "tktpol2"' not in out:
    fail('Expected error not seen in kadmin.local output')

# Try to create a ticket policy conflicting with a password policy.
realm.run([kadminl, 'addpol', 'pwpol'])
out = kldaputil(['create_policy', 'pwpol'], expected_code=1)
if 'Already exists while creating policy object' not in out:
    fail('Expected error not seen in kdb5_ldap_util output')

# Try to use a password policy as a ticket policy.
out = realm.run([kadminl, 'modprinc', '-x', 'tktpolicy=pwpol', 'princ4'],
                expected_code=1)
if 'Object class violation' not in out:
    fail('Expected error not seem in kadmin.local output')

# Use a ticket policy as a password policy (CVE-2014-5353).  This
# works with a warning; use kadmin.local -q so the warning is shown.
out = realm.run([kadminl, '-q', 'modprinc -policy tktpol2 princ4'])
if 'WARNING: policy "tktpol2" does not exist' not in out:
    fail('Expected error not seen in kadmin.local output')

# Do some basic tests with a KDC against the LDAP module, exercising the
# db_args processing code.
realm.start_kdc(['-x', 'nconns=3', '-x', 'host=' + ldap_uri,
                 '-x', 'binddn=' + admin_dn, '-x', 'bindpwd=' + admin_pw])
realm.addprinc(realm.user_princ, password('user'))
realm.addprinc(realm.host_princ)
realm.extract_keytab(realm.host_princ, realm.keytab)
realm.kinit(realm.user_princ, password('user'))
realm.run([kvno, realm.host_princ])
realm.klist(realm.user_princ, realm.host_princ)

# Test auth indicator support
realm.addprinc('authind', password('authind'))
realm.run([kadminl, 'setstr', 'authind', 'require_auth', 'otp radius'])

out = ldap_search('(krbPrincipalName=authind*)')
if 'krbPrincipalAuthInd: otp' not in out:
    fail('Expected krbPrincipalAuthInd value not in output')
if 'krbPrincipalAuthInd: radius' not in out:
    fail('Expected krbPrincipalAuthInd value not in output')

out = realm.run([kadminl, 'getstrs', 'authind'])
if 'require_auth: otp radius' not in out:
    fail('Expected auth indicators value not in output')

# Test service principal aliases.
realm.addprinc('canon', password('canon'))
ldap_modify('dn: krbPrincipalName=canon@KRBTEST.COM,cn=t1,cn=krb5\n'
            'changetype: modify\n'
            'add: krbPrincipalName\n'
            'krbPrincipalName: alias@KRBTEST.COM\n'
            '-\n'
            'add: krbCanonicalName\n'
            'krbCanonicalName: canon@KRBTEST.COM\n')
out = realm.run([kadminl, 'getprinc', 'alias'])
if 'Principal: canon@KRBTEST.COM\n' not in out:
    fail('Could not fetch canon through alias')
out = realm.run([kadminl, 'getprinc', 'canon'])
if 'Principal: canon@KRBTEST.COM\n' not in out:
    fail('Could not fetch canon through canon')
realm.run([kvno, 'alias'])
realm.run([kvno, 'canon'])
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
out = realm.run([kadminl, 'getprinc', 'tgtalias'])
if 'Principal: krbtgt/KRBTEST.COM@KRBTEST.COM' not in out:
    fail('Could not fetch krbtgt through tgtalias')
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
out = realm.kinit('alias', password('canon'), expected_code=1)
if 'not found in Kerberos database' not in out:
    fail('Wrong error message for kinit to alias without -C flag')
realm.kinit('alias', password('canon'), ['-C'])
realm.run([kvno, 'alias'])
realm.klist('canon@KRBTEST.COM', 'alias@KRBTEST.COM')
realm.run([kadminl, 'modprinc', '+requires_preauth', 'canon'])
realm.kinit('canon', password('canon'))
realm.kinit('alias', password('canon'), ['-C'])

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

realm.addprinc("rename", password('rename'))
renameprinc = get_princ("rename")
realm.run([kadminl, '-p', 'fake@KRBTEST.COM', 'renprinc', 'rename', 'renamed'])
renamedprinc = get_princ("renamed")
if renameprinc['Last modified'] == renamedprinc['Last modified']:
    fail('Last modified data not updated when principal was renamed')

# Regression test for #7980 (fencepost when dividing keys up by kvno).
realm.run([kadminl, 'addprinc', '-randkey', '-e', 'aes256-cts,aes128-cts',
           'kvnoprinc'])
realm.run([kadminl, 'cpw', '-randkey', '-keepold', '-e',
           'aes256-cts,aes128-cts', 'kvnoprinc'])
out = realm.run([kadminl, 'getprinc', 'kvnoprinc'])
if 'Number of keys: 4' not in out:
    fail('After cpw -keepold, wrong number of keys')
realm.run([kadminl, 'cpw', '-randkey', '-keepold', '-e',
           'aes256-cts,aes128-cts', 'kvnoprinc'])
out = realm.run([kadminl, 'getprinc', 'kvnoprinc'])
if 'Number of keys: 6' not in out:
    fail('After cpw -keepold, wrong number of keys')

# Regression test for #8041 (NULL dereference on keyless principals).
realm.run([kadminl, 'addprinc', '-nokey', 'keylessprinc'])
out = realm.run([kadminl, 'getprinc', 'keylessprinc'])
if 'Number of keys: 0' not in out:
    fail('Failed to create a principal with no keys')
realm.run([kadminl, 'cpw', '-randkey', '-e', 'aes256-cts,aes128-cts',
           'keylessprinc'])
realm.run([kadminl, 'cpw', '-randkey', '-keepold', '-e',
           'aes256-cts,aes128-cts', 'keylessprinc'])
out = realm.run([kadminl, 'getprinc', 'keylessprinc'])
if 'Number of keys: 4' not in out:
    fail('Failed to add keys to keylessprinc')
realm.run([kadminl, 'purgekeys', '-all', 'keylessprinc'])
out = realm.run([kadminl, 'getprinc', 'keylessprinc'])
if 'Number of keys: 0' not in out:
    fail('After purgekeys -all, keys remain')

# Test for 8354 (old password history entries when -keepold is used)
realm.run([kadminl, 'addpol', '-history', '2', 'keepoldpasspol'])
realm.run([kadminl, 'addprinc', '-policy', 'keepoldpasspol', '-pw', 'aaaa',
           'keepoldpassprinc'])
for p in ('bbbb', 'cccc', 'aaaa'):
    realm.run([kadminl, 'cpw', '-keepold', '-pw', p, 'keepoldpassprinc'])

realm.stop()

# Briefly test dump and load.
dumpfile = os.path.join(realm.testdir, 'dump')
realm.run([kdb5_util, 'dump', dumpfile])
out = realm.run([kdb5_util, 'load', dumpfile], expected_code=1)
if 'KDB module requires -update argument' not in out:
    fail('Unexpected error from kdb5_util load without -update')
realm.run([kdb5_util, 'load', '-update', dumpfile])

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
out = realm.run([kadminl, '-x', 'sasl_authcid=ab', 'getprinc', 'user'],
                expected_code=1)
if 'Cannot bind to LDAP server' not in out:
    fail('Expected error not seen in kadmin.local output')
out = realm.run([kadminl, '-x', 'bindpwd=wrong', 'getprinc', 'user'],
                expected_code=1)
if 'Cannot bind to LDAP server' not in out:
    fail('Expected error not seen in kadmin.local output')
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
