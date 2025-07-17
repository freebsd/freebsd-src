#!/usr/bin/env python3
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2017 Kristof Provost <kp@FreeBSD.org>
# Copyright (c) 2023 Kajetan Staszkiewicz <vegeta@tuxpowered.net>
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
import math
import scapy.all as sp
import sys
import socket

from copy import copy
from sniffer import Sniffer

logging.basicConfig(format='%(message)s')
LOGGER = logging.getLogger(__name__)

PAYLOAD_MAGIC = bytes.fromhex('42c0ffee')

def build_payload(l):
    pl = len(PAYLOAD_MAGIC)
    ret = PAYLOAD_MAGIC * math.floor(l/pl)
    ret += PAYLOAD_MAGIC[0:(l % pl)]
    return ret


def clean_params(params):
    # Prepare a copy of safe copy of params
    ret = copy(params)
    ret.pop('src_address')
    ret.pop('dst_address')
    ret.pop('flags')
    return ret

def prepare_ipv6(send_params):
    src_address = send_params.get('src_address')
    dst_address = send_params.get('dst_address')
    hlim = send_params.get('hlim')
    tc = send_params.get('tc')
    ip6 = sp.IPv6(dst=dst_address)
    if src_address:
        ip6.src = src_address
    if hlim:
        ip6.hlim = hlim
    if tc:
        ip6.tc = tc
    return ip6


def prepare_ipv4(send_params):
    src_address = send_params.get('src_address')
    dst_address = send_params.get('dst_address')
    flags = send_params.get('flags')
    tos = send_params.get('tc')
    ttl = send_params.get('hlim')
    opt = send_params.get('nop')
    options = ''
    if opt:
        options='\x00'
    ip = sp.IP(dst=dst_address, options=options)
    if src_address:
        ip.src = src_address
    if flags:
        ip.flags = flags
    if tos:
        ip.tos = tos
    if ttl:
        ip.ttl = ttl
    return ip


def send_icmp_ping(send_params):
    send_length = send_params['length']
    send_frag_length = send_params['frag_length']
    packets = []
    ether = sp.Ether()
    if ':' in send_params['dst_address']:
        ip6 = prepare_ipv6(send_params)
        icmp = sp.ICMPv6EchoRequest(data=sp.raw(build_payload(send_length)))
        if send_frag_length:
            for packet in sp.fragment6(ip6 / icmp, fragSize=send_frag_length):
                packets.append(ether / packet)
        else:
            packets.append(ether / ip6 / icmp)

    else:
        ip = prepare_ipv4(send_params)
        icmp = sp.ICMP(type='echo-request')
        raw = sp.raw(build_payload(send_length))
        if send_frag_length:
            for packet in sp.fragment(ip / icmp / raw, fragsize=send_frag_length):
                packets.append(ether / packet)
        else:
            packets.append(ether / ip / icmp / raw)
    for packet in packets:
        sp.sendp(packet, iface=send_params['sendif'], verbose=False)


def send_tcp_syn(send_params):
    tcpopt_unaligned = send_params.get('tcpopt_unaligned')
    seq = send_params.get('seq')
    mss = send_params.get('mss')
    ether = sp.Ether()
    opts=[('Timestamp', (1, 1)), ('MSS', mss if mss else 1280)]
    if tcpopt_unaligned:
        opts = [('NOP', 0 )] + opts
    if ':' in send_params['dst_address']:
        ip = prepare_ipv6(send_params)
    else:
        ip = prepare_ipv4(send_params)
    tcp = sp.TCP(
        sport=send_params.get('sport'), dport=send_params.get('dport'),
        flags='S', options=opts, seq=seq,
    )
    req = ether / ip / tcp
    sp.sendp(req, iface=send_params['sendif'], verbose=False)


def send_udp(send_params):
    LOGGER.debug(f'Sending UDP ping')
    packets = []
    send_length = send_params['length']
    send_frag_length = send_params['frag_length']
    ether = sp.Ether()
    if ':' in send_params['dst_address']:
        ip6 = prepare_ipv6(send_params)
        udp = sp.UDP(
            sport=send_params.get('sport'), dport=send_params.get('dport'),
        )
        raw = sp.Raw(load=build_payload(send_length))
        if send_frag_length:
            for packet in sp.fragment6(ip6 / udp / raw, fragSize=send_frag_length):
                packets.append(ether / packet)
        else:
            packets.append(ether / ip6 / udp / raw)
    else:
        ip = prepare_ipv4(send_params)
        udp = sp.UDP(
            sport=send_params.get('sport'), dport=send_params.get('dport'),
        )
        raw = sp.Raw(load=build_payload(send_length))
        if send_frag_length:
            for packet in sp.fragment(ip / udp / raw, fragsize=send_frag_length):
                packets.append(ether / packet)
        else:
            packets.append(ether / ip / udp / raw)

    for packet in packets:
        sp.sendp(packet, iface=send_params['sendif'], verbose=False)


