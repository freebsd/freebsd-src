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

import argparse
import logging
logging.getLogger("scapy").setLevel(logging.CRITICAL)
import scapy.all as sp

def syn_flood(args):
	s = sp.conf.L2socket(iface=args.sendif[0])

	# Set a src mac, to avoid doing lookups which really slow us down.
	ether = sp.Ether(src='01:02:03:04:05')
	if args.ip6:
		ip = sp.IPv6(dst=args.to[0])
	else:
		ip = sp.IP(dst=args.to[0])
	for i in range(int(args.count[0])):
		tcp = sp.TCP(flags='S', sport=1+i, dport=22, seq=500+i)
		pkt = ether / ip / tcp
		s.send(pkt)

def main():
	parser = argparse.ArgumentParser("pft_synflood.py",
		description="SYN flooding tool")
	parser.add_argument('--ip6',
		action='store_true',
		help='Use IPv6 rather than IPv4')
	parser.add_argument('--sendif', nargs=1,
		required=True,
		help='The interface through which the packet(s) will be sent')
	parser.add_argument('--to', nargs=1,
		required=True,
		help='The destination IP address for the SYN packets')
	parser.add_argument('--count', nargs=1,
		required=True,
		help='The number of packets to send')

	args = parser.parse_args()

	syn_flood(args)

if __name__ == '__main__':
	main()
