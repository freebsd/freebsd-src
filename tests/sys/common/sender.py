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
import logging
logging.getLogger("scapy").setLevel(logging.CRITICAL)
import scapy.all as sc
import argparse
import time


def parse_args():
    parser = argparse.ArgumentParser(description='divert socket tester')
    parser.add_argument('--dip', type=str, help='destination packet IP')
    parser.add_argument('--sip', type=str, help='source packet IP')
    parser.add_argument('--dmac', type=str, help='packet dst mac')
    parser.add_argument('--smac', type=str, help='packet src mac')
    parser.add_argument('--iface', type=str, help='interface to use')
    parser.add_argument('--test_name', type=str, required=True,
                        help='test name to run')
    return parser.parse_args()


def send_packet(args, pkt):
    sc.sendp(pkt, iface=args.iface, verbose=False)


def is_icmp6_echo_request(pkt):
    return pkt.type == 0x86DD and pkt.payload.nh == 58 and \
            pkt.payload.payload.type == 128


def check_forwarded_ip_packet(orig_pkt, fwd_pkt):
    """
    Checks that forwarded ICMP packet @fwd_ptk is the same as
    @orig_pkt. Assumes router-on-the-stick forwarding behaviour:
     * src/dst macs are swapped
     * TTL is decremented
    """
    # Check ether fields
    assert orig_pkt.src == fwd_pkt.dst
    assert orig_pkt.dst == fwd_pkt.src
    assert len(orig_pkt) == len(fwd_pkt)
    # Check IP
    fwd_ip = fwd_pkt[sc.IP]
    orig_ip = orig_pkt[sc.IP]
    assert orig_ip.src == orig_ip.src
    assert orig_ip.dst == fwd_ip.dst
    assert orig_ip.ttl == fwd_ip.ttl + 1
    # Check ICMP
    fwd_icmp = fwd_ip[sc.ICMP]
    orig_icmp = orig_ip[sc.ICMP]
    assert bytes(orig_ip.payload) == bytes(fwd_ip.payload)


def fwd_ip_icmp_fast(args):
    """
    Sends ICMP packet via args.iface interface.
    Receives and checks the forwarded packet.
    Assumes forwarding router decrements TTL
    """

    def filter_f(x):
        return x.src == args.dmac and x.type == 0x0800

    e = sc.Ether(src=args.smac, dst=args.dmac)
    ip = sc.IP(src=args.sip, dst=args.dip)
    icmp = sc.ICMP(type='echo-request')
    pkt = e / ip / icmp

    send_cb = partial(send_packet, args, pkt)
    packets = sc.sniff(iface=args.iface, started_callback=send_cb,
                       stop_filter=filter_f, lfilter=filter_f, timeout=5)
    assert len(packets) > 0
    fwd_pkt = packets[-1]
    try:
        check_forwarded_ip_packet(pkt, fwd_pkt)
    except Exception as e:
        print('Original packet:')
        pkt.show()
        print('Forwarded packet:')
        fwd_pkt.show()
        for a_packet in packets:
            a_packet.summary()
        raise Exception from e


def fwd_ip_icmp_slow(args):
    """
    Sends ICMP packet via args.iface interface.
    Forces slow path processing by introducing IP option.
    Receives and checks the forwarded packet.
    Assumes forwarding router decrements TTL
    """

    def filter_f(x):
        return x.src == args.dmac and x.type == 0x0800

    e = sc.Ether(src=args.smac, dst=args.dmac)
    # Add IP option to switch to 'normal' IP processing
    stream_id = sc.IPOption_Stream_Id(security=0xFFFF)
    ip = sc.IP(src=args.sip, dst=args.dip,
               options=[sc.IPOption_Stream_Id(security=0xFFFF)])
    icmp = sc.ICMP(type='echo-request')
    pkt = e / ip / icmp

    send_cb = partial(send_packet, args, pkt)
    packets = sc.sniff(iface=args.iface, started_callback=send_cb,
                       stop_filter=filter_f, lfilter=filter_f, timeout=5)
    assert len(packets) > 0
    check_forwarded_ip_packet(pkt, packets[-1])


def check_forwarded_ip6_packet(orig_pkt, fwd_pkt):
    """
    Checks that forwarded ICMP packet @fwd_ptk is the same as
    @orig_pkt. Assumes router-on-the-stick forwarding behaviour:
     * src/dst macs are swapped
     * TTL is decremented
    """
    # Check ether fields
    assert orig_pkt.src == fwd_pkt.dst
    assert orig_pkt.dst == fwd_pkt.src
    assert len(orig_pkt) == len(fwd_pkt)
    # Check IP
    fwd_ip = fwd_pkt[sc.IPv6]
    orig_ip = orig_pkt[sc.IPv6]
    assert orig_ip.src == orig_ip.src
    assert orig_ip.dst == fwd_ip.dst
    assert orig_ip.hlim == fwd_ip.hlim + 1
    # Check ICMPv6
    assert bytes(orig_ip.payload) == bytes(fwd_ip.payload)


def fwd_ip6_icmp(args):
    """
    Sends ICMPv6 packet via args.iface interface.
    Receives and checks the forwarded packet.
    Assumes forwarding router decrements TTL
    """

    def filter_f(x):
        return x.src == args.dmac and is_icmp6_echo_request(x)

    e = sc.Ether(src=args.smac, dst=args.dmac)
    ip = sc.IPv6(src=args.sip, dst=args.dip)
    icmp = sc.ICMPv6EchoRequest()
    pkt = e / ip / icmp

    send_cb = partial(send_packet, args, pkt)
    packets = sc.sniff(iface=args.iface, started_callback=send_cb,
                       stop_filter=filter_f, lfilter=filter_f, timeout=5)
    assert len(packets) > 0
    fwd_pkt = packets[-1]
    try:
        check_forwarded_ip6_packet(pkt, fwd_pkt)
    except Exception as e:
        print('Original packet:')
        pkt.show()
        print('Forwarded packet:')
        fwd_pkt.show()
        for idx, a_packet in enumerate(packets):
            print('{}: {}'.format(idx, a_packet.summary()))
        raise Exception from e


def main():
    args = parse_args()
    test_ptr = globals()[args.test_name]
    test_ptr(args)


if __name__ == '__main__':
    main()