def send_ping(ping_type, send_params):
    if ping_type == 'icmp':
        send_icmp_ping(send_params)
    elif (
        ping_type == 'tcpsyn' or
        ping_type == 'tcp3way'
    ):
        send_tcp_syn(send_params)
    elif ping_type == 'udp':
        send_udp(send_params)
    else:
        raise Exception('Unsupported ping type')


def check_ipv4(expect_params, packet):
    src_address = expect_params.get('src_address')
    dst_address = expect_params.get('dst_address')
    flags = expect_params.get('flags')
    tos = expect_params.get('tc')
    ttl = expect_params.get('hlim')
    ip = packet.getlayer(sp.IP)
    LOGGER.debug(f'Packet: {ip}')
    if not ip:
        LOGGER.debug('Packet is not IPv4!')
        return False
    if src_address and ip.src != src_address:
        LOGGER.debug(f'Wrong IPv4 source {ip.src}, expected {src_address}')
        return False
    if dst_address and ip.dst != dst_address:
        LOGGER.debug(f'Wrong IPv4 destination {ip.dst}, expected {dst_address}')
        return False
    if flags and ip.flags != flags:
        LOGGER.debug(f'Wrong IP flags value {ip.flags}, expected {flags}')
        return False
    if tos and ip.tos != tos:
        LOGGER.debug(f'Wrong ToS value {ip.tos}, expected {tos}')
        return False
    if ttl and ip.ttl != ttl:
        LOGGER.debug(f'Wrong TTL value {ip.ttl}, expected {ttl}')
        return False
    return True


def check_ipv6(expect_params, packet):
    src_address = expect_params.get('src_address')
    dst_address = expect_params.get('dst_address')
    flags = expect_params.get('flags')
    hlim = expect_params.get('hlim')
    tc = expect_params.get('tc')
    ip6 = packet.getlayer(sp.IPv6)
    if not ip6:
        LOGGER.debug('Packet is not IPv6!')
        return False
    if src_address and socket.inet_pton(socket.AF_INET6, ip6.src) != \
      socket.inet_pton(socket.AF_INET6, src_address):
        LOGGER.debug(f'Wrong IPv6 source {ip6.src}, expected {src_address}')
        return False
    if dst_address and socket.inet_pton(socket.AF_INET6, ip6.dst) != \
      socket.inet_pton(socket.AF_INET6, dst_address):
        LOGGER.debug(f'Wrong IPv6 destination {ip6.dst}, expected {dst_address}')
        return False
    # IPv6 has no IP-level checksum.
    if flags:
        raise Exception("There's no fragmentation flags in IPv6")
    if hlim and ip6.hlim != hlim:
        LOGGER.debug(f'Wrong Hop Limit value {ip6.hlim}, expected {hlim}')
        return False
    if tc and ip6.tc != tc:
        LOGGER.debug(f'Wrong TC value {ip6.tc}, expected {tc}')
        return False
    return True


def check_ping_4(expect_params, packet):
    expect_length = expect_params['length']
    if not check_ipv4(expect_params, packet):
        return False
    icmp = packet.getlayer(sp.ICMP)
    if not icmp:
        LOGGER.debug('Packet is not IPv4 ICMP!')
        return False
    raw = packet.getlayer(sp.Raw)
    if not raw:
        LOGGER.debug('Packet contains no payload!')
        return False
    if raw.load != build_payload(expect_length):
        LOGGER.debug('Payload magic does not match!')
        return False
    return True


def check_ping_request_4(expect_params, packet):
    if not check_ping_4(expect_params, packet):
        return False
    icmp = packet.getlayer(sp.ICMP)
    if sp.icmptypes[icmp.type] != 'echo-request':
        LOGGER.debug('Packet is not IPv4 ICMP Echo Request!')
        return False
    return True


