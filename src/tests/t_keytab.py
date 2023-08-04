from k5test import *

for realm in multipass_realms(create_user=False):
    # Test kinit with a keytab.
    realm.kinit(realm.host_princ, flags=['-k'])

realm = K5Realm(get_creds=False, start_kadmind=True)

# Test kinit with a partial keytab.
mark('partial keytab')
pkeytab = realm.keytab + '.partial'
realm.run([ktutil], input=('rkt %s\ndelent 1\nwkt %s\n' %
                           (realm.keytab, pkeytab)))
realm.kinit(realm.host_princ, flags=['-k', '-t', pkeytab])

# Test kinit with no keys for client in keytab.
mark('no keys for client')
realm.kinit(realm.user_princ, flags=['-k'], expected_code=1,
            expected_msg='no suitable keys')

# Test kinit and klist with client keytab defaults.
mark('client keytab')
realm.extract_keytab(realm.user_princ, realm.client_keytab);
realm.run([kinit, '-k', '-i'])
realm.klist(realm.user_princ)
realm.run([kdestroy])
realm.kinit(realm.user_princ, flags=['-k', '-i'])
realm.klist(realm.user_princ)
out = realm.run([klist, '-k', '-i'])
if realm.client_keytab not in out or realm.user_princ not in out:
    fail('Expected output not seen from klist -k -i')

# Test implicit request for keytab (-i or -t without -k)
mark('implicit -k')
realm.run([kdestroy])
realm.kinit(realm.host_princ, flags=['-t', realm.keytab],
            expected_msg='keytab specified, forcing -k')
realm.klist(realm.host_princ)
realm.run([kdestroy])
realm.kinit(realm.user_princ, flags=['-i'],
            expected_msg='keytab specified, forcing -k')
realm.klist(realm.user_princ)

# Test default principal for -k.  This operation requires
# canonicalization against the keytab in krb5_get_init_creds_keytab()
# as the krb5_sname_to_principal() result won't have a realm.  Try
# with and without without fallback processing since the code paths
# are different.
mark('default principal for -k')
realm.run([kinit, '-k'])
realm.klist(realm.host_princ)
no_canon_conf = {'libdefaults': {'dns_canonicalize_hostname': 'false'}}
no_canon = realm.special_env('no_canon', False, krb5_conf=no_canon_conf)
realm.run([kinit, '-k'], env=no_canon)
realm.klist(realm.host_princ)

# Test extracting keys with multiple key versions present.
mark('multi-kvno extract')
os.remove(realm.keytab)
realm.run([kadminl, 'cpw', '-randkey', '-keepold', realm.host_princ])
out = realm.run([kadminl, 'ktadd', '-norandkey', realm.host_princ])
if 'with kvno 1,' not in out or 'with kvno 2,' not in out:
    fail('Expected output not seen from kadmin.local ktadd -norandkey')
out = realm.run([klist, '-k', '-e'])
if ' 1 host/' not in out or ' 2 host/' not in out:
    fail('Expected output not seen from klist -k -e')

# Test again using kadmin over the network.
mark('multi-kvno extract (via kadmin)')
realm.prep_kadmin()
os.remove(realm.keytab)
out = realm.run_kadmin(['ktadd', '-norandkey', realm.host_princ])
if 'with kvno 1,' not in out or 'with kvno 2,' not in out:
    fail('Expected output not seen from kadmin.local ktadd -norandkey')
out = realm.run([klist, '-k', '-e'])
if ' 1 host/' not in out or ' 2 host/' not in out:
    fail('Expected output not seen from klist -k -e')

# Test handling of kvno values beyond 255.  Use kadmin over the
# network since we used to have an 8-bit limit on kvno marshalling.

# Test one key rotation, verifying that the expected new kvno appears
# in the keytab and in the principal entry.
def test_key_rotate(realm, princ, expected_kvno):
    realm.run_kadmin(['ktadd', '-k', realm.keytab, princ])
    realm.run([kadminl, 'ktrem', princ, 'old'])
    realm.kinit(princ, flags=['-k'])
    msg = '%d %s' % (expected_kvno, princ)
    out = realm.run([klist, '-k'], expected_msg=msg)
    msg = 'Key: vno %d,' % expected_kvno
    out = realm.run_kadmin(['getprinc', princ], expected_msg=msg)

mark('key rotation across boundaries')
princ = 'foo/bar@%s' % realm.realm
realm.addprinc(princ)
os.remove(realm.keytab)
realm.run([kadminl, 'modprinc', '-kvno', '253', princ])
test_key_rotate(realm, princ, 254)
test_key_rotate(realm, princ, 255)
test_key_rotate(realm, princ, 256)
test_key_rotate(realm, princ, 257)
realm.run([kadminl, 'modprinc', '-kvno', '32766', princ])
test_key_rotate(realm, princ, 32767)
test_key_rotate(realm, princ, 32768)
test_key_rotate(realm, princ, 32769)
realm.run([kadminl, 'modprinc', '-kvno', '65534', princ])
test_key_rotate(realm, princ, 65535)
test_key_rotate(realm, princ, 1)
test_key_rotate(realm, princ, 2)

