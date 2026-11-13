#!/usr/bin/env python3
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Maksym Hrytsaienko <hrytsaienko.my@gmail.com>
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

# Send a single pfsync PFSYNC_ACT_UPD_C message for an existing TCP state.

import argparse
import struct
import sys
import scapy.all as sp

PFSYNC_VERSION = 5
PFSYNC_ACT_UPD_C = 4
TCPS_ESTABLISHED = 4
PFTM_TCP_ESTABLISHED = 2

def peer(seqlo, seqhi, wscale):
    # struct pf_state_peer_export: scrub, seqlo, seqhi, seqdiff, max_win,
    # mss, state, wscale, dummy[6]
    return bytes(8) + struct.pack('!IIIHHBB6x', seqlo, seqhi, 0, 65535, 0,
        TCPS_ESTABLISHED, wscale)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--to', required=True)
    parser.add_argument('--id', required=True,
        help='state id as printed by pfctl -ss -vv')
    parser.add_argument('--creatorid', required=True)
    parser.add_argument('--src', nargs=3, type=int, required=True,
        metavar=('SEQLO', 'SEQHI', 'WSCALE'))
    parser.add_argument('--dst', nargs=3, type=int, required=True,
        metavar=('SEQLO', 'SEQHI', 'WSCALE'))
    args = parser.parse_args()

    # pfctl prints the raw id as a host-order integer.
    sid = bytes.fromhex(args.id)
    if sys.byteorder == 'little':
        sid = sid[::-1]
    upd = sid + peer(*args.src) + peer(*args.dst) + \
        bytes.fromhex(args.creatorid) + \
        struct.pack('!IB3x', 0, PFTM_TCP_ESTABLISHED)
    sub = struct.pack('!BBH', PFSYNC_ACT_UPD_C, 0, 1) + upd
    hdr = struct.pack('!BBH', PFSYNC_VERSION, 0, 20 + len(sub)) + bytes(16)
    sp.send(sp.IP(dst=args.to, ttl=255, proto=240) / sp.Raw(hdr + sub),
        verbose=False)

if __name__ == '__main__':
    main()