def check_ping_reply_4(expect_params, packet):
    if not check_ping_4(expect_params, packet):
        return False
    icmp = packet.getlayer(sp.ICMP)
    if sp.icmptypes[icmp.type] != 'echo-reply':
        LOGGER.debug('Packet is not IPv4 ICMP Echo Reply!')
        return False
    return True


def check_ping_request_6(expect_params, packet):
    expect_length = expect_params['length']
    if not check_ipv6(expect_params, packet):
        return False
    icmp = packet.getlayer(sp.ICMPv6EchoRequest)
    if not icmp:
        LOGGER.debug('Packet is not IPv6 ICMP Echo Request!')
        return False
    if icmp.data != build_payload(expect_length):
        LOGGER.debug('Payload magic does not match!')
        return False
    return True


def check_ping_reply_6(expect_params, packet):
    expect_length = expect_params['length']
    if not check_ipv6(expect_params, packet):
        return False
    icmp = packet.getlayer(sp.ICMPv6EchoReply)
    if not icmp:
        LOGGER.debug('Packet is not IPv6 ICMP Echo Reply!')
        return False
    if icmp.data != build_payload(expect_length):
        LOGGER.debug('Payload magic does not match!')
        return False
    return True


def check_ping_request(args, packet):
    src_address = args['expect_params'].get('src_address')
    dst_address = args['expect_params'].get('dst_address')
    if not (src_address or dst_address):
        raise Exception('Source or destination address must be given to match the ping request!')
    if (
        (src_address and ':' in src_address) or
        (dst_address and ':' in dst_address)
    ):
        return check_ping_request_6(args['expect_params'], packet)
    else:
        return check_ping_request_4(args['expect_params'], packet)


def check_ping_reply(args, packet):
    src_address = args['expect_params'].get('src_address')
    dst_address = args['expect_params'].get('dst_address')
    if not (src_address or dst_address):
        raise Exception('Source or destination address must be given to match the ping reply!')
    if (
        (src_address and ':' in src_address) or
        (dst_address and ':' in dst_address)
    ):
        return check_ping_reply_6(args['expect_params'], packet)
    else:
        return check_ping_reply_4(args['expect_params'], packet)


def check_tcp(expect_params, packet):
    tcp_flags = expect_params.get('tcp_flags')
    mss = expect_params.get('mss')
    seq = expect_params.get('seq')
    tcp = packet.getlayer(sp.TCP)
    if not tcp:
        LOGGER.debug('Packet is not TCP!')
        return False
    chksum = tcp.chksum
    tcp.chksum = None
    newpacket = sp.Ether(sp.raw(packet[sp.Ether]))
    new_chksum = newpacket[sp.TCP].chksum
    if new_chksum and chksum != new_chksum:
        LOGGER.debug(f'Wrong TCP checksum {chksum}, expected {new_chksum}!')
        return False
    if tcp_flags and tcp.flags != tcp_flags:
        LOGGER.debug(f'Wrong TCP flags {tcp.flags}, expected {tcp_flags}!')
        return False
    if seq:
        if tcp_flags == 'S':
            tcp_seq = tcp.seq
        elif tcp_flags == 'SA':
            tcp_seq = tcp.ack - 1
        if seq != tcp_seq:
            LOGGER.debug(f'Wrong TCP Sequence Number {tcp_seq}, expected {seq}')
            return False
    if mss:
        for option in tcp.options:
            if option[0] == 'MSS':
                if option[1] != mss:
                    LOGGER.debug(f'Wrong TCP MSS {option[1]}, expected {mss}')
                    return False
    return True


def check_udp(expect_params, packet):
    expect_length = expect_params['length']
    udp = packet.getlayer(sp.UDP)
    if not udp:
        LOGGER.debug('Packet is not UDP!')
        return False
    raw = packet.getlayer(sp.Raw)
    if not raw:
        LOGGER.debug('Packet contains no payload!')
        return False
    if raw.load != build_payload(expect_length):
        LOGGER.debug(f'Payload magic does not match len {len(raw.load)} vs {expect_length}!')
        return False
    orig_chksum = udp.chksum
    udp.chksum = None
    newpacket = sp.Ether(sp.raw(packet[sp.Ether]))
    new_chksum = newpacket[sp.UDP].chksum
    if new_chksum and orig_chksum != new_chksum:
        LOGGER.debug(f'Wrong UDP checksum {orig_chksum}, expected {new_chksum}!')
        return False

    return True


