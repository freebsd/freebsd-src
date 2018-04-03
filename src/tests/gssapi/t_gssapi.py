#!/usr/bin/python
from k5test import *

# Test krb5 negotiation under SPNEGO for all enctype configurations.  Also
# test IOV wrap/unwrap with and without SPNEGO.
for realm in multipass_realms():
    realm.run(['./t_spnego','p:' + realm.host_princ, realm.keytab])
    realm.run(['./t_iov', 'p:' + realm.host_princ])
    realm.run(['./t_iov', '-s', 'p:' + realm.host_princ])
    realm.run(['./t_pcontok', 'p:' + realm.host_princ])

### Test acceptor name behavior.

realm = K5Realm()

# Create some host-based principals and put most of them into the
# keytab.  Rename one principal so that the keytab name matches the
# key but not the client name.
realm.run([kadminl, 'addprinc', '-randkey', 'service1/abraham'])
realm.run([kadminl, 'addprinc', '-randkey', 'service1/barack'])
realm.run([kadminl, 'addprinc', '-randkey', 'service2/calvin'])
realm.run([kadminl, 'addprinc', '-randkey', 'service2/dwight'])
realm.run([kadminl, 'addprinc', '-randkey', 'host/-nomatch-'])
realm.run([kadminl, 'xst', 'service1/abraham'])
realm.run([kadminl, 'xst', 'service1/barack'])
realm.run([kadminl, 'xst', 'service2/calvin'])
realm.run([kadminl, 'renprinc', 'service1/abraham', 'service1/andrew'])

# Test with no acceptor name, including client/keytab principal
# mismatch (non-fatal) and missing keytab entry (fatal).
realm.run(['./t_accname', 'p:service1/andrew'],
          expected_msg='service1/abraham')
realm.run(['./t_accname', 'p:service1/barack'], expected_msg='service1/barack')
realm.run(['./t_accname', 'p:service2/calvin'], expected_msg='service2/calvin')
realm.run(['./t_accname', 'p:service2/dwight'], expected_code=1,
          expected_msg=' not found in keytab')

# Test with acceptor name containing service only, including
# client/keytab hostname mismatch (non-fatal) and service name
# mismatch (fatal).
realm.run(['./t_accname', 'p:service1/andrew', 'h:service1'],
          expected_msg='service1/abraham')
realm.run(['./t_accname', 'p:service1/andrew', 'h:service2'], expected_code=1,
          expected_msg=' not found in keytab')
realm.run(['./t_accname', 'p:service2/calvin', 'h:service2'],
          expected_msg='service2/calvin')
realm.run(['./t_accname', 'p:service2/calvin', 'h:service1'], expected_code=1,
          expected_msg=' found in keytab but does not match server principal')

# Test with acceptor name containing service and host.  Use the
# client's un-canonicalized hostname as acceptor input to mirror what
# many servers do.
realm.run(['./t_accname', 'p:' + realm.host_princ,
           'h:host@%s' % socket.gethostname()], expected_msg=realm.host_princ)
realm.run(['./t_accname', 'p:host/-nomatch-',
           'h:host@%s' % socket.gethostname()], expected_code=1,
          expected_msg=' not found in keytab')

# Test krb5_gss_import_cred.
realm.run(['./t_imp_cred', 'p:service1/barack'])
realm.run(['./t_imp_cred', 'p:service1/barack', 'service1/barack'])
realm.run(['./t_imp_cred', 'p:service1/andrew', 'service1/abraham'])
realm.run(['./t_imp_cred', 'p:service2/dwight'], expected_code=1,
          expected_msg=' not found in keytab')

# Test credential store extension.
tmpccname = 'FILE:' + os.path.join(realm.testdir, 'def_cache')
realm.env['KRB5CCNAME'] = tmpccname
storagecache = 'FILE:' + os.path.join(realm.testdir, 'user_store')
servicekeytab = os.path.join(realm.testdir, 'kt')
service_cs = 'service/cs@%s' % realm.realm
realm.addprinc(service_cs)
realm.extract_keytab(service_cs, servicekeytab)
realm.kinit(service_cs, None, ['-k', '-t', servicekeytab])
realm.run(['./t_credstore', '-s', 'p:' + service_cs, 'ccache', storagecache,
           'keytab', servicekeytab])

