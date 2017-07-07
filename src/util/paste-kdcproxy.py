#!/usr/bin/python
import kdcproxy
from paste import httpserver
import os
import sys

if len(sys.argv) > 1:
    port = sys.argv[1]
else:
    port = 8443
if len(sys.argv) > 2:
    pem = sys.argv[2]
else:
    pem = '*'
server = httpserver.serve(kdcproxy.Application(), port=port, ssl_pem=pem,
                          start_loop=False)
os.write(sys.stdout.fileno(), 'proxy server ready\n')
server.serve_forever()