def check_tcp_syn_request_4(expect_params, packet):
    if not check_ipv4(expect_params, packet):
        return False
    if not check_tcp(expect_params | {'tcp_flags': 'S'}, packet):
        return False
    return True


def check_tcp_syn_reply_4(send_params, expect_params, packet):
    if not check_ipv4(expect_params, packet):
        return False
    if not check_tcp(expect_params | {'tcp_flags': 'SA'}, packet):
        return False
    return True


def check_tcp_3way_4(args, packet):
    send_params = args['send_params']

    expect_params_sa = clean_params(args['expect_params'])
    expect_params_sa['src_address'] = send_params['dst_address']
    expect_params_sa['dst_address'] = send_params['src_address']

    # Sniff incoming SYN+ACK packet
    if (
        check_ipv4(expect_params_sa, packet) and
        check_tcp(expect_params_sa | {'tcp_flags': 'SA'}, packet)
    ):
        ether = sp.Ether()
        ip_sa = packet.getlayer(sp.IP)
        tcp_sa = packet.getlayer(sp.TCP)
        reply_params = clean_params(send_params)
        reply_params['src_address'] = ip_sa.dst
        reply_params['dst_address'] = ip_sa.src
        ip_a = prepare_ipv4(reply_params)
        tcp_a = sp.TCP(
            sport=tcp_sa.dport, dport=tcp_sa.sport, flags='A',
            seq=tcp_sa.ack, ack=tcp_sa.seq + 1,
        )
        req = ether / ip_a / tcp_a
        sp.sendp(req, iface=send_params['sendif'], verbose=False)
        return True

    return False


def check_udp_request_4(expect_params, packet):
    if not check_ipv4(expect_params, packet):
        return False
    if not check_udp(expect_params, packet):
        return False
    return True


def check_tcp_syn_request_6(expect_params, packet):
    if not check_ipv6(expect_params, packet):
        return False
    if not check_tcp(expect_params | {'tcp_flags': 'S'}, packet):
        return False
    return True


def check_tcp_syn_reply_6(expect_params, packet):
    if not check_ipv6(expect_params, packet):
        return False
    if not check_tcp(expect_params | {'tcp_flags': 'SA'}, packet):
        return False
    return True


def check_tcp_3way_6(args, packet):
    send_params = args['send_params']

    expect_params_sa = clean_params(args['expect_params'])
    expect_params_sa['src_address'] = send_params['dst_address']
    expect_params_sa['dst_address'] = send_params['src_address']

    # Sniff incoming SYN+ACK packet
    if (
        check_ipv6(expect_params_sa, packet) and
        check_tcp(expect_params_sa | {'tcp_flags': 'SA'}, packet)
    ):
        ether = sp.Ether()
        ip6_sa = packet.getlayer(sp.IPv6)
        tcp_sa = packet.getlayer(sp.TCP)
        reply_params = clean_params(send_params)
        reply_params['src_address'] = ip6_sa.dst
        reply_params['dst_address'] = ip6_sa.src
        ip_a = prepare_ipv6(reply_params)
        tcp_a = sp.TCP(
            sport=tcp_sa.dport, dport=tcp_sa.sport, flags='A',
            seq=tcp_sa.ack, ack=tcp_sa.seq + 1,
        )
        req = ether / ip_a / tcp_a
        sp.sendp(req, iface=send_params['sendif'], verbose=False)
        return True

    return False


def check_udp_request_6(expect_params, packet):
    if not check_ipv6(expect_params, packet):
        return False
    if not check_udp(expect_params, packet):
        return False
    return True

def check_tcp_syn_request(args, packet):
    expect_params = args['expect_params']
    src_address = expect_params.get('src_address')
    dst_address = expect_params.get('dst_address')
    if not (src_address or dst_address):
        raise Exception('Source or destination address must be given to match the tcp syn request!')
    if (
        (src_address and ':' in src_address) or
        (dst_address and ':' in dst_address)
    ):
        return check_tcp_syn_request_6(expect_params, packet)
    else:
        return check_tcp_syn_request_4(expect_params, packet)


