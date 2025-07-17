#!/usr/bin/env python
#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Netflix, Inc.
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

import argparse
import logging
logging.getLogger("scapy").setLevel(logging.CRITICAL)
import scapy.all as sp
import socket
import sys
from sniffer import Sniffer
from time import sleep

def check_icmp6_error(args, packet):
	ip6 = packet.getlayer(sp.IPv6)
	if not ip6:
		return False
	oip6 = sp.IPv6(src=args.src[0], dst=args.to[0])
	if ip6.dst != oip6.src:
		return False
	icmp6 = packet.getlayer(sp.ICMPv6TimeExceeded)
	if not icmp6:
		return False
	# ICMP6_TIME_EXCEED_REASSEMBLY 1
	if icmp6.code != 1:
		return False
	# Should we check the payload as well?
	# We are running in a very isolated environment and nothing else
	# should trigger an ICMPv6 Time Exceeded / Frag reassembly so leave it.
	#icmp6.display()
	return True


def main():
	parser = argparse.ArgumentParser("frag6.py",
		description="IPv6 fragementation test tool")
	parser.add_argument('--sendif', nargs=1,
		required=True,
		help='The interface through which the packet will be sent')
	parser.add_argument('--recvif', nargs=1,
		required=True,
		help='The interface on which to check for the packet')
	parser.add_argument('--src', nargs=1,
		required=True,
		help='The source IP address')
	parser.add_argument('--to', nargs=1,
		required=True,
		help='The destination IP address')
	parser.add_argument('--debug',
		required=False, action='store_true',
		help='Enable test debugging')

	args = parser.parse_args()


	# Start sniffing on recvif
	sniffer = Sniffer(args, check_icmp6_error)


	########################################################################
	#
	# Send a proper first fragment (off=0) and a second fragment which
	# just fits the 64k.  The re-send the first fragment with an extra
	# unfragmentable part making the 64k to exceed the limit.
	# This is to make sure we don't allow to update meta-data for a
	# 1st fragmented packet should a second arrive but given the
	# fragmentable part is an exact duplicate only that fragment
	# will be silently discarded.
	#
	# A:  Reassembly failure, timeout after
	# R:  ICMPv6 time exceeded / statistics for the duplicate
	#
	data = "6" * 8
	ip6f00 = \
		sp.Ether() / \
		sp.IPv6(src=args.src[0], dst=args.to[0]) / \
		sp.IPv6ExtHdrFragment(offset=0, m=1, id=20) / \
		sp.UDP(dport=3456, sport=6543) / \
		data
	data = "6" * 15
	ip6f01 = \
		sp.Ether() / \
		sp.IPv6(src=args.src[0], dst=args.to[0]) / \
		sp.IPv6ExtHdrFragment(offset=0x1ffc, m=0, id=20) / \
		sp.UDP(dport=3456, sport=6543) / \
		data
	data = "6" * 8
	ip6f02 = \
		sp.Ether() / \
		sp.IPv6(src=args.src[0], dst=args.to[0]) / \
		sp.IPv6ExtHdrDestOpt(options = \
		    sp.PadN(optdata="\x00\x00\x00\x00\x00\x00")) / \
		sp.IPv6ExtHdrFragment(offset=0, m=1, id=20) / \
		sp.UDP(dport=3456, sport=6543) / \
		data
	if args.debug :
		ip6f00.display()
		ip6f01.display()
		ip6f02.display()
	sp.sendp(ip6f00, iface=args.sendif[0], verbose=False)
	sp.sendp(ip6f01, iface=args.sendif[0], verbose=False)
	sp.sendp(ip6f02, iface=args.sendif[0], verbose=False)

	sleep(3)
	sniffer.setEnd()
	sniffer.join()
	if not sniffer.foundCorrectPacket:
		sys.exit(1)

	sys.exit(0)

if __name__ == '__main__':
	main()
