#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 Klara, Inc.
#

import argparse
import scapy.all as sp
import sys


#
# Emit a router advertisement with the specified prefix.
#
def main():
    parser = argparse.ArgumentParser("ra.py",
                                     description="Emits Router Advertisement packets")
    parser.add_argument('--sendif', nargs=1, required=True,
                        help='The interface through which the packet will be sent')
    parser.add_argument('--src', nargs=1, required=True,
                        help='The source IP address')
    parser.add_argument('--dst', nargs=1, required=True,
                        help='The destination IP address')
    parser.add_argument('--prefix', nargs=1, required=True,
                        help='The prefix to be advertised')
    parser.add_argument('--prefixlen', nargs=1, required=True, type=int,
                        help='The prefix length to be advertised')
    parser.add_argument('--validlifetime', nargs=1, required=False,
                        type=int, default=4294967295,
                        help='The valid lifetime of the prefix')
    parser.add_argument('--preferredlifetime', nargs=1, required=False,
                        type=int, default=4294967295,
                        help='The preferred lifetime of the prefix')

    args = parser.parse_args()
    pkt = sp.Ether() / \
        sp.IPv6(src=args.src, dst=args.dst) / \
        sp.ICMPv6ND_RA(chlim=64) / \
        sp.ICMPv6NDOptPrefixInfo(prefix=args.prefix,
                                 prefixlen=args.prefixlen,
                                 validlifetime=args.validlifetime,
                                 preferredlifetime=args.preferredlifetime)

    sp.sendp(pkt, iface=args.sendif[0], verbose=False)
    sys.exit(0)


if __name__ == '__main__':
    main()
