from k5test import *

# Test that the kdcpreauth client_keyblock() callback matches the key
# indicated by the etype info, and returns NULL if no key was selected.
testpreauth = os.path.join(buildtop, 'plugins', 'preauth', 'test', 'test.so')
conf = {'plugins': {'kdcpreauth': {'module': 'test:' + testpreauth},
                    'clpreauth': {'module': 'test:' + testpreauth}}}
realm = K5Realm(create_host=False, get_creds=False, krb5_conf=conf)
realm.run([kadminl, 'modprinc', '+requires_preauth', realm.user_princ])
realm.run([kadminl, 'setstr', realm.user_princ, 'teststring', 'testval'])
realm.run([kadminl, 'addprinc', '-nokey', '+requires_preauth', 'nokeyuser'])
realm.kinit(realm.user_princ, password('user'), expected_msg='testval')
realm.kinit('nokeyuser', password('user'), expected_code=1,
            expected_msg='no key')

# Preauth type -123 is the test preauth module type; 133 is FAST
# PA-FX-COOKIE; 2 is encrypted timestamp.

# Test normal preauth flow.
mark('normal')
msgs = ('Sending unauthenticated request',
        '/Additional pre-authentication required',
        'Preauthenticating using KDC method data',
        'Processing preauth types:',
        'Preauth module test (-123) (real) returned: 0/Success',
        'Produced preauth for next request: PA-FX-COOKIE (133), -123',
        'Decrypted AS reply')
realm.run(['./icred', realm.user_princ, password('user')],
          expected_msg='testval', expected_trace=msgs)

# Test successful optimistic preauth.
mark('optimistic')
expected_trace = ('Attempting optimistic preauth',
                  'Processing preauth types: -123',
                  'Preauth module test (-123) (real) returned: 0/Success',
                  'Produced preauth for next request: -123',
                  'Decrypted AS reply')
realm.run(['./icred', '-o', '-123', realm.user_princ, password('user')],
          expected_trace=expected_trace)

# Test optimistic preauth failing on client, falling back to encrypted
# timestamp.
mark('optimistic (client failure)')
msgs = ('Attempting optimistic preauth',
        'Processing preauth types: -123',
        '/induced optimistic fail',
        'Sending unauthenticated request',
        '/Additional pre-authentication required',
        'Preauthenticating using KDC method data',
        'Processing preauth types:',
        'Encrypted timestamp (for ',
        'module encrypted_timestamp (2) (real) returned: 0/Success',
        'preauth for next request: PA-FX-COOKIE (133), PA-ENC-TIMESTAMP (2)',
        'Decrypted AS reply')
realm.run(['./icred', '-o', '-123', '-X', 'fail_optimistic', realm.user_princ,
           password('user')], expected_trace=msgs)

# Test optimistic preauth failing on KDC, falling back to encrypted
# timestamp.
mark('optimistic (KDC failure)')
realm.run([kadminl, 'setstr', realm.user_princ, 'failopt', 'yes'])
msgs = ('Attempting optimistic preauth',
        'Processing preauth types: -123',
        'Preauth module test (-123) (real) returned: 0/Success',
        'Produced preauth for next request: -123',
        '/Preauthentication failed',
        'Preauthenticating using KDC method data',
        'Processing preauth types:',
        'Encrypted timestamp (for ',
        'module encrypted_timestamp (2) (real) returned: 0/Success',
        'preauth for next request: PA-FX-COOKIE (133), PA-ENC-TIMESTAMP (2)',
        'Decrypted AS reply')
realm.run(['./icred', '-o', '-123', realm.user_princ, password('user')],
          expected_trace=msgs)
# Leave failopt set for the next test.

# Test optimistic preauth failing on KDC, stopping because the test
# module disabled fallback.
mark('optimistic (KDC failure, no fallback)')
msgs = ('Attempting optimistic preauth',
        'Processing preauth types: -123',
        'Preauth module test (-123) (real) returned: 0/Success',
        'Produced preauth for next request: -123',
        '/Preauthentication failed')
realm.run(['./icred', '-X', 'disable_fallback', '-o', '-123', realm.user_princ,
           password('user')], expected_code=1,
          expected_msg='Preauthentication failed', expected_trace=msgs)
realm.run([kadminl, 'delstr', realm.user_princ, 'failopt'])