# Test rcache feature of cred stores.  t_credstore -r should produce a
# replay error normally, but not with rcache set to "none:".
output = realm.run(['./t_credstore', '-r', '-a', 'p:' + realm.host_princ],
                   expected_code=1)
if 'gss_accept_sec_context(2): Request is a replay' not in output:
    fail('Expected replay error not seen in t_credstore output')
realm.run(['./t_credstore', '-r', '-a', 'p:' + realm.host_princ,
           'rcache', 'none:'])

# Verify that we can't acquire acceptor creds without a keytab.
os.remove(realm.keytab)
output = realm.run(['./t_accname', 'p:abc'], expected_code=1)
if ('gss_acquire_cred: Keytab' not in output or
    'nonexistent or empty' not in output):
    fail('Expected error message not seen for nonexistent keytab')

realm.stop()

# Re-run the last acceptor name test with ignore_acceptor_hostname set
# and the principal for the mismatching hostname in the keytab.
ignore_conf = {'libdefaults': {'ignore_acceptor_hostname': 'true'}}
realm = K5Realm(krb5_conf=ignore_conf)
realm.run([kadminl, 'addprinc', '-randkey', 'host/-nomatch-'])
realm.run([kadminl, 'xst', 'host/-nomatch-'])
realm.run(['./t_accname', 'p:host/-nomatch-',
           'h:host@%s' % socket.gethostname()], expected_msg='host/-nomatch-')

realm.stop()

# Make sure a GSSAPI acceptor can handle cross-realm tickets with a
# transited field.  (Regression test for #7639.)
r1, r2, r3 = cross_realms(3, xtgts=((0,1), (1,2)),
                          create_user=False, create_host=False,
                          args=[{'realm': 'A.X', 'create_user': True},
                                {'realm': 'X'},
                                {'realm': 'B.X', 'create_host': True}])
os.rename(r3.keytab, r1.keytab)
r1.run(['./t_accname', 'p:' + r3.host_princ, 'h:host'])
r1.stop()
r2.stop()
r3.stop()

### Test gss_inquire_cred behavior.

realm = K5Realm()

# Test deferred resolution of the default ccache for initiator creds.
realm.run(['./t_inq_cred'], expected_msg=realm.user_princ)
realm.run(['./t_inq_cred', '-k'], expected_msg=realm.user_princ)
realm.run(['./t_inq_cred', '-s'], expected_msg=realm.user_princ)

# Test picking a name from the keytab for acceptor creds.
realm.run(['./t_inq_cred', '-a'], expected_msg=realm.host_princ)
realm.run(['./t_inq_cred', '-k', '-a'], expected_msg=realm.host_princ)
realm.run(['./t_inq_cred', '-s', '-a'], expected_msg=realm.host_princ)

# Test client keytab initiation (non-deferred) with a specified name.
realm.extract_keytab(realm.user_princ, realm.client_keytab)
os.remove(realm.ccache)
realm.run(['./t_inq_cred', '-k'], expected_msg=realm.user_princ)

# Test deferred client keytab initiation and GSS_C_BOTH cred usage.
os.remove(realm.client_keytab)
os.remove(realm.ccache)
shutil.copyfile(realm.keytab, realm.client_keytab)
realm.run(['./t_inq_cred', '-k', '-b'], expected_msg=realm.host_princ)

# Test gss_export_name behavior.
out = realm.run(['./t_export_name', 'u:x'])
if out != '0401000B06092A864886F7120102020000000D78404B5242544553542E434F4D\n':
    fail('Unexpected output from t_export_name (krb5 username)')
