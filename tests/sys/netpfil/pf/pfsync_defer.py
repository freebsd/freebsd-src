#!/usr/bin/env python3
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Rubicon Communications, LLC (Netgate)
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

import argparse
import logging
logging.getLogger("scapy").setLevel(logging.CRITICAL)
import scapy.all as sp
import socket
import sys
import time
from sniffer import Sniffer

got_pfsync = False
got_ping = False
sent_ping = False

def check_pfsync(args, packet):
    global got_pfsync
    global got_ping

    ip = packet.getlayer(sp.IP)
    if not ip:
        return False

    if ip.proto != 240:
        return False

    # Only look at the first packet
    if got_pfsync:
        return False

    got_pfsync = time.time()

    return False

def check_reply(args, packet):
    global got_pfsync
    global got_ping

    if not packet.getlayer(sp.ICMP):
        return False

    # Only look at the first packet
    if got_ping:
        return False

    got_ping = time.time()

    return False

def ping(intf):
    global sent_ping

    ether = sp.Ether()
    ip = sp.IP(dst="203.0.113.2", src="198.51.100.2")
    icmp = sp.ICMP(type='echo-request')
    raw = sp.raw(bytes.fromhex('00010203'))

    req = ether / ip / icmp / raw
    sp.sendp(req, iface=intf, verbose=False)
    sent_ping = time.time()

def main():
    global got_pfsync
    global got_ping
    global sent_ping

    parser = argparse.ArgumentParser("pfsync_defer.py",
        description="pfsync defer mode test")
    parser.add_argument('--syncdev', nargs=1,
        required=True,
        help='The pfsync interface')
    parser.add_argument('--outdev', nargs=1,
        required=True,
        help='The interface we will send packets on')
    parser.add_argument('--indev', nargs=1,
        required=True,
        help='The interface we will receive packets on')

    args = parser.parse_args()

    syncmon = Sniffer(args, check_pfsync, args.syncdev[0])
    datamon = Sniffer(args, check_reply, args.indev[0])

    # Send traffic on datadev, which should create state and produce a pfsync message
    ping(args.outdev[0])

    syncmon.join()
    datamon.join()

    if not got_pfsync:
        sys.exit(1)

    if not got_ping:
        sys.exit(2)

    # Deferred packets are delayed around 2.5s (unless the pfsync peer, which
    # we don't have here, acks their state update earlier)
    # Expect at least a second of delay, to be somewhat robust against
    # scheduling-induced jitter.
    if (sent_ping + 1) > got_ping:
        sys.exit(3)

if __name__ == '__main__':
    main()