mark('32-bit kvno')

# Test that klist -k can read a keytab entry without a 32-bit kvno and
# reports the 8-bit key version.
record = b'\x00\x01'             # principal component count
record += b'\x00\x0bKRBTEST.COM' # realm
record += b'\x00\x04user'        # principal component
record += b'\x00\x00\x00\x01'    # name type (NT-PRINCIPAL)
record += b'\x54\xf7\x4d\x35'    # timestamp
record += b'\x02'                # key version
record += b'\x00\x12'            # enctype
record += b'\x00\x20'            # key length
record += b'\x00' * 32           # key bytes
f = open(realm.keytab, 'wb')
f.write(b'\x05\x02\x00\x00\x00' + bytes([len(record)]))
f.write(record)
f.close()
msg = '   2 %s' % realm.user_princ
out = realm.run([klist, '-k'], expected_msg=msg)

# Make sure zero-fill isn't treated as a 32-bit kvno.
f = open(realm.keytab, 'wb')
f.write(b'\x05\x02\x00\x00\x00' + bytes([len(record) + 4]))
f.write(record)
f.write(b'\x00\x00\x00\x00')
f.close()
msg = '   2 %s' % realm.user_princ
out = realm.run([klist, '-k'], expected_msg=msg)

# Make sure a hand-crafted 32-bit kvno is recognized.
f = open(realm.keytab, 'wb')
f.write(b'\x05\x02\x00\x00\x00' + bytes([len(record) + 4]))
f.write(record)
f.write(b'\x00\x00\x00\x03')
f.close()
msg = '   3 %s' % realm.user_princ
out = realm.run([klist, '-k'], expected_msg=msg)

# Test parameter expansion in profile variables
mark('parameter expansion')
realm.stop()
conf = {'libdefaults': {
        'default_keytab_name': 'testdir/%{null}abc%{uid}',
        'default_client_keytab_name': 'testdir/%{null}xyz%{uid}'}}
realm = K5Realm(krb5_conf=conf, create_kdb=False)
del realm.env['KRB5_KTNAME']
del realm.env['KRB5_CLIENT_KTNAME']
uidstr = str(os.getuid())
msg = 'FILE:testdir/abc%s' % uidstr
out = realm.run([klist, '-k'], expected_code=1, expected_msg=msg)
msg = 'FILE:testdir/xyz%s' % uidstr
out = realm.run([klist, '-ki'], expected_code=1, expected_msg=msg)

conf = {'libdefaults': {'allow_weak_crypto': 'true'}}
realm = K5Realm(create_user=False, create_host=False, krb5_conf=conf)

realm.run([kadminl, 'ank', '-pw', 'pw', 'default'])
realm.run([kadminl, 'ank', '-e', 'aes256-cts:special', '-pw', 'pw', 'exp'])
realm.run([kadminl, 'ank', '-e', 'aes256-cts:special', '-pw', 'pw', '+preauth',
           'pexp'])

# Extract one of the explicit salt values from the database.
out = realm.run([kdb5_util, 'tabdump', 'keyinfo'])
salt_dict = {f[0]: f[5] for f in [l.split('\t') for l in out.splitlines()]}
exp_salt = bytes.fromhex(salt_dict['exp@KRBTEST.COM']).decode('ascii')

# Create a keytab using ktutil addent with the specified options and
# password "pw".  Test that we can use it to get initial tickets.
# Remove the keytab afterwards.
def test_addent(realm, princ, opts):
    realm.run([ktutil], input=('addent -password -p %s -k 1 %s\npw\nwkt %s\n' %
                               (princ, opts, realm.keytab)))
    realm.kinit(princ, flags=['-k'])
    os.remove(realm.keytab)

mark('ktutil addent')

# Test with default salt.
test_addent(realm, 'default', '-e aes128-cts')
test_addent(realm, 'default', '-e aes256-cts')

# Test with a salt specified to ktutil addent.
test_addent(realm, 'exp', '-e aes256-cts -s %s' % exp_salt)

# Test etype-info fetching.
test_addent(realm, 'default', '-f')
test_addent(realm, 'default', '-f -e aes128-cts')
test_addent(realm, 'exp', '-f')
test_addent(realm, 'pexp', '-f')

# Regression test for #8914: INT32_MIN length can cause backwards seek
mark('invalid record length')
f = open(realm.keytab, 'wb')
f.write(b'\x05\x02\x80\x00\x00\x00')
f.close()
msg = 'Bad format in keytab while scanning keytab'
realm.run([klist, '-k'], expected_code=1, expected_msg=msg)

success('Keytab-related tests')
