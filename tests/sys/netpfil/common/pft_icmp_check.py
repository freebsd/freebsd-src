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
import random
import scapy.all as sp
import socket
import sys
from sniffer import Sniffer

PAYLOAD_MAGIC = bytes.fromhex('42c0ffee')

def ping(send_if, dst_ip, args):
	ether = sp.Ether()
	ip = sp.IP(dst=dst_ip, src=args.fromaddr[0])
	icmp = sp.ICMP(type='echo-request')
	raw = sp.raw(PAYLOAD_MAGIC * 250) # We want 1000 bytes payload, -ish

	ip.flags = 2 # Don't fragment
	icmp.seq = random.randint(0, 65535)
	args.icmp_seq = icmp.seq

	req = ether / ip / icmp / raw
	sp.sendp(req, iface=send_if, verbose=False)

def check_icmp_too_big(args, packet):
	"""
	Verify that this is an ICMP packet too big error, and that the IP addresses
	in the payload packet match expectations.
	"""
	icmp = packet.getlayer(sp.ICMP)
	if not icmp:
		return False

	if not icmp.type == 3:
		return False
	ip = packet.getlayer(sp.IPerror)
	if not ip:
		return False

	if ip.src != args.fromaddr[0]:
		print("Incorrect src addr %s" % ip.src)
		return False
	if ip.dst != args.to[0]:
		print("Incorrect dst addr %s" % ip.dst)
		return False

	icmp2 = packet.getlayer(sp.ICMPerror)
	if not icmp2:
		print("IPerror doesn't contain ICMP")
		return False
	if icmp2.seq != args.icmp_seq:
		print("Incorrect icmp seq %d != %d" % (icmp2.seq, args.icmp_seq))
		return False
	return True

def main():
	parser = argparse.ArgumentParser("pft_icmp_check.py",
	    description="ICMP error validation tool")
	parser.add_argument('--to', nargs=1, required=True,
	    help='The destination IP address')
	parser.add_argument('--fromaddr', nargs=1, required=True,
	    help='The source IP address')
	parser.add_argument('--sendif', nargs=1, required=True,
	    help='The interface through which the packet(s) will be sent')
	parser.add_argument('--recvif', nargs=1,
	    help='The interface on which to expect the ICMP error')

	args = parser.parse_args()
	sniffer = None
	if not args.recvif is None:
		sniffer = Sniffer(args, check_icmp_too_big)

	ping(args.sendif[0], args.to[0], args)

	if sniffer:
		sniffer.join()

		if sniffer.foundCorrectPacket:
			sys.exit(0)
		else:
			sys.exit(1)

if __name__ == '__main__':
	main()
