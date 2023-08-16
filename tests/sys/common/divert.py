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


from socket import socket, PF_DIVERT, SOCK_RAW
import logging
logging.getLogger("scapy").setLevel(logging.CRITICAL)
import scapy.all as sc
import argparse


def parse_args():
    parser = argparse.ArgumentParser(description='divert socket tester')
    parser.add_argument('--dip', type=str, help='destination packet IP')
    parser.add_argument('--sip', type=str, help='source packet IP')
    parser.add_argument('--divert_port', type=int, default=6668,
                        help='divert port to use')
    parser.add_argument('--test_name', type=str, required=True,
                        help='test name to run')
    return parser.parse_args()


def ipdivert_ip_output_remote_success(args):
    packet = sc.IP(dst=args.dip) / sc.ICMP(type='echo-request')
    with socket(PF_DIVERT, SOCK_RAW, 0) as s:
        s.bind(('0.0.0.0', args.divert_port))
        s.sendto(bytes(packet), ('0.0.0.0', 0))


def ipdivert_ip6_output_remote_success(args):
    packet = sc.IPv6(dst=args.dip) / sc.ICMPv6EchoRequest()
    with socket(PF_DIVERT, SOCK_RAW, 0) as s:
        s.bind(('0.0.0.0', args.divert_port))
        s.sendto(bytes(packet), ('0.0.0.0', 0))


def ipdivert_ip_input_local_success(args):
    """Sends IPv4 packet to OS stack as inbound local packet."""
    packet = sc.IP(dst=args.dip,src=args.sip) / sc.ICMP(type='echo-request')
    with socket(PF_DIVERT, SOCK_RAW, 0) as s:
        s.bind(('0.0.0.0', args.divert_port))
        s.sendto(bytes(packet), (args.dip, 0))


# XXX: IPv6 local divert is currently not supported
# TODO: add IPv4 ifname output verification


def main():
    args = parse_args()
    test_ptr = globals()[args.test_name]
    test_ptr(args)


if __name__ == '__main__':
    main()
