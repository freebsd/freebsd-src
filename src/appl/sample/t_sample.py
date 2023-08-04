from k5test import *

sclient = os.path.join(buildtop, 'appl', 'sample', 'sclient', 'sclient')
sserver = os.path.join(buildtop, 'appl', 'sample', 'sserver', 'sserver')

for realm in multipass_realms(create_host=False):
    server_princ = 'sample/%s@%s' % (hostname, realm.realm)
    realm.addprinc(server_princ)
    realm.extract_keytab(server_princ, realm.keytab)

    portstr = str(realm.server_port())
    server = realm.start_server([sserver, '-p', portstr], 'starting...')
    out = realm.run([sclient, hostname, portstr],
                    expected_msg='You are user@KRBTEST.COM')
    await_daemon_exit(server)

    server = realm.start_in_inetd([sserver])
    out = realm.run([sclient, hostname, portstr],
                    expected_msg='You are user@KRBTEST.COM')
    await_daemon_exit(server)

success('sim_client/sim_server tests')
