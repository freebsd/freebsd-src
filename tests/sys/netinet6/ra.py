#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 Klara, Inc.
# Copyright (c) 2026 Pouria Mousavizadeh Tehrani <pouria@FreeBSD.org>
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
    parser.add_argument('--hoplimit', nargs=1, required=False,
                        type=int, default=255,
                        help='The hop limit of IPv6 packet')
    parser.add_argument('--rtrpref', nargs=1, required=False,
                        type=int, default=1,
                        help='The router preference advertised')
    parser.add_argument('--rtrltime', nargs=1, required=False,
                        type=int, default=1800,
                        help='The router preference advertised')
    parser.add_argument('--prefix', nargs=1, required=False,
                        help='The prefix to be advertised')
    parser.add_argument('--prefixlen', nargs=1, required=False,
                        type=int, default=64,
                        help='The prefix length to be advertised')
    parser.add_argument('--validlifetime', nargs=1, required=False,
                        type=int, default=4294967295,
                        help='The valid lifetime of the prefix')
    parser.add_argument('--preferredlifetime', nargs=1, required=False,
                        type=int, default=4294967295,
                        help='The preferred lifetime of the prefix')
    parser.add_argument('--route', nargs='*', required=False,
                        action='extend', type=str, default=[],
                        help='The route to be advertised')
    parser.add_argument('--routelen', nargs='*', required=False,
                        action='extend', type=int, default=[],
                        help='The route length to be advertised')
    parser.add_argument('--rtipref', nargs='*', required=False,
                        action='extend', type=int, default=[],
                        help='The route info preference advertised')
    parser.add_argument('--rtiltime', nargs='*', required=False,
                        action='extend', type=int, default=[],
                        help='The route info preference advertised')

    args = parser.parse_args()
    pkt = sp.Ether() / \
        sp.IPv6(src=args.src, dst=args.dst, hlim=args.hoplimit) / \
        sp.ICMPv6ND_RA(chlim=64,
                       prf=args.rtrpref,
                       routerlifetime=args.rtrltime)

    if (args.prefix):
        pkt = pkt / \
            sp.ICMPv6NDOptPrefixInfo(prefix=args.prefix,
                                     prefixlen=args.prefixlen,
                                     validlifetime=args.validlifetime,
                                     preferredlifetime=args.preferredlifetime)

    for i in range(0, len(args.route)):
        pkt = pkt / \
            sp.ICMPv6NDOptRouteInfo(prefix=args.route[i],
                                    plen=args.routelen[i],
                                    prf=args.rtipref[i],
                                    rtlifetime=args.rtiltime[i])

    sp.sendp(pkt, iface=args.sendif[0], verbose=False)
    sys.exit(0)


if __name__ == '__main__':
    main()
