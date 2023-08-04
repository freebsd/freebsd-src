import kdcproxy
import os
import ssl
import sys
from wsgiref.simple_server import make_server

if len(sys.argv) > 1:
    port = int(sys.argv[1])
else:
    port = 8443
if len(sys.argv) > 2:
    pem = sys.argv[2]
else:
    pem = '*'

server = make_server('localhost', port, kdcproxy.Application())
server.socket = ssl.wrap_socket(server.socket, certfile=pem, server_side=True)
os.write(sys.stdout.fileno(), b'proxy server ready\n')
server.serve_forever()