# Test KDC_ERR_MORE_PREAUTH_DATA_REQUIRED and secure cookies.
mark('second round-trip')
realm.run([kadminl, 'setstr', realm.user_princ, '2rt', 'secondtrip'])
msgs = ('Sending unauthenticated request',
        '/Additional pre-authentication required',
        'Preauthenticating using KDC method data',
        'Processing preauth types:',
        'Preauth module test (-123) (real) returned: 0/Success',
        'Produced preauth for next request: PA-FX-COOKIE (133), -123',
        '/More preauthentication data is required',
        'Continuing preauth mech -123',
        'Processing preauth types: -123, PA-FX-COOKIE (133)',
        'Produced preauth for next request: PA-FX-COOKIE (133), -123',
        'Decrypted AS reply')
realm.run(['./icred', realm.user_princ, password('user')],
          expected_msg='2rt: secondtrip', expected_trace=msgs)

# Test client-side failure after KDC_ERR_MORE_PREAUTH_DATA_REQUIRED,
# falling back to encrypted timestamp.
mark('second round-trip (client failure)')
msgs = ('Sending unauthenticated request',
        '/Additional pre-authentication required',
        'Preauthenticating using KDC method data',
        'Processing preauth types:',
        'Preauth module test (-123) (real) returned: 0/Success',
        'Produced preauth for next request: PA-FX-COOKIE (133), -123',
        '/More preauthentication data is required',
        'Continuing preauth mech -123',
        'Processing preauth types: -123, PA-FX-COOKIE (133)',
        '/induced 2rt fail',
        'Preauthenticating using KDC method data',
        'Processing preauth types:',
        'Encrypted timestamp (for ',
        'module encrypted_timestamp (2) (real) returned: 0/Success',
        'preauth for next request: PA-FX-COOKIE (133), PA-ENC-TIMESTAMP (2)',
        'Decrypted AS reply')
realm.run(['./icred', '-X', 'fail_2rt', realm.user_princ, password('user')],
          expected_msg='2rt: secondtrip', expected_trace=msgs)

# Test client-side failure after KDC_ERR_MORE_PREAUTH_DATA_REQUIRED,
# stopping because the test module disabled fallback.
mark('second round-trip (client failure, no fallback)')
msgs = ('Sending unauthenticated request',
        '/Additional pre-authentication required',
        'Preauthenticating using KDC method data',
        'Processing preauth types:',
        'Preauth module test (-123) (real) returned: 0/Success',
        'Produced preauth for next request: PA-FX-COOKIE (133), -123',
        '/More preauthentication data is required',
        'Continuing preauth mech -123',
        'Processing preauth types: -123, PA-FX-COOKIE (133)',
        '/induced 2rt fail')
realm.run(['./icred', '-X', 'fail_2rt', '-X', 'disable_fallback',
           realm.user_princ, password('user')], expected_code=1,
          expected_msg='Pre-authentication failed: induced 2rt fail',
          expected_trace=msgs)

# Test KDC-side failure after KDC_ERR_MORE_PREAUTH_DATA_REQUIRED,
# falling back to encrypted timestamp.
mark('second round-trip (KDC failure)')
realm.run([kadminl, 'setstr', realm.user_princ, 'fail2rt', 'yes'])
msgs = ('Sending unauthenticated request',
        '/Additional pre-authentication required',
        'Preauthenticating using KDC method data',
        'Processing preauth types:',
        'Preauth module test (-123) (real) returned: 0/Success',
        'Produced preauth for next request: PA-FX-COOKIE (133), -123',
        '/More preauthentication data is required',
        'Continuing preauth mech -123',
        'Processing preauth types: -123, PA-FX-COOKIE (133)',
        'Preauth module test (-123) (real) returned: 0/Success',
        'Produced preauth for next request: PA-FX-COOKIE (133), -123',
        '/Preauthentication failed',
        'Preauthenticating using KDC method data',
        'Processing preauth types:',
        'Encrypted timestamp (for ',
        'module encrypted_timestamp (2) (real) returned: 0/Success',
        'preauth for next request: PA-FX-COOKIE (133), PA-ENC-TIMESTAMP (2)',
        'Decrypted AS reply')
realm.run(['./icred', realm.user_princ, password('user')],
          expected_msg='2rt: secondtrip', expected_trace=msgs)
# Leave fail2rt set for the next test.

# Test KDC-side failure after KDC_ERR_MORE_PREAUTH_DATA_REQUIRED,
# stopping because the test module disabled fallback.
mark('second round-trip (KDC failure, no fallback)')
msgs = ('Sending unauthenticated request',
        '/Additional pre-authentication required',
        'Preauthenticating using KDC method data',
        'Processing preauth types:',
        'Preauth module test (-123) (real) returned: 0/Success',
        'Produced preauth for next request: PA-FX-COOKIE (133), -123',
        '/More preauthentication data is required',
        'Continuing preauth mech -123',
        'Processing preauth types: -123, PA-FX-COOKIE (133)',
        'Preauth module test (-123) (real) returned: 0/Success',
        'Produced preauth for next request: PA-FX-COOKIE (133), -123',
        '/Preauthentication failed')