def check_tcp_syn_reply(args, packet):
    expect_params = args['expect_params']
    src_address = expect_params.get('src_address')
    dst_address = expect_params.get('dst_address')
    if not (src_address or dst_address):
        raise Exception('Source or destination address must be given to match the tcp syn reply!')
    if (
        (src_address and ':' in src_address) or
        (dst_address and ':' in dst_address)
    ):
        return check_tcp_syn_reply_6(expect_params, packet)
    else:
        return check_tcp_syn_reply_4(expect_params, packet)

def check_tcp_3way(args, packet):
    expect_params = args['expect_params']
    src_address = expect_params.get('src_address')
    dst_address = expect_params.get('dst_address')
    if not (src_address or dst_address):
        raise Exception('Source or destination address must be given to match the tcp syn reply!')
    if (
            (src_address and ':' in src_address) or
            (dst_address and ':' in dst_address)
    ):
        return check_tcp_3way_6(args, packet)
    else:
        return check_tcp_3way_4(args, packet)


def check_udp_request(args, packet):
    expect_params = args['expect_params']
    src_address = expect_params.get('src_address')
    dst_address = expect_params.get('dst_address')
    if not (src_address or dst_address):
        raise Exception('Source or destination address must be given to match the tcp syn request!')
    if (
            (src_address and ':' in src_address) or
            (dst_address and ':' in dst_address)
    ):
        return check_udp_request_6(expect_params, packet)
    else:
        return check_udp_request_4(expect_params, packet)


def setup_sniffer(
        recvif, ping_type, sniff_type, expect_params, defrag, send_params,
):
    if ping_type == 'icmp' and sniff_type == 'request':
        checkfn = check_ping_request
    elif ping_type == 'icmp' and sniff_type == 'reply':
        checkfn = check_ping_reply
    elif ping_type == 'tcpsyn' and sniff_type == 'request':
        checkfn = check_tcp_syn_request
    elif ping_type == 'tcpsyn' and sniff_type == 'reply':
        checkfn = check_tcp_syn_reply
    elif ping_type == 'tcp3way' and sniff_type == 'reply':
        checkfn = check_tcp_3way
    elif ping_type == 'udp' and sniff_type == 'request':
        checkfn = check_udp_request
    else:
        raise Exception('Unspported ping and sniff type combination')

    return Sniffer(
        {'send_params': send_params, 'expect_params': expect_params},
        checkfn, recvif, defrag=defrag,
    )


def parse_args():
    parser = argparse.ArgumentParser("pft_ping.py",
        description="Ping test tool")

    # Parameters of sent ping request
    parser.add_argument('--sendif', required=True,
        help='The interface through which the packet(s) will be sent')
    parser.add_argument('--to', required=True,
        help='The destination IP address for the ping request')
    parser.add_argument('--ping-type',
        choices=('icmp', 'tcpsyn', 'tcp3way', 'udp'),
        help='Type of ping: ICMP (default) or TCP SYN or 3-way TCP handshake',
        default='icmp')
    parser.add_argument('--fromaddr',
        help='The source IP address for the ping request')

    # Where to look for packets to analyze.
    # The '+' format is ugly as it mixes positional with optional syntax.
    # But we have no positional parameters so I guess it's fine to use it.
    parser.add_argument('--recvif', nargs='+',
        help='The interfaces on which to expect the ping request')
    parser.add_argument('--replyif', nargs='+',
        help='The interfaces which to expect the ping response')

    # Packet settings
    parser_send = parser.add_argument_group('Values set in transmitted packets')
    parser_send.add_argument('--send-flags', type=str,
        help='IPv4 fragmentation flags')
    parser_send.add_argument('--send-frag-length', type=int,
        help='Force IP fragmentation with given fragment length')
    parser_send.add_argument('--send-hlim', type=int,
        help='IPv6 Hop Limit or IPv4 Time To Live')
    parser_send.add_argument('--send-mss', type=int,
        help='TCP Maximum Segment Size')
    parser_send.add_argument('--send-seq', type=int,
        help='TCP sequence number')
    parser_send.add_argument('--send-sport', type=int,
        help='TCP source port')
    parser_send.add_argument('--send-dport', type=int, default=9,
        help='TCP destination port')
    parser_send.add_argument('--send-length', type=int, default=len(PAYLOAD_MAGIC),
        help='ICMP Echo Request payload size')
    parser_send.add_argument('--send-tc', type=int,
        help='IPv6 Traffic Class or IPv4 DiffServ / ToS')
    parser_send.add_argument('--send-tcpopt-unaligned', action='store_true',
        help='Include unaligned TCP options')
    parser_send.add_argument('--send-nop', action='store_true',
        help='Include a NOP IPv4 option')

    # Expectations
    parser_expect = parser.add_argument_group('Values expected in sniffed packets')
    parser_expect.add_argument('--expect-flags', type=str,
        help='IPv4 fragmentation flags')
    parser_expect.add_argument('--expect-hlim', type=int,
        help='IPv6 Hop Limit or IPv4 Time To Live')
    parser_expect.add_argument('--expect-mss', type=int,
        help='TCP Maximum Segment Size')
    parser_send.add_argument('--expect-seq', type=int,
        help='TCP sequence number')
    parser_expect.add_argument('--expect-tc', type=int,
        help='IPv6 Traffic Class or IPv4 DiffServ / ToS')

    parser.add_argument('-v', '--verbose', action='store_true',
        help=('Enable verbose logging. Apart of potentially useful information '
            'you might see warnings from parsing packets like NDP or other '
            'packets not related to the test being run. Use only when '
            'developing because real tests expect empty stderr and stdout.'))

    return parser.parse_args()


