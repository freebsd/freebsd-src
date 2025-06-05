from k5test import *

conf = {'plugins': {'audit': {
            'module': 'test:$plugins/audit/test/k5audit_test.so'}}}

realm = K5Realm(krb5_conf=conf, get_creds=False)
realm.addprinc('target')
realm.run([kadminl, 'modprinc', '+ok_to_auth_as_delegate', realm.host_princ])

# Make normal AS and TGS requests so they will be audited.
realm.kinit(realm.host_princ, flags=['-k', '-f'])
realm.run([kvno, 'target'])

# Make S4U2Self and S4U2Proxy requests so they will be audited.  The
# S4U2Proxy request is expected to fail.
realm.run([kvno, '-k', realm.keytab, '-U', 'user', '-P', 'target'],
          expected_code=1, expected_msg='KDC can\'t fulfill requested option')

# Make a U2U request so it will be audited.
uuserver = os.path.join(buildtop, 'appl', 'user_user', 'uuserver')
uuclient = os.path.join(buildtop, 'appl', 'user_user', 'uuclient')
port_arg = str(realm.server_port())
realm.start_server([uuserver, port_arg], 'Server started')
realm.run([uuclient, hostname, 'testing message', port_arg],
          expected_msg='Hello')

success('Audit tests')
