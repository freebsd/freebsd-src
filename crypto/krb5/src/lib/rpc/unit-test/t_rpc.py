import re

from k5test import *

realm = K5Realm()
realm.env['GSSRPC_ALLOW_AUTH_GSSAPI'] = 'yes'

server = realm.start_server(['./server', '-t'], 'running')
line = server.stdout.readline()
portstr = re.match(r'^port: (\d+)$', line).group(1)

realm.run(['./client', '-t', hostname, portstr, 'host@' + hostname, '1026'],
          expected_msg='...........')

for i in range(4):
    line = server.stdout.readline()
    if 'rpc_test server: bad verifier from user@KRBTEST.COM at ' not in line:
        fail('unexpected server message: ' + line)
    output(line)

mark('RPCSEC_GSS service levels')
realm.run(['./client', '-t', '-g', hostname, portstr, 'host@' + hostname])

realm.addprinc('nokey/' + hostname)

realm.run(['./client', '-t', hostname, portstr, 'nokey@' + hostname, '1026'],
          expected_code=2)

line = server.stdout.readline()
if 'rpc_test server: Authentication attempt failed: ' not in line:
    fail('unexpected server message: ' + line)

success('gssrpc auth_gssapi tests')