def main():
    args = parse_args()

    if args.verbose:
        LOGGER.setLevel(logging.DEBUG)

    # Split parameters into send and expect parameters. Parameters might be
    # missing from the command line, always fill the dictionaries with None.
    send_params = {}
    expect_params = {}
    for param_name in (
        'flags', 'hlim', 'length', 'mss', 'seq', 'tc', 'frag_length',
        'sport', 'dport',
    ):
        param_arg = vars(args).get(f'send_{param_name}')
        send_params[param_name] = param_arg if param_arg else None
        param_arg = vars(args).get(f'expect_{param_name}')
        expect_params[param_name] = param_arg if param_arg else None

    expect_params['length'] = send_params['length']
    send_params['tcpopt_unaligned'] = args.send_tcpopt_unaligned
    send_params['nop'] = args.send_nop
    send_params['src_address'] = args.fromaddr if args.fromaddr else None
    send_params['dst_address'] = args.to
    send_params['sendif'] = args.sendif

    # We may not have a default route. Tell scapy where to start looking for routes
    sp.conf.iface6 = args.sendif

    # Configuration sanity checking.
    if not (args.replyif or args.recvif):
        raise Exception('With no reply or recv interface specified no traffic '
            'can be sniffed and verified!'
        )

    sniffers = []

    if send_params['frag_length']:
        if (
            (send_params['src_address'] and ':' in send_params['src_address']) or
            (send_params['dst_address'] and ':' in send_params['dst_address'])
        ):
            defrag = 'IPv6'
        else:
            defrag = 'IPv4'
    else:
        defrag = False

    if args.recvif:
        sniffer_params = copy(expect_params)
        sniffer_params['src_address'] = None
        sniffer_params['dst_address'] = args.to
        for iface in args.recvif:
            LOGGER.debug(f'Installing receive sniffer on {iface}')
            sniffers.append(
                setup_sniffer(iface, args.ping_type, 'request',
                              sniffer_params, defrag, send_params,
            ))

    if args.replyif:
        sniffer_params = copy(expect_params)
        sniffer_params['src_address'] = args.to
        sniffer_params['dst_address'] = None
        for iface in args.replyif:
            LOGGER.debug(f'Installing reply sniffer on {iface}')
            sniffers.append(
                setup_sniffer(iface, args.ping_type, 'reply',
                              sniffer_params, defrag, send_params,
            ))

    LOGGER.debug(f'Installed {len(sniffers)} sniffers')

    send_ping(args.ping_type, send_params)

    err = 0
    sniffer_num = 0
    for sniffer in sniffers:
        sniffer.join()
        if sniffer.correctPackets == 1:
            LOGGER.debug(f'Expected ping has been sniffed on {sniffer._recvif}.')
        else:
            # Set a bit in err for each failed sniffer.
            err |= 1<<sniffer_num
            if sniffer.correctPackets > 1:
                LOGGER.debug(f'Duplicated ping has been sniffed on {sniffer._recvif}!')
            else:
                LOGGER.debug(f'Expected ping has not been sniffed on {sniffer._recvif}!')
        sniffer_num += 1

    return err


if __name__ == '__main__':
    sys.exit(main())
