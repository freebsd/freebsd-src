from k5test import *

if not which('systemd-socket-activate'):
    skip_rest('socket activation tests', 'systemd-socket-activate not found')

# Configure listeners for two UNIX domain sockets and two ports.
kdc_conf = {'realms': {'$realm': {
    'kdc_listen': '$testdir/sock1 $testdir/sock2',
    'kdc_tcp_listen': '$port7 $port8'}}}
realm = K5Realm(kdc_conf=kdc_conf, start_kdc=False)

# Create socket activation fds for just one of the UNIX domain sockets
# and one of the ports.
realm.start_server(['./t_sockact', os.path.join(realm.testdir, 'sock1'),
                    str(realm.portbase + 8), '--', krb5kdc, '-n'],
                   'starting...')

mark('UNIX socket 1')
cconf1 = {'realms': {'$realm': {'kdc': '$testdir/sock1'}}}
env1 = realm.special_env('sock1', False, krb5_conf=cconf1)
realm.kinit(realm.user_princ, password('user'), env=env1)

mark('port8')
cconf2 = {'realms': {'$realm': {'kdc': '$hostname:$port8'}}}
env2 = realm.special_env('sock1', False, krb5_conf=cconf2)
realm.kinit(realm.user_princ, password('user'), env=env2)

# Test that configured listener addresses are ignored if they don't
# match caller-provided sockets.

mark('UNIX socket 2')
cconf3 = {'realms': {'$realm': {'kdc': '$testdir/sock2'}}}
env3 = realm.special_env('sock2', False, krb5_conf=cconf3)
realm.kinit(realm.user_princ, password('user'), env=env3, expected_code=1,
            expected_msg='Cannot contact any KDC')

mark('port7')
cconf4 = {'realms': {'$realm': {'kdc': '$hostname:$port7'}}}
env4 = realm.special_env('sock1', False, krb5_conf=cconf4)
realm.kinit(realm.user_princ, password('user'), env=env3, expected_code=1,
            expected_msg='Cannot contact any KDC')

success('systemd socket activation tests')