output = realm.run(['./t_export_name', '-s', 'u:xyz'])
if output != '0401000806062B06010505020000000378797A\n':
    fail('Unexpected output from t_export_name (SPNEGO username)')
output = realm.run(['./t_export_name', 'p:a@b'])
if output != '0401000B06092A864886F71201020200000003614062\n':
    fail('Unexpected output from t_export_name (krb5 principal)')
output = realm.run(['./t_export_name', '-s', 'p:a@b'])
if output != '0401000806062B060105050200000003614062\n':
    fail('Unexpected output from t_export_name (SPNEGO krb5 principal)')

# Test that composite-export tokens can be imported.
output = realm.run(['./t_export_name', '-c', 'p:a@b'])
if (output != '0402000B06092A864886F7120102020000000361406200000000\n'):
    fail('Unexpected output from t_export_name (using COMPOSITE_EXPORT)')

# Test gss_inquire_mechs_for_name behavior.
krb5_mech = '{ 1 2 840 113554 1 2 2 }'
spnego_mech = '{ 1 3 6 1 5 5 2 }'
out = realm.run(['./t_inq_mechs_name', 'p:a@b'])
if krb5_mech not in out:
    fail('t_inq_mechs_name (principal)')
out = realm.run(['./t_inq_mechs_name', 'u:x'])
if krb5_mech not in out or spnego_mech not in out:
    fail('t_inq_mecs_name (user)')
out = realm.run(['./t_inq_mechs_name', 'h:host'])
if krb5_mech not in out or spnego_mech not in out:
    fail('t_inq_mecs_name (hostbased)')

# Test that accept_sec_context can produce an error token and
# init_sec_context can interpret it.
realm.run(['./t_err', 'p:' + realm.host_princ])

# Test the GSS_KRB5_CRED_NO_CI_FLAGS_X cred option.
realm.run(['./t_ciflags', 'p:' + realm.host_princ])

# Test that inquire_context works properly, even on incomplete
# contexts.
realm.run(['./t_inq_ctx', 'user', password('user'), 'p:%s' % realm.host_princ])

if runenv.sizeof_time_t <= 4:
    skip_rest('y2038 GSSAPI tests', 'platform has 32-bit time_t')

# Test lifetime results, using a realm with a large maximum lifetime
# so that we can test ticket end dates after y2038.
realm.stop()
conf = {'realms': {'$realm': {'max_life': '9000d'}}}
realm = K5Realm(kdc_conf=conf, get_creds=False)

# Check a lifetime string result against an expected number value (or None).
# Allow some variance due to time elapsed during the tests.
def check_lifetime(msg, val, expected):
    if expected is None and val != 'indefinite':
        fail('%s: expected indefinite, got %s' % (msg, val))
    if expected is not None and val == 'indefinite':
        fail('%s: expected %d, got indefinite' % (msg, expected))
    if expected is not None and abs(int(val) - expected) > 100:
        fail('%s: expected %d, got %s' % (msg, expected, val))

realm.kinit(realm.user_princ, password('user'), flags=['-l', '8500d'])
out = realm.run(['./t_lifetime', 'p:' + realm.host_princ, str(8000 * 86400)])
ln = out.split('\n')
check_lifetime('icred gss_acquire_cred', ln[0], 8500 * 86400)
check_lifetime('icred gss_inquire_cred', ln[1], 8500 * 86400)
check_lifetime('acred gss_acquire_cred', ln[2], None)
check_lifetime('acred gss_inquire_cred', ln[3], None)
check_lifetime('ictx gss_init_sec_context', ln[4], 8000 * 86400)
check_lifetime('ictx gss_inquire_context', ln[5], 8000 * 86400)
check_lifetime('ictx gss_context_time', ln[6], 8000 * 86400)
check_lifetime('actx gss_accept_sec_context', ln[7], 8000 * 86400 + 300)
check_lifetime('actx gss_inquire_context', ln[8], 8000 * 86400 + 300)
check_lifetime('actx gss_context_time', ln[9], 8000 * 86400 + 300)

success('GSSAPI tests')
