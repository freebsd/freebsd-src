#!/usr/bin/python
from k5test import *

# If uuserver is not compiled under -DDEBUG, then set to 0
debug_compiled=1

for realm in multipass_realms():
    if debug_compiled == 0:
        realm.start_in_inetd(['./uuserver', 'uuserver'], port=9999)
    else:
        srv_output = realm.start_server(['./uuserver', '9999'], 'Server started')

    output = realm.run(['./uuclient', hostname, 'testing message', '9999'])
    if 'uu-client: server says \"Hello, other end of connection.\"' not in output:
        fail('Message not echoed back.')


success('User-2-user test programs')
