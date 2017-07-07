#!/usr/bin/python
from k5test import *

for realm in multipass_realms(create_user=False):
    # Test kinit with a keytab.
    realm.kinit(realm.host_princ, flags=['-k'])

realm = K5Realm(get_creds=False, start_kadmind=True)

# Test kinit with a partial keytab.
pkeytab = realm.keytab + '.partial'
realm.run([ktutil], input=('rkt %s\ndelent 1\nwkt %s\n' %
                           (realm.keytab, pkeytab)))
realm.kinit(realm.host_princ, flags=['-k', '-t', pkeytab])

# Test kinit with no keys for client in keytab.
output = realm.kinit(realm.user_princ, flags=['-k'], expected_code=1)
if 'no suitable keys' not in output:
    fail('Expected error not seen in kinit output')

# Test kinit and klist with client keytab defaults.
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
realm.run([kdestroy])
output = realm.kinit(realm.host_princ, flags=['-t', realm.keytab])
if 'keytab specified, forcing -k' not in output:
    fail('Expected output not seen from kinit -t keytab')
realm.klist(realm.host_princ)
realm.run([kdestroy])
output = realm.kinit(realm.user_princ, flags=['-i'])
if 'keytab specified, forcing -k' not in output:
    fail('Expected output not seen from kinit -i')
realm.klist(realm.user_princ)

# Test extracting keys with multiple key versions present.
os.remove(realm.keytab)
realm.run([kadminl, 'cpw', '-randkey', '-keepold', realm.host_princ])
out = realm.run([kadminl, 'ktadd', '-norandkey', realm.host_princ])
if 'with kvno 1,' not in out or 'with kvno 2,' not in out:
    fail('Expected output not seen from kadmin.local ktadd -norandkey')
out = realm.run([klist, '-k', '-e'])
if ' 1 host/' not in out or ' 2 host/' not in out:
    fail('Expected output not seen from klist -k -e')

# Test again using kadmin over the network.
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
    out = realm.run([klist, '-k'])
    if ('%d %s' % (expected_kvno, princ)) not in out:
        fail('kvno %d not listed in keytab' % expected_kvno)
    out = realm.run_kadmin(['getprinc', princ])
    if ('Key: vno %d,' % expected_kvno) not in out:
        fail('vno %d not seen in getprinc output' % expected_kvno)

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

# Test that klist -k can read a keytab entry without a 32-bit kvno and
# reports the 8-bit key version.
record = '\x00\x01'             # principal component count
record += '\x00\x0bKRBTEST.COM' # realm
record += '\x00\x04user'        # principal component
record += '\x00\x00\x00\x01'    # name type (NT-PRINCIPAL)
record += '\x54\xf7\x4d\x35'    # timestamp
record += '\x02'                # key version
record += '\x00\x12'            # enctype
record += '\x00\x20'            # key length
record += '\x00' * 32           # key bytes
f = open(realm.keytab, 'w')
f.write('\x05\x02\x00\x00\x00' + chr(len(record)))
f.write(record)
f.close()
out = realm.run([klist, '-k'])
if ('   2 %s' % realm.user_princ) not in out:
    fail('Expected entry not seen in klist -k output')

# Make sure zero-fill isn't treated as a 32-bit kvno.
f = open(realm.keytab, 'w')
f.write('\x05\x02\x00\x00\x00' + chr(len(record) + 4))
f.write(record)
f.write('\x00\x00\x00\x00')
f.close()
out = realm.run([klist, '-k'])
if ('   2 %s' % realm.user_princ) not in out:
    fail('Expected entry not seen in klist -k output')

# Make sure a hand-crafted 32-bit kvno is recognized.
f = open(realm.keytab, 'w')
f.write('\x05\x02\x00\x00\x00' + chr(len(record) + 4))
f.write(record)
f.write('\x00\x00\x00\x03')
f.close()
out = realm.run([klist, '-k'])
if ('   3 %s' % realm.user_princ) not in out:
    fail('Expected entry not seen in klist -k output')

# Test parameter expansion in profile variables
realm.stop()
conf = {'libdefaults': {
        'default_keytab_name': 'testdir/%{null}abc%{uid}',
        'default_client_keytab_name': 'testdir/%{null}xyz%{uid}'}}
realm = K5Realm(krb5_conf=conf, create_kdb=False)
del realm.env['KRB5_KTNAME']
del realm.env['KRB5_CLIENT_KTNAME']
uidstr = str(os.getuid())
out = realm.run([klist, '-k'], expected_code=1)
if 'FILE:testdir/abc%s' % uidstr not in out:
    fail('Wrong keytab in klist -k output')
out = realm.run([klist, '-ki'], expected_code=1)
if 'FILE:testdir/xyz%s' % uidstr not in out:
    fail('Wrong keytab in klist -ki output')

success('Keytab-related tests')
