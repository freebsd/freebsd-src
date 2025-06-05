from k5test import *

# If uuserver is not compiled under -DDEBUG, then set to 0
debug_compiled=1

for realm in multipass_realms():
    if debug_compiled == 0:
        server = realm.start_in_inetd(['./uuserver'], port=9999)
    else:
        server = realm.start_server(['./uuserver', '9999'], 'Server started')

    msg = 'uu-client: server says "Hello, other end of connection."'
    realm.run(['./uuclient', hostname, 'testing message', '9999'],
              expected_msg=msg)

    await_daemon_exit(server)

success('User-2-user test programs')
