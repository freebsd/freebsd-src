from k5test import *

sim_client = os.path.join(buildtop, 'appl', 'simple', 'client', 'sim_client')
sim_server = os.path.join(buildtop, 'appl', 'simple', 'server', 'sim_server')

for realm in multipass_realms(create_host=False):
    server_princ = 'sample/%s@%s' % (hostname, realm.realm)
    realm.addprinc(server_princ)
    realm.extract_keytab(server_princ, realm.keytab)

    portstr = str(realm.server_port())
    server = realm.start_server([sim_server, '-p', portstr], 'starting...')

    out = realm.run([sim_client, '-p', portstr, hostname])
    if ('Sent checksummed message:' not in out or
        'Sent encrypted message:' not in out):
        fail('Expected client messages not seen')

    # sim_server exits after one client execution, so we can read
    # until it closes stdout.
    seen1 = seen2 = seen3 = False
    for line in server.stdout:
        if line == 'Got authentication info from user@KRBTEST.COM\n':
            seen1 = True
        if line == "Safe message is: 'hi there!'\n":
            seen2 = True
        if line == "Decrypted message is: 'hi there!'\n":
            seen3 = True
    if not (seen1 and seen2 and seen3):
        fail('Expected server messages not seen')

    await_daemon_exit(server)

success('sim_client/sim_server tests')
