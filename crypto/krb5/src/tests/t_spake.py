from k5test import *

# The name and number of each supported SPAKE group.
builtin_groups = ((1, 'edwards25519'),)
openssl_groups = ((2, 'P-256'), (3, 'P-384'), (4, 'P-521'))
if runenv.have_spake_openssl == 'yes':
    groups = builtin_groups + openssl_groups
else:
    groups = builtin_groups

for gnum, gname in groups:
    mark('group %s' % gname)
    conf = {'libdefaults': {'spake_preauth_groups': gname}}
    for realm in multipass_realms(create_user=False, create_host=False,
                                  krb5_conf=conf):
        realm.run([kadminl, 'addprinc', '+preauth', '-pw', 'pw', 'user'])

        # Test a basic SPAKE preauth scenario with no optimizations.
        msgs = ('Sending unauthenticated request',
                '/Additional pre-authentication required',
                'Selected etype info:',
                'Sending SPAKE support message',
                'for next request: PA-FX-COOKIE (133), PA-SPAKE (151)',
                '/More preauthentication data is required',
                'Continuing preauth mech PA-SPAKE (151)',
                'SPAKE challenge received with group ' + str(gnum),
                'Sending SPAKE response',
                'for next request: PA-FX-COOKIE (133), PA-SPAKE (151)',
                'AS key determined by preauth:',
                'Decrypted AS reply')
        realm.kinit('user', 'pw', expected_trace=msgs)

        # Test an unsuccessful authentication.
        msgs = ('/Additional pre-authentication required',
                'Selected etype info:',
                'Sending SPAKE support message',
                'for next request: PA-FX-COOKIE (133), PA-SPAKE (151)',
                '/More preauthentication data is required',
                'Continuing preauth mech PA-SPAKE (151)',
                'SPAKE challenge received with group ' + str(gnum),
                'Sending SPAKE response',
                '/Preauthentication failed')
        realm.kinit('user', 'wrongpw', expected_code=1, expected_trace=msgs)

conf = {'libdefaults': {'spake_preauth_groups': 'edwards25519'}}
kdcconf = {'realms': {'$realm': {'spake_preauth_indicator': 'indspake'}}}
realm = K5Realm(create_user=False, krb5_conf=conf, kdc_conf=kdcconf)
realm.run([kadminl, 'addprinc', '+preauth', '-pw', 'pw', 'user'])

# Test with FAST.
mark('FAST')
msgs = ('Using FAST due to armor ccache negotiation',
        'FAST armor key:',
        'Sending unauthenticated request',
        '/Additional pre-authentication required',
        'Decoding FAST response',
        'Selected etype info:',
        'Sending SPAKE support message',
        'for next request: PA-FX-COOKIE (133), PA-SPAKE (151)',
        '/More preauthentication data is required',
        'Continuing preauth mech PA-SPAKE (151)',
        'SPAKE challenge received with group 1',
        'Sending SPAKE response',
        'for next request: PA-FX-COOKIE (133), PA-SPAKE (151)',
        'AS key determined by preauth:',
        'FAST reply key:')
realm.kinit(realm.host_princ, flags=['-k'])
realm.kinit('user', 'pw', flags=['-T', realm.ccache], expected_trace=msgs)

# Test optimistic client preauth (151 is PA-SPAKE).
mark('client optimistic')
msgs = ('Attempting optimistic preauth',
        'Processing preauth types: PA-SPAKE (151)',
        'Sending SPAKE support message',
        'for next request: PA-SPAKE (151)',
        '/More preauthentication data is required',
        'Selected etype info:',
        'SPAKE challenge received with group 1',
        'Sending SPAKE response',
        'for next request: PA-FX-COOKIE (133), PA-SPAKE (151)',
        'AS key determined by preauth:',
        'Decrypted AS reply')
realm.run(['./icred', '-o', '151', 'user', 'pw'], expected_trace=msgs)

# Test KDC optimistic challenge (accepted by client).
mark('KDC optimistic')
oconf = {'kdcdefaults': {'spake_preauth_kdc_challenge': 'edwards25519'}}
oenv = realm.special_env('ochal', True, krb5_conf=oconf)
realm.stop_kdc()
realm.start_kdc(env=oenv)
msgs = ('Sending unauthenticated request',
        '/Additional pre-authentication required',
        'Selected etype info:',
        'SPAKE challenge received with group 1',
        'Sending SPAKE response',
        'for next request: PA-FX-COOKIE (133), PA-SPAKE (151)',
        'AS key determined by preauth:',
        'Decrypted AS reply')
realm.kinit('user', 'pw', expected_trace=msgs)

if runenv.have_spake_openssl != 'yes':
    skip_rest('SPAKE fallback tests', 'SPAKE not built using OpenSSL')

# Test optimistic client preauth falling back to encrypted timestamp
# because the KDC doesn't support any of the client groups.
mark('client optimistic (fallback)')
p256conf={'libdefaults': {'spake_preauth_groups': 'P-256'}}
p256env = realm.special_env('p256', False, krb5_conf=p256conf)
msgs = ('Attempting optimistic preauth',
        'Processing preauth types: PA-SPAKE (151)',
        'Sending SPAKE support message',
        'for next request: PA-SPAKE (151)',
        '/Preauthentication failed',
        'Selected etype info:',
        'Encrypted timestamp ',
        'for next request: PA-FX-COOKIE (133), PA-ENC-TIMESTAMP (2)',
        'AS key determined by preauth:',
        'Decrypted AS reply')
realm.run(['./icred', '-o', '151', 'user', 'pw'], env=p256env,
          expected_trace=msgs)

# Test KDC optimistic challenge (rejected by client).
mark('KDC optimistic (rejected)')
rconf = {'libdefaults': {'spake_preauth_groups': 'P-384,edwards25519'},
         'kdcdefaults': {'spake_preauth_kdc_challenge': 'P-384'}}
renv = realm.special_env('ochal', True, krb5_conf=rconf)
realm.stop_kdc()
realm.start_kdc(env=renv)
msgs = ('Sending unauthenticated request',
        '/Additional pre-authentication required',
        'Selected etype info:',
        'SPAKE challenge with group 3 rejected',
        'Sending SPAKE support message',
        'for next request: PA-FX-COOKIE (133), PA-SPAKE (151)',
        '/More preauthentication data is required',
        'Continuing preauth mech PA-SPAKE (151)',
        'SPAKE challenge received with group 1',
        'Sending SPAKE response',
        'for next request: PA-FX-COOKIE (133), PA-SPAKE (151)',
        'AS key determined by preauth:',
        'Decrypted AS reply')
realm.kinit('user', 'pw', expected_trace=msgs)

# Check that the auth indicator for SPAKE is properly included by the KDC.
mark('auth indicator')
realm.run([kvno, realm.host_princ])
realm.run(['./adata', realm.host_princ], expected_msg='+97: [indspake]')

success('SPAKE pre-authentication tests')
