from k5test import *

realm = K5Realm(start_kadmind=True)

# Create a principal.  Test -q option and keyboard entry of the admin
# password and principal password.  Verify creation with kadmin.local.
realm.run([kadmin, '-q', 'addprinc princ/pw'],
          input=password('admin') + '\npw1\npw1\n')
realm.run([kadminl, 'getprinc', 'princ/pw'],
          expected_msg='Principal: princ/pw@KRBTEST.COM')

# Run the remaining tests with a cache for efficiency.
realm.prep_kadmin()

realm.run_kadmin(['addpol', 'standardpol'])
realm.run_kadmin(['listpols'], expected_msg='standardpol')
realm.run_kadmin(['modpol', '-minlength', '5', 'standardpol'])
realm.run_kadmin(['getpol', 'standardpol'],
                 expected_msg='Minimum password length: 5')

realm.run_kadmin(['addprinc', '-randkey', 'princ/random'])
realm.run([kadminl, 'getprinc', 'princ/random'],
          expected_msg='Principal: princ/random@KRBTEST.COM')

realm.run_kadmin(['cpw', 'princ/pw'], input='newpw\nnewpw\n')
realm.run_kadmin(['cpw', '-randkey', 'princ/random'])

realm.run_kadmin(['modprinc', '-allow_tix', 'princ/random'])
realm.run_kadmin(['modprinc', '+allow_tix', 'princ/random'])
realm.run_kadmin(['modprinc', '-policy', 'standardpol', 'princ/random'])

realm.run_kadmin(['listprincs'], expected_msg='princ/random@KRBTEST.COM')

realm.run_kadmin(['ktadd', 'princ/pw'])

realm.run_kadmin(['delprinc', 'princ/random'])
realm.run([kadminl, 'getprinc', 'princ/random'], expected_code=1,
          expected_msg='Principal does not exist')
realm.run_kadmin(['delprinc', 'princ/pw'])
realm.run([kadminl, 'getprinc', 'princ/pw'], expected_code=1,
          expected_msg='Principal does not exist')

realm.run_kadmin(['delpol', 'standardpol'])
realm.run([kadminl, 'getpol', 'standardpol'], expected_code=1,
          expected_msg='Policy does not exist')

# Regression test for #2877 (fixed-sized GSSRPC buffers can't
# accomodate large listprinc results).
mark('large listprincs result')
for i in range(200):
    realm.run_kadmin(['addprinc', '-randkey', 'foo%d' % i])
realm.run_kadmin(['listprincs'], expected_msg='foo199')

# Test kadmin -k with the default principal, with and without
# fallback.  This operation requires canonicalization against the
# keytab in krb5_get_init_creds_keytab() as the
# krb5_sname_to_principal() result won't have a realm.  Try with and
# without without fallback processing since the code paths are
# different.
mark('kadmin -k')
realm.run([kadmin, '-k', 'getprinc', realm.host_princ])
no_canon_conf = {'libdefaults': {'dns_canonicalize_hostname': 'false'}}
no_canon = realm.special_env('no_canon', False, krb5_conf=no_canon_conf)
realm.run([kadmin, '-k', 'getprinc', realm.host_princ], env=no_canon)

success('kadmin and kpasswd tests')
