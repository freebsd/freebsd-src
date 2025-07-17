#!/usr/bin/env python
# -
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Alexander V. Chernikov
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#


from functools import partial
import socket
import select
import argparse
import time


def parse_args():
    parser = argparse.ArgumentParser(description='divert socket tester')
    parser.add_argument('--sip', type=str, default='', help='IP to listen on')
    parser.add_argument('--family', type=str, help='inet/inet6')
    parser.add_argument('--ports', type=str, help='packet ports 1,2,3')
    parser.add_argument('--match_str', type=str, help='match string to use')
    parser.add_argument('--count', type=int, default=1,
                        help='Number of messages to receive')
    parser.add_argument('--test_name', type=str, required=True,
                        help='test name to run')
    return parser.parse_args()


def test_listen_tcp(args):
    if args.family == 'inet6':
        fam = socket.AF_INET6
    else:
        fam = socket.AF_INET
    sockets = []
    ports = [int(port) for port in args.ports.split(',')]
    for port in ports:
        s = socket.socket(fam, socket.SOCK_STREAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.setblocking(0)
        s.bind((args.sip, port))
        print('binding on {}:{}'.format(args.sip, port))
        s.listen(5)
        sockets.append(s)
    inputs = sockets
    count = 0
    while count < args.count:
        readable, writable, exceptional = select.select(inputs, [], inputs)
        for s in readable:
            (c, address) = s.accept()
            print('C: {}'.format(address))
            data = c.recv(9000)
            if args.match_str and args.match_str.encode('utf-8') != data:
                raise Exception('Expected "{}" but got "{}"'.format(
                    args.match_str, data.decode('utf-8')))
            count += 1
            c.close()


def test_listen_udp(args):
    if args.family == 'inet6':
        fam = socket.AF_INET6
    else:
        fam = socket.AF_INET
    sockets = []
    ports = [int(port) for port in args.ports.split(',')]
    for port in ports:
        s = socket.socket(fam, socket.SOCK_DGRAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.setblocking(0)
        s.bind((args.sip, port))
        print('binding on {}:{}'.format(args.sip, port))
        sockets.append(s)
    inputs = sockets
    count = 0
    while count < args.count:
        readable, writable, exceptional = select.select(inputs, [], inputs)
        for s in readable:
            (data, address) = s.recvfrom(9000)
            print('C: {}'.format(address))
            if args.match_str and args.match_str.encode('utf-8') != data:
                raise Exception('Expected "{}" but got "{}"'.format(
                    args.match_str, data.decode('utf-8')))
            count += 1


def main():
    args = parse_args()
    test_ptr = globals()[args.test_name]
    test_ptr(args)


if __name__ == '__main__':
    main()
