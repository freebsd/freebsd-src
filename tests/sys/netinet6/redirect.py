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
# $FreeBSD$
#

import argparse
import logging
logging.getLogger("scapy").setLevel(logging.CRITICAL)
import scapy.all as sc
import socket
import sys
import fcntl
import struct


def parse_args():
    parser = argparse.ArgumentParser(description='ICMPv6 redirect generator')
    parser.add_argument('--smac', type=str, required=True,
                        help='eth source mac')
    parser.add_argument('--dmac', type=str, required=True,
                        help='eth dest mac')
    parser.add_argument('--sip', type=str, required=True,
                        help='remote router ll source ip')
    parser.add_argument('--dip', type=str, required=True,
                        help='local router ip')
    parser.add_argument('--iface', type=str, required=True,
                        help='ifname to send packet to')
    parser.add_argument('--route', type=str, required=True,
                        help='destination IP to redirect')
    parser.add_argument('--gw', type=str, required=True,
                        help='redirect GW')
    return parser.parse_args()


def construct_icmp6_redirect(smac, dmac, sip, dip, route_dst, route_gw):
    e = sc.Ether(src=smac, dst=dmac)
    l3 = sc.IPv6(src=sip, dst=dip)
    icmp6 = sc.ICMPv6ND_Redirect(tgt=route_gw, dst=route_dst)
    return e / l3 / icmp6


def send_packet(pkt, iface, feedback=False):
    if feedback:
        # Make kernel receive the packet as well
        BIOCFEEDBACK = 0x8004427c
        socket = sc.conf.L2socket(iface=args.iface)
        fcntl.ioctl(socket.ins, BIOCFEEDBACK, struct.pack('I', 1))
        sc.sendp(pkt, socket=socket, verbose=True)
    else:
        sc.sendp(pkt, iface=iface, verbose=False)


def main():
    args = parse_args()
    pkt = construct_icmp6_redirect(args.smac, args.dmac, args.sip, args.dip,
                                   args.route, args.gw)
    send_packet(pkt, args.iface)


if __name__ == '__main__':
    main()
