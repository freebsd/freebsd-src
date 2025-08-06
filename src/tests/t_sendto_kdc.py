from k5test import *

realm = K5Realm(create_host=False)

mark('Fallback to primary KDC')

# Create a replica database and start a KDC.
conf_rep = {'dbmodules': {'db': {'database_name': '$testdir/db.replica2'}},
            'realms': {'$realm': {'kdc_listen': '$port9',
                                  'kdc_tcp_listen': '$port9'}}}
replica = realm.special_env('replica', True, kdc_conf=conf_rep)
dumpfile = os.path.join(realm.testdir, 'dump')
realm.run([kdb5_util, 'dump', dumpfile])
realm.run([kdb5_util, 'load', dumpfile], env=replica)
replica_kdc = realm.start_server([krb5kdc, '-n'], 'starting...', env=replica)

# Change the password on the primary.
realm.run([kadminl, 'cpw', '-pw', 'new', realm.user_princ])

conf_fallback = {'realms': {'$realm': {'kdc': '$hostname:$port9',
                                       'primary_kdc': '$hostname:$port0'}}}
fallback = realm.special_env('fallback', False, krb5_conf=conf_fallback)
msgs = ('Retrying AS request with primary KDC',)
realm.kinit(realm.user_princ, 'new', env=fallback, expected_trace=msgs)

stop_daemon(replica_kdc)

mark('UNIX domain socket')

conf_unix = {'realms': {'$realm': {'kdc_listen': '$testdir/krb5.sock',
                                   'kdc_tcp_listen': ''}}}
unix = realm.special_env('unix', True, kdc_conf=conf_unix)
realm.run([kdb5_util, 'load', dumpfile], env=unix)
realm.stop_kdc()
realm.start_kdc(env=unix)

conf_unix_cli = {'realms': {'$realm': {'kdc': '$testdir/krb5.sock'}}}
unix_cli = realm.special_env('unix_cli', False, krb5_conf=conf_unix_cli)

# Do a kinit and check if we send the packet via a UNIX domain socket.
msgs = ('Sending TCP request to UNIX domain socket',)
realm.kinit(realm.user_princ, password('user'), env=unix_cli,
            expected_trace=msgs)

success('sendto_kdc')