realm.run(['./icred', '-X', 'disable_fallback',
           realm.user_princ, password('user')], expected_code=1,
          expected_msg='Preauthentication failed', expected_trace=msgs)
realm.run([kadminl, 'delstr', realm.user_princ, 'fail2rt'])

# Test tryagain flow by inducing a KDC_ERR_ENCTYPE_NOSUPP error on the KDC.
mark('tryagain')
realm.run([kadminl, 'setstr', realm.user_princ, 'err', 'testagain'])
msgs = ('Sending unauthenticated request',
        '/Additional pre-authentication required',
        'Preauthenticating using KDC method data',
        'Processing preauth types:',
        'Preauth module test (-123) (real) returned: 0/Success',
        'Produced preauth for next request: PA-FX-COOKIE (133), -123',
        '/KDC has no support for encryption type',
        'Recovering from KDC error 14 using preauth mech -123',
        'Preauth tryagain input types (-123): -123, PA-FX-COOKIE (133)',
        'Preauth module test (-123) tryagain returned: 0/Success',
        'Followup preauth for next request: -123, PA-FX-COOKIE (133)',
        'Decrypted AS reply')
realm.run(['./icred', realm.user_princ, password('user')],
          expected_msg='tryagain: testagain', expected_trace=msgs)

# Test a client-side tryagain failure, falling back to encrypted
# timestamp.
mark('tryagain (client failure)')
msgs = ('Sending unauthenticated request',
        '/Additional pre-authentication required',
        'Preauthenticating using KDC method data',
        'Processing preauth types:',
        'Preauth module test (-123) (real) returned: 0/Success',
        'Produced preauth for next request: PA-FX-COOKIE (133), -123',
        '/KDC has no support for encryption type',
        'Recovering from KDC error 14 using preauth mech -123',
        'Preauth tryagain input types (-123): -123, PA-FX-COOKIE (133)',
        '/induced tryagain fail',
        'Preauthenticating using KDC method data',
        'Processing preauth types:',
        'Encrypted timestamp (for ',
        'module encrypted_timestamp (2) (real) returned: 0/Success',
        'preauth for next request: PA-FX-COOKIE (133), PA-ENC-TIMESTAMP (2)',
        'Decrypted AS reply')
realm.run(['./icred', '-X', 'fail_tryagain', realm.user_princ,
           password('user')], expected_trace=msgs)

# Test a client-side tryagain failure, stopping because the test
# module disabled fallback.
mark('tryagain (client failure, no fallback)')
msgs = ('Sending unauthenticated request',
        '/Additional pre-authentication required',
        'Preauthenticating using KDC method data',
        'Processing preauth types:',
        'Preauth module test (-123) (real) returned: 0/Success',
        'Produced preauth for next request: PA-FX-COOKIE (133), -123',
        '/KDC has no support for encryption type',
        'Recovering from KDC error 14 using preauth mech -123',
        'Preauth tryagain input types (-123): -123, PA-FX-COOKIE (133)',
        '/induced tryagain fail')
realm.run(['./icred', '-X', 'fail_tryagain', '-X', 'disable_fallback',
           realm.user_princ, password('user')], expected_code=1,
          expected_msg='KDC has no support for encryption type',
          expected_trace=msgs)

# Test that multiple stepwise initial creds operations can be
# performed with the same krb5_context, with proper tracking of
# clpreauth module request handles.
mark('interleaved')
realm.run([kadminl, 'addprinc', '-pw', 'pw', 'u1'])
realm.run([kadminl, 'addprinc', '+requires_preauth', '-pw', 'pw', 'u2'])
realm.run([kadminl, 'addprinc', '+requires_preauth', '-pw', 'pw', 'u3'])
realm.run([kadminl, 'setstr', 'u2', '2rt', 'extra'])
out = realm.run(['./icinterleave', 'pw', 'u1', 'u2', 'u3'])
if out != ('step 1\nstep 2\nstep 3\nstep 1\nfinish 1\nstep 2\nno attr\n'
           'step 3\nno attr\nstep 2\n2rt: extra\nstep 3\nfinish 3\nstep 2\n'
           'finish 2\n'):
    fail('unexpected output from icinterleave')

success('Pre-authentication framework tests')
